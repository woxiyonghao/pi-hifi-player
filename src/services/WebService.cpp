#include "services/WebService.hpp"
#include "tools/PlayerAdmin.hpp"
#include "tools/MusicDatabase.hpp"
#include "tools/MusicScanManager.hpp"
#include "tools/AudioDeviceTool.hpp"
#include "views/MagicTuningView.hpp"
#include "views/DACSettingView.hpp"
#include "themes/ThemeManager.hpp"
#include "public/Platform.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <cmath>

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

// ==============================================================================
// 1. RFC 6455 WebSocket 握手加密算法 (标准 SHA-1 与 Base64，纯 C++ 零第三方依赖)
// ==============================================================================
namespace crypto {

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

inline std::string base64_encode(const unsigned char* src, size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t val = (src[i] << 16) | ((i + 1 < len ? src[i + 1] : 0) << 8) | (i + 2 < len ? src[i + 2] : 0);
        out.push_back(b64_table[(val >> 18) & 0x3F]);
        out.push_back(b64_table[(val >> 12) & 0x3F]);
        out.push_back(i + 1 < len ? b64_table[(val >> 6) & 0x3F] : '=');
        out.push_back(i + 2 < len ? b64_table[val & 0x3F] : '=');
    }
    return out;
}

inline uint32_t rol(uint32_t value, size_t bits) {
    return (value << bits) | (value >> (32 - bits));
}

inline void sha1(const std::string& input, unsigned char hash[20]) {
    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;

    std::vector<uint8_t> msg(input.begin(), input.end());
    uint64_t bit_len = msg.size() * 8;

    msg.push_back(0x80);
    while ((msg.size() % 64) != 56) {
        msg.push_back(0x00);
    }
    for (int i = 7; i >= 0; --i) {
        msg.push_back(static_cast<uint8_t>((bit_len >> (i * 8)) & 0xFF));
    }

    for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(msg[chunk + i * 4]) << 24) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 3]));
        }
        for (int i = 16; i < 80; ++i) {
            w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            uint32_t temp = rol(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rol(b, 30);
            b = a;
            a = temp;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    uint32_t final_h[5] = {h0, h1, h2, h3, h4};
    for (int i = 0; i < 5; ++i) {
        hash[i * 4]     = static_cast<unsigned char>((final_h[i] >> 24) & 0xFF);
        hash[i * 4 + 1] = static_cast<unsigned char>((final_h[i] >> 16) & 0xFF);
        hash[i * 4 + 2] = static_cast<unsigned char>((final_h[i] >> 8) & 0xFF);
        hash[i * 4 + 3] = static_cast<unsigned char>(final_h[i] & 0xFF);
    }
}

inline std::string compute_ws_accept(const std::string& key) {
    static const std::string guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = key + guid;
    unsigned char hash[20];
    sha1(combined, hash);
    return base64_encode(hash, 20);
}

} // namespace crypto

// 辅助 JSON 字符串转义
std::string escapeJson(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        if (c == '"') o << "\\\"";
        else if (c == '\\') o << "\\\\";
        else if (c == '\b') o << "\\b";
        else if (c == '\f') o << "\\f";
        else if (c == '\n') o << "\\n";
        else if (c == '\r') o << "\\r";
        else if (c == '\t') o << "\\t";
        else if (static_cast<unsigned char>(c) < 32) {
            o << "\\u00" << ((c >> 4) & 0xF) << (c & 0xF);
        } else {
            o << c;
        }
    }
    return o.str();
}

// ==============================================================================
// 2. 内嵌全功能 HiFi 控制中心 SPA H5 页面 (含 6 大核心模块与完美自适应 UI)
// ==============================================================================
const char* INDEX_HTML = R"rawhtml(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<meta name="theme-color" content="#090d14">
<title>PiHiFi 发烧纯音远程中枢</title>
<style>
:root {
  --bg-color: #090d14;
  --card-bg: rgba(22, 30, 46, 0.72);
  --card-border: rgba(255, 255, 255, 0.09);
  --accent: #10b981;
  --accent-glow: rgba(16, 185, 129, 0.35);
  --text-active: #ffffff;
  --text-normal: #cbd5e1;
  --text-muted: #64748b;
  --progress-bg: #1e293b;
  --danger: #ef4444;
}
* { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "PingFang SC", "Microsoft YaHei", sans-serif; -webkit-tap-highlight-color: transparent; }
body {
  background: var(--bg-color);
  color: var(--text-normal);
  min-height: 100vh;
  min-height: 100dvh;
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 16px 14px max(34px, env(safe-area-inset-bottom));
  background-image: radial-gradient(circle at 50% 10%, rgba(16, 185, 129, 0.10), transparent 55%);
}
.container { width: 100%; max-width: 760px; margin: 0 auto; box-sizing: border-box; }

/* 头部与状态 */
.header { text-align: center; margin-bottom: 14px; }
.header h1 { font-size: 20px; font-weight: 700; color: var(--text-active); letter-spacing: 0.5px; margin-bottom: 4px; }
.badge { display: inline-flex; align-items: center; gap: 6px; padding: 3px 12px; background: rgba(16, 185, 129, 0.12); border: 1px solid var(--accent); border-radius: 20px; color: var(--accent); font-size: 11px; font-weight: 600; }
.badge-dot { width: 7px; height: 7px; border-radius: 50%; background: var(--accent); box-shadow: 0 0 8px var(--accent); }
.badge.disconnected { background: rgba(239, 68, 68, 0.12); border-color: var(--danger); color: var(--danger); }
.badge.disconnected .badge-dot { background: var(--danger); box-shadow: 0 0 8px var(--danger); }

/* 顶部分段导航栏 (6 大核心功能) */
.nav-tabs {
  display: grid;
  grid-template-columns: repeat(6, 1fr);
  background: rgba(15, 23, 42, 0.7);
  border: 1px solid var(--card-border);
  border-radius: 12px;
  padding: 4px;
  margin-bottom: 16px;
  gap: 4px;
  backdrop-filter: blur(16px);
}
.nav-tab {
  text-align: center;
  padding: 8px 2px;
  font-size: 11px;
  font-weight: 600;
  border-radius: 8px;
  cursor: pointer;
  color: var(--text-muted);
  transition: all 0.2s ease;
  user-select: none;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}
.nav-tab.active {
  background: var(--accent);
  color: #0c1017;
  box-shadow: 0 2px 10px var(--accent-glow);
}

.tab-pane { display: none; }
.tab-pane.active { display: block; }

/* 通用毛玻璃卡片 */
.card {
  background: var(--card-bg);
  border: 1px solid var(--card-border);
  border-radius: 18px;
  padding: 20px 16px;
  box-shadow: 0 10px 32px rgba(0, 0, 0, 0.45);
  backdrop-filter: blur(20px);
  margin-bottom: 14px;
  box-sizing: border-box;
  overflow: hidden;
}

/* 子切换药丸按钮 */
.sub-switcher {
  display: flex;
  background: rgba(15, 23, 42, 0.6);
  border: 1px solid rgba(255, 255, 255, 0.08);
  border-radius: 10px;
  padding: 3px;
  margin-bottom: 16px;
  gap: 4px;
}
.sub-pill {
  flex: 1;
  text-align: center;
  padding: 6px 4px;
  font-size: 12px;
  font-weight: 600;
  border-radius: 7px;
  color: var(--text-muted);
  cursor: pointer;
  transition: all 0.2s;
  user-select: none;
}
.sub-pill.active {
  background: rgba(16, 185, 129, 0.18);
  color: var(--accent);
  border: 1px solid rgba(16, 185, 129, 0.4);
}

