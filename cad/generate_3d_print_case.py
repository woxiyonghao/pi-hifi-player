#!/usr/bin/env python3
"""
Generate v3 3D-Printer Ready HiFi Streamer Enclosure
Specifically tailored to:
1. Dock the ALREADY-PRINTED Waveshare 70H-1024600 Screen Shell (~178x108x14mm) directly into the front cartridge bay!
2. Built-in mount for Waveshare HDMI Adapter Board (50x25.5mm, holes 44.7x20.2mm).
3. Left Bay: 2.5-inch SATA SSD/HDD Cradle (100x70mm) with bottom M3 mounting holes.
4. Right Bay: Raspberry Pi 5 + S1 (ES9038Q2M) DAC stack (58x49mm standoffs) with direct rear I/O.
5. Quad-side high-airflow vents (Top + Bottom + Left + Right) + Rear 40mm (4010) External Fan Mount (32x32mm hole pitch).
"""
import os
from build123d import *

out_dir = "/Users/mk10/Desktop/hifi_player/cad/3d_print_case"
os.makedirs(out_dir, exist_ok=True)

W = 220.0        # 机箱总宽 (mm)
H = 130.0        # 机箱总高 (mm)
D = 145.0        # 机箱总深 (145mm，前部14.5mm嵌入已打印的屏幕壳 + 后部主机仓)
WALL = 4.5       # 机箱壁厚 (mm)
DOCK_W = 175.80  # 精准匹配微雪官方3D打印壳外宽 175.30mm (+0.50mm装配余量，单边0.25mm)
DOCK_H = 105.80  # 精准匹配微雪官方3D打印壳外高 105.30mm (+0.50mm装配余量，单边0.25mm)
DOCK_D = 11.00   # 精准匹配微雪官方3D打印壳厚度 11.00mm (嵌入后前脸100%齐平)
BULKHEAD_T = 4.0 # 屏幕母槽后方的承重隔板厚度 (mm)

print(">> 正在生成 v3.1 精准匹配版 3D 打印模型 (100%对齐微雪 70H-1024600-QLED-CT 官方打印壳 STP)...")

cavity_z0 = DOCK_D + BULKHEAD_T  # 15.0 mm
inner_w = W - 2 * WALL  # 211.0 mm
inner_h = H - 2 * WALL  # 121.0 mm
inner_d = D - cavity_z0 # 130.0 mm
floor_y = -inner_h / 2  # -60.5 mm

