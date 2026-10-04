#include "tools/WifiTransferServer.hpp"
#include "tools/MusicScanManager.hpp"
#include "public/AppConfig.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <cstring>
#include <cctype>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#endif

namespace {

// 发烧纯音深色玻璃风 H5 传歌单页应用 (自包含 CSS 与原生 JS，完全离线免外部 CDN)
const char* INDEX_HTML = R"html(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>PiHiFi 无线传歌中枢</title>
<style>
:root {
  --bg-color: #0c1017;
  --card-bg: rgba(22, 28, 40, 0.85);
  --card-border: rgba(255, 255, 255, 0.12);
  --accent: #10b981;
  --accent-glow: rgba(16, 185, 129, 0.35);
  --text-active: #ffffff;
  --text-normal: #cbd5e1;
  --text-muted: #64748b;
  --progress-bg: #1e293b;
  --danger: #ef4444;
}
* { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "PingFang SC", "Microsoft YaHei", sans-serif; }
body {
  background: var(--bg-color);
  color: var(--text-normal);
  min-height: 100vh;
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 24px 16px;
  background-image: radial-gradient(circle at 50% 10%, rgba(16, 185, 129, 0.08), transparent 45%);
}
.container { width: 100%; max-width: 680px; }
.header { text-align: center; margin-bottom: 24px; }
.header h1 { font-size: 26px; font-weight: 700; color: var(--text-active); letter-spacing: 0.5px; margin-bottom: 8px; }
.header p { font-size: 14px; color: var(--text-muted); }
.badge { display: inline-flex; align-items: center; gap: 6px; padding: 4px 12px; background: rgba(16, 185, 129, 0.12); border: 1px solid var(--accent); border-radius: 20px; color: var(--accent); font-size: 12px; font-weight: 600; margin-top: 10px; }
.badge-dot { width: 8px; height: 8px; border-radius: 50%; background: var(--accent); box-shadow: 0 0 8px var(--accent); }
.card {
  background: var(--card-bg);
  border: 1px solid var(--card-border);
  border-radius: 16px;
  padding: 24px;
  box-shadow: 0 8px 32px rgba(0, 0, 0, 0.4);
  backdrop-filter: blur(16px);
  margin-bottom: 20px;
}
.drop-zone {
  border: 2px dashed rgba(255, 255, 255, 0.2);
  border-radius: 12px;
  padding: 40px 20px;
  text-align: center;
  cursor: pointer;
  transition: all 0.25s ease;
  background: rgba(15, 23, 42, 0.4);
}
.drop-zone:hover, .drop-zone.dragover {
  border-color: var(--accent);
  background: rgba(16, 185, 129, 0.05);
  box-shadow: 0 0 20px var(--accent-glow);
}
.drop-icon { font-size: 44px; margin-bottom: 12px; color: var(--accent); }
.drop-title { font-size: 17px; font-weight: 600; color: var(--text-active); margin-bottom: 6px; }
.drop-sub { font-size: 13px; color: var(--text-muted); line-height: 1.5; }
.file-formats { margin-top: 12px; font-size: 12px; color: var(--text-muted); }
.file-formats span { background: rgba(255, 255, 255, 0.06); padding: 2px 6px; border-radius: 4px; margin: 0 2px; }
input[type="file"] { display: none; }
.actions { display: flex; gap: 12px; margin-top: 16px; }
.btn {
  flex: 1;
  padding: 12px;
  border-radius: 10px;
  border: none;
  font-size: 15px;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.2s ease;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 8px;
}
.btn-primary { background: var(--accent); color: #0c1017; }
.btn-primary:hover:not(:disabled) { filter: brightness(1.1); box-shadow: 0 0 16px var(--accent-glow); }
.btn-primary:disabled { opacity: 0.4; cursor: not-allowed; }
.btn-outline { background: transparent; border: 1px solid rgba(255, 255, 255, 0.2); color: var(--text-normal); }
.btn-outline:hover:not(:disabled) { background: rgba(255, 255, 255, 0.08); }
.queue-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 14px; font-size: 14px; font-weight: 600; color: var(--text-active); }
.queue-list { display: flex; flex-direction: column; gap: 10px; max-height: 320px; overflow-y: auto; padding-right: 4px; }
.queue-item {
  background: rgba(15, 23, 42, 0.6);
  border: 1px solid rgba(255, 255, 255, 0.08);
  border-radius: 10px;
  padding: 12px 14px;
}
.queue-top { display: flex; justify-content: space-between; font-size: 14px; margin-bottom: 8px; }
.file-name { font-weight: 500; color: var(--text-active); overflow: hidden; text-overflow: ellipsis; white-space: nowrap; max-width: 75%; }
.file-status { font-size: 12px; color: var(--text-muted); }
.progress-bar-bg { width: 100%; height: 6px; background: var(--progress-bg); border-radius: 3px; overflow: hidden; }
.progress-bar-fill { height: 100%; width: 0%; background: var(--accent); border-radius: 3px; transition: width 0.15s ease; }
.queue-bottom { display: flex; justify-content: space-between; font-size: 12px; color: var(--text-muted); margin-top: 6px; }
.status-success { color: var(--accent) !important; font-weight: 600; }
.status-error { color: var(--danger) !important; }
.empty-tip { text-align: center; color: var(--text-muted); padding: 24px; font-size: 14px; }
</style>
</head>
<body>
<div class="container">
  <div class="header">
    <h1>PiHiFi 发烧纯音无线传歌中枢</h1>
    <p>树莓派局域网极速无损传输 · 即传即播 · 自动扫描入库</p>
    <div class="badge"><div class="badge-dot"></div><span>树莓派服务已就绪 · 本地直连</span></div>
  </div>

