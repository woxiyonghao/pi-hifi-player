#!/usr/bin/env bash
# ==============================================================================
# Mac 本地代码一键增量部署与树莓派 5 编译运行脚本
#
# 常见用法：
#   ./scripts/deploy_pi.sh                  # 一键部署并在树莓派屏幕上启动 (前台日志调试)
#   ./scripts/deploy_pi.sh --daemon         # 一键部署并在后台常驻运行
#   ./scripts/deploy_pi.sh --service        # 一键部署并配置为开机自启 systemd 服务
#   ./scripts/deploy_pi.sh --kill           # 停止树莓派上正在运行的播放器
#   ./scripts/deploy_pi.sh --log            # 实时查看树莓派运行日志
#   ./scripts/deploy_pi.sh 192.168.1.169    # 指定树莓派 IP
# ==============================================================================
set -e

# 默认优先使用的树莓派 IP 与用户名
DEFAULT_PI_HOST="192.168.1.184"
DEFAULT_PI_USER="winheo"

PI_HOST=""
PI_USER=""
ACTION="foreground"

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--host)
            PI_HOST="$2"
            shift 2
            ;;
        -u|--user)
            PI_USER="$2"
            shift 2
            ;;
        -d|--daemon)
            ACTION="daemon"
            shift
            ;;
        -s|--service)
            ACTION="service"
            shift
            ;;
        -r|--run)
            ACTION="foreground"
            shift
            ;;
        -k|--kill)
            ACTION="kill"
            shift
            ;;
        -l|--log)
            ACTION="log"
            shift
            ;;
        --help)
            echo "用法: $0 [选项] [树莓派IP] [用户名]"
            echo ""
            echo "选项:"
            echo "  -h, --host <IP>      指定树莓派 IP (默认: ${DEFAULT_PI_HOST})"
            echo "  -u, --user <USER>    指定树莓派登录用户 (默认: ${DEFAULT_PI_USER})"
            echo "  -r, --run            一键同步、编译并前台运行 (默认)"
            echo "  -d, --daemon         一键同步、编译并在后台守护运行"
            echo "  -s, --service        一键同步、编译并配置/重启 systemd 开机自启服务"
            echo "  -k, --kill           停止树莓派上正在运行的播放器进程与服务"
            echo "  -l, --log            实时查看树莓派播放器运行日志"
            exit 0
            ;;
        *)
            if [ -z "$PI_HOST" ]; then
                PI_HOST="$1"
            elif [ -z "$PI_USER" ]; then
                PI_USER="$1"
            fi
            shift
            ;;
    esac
done

PI_HOST="${PI_HOST:-$DEFAULT_PI_HOST}"
PI_USER="${PI_USER:-$DEFAULT_PI_USER}"
REMOTE_DIR="/home/${PI_USER}/pi-hifi-music"

# SSH 基础选项 (5秒超时检测)
SSH_OPTS="-o ConnectTimeout=5 -o StrictHostKeyChecking=accept-new"

GREEN="\033[1;32m"
YELLOW="\033[1;33m"
CYAN="\033[1;36m"
RED="\033[1;31m"
RESET="\033[0m"

echo -e "${CYAN}==========================================================${RESET}"
echo -e "${CYAN} 🚀 树莓派 5 HiFi 数播一键部署与运行系统${RESET}"
echo -e "${CYAN} 目标设备: ${PI_USER}@${PI_HOST}${RESET}"
echo -e "${CYAN} 执行动作: ${ACTION}${RESET}"
echo -e "${CYAN}==========================================================${RESET}"

# 1. 连通性测试
echo -ne "${YELLOW}>> 正在测试与树莓派的 SSH 连接... ${RESET}"
if ! ssh ${SSH_OPTS} "${PI_USER}@${PI_HOST}" "true" 2>/dev/null; then
    echo -e "${RED}[失败]${RESET}"
    # 尝试使用 mDNS 主机名
    ALT_HOST="winheo-pi.local"
    echo -ne "${YELLOW}>> 尝试备用 mDNS 主机名 (${PI_USER}@${ALT_HOST})... ${RESET}"
    if ssh ${SSH_OPTS} "${PI_USER}@${ALT_HOST}" "true" 2>/dev/null; then
        echo -e "${GREEN}[成功]${RESET}"
        PI_HOST="${ALT_HOST}"
    else
        echo -e "${RED}[连接失败]${RESET}"
        echo -e "${RED}错误：无法连接到树莓派！请确认：${RESET}"
        echo "  1. 树莓派已开机并连上与 Mac 相同的局域网/Wi-Fi"
        echo "  2. 树莓派 IP 是否改变（可在路由器后台查看，或使用: $0 <实际IP>）"
        echo "  3. 若需免密登录，可先在 Mac 终端执行: ssh-copy-id ${PI_USER}@${PI_HOST}"
        exit 1
    fi
fi
echo -e "${GREEN}[OK]${RESET}"

# 2. 如果动作是 kill 或 log，直接调用远程脚本对应动作，跳过同步与编译
if [ "${ACTION}" = "kill" ] || [ "${ACTION}" = "log" ]; then
    ssh -t ${SSH_OPTS} "${PI_USER}@${PI_HOST}" "bash ${REMOTE_DIR}/scripts/remote_run.sh ${ACTION}"
    exit 0
fi

# 3. 准备远程目录与增量代码同步
echo -e "${YELLOW}>> 正在同步本地代码至树莓派 (增量同步)...${RESET}"
ssh ${SSH_OPTS} "${PI_USER}@${PI_HOST}" "mkdir -p ${REMOTE_DIR}"

rsync -avz --delete \
    -e "ssh ${SSH_OPTS}" \
    --exclude "build/" \
    --exclude "build-ios/" \
    --exclude "cmake-build-debug/" \
    --exclude ".idea/" \
    --exclude ".vscode/" \
    --exclude ".cache/" \
    --exclude "*.DS_Store" \
    ./ "${PI_USER}@${PI_HOST}:${REMOTE_DIR}/"

echo -e "${GREEN}>> ✅ 代码同步完毕！${RESET}"

# 4. 远程执行环境检测、编译与运行
echo -e "${YELLOW}>> 启动树莓派远端原生编译与运行引擎...${RESET}"
ssh -t ${SSH_OPTS} "${PI_USER}@${PI_HOST}" "chmod +x ${REMOTE_DIR}/scripts/*.sh && bash ${REMOTE_DIR}/scripts/remote_run.sh ${ACTION}"
