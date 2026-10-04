# 树莓派 5 HiFi 数播系统部署与运维手册

本文档汇总了树莓派 5 部署 HiFi 数播系统的核心命令、Git 自动更新机制、开机自启配置、音乐传输方法、音量与声学调优以及应急终端切换方案。  
所有敏感信息（WiFi 密码、私钥等）均已脱敏处理。

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

## 二、Git 认证与专属 SSH 密钥配置 (打通 GitHub 免密 OTA 自更)

为了让树莓派内置的 **UpdateManager (OTA 检查与自动更新模块)** 能够随时拉取 GitHub 最新代码并自动编译，需在树莓派上配置 GitHub 专属 SSH 密钥：

### 1. 在树莓派上一键生成原生 ED25519 密钥
在树莓派终端直接运行：
```bash
# 生成高安全性专属密钥对 (回车即可，无需输入密码)
ssh-keygen -t ed25519 -C "raspberrypi-5-hifi" -f ~/.ssh/id_ed25519 -N ""

# 配置针对 github.com 优先使用该密钥
cat << 'CONFIG_EOF' > ~/.ssh/config
Host github.com
    IdentityFile ~/.ssh/id_ed25519
    StrictHostKeyChecking no
    UserKnownHostsFile /dev/null
    User git
CONFIG_EOF
chmod 600 ~/.ssh/config ~/.ssh/id_ed25519
chmod 644 ~/.ssh/id_ed25519.pub

# 查看并复制生成的公钥
cat ~/.ssh/id_ed25519.pub
```

### 2. 将公钥添加至 GitHub
1. 打开浏览器访问：[GitHub SSH 设置页面 (https://github.com/settings/keys)](https://github.com/settings/keys)
2. 点击右上角 **“New SSH key”**。
3. Title 填写：`Raspberry Pi 5 HiFi`。
4. Key 粘贴上方打印出的 `ssh-ed25519 AAA...` 整行公钥。
5. 点击 **“Add SSH key”** 保存。

### 3. 验证 GitHub 认证
```bash
ssh -T git@github.com
# 终端返回: Hi <USERNAME>! You've successfully authenticated... 即表示成功！

# 配置本地 Git 用户信息
git config --global user.name "<YOUR_NAME>"
git config --global user.email "<YOUR_EMAIL>"
git config --global --add safe.directory /home/<PI_USER>/pi-hifi-music
```

> **从 Mac 端一键自动生成与配置**：  
> 也可以在 Mac 终端直接执行：`./scripts/setup_pi_github_key.sh <树莓派IP>`，脚本会自动连上树莓派生成并打印出公钥供你复制。

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

## 四、Mac 向树莓派传输音乐文件的三大方法

音乐文件统一存放在树莓派的 `~/Music` 目录下，支持 MP3、FLAC、WAV、DSD(DSF/DFF) 等全格式：

### 方法 1：Mac 访达 (Finder) 像插 U 盘一样拖拽传歌 (推荐 ⭐)
1. 在 Mac 访达按下快捷键 `Command + K`（或菜单：前往 -> 连接服务器）。
2. 输入地址：`sftp://<PI_USER>@<PI_IP>`
3. 点击连接并输入密码。
4. 访达窗口打开后，进入 `Music` 文件夹，将 Mac 上的音乐文件直接拖入即可！

### 方法 2：Mac 终端命令行极速发送 (支持断点续传)
```bash
# 发送单首歌曲或专辑目录
scp -r ~/Music/你的专辑目录 <PI_USER>@<PI_IP>:~/Music/

# 增量镜像同步整整个音乐文件夹
rsync -avP ~/Music/ <PI_USER>@<PI_IP>:~/Music/
```

### 方法 3：直接插 U 盘 / 移动硬盘
直接将拷有无损音乐的 U 盘插入树莓派 5 的蓝色 USB 3.0 接口。

> **曲库入库**：传歌完毕后，在树莓派屏幕点击 **“扫描音乐”** -> **“开始扫描”**，程序会自动解析 ID3/FLAC 标签并毫秒级入库。

---

## 五、树莓派本地原生编译与运行命令

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

## 六、开机自启 systemd 服务管理命令

播放器作为系统服务（Kiosk Mode）运行时的日常管理命令：

```bash
# 查看自启服务当前运行状态
sudo systemctl status pi-hifi.service

# 重启播放器服务 (热更新后立即生效)
sudo systemctl restart pi-hifi.service

# 停止服务 (释放屏幕与底层显示控制权，退回终端)
sudo systemctl stop pi-hifi.service

# 开机启用自启 / 关闭开机自启
sudo systemctl enable pi-hifi.service   # 开启自启
sudo systemctl disable pi-hifi.service  # 禁用自启

# 实时查看系统服务运行日志 (类似 Mac 的 Console)
sudo journalctl -u pi-hifi.service -f -n 50
```

---

## 七、声学与视觉优化说明

1. **三次对数电位器音量曲线 (Cubic Audio Taper: vol³)**：
   * 人耳感知声音强度是对数分贝（dB）特性。纯线性乘法在 11% 音量下仅衰减 19dB（听感接近 50% 响度，震耳欲聋）。
   * 现已改用经典 HiFi 对数曲线：11% 音量衰减至约 -58dB（夜间细腻柔和）；50% 约 -18dB（舒适正常聆听）；100% 保持 0dB Bit-Perfect 源码直出。
2. **48 列 LED 矩阵真频谱动态防过冲**：
   * 采用 256 点真 FFT 频域分解。移除了原本过度饱和的激进过载倍率（防止每根柱子撞顶满格），呈现随低音、人声、高音真实起伏错落的声学现场律动。
3. **DAC 硬件标识自适应**：
   * 在树莓派上默认智能识别并显示为 `ALSA Direct`（树莓派硬件直通），避免错误显示 Mac 的 `Apple Direct`。