/* ================== Tab 1: 播放面板 ================== */
.disc-wrapper { display: flex; justify-content: center; align-items: center; margin: 4px 0 16px; }
.vinyl-disc {
  width: 140px;
  height: 140px;
  border-radius: 50%;
  background: radial-gradient(circle, #2a3344 0%, #151b26 30%, #080b10 70%, #151b26 100%);
  border: 3px solid rgba(255, 255, 255, 0.1);
  box-shadow: 0 8px 24px rgba(0, 0, 0, 0.65), inset 0 0 15px rgba(0,0,0,0.8);
  display: flex;
  justify-content: center;
  align-items: center;
  position: relative;
  animation: spin 16s linear infinite;
  animation-play-state: paused;
}
.vinyl-disc.playing { animation-play-state: running; }
.vinyl-center {
  width: 46px;
  height: 46px;
  border-radius: 50%;
  background: var(--accent);
  border: 2px solid #ffffff;
  display: flex;
  justify-content: center;
  align-items: center;
  color: #0c1017;
  font-size: 18px;
  box-shadow: 0 0 12px var(--accent-glow);
}
@keyframes spin { 100% { transform: rotate(360deg); } }

.track-info { text-align: center; margin-bottom: 14px; }
.track-title { font-size: 18px; font-weight: 700; color: var(--text-active); margin-bottom: 4px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.track-artist { font-size: 13px; color: var(--text-muted); margin-bottom: 8px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.format-badge { display: inline-block; padding: 2px 10px; background: rgba(255, 255, 255, 0.08); border-radius: 12px; font-size: 11px; color: var(--accent); border: 1px solid rgba(16, 185, 129, 0.3); }

/* 12 频段实时动态 LED 频谱条 */
.spectrum-wrap {
  display: flex;
  justify-content: center;
  align-items: flex-end;
  gap: 5px;
  height: 38px;
  margin: 10px 0 16px;
  padding: 0 20px;
}
.spec-bar {
  flex: 1;
  max-width: 14px;
  height: 4px;
  background: linear-gradient(to top, var(--accent), #34d399);
  border-radius: 3px 3px 1px 1px;
  transition: height 0.12s ease-out;
  box-shadow: 0 0 6px var(--accent-glow);
}

.progress-container { margin-bottom: 18px; }
.progress-bar-wrap { position: relative; width: 100%; height: 8px; background: var(--progress-bg); border-radius: 4px; cursor: pointer; touch-action: none; }
.progress-fill { height: 100%; width: 0%; background: var(--accent); border-radius: 4px; box-shadow: 0 0 10px var(--accent-glow); transition: width 0.08s linear; pointer-events: none; }
.time-labels { display: flex; justify-content: space-between; font-size: 12px; color: var(--text-muted); margin-top: 6px; }

.controls { display: flex; justify-content: center; align-items: center; gap: 16px; margin-bottom: 18px; }
.btn-icon { background: transparent; border: none; color: var(--text-normal); font-size: 20px; cursor: pointer; width: 42px; height: 42px; border-radius: 50%; display: flex; align-items: center; justify-content: center; transition: all 0.2s; }
.btn-icon:hover { background: rgba(255, 255, 255, 0.08); color: var(--text-active); }
.btn-play { width: 56px; height: 56px; background: var(--accent); color: #0c1017; font-size: 24px; border-radius: 50%; box-shadow: 0 4px 18px var(--accent-glow); }
.btn-play:hover { filter: brightness(1.1); transform: scale(1.04); }

.volume-wrap { display: flex; align-items: center; gap: 12px; padding: 0 6px; }
.vol-slider { flex: 1; -webkit-appearance: none; appearance: none; height: 6px; background: var(--progress-bg); border-radius: 3px; outline: none; }
.vol-slider::-webkit-slider-thumb { -webkit-appearance: none; appearance: none; width: 18px; height: 18px; border-radius: 50%; background: var(--accent); cursor: pointer; box-shadow: 0 0 8px var(--accent-glow); }
.vol-slider::-moz-range-thumb { width: 18px; height: 18px; border-radius: 50%; background: var(--accent); cursor: pointer; border: none; }

/* ================== Tab 2: 播放列表与歌单 ================== */
.queue-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 14px; }
.queue-title { font-size: 14px; font-weight: 700; color: var(--text-active); }
.song-list { display: flex; flex-direction: column; gap: 6px; max-height: 480px; overflow-y: auto; padding-right: 2px; }
.song-item { display: flex; justify-content: space-between; align-items: center; padding: 10px 12px; border-radius: 10px; background: rgba(15, 23, 42, 0.5); border: 1px solid rgba(255, 255, 255, 0.05); cursor: pointer; transition: all 0.2s; }
.song-item:hover { background: rgba(16, 185, 129, 0.08); border-color: rgba(16, 185, 129, 0.25); }
.song-item.active { background: rgba(16, 185, 129, 0.15); border-color: var(--accent); color: var(--accent); }
.song-left { display: flex; align-items: center; gap: 10px; flex: 1; min-width: 0; }
.song-idx { font-size: 12px; font-weight: 700; color: var(--text-muted); width: 22px; text-align: center; }
.song-item.active .song-idx { color: var(--accent); }
.song-main { flex: 1; min-width: 0; }
.song-title { font-size: 13px; font-weight: 600; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.song-sub { font-size: 11px; color: var(--text-muted); margin-top: 2px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.song-actions { display: flex; align-items: center; gap: 8px; }
.song-meta { font-size: 11px; color: var(--text-muted); text-align: right; white-space: nowrap; }
.btn-del { background: transparent; border: none; color: var(--text-muted); cursor: pointer; font-size: 14px; padding: 4px; border-radius: 4px; transition: color 0.2s; }
.btn-del:hover { color: var(--danger); }

/* 歌单网格卡片 */
.playlists-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(200px, 1fr)); gap: 10px; margin-top: 8px; }
.playlist-card { background: rgba(15, 23, 42, 0.55); border: 1px solid rgba(255, 255, 255, 0.08); border-radius: 12px; padding: 12px; display: flex; flex-direction: column; justify-content: space-between; transition: all 0.2s; }
.playlist-card:hover { border-color: var(--accent); transform: translateY(-2px); }
.playlist-header { display: flex; align-items: center; gap: 8px; margin-bottom: 6px; }
.playlist-name { font-size: 13px; font-weight: 700; color: var(--text-active); overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.playlist-count { font-size: 11px; color: var(--text-muted); margin-bottom: 10px; }
.playlist-btns { display: flex; gap: 6px; }

/* ================== Tab 3: 曲库与搜索 ================== */
.lib-header { display: flex; gap: 10px; margin-bottom: 12px; }
.lib-search { flex: 1; background: rgba(15, 23, 42, 0.7); border: 1px solid var(--card-border); border-radius: 10px; padding: 8px 12px; color: var(--text-active); font-size: 13px; outline: none; }
.lib-search:focus { border-color: var(--accent); }
.btn-sm { padding: 8px 14px; border-radius: 10px; border: 1px solid rgba(255, 255, 255, 0.12); background: rgba(255, 255, 255, 0.05); color: var(--text-active); font-size: 12px; font-weight: 600; cursor: pointer; white-space: nowrap; transition: all 0.2s; }
.btn-sm:hover { background: rgba(16, 185, 129, 0.15); border-color: var(--accent); color: var(--accent); }
.btn-sm.primary { background: var(--accent); color: #0c1017; border-color: var(--accent); }

/* ================== Tab 4: 调音 (EQ + 魔棒) ================== */
.eq-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 14px; }
.eq-switch-wrap { display: flex; align-items: center; gap: 8px; font-size: 13px; font-weight: 600; color: var(--text-active); }
.switch { position: relative; display: inline-block; width: 44px; height: 24px; }
.switch input { opacity: 0; width: 0; height: 0; }
.slider-toggle { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #334155; transition: .3s; border-radius: 24px; }
.slider-toggle:before { position: absolute; content: ""; height: 18px; width: 18px; left: 3px; bottom: 3px; background-color: white; transition: .3s; border-radius: 50%; }
input:checked + .slider-toggle { background-color: var(--accent); }
input:checked + .slider-toggle:before { transform: translateX(20px); }

.presets-scroll { display: flex; gap: 6px; overflow-x: auto; padding-bottom: 8px; margin-bottom: 14px; -webkit-overflow-scrolling: touch; scrollbar-width: none; }
.presets-scroll::-webkit-scrollbar { display: none; }
.preset-chip { padding: 6px 12px; background: rgba(255, 255, 255, 0.06); border: 1px solid rgba(255, 255, 255, 0.1); border-radius: 16px; font-size: 12px; white-space: nowrap; cursor: pointer; transition: all 0.2s; color: var(--text-normal); }
.preset-chip.active, .preset-chip:hover { background: rgba(16, 185, 129, 0.15); border-color: var(--accent); color: var(--accent); }

/* 10 段垂直滑动推子 (完美限制在父容器内，杜绝溢出) */
.eq-scroll-container { width: 100%; overflow-x: auto; -webkit-overflow-scrolling: touch; scrollbar-width: thin; padding-bottom: 4px; box-sizing: border-box; }
.eq-sliders-grid {
  display: grid;
  grid-template-columns: repeat(10, minmax(36px, 1fr));
  gap: 4px;
  width: 100%;
  box-sizing: border-box;
  padding: 10px 0;
  align-items: center;
  justify-items: center;
}
.eq-col {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: space-between;
  width: 100%;
  height: 180px;
  box-sizing: border-box;
}
.eq-vslider-wrap {
  height: 120px;
  width: 32px;
  display: flex;
  align-items: center;
  justify-content: center;
  position: relative;
}
.eq-vslider {
  writing-mode: vertical-lr;
  direction: rtl;
  -webkit-appearance: slider-vertical;
  appearance: slider-vertical;
  width: 24px;
  height: 120px;
  padding: 0;
  margin: 0;
  accent-color: var(--accent);
  cursor: pointer;
}
.eq-gain-lbl { font-size: 11px; color: var(--accent); font-weight: 600; min-height: 16px; line-height: 16px; }
.eq-freq-lbl { font-size: 11px; color: var(--text-muted); font-weight: 500; min-height: 16px; line-height: 16px; }

/* MSEB 调音魔棒界面 */
.mseb-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
  gap: 10px;
  margin-top: 12px;
}
.mseb-card {
  background: rgba(15, 23, 42, 0.55);
  border: 1px solid rgba(255, 255, 255, 0.07);
  border-radius: 12px;
  padding: 10px 14px;
}
.mseb-top {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 6px;
}
.mseb-name {
  font-size: 13px;
  font-weight: 600;
  color: var(--text-active);
}
.mseb-val {
  font-size: 12px;
  font-weight: 700;
  color: var(--accent);
}
.mseb-slider-row {
  display: flex;
  align-items: center;
  gap: 8px;
}
.mseb-lbl-l, .mseb-lbl-r {
  font-size: 10px;
  color: var(--text-muted);
  white-space: nowrap;
  width: 48px;
}
.mseb-lbl-l { text-align: right; }
.mseb-lbl-r { text-align: left; }
.mseb-slider {
  flex: 1;
  -webkit-appearance: none;
  appearance: none;
  height: 6px;
  background: var(--progress-bg);
  border-radius: 3px;
  outline: none;
}
.mseb-slider::-webkit-slider-thumb {
  -webkit-appearance: none;
  appearance: none;
  width: 16px;
  height: 16px;
  border-radius: 50%;
  background: var(--accent);
  cursor: pointer;
  box-shadow: 0 0 6px var(--accent-glow);
}

/* ================== Tab 5: DAC 硬件与解码模式 ================== */
.dac-hw-banner {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 12px 14px;
  background: rgba(15, 23, 42, 0.65);
  border: 1px solid rgba(255, 255, 255, 0.08);
  border-radius: 12px;
  margin-bottom: 16px;
}
.dac-hw-icon { font-size: 24px; color: var(--accent); }
.dac-hw-info { flex: 1; min-width: 0; }
.dac-hw-name { font-size: 14px; font-weight: 700; color: var(--text-active); overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.dac-hw-desc { font-size: 11px; color: var(--text-muted); margin-top: 2px; }

.dac-chips-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
  gap: 8px;
  margin-bottom: 16px;
}
.dac-chip-card {
  padding: 10px;
  border-radius: 10px;
  background: rgba(15, 23, 42, 0.5);
  border: 1px solid rgba(255, 255, 255, 0.07);
  cursor: pointer;
  transition: all 0.2s;
  text-align: center;
}
.dac-chip-card:hover { border-color: rgba(16, 185, 129, 0.4); }
.dac-chip-card.active {
  background: rgba(16, 185, 129, 0.15);
  border-color: var(--accent);
  color: var(--accent);
  box-shadow: 0 2px 12px var(--accent-glow);
}
.dac-chip-name { font-size: 12px; font-weight: 700; margin-bottom: 2px; }
.dac-chip-brand { font-size: 10px; color: var(--text-muted); }
.dac-chip-card.active .dac-chip-brand { color: var(--accent); opacity: 0.85; }

.dac-opts-group { display: flex; flex-direction: column; gap: 12px; }
.dac-opt-row { display: flex; flex-direction: column; gap: 6px; }
.dac-opt-lbl { font-size: 12px; font-weight: 600; color: var(--text-active); }
.dac-opt-btns { display: flex; flex-wrap: wrap; gap: 6px; }
.dac-opt-btn {
  padding: 6px 12px;
  border-radius: 8px;
  background: rgba(15, 23, 42, 0.5);
  border: 1px solid rgba(255, 255, 255, 0.08);
  color: var(--text-normal);
  font-size: 11px;
  cursor: pointer;
  transition: all 0.2s;
}
.dac-opt-btn:hover { border-color: rgba(16, 185, 129, 0.3); }
.dac-opt-btn.active {
  background: var(--accent);
  color: #0c1017;
  font-weight: 700;
  border-color: var(--accent);
  box-shadow: 0 2px 8px var(--accent-glow);
}

/* ================== Tab 6: 视觉与系统设定 ================== */
.section-title { font-size: 13px; font-weight: 700; color: var(--text-active); margin-bottom: 10px; }
.grid-2col { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; margin-bottom: 16px; }
.theme-card { padding: 10px; border-radius: 10px; background: rgba(15, 23, 42, 0.5); border: 1px solid rgba(255, 255, 255, 0.06); cursor: pointer; display: flex; align-items: center; gap: 8px; transition: all 0.2s; }
.theme-card:hover { border-color: var(--accent); }
.theme-card.active { background: rgba(16, 185, 129, 0.15); border-color: var(--accent); color: var(--accent); }
.theme-dot { width: 12px; height: 12px; border-radius: 50%; flex-shrink: 0; }
.theme-name { font-size: 12px; font-weight: 600; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }

.settings-row { display: flex; justify-content: space-between; align-items: center; padding: 12px 0; border-bottom: 1px solid rgba(255, 255, 255, 0.05); }
.settings-row:last-child { border-bottom: none; }
.settings-lbl { font-size: 13px; color: var(--text-active); }
.settings-sub { font-size: 11px; color: var(--text-muted); margin-top: 2px; }
.timer-btns { display: flex; gap: 6px; }
.timer-btn { padding: 4px 10px; border-radius: 8px; background: rgba(255, 255, 255, 0.05); border: 1px solid rgba(255, 255, 255, 0.1); font-size: 11px; color: var(--text-normal); cursor: pointer; }
.timer-btn.active { background: var(--accent); color: #0c1017; border-color: var(--accent); font-weight: 700; }

/* 模态弹窗 */
.modal-overlay { position: fixed; top: 0; left: 0; right: 0; bottom: 0; background: rgba(0,0,0,0.7); backdrop-filter: blur(8px); display: none; align-items: center; justify-content: center; z-index: 999; }
.modal-overlay.active { display: flex; }
.modal-box { background: var(--bg-color); border: 1px solid var(--card-border); border-radius: 16px; padding: 20px; width: 90%; max-width: 360px; box-shadow: 0 10px 40px rgba(0,0,0,0.8); }
.modal-title { font-size: 15px; font-weight: 700; color: var(--text-active); margin-bottom: 12px; }
.modal-input { width: 100%; background: rgba(15, 23, 42, 0.8); border: 1px solid var(--card-border); border-radius: 8px; padding: 10px; color: var(--text-active); font-size: 13px; margin-bottom: 16px; outline: none; }
.modal-btns { display: flex; justify-content: flex-end; gap: 8px; }
</style>
</head>
<body>

<div class="container">
  <!-- 头部信息 -->
  <div class="header">
    <h1>PiHiFi 发烧纯音远程中枢</h1>
    <div class="badge disconnected" id="connStatus">
      <div class="badge-dot"></div>
      <span id="connText">正在连接数播 WebSocket...</span>
    </div>
  </div>

  <!-- 顶部分段导航 (6 大功能模块) -->
  <div class="nav-tabs">
    <div class="nav-tab active" data-tab="tabPlayer">♪ 播放</div>
    <div class="nav-tab" data-tab="tabQueue">≡ 列表</div>
    <div class="nav-tab" data-tab="tabLib">📁 曲库</div>
    <div class="nav-tab" data-tab="tabEQ">🎚 调音</div>
    <div class="nav-tab" data-tab="tabDAC">⚡ DAC</div>
    <div class="nav-tab" data-tab="tabSettings">⚙ 设置</div>
  </div>

  <!-- ==================== Tab 1: 正在播放 ==================== -->
  <div class="tab-pane active" id="tabPlayer">
    <div class="card">
      <div class="disc-wrapper">
        <div class="vinyl-disc" id="vinylDisc">
          <div class="vinyl-center">&#9835;</div>
        </div>
      </div>

      <div class="track-info">
        <div class="track-title" id="trackTitle">暂未载入曲目</div>
        <div class="track-artist" id="trackArtist">未知艺术家</div>
        <div class="format-badge" id="formatBadge">Hi-Res 直通</div>
      </div>

      <!-- 12 频段实时 LED 动态跳动频谱 -->
      <div class="spectrum-wrap" id="spectrumBar">
        <div class="spec-bar"></div><div class="spec-bar"></div><div class="spec-bar"></div>
        <div class="spec-bar"></div><div class="spec-bar"></div><div class="spec-bar"></div>
        <div class="spec-bar"></div><div class="spec-bar"></div><div class="spec-bar"></div>
        <div class="spec-bar"></div><div class="spec-bar"></div><div class="spec-bar"></div>
      </div>

      <!-- 时间轴进度条 -->
      <div class="progress-container">
        <div class="progress-bar-wrap" id="progressBar">
          <div class="progress-fill" id="progressFill"></div>
        </div>
        <div class="time-labels">
          <span id="timeCurrent">00:00</span>
          <span id="timeTotal">00:00</span>
        </div>
      </div>

      <!-- 核心播放控制 -->
      <div class="controls">
        <button class="btn-icon" id="btnMode" title="循环模式">&#10227;</button>
        <button class="btn-icon" id="btnPrev" title="上一曲">&#9198;</button>
        <button class="btn-icon btn-play" id="btnPlay" title="播放/暂停">&#9654;</button>
        <button class="btn-icon" id="btnNext" title="下一曲">&#9197;</button>
        <button class="btn-icon" id="btnMute" title="静音">&#128266;</button>
      </div>

      <!-- 硬件音量旋钮控制 -->
      <div class="volume-wrap">
        <span style="font-size: 14px; color: var(--text-muted);">&#128265;</span>
        <input type="range" class="vol-slider" id="volSlider" min="0" max="100" value="80">
        <span id="volLabel" style="font-size: 12px; color: var(--text-muted); min-width: 32px;">80%</span>
      </div>
    </div>
  </div>

  <!-- ==================== Tab 2: 播放列表与歌单 ==================== -->
  <div class="tab-pane" id="tabQueue">
    <div class="card">
      <div class="sub-switcher">
        <div class="sub-pill active" id="pillQueue">正在播放队列 (<span id="qCountBadge">0</span>)</div>
        <div class="sub-pill" id="pillPlaylists">发烧歌单 (<span id="plCountBadge">0</span>)</div>
      </div>

      <!-- 正在播放队列子面板 -->
      <div id="paneQueueList">
        <div class="queue-header">
          <div class="queue-title">播放队列 · 共 <span id="qTotal">0</span> 首</div>
          <button class="btn-sm" id="btnClearQueue">清空队列</button>
        </div>
        <div class="song-list" id="queueList">
          <div style="text-align: center; color: var(--text-muted); padding: 30px 0; font-size: 13px;">队列为空</div>
        </div>
      </div>

      <!-- 发烧歌单子面板 -->
      <div id="panePlaylistsList" style="display: none;">
        <div class="queue-header">
          <div class="queue-title">我的歌单 · 共 <span id="plTotal">0</span> 个</div>
          <button class="btn-sm primary" id="btnOpenCreatePlModal">＋ 新建歌单</button>
        </div>
        <div class="playlists-grid" id="playlistsGrid">
          <!-- 动态渲染歌单卡片 -->
        </div>
      </div>
    </div>
  </div>

  <!-- ==================== Tab 3: 本地曲库与搜索 ==================== -->
  <div class="tab-pane" id="tabLib">
    <div class="card">
      <div class="lib-header">
        <input type="text" class="lib-search" id="libSearchInput" placeholder="搜索歌曲、艺术家或专辑...">
        <button class="btn-sm primary" id="btnScanMusic">&#8635; 扫描曲库</button>
      </div>
      <div style="font-size: 11px; color: var(--text-muted); margin-bottom: 10px;">全部本地扫描曲目 · 共 <span id="libTotal">0</span> 首</div>
      <div class="song-list" id="libSongList">
        <div style="text-align: center; color: var(--text-muted); padding: 30px 0; font-size: 13px;">正在载入曲库...</div>
      </div>
    </div>
  </div>

  <!-- ==================== Tab 4: 调音 (EQ + 魔棒) ==================== -->
  <div class="tab-pane" id="tabEQ">
    <div class="card">
      <div class="sub-switcher">
        <div class="sub-pill active" id="pillEq">10段专业图形EQ</div>
        <div class="sub-pill" id="pillMseb">🪄 调音魔棒 (MSEB)</div>
      </div>

      <!-- 10段图形EQ子面板 -->
      <div id="paneEq">
        <div class="eq-header">
          <div class="eq-switch-wrap">
            <span>图形EQ开关</span>
            <label class="switch">
              <input type="checkbox" id="eqToggle" checked>
              <span class="slider-toggle"></span>
            </label>
          </div>
          <button class="btn-sm" id="btnResetEq">一键归零</button>
        </div>

        <div class="presets-scroll" id="eqPresetsList">
          <div class="preset-chip active" data-preset="flat">原音直通</div>
          <div class="preset-chip" data-preset="pop">流行音乐</div>
          <div class="preset-chip" data-preset="rock">摇滚现场</div>
          <div class="preset-chip" data-preset="vocal">经典人声</div>
          <div class="preset-chip" data-preset="classic">古典交响</div>
          <div class="preset-chip" data-preset="bass">震撼低音</div>
          <div class="preset-chip" data-preset="treble">通透高音</div>
        </div>

        <!-- 10 频段垂直滑块容器 (带滚动与绝对防溢出) -->
        <div class="eq-scroll-container">
          <div class="eq-sliders-grid" id="eqSliders">
            <!-- 由 JS 渲染 10 列推子 (31Hz ~ 16kHz) -->
          </div>
        </div>
      </div>

      <!-- MSEB 调音魔棒子面板 -->
      <div id="paneMseb" style="display: none;">
        <div class="eq-header">
          <div class="eq-switch-wrap">
            <span>魔棒调音</span>
            <label class="switch">
              <input type="checkbox" id="msebToggle" checked>
              <span class="slider-toggle"></span>
            </label>
          </div>
          <button class="btn-sm" id="btnResetMseb">全部归零</button>
        </div>
        <div style="font-size: 11px; color: var(--text-muted); margin-bottom: 8px;">心理声学主观听感微调 · 实时映射高精度双二阶滤波器</div>
        <div class="mseb-grid" id="msebGrid">
          <!-- 由 JS 动态渲染 10 个听感滑块 -->
        </div>
      </div>
    </div>
  </div>

  <!-- ==================== Tab 5: DAC 硬件与解码模式 ==================== -->
  <div class="tab-pane" id="tabDAC">
    <div class="card">
      <!-- 物理硬件连接状态卡片 -->
      <div class="dac-hw-banner">
        <div class="dac-hw-icon" id="dacStatusIcon">&#9889;</div>
        <div class="dac-hw-info">
          <div class="dac-hw-name" id="dacDeviceName">物理 DAC 检测中...</div>
          <div class="dac-hw-desc" id="dacDeviceDesc">当前输出设备: 默认音频输出</div>
        </div>
      </div>

      <div class="section-title">6 大名片解码芯片架构选择</div>
      <div class="dac-chips-grid" id="dacChipsGrid">
        <div class="dac-chip-card active" data-chip="0">
          <div class="dac-chip-name">Apple Direct</div>
          <div class="dac-chip-brand">Mac/iOS 硬件直通</div>
        </div>
        <div class="dac-chip-card" data-chip="1">
          <div class="dac-chip-name">ESS Sabre</div>
          <div class="dac-chip-brand">ES9038PRO 8-Ch并联</div>
        </div>
        <div class="dac-chip-card" data-chip="2">
          <div class="dac-chip-name">AKM 旭化成</div>
          <div class="dac-chip-brand">AK4499EX Velvet</div>
        </div>
        <div class="dac-chip-card" data-chip="3">
          <div class="dac-chip-name">Cirrus Logic</div>
          <div class="dac-chip-brand">CS43198 MasterHIFI</div>
        </div>
        <div class="dac-chip-card" data-chip="4">
          <div class="dac-chip-name">R-2R 梯形电阻</div>
          <div class="dac-chip-brand">纯分立 NOS/OS</div>
        </div>
        <div class="dac-chip-card" data-chip="5">
          <div class="dac-chip-name">ROHM 罗姆</div>
          <div class="dac-chip-brand">MUS-IC BD34301</div>
        </div>
      </div>

      <div class="section-title">芯片专属发烧硬件参数配置</div>
      <div class="dac-opts-group" id="dacOptionsContainer">
        <!-- 由 JS 动态根据当前选中的芯片渲染参数行 -->
      </div>
    </div>
  </div>

  <!-- ==================== Tab 6: 视觉主题与系统设定 ==================== -->
  <div class="tab-pane" id="tabSettings">
    <div class="card">
      <div class="section-title">机身名机皇主题配色 (8 款经典)</div>
      <div class="grid-2col" id="themesGrid">
        <div class="theme-card active" data-theme="0"><div class="theme-dot" style="background:#FA2D48;"></div><div class="theme-name">现代深空玫红</div></div>
        <div class="theme-card" data-theme="1"><div class="theme-dot" style="background:#00B4D8;"></div><div class="theme-name">麦景图湖蓝</div></div>
        <div class="theme-card" data-theme="2"><div class="theme-dot" style="background:#E2C792;"></div><div class="theme-name">金嗓子香槟金</div></div>
        <div class="theme-card" data-theme="3"><div class="theme-dot" style="background:#F59E0B;"></div><div class="theme-name">复古琥珀卡座</div></div>
        <div class="theme-card" data-theme="5"><div class="theme-dot" style="background:#10B981;"></div><div class="theme-name">马兰士翡翠绿</div></div>
        <div class="theme-card" data-theme="6"><div class="theme-dot" style="background:#94A3B8;"></div><div class="theme-name">柏林之声冷银</div></div>
        <div class="theme-card" data-theme="7"><div class="theme-dot" style="background:#22C55E;"></div><div class="theme-name">英国名暗夜翠</div></div>
        <div class="theme-card" data-theme="8"><div class="theme-dot" style="background:#EF4444;"></div><div class="theme-name">马克莱文森赤晶</div></div>
      </div>

      <div class="section-title">数播大屏全景动效模式 (11 种)</div>
      <div class="grid-2col" id="visualsGrid">
        <div class="theme-card active" data-vmode="2"><div class="theme-name">金嗓子精密大表头</div></div>
        <div class="theme-card" data-vmode="1"><div class="theme-name">麦景图动圈双表头</div></div>
        <div class="theme-card" data-vmode="0"><div class="theme-name">48列全景LED频谱</div></div>
        <div class="theme-card" data-vmode="3"><div class="theme-name">复古开盘磁带机</div></div>
        <div class="theme-card" data-vmode="4"><div class="theme-name">Siri流光正弦波</div></div>
        <div class="theme-card" data-vmode="5"><div class="theme-name">Siri悬浮微光光球</div></div>
        <div class="theme-card" data-vmode="6"><div class="theme-name">音乐共鸣升腾气泡</div></div>
        <div class="theme-card" data-vmode="8"><div class="theme-name">电光霓虹脉冲波</div></div>
        <div class="theme-card" data-vmode="9"><div class="theme-name">赛博3D粒子网格</div></div>
        <div class="theme-card" data-vmode="10"><div class="theme-name">液态StandBy时钟</div></div>
        <div class="theme-card" data-vmode="7"><div class="theme-name">纯净暗黑省电模式</div></div>
      </div>

      <div class="section-title">发烧级输出与睡眠定时</div>
      <div class="settings-row">
        <div>
          <div class="settings-lbl">切歌平滑淡入淡出</div>
          <div class="settings-sub">Hann 窗口无缝音频插值防爆音</div>
        </div>
        <input type="range" id="fadeSlider" min="0" max="2" step="0.1" value="0.5" style="width: 100px;">
      </div>
      <div class="settings-row">
        <div>
          <div class="settings-lbl">睡眠定时关机</div>
          <div class="settings-sub" id="sleepTimerStatus">未开启睡眠定时</div>
        </div>
        <div class="timer-btns">
          <button class="timer-btn" data-min="15">15分</button>
          <button class="timer-btn" data-min="30">30分</button>
          <button class="timer-btn" data-min="60">60分</button>
          <button class="timer-btn active" data-min="0">关闭</button>
        </div>
      </div>
      <div class="settings-row">
        <div>
          <div class="settings-lbl">Bit-Perfect 源码输出</div>
          <div class="settings-sub">100% 原始数据无衰减硬件直通</div>
        </div>
        <span class="badge" id="bitPerfectBadge">生效中</span>
      </div>
      <div class="settings-row">
        <div>
          <div class="settings-lbl">WiFi 网页无线传歌</div>
          <div class="settings-sub">打开同机 8080 端口上传音乐</div>
        </div>
        <a id="btnWifiUploadLink" href="#" target="_blank" class="btn-sm" style="text-decoration:none; display:inline-block;">打开传歌页面 &rarr;</a>
      </div>
    </div>
  </div>
</div>

<!-- 新建歌单模态框 -->
<div class="modal-overlay" id="createPlModal">
  <div class="modal-box">
    <div class="modal-title">新建发烧自定义歌单</div>
    <input type="text" class="modal-input" id="newPlNameInput" placeholder="输入歌单名称 (例如: 深夜人声)..." maxlength="32">
    <div class="modal-btns">
      <button class="btn-sm" id="btnCancelCreatePl">取消</button>
      <button class="btn-sm primary" id="btnConfirmCreatePl">立即创建</button>
    </div>
  </div>
</div>

<script>
// ==================== 全局状态与 WebSocket 引擎 ====================
let ws = null;
let isSeeking = false;
let currentDuration = 0;
let queueData = [];
let playlistsData = [];
let libraryData = [];
let dacConfigData = null;
let activeDacChip = 0;

// MSEB 参数定义
const msebDefs = [
  { name: "声音冷暖", left: "偏冷通透", right: "偏暖醇厚" },
  { name: "低音下潜", left: "紧致轻盈", right: "深沉澎湃" },
  { name: "低音质感", left: "速度迅捷", right: "蓬松弹性" },
  { name: "音符厚度", left: "纤细轻盈", right: "扎实饱满" },
  { name: "人声位置", left: "舞台后缩", right: "贴耳靠前" },
  { name: "女声甜度", left: "自然平直", right: "润泽甜美" },
  { name: "齿音消除", left: "柔化去刺", right: "原生锋芒" },
  { name: "冲激响应", left: "柔和松弛", right: "硬朗凌厉" },
  { name: "空气感",   left: "凝聚内敛", right: "空灵弥漫" },
  { name: "声场重塑", left: "紧凑聚焦", right: "宏大宽广" }
];

// 10 频段 EQ 定义
const eqFreqs = ['31Hz','62Hz','125Hz','250Hz','500Hz','1kHz','2kHz','4kHz','8kHz','16kHz'];
const eqPresets = {
  flat:    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
  pop:     [1.5, 2.0, 1.0, 0, -1.0, -1.0, 0.5, 1.5, 2.5, 3.0],
  rock:    [4.0, 3.0, 1.5, 0, -1.0, -0.5, 1.5, 3.0, 4.0, 4.5],
  vocal:   [-1.5, -1.0, 0, 1.5, 3.0, 3.5, 2.5, 1.0, 0, -0.5],
  classic: [3.5, 2.5, 1.5, 0.5, 0, 0, 1.0, 2.0, 3.0, 3.5],
  bass:    [6.0, 5.0, 3.5, 2.0, 1.0, 0, 0, 0, 0, 0],
  treble:  [-1.0, -0.5, 0, 0, 0.5, 1.0, 2.5, 4.0, 5.5, 6.0]
};

// DAC 参数项定义表
const dacChipOptions = {
  0: [ // Apple Direct
    { key: "apple_exclusive", label: "输出模式", opts: ["Bit-Perfect 独占流", "系统混音共享"] },
    { key: "apple_sample_rate", label: "采样率模式", opts: ["原生跟随母带", "固定 96kHz", "固定 192kHz"] },
    { key: "apple_drive", label: "耳放输出电平", opts: ["智能阻抗自适应", "强制高输出 3.0Vrms", "标准输出 1.25Vrms"] },
    { key: "apple_bit_depth", label: "定点/浮点精度", opts: ["32-bit Float 直通", "24-bit 整数定点"] }
  ],
  1: [ // ESS Sabre
    { key: "pcm_filter", label: "PCM 数字滤波", opts: ["快速最小", "慢速最小", "快速线性", "慢速线性", "变迹滤波", "砖墙滤波", "混合滤波"] },
    { key: "dsd_bypass", label: "DSD 直通通道", opts: ["Direct 1-Bit 直通", "FIR 模拟滤波"] },
    { key: "pcm_dpll", label: "DPLL 抖动抑制带宽", opts: ["极窄带 (发烧低抖)", "标准平衡", "宽带 (兼容)"] },
    { key: "mono_mode", label: "芯片输出架构", opts: ["双芯片 8-Ch 并联", "双芯片立体声"] },
    { key: "output_mode", label: "模拟输出级", opts: ["固定后级 (Line-Out 4.2V)", "可调前级 (Pre-Out)"] },
    { key: "phase", label: "绝对极性", opts: ["绝对正相 (0°)", "极性反转 (180°)"] }
  ],
  2: [ // AKM Velvet
    { key: "akm_filter", label: "Velvet 数字滤波器", opts: ["短延时锐滚降", "短延时慢滚降", "锐滚降", "慢滚降", "超低群延迟", "低色散"] },
    { key: "akm_color", label: "声音色彩调节", opts: ["自然温润", "细腻通透", "动感宽厚", "极简监听"] },
    { key: "akm_dsd", label: "DSD 解码模式", opts: ["Direct 旁路调制", "Normal 滤波模式"] }
  ],
  3: [ // Cirrus Logic
    { key: "cs_filter", label: "MasterHIFI 滤波", opts: ["快速最小", "慢速最小", "快速线性", "慢速线性", "NOS 无过采样"] },
    { key: "cs_dsd", label: "DSD 处理通道", opts: ["Direct DSD 直通", "DoP 硬件解调"] },
    { key: "cs_drive", label: "驱动推力电平", opts: ["高推力差分 (2Vrms)", "标准单端 (1Vrms)"] }
  ],
  4: [ // R-2R
    { key: "r2r_mode", label: "超采样架构", opts: ["NOS 纯无过采样", "OS 线性相位", "OS 最小相位"] },
    { key: "r2r_dsd", label: "DSD 解码方式", opts: ["独立 1-Bit 电阻网络", "转为 24-bit 阶梯解码"] },
    { key: "r2r_clock", label: "飞秒 FIFO 重整", opts: ["本地 FIFO 重整", "跟随输入时钟"] },
    { key: "r2r_phase", label: "相位输出", opts: ["绝对正相 (0°)", "极性反转 (180°)"] }
  ],
  5: [ // ROHM
    { key: "rohm_filter", label: "MUS-IC 滤波器", opts: ["Sharp Roll-Off (高解析)", "Slow Roll-Off (自然松弛)"] },
    { key: "rohm_clock", label: "调制器主时钟", opts: ["智能倍频匹配", "锁定 64x fs", "锁定 128x fs"] },
    { key: "rohm_dsd", label: "模拟音频通道", opts: ["Direct Path 纯模拟", "标准多级滤波"] }
  ]
};

// 格式化秒数为 mm:ss
function formatTime(sec) {
  if (isNaN(sec) || sec < 0) return "00:00";
  let m = Math.floor(sec / 60);
  let s = Math.floor(sec % 60);
  return (m < 10 ? "0" + m : m) + ":" + (s < 10 ? "0" + s : s);
}

// 建立 WebSocket 连接
function initWebSocket() {
  const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  const wsUrl = protocol + '//' + window.location.host;
  
  ws = new WebSocket(wsUrl);

  ws.onopen = () => {
    document.getElementById('connStatus').className = 'badge';
    document.getElementById('connText').textContent = 'WebSocket 毫秒级同步在线';
  };

  ws.onclose = () => {
    document.getElementById('connStatus').className = 'badge disconnected';
    document.getElementById('connText').textContent = '未连接数播 (请确保数播端“Web服务”已开启)';
    setTimeout(initWebSocket, 2000);
  };

  ws.onerror = () => {
    document.getElementById('connStatus').className = 'badge disconnected';
  };

  ws.onmessage = (e) => {
    try {
      const data = JSON.parse(e.data);
      if (data.type === 'state') updateState(data);
      else if (data.type === 'queue') updateQueue(data);
      else if (data.type === 'playlists') updatePlaylists(data);
      else if (data.type === 'library') updateLibrary(data);
      else if (data.type === 'dac') updateDac(data);
      else if (data.type === 'mseb') updateMseb(data);
    } catch (err) {
      console.error("WS Parse err:", err);
    }
  };
}

function sendAction(msg) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify(msg));
  }
}

// ==================== 状态更新与渲染 ====================
function updateState(s) {
  document.getElementById('trackTitle').textContent = s.title || '暂未载入曲目';
  document.getElementById('trackArtist').textContent = s.artist || '未知艺术家';
  document.getElementById('formatBadge').textContent = s.specs || 'Hi-Res 直通';

  const disc = document.getElementById('vinylDisc');
  const btnPlay = document.getElementById('btnPlay');
  if (s.is_playing) {
    disc.classList.add('playing');
    btnPlay.innerHTML = '&#10074;&#10074;';
  } else {
    disc.classList.remove('playing');
    btnPlay.innerHTML = '&#9654;';
  }

  currentDuration = s.duration || 0;
  document.getElementById('timeTotal').textContent = formatTime(currentDuration);

  if (!isSeeking) {
    const curTime = s.current_time || 0;
    document.getElementById('timeCurrent').textContent = formatTime(curTime);
    const pct = currentDuration > 0 ? (curTime / currentDuration) * 100 : 0;
    document.getElementById('progressFill').style.width = pct + '%';
  }

  // 音量
  const volPct = Math.round((s.volume || 0.8) * 100);
  document.getElementById('volSlider').value = volPct;
  document.getElementById('volLabel').textContent = volPct + '%';

  // 静音按钮
  document.getElementById('btnMute').innerHTML = s.is_muted ? '&#128263;' : '&#128266;';

  // 模式
  const modeBtn = document.getElementById('btnMode');
  if (s.mode === 'single') modeBtn.innerHTML = '&#128472;';
  else if (s.mode === 'shuffle') modeBtn.innerHTML = '&#128256;';
  else modeBtn.innerHTML = '&#10227;';

  // 12 频段频谱跳动
  if (s.spectrum && s.spectrum.length === 12) {
    const bars = document.querySelectorAll('.spec-bar');
    s.spectrum.forEach((v, idx) => {
      if (bars[idx]) {
        const h = Math.max(4, Math.round(v * 36));
        bars[idx].style.height = h + 'px';
      }
    });
  }

  // 均衡器开关与增益
  document.getElementById('eqToggle').checked = s.eq_enabled;
  if (s.eq_bands && s.eq_bands.length === 10) {
    s.eq_bands.forEach((g, idx) => {
      const slider = document.querySelector(`.eq-vslider[data-band="${idx}"]`);
      if (slider) slider.value = g;
      const lbl = document.getElementById(`eqGain${idx}`);
      if (lbl) lbl.textContent = (g > 0 ? '+' : '') + g.toFixed(1);
    });
  }

  // 调音魔棒状态与数值
  if (s.mseb_enabled !== undefined) {
    document.getElementById('msebToggle').checked = s.mseb_enabled;
  }
  if (s.mseb_params && s.mseb_params.length === 10) {
    s.mseb_params.forEach((val, idx) => {
      const slider = document.querySelector(`.mseb-slider[data-param="${idx}"]`);
      if (slider) slider.value = val;
      const valLbl = document.getElementById(`msebVal${idx}`);
      if (valLbl) valLbl.textContent = (val > 0 ? '+' : '') + val.toFixed(1);
    });
  }

  // 主题与动效模式激活态
  document.querySelectorAll('#themesGrid .theme-card').forEach(c => {
    c.classList.toggle('active', parseInt(c.dataset.theme) === s.theme_id);
  });
  document.querySelectorAll('#visualsGrid .theme-card').forEach(c => {
    c.classList.toggle('active', parseInt(c.dataset.vmode) === s.visual_mode);
  });

  // 淡入淡出
  if (s.fade_duration !== undefined) {
    document.getElementById('fadeSlider').value = s.fade_duration;
  }

  // 源码直通状态
  const bp = document.getElementById('bitPerfectBadge');
  if (s.is_bit_perfect) {
    bp.textContent = '源码直通中';
    bp.className = 'badge';
  } else {
    bp.textContent = '软件混音';
    bp.className = 'badge disconnected';
  }
}

// ==================== 播放队列渲染 ====================
function updateQueue(q) {
  queueData = q.tracks || [];
  const curIdx = q.current_index || 0;
  document.getElementById('qCountBadge').textContent = queueData.length;
  document.getElementById('qTotal').textContent = queueData.length;

  const list = document.getElementById('queueList');
  if (queueData.length === 0) {
    list.innerHTML = '<div style="text-align: center; color: var(--text-muted); padding: 30px 0; font-size: 13px;">队列为空，快去曲库添加歌曲吧</div>';
    return;
  }

  let html = '';
  queueData.forEach((t, i) => {
    const isAct = (i === curIdx);
    html += `
      <div class="song-item ${isAct ? 'active' : ''}" data-idx="${i}">
        <div class="song-left">
          <div class="song-idx">${isAct ? '&#9654;' : (i + 1 < 10 ? '0' + (i + 1) : i + 1)}</div>
          <div class="song-main">
            <div class="song-title">${t.title || '未知歌曲'}</div>
            <div class="song-sub">${t.artist || '未知艺术家'} &middot; ${t.album || '纯音专辑'}</div>
          </div>
        </div>
        <div class="song-actions">
          <div class="song-meta">${formatTime(t.duration)}</div>
          <button class="btn-del" data-del="${i}" title="从队列中移除">&times;</button>
        </div>
      </div>`;
  });
  list.innerHTML = html;

  // 绑定点击起播与删除
  list.querySelectorAll('.song-item').forEach(item => {
    item.addEventListener('click', (e) => {
      if (e.target.closest('.btn-del')) return;
      const idx = parseInt(item.dataset.idx);
      sendAction({ action: 'play_queue_index', index: idx });
    });
  });

  list.querySelectorAll('.btn-del').forEach(btn => {
    btn.addEventListener('click', (e) => {
      e.stopPropagation();
      const idx = parseInt(btn.dataset.del);
      sendAction({ action: 'remove_queue_index', index: idx });
    });
  });
}

// ==================== 发烧歌单渲染 ====================
function updatePlaylists(p) {
  playlistsData = p.playlists || [];
  document.getElementById('plCountBadge').textContent = playlistsData.length;
  document.getElementById('plTotal').textContent = playlistsData.length;

  const grid = document.getElementById('playlistsGrid');
  if (playlistsData.length === 0) {
    grid.innerHTML = '<div style="grid-column: 1/-1; text-align: center; color: var(--text-muted); padding: 30px 0; font-size: 13px;">暂未创建歌单，点击上方“＋ 新建歌单”开始</div>';
    return;
  }

  let html = '';
  playlistsData.forEach(pl => {
    html += `
      <div class="playlist-card" data-plid="${pl.id}">
        <div>
          <div class="playlist-header">
            <span style="font-size: 18px; color: var(--accent);">&#128193;</span>
            <div class="playlist-name">${pl.name}</div>
          </div>
          <div class="playlist-count">${pl.track_count} 首曲目 &middot; ${formatTime(pl.duration)}</div>
        </div>
        <div class="playlist-btns">
          <button class="btn-sm primary btn-play-pl" data-plid="${pl.id}">&#9654; 启播歌单</button>
          <button class="btn-sm btn-del-pl" data-plid="${pl.id}" style="color:var(--danger);">&#128465;</button>
        </div>
      </div>`;
  });
  grid.innerHTML = html;

  grid.querySelectorAll('.btn-play-pl').forEach(btn => {
    btn.addEventListener('click', () => {
      const id = parseInt(btn.dataset.plid);
      sendAction({ action: 'play_playlist', id: id, start_index: 0 });
    });
  });

  grid.querySelectorAll('.btn-del-pl').forEach(btn => {
    btn.addEventListener('click', () => {
      const id = parseInt(btn.dataset.plid);
      if (confirm('确定要删除此歌单吗？')) {
        sendAction({ action: 'delete_playlist', id: id });
      }
    });
  });
}

// ==================== 曲库渲染与搜索 ====================
function updateLibrary(l) {
  libraryData = l.tracks || [];
  document.getElementById('libTotal').textContent = libraryData.length;
  renderLibraryList(libraryData);
}

function renderLibraryList(tracks) {
  const list = document.getElementById('libSongList');
  if (tracks.length === 0) {
    list.innerHTML = '<div style="text-align: center; color: var(--text-muted); padding: 30px 0; font-size: 13px;">未找到匹配曲目</div>';
    return;
  }

  let html = '';
  tracks.forEach((t, i) => {
    html += `
      <div class="song-item" data-libidx="${i}" data-id="${t.id}">
        <div class="song-left">
          <div class="song-idx">${i + 1 < 10 ? '0' + (i + 1) : i + 1}</div>
          <div class="song-main">
            <div class="song-title">${t.title || '未知曲目'}</div>
            <div class="song-sub">${t.artist || '未知艺术家'} &middot; ${t.badge || 'FLAC'}</div>
          </div>
        </div>
        <div class="song-actions">
          <div class="song-meta">${formatTime(t.duration)}</div>
          <button class="btn-sm btn-add-q" data-id="${t.id}" title="加入播放队列">＋ 队列</button>
        </div>
      </div>`;
  });
  list.innerHTML = html;

  list.querySelectorAll('.song-item').forEach(item => {
    item.addEventListener('click', (e) => {
      if (e.target.closest('.btn-add-q')) return;
      const idx = parseInt(item.dataset.libidx);
      sendAction({ action: 'play_library_index', index: idx });
    });
  });

  list.querySelectorAll('.btn-add-q').forEach(btn => {
    btn.addEventListener('click', (e) => {
      e.stopPropagation();
      const tid = parseInt(btn.dataset.id);
      sendAction({ action: 'add_to_queue', id: tid });
      btn.textContent = '已添加';
      btn.style.borderColor = 'var(--accent)';
      setTimeout(() => { btn.textContent = '＋ 队列'; }, 1000);
    });
  });
}

// ==================== 10段图形 EQ 渲染 ====================
function renderEqSliders() {
  let html = '';
  for (let i = 0; i < 10; ++i) {
    html += `
      <div class="eq-col">
        <div class="eq-gain-lbl" id="eqGain${i}">0.0</div>
        <div class="eq-vslider-wrap">
          <input type="range" class="eq-vslider" min="-12" max="12" step="0.5" value="0" data-band="${i}">
        </div>
        <div class="eq-freq-lbl">${eqFreqs[i]}</div>
      </div>`;
  }
  document.getElementById('eqSliders').innerHTML = html;

  document.querySelectorAll('.eq-vslider').forEach(slider => {
    slider.addEventListener('input', (e) => {
      const idx = parseInt(e.target.dataset.band);
      const val = parseFloat(e.target.value);
      const lbl = document.getElementById(`eqGain${idx}`);
      if (lbl) lbl.textContent = (val > 0 ? '+' : '') + val.toFixed(1);
      sendAction({ action: 'set_eq_band', index: idx, value: val });
    });
  });
}

// ==================== 调音魔棒 (MSEB) 渲染 ====================
function renderMseb() {
  let html = '';
  msebDefs.forEach((def, i) => {
    html += `
      <div class="mseb-card">
        <div class="mseb-top">
          <span class="mseb-name">${def.name}</span>
          <span class="mseb-val" id="msebVal${i}">0.0</span>
        </div>
        <div class="mseb-slider-row">
          <span class="mseb-lbl-l">${def.left}</span>
          <input type="range" class="mseb-slider" min="-10" max="10" step="0.5" value="0" data-param="${i}">
          <span class="mseb-lbl-r">${def.right}</span>
        </div>
      </div>`;
  });
  document.getElementById('msebGrid').innerHTML = html;

  document.querySelectorAll('.mseb-slider').forEach(slider => {
    slider.addEventListener('input', (e) => {
      const idx = parseInt(e.target.dataset.param);
      const val = parseFloat(e.target.value);
      const lbl = document.getElementById(`msebVal${idx}`);
      if (lbl) lbl.textContent = (val > 0 ? '+' : '') + val.toFixed(1);
      sendAction({ action: 'set_mseb_param', index: idx, value: val });
    });
  });
}

function updateMseb(m) {
  document.getElementById('msebToggle').checked = m.enabled;
  if (m.params && m.params.length === 10) {
    m.params.forEach((val, idx) => {
      const slider = document.querySelector(`.mseb-slider[data-param="${idx}"]`);
      if (slider) slider.value = val;
      const lbl = document.getElementById(`msebVal${idx}`);
      if (lbl) lbl.textContent = (val > 0 ? '+' : '') + val.toFixed(1);
    });
  }
}

// ==================== DAC 硬件与参数配置渲染 ====================
function updateDac(d) {
  dacConfigData = d;
  activeDacChip = d.selected_chip || 0;

  document.getElementById('dacDeviceName').textContent = d.dac_name || '系统默认声卡';
  document.getElementById('dacDeviceDesc').textContent = '活跃输出: ' + (d.active_output || '默认音频输出');
  document.getElementById('dacStatusIcon').style.color = d.has_external_dac ? 'var(--accent)' : 'var(--text-muted)';

  // 芯片卡片高亮
  document.querySelectorAll('#dacChipsGrid .dac-chip-card').forEach(c => {
    c.classList.toggle('active', parseInt(c.dataset.chip) === activeDacChip);
  });

  renderDacOptions(activeDacChip, d.params || {});
}

function renderDacOptions(chipIdx, params) {
  const container = document.getElementById('dacOptionsContainer');
  const rows = dacChipOptions[chipIdx] || [];
  let html = '';

  rows.forEach(r => {
    const curVal = params[r.key] !== undefined ? params[r.key] : 0;
    html += `
      <div class="dac-opt-row">
        <div class="dac-opt-lbl">${r.label}</div>
        <div class="dac-opt-btns">`;
    r.opts.forEach((optName, oIdx) => {
      const isAct = (oIdx === curVal);
      html += `<button class="dac-opt-btn ${isAct ? 'active' : ''}" data-key="${r.key}" data-val="${oIdx}">${optName}</button>`;
    });
    html += `</div></div>`;
  });

  container.innerHTML = html;

  container.querySelectorAll('.dac-opt-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      const key = btn.dataset.key;
      const val = parseInt(btn.dataset.val);
      btn.parentElement.querySelectorAll('.dac-opt-btn').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      sendAction({ action: 'set_dac_param', key: key, val: val });
    });
  });
}

