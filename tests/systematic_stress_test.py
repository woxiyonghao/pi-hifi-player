#!/usr/bin/env python3
import os
import sys
import time
import subprocess
import signal
import struct
import random

CMD_FILE = "/tmp/pi_hifi_cmd"
SCREEN_TRIGGER = "/tmp/dump_screen"
SCREEN_PPM = "/tmp/screenshot.ppm"
LOG_FILE = "tests/stress_test.log"
SNAP_DIR = "tests/snapshots"

os.makedirs(SNAP_DIR, exist_ok=True)

def log(msg):
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
    formatted = f"[{timestamp}] {msg}"
    print(formatted, flush=True)
    with open(LOG_FILE, "a", encoding="utf-8") as f:
        f.write(formatted + "\n")

def send_cmd(cmd_str, timeout=3.0):
    start = time.time()
    with open(CMD_FILE, "w", encoding="utf-8") as f:
        f.write(cmd_str + "\n")
    while os.path.exists(CMD_FILE):
        time.sleep(0.02)
        if time.time() - start > timeout:
            log(f"[WARN] Command '{cmd_str}' timed out waiting to be consumed")
            try:
                os.unlink(CMD_FILE)
            except OSError:
                pass
            return False
    return True

def capture_screenshot(save_name=None):
    if os.path.exists(SCREEN_PPM):
        try:
            os.unlink(SCREEN_PPM)
        except OSError:
            pass
    with open(SCREEN_TRIGGER, "w") as f:
        pass
    start = time.time()
    while True:
        if os.path.exists(SCREEN_PPM):
            try:
                if os.path.getsize(SCREEN_PPM) >= 1843200:
                    break
            except OSError:
                pass
        time.sleep(0.05)
        if time.time() - start > 5.0:
            log("[WARN] Screenshot dump timed out or file incomplete")
            break
    try:
        size = os.path.getsize(SCREEN_PPM)
        if size < 1800000:
            log(f"[WARN] Screenshot PPM file too small ({size} bytes)")
            return None
        if save_name:
            dst_ppm = os.path.join(SNAP_DIR, save_name + ".ppm")
            dst_png = os.path.join(SNAP_DIR, save_name + ".png")
            os.system(f"cp {SCREEN_PPM} {dst_ppm} && sips -s format png {dst_ppm} --out {dst_png} >/dev/null 2>&1")
        return size
    except Exception as e:
        log(f"[ERROR] Screenshot error: {e}")
        return None

def get_process_rss_mb(pid):
    try:
        out = subprocess.check_output(["ps", "-o", "rss=", "-p", str(pid)]).decode().strip()
        if out:
            return float(out) / 1024.0
    except Exception:
        pass
    return 0.0

def main():
    duration_hours = 2.0
    if len(sys.argv) > 1:
        try:
            duration_hours = float(sys.argv[1])
        except ValueError:
            pass

    total_duration_sec = duration_hours * 3600.0
    log("===========================================================")
    log(f"启动发烧级数播 UI 系统性自动化高压稳定性测试 (预计运行: {duration_hours} 小时)")
    log("===========================================================")

    bin_path = "./build/PiHifiPlayer"
    if not os.path.exists(bin_path):
        log(f"[FATAL] 未找到二进制文件: {bin_path}")
        sys.exit(1)

    for f in [CMD_FILE, SCREEN_TRIGGER, SCREEN_PPM]:
        if os.path.exists(f):
            try:
                os.unlink(f)
            except OSError:
                pass

    log(f"[INIT] 启动 {bin_path}...")
    proc = subprocess.Popen([bin_path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    app_pid = proc.pid
    log(f"[INIT] 播放器进程已启动，PID: {app_pid}")

    time.sleep(1.5)
    if proc.poll() is not None:
        log(f"[FATAL] 播放器启动即崩溃，退出码: {proc.returncode}")
        sys.exit(1)

    initial_rss = get_process_rss_mb(app_pid)
    log(f"[METRIC] 初始常驻内存 (RSS): {initial_rss:.2f} MB")

    tabs = [
        (0, "ScanMusic"),
        (1, "Equalizer"),
        (2, "MSEBTuning"),
        (3, "DACSettings"),
        (4, "ThemeSettings"),
        (5, "SystemSettings"),
        (6, "AllMusic"),
        (7, "CustomPlaylist"),
        (8, "Terminal"),
        (9, "WifiTransfer")
    ]

    themes = [0, 1, 2, 3, 4, 5, 6, 7, 8]
    visuals = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    volumes = [0.1, 0.25, 0.45, 0.65, 0.85, 1.0, 0.5, 0.0]

    start_time = time.time()
    iteration = 0
    snapshot_interval_sec = 180.0
    last_snap_time = 0.0

    try:
        send_cmd("play")
        time.sleep(0.5)

        while True:
            elapsed = time.time() - start_time
            if elapsed >= total_duration_sec:
                log(f"[COMPLETE] 自动化测试已圆满运行满 {duration_hours} 小时 ({elapsed:.1f} 秒)")
                break

            ret = proc.poll()
            if ret is not None:
                log(f"[FATAL] 进程意外崩溃或退出！退出码: {ret}")
                sys.exit(2)

            iteration += 1
            cur_rss = get_process_rss_mb(app_pid)

            tab_id, tab_name = tabs[iteration % len(tabs)]
            send_cmd(f"tab {tab_id}")
            time.sleep(0.3)

            th_id = themes[(iteration * 3) % len(themes)]
            send_cmd(f"theme {th_id}")
            time.sleep(0.2)

            vis_id = visuals[(iteration * 7) % len(visuals)]
            send_cmd(f"visual {vis_id}")
            time.sleep(0.2)

            vol = volumes[iteration % len(volumes)]
            send_cmd(f"volume {vol}")
            time.sleep(0.2)

            action_rnd = random.random()
            if action_rnd < 0.15:
                send_cmd("next")
            elif action_rnd < 0.25:
                send_cmd("prev")
            elif action_rnd < 0.30:
                send_cmd("toggle")
                time.sleep(0.2)
                send_cmd("toggle")

            if tab_name in ["AllMusic", "CustomPlaylist", "DACSettings"]:
                for _ in range(4):
                    dy = random.choice([25.0, -30.0, 45.0, -50.0])
                    send_cmd(f"scroll {dy}")
                    time.sleep(0.1)

            if elapsed - last_snap_time >= snapshot_interval_sec:
                last_snap_time = elapsed
                snap_name = f"snap_{int(elapsed)}s_iter{iteration}_tab{tab_id}_vis{vis_id}"
                sz = capture_screenshot(snap_name)
                log(f"[ROUND {iteration}] 已运行: {elapsed/60.0:.1f}m | RSS内存: {cur_rss:.2f}MB (Δ: {cur_rss - initial_rss:+.2f}MB) | 截屏已生成: {snap_name}.png ({sz} bytes)")
            elif iteration % 15 == 0:
                log(f"[ROUND {iteration}] 已运行: {elapsed/60.0:.1f}m | RSS内存: {cur_rss:.2f}MB | 当前Tab: {tab_name} | 主题: {th_id} | 视觉: {vis_id} | 音量: {int(vol*100)}%")

            time.sleep(1.0)

    except KeyboardInterrupt:
        log("[INTERRUPT] 收到手动中断信号，安全停止测试")
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=3.0)
            except subprocess.TimeoutExpired:
                proc.kill()
        log("[SHUTDOWN] 测试进程已安全回收完毕")

if __name__ == "__main__":
    main()