  <div class="card">
    <div class="drop-zone" id="dropZone">
      <div class="drop-icon">&#9835;</div>
      <div class="drop-title">拖拽音频文件到此处，或点击浏览文件</div>
      <div class="drop-sub">支持单选、多选或批量拖拽，传输完毕后数播自动更新曲库</div>
      <div class="file-formats">
        <span>FLAC</span><span>WAV</span><span>DSF</span><span>DFF</span><span>APE</span><span>MP3</span><span>M4A</span><span>AAC</span>
      </div>
      <input type="file" id="fileInput" multiple accept=".flac,.wav,.dsf,.dff,.ape,.mp3,.m4a,.aac,.ogg,.wma,audio/*">
    </div>
    <div class="actions">
      <button class="btn btn-primary" id="btnUpload" disabled>开始批量传输 (0)</button>
      <button class="btn btn-outline" id="btnClear" disabled>清空列表</button>
    </div>
  </div>

  <div class="card">
    <div class="queue-header">
      <span>传输队列与状态 (<span id="queueCount">0</span>)</span>
      <span id="overallSpeed" style="font-size:12px; color:var(--text-muted);">空闲</span>
    </div>
    <div class="queue-list" id="queueList">
      <div class="empty-tip" id="emptyTip">暂无待上传音频文件</div>
    </div>
  </div>
</div>

<script>
const dropZone = document.getElementById('dropZone');
const fileInput = document.getElementById('fileInput');
const btnUpload = document.getElementById('btnUpload');
const btnClear = document.getElementById('btnClear');
const queueList = document.getElementById('queueList');
const emptyTip = document.getElementById('emptyTip');
const queueCount = document.getElementById('queueCount');
const overallSpeed = document.getElementById('overallSpeed');

let fileQueue = [];
let isUploading = false;

dropZone.addEventListener('click', () => fileInput.click());
dropZone.addEventListener('dragover', (e) => { e.preventDefault(); dropZone.classList.add('dragover'); });
dropZone.addEventListener('dragleave', () => dropZone.classList.remove('dragover'));
dropZone.addEventListener('drop', (e) => {
  e.preventDefault();
  dropZone.classList.remove('dragover');
  if (e.dataTransfer.files.length > 0) {
    addFiles(e.dataTransfer.files);
  }
});
fileInput.addEventListener('change', () => {
  if (fileInput.files.length > 0) {
    addFiles(fileInput.files);
  }
});

function formatBytes(bytes) {
  if (bytes === 0) return '0 B';
  const k = 1024;
  const sizes = ['B', 'KB', 'MB', 'GB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

function addFiles(files) {
  for (let file of files) {
    if (!fileQueue.some(item => item.file.name === file.name && item.file.size === file.size)) {
      fileQueue.push({ file: file, status: 'pending', loaded: 0, total: file.size, speed: '0 MB/s' });
    }
  }
  renderQueue();
}

function renderQueue() {
  if (fileQueue.length === 0) {
    emptyTip.style.display = 'block';
    queueCount.textContent = '0';
    btnUpload.disabled = true;
    btnClear.disabled = true;
    btnUpload.textContent = '开始批量传输 (0)';
    return;
  }

  emptyTip.style.display = 'none';
  queueCount.textContent = fileQueue.length;
  btnUpload.disabled = isUploading;
  btnClear.disabled = isUploading;
  btnUpload.textContent = `开始批量传输 (${fileQueue.filter(i => i.status === 'pending').length})`;

  queueList.innerHTML = '';
  fileQueue.forEach((item, index) => {
    const div = document.createElement('div');
    div.className = 'queue-item';
    let pct = item.total > 0 ? Math.round((item.loaded / item.total) * 100) : 0;
    if (item.status === 'success') pct = 100;

    let statusText = '等待上传';
    let statusClass = 'file-status';
    if (item.status === 'uploading') {
      statusText = `上传中 ${pct}% · ${item.speed}`;
    } else if (item.status === 'success') {
      statusText = '已入库 ✓';
      statusClass = 'file-status status-success';
    } else if (item.status === 'error') {
      statusText = '上传失败 ✗';
      statusClass = 'file-status status-error';
    }

    div.innerHTML = `
      <div class="queue-top">
        <span class="file-name" title="${item.file.name}">${item.file.name}</span>
        <span class="${statusClass}">${statusText}</span>
      </div>
      <div class="progress-bar-bg">
        <div class="progress-bar-fill" style="width: ${pct}%"></div>
      </div>
      <div class="queue-bottom">
        <span>${formatBytes(item.loaded)} / ${formatBytes(item.total)}</span>
        <span>${item.status === 'uploading' ? item.speed : ''}</span>
      </div>
    `;
    queueList.appendChild(div);
  });
}

btnClear.addEventListener('click', () => {
  if (isUploading) return;
  fileQueue = [];
  renderQueue();
});

btnUpload.addEventListener('click', async () => {
  if (isUploading) return;
  isUploading = true;
  btnUpload.disabled = true;
  btnClear.disabled = true;

  for (let i = 0; i < fileQueue.length; ++i) {
    let item = fileQueue[i];
    if (item.status === 'success') continue;

    item.status = 'uploading';
    renderQueue();

    try {
      await uploadSingleFile(item);
      item.status = 'success';
      item.loaded = item.total;
    } catch (err) {
      item.status = 'error';
    }
    renderQueue();
  }

  isUploading = false;
  overallSpeed.textContent = '传输完成';
  btnUpload.disabled = false;
  btnClear.disabled = false;
  btnUpload.textContent = '传输已完成';
});

function uploadSingleFile(item) {
  return new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    const encodedName = encodeURIComponent(item.file.name);
    xhr.open('POST', `/upload?filename=${encodedName}`);

    let startTime = Date.now();
    let lastLoaded = 0;

    xhr.upload.onprogress = (e) => {
      if (e.lengthComputable) {
        item.loaded = e.loaded;
        item.total = e.total;

        let now = Date.now();
        let elapsed = (now - startTime) / 1000;
        if (elapsed > 0.4) {
          let speedVal = ((e.loaded - lastLoaded) / elapsed / 1024 / 1024).toFixed(1);
          item.speed = `${speedVal} MB/s`;
          overallSpeed.textContent = `当前速率: ${item.speed}`;
          startTime = now;
          lastLoaded = e.loaded;
          renderQueue();
        }
      }
    };

    xhr.onload = () => {
      if (xhr.status === 200) {
        resolve();
      } else {
        reject(new Error(xhr.responseText || '上传失败'));
      }
    };
    xhr.onerror = () => reject(new Error('网络传输中断'));
    xhr.send(item.file);
  });
}
</script>
</body>
</html>
)html";

} // namespace