// ==================== 事件监听绑定 ====================
document.addEventListener('DOMContentLoaded', () => {
  renderEqSliders();
  renderMseb();
  initWebSocket();

  // 6 大顶层 Tab 切换
  document.querySelectorAll('.nav-tab').forEach(tab => {
    tab.addEventListener('click', () => {
      document.querySelectorAll('.nav-tab').forEach(t => t.classList.remove('active'));
      document.querySelectorAll('.tab-pane').forEach(p => p.classList.remove('active'));
      tab.classList.add('active');
      const targetPane = document.getElementById(tab.dataset.tab);
      if (targetPane) targetPane.classList.add('active');
    });
  });

  // 列表 Sub-Pill 切换
  document.getElementById('pillQueue').addEventListener('click', () => {
    document.getElementById('pillQueue').classList.add('active');
    document.getElementById('pillPlaylists').classList.remove('active');
    document.getElementById('paneQueueList').style.display = 'block';
    document.getElementById('panePlaylistsList').style.display = 'none';
  });
  document.getElementById('pillPlaylists').addEventListener('click', () => {
    document.getElementById('pillPlaylists').classList.add('active');
    document.getElementById('pillQueue').classList.remove('active');
    document.getElementById('panePlaylistsList').style.display = 'block';
    document.getElementById('paneQueueList').style.display = 'none';
  });

  // 调音 Sub-Pill 切换
  document.getElementById('pillEq').addEventListener('click', () => {
    document.getElementById('pillEq').classList.add('active');
    document.getElementById('pillMseb').classList.remove('active');
    document.getElementById('paneEq').style.display = 'block';
    document.getElementById('paneMseb').style.display = 'none';
  });
  document.getElementById('pillMseb').addEventListener('click', () => {
    document.getElementById('pillMseb').classList.add('active');
    document.getElementById('pillEq').classList.remove('active');
    document.getElementById('paneMseb').style.display = 'block';
    document.getElementById('paneEq').style.display = 'none';
  });

  // 播放控制
  document.getElementById('btnPlay').addEventListener('click', () => sendAction({ action: 'toggle' }));
  document.getElementById('btnPrev').addEventListener('click', () => sendAction({ action: 'prev' }));
  document.getElementById('btnNext').addEventListener('click', () => sendAction({ action: 'next' }));
  document.getElementById('btnMode').addEventListener('click', () => sendAction({ action: 'mode' }));
  document.getElementById('btnMute').addEventListener('click', () => sendAction({ action: 'mute' }));

  // 音量控制
  const vol = document.getElementById('volSlider');
  vol.addEventListener('input', (e) => {
    const v = parseInt(e.target.value);
    document.getElementById('volLabel').textContent = v + '%';
    sendAction({ action: 'volume', value: v / 100.0 });
  });

  // 进度条拖拽
  const pBar = document.getElementById('progressBar');
  pBar.addEventListener('click', (e) => {
    const rect = pBar.getBoundingClientRect();
    const ratio = Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width));
    if (currentDuration > 0) {
      sendAction({ action: 'seek', value: ratio * currentDuration });
    }
  });

  // 均衡器开关与归零
  document.getElementById('eqToggle').addEventListener('change', (e) => {
    sendAction({ action: 'set_eq_enabled', enabled: e.target.checked });
  });
  document.getElementById('btnResetEq').addEventListener('click', () => {
    sendAction({ action: 'reset_eq' });
  });

  // 均衡器预设点击
  document.querySelectorAll('#eqPresetsList .preset-chip').forEach(chip => {
    chip.addEventListener('click', () => {
      document.querySelectorAll('#eqPresetsList .preset-chip').forEach(c => c.classList.remove('active'));
      chip.classList.add('active');
      const presetKey = chip.dataset.preset;
      if (eqPresets[presetKey]) {
        sendAction({ action: 'set_eq_preset', bands: eqPresets[presetKey] });
      }
    });
  });

  // 魔棒开关与归零
  document.getElementById('msebToggle').addEventListener('change', (e) => {
    sendAction({ action: 'set_mseb_enabled', enabled: e.target.checked });
  });
  document.getElementById('btnResetMseb').addEventListener('click', () => {
    sendAction({ action: 'reset_mseb' });
  });

  // DAC 芯片切换点击
  document.querySelectorAll('#dacChipsGrid .dac-chip-card').forEach(card => {
    card.addEventListener('click', () => {
      const chip = parseInt(card.dataset.chip);
      activeDacChip = chip;
      document.querySelectorAll('#dacChipsGrid .dac-chip-card').forEach(c => c.classList.remove('active'));
      card.classList.add('active');
      sendAction({ action: 'set_dac_chip', chip: chip });
      renderDacOptions(chip, dacConfigData ? (dacConfigData.params || {}) : {});
    });
  });

  // 清空队列
  document.getElementById('btnClearQueue').addEventListener('click', () => {
    if (confirm('确定要清空正在播放队列吗？')) {
      sendAction({ action: 'clear_queue' });
    }
  });

  // 新建歌单弹窗
  const modal = document.getElementById('createPlModal');
  document.getElementById('btnOpenCreatePlModal').addEventListener('click', () => {
    document.getElementById('newPlNameInput').value = '';
    modal.classList.add('active');
  });
  document.getElementById('btnCancelCreatePl').addEventListener('click', () => {
    modal.classList.remove('active');
  });
  document.getElementById('btnConfirmCreatePl').addEventListener('click', () => {
    const name = document.getElementById('newPlNameInput').value.trim();
    if (name) {
      sendAction({ action: 'create_playlist', name: name });
      modal.classList.remove('active');
    }
  });

  // 曲库搜索
  document.getElementById('libSearchInput').addEventListener('input', (e) => {
    const kw = e.target.value.toLowerCase().trim();
    if (!kw) {
      renderLibraryList(libraryData);
      return;
    }
    const filtered = libraryData.filter(t => 
      (t.title && t.title.toLowerCase().includes(kw)) ||
      (t.artist && t.artist.toLowerCase().includes(kw)) ||
      (t.album && t.album.toLowerCase().includes(kw))
    );
    renderLibraryList(filtered);
  });

  // 扫描曲库
  document.getElementById('btnScanMusic').addEventListener('click', () => {
    sendAction({ action: 'start_scan' });
    alert('已向数播发送本地全盘扫描指令');
  });

  // 主题配色点击
  document.querySelectorAll('#themesGrid .theme-card').forEach(card => {
    card.addEventListener('click', () => {
      const tid = parseInt(card.dataset.theme);
      sendAction({ action: 'set_theme', theme_id: tid });
    });
  });

  // 视觉动效点击
  document.querySelectorAll('#visualsGrid .theme-card').forEach(card => {
    card.addEventListener('click', () => {
      const mode = parseInt(card.dataset.vmode);
      sendAction({ action: 'set_visual_mode', mode: mode });
    });
  });

  // 淡入淡出
  document.getElementById('fadeSlider').addEventListener('change', (e) => {
    sendAction({ action: 'set_fade', value: parseFloat(e.target.value) });
  });

  // 睡眠定时
  document.querySelectorAll('.timer-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('.timer-btn').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      const min = parseInt(btn.dataset.min);
      sendAction({ action: 'set_sleep_timer', minutes: min });
      document.getElementById('sleepTimerStatus').textContent = min > 0 ? `将在 ${min} 分钟后自动关机` : '未开启睡眠定时';
    });
  });

  // WiFi 网页传歌跳转链接
  document.getElementById('btnWifiUploadLink').href = window.location.protocol + '//' + window.location.hostname + ':8080';
});
</script>
</body>
</html>
)rawhtml";

} // namespace

