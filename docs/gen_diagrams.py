#!/usr/bin/env python3
"""Renders block_diagram, flowchart and circuit_diagram (PNG + JPG) for the
Smart Electronic Voting Machine. Same visual house-style as the other projects.
Run with:  py -3.12 gen_diagrams.py
"""
import os, math
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
FDIR = r"C:\Windows\Fonts"

def font(name, size):
    return ImageFont.truetype(os.path.join(FDIR, name), size)

REG, BOLD, MONO = "segoeui.ttf", "segoeuib.ttf", "consola.ttf"

# palette
BG     = (255, 255, 255)
INK    = (26, 32, 40)
MUTED  = (110, 122, 138)
LINE   = (176, 188, 202)
AUTH   = (125, 84, 184)     # authentication domain  (purple)
VOTER  = (32, 124, 196)     # voter interface        (blue)
MCU    = (35, 140, 86)      # firmware               (green)
CLIENT = (196, 96, 36)      # admin clients          (orange)
POWER  = (196, 128, 16)     # power chain            (amber)
DANGER = (214, 69, 48)      # denied / fault         (red)
DECIDE = (196, 128, 16)

def tint(c, a):
    return tuple(int(x + (255 - x) * a) for x in c)

def tsize(d, s, f):
    x0, y0, x1, y1 = d.textbbox((0, 0), s, font=f)
    return x1 - x0, y1 - y0

def box(d, xy, fill, outline, w=3, r=14):
    d.rounded_rectangle(xy, radius=r, fill=fill, outline=outline, width=w)

def centered(d, xy, lines, fonts, colors, lead=8):
    x0, y0, x1, y1 = xy
    hs = [tsize(d, s, f)[1] for s, f in zip(lines, fonts)]
    total = sum(hs) + lead * (len(lines) - 1)
    y = (y0 + y1) / 2 - total / 2
    for s, f, c, h in zip(lines, fonts, colors, hs):
        tw, _ = tsize(d, s, f)
        d.text(((x0 + x1) / 2 - tw / 2, y), s, font=f, fill=c)
        y += h + lead

def arrow(d, p0, p1, color=INK, w=3, head=13, dash=False):
    x0, y0 = p0; x1, y1 = p1
    if dash:
        L = math.hypot(x1 - x0, y1 - y0); n = max(int(L / 14), 1)
        for i in range(n):
            if i % 2: continue
            a = i / n; b = min((i + 1) / n, 1)
            d.line([x0 + (x1 - x0) * a, y0 + (y1 - y0) * a,
                    x0 + (x1 - x0) * b, y0 + (y1 - y0) * b], fill=color, width=w)
    else:
        d.line([x0, y0, x1, y1], fill=color, width=w)
    ang = math.atan2(y1 - y0, x1 - x0)
    for s in (2.6, -2.6):
        d.line([x1, y1, x1 + head * math.cos(ang + s), y1 + head * math.sin(ang + s)],
               fill=color, width=w)

def biarrow(d, p0, p1, color=INK, w=3, head=12):
    arrow(d, p0, p1, color, w, head)
    arrow(d, p1, p0, color, w, head)

def tag(d, x, y, s, f, color=MUTED, anchor="lt", pad=4):
    tw, th = tsize(d, s, f)
    if anchor == "ct": x -= tw / 2
    if anchor == "rt": x -= tw
    d.rectangle([x - pad, y - pad, x + tw + pad, y + th + pad], fill=BG)
    d.text((x, y), s, font=f, fill=color)