WifiTransferServer::WifiTransferServer() {
    // 默认保存路径：优先使用 ~/Music，若无则使用应用统一配置路径
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        std::filesystem::path p = std::filesystem::path(home) / "Music";
        target_music_dir_ = p.string();
    } else {
        target_music_dir_ = AppConfig::Path::getMusicDir();
    }
}

WifiTransferServer::~WifiTransferServer() {
    stop();
}

WifiTransferServer& WifiTransferServer::getInstance() {
    static WifiTransferServer s_instance;
    return s_instance;
}

bool WifiTransferServer::start(int port, const std::string& music_dir) {
    if (is_running_.load()) {
        return true;
    }

    port_ = port;
    if (!music_dir.empty()) {
        target_music_dir_ = music_dir;
    }

    // 确保目标音乐目录存在
    try {
        std::filesystem::create_directories(target_music_dir_);
    } catch (...) {
        std::cerr << "[WifiTransferServer] 无法创建目标目录: " << target_music_dir_ << std::endl;
    }

    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "[WifiTransferServer] 创建 socket 失败" << std::endl;
        return false;
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(server_fd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[WifiTransferServer] bind 端口 " << port_ << " 失败" << std::endl;
#if defined(_WIN32)
        closesocket(server_fd_);
#else
        close(server_fd_);
#endif
        server_fd_ = -1;
        return false;
    }

    if (listen(server_fd_, 16) < 0) {
        std::cerr << "[WifiTransferServer] listen 失败" << std::endl;
#if defined(_WIN32)
        closesocket(server_fd_);
#else
        close(server_fd_);
#endif
        server_fd_ = -1;
        return false;
    }

    is_running_.store(true);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        progress_.is_running = true;
        progress_.is_uploading = false;
        progress_.error_message.clear();
    }

    std::cout << "[WifiTransferServer] WiFi 传歌服务已就绪，监听端口: " << port_
              << "，目标存储目录: " << target_music_dir_ << std::endl;

    worker_thread_ = std::thread(&WifiTransferServer::serverLoop, this);
    return true;
}