// ==============================================================================
// 3. WebService 服务核心类实现
// ==============================================================================
WebService& WebService::getInstance() {
    static WebService instance;
    return instance;
}

WebService::WebService() {
#if defined(_WIN32)
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

WebService::~WebService() {
    stop();
#if defined(_WIN32)
    WSACleanup();
#endif
}

bool WebService::start(int port) {
    if (is_running_.load()) {
        if (port_ == port) return true;
        stop();
    }

    port_ = port;

    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "[WebService] Failed to create socket." << std::endl;
        return false;
    }

    int opt = 1;
#if defined(_WIN32)
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(port_));

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[WebService] Failed to bind port " << port_ << std::endl;
#if defined(_WIN32)
        closesocket(server_fd_);
#else
        close(server_fd_);
#endif
        server_fd_ = -1;
        return false;
    }

    if (listen(server_fd_, 16) < 0) {
        std::cerr << "[WebService] Failed to listen." << std::endl;
#if defined(_WIN32)
        closesocket(server_fd_);
#else
        close(server_fd_);
#endif
        server_fd_ = -1;
        return false;
    }

    is_running_.store(true);
    server_thread_ = std::thread(&WebService::serverLoop, this);
    broadcast_thread_ = std::thread(&WebService::broadcastLoop, this);

    std::cout << "[WebService] 纯音 Web 远程控制中枢已启动，监听端口: " << port_ << std::endl;
    return true;
}