# ==============================================================================
# 1. 一体式主壳体 (01_Main_Body_Dock_For_Printed_Screen_220x130x145.stl)
# ==============================================================================
with BuildPart() as main_shell:
    # 外部大圆角箱体 (Z 从 0 到 D=145)
    with Locations((0, 0, D / 2)):
        Box(W, H, D)
    
    # 四条侧棱做 WiiM 风格大圆角 (R = 12.0mm)
    z_edges = main_shell.edges().filter_by(Axis.Z)
    fillet(z_edges, 12.0)
    
    # 【前部核心】：已打印微雪屏幕外壳 (175.30 x 105.30 x 11.00 mm, R4.15) 的整体嵌入母槽
    with BuildSketch(Plane.XY.offset(0.0)) as dock_sk:
        RectangleRounded(DOCK_W, DOCK_H, 4.2)
    extrude(amount=DOCK_D, mode=Mode.SUBTRACT)
    
    # 【上方排线专属过线窗】：对称宽口设计 (X: -32~+32, Y_rel: +24.5~+51.5)，精准覆盖屏幕壳背部 40-Pin FPC 排线出口
    with BuildSketch(Plane.XY.offset(0.0)) as fpc_sk:
        with Locations((0.0, 38.0)):
            RectangleRounded(64.0, 27.0, 4.0)
    extrude(amount=cavity_z0 + 1.0, mode=Mode.SUBTRACT)

    # 【中部横梁 2颗原装背孔锁紧位】：精准对准已打印屏幕壳背面的 2颗 M4 固定孔 (X = ±37.50mm, Y_rel = +10.00mm)
    for mx in [-37.50, 37.50]:
        with Locations((mx, 10.00, cavity_z0 / 2)):
            Cylinder(radius=2.2, height=cavity_z0 + 4.0, mode=Mode.SUBTRACT)

    # 【下方减重与辅助通风大窗】：避开 Y=+10mm 锁紧横梁，在 Y=-42 ~ -4mm 处开窗
    with BuildSketch(Plane.XY.offset(0.0)) as pass_sk:
        with Locations((0.0, -23.0)):
            RectangleRounded(125.0, 38.0, 6.0)
    extrude(amount=cavity_z0 + 1.0, mode=Mode.SUBTRACT)
    
    # 掏空内部主腔体 (从 Z = 15.0mm 到 Z = 145mm)
    with BuildSketch(Plane.XY.offset(cavity_z0)) as inner_sk:
        RectangleRounded(inner_w, inner_h, 12.0 - WALL)
    extrude(amount=inner_d, mode=Mode.SUBTRACT)
    
    # 背面止口台阶 (用于嵌入 3.0mm 厚的后盖板)
    with BuildSketch(Plane.XY.offset(D - 3.0)) as back_lip_sk:
        RectangleRounded(inner_w + 4.0, inner_h + 4.0, 12.0 - WALL + 2.0)
    extrude(amount=3.0, mode=Mode.SUBTRACT)
    
    # 四角后盖螺丝固定柱 (内嵌融合进内壁四角)
    boss_x = inner_w / 2 - 3.5
    boss_y = inner_h / 2 - 3.5
    boss_locs = [
        (-boss_x, -boss_y), (boss_x, -boss_y),
        (-boss_x,  boss_y), (boss_x,  boss_y)
    ]
    with BuildSketch(Plane.XY.offset(cavity_z0 - 1.0)) as boss_sk:
        with Locations(boss_locs):
            Circle(radius=5.5)
    extrude(amount=D - cavity_z0 - 2.0, mode=Mode.ADD)
    
    with Locations([(x, y, D - 15.0) for x, y in boss_locs]):
        Cylinder(radius=1.4, height=30.0, mode=Mode.SUBTRACT)
        
    # 【右舱位】：树莓派 5 + S1 解码板 双层模块安装柱 (4x M2.5, 58.0 x 49.0 mm)
    pi_center_x = 50.0
    pi_back_z = D - 3.0 - 4.0
    pi_z1 = pi_back_z - 3.5
    pi_z2 = pi_z1 - 58.0
    pi_standoff_locs = [
        (pi_center_x - 24.5, floor_y + 2.0, pi_z1),
        (pi_center_x + 24.5, floor_y + 2.0, pi_z1),
        (pi_center_x - 24.5, floor_y + 2.0, pi_z2),
        (pi_center_x + 24.5, floor_y + 2.0, pi_z2),
    ]
    for sx, sy, sz in pi_standoff_locs:
        with Locations((sx, sy, sz)):
            Cylinder(radius=3.8, height=8.0, rotation=(90, 0, 0), mode=Mode.ADD)
            Cylinder(radius=1.15, height=10.0, rotation=(90, 0, 0), mode=Mode.SUBTRACT)

    # 【右前区】：微雪 HDMI 小转接板固定柱 (尺寸 50x25.5mm，官方孔距 44.70 x 20.20 mm)
    adap_cx = 50.0
    adap_cz = cavity_z0 + 20.0
    adap_locs = [
        (adap_cx - 22.35, floor_y + 1.5, adap_cz - 10.10),
        (adap_cx + 22.35, floor_y + 1.5, adap_cz - 10.10),
        (adap_cx - 22.35, floor_y + 1.5, adap_cz + 10.10),
        (adap_cx + 22.35, floor_y + 1.5, adap_cz + 10.10),
    ]
    for ax, ay, az in adap_locs:
        with Locations((ax, ay, az)):
            Cylinder(radius=3.2, height=7.0, rotation=(90, 0, 0), mode=Mode.ADD)
            Cylinder(radius=1.15, height=9.0, rotation=(90, 0, 0), mode=Mode.SUBTRACT)

    # 【左舱位】：2.5 寸 SATA 硬盘专属限位导轨托架 (101 x 70.6 mm) + 底部 M3 固定孔
    hdd_center_x = -48.0
    hdd_center_z = cavity_z0 + 62.0
    hdd_w = 70.6
    hdd_l = 101.0
    rail_t = 3.2
    rail_h = 9.0
    
    # 导轨向下深入底板 1.5mm 确保 100% 布尔融合
    with Locations((hdd_center_x - hdd_w / 2 - rail_t / 2, floor_y + rail_h / 2 - 1.5, hdd_center_z)):
        Box(rail_t, rail_h, hdd_l, mode=Mode.ADD)
    with Locations((hdd_center_x + hdd_w / 2 + rail_t / 2, floor_y + rail_h / 2 - 1.5, hdd_center_z)):
        Box(rail_t, rail_h, hdd_l, mode=Mode.ADD)
    with Locations((hdd_center_x, floor_y + rail_h / 2 - 1.5, hdd_center_z + hdd_l / 2 + rail_t / 2)):
        Box(hdd_w + 2 * rail_t, rail_h, rail_t, mode=Mode.ADD)
        
    hdd_hole_locs = [
        (hdd_center_x - 30.86, -H / 2, hdd_center_z - 38.30),
        (hdd_center_x + 30.86, -H / 2, hdd_center_z - 38.30),
        (hdd_center_x - 30.86, -H / 2, hdd_center_z + 38.30),
        (hdd_center_x + 30.86, -H / 2, hdd_center_z + 38.30),
    ]
    for hx, hy, hz in hdd_hole_locs:
        with Locations((hx, hy, hz)):
            Cylinder(radius=1.6, height=14.0, rotation=(90, 0, 0), mode=Mode.SUBTRACT)

    # 【四维立体散热风道】：顶部 + 底部 + 左侧壁 + 右侧壁 大面积出风百叶窗
    # 1. 顶部与底部各 11 条大通量散热槽
    for y_pos in [-H / 2, H / 2]:
        for i in range(-5, 6):
            with Locations((i * 14.0, y_pos, cavity_z0 + inner_d / 2)):
                Box(5.0, WALL * 3, 64.0, mode=Mode.SUBTRACT)
                
    # 2. 左右两侧壁双排横向通风百叶槽
    for x_pos in [-W / 2, W / 2]:
        for row_y in [-20.0, 20.0]:
            for k in range(-3, 4):
                with Locations((x_pos, row_y, cavity_z0 + inner_d / 2 + k * 11.0)):
                    Box(WALL * 3, 25.0, 4.5, mode=Mode.SUBTRACT)

    # 底部外壁：4 个避震机脚安装孔 (M3)
    feet_locs = [
        (-80.0, -H / 2, 30.0), (80.0, -H / 2, 30.0),
        (-80.0, -H / 2, D - 30.0), (80.0, -H / 2, D - 30.0)
    ]
    for fx, fy, fz in feet_locs:
        with Locations((fx, fy, fz)):
            Cylinder(radius=1.4, height=12.0, rotation=(90, 0, 0), mode=Mode.SUBTRACT)

print("Main shell bounding box:", main_shell.part.bounding_box())

shell_stl = os.path.join(out_dir, "01_Main_Body_Dock_For_Printed_Screen_220x130x145.stl")
shell_step = os.path.join(out_dir, "01_Main_Body_Dock_For_Printed_Screen_220x130x145.step")
export_stl(main_shell.part, shell_stl)
export_step(main_shell.part, shell_step)
print(f"✅ v3 主壳体生成完成: {shell_stl}")
