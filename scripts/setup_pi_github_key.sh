#!/usr/bin/env bash
# ==============================================================================
# 一键在树莓派本地生成 GitHub 专属 ED25519 SSH 密钥并打印公钥
# ==============================================================================
set -e

PI_HOST="${1:-192.168.1.184}"
PI_USER="${2:-winheo}"

SSH_OPTS="-o ConnectTimeout=5 -o StrictHostKeyChecking=accept-new"

echo ">> 正在远程连接树莓派 (${PI_USER}@${PI_HOST}) 并生成专属 GitHub ED25519 密钥..."

ssh ${SSH_OPTS} "${PI_USER}@${PI_HOST}" "bash -s" << 'REMOTE_SCRIPT'
mkdir -p ~/.ssh
chmod 700 ~/.ssh

if [ ! -f ~/.ssh/id_ed25519 ]; then
    ssh-keygen -t ed25519 -C "raspberrypi-5-hifi" -f ~/.ssh/id_ed25519 -N ""
    echo ">> ✅ 树莓派专属密钥生成完毕！"
else
    echo ">> 密钥已存在，直接读取现有公钥："
fi

chmod 600 ~/.ssh/id_ed25519
chmod 644 ~/.ssh/id_ed25519.pub

cat << 'CONFIG_EOF' > ~/.ssh/config
Host github.com
    IdentityFile ~/.ssh/id_ed25519
    StrictHostKeyChecking no
    UserKnownHostsFile /dev/null
    User git
CONFIG_EOF
chmod 600 ~/.ssh/config

echo ""
echo "================================================================="
echo "  🔑 树莓派 GitHub 公钥内容 (请复制整行添加到 GitHub SSH Keys):"
echo "================================================================="
cat ~/.ssh/id_ed25519.pub
echo "================================================================="
REMOTE_SCRIPT