void WebService::stop() {
    if (!is_running_.load()) return;

    is_running_.store(false);

    if (server_fd_ >= 0) {
#if defined(_WIN32)
        closesocket(server_fd_);
#else
        shutdown(server_fd_, SHUT_RDWR);
        close(server_fd_);
#endif
        server_fd_ = -1;
    }

    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (int fd : ws_clients_) {
#if defined(_WIN32)
            closesocket(fd);
#else
            shutdown(fd, SHUT_RDWR);
            close(fd);
#endif
        }
        ws_clients_.clear();
    }

    if (server_thread_.joinable()) server_thread_.join();
    if (broadcast_thread_.joinable()) broadcast_thread_.join();

    std::cout << "[WebService] 纯音 Web 远程控制中枢已安全停运，端口与套接字资源已彻底释放。" << std::endl;
}

int WebService::getConnectedClientsCount() {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    return static_cast<int>(ws_clients_.size());
}

WebServiceStats WebService::getStats() {
    WebServiceStats stats;
    stats.is_running = is_running_.load();
    stats.port = port_;
    stats.active_ws_clients = getConnectedClientsCount();

    auto& pa = PlayerAdmin::getInstance();
    stats.is_playing = pa.isPlaying();
    stats.volume = pa.getVolume();
    auto cur_opt = pa.getCurrentTrack();
    if (cur_opt.has_value()) {
        stats.current_track_title = cur_opt->title;
        stats.current_artist = cur_opt->artist;
    } else {
        stats.current_track_title = "暂未载入曲目";
        stats.current_artist = "未知";
    }

    stats.messages_sent = messages_sent_.load();
    stats.messages_received = messages_received_.load();
    return stats;
}