# ============================================================== BLOCK DIAGRAM
def block_diagram():
    W, H = 2500, 1580
    img = Image.new("RGB", (W, H), BG); d = ImageDraw.Draw(img)
    f_title = font(BOLD, 44); f_sub = font(REG, 26); f_dom = font(BOLD, 24)
    f_bx = font(BOLD, 27); f_bs = font(REG, 22); f_sm = font(REG, 20); f_lbl = font(BOLD, 20)

    d.text((60, 44), "Smart Electronic Voting Machine — Block Diagram", font=f_title, fill=INK)
    d.text((60, 102), "ESP32 DevKit V1 · fully offline · two-factor voter authentication (NFC + fingerprint)",
           font=f_sub, fill=MUTED)
    d.line([60, 150, W - 60, 150], fill=LINE, width=3)

    # ---- LEFT COLUMN : authentication (top) + voter interface (bottom) ----
    # authentication domain
    d.rounded_rectangle([60, 190, 700, 740], radius=18, fill=tint(AUTH, .95),
                        outline=tint(AUTH, .55), width=3)
    d.text((84, 208), "AUTHENTICATION", font=f_dom, fill=AUTH)
    box(d, [92, 258, 668, 470], tint(AUTH, .9), AUTH)
    centered(d, [92, 258, 668, 470],
             ["PN532 NFC Reader", "13.56 MHz · I²C @ 0x24", "reads card UID (1st factor)"],
             [f_bx, f_bs, f_bs], [INK, MUTED, MUTED])
    box(d, [92, 500, 668, 712], tint(AUTH, .9), AUTH)
    centered(d, [92, 500, 668, 712],
             ["AS608 Fingerprint", "optical · UART2 @ 57600", "verifies identity (2nd factor)"],
             [f_bx, f_bs, f_bs], [INK, MUTED, MUTED])

    # voter interface domain
    d.rounded_rectangle([60, 790, 700, 1400], radius=18, fill=tint(VOTER, .95),
                        outline=tint(VOTER, .55), width=3)
    d.text((84, 808), "VOTER INTERFACE", font=f_dom, fill=VOTER)
    box(d, [92, 858, 668, 992], tint(VOTER, .9), VOTER)
    centered(d, [92, 858, 668, 992],
             ["16×2 I²C LCD", "voter prompts only — admins never use it"],
             [f_bx, f_bs], [INK, MUTED])
    box(d, [92, 1016, 668, 1150], tint(VOTER, .9), VOTER)
    centered(d, [92, 1016, 668, 1150],
             ["UP · DOWN · OK Buttons", "candidate selection · confirm"],
             [f_bx, f_bs], [INK, MUTED])
    box(d, [92, 1174, 668, 1356], tint(VOTER, .9), VOTER)
    centered(d, [92, 1174, 668, 1356],
             ["Buzzer + Status LEDs", "green = accepted · red = denied", "audible/visual feedback"],
             [f_bx, f_bs, f_bs], [INK, MUTED, MUTED])

    # ---- CENTRE : ESP32 firmware ----
    d.rounded_rectangle([780, 190, 1760, 1400], radius=18, fill=tint(MCU, .95),
                        outline=tint(MCU, .55), width=3)
    d.text((804, 208), "ESP32 — FIRMWARE (FreeRTOS, dual-core)", font=f_dom, fill=MCU)
    rows = [
        ("Voter State Machine  (core 1)", "idle → NFC → fingerprint → select → cast"),
        ("Enrollment Workflow", "captures fingerprint + card for a new voter"),
        ("Data Store  (LittleFS + JSON)", "voters · candidates · votes · election state"),
        ("Web Server + REST API  (core 0)", "serves dashboard · /api/* endpoints"),
        ("WiFi Soft Access Point", "SSID VotingMachine · 192.168.4.1"),
    ]
    y = 266
    for t, s in rows:
        box(d, [812, y, 1728, y + 188], tint(MCU, .9), MCU, w=2, r=12)
        centered(d, [812, y, 1728, y + 188], [t, s], [f_bx, f_bs], [INK, MUTED], lead=8)
        y += 212

    # ---- RIGHT : admin clients ----
    d.rounded_rectangle([1840, 190, 2440, 1030], radius=18, fill=tint(CLIENT, .95),
                        outline=tint(CLIENT, .55), width=3)
    d.text((1864, 208), "ADMIN CLIENTS", font=f_dom, fill=CLIENT)
    box(d, [1872, 300, 2408, 560], tint(CLIENT, .9), CLIENT)
    centered(d, [1872, 300, 2408, 560],
             ["Phone / Tablet / Laptop", "Web Dashboard", "overview · voters · candidates",
              "results · election control"],
             [f_bx, f_bx, f_bs, f_bs], [INK, INK, MUTED, MUTED], lead=8)
    d.rounded_rectangle([1872, 600, 2408, 1000], radius=12, fill=BG,
                        outline=tint(CLIENT, .6), width=2)
    centered(d, [1872, 600, 2408, 1000],
             ["No internet, no cloud,", "no external server.", "The ESP32 hosts the",
              "dashboard itself over", "its own WiFi network."],
             [f_sm, f_sm, f_sm, f_sm, f_sm], [MUTED]*5, lead=10)

    # ---- arrows ----
    biarrow(d, (700, 364), (778, 440), AUTH, 4); tag(d, 712, 372, "I²C", f_lbl, AUTH, "lt")
    biarrow(d, (700, 606), (778, 560), AUTH, 4); tag(d, 712, 596, "UART2", f_lbl, AUTH, "lt")
    biarrow(d, (700, 925), (778, 760), VOTER, 4); tag(d, 712, 905, "I²C", f_lbl, VOTER, "lt")
    biarrow(d, (700, 1083), (778, 980), VOTER, 4); tag(d, 712, 1063, "GPIO", f_lbl, VOTER, "lt")
    biarrow(d, (700, 1265), (778, 1180), VOTER, 4); tag(d, 712, 1245, "GPIO", f_lbl, VOTER, "lt")
    arrow(d, (1760, 430), (1838, 430), MCU, 4); tag(d, 1770, 392, "WiFi / HTTP", f_lbl, MCU, "lt")
    arrow(d, (1838, 470), (1760, 470), MCU, 4)

    # ---- power chain ----
    d.rounded_rectangle([60, 1440, 2440, 1560], radius=14, fill=tint(POWER, .96),
                        outline=tint(POWER, .5), width=3)
    d.text((84, 1452), "POWER:", font=f_dom, fill=POWER)
    d.text((240, 1454),
           "5 V supply → ESP32 VIN (on-board reg → 3V3 for logic).   "
           "Separate AMS1117 regulator: 5 V → 3.3 V for PN532 · AS608 · LCD.   All grounds common.",
           font=f_bs, fill=INK)

    img.save(os.path.join(HERE, "block_diagram.png"))
    img.convert("RGB").save(os.path.join(HERE, "block_diagram.jpg"), quality=92)
    return img.size

