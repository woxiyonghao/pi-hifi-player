# 树莓派 5 HiFi 数播系统部署与运维手册

本文档汇总了树莓派 5 部署 HiFi 数播系统的核心命令、Git 自动更新机制、开机自启配置以及应急终端切换方案。  
所有敏感信息（WiFi 密码、私钥内容等）均已脱敏处理。

---

## 一、网络连接与 Wi-Fi 配置命令

在树莓派命令行中操作（只需配置一次，后续开机自动回连）：

```bash
# 1. 扫描周围 Wi-Fi 接入点 (查看信号强度与信道)
sudo nmcli dev wifi list --rescan yes

# 2. 连接指定 Wi-Fi (将 SSID 与 PASSWORD 替换为实际值)
sudo nmcli dev wifi connect "<YOUR_WIFI_SSID>" password "<YOUR_WIFI_PASSWORD>"

# 若 Wi-Fi 设置了隐藏广播，需加上 hidden yes：
# sudo nmcli dev wifi connect "<YOUR_WIFI_SSID>" password "<YOUR_WIFI_PASSWORD>" hidden yes

# 3. 推荐：使用终端图形界面直观选网与连接（最防错）
sudo nmtui

# 4. 优化 Wi-Fi 功耗策略 (防止无线芯片自动休眠导致 SSH 掉线或高延迟)
# 将 powersave 设置为 2 (禁用休眠省电，保持全速响应)
sudo nmcli connection modify "<YOUR_WIFI_SSID>" 802-11-wireless.powersave 2

# 5. 查看树莓派当前分配到的局域网 IPv4 地址
hostname -I
```

---

## 二、Git 认证与自动更新配置 (打通 GitHub 免密拉取)

为了让树莓派内置的 **UpdateManager (OTA 检查与自动更新模块)** 能够随时拉取 GitHub 最新代码并自动编译，需在树莓派上配置 Git 身份与 SSH 认证：

```bash
# 1. 在 Mac 本地将 GitHub 访问私钥推送到树莓派 (脱敏示例)
scp ~/.ssh/id_rsa ~/.ssh/id_rsa.pub <PI_USER>@<PI_IP>:~/.ssh/

# 2. 在树莓派上设置正确的私钥文件权限 (SSH 强制要求 600)
chmod 700 ~/.ssh
chmod 600 ~/.ssh/id_rsa
chmod 644 ~/.ssh/id_rsa.pub

# 3. 在树莓派配置 ~/.ssh/config，实现免人机交互验证
cat << 'CONFIG_EOF' > ~/.ssh/config
Host github.com
    StrictHostKeyChecking no
    UserKnownHostsFile /dev/null
    IdentityFile ~/.ssh/id_rsa
    User git
CONFIG_EOF
chmod 600 ~/.ssh/config

# 4. 验证树莓派是否成功连接 GitHub
ssh -T git@github.com
# 终端返回: Hi <USERNAME>! You've successfully authenticated... 即表示成功！

# 5. 配置树莓派本地 Git 用户信息与安全目录
git config --global user.name "<YOUR_NAME>"
git config --global user.email "<YOUR_EMAIL>"
git config --global --add safe.directory /home/<PI_USER>/pi-hifi-music
```

---

## 三、Mac 端一键增量部署与控制命令

在 Mac 开发机的主工程目录下运行：

```bash
# 1. 默认一键同步最新代码、增量编译并直接在前台点亮树莓派屏幕 (附带实时日志)
./scripts/deploy_pi.sh

# 2. 指定树莓派 IP 进行部署
./scripts/deploy_pi.sh <PI_IP>

# 3. 部署并注册为 Linux systemd 开机免密自启服务 (开机即进播放器)
./scripts/deploy_pi.sh --service

# 4. 部署并在后台常驻运行 (断开 SSH 连接后屏幕依然保持点亮)
./scripts/deploy_pi.sh --daemon

# 5. 实时追踪查看树莓派上的运行日志
./scripts/deploy_pi.sh --log

# 6. 安全停止树莓派上正在运行的播放器进程
./scripts/deploy_pi.sh --kill
```

---

## 四、树莓派本地原生编译与运行命令

若直接登录在树莓派终端中操作：

```bash
cd ~/pi-hifi-music

# 1. 检查 CMake 配置 (Release 极致性能优化模式)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 2. 启用树莓派 5 全部 4 核 Cortex-A76 并行编译
cmake --build build -j$(nproc)

# 3. 直通底层 Linux KMS/DRM 硬件帧缓冲区运行 (无需任何桌面环境)
env SDL_VIDEODRIVER=kmsdrm SDL_RENDER_VSYNC=1 ./build/PiHifiPlayer

# 4. 手动执行 OTA 检查更新脚本 (只检测不编译)
./scripts/update_and_rebuild.sh --check-only

# 5. 手动执行一键拉取并重新编译
./scripts/update_and_rebuild.sh
```

---

## 五、开机自启 systemd 服务管理命令

播放器作为系统服务（Kiosk Mode）运行时的日常管理命令：

```bash
# 查看自启服务当前运行状态
sudo systemctl status pi-hifi.service

# 重启播放器服务 (热更新后立即生效)
sudo systemctl restart pi-hifi.service

# 停止服务 (释放屏幕与底层显示控制权)
sudo systemctl stop pi-hifi.service

# 开机启用自启 / 关闭开机自启
sudo systemctl enable pi-hifi.service   # 开启自启
sudo systemctl disable pi-hifi.service  # 禁用自启

# 实时查看系统服务运行日志 (类似 Mac 的 Console)
sudo journalctl -u pi-hifi.service -f -n 50
```