void WebService::serverLoop() {
    while (is_running_.load()) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);

        if (client_fd < 0) {
            if (!is_running_.load()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        std::thread([this, client_fd]() {
            this->handleClient(client_fd);
        }).detach();
    }
}

void WebService::handleClient(int client_fd) {
    // 设置 5 秒读超时
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    std::vector<char> buffer(4096);
    ssize_t bytes_read = recv(client_fd, buffer.data(), buffer.size() - 1, 0);

    if (bytes_read <= 0) {
#if defined(_WIN32)
        closesocket(client_fd);
#else
        close(client_fd);
#endif
        return;
    }

    buffer[bytes_read] = '\0';
    std::string req(buffer.data());

    std::istringstream req_stream(req);
    std::string method, uri, http_version;
    std::string request_line;
    if (std::getline(req_stream, request_line)) {
        if (!request_line.empty() && request_line.back() == '\r') request_line.pop_back();
        std::istringstream line_stream(request_line);
        line_stream >> method >> uri >> http_version;
    }

    // 检查 WebSocket 升级握手
    bool is_websocket = false;
    std::string ws_key;
    std::string line;
    while (std::getline(req_stream, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty()) break;
        auto colon = line.find(':');
        if (colon != std::string::npos) {
            std::string header_name = line.substr(0, colon);
            std::string header_val = line.substr(colon + 1);
            while (!header_val.empty() && header_val.front() == ' ') header_val.erase(0, 1);
            while (!header_val.empty() && header_val.back() == ' ') header_val.pop_back();
            std::transform(header_name.begin(), header_name.end(), header_name.begin(), ::tolower);
            std::string val_lower = header_val;
            std::transform(val_lower.begin(), val_lower.end(), val_lower.begin(), ::tolower);

            if (header_name == "upgrade" && val_lower.find("websocket") != std::string::npos) {
                is_websocket = true;
            }
            if (header_name == "sec-websocket-key") {
                ws_key = header_val;
            }
        }
    }

    // 绝对可靠兜底：直接从原始报文中检索 Upgrade 与 Sec-WebSocket-Key
    if (!is_websocket || ws_key.empty()) {
        std::string req_lower = req;
        std::transform(req_lower.begin(), req_lower.end(), req_lower.begin(), ::tolower);
        if (req_lower.find("upgrade: websocket") != std::string::npos ||
            req_lower.find("upgrade:websocket") != std::string::npos) {
            is_websocket = true;
        }
        size_t kpos = req_lower.find("sec-websocket-key:");
        if (kpos != std::string::npos) {
            size_t val_start = kpos + 18;
            while (val_start < req.size() && req[val_start] == ' ') val_start++;
            size_t val_end = req.find("\r", val_start);
            if (val_end == std::string::npos) val_end = req.find("\n", val_start);
            if (val_end != std::string::npos) {
                ws_key = req.substr(val_start, val_end - val_start);
                while (!ws_key.empty() && (ws_key.back() == ' ' || ws_key.back() == '\r')) ws_key.pop_back();
            }
        }
    }

    if (is_websocket && !ws_key.empty()) {
        handleWebSocket(client_fd, ws_key);
        return;
    }

    // 1. GET /: 返回内嵌 H5 SPA 控制中心
    if (method == "GET" && (uri == "/" || uri == "/index.html")) {
        std::string html = INDEX_HTML;
        std::ostringstream resp;
        resp << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: text/html; charset=utf-8\r\n"
             << "Content-Length: " << html.size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << html;
        std::string resp_str = resp.str();
        send(client_fd, resp_str.data(), resp_str.size(), 0);
#if defined(_WIN32)
        closesocket(client_fd);
#else
        close(client_fd);
#endif
        return;
    }

    // 2. GET /api/status: 获取 JSON 播放状态
    if (method == "GET" && uri == "/api/status") {
        std::string body = buildStateJson();
        std::ostringstream resp;
        resp << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: application/json; charset=utf-8\r\n"
             << "Access-Control-Allow-Origin: *\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << body;
        std::string resp_str = resp.str();
        send(client_fd, resp_str.data(), resp_str.size(), 0);
#if defined(_WIN32)
        closesocket(client_fd);
#else
        close(client_fd);
#endif
        return;
    }

    // 3. GET /api/queue: 获取当前播放队列
    if (method == "GET" && uri == "/api/queue") {
        std::string body = buildQueueJson();
        std::ostringstream resp;
        resp << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: application/json; charset=utf-8\r\n"
             << "Access-Control-Allow-Origin: *\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << body;
        std::string resp_str = resp.str();
        send(client_fd, resp_str.data(), resp_str.size(), 0);
#if defined(_WIN32)
        closesocket(client_fd);
#else
        close(client_fd);
#endif
        return;
    }

    // 4. GET /api/playlists: 获取歌单列表
    if (method == "GET" && uri == "/api/playlists") {
        std::string body = buildPlaylistsJson();
        std::ostringstream resp;
        resp << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: application/json; charset=utf-8\r\n"
             << "Access-Control-Allow-Origin: *\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << body;
        std::string resp_str = resp.str();
        send(client_fd, resp_str.data(), resp_str.size(), 0);
#if defined(_WIN32)
        closesocket(client_fd);
#else
        close(client_fd);
#endif
        return;
    }

    // 5. GET /api/library: 获取全部曲库
    if (method == "GET" && uri == "/api/library") {
        std::string body = buildLibraryJson();
        std::ostringstream resp;
        resp << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: application/json; charset=utf-8\r\n"
             << "Access-Control-Allow-Origin: *\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << body;
        std::string resp_str = resp.str();
        send(client_fd, resp_str.data(), resp_str.size(), 0);
#if defined(_WIN32)
        closesocket(client_fd);
#else
        close(client_fd);
#endif
        return;
    }

    // 兜底 404
    std::string not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    send(client_fd, not_found.data(), not_found.size(), 0);
#if defined(_WIN32)
    closesocket(client_fd);
#else
    close(client_fd);
#endif
}