void WifiTransferServer::stop() {
    if (!is_running_.load()) {
        return;
    }

    is_running_.store(false);

    if (server_fd_ >= 0) {
#if defined(_WIN32)
        closesocket(server_fd_);
#else
        close(server_fd_);
#endif
        server_fd_ = -1;
    }

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    progress_.is_running = false;
    progress_.is_uploading = false;
    std::cout << "[WifiTransferServer] WiFi 传歌服务已停止" << std::endl;
}

TransferProgress WifiTransferServer::getProgress() {
    std::lock_guard<std::mutex> lock(mutex_);
    return progress_;
}

std::string WifiTransferServer::urlDecode(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size()) {
            int h1 = std::tolower(in[i + 1]);
            int h2 = std::tolower(in[i + 2]);
            int v1 = (h1 >= '0' && h1 <= '9') ? (h1 - '0') : (h1 - 'a' + 10);
            int v2 = (h2 >= '0' && h2 <= '9') ? (h2 - '0') : (h2 - 'a' + 10);
            out.push_back(static_cast<char>((v1 << 4) | v2));
            i += 2;
        } else if (in[i] == '+') {
            out.push_back(' ');
        } else {
            out.push_back(in[i]);
        }
    }
    return out;
}

std::string WifiTransferServer::sanitizeFilename(const std::string& in) {
    std::filesystem::path p(in);
    std::string name = p.filename().string();
    if (name.empty() || name == "." || name == "..") {
        name = "unknown_track.flac";
    }
    return name;
}