# =================================================================== FLOWCHART
def flowchart():
    W, H = 1500, 3250
    img = Image.new("RGB", (W, H), BG); d = ImageDraw.Draw(img)
    f_title = font(BOLD, 42); f_sub = font(REG, 24)
    f_bx = font(BOLD, 28); f_lbl = font(BOLD, 21)

    d.text((60, 44), "How the Voting Machine Works", font=f_title, fill=INK)
    d.text((60, 100), "From power-on to a recorded vote", font=f_sub, fill=MUTED)
    d.line([60, 144, W - 60, 144], fill=LINE, width=3)

    CX = 680; BW = 760
    RIGHT = 1230; LEFT = 150

    def term(y, text, h=84):
        box(d, [CX - BW/2, y, CX + BW/2, y + h], tint(MCU, .88), MCU, r=42)
        centered(d, [CX - BW/2, y, CX + BW/2, y + h], [text], [f_bx], [INK]); return y + h
    def step(y, text, h=90, color=VOTER, sub=None):
        box(d, [CX - BW/2, y, CX + BW/2, y + h], tint(color, .92), color, r=12)
        lines = [text] if sub is None else [text, sub]
        centered(d, [CX - BW/2, y, CX + BW/2, y + h], lines,
                 [f_bx]*len(lines), [INK]*len(lines), lead=6); return y + h
    def dec(y, text, h=180, w=640, sub=None):
        cy = y + h/2
        d.polygon([(CX, y), (CX + w/2, cy), (CX, y + h), (CX - w/2, cy)],
                  fill=tint(DECIDE, .9), outline=DECIDE)
        lines = [text] if sub is None else [text, sub]
        centered(d, [CX - w/2, y, CX + w/2, y + h], lines, [f_bx]*len(lines),
                 [INK]*len(lines), lead=6); return y + h
    def down(y0, y1, label_=None):
        arrow(d, (CX, y0), (CX, y1), INK, 3)
        if label_: tag(d, CX + 16, (y0 + y1)/2 - 14, label_, f_lbl, INK)
    def reject(y_mid, text):
        # branch right to a red "denied" box, then merge back down to loop
        box(d, [RIGHT - 150, y_mid - 48, RIGHT + 150, y_mid + 48], tint(DANGER, .9), DANGER, r=12)
        centered(d, [RIGHT - 150, y_mid - 48, RIGHT + 150, y_mid + 48], [text], [f_lbl], [INK])
        arrow(d, (CX + 320, y_mid), (RIGHT - 150, y_mid), DANGER, 3)
        tag(d, CX + 330, y_mid - 36, "No", f_lbl, DANGER)

    GAP = 50; y = 190
    y = term(y, "START"); ny = y + GAP; down(y, ny); y = ny
    y = step(y, "Boot: load data, start WiFi AP +", color=MCU, h=112, sub="web dashboard, ready chime")
    ny = y + GAP; down(y, ny); y = ny

    loop_top = y
    dtop = y; y = dec(y, "Election running?"); dmid = (dtop + y)/2
    # No -> election closed box on right, loop back
    box(d, [RIGHT - 150, dmid - 48, RIGHT + 150, dmid + 48], tint(POWER, .9), POWER, r=12)
    centered(d, [RIGHT - 150, dmid - 48, RIGHT + 150, dmid + 48], ["Election", "Closed"], [f_lbl, f_lbl], [INK, INK], lead=4)
    arrow(d, (CX + 320, dmid), (RIGHT - 150, dmid), POWER, 3); tag(d, CX + 330, dmid - 36, "No", f_lbl, POWER)
    ny = y + GAP; down(y, ny, "Yes"); y = ny

    y = step(y, 'Display "Tap NFC Card"'); ny = y + GAP; down(y, ny); y = ny
    dtop = y; y = dec(y, "Card recognised?"); dmid = (dtop + y)/2
    reject(dmid, "Access Denied"); ny = y + GAP; down(y, ny, "Yes"); y = ny

    dtop = y; y = dec(y, "Already voted?"); dmid = (dtop + y)/2
    # Yes -> denied (reject uses 'No' label, so custom here)
    box(d, [RIGHT - 150, dmid - 48, RIGHT + 150, dmid + 48], tint(DANGER, .9), DANGER, r=12)
    centered(d, [RIGHT - 150, dmid - 48, RIGHT + 150, dmid + 48], ["Already Voted"], [f_lbl], [INK])
    arrow(d, (CX + 320, dmid), (RIGHT - 150, dmid), DANGER, 3); tag(d, CX + 330, dmid - 36, "Yes", f_lbl, DANGER)
    ny = y + GAP; down(y, ny, "No"); y = ny

    y = step(y, 'Display "Place Finger"'); ny = y + GAP; down(y, ny); y = ny
    dtop = y; y = dec(y, "Fingerprint matches", sub="the card's owner?"); dmid = (dtop + y)/2
    reject(dmid, "Access Denied"); ny = y + GAP; down(y, ny, "Yes"); y = ny

    y = step(y, "Accredited — press OK", color=MCU); ny = y + GAP; down(y, ny); y = ny
    y = step(y, "Select candidate (UP / DOWN)", h=112, sub="then OK to choose"); ny = y + GAP; down(y, ny); y = ny
    y = step(y, "Confirm the choice"); ny = y + GAP; down(y, ny); y = ny
    y = step(y, "Record vote: mark voter as voted,", color=MCU, h=130,
             sub="increment candidate, save to flash"); ny = y + GAP; down(y, ny); y = ny
    y = step(y, 'Beep + "Vote Recorded — Thank You"', color=MCU)

    # loop back to election check
    d.line([CX - BW/2 - 30, y - 45, LEFT, y - 45], fill=MCU, width=4)
    d.line([LEFT, y - 45, LEFT, loop_top + 90], fill=MCU, width=4)
    arrow(d, (LEFT, loop_top + 90), (CX - BW/2 - 6, loop_top + 90), MCU, 4)
    tag(d, LEFT + 16, (loop_top + y)/2, "Next voter", f_lbl, MCU)

    img = img.crop((0, 0, W, y + 80))
    img.save(os.path.join(HERE, "flowchart.png"))
    img.convert("RGB").save(os.path.join(HERE, "flowchart.jpg"), quality=92)
    return img.size