void WebService::handleWebSocket(int client_fd, const std::string& key) {
    std::string accept_key = crypto::compute_ws_accept(key);
    std::ostringstream resp;
    resp << "HTTP/1.1 101 Switching Protocols\r\n"
         << "Upgrade: websocket\r\n"
         << "Connection: Upgrade\r\n"
         << "Sec-WebSocket-Accept: " << accept_key << "\r\n\r\n";
    std::string resp_str = resp.str();
    send(client_fd, resp_str.data(), resp_str.size(), 0);

    // 取消超时，进入长连接保活
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#if defined(SO_NOSIGPIPE)
    int opt_nosigpipe = 1;
    setsockopt(client_fd, SOL_SOCKET, SO_NOSIGPIPE, (void*)&opt_nosigpipe, sizeof(opt_nosigpipe));
#endif

    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        ws_clients_.push_back(client_fd);
    }

    // 立即向新连接下发最新状态、队列、歌单、DAC 与曲库
    sendWsTextFrame(client_fd, buildStateJson());
    sendWsTextFrame(client_fd, buildQueueJson());
    sendWsTextFrame(client_fd, buildPlaylistsJson());
    sendWsTextFrame(client_fd, buildLibraryJson());
    sendWsTextFrame(client_fd, buildDacJson());
    sendWsTextFrame(client_fd, buildMsebJson());

    // 读取客户端下发的 WebSocket 数据帧
    while (is_running_.load()) {
        uint8_t hdr[2];
        ssize_t n = recv(client_fd, (char*)hdr, 2, 0);
        if (n <= 0) break;

        uint8_t opcode = hdr[0] & 0x0F;
        bool masked = (hdr[1] & 0x80) != 0;
        uint64_t payload_len = hdr[1] & 0x7F;

        if (opcode == 0x08) {
            break; // Close 帧
        }

        if (payload_len == 126) {
            uint8_t ext[2];
            if (recv(client_fd, (char*)ext, 2, 0) <= 0) break;
            payload_len = (ext[0] << 8) | ext[1];
        } else if (payload_len == 127) {
            uint8_t ext[8];
            if (recv(client_fd, (char*)ext, 8, 0) <= 0) break;
            payload_len = 0;
            for (int i = 0; i < 8; ++i) {
                payload_len = (payload_len << 8) | ext[i];
            }
        }

        uint8_t mask_key[4] = {0};
        if (masked) {
            if (recv(client_fd, (char*)mask_key, 4, 0) <= 0) break;
        }

        std::vector<char> payload(payload_len);
        size_t total_recv = 0;
        while (total_recv < payload_len) {
            ssize_t r = recv(client_fd, payload.data() + total_recv, payload_len - total_recv, 0);
            if (r <= 0) break;
            total_recv += r;
        }
        if (total_recv < payload_len) break;

        if (masked) {
            for (size_t i = 0; i < payload_len; ++i) {
                payload[i] ^= mask_key[i % 4];
            }
        }

        if (opcode == 0x09) {
            // Ping 响应 Pong (0x0A)
            std::vector<uint8_t> pong;
            pong.push_back(0x8A);
            pong.push_back(0x00);
            send(client_fd, (char*)pong.data(), pong.size(), 0);
            continue;
        }

        if (opcode == 0x01) {
            messages_received_++;
            std::string cmd(payload.data(), payload.size());

            // -------------------------------------------------------------
            // 全功能指令分发
            // -------------------------------------------------------------
            // [1] 基础播放控制
            if (cmd.find("\"action\":\"play\"") != std::string::npos || cmd.find("\"action\": \"play\"") != std::string::npos) {
                PlayerAdmin::getInstance().play();
            } else if (cmd.find("\"action\":\"pause\"") != std::string::npos || cmd.find("\"action\": \"pause\"") != std::string::npos) {
                PlayerAdmin::getInstance().pause();
            } else if (cmd.find("\"action\":\"toggle\"") != std::string::npos || cmd.find("\"action\": \"toggle\"") != std::string::npos) {
                PlayerAdmin::getInstance().togglePlayPause();
            } else if (cmd.find("\"action\":\"next\"") != std::string::npos || cmd.find("\"action\": \"next\"") != std::string::npos) {
                PlayerAdmin::getInstance().next();
            } else if (cmd.find("\"action\":\"prev\"") != std::string::npos || cmd.find("\"action\": \"prev\"") != std::string::npos) {
                PlayerAdmin::getInstance().previous();
            } else if (cmd.find("\"action\":\"mode\"") != std::string::npos || cmd.find("\"action\": \"mode\"") != std::string::npos) {
                PlayerAdmin::getInstance().cyclePlayMode();
            } else if (cmd.find("\"action\":\"mute\"") != std::string::npos || cmd.find("\"action\": \"mute\"") != std::string::npos) {
                PlayerAdmin::getInstance().toggleMute();
            } else if (cmd.find("\"action\":\"seek\"") != std::string::npos || cmd.find("\"action\": \"seek\"") != std::string::npos) {
                size_t vpos = cmd.find("\"value\":");
                if (vpos == std::string::npos) vpos = cmd.find("\"value\" :");
                if (vpos != std::string::npos) {
                    double target_sec = std::strtod(cmd.c_str() + vpos + 8, nullptr);
                    PlayerAdmin::getInstance().seek(target_sec);
                }
            } else if (cmd.find("\"action\":\"volume\"") != std::string::npos || cmd.find("\"action\": \"volume\"") != std::string::npos) {
                size_t vpos = cmd.find("\"value\":");
                if (vpos == std::string::npos) vpos = cmd.find("\"value\" :");
                if (vpos != std::string::npos) {
                    float target_vol = std::strtof(cmd.c_str() + vpos + 8, nullptr);
                    PlayerAdmin::getInstance().setVolume(target_vol);
                }
            }
            // [2] 正在播放队列操作
            else if (cmd.find("\"action\":\"play_queue_index\"") != std::string::npos) {
                size_t ipos = cmd.find("\"index\":");
                if (ipos != std::string::npos) {
                    size_t track_idx = static_cast<size_t>(std::strtoul(cmd.c_str() + ipos + 8, nullptr, 10));
                    PlayerAdmin::getInstance().playQueueIndex(track_idx);
                }
            } else if (cmd.find("\"action\":\"remove_queue_index\"") != std::string::npos) {
                size_t ipos = cmd.find("\"index\":");
                if (ipos != std::string::npos) {
                    size_t track_idx = static_cast<size_t>(std::strtoul(cmd.c_str() + ipos + 8, nullptr, 10));
                    PlayerAdmin::getInstance().removeTrackFromQueue(track_idx);
                    broadcastWsText(buildQueueJson());
                }
            } else if (cmd.find("\"action\":\"clear_queue\"") != std::string::npos) {
                PlayerAdmin::getInstance().clearQueue();
                broadcastWsText(buildQueueJson());
            } else if (cmd.find("\"action\":\"add_to_queue\"") != std::string::npos) {
                size_t idpos = cmd.find("\"id\":");
                if (idpos != std::string::npos) {
                    uint64_t tid = std::strtoull(cmd.c_str() + idpos + 5, nullptr, 10);
                    auto all = MusicDatabase::getInstance().loadScannedTracks();
                    auto it = std::find_if(all.begin(), all.end(), [tid](const Track& t){ return t.id == tid; });
                    if (it != all.end()) {
                        PlayerAdmin::getInstance().addToQueue(*it);
                        broadcastWsText(buildQueueJson());
                    }
                }
            }
            // [3] 自定义发烧歌单操作
            else if (cmd.find("\"action\":\"play_playlist\"") != std::string::npos) {
                size_t idpos = cmd.find("\"id\":");
                size_t spos = cmd.find("\"start_index\":");
                if (idpos != std::string::npos) {
                    uint64_t pid = std::strtoull(cmd.c_str() + idpos + 5, nullptr, 10);
                    size_t start_idx = 0;
                    if (spos != std::string::npos) {
                        start_idx = static_cast<size_t>(std::strtoul(cmd.c_str() + spos + 14, nullptr, 10));
                    }
                    auto pls = MusicDatabase::getInstance().loadPlaylists();
                    auto it = std::find_if(pls.begin(), pls.end(), [pid](const Playlist& p){ return p.getId() == pid; });
                    if (it != pls.end()) {
                        PlayerAdmin::getInstance().playPlaylist(*it, start_idx);
                        broadcastWsText(buildQueueJson());
                    }
                }
            } else if (cmd.find("\"action\":\"create_playlist\"") != std::string::npos) {
                size_t npos = cmd.find("\"name\":\"");
                if (npos != std::string::npos) {
                    std::string pl_name = cmd.substr(npos + 8);
                    size_t nend = pl_name.find('"');
                    if (nend != std::string::npos) pl_name = pl_name.substr(0, nend);
                    if (!pl_name.empty()) {
                        auto pls = MusicDatabase::getInstance().loadPlaylists();
                        uint64_t new_id = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count());
                        pls.emplace_back(new_id, pl_name);
                        MusicDatabase::getInstance().savePlaylists(pls);
                        broadcastWsText(buildPlaylistsJson());
                    }
                }
            } else if (cmd.find("\"action\":\"delete_playlist\"") != std::string::npos) {
                size_t idpos = cmd.find("\"id\":");
                if (idpos != std::string::npos) {
                    uint64_t pid = std::strtoull(cmd.c_str() + idpos + 5, nullptr, 10);
                    auto pls = MusicDatabase::getInstance().loadPlaylists();
                    auto it = std::remove_if(pls.begin(), pls.end(), [pid](const Playlist& p){ return p.getId() == pid; });
                    if (it != pls.end()) {
                        pls.erase(it, pls.end());
                        MusicDatabase::getInstance().savePlaylists(pls);
                        broadcastWsText(buildPlaylistsJson());
                    }
                }
            }
            // [4] 曲库浏览与扫描
            else if (cmd.find("\"action\":\"play_library_index\"") != std::string::npos) {
                size_t ipos = cmd.find("\"index\":");
                if (ipos != std::string::npos) {
                    size_t track_idx = static_cast<size_t>(std::strtoul(cmd.c_str() + ipos + 8, nullptr, 10));
                    auto all = MusicDatabase::getInstance().loadScannedTracks();
                    if (track_idx < all.size()) {
                        PlayerAdmin::getInstance().playTracks(all, track_idx);
                        broadcastWsText(buildQueueJson());
                    }
                }
            } else if (cmd.find("\"action\":\"start_scan\"") != std::string::npos) {
                MusicScanManager::getInstance().startScan();
            } else if (cmd.find("\"action\":\"get_library\"") != std::string::npos) {
                sendWsTextFrame(client_fd, buildLibraryJson());
            } else if (cmd.find("\"action\":\"get_queue\"") != std::string::npos) {
                sendWsTextFrame(client_fd, buildQueueJson());
            } else if (cmd.find("\"action\":\"get_playlists\"") != std::string::npos) {
                sendWsTextFrame(client_fd, buildPlaylistsJson());
            }
            // [5] 10段图形 EQ 调音
            else if (cmd.find("\"action\":\"set_eq_enabled\"") != std::string::npos) {
                bool en = (cmd.find("\"enabled\":true") != std::string::npos || cmd.find("\"enabled\": true") != std::string::npos);
                PlayerAdmin::getInstance().setEqEnabled(en);
            } else if (cmd.find("\"action\":\"set_eq_band\"") != std::string::npos) {
                size_t ipos = cmd.find("\"index\":");
                size_t vpos = cmd.find("\"value\":");
                if (ipos != std::string::npos && vpos != std::string::npos) {
                    size_t idx = static_cast<size_t>(std::strtoul(cmd.c_str() + ipos + 8, nullptr, 10));
                    float val = std::strtof(cmd.c_str() + vpos + 8, nullptr);
                    if (idx < 10) {
                        auto bands = PlayerAdmin::getInstance().getEqBands();
                        bands[idx] = std::clamp(val, -12.0f, 12.0f);
                        PlayerAdmin::getInstance().setEqBands(bands);
                    }
                }
            } else if (cmd.find("\"action\":\"set_eq_preset\"") != std::string::npos) {
                size_t bpos = cmd.find("\"bands\":[");
                if (bpos != std::string::npos) {
                    std::string bstr = cmd.substr(bpos + 9);
                    size_t bend = bstr.find(']');
                    if (bend != std::string::npos) {
                        bstr = bstr.substr(0, bend);
                        std::istringstream bss(bstr);
                        std::string token;
                        std::array<float, 10> bands{};
                        size_t bidx = 0;
                        while (std::getline(bss, token, ',') && bidx < 10) {
                            bands[bidx++] = std::strtof(token.c_str(), nullptr);
                        }
                        PlayerAdmin::getInstance().setEqBands(bands);
                    }
                }
            } else if (cmd.find("\"action\":\"reset_eq\"") != std::string::npos) {
                std::array<float, 10> flat{};
                PlayerAdmin::getInstance().setEqBands(flat);
            }
            // [6] 调音魔棒 (MSEB 心理声学听感微调)
            else if (cmd.find("\"action\":\"set_mseb_enabled\"") != std::string::npos) {
                bool en = (cmd.find("\"enabled\":true") != std::string::npos || cmd.find("\"enabled\": true") != std::string::npos);
                if (auto* m = MagicTuningView::getInstance()) {
                    m->setEnabled(en);
                }
                broadcastWsText(buildMsebJson());
            } else if (cmd.find("\"action\":\"set_mseb_param\"") != std::string::npos) {
                size_t ipos = cmd.find("\"index\":");
                size_t vpos = cmd.find("\"value\":");
                if (ipos != std::string::npos && vpos != std::string::npos) {
                    size_t idx = static_cast<size_t>(std::strtoul(cmd.c_str() + ipos + 8, nullptr, 10));
                    float val = std::strtof(cmd.c_str() + vpos + 8, nullptr);
                    if (auto* m = MagicTuningView::getInstance()) {
                        m->setParamValue(idx, val);
                    }
                    broadcastWsText(buildMsebJson());
                }
            } else if (cmd.find("\"action\":\"reset_mseb\"") != std::string::npos) {
                if (auto* m = MagicTuningView::getInstance()) {
                    m->resetAll();
                }
                broadcastWsText(buildMsebJson());
            }
            // [7] DAC 硬件与解码模式配置
            else if (cmd.find("\"action\":\"set_dac_chip\"") != std::string::npos) {
                size_t cpos = cmd.find("\"chip\":");
                if (cpos != std::string::npos) {
                    int chip = std::atoi(cmd.c_str() + cpos + 7);
                    if (auto* d = DACSettingView::getInstance()) {
                        d->setSelectedChip(chip);
                    } else {
                        MusicDatabase::getInstance().setSetting("setting_dac_selected_chip", std::to_string(chip));
                    }
                    broadcastWsText(buildDacJson());
                }
            } else if (cmd.find("\"action\":\"set_dac_param\"") != std::string::npos) {
                size_t kpos = cmd.find("\"key\":\"");
                size_t vpos = cmd.find("\"val\":");
                if (kpos != std::string::npos && vpos != std::string::npos) {
                    std::string key = cmd.substr(kpos + 7);
                    size_t kend = key.find('"');
                    if (kend != std::string::npos) key = key.substr(0, kend);
                    int val = std::atoi(cmd.c_str() + vpos + 6);
                    if (auto* d = DACSettingView::getInstance()) {
                        d->setParam(key, val);
                    } else {
                        MusicDatabase::getInstance().setSetting("setting_dac_" + key, std::to_string(val));
                    }
                    broadcastWsText(buildDacJson());
                }
            }
            // [8] 主题配色与大屏全景视觉动效
            else if (cmd.find("\"action\":\"set_theme\"") != std::string::npos) {
                size_t tpos = cmd.find("\"theme_id\":");
                if (tpos != std::string::npos) {
                    int tid = std::atoi(cmd.c_str() + tpos + 11);
                    ThemeManager::getInstance().setTheme(static_cast<ThemeId>(tid), true);
                }
            } else if (cmd.find("\"action\":\"set_visual_mode\"") != std::string::npos) {
                size_t mpos = cmd.find("\"mode\":");
                if (mpos != std::string::npos) {
                    int mode_idx = std::atoi(cmd.c_str() + mpos + 7);
                    ThemeManager::getInstance().setBackgroundVisualMode(static_cast<BackgroundVisualMode>(mode_idx), true);
                }
            }
            // [9] 发烧切歌平滑淡入淡出与定时休眠
            else if (cmd.find("\"action\":\"set_fade\"") != std::string::npos) {
                size_t vpos = cmd.find("\"value\":");
                if (vpos != std::string::npos) {
                    float val = std::strtof(cmd.c_str() + vpos + 8, nullptr);
                    PlayerAdmin::getInstance().setFadeDuration(std::clamp(val, 0.0f, 2.0f));
                }
            } else if (cmd.find("\"action\":\"set_sleep_timer\"") != std::string::npos) {
                size_t mpos = cmd.find("\"minutes\":");
                if (mpos != std::string::npos) {
                    int minutes = std::atoi(cmd.c_str() + mpos + 10);
                    if (minutes > 0) {
                        auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();
                        sleep_timer_target_sec_.store(now_sec + minutes * 60);
                    } else {
                        sleep_timer_target_sec_.store(0);
                    }
                }
            }

            // 立即广播一次最新全景状态
            broadcastState();
        }
    }

    // 清理断开连接的客户端
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = std::find(ws_clients_.begin(), ws_clients_.end(), client_fd);
        if (it != ws_clients_.end()) {
            ws_clients_.erase(it);
        }
    }