void WifiTransferServer::serverLoop() {
    while (is_running_.load()) {
        struct pollfd pfd = { server_fd_, POLLIN, 0 };
        int ret = poll(&pfd, 1, 200);
        if (ret > 0 && (pfd.revents & POLLIN)) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd_, (sockaddr*)&client_addr, &client_len);
            if (client_fd >= 0) {
                handleClient(client_fd);
#if defined(_WIN32)
                closesocket(client_fd);
#else
                close(client_fd);
#endif
            }
        }
    }
}

void WifiTransferServer::handleClient(int client_fd) {
    // 设置 15 秒套接字读写超时，防止异常断开死锁
    struct timeval tv;
    tv.tv_sec = 15;
    tv.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    std::vector<char> header_buf;
    header_buf.resize(8192);
    size_t total_header_read = 0;
    int header_end_pos = -1;

    // 读取 HTTP 头部直到 \r\n\r\n
    while (total_header_read < header_buf.size() - 1) {
        ssize_t n = recv(client_fd, header_buf.data() + total_header_read, 1, 0);
        if (n <= 0) break;
        total_header_read += n;
        header_buf[total_header_read] = '\0';

        if (total_header_read >= 4) {
            const char* p = header_buf.data() + total_header_read - 4;
            if (std::memcmp(p, "\r\n\r\n", 4) == 0) {
                header_end_pos = static_cast<int>(total_header_read);
                break;
            }
        }
    }

    if (header_end_pos < 0) {
        return;
    }

    std::string header_str(header_buf.data(), header_end_pos);
    std::istringstream hstream(header_str);
    std::string method, uri, version;
    hstream >> method >> uri >> version;

    // 1. GET / 或 /index.html: 返回 H5 网页
    if (method == "GET" && (uri == "/" || uri == "/index.html")) {
        std::string body = INDEX_HTML;
        std::ostringstream resp;
        resp << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: text/html; charset=utf-8\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << body;
        std::string resp_str = resp.str();
        send(client_fd, resp_str.data(), resp_str.size(), 0);
        return;
    }

    // 2. GET /api/status: 状态查询
    if (method == "GET" && uri.rfind("/api/status", 0) == 0) {
        std::ostringstream json;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            json << "{\"status\":\"running\",\"port\":" << port_
                 << ",\"dir\":\"" << target_music_dir_ << "\""
                 << ",\"completed\":" << progress_.completed_count << "}";
        }
        std::string body = json.str();
        std::ostringstream resp;
        resp << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: application/json; charset=utf-8\r\n"
             << "Access-Control-Allow-Origin: *\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << body;
        std::string resp_str = resp.str();
        send(client_fd, resp_str.data(), resp_str.size(), 0);
        return;
    }

    // 3. POST /upload: 接收音频二进制数据流
    if (method == "POST" && uri.rfind("/upload", 0) == 0) {
        // 解析 Content-Length
        uint64_t content_length = 0;
        size_t cl_pos = header_str.find("Content-Length:");
        if (cl_pos == std::string::npos) {
            cl_pos = header_str.find("content-length:");
        }
        if (cl_pos != std::string::npos) {
            content_length = std::strtoull(header_str.c_str() + cl_pos + 15, nullptr, 10);
        }

        // 解析文件名 (从 query 字符串 ?filename=...)
        std::string filename = "uploaded_track.flac";
        size_t q_pos = uri.find("filename=");
        if (q_pos != std::string::npos) {
            std::string raw_fn = uri.substr(q_pos + 9);
            size_t amp_pos = raw_fn.find('&');
            if (amp_pos != std::string::npos) {
                raw_fn = raw_fn.substr(0, amp_pos);
            }
            filename = sanitizeFilename(urlDecode(raw_fn));
        }

        std::filesystem::path out_path = std::filesystem::path(target_music_dir_) / filename;
        std::ofstream outfile(out_path, std::ios::binary);

        if (!outfile.is_open()) {
            std::string err_body = "{\"status\":\"error\",\"message\":\"无法创建文件\"}";
            std::ostringstream resp;
            resp << "HTTP/1.1 500 Internal Server Error\r\n"
                 << "Content-Type: application/json\r\n"
                 << "Content-Length: " << err_body.size() << "\r\n"
                 << "Connection: close\r\n\r\n"
                 << err_body;
            std::string resp_str = resp.str();
            send(client_fd, resp_str.data(), resp_str.size(), 0);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            progress_.is_uploading = true;
            progress_.current_filename = filename;
            progress_.bytes_received = 0;
            progress_.total_bytes = content_length;
            progress_.speed_mbps = 0.0f;
        }

        std::vector<char> chunk_buf(65536); // 64KB 高性能接收缓冲
        uint64_t total_received = 0;
        auto start_time = std::chrono::steady_clock::now();
        auto last_calc_time = start_time;
        uint64_t last_calc_bytes = 0;

        while (total_received < content_length) {
            size_t to_read = static_cast<size_t>(std::min<uint64_t>(chunk_buf.size(), content_length - total_received));
            ssize_t n = recv(client_fd, chunk_buf.data(), to_read, 0);
            if (n <= 0) {
                break; // 对端关闭或出错
            }

            outfile.write(chunk_buf.data(), n);
            total_received += n;

            auto now = std::chrono::steady_clock::now();
            double elapsed_sec = std::chrono::duration<double>(now - last_calc_time).count();
            if (elapsed_sec >= 0.3) {
                float mb = static_cast<float>(total_received - last_calc_bytes) / (1024.0f * 1024.0f);
                float speed = (elapsed_sec > 0.0) ? (mb / static_cast<float>(elapsed_sec)) : 0.0f;

                std::lock_guard<std::mutex> lock(mutex_);
                progress_.bytes_received = total_received;
                progress_.speed_mbps = speed;

                last_calc_time = now;
                last_calc_bytes = total_received;
            }
        }

        outfile.close();

        bool success = (total_received == content_length && content_length > 0);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            progress_.is_uploading = false;
            if (success) {
                progress_.completed_count++;
                progress_.completed_files.push_back({filename, total_received});
            }
        }

        if (success) {
            std::cout << "[WifiTransferServer] 成功接收音频文件: " << filename
                      << " (" << (total_received / (1024 * 1024)) << " MB)" << std::endl;

            // 触发曲库自动索引与通知
            if (on_file_received_) {
                on_file_received_(out_path.string());
            } else {
                MusicScanManager::getInstance().startScan(target_music_dir_);
            }

            std::string ok_body = "{\"status\":\"success\",\"filename\":\"" + filename + "\"}";
            std::ostringstream resp;
            resp << "HTTP/1.1 200 OK\r\n"
                 << "Content-Type: application/json\r\n"
                 << "Access-Control-Allow-Origin: *\r\n"
                 << "Content-Length: " << ok_body.size() << "\r\n"
                 << "Connection: close\r\n\r\n"
                 << ok_body;
            std::string resp_str = resp.str();
            send(client_fd, resp_str.data(), resp_str.size(), 0);
        } else {
            std::string err_body = "{\"status\":\"error\",\"message\":\"文件传输不完整\"}";
            std::ostringstream resp;
            resp << "HTTP/1.1 400 Bad Request\r\n"
                 << "Content-Type: application/json\r\n"
                 << "Content-Length: " << err_body.size() << "\r\n"
                 << "Connection: close\r\n\r\n"
                 << err_body;
            std::string resp_str = resp.str();
            send(client_fd, resp_str.data(), resp_str.size(), 0);
        }
        return;
    }

    // 默认返回 404
    std::string not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    send(client_fd, not_found.data(), not_found.size(), 0);
}