# ============================================================== CIRCUIT DIAGRAM
def circuit_diagram():
    W, H = 1760, 1240
    img = Image.new("RGB", (W, H), BG); d = ImageDraw.Draw(img)
    f_title = font(BOLD, 40); f_sub = font(REG, 23)
    f_ic = font(BOLD, 32); f_mod = font(BOLD, 25); f_pin = font(MONO, 19); f_sig = font(BOLD, 19)
    f_sm = font(REG, 19)

    d.text((60, 40), "Smart Voting Machine — Circuit / Wiring Diagram", font=f_title, fill=INK)
    d.text((60, 92), "ESP32 DevKit V1 pin connections. Sensors and LCD at 3.3 V; common ground throughout.",
           font=f_sub, fill=MUTED)
    d.line([60, 134, W - 60, 134], fill=LINE, width=3)

    # ESP32 in the centre
    EX0, EY0, EX1, EY1 = 695, 250, 1065, 1115
    box(d, [EX0, EY0, EX1, EY1], tint(MCU, .95), MCU, w=3, r=16)
    centered(d, [EX0, EY0, EX1, EY0 + 86], ["ESP32", "DevKit V1"], [f_ic, f_mod], [INK, MCU], lead=4)

    def epin(y, label, side):
        if side == "L":
            d.line([EX0 - 26, y, EX0, y], fill=INK, width=3)
            d.text((EX0 + 12, y - 11), label, font=f_pin, fill=INK)
            return (EX0 - 26, y)
        else:
            d.line([EX1, y, EX1 + 26, y], fill=INK, width=3)
            tw, _ = tsize(d, label, f_pin); d.text((EX1 - 12 - tw, y - 11), label, font=f_pin, fill=INK)
            return (EX1 + 26, y)

    def module(xy, title, lines, color):
        box(d, xy, tint(color, .92), color, w=3, r=12)
        x0, y0, x1, y1 = xy
        d.text((x0 + 18, y0 + 12), title, font=f_mod, fill=color)
        yy = y0 + 52
        for ln in lines:
            d.text((x0 + 18, yy), ln, font=f_sm, fill=INK); yy += 26
        return xy

    def wire(p0, p1, color, label=None):
        """horizontal-first elbow: (x0,y0)->(x1,y0)->(x1,y1)"""
        x0, y0 = p0; x1, y1 = p1
        d.line([x0, y0, x1, y0], fill=color, width=3)
        d.line([x1, y0, x1, y1], fill=color, width=3)
        if label:
            tag(d, (x0 + x1) / 2, y0 - 24, label, f_sig, color, "ct")

    def dot(x, y, color):
        d.ellipse([x - 5, y - 5, x + 5, y + 5], fill=color)

    # ---------------- LEFT modules ----------------
    module([90, 290, 560, 450], "PN532 NFC", ["VCC 3.3V   GND", "SDA  SCL", "IRQ  RST"], AUTH)
    module([90, 510, 560, 650], "16x2 I2C LCD", ["VCC 3.3V   GND", "SDA  SCL (shared bus)"], VOTER)
    module([90, 710, 560, 870], "AS608 Fingerprint", ["VCC 3.3V   GND", "TX  ->  RX2", "RX  ->  TX2"], AUTH)

    # left pins
    p_sda = epin(330, "GPIO21 SDA", "L")
    p_scl = epin(375, "GPIO22 SCL", "L")
    p_irq = epin(430, "GPIO4  IRQ", "L")
    p_rst = epin(475, "GPIO5  RST", "L")
    p_rx2 = epin(770, "GPIO16 RX2", "L")
    p_tx2 = epin(815, "GPIO17 TX2", "L")
    p_vin = epin(975, "VIN  5V", "L")
    p_gnd = epin(1020, "GND", "L")

    # PN532 -> I2C + IRQ/RST
    wire((560, 360), p_sda, VOTER, "SDA")
    wire((560, 395), p_scl, VOTER, None)
    wire((560, 420), p_irq, AUTH, "IRQ/RST")
    wire((560, 435), p_rst, AUTH, None)
    # LCD taps onto the same I2C bus (junction dots on the PN532 SDA/SCL runs)
    d.line([560, 560, 615, 560], fill=VOTER, width=3); d.line([615, 560, 615, 360], fill=VOTER, width=3)
    dot(615, 360, VOTER)
    d.line([560, 595, 635, 595], fill=VOTER, width=3); d.line([635, 595, 635, 375], fill=VOTER, width=3)
    dot(635, 375, VOTER)
    # AS608 -> UART2
    wire((560, 775), p_rx2, AUTH, "TX->RX2")
    wire((560, 810), p_tx2, AUTH, None)

    # ---------------- RIGHT modules ----------------
    module([1200, 290, 1670, 470], "Buttons", ["UP     -> GPIO25", "DOWN   -> GPIO26",
                                                "OK     -> GPIO27", "other leg -> GND"], VOTER)
    module([1200, 520, 1670, 660], "Buzzer (low-trig)", ["IO  -> GPIO23", "VCC 3.3V   GND"], CLIENT)
    module([1200, 710, 1670, 870], "Status LEDs", ["Green -> GPIO32", "Red   -> GPIO33",
                                                    "via 220-330 ohm -> GND"], CLIENT)
    p_b1 = epin(330, "GPIO25", "R")
    p_b2 = epin(375, "GPIO26", "R")
    p_b3 = epin(420, "GPIO27", "R")
    p_bz = epin(575, "GPIO23", "R")
    p_lg = epin(760, "GPIO32", "R")
    p_lr = epin(805, "GPIO33", "R")
    wire((1200, 350), p_b1, VOTER, None)
    wire((1200, 380), p_b2, VOTER, None)
    wire((1200, 410), p_b3, VOTER, None)
    wire((1200, 575), p_bz, CLIENT, "IO")
    wire((1200, 760), p_lg, CLIENT, None)
    wire((1200, 800), p_lr, CLIENT, None)

    # ---------------- POWER (aligned with VIN/GND pins) ----------------
    module([90, 940, 560, 1130], "Power Supply",
           ["5V supply  ->  ESP32 VIN", "AMS1117:  5V -> 3.3V",
            "3.3V -> PN532, AS608, LCD", "ALL grounds tied common"], POWER)
    module([1200, 940, 1670, 1130], "Grounding",
           ["Button + LED legs -> GND", "Buzzer GND -> GND",
            "single common ground;", "prevents I2C / UART faults"], POWER)
    wire((560, 975), p_vin, POWER, "5V")
    wire((560, 1020), p_gnd, POWER, "GND")

    img.save(os.path.join(HERE, "circuit_diagram.png"))
    img.convert("RGB").save(os.path.join(HERE, "circuit_diagram.jpg"), quality=92)
    return img.size

if __name__ == "__main__":
    print("block_diagram:", block_diagram())
    print("flowchart:", flowchart())
    print("circuit_diagram:", circuit_diagram())