#if defined(_WIN32)
    closesocket(client_fd);
#else
    close(client_fd);
#endif
}

bool WebService::sendWsTextFrame(int fd, const std::string& text) {
    std::vector<uint8_t> frame;
    frame.reserve(text.size() + 10);
    frame.push_back(0x81); // FIN + Text opcode

    size_t len = text.size();
    if (len <= 125) {
        frame.push_back(static_cast<uint8_t>(len));
    } else if (len <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(len & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<uint8_t>((len >> (i * 8)) & 0xFF));
        }
    }

    frame.insert(frame.end(), text.begin(), text.end());

    std::lock_guard<std::mutex> lock(send_mutex_);
    size_t total_sent = 0;
    while (total_sent < frame.size()) {
#if defined(_WIN32)
        int sent = send(fd, (const char*)frame.data() + total_sent, static_cast<int>(frame.size() - total_sent), 0);
#else
        ssize_t sent = send(fd, (const char*)frame.data() + total_sent, frame.size() - total_sent, 0);
#endif
        if (sent <= 0) {
            return false;
        }
        total_sent += static_cast<size_t>(sent);
    }
    messages_sent_++;
    return true;
}

void WebService::broadcastWsText(const std::string& text) {
    std::vector<int> current_clients;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        current_clients = ws_clients_;
    }

    std::vector<int> dead_clients;
    for (int fd : current_clients) {
        if (!sendWsTextFrame(fd, text)) {
            dead_clients.push_back(fd);
        }
    }

    if (!dead_clients.empty()) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (int dead_fd : dead_clients) {
            auto it = std::find(ws_clients_.begin(), ws_clients_.end(), dead_fd);
            if (it != ws_clients_.end()) {
                ws_clients_.erase(it);
            }
#if defined(_WIN32)
            closesocket(dead_fd);
#else
            close(dead_fd);
#endif
        }
    }
}

std::string WebService::buildStateJson() {
    auto& pa = PlayerAdmin::getInstance();
    auto& tm = ThemeManager::getInstance();
    auto cur_opt = pa.getCurrentTrack();

    std::string title = cur_opt.has_value() ? escapeJson(cur_opt->title) : "暂未载入曲目";
    std::string artist = cur_opt.has_value() ? escapeJson(cur_opt->artist) : "未知艺术家";
    std::string album = cur_opt.has_value() ? escapeJson(cur_opt->album) : "纯音曲库";
    std::string specs = cur_opt.has_value() ? escapeJson(cur_opt->getFormatBadge()) : "Hi-Res 直通";

    std::string mode_str = "list";
    PlayMode pm = pa.getPlayMode();
    if (pm == PlayMode::LoopSingle) mode_str = "single";
    else if (pm == PlayMode::Shuffle) mode_str = "shuffle";
    else if (pm == PlayMode::Sequence) mode_str = "seq";

    auto eq_bands = pa.getEqBands();
    float spectrum[12] = {0.0f};
    if (pa.isPlaying()) {
        pa.getSpectrumLevels(spectrum, 12);
    }

    auto* mseb = MagicTuningView::getInstance();
    bool mseb_en = mseb ? mseb->isEnabled() : true;

    std::ostringstream json;
    json << "{\"type\":\"state\","
         << "\"is_playing\":" << (pa.isPlaying() ? "true" : "false") << ","
         << "\"title\":\"" << title << "\","
         << "\"artist\":\"" << artist << "\","
         << "\"album\":\"" << album << "\","
         << "\"specs\":\"" << specs << "\","
         << "\"current_time\":" << pa.getCurrentTimeSec() << ","
         << "\"duration\":" << pa.getDurationSec() << ","
         << "\"volume\":" << pa.getVolume() << ","
         << "\"is_muted\":" << (pa.isMuted() ? "true" : "false") << ","
         << "\"mode\":\"" << mode_str << "\","
         << "\"eq_enabled\":" << (pa.isEqEnabled() ? "true" : "false") << ","
         << "\"eq_bands\":[";
    for (size_t i = 0; i < 10; ++i) {
        if (i > 0) json << ",";
        json << eq_bands[i];
    }
    json << "],\"spectrum\":[";
    for (size_t i = 0; i < 12; ++i) {
        if (i > 0) json << ",";
        json << std::clamp(spectrum[i], 0.0f, 1.0f);
    }
    json << "],\"mseb_enabled\":" << (mseb_en ? "true" : "false")
         << ",\"mseb_params\":[";
    for (size_t i = 0; i < MagicTuningView::NUM_PARAMS; ++i) {
        if (i > 0) json << ",";
        json << (mseb ? mseb->getParamValue(i) : 0.0f);
    }
    json << "],\"theme_id\":" << static_cast<int>(tm.getCurrentTheme())
         << ",\"visual_mode\":" << static_cast<int>(tm.getBackgroundVisualMode())
         << ",\"fade_duration\":" << pa.getFadeDuration()
         << ",\"is_bit_perfect\":" << (pa.isBitPerfectDirect() ? "true" : "false")
         << ",\"is_scanning\":" << (MusicScanManager::getInstance().isScanning() ? "true" : "false")
         << "}";

    return json.str();
}

std::string WebService::buildQueueJson() {
    auto& pa = PlayerAdmin::getInstance();
    const auto& queue = pa.getPlaybackQueue();
    size_t cur_idx = pa.getCurrentTrackIndex();

    std::ostringstream json;
    json << "{\"type\":\"queue\","
         << "\"current_index\":" << cur_idx << ","
         << "\"tracks\":[";

    for (size_t i = 0; i < queue.size(); ++i) {
        if (i > 0) json << ",";
        json << "{\"id\":" << queue[i].id
             << ",\"title\":\"" << escapeJson(queue[i].title) << "\""
             << ",\"artist\":\"" << escapeJson(queue[i].artist) << "\""
             << ",\"album\":\"" << escapeJson(queue[i].album) << "\""
             << ",\"duration\":" << queue[i].duration_sec << "}";
    }

    json << "]}";
    return json.str();
}

std::string WebService::buildPlaylistsJson() {
    auto playlists = MusicDatabase::getInstance().loadPlaylists();
    std::ostringstream json;
    json << "{\"type\":\"playlists\",\"playlists\":[";
    for (size_t i = 0; i < playlists.size(); ++i) {
        if (i > 0) json << ",";
        json << "{\"id\":" << playlists[i].getId()
             << ",\"name\":\"" << escapeJson(playlists[i].getName()) << "\""
             << ",\"track_count\":" << playlists[i].getTrackCount()
             << ",\"duration\":" << playlists[i].getTotalDurationSec()
             << "}";
    }
    json << "]}";
    return json.str();
}

std::string WebService::buildLibraryJson() {
    auto all = MusicDatabase::getInstance().loadScannedTracks();
    std::ostringstream json;
    json << "{\"type\":\"library\",\"tracks\":[";
    for (size_t i = 0; i < all.size(); ++i) {
        if (i > 0) json << ",";
        json << "{\"id\":" << all[i].id
             << ",\"title\":\"" << escapeJson(all[i].title) << "\""
             << ",\"artist\":\"" << escapeJson(all[i].artist) << "\""
             << ",\"album\":\"" << escapeJson(all[i].album) << "\""
             << ",\"duration\":" << all[i].duration_sec
             << ",\"badge\":\"" << escapeJson(all[i].getFormatBadge()) << "\"}";
    }
    json << "]}";
    return json.str();
}

std::string WebService::buildDacJson() {
    auto hw = AudioDeviceTool::getHardwareStatus();
    auto* dac = DACSettingView::getInstance();
    int chip = dac ? dac->getSelectedChip() : 0;
    std::string chip_name = dac ? dac->getCurrentChipName() : (hw.has_external_dac ? hw.dac_name : "Apple Direct");

    std::ostringstream json;
    json << "{\"type\":\"dac\","
         << "\"has_external_dac\":" << (hw.has_external_dac ? "true" : "false") << ","
         << "\"dac_name\":\"" << escapeJson(hw.dac_name.empty() ? (hw.has_external_dac ? "外置 USB DAC" : "系统默认声卡") : hw.dac_name) << "\","
         << "\"active_output\":\"" << escapeJson(hw.active_output_name.empty() ? "系统默认音频输出" : hw.active_output_name) << "\","
         << "\"selected_chip\":" << chip << ","
         << "\"chip_name\":\"" << escapeJson(chip_name) << "\","
         << "\"params\":{";

    std::vector<std::string> keys = {
        "apple_exclusive", "apple_sample_rate", "apple_drive", "apple_bit_depth",
        "pcm_filter", "dsd_bypass", "dsd_cutoff", "pcm_dpll", "dsd_dpll",
        "thd_comp", "mono_mode", "output_mode", "phase",
        "akm_filter", "akm_color", "akm_dsd", "akm_exdf",
        "cs_filter", "cs_dsd", "cs_drive", "cs_impedance",
        "r2r_mode", "r2r_dsd", "r2r_clock", "r2r_phase",
        "rohm_filter", "rohm_clock", "rohm_dsd"
    };

    for (size_t i = 0; i < keys.size(); ++i) {
        if (i > 0) json << ",";
        int val = dac ? dac->getParam(keys[i]) : 0;
        json << "\"" << keys[i] << "\":" << val;
    }
    json << "}}";
    return json.str();
}

std::string WebService::buildMsebJson() {
    auto* mseb = MagicTuningView::getInstance();
    bool enabled = mseb ? mseb->isEnabled() : true;
    std::ostringstream json;
    json << "{\"type\":\"mseb\","
         << "\"enabled\":" << (enabled ? "true" : "false") << ","
         << "\"params\":[";
    for (size_t i = 0; i < MagicTuningView::NUM_PARAMS; ++i) {
        if (i > 0) json << ",";
        json << (mseb ? mseb->getParamValue(i) : 0.0f);
    }
    json << "]}";
    return json.str();
}

void WebService::broadcastState() {
    if (!is_running_.load()) return;
    std::string state_json = buildStateJson();
    broadcastWsText(state_json);
}

void WebService::broadcastLoop() {
    while (is_running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (!is_running_.load()) break;

        // 睡眠定时器检查
        int64_t target = sleep_timer_target_sec_.load();
        if (target > 0) {
            auto now_sec = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            if (now_sec >= target) {
                sleep_timer_target_sec_.store(0);
                PlayerAdmin::getInstance().pause();
                broadcastState();
            }
        }

        bool has_clients = false;
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            has_clients = !ws_clients_.empty();
        }

        // 有客户端连接时周期性广播（包括频谱跳动和时间推进）
        if (has_clients && PlayerAdmin::getInstance().isPlaying()) {
            broadcastState();
        }
    }
}
