"""YamuraLog Hub - Teensy 4.1 carrier board (2-layer, all through-hole).

Single source of truth for parts, placement and connectivity. build_pcb.py and
build_sch.py both read this file, so the schematic and board always agree.

Pin assignments come from YamuraHubV6.1/src/main.h. Differences from the
Teensy 4.0 build:
  * SD card uses the Teensy 4.1 built-in SDIO slot (BUILTIN_SDCARD), so the
    old SD_CS (pin 3) to the display's SD slot is gone. Display SD_CS is left
    unconnected.
  * Display SDO/MISO is isolated by default (JP1 open): the ST7796 SDO does not
    tri-state and would fight the AirLift on the shared MISO line, and the
    display's own SD slot is no longer used.

Rev B layout: the display plugs into J4 on the front; every other part is on the
back. Board is the display outline plus margins for four corner mounting holes.
Buttons and LEDs are panel-mounted (J6/J7 headers only).
"""

BOARD_W, BOARD_H = 122.0, 68.0          # mm (display 108 x 60.88 + corner-hole margins)
TITLE = "YamuraLog Hub - Teensy 4.1"
REV = "B"

KLIB = "C:/Program Files/KiCad/10.0/share/kicad/footprints/"

# Net classes: (track width mm, nets)
NETCLASS = {
    "Power12": (1.0, ["+12V_IN", "+12V", "+12V_BUS"]),
    "Power5":  (0.8, ["+5V"]),
    "Power3":  (0.5, ["+3V3"]),
    "Gnd":     (0.6, ["GND"]),
}
SIGNAL_W = 0.3
CLEARANCE = 0.2

# ---------------------------------------------------------------------------
# Teensy 4.1 pinout, DIP-style numbering of the 48 edge pins (top view, USB up):
# left row top->bottom = 1..24, right row bottom->top = 25..48
TEENSY_LEFT = ["GND", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12",
               "3V3", "24", "25", "26", "27", "28", "29", "30", "31", "32"]
TEENSY_RIGHT_TOP_DOWN = ["VIN", "GND", "3V3", "23", "22", "21", "20", "19", "18", "17", "16",
                         "15", "14", "13", "GND", "41", "40", "39", "38", "37", "36", "35", "34", "33"]
TEENSY_PINS = {}            # pin number -> name
for i, n in enumerate(TEENSY_LEFT):
    TEENSY_PINS[str(i + 1)] = n
for k, n in enumerate(TEENSY_RIGHT_TOP_DOWN):
    TEENSY_PINS[str(48 - k)] = n

def tpin(name):
    """Teensy pin number for a pin name (first match, e.g. 'D11' -> '13')."""
    name = name.lstrip("D")
    for num, n in TEENSY_PINS.items():
        if n == name:
            return num
    raise KeyError(name)

# Teensy net map by pin name (from main.h)
TEENSY_NETS = {
    "VIN": "+5V", "3V3": "+3V3", "GND": "GND",
    "D2": "TFT_DC", "D4": "TFT_RST", "D15": "TFT_CS",
    "D5": "WIFI_CS", "D6": "WIFI_RST", "D9": "WIFI_BUSY",
    "D11": "MOSI", "D12": "MISO", "D13": "SCK",
    "D16": "BTN_SELECT", "D17": "BTN_UP", "D18": "BTN_DOWN",
    "D21": "LED_REC_DRV", "D22": "LED_GPS_DRV",
    "D30": "CAN_RX", "D31": "CAN_TX",           # CAN3 (FlexCAN_T4FD<CAN3>)
    "D24": "I2C2_SCL", "D25": "I2C2_SDA",       # Wire2, spare for an on-board GPS
}
teensy_pinnets = {}
for num, n in TEENSY_PINS.items():
    if n in ("VIN", "3V3", "GND"):
        teensy_pinnets[num] = TEENSY_NETS[n]
    elif "D" + n in TEENSY_NETS:
        teensy_pinnets[num] = TEENSY_NETS["D" + n]

# ---------------------------------------------------------------------------
# Parts. pins: pad number -> net (unlisted pads are unconnected).
# pos = footprint origin (mm, top view), rot = degrees (KiCad CCW) *before* flipping.
# side "B" parts are flipped to the back (mirrored in x about pos).
# sym = schematic symbol (lib:name, or 'local:<name>' for project symbols)

# Display module MSP4030/4031: PCB 108 x 60.88, M3 holes on 102 x 54.88 (3.0 mm in from
# each edge), 14-pin header 2.0 mm in from the left short edge, pins centred
# (13.93 mm from top/bottom), pin 1 (VCC) nearest the bottom edge (front view).
DISP_W, DISP_H = 108.0, 60.88
DISP_X0, DISP_Y0 = (BOARD_W - DISP_W) / 2, (BOARD_H - DISP_H) / 2
DISP_HOLES = [(DISP_X0 + 3.0, DISP_Y0 + 3.0), (DISP_X0 + 105.0, DISP_Y0 + 3.0),
              (DISP_X0 + 3.0, DISP_Y0 + 57.88), (DISP_X0 + 105.0, DISP_Y0 + 57.88)]
DISP_HDR_X, DISP_HDR_PIN1_Y = DISP_X0 + 2.0, DISP_Y0 + 13.93 + 33.02
CORNER_HOLES = [(3.5, 3.5), (BOARD_W - 3.5, 3.5), (3.5, BOARD_H - 3.5), (BOARD_W - 3.5, BOARD_H - 3.5)]
DB9_Y = BOARD_H - 10.54                  # DB9 flange flush with bottom edge
DB9_PINS = {"2": "CANL", "7": "CANH", "3": "GND", "6": "GND", "9": "+12V_BUS", "SH": "GND"}
DB9_FP = "Connector_Dsub:DSUB-9_Pins_Horizontal_P2.77x2.84mm_EdgePinOffset7.70mm_Housed_MountingHolesOffset9.12mm"

PARTS = [
    dict(ref="A1", value="Teensy 4.1", fp="YamuraHub:Teensy41", pos=(99.06, 34.29), rot=0, side="B",
         ref_pos=(88.0, 2.2), sym="local:Teensy41", pins=teensy_pinnets),
    dict(ref="A2", value="Adafruit AirLift (ESP32)", fp="Connector_PinSocket_2.54mm:PinSocket_1x12_P2.54mm_Vertical",
         pos=(53.975, 7.62), rot=-90, side="B", ref_pos=(67.945, 4.9), sym="local:AirLift",
         pins={"1": "+5V", "3": "GND", "4": "SCK", "5": "MISO", "6": "MOSI", "7": "WIFI_CS",
               "8": "WIFI_BUSY", "9": "WIFI_RST"},
         pinnames=["VIN", "3Vo", "GND", "SCK", "MISO", "MOSI", "CS", "BUSY", "~{RST}", "GP0", "RXI", "TXO"]),
    dict(ref="A3", value="Waveshare SN65HVD230 CAN board", fp="Connector_PinSocket_2.54mm:PinSocket_1x04_P2.54mm_Vertical",
         pos=(17.78, 27.94), rot=0, side="B", sym="local:CANBoard",
         pins={"1": "+3V3", "2": "GND", "3": "CAN_TX", "4": "CAN_RX"},
         pinnames=["3V3", "GND", "CTX", "CRX"]),
    dict(ref="J2", value="CAN H/L (from A3)", fp="Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical",
         pos=(40.64, 50.8), rot=-90, side="B", sym="Connector_Generic:Conn_01x02",
         pins={"1": "CANH", "2": "CANL"}, pinnames=["CANH", "CANL"]),
    dict(ref="J3", value="DB9 male - CAN bus 1", fp=DB9_FP, pos=(35.0, DB9_Y), rot=0, side="B",
         sym="Connector:DE9_Pins_MountingHoles", pins=DB9_PINS),
    dict(ref="J8", value="DB9 male - CAN bus 2", fp=DB9_FP, pos=(67.1, DB9_Y), rot=0, side="B",
         sym="Connector:DE9_Pins_MountingHoles", pins=DB9_PINS),
    dict(ref="J1", value="12V in", fp="TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal",
         pos=(81.1, 62.23), rot=180, side="B", sym="Connector:Screw_Terminal_01x02",
         pins={"1": "+12V_IN", "2": "GND"}, ref_pos=(83.6, 54.6),
         silk=[("+12V", 81.1, 56.4), ("GND", 86.18, 56.4)]),
    dict(ref="D1", value="1N5822", fp="Diode_THT:D_DO-201AD_P15.24mm_Horizontal",
         pos=(66.04, 38.1), rot=0, side="B", sym="Device:D_Schottky",
         pins={"2": "+12V_IN", "1": "+12V"}),         # pad1 = cathode
    dict(ref="D4", value="1.5KE20A (TVS)", fp="Diode_THT:D_DO-201AE_P5.08mm_Vertical_CathodeUp",
         pos=(73.66, 35.56), rot=-90, side="B", sym="Device:D_TVS",
         pins={"1": "+12V", "2": "GND"}),         # load-dump clamp, pad1 = cathode
    dict(ref="F1", value="MF-RG110 (1.1A PTC)", fp="Fuse:Fuse_Bourns_MF-RG1100",
         pos=(24.13, 50.8), rot=0, side="B", sym="Device:Polyfuse",
         pins={"1": "+12V", "2": "+12V_BUS"}),
    dict(ref="U1", value="R-78E5.0-1.0", fp="Converter_DCDC:Converter_DCDC_RECOM_R-78E-0.5_THT",
         pos=(69.85, 51.44), rot=0, side="B", sym="Converter_DCDC:R-78E5.0-1.0",
         pins={"1": "+12V", "2": "GND", "3": "+5V"}),
    dict(ref="C1", value="47uF 35V", fp="Capacitor_THT:CP_Radial_D8.0mm_P3.50mm",
         pos=(54.3, 48.26), rot=0, side="B", sym="Device:C_Polarized",
         pins={"1": "+12V", "2": "GND"}),
    dict(ref="C2", value="47uF 16V", fp="Capacitor_THT:CP_Radial_D8.0mm_P3.50mm",
         pos=(80.01, 48.9), rot=0, side="B", sym="Device:C_Polarized",
         pins={"1": "+5V", "2": "GND"}),
    dict(ref="C3", value="100nF", fp="Capacitor_THT:C_Disc_D5.0mm_W2.5mm_P5.00mm",
         pos=(88.27, 41.91), rot=0, side="B", sym="Device:C",
         pins={"1": "+5V", "2": "GND"}),
    dict(ref="J4", value="Display socket (MSP4030/4031 14-pin)", fp="Connector_PinSocket_2.54mm:PinSocket_1x14_P2.54mm_Vertical",
         pos=(DISP_HDR_X, DISP_HDR_PIN1_Y), rot=180, side="F", sym="Connector_Generic:Conn_01x14",
         pins={"1": "+5V", "2": "GND", "3": "TFT_CS", "4": "TFT_RST", "5": "TFT_DC", "6": "MOSI",
               "7": "SCK", "8": "+5V", "9": "TFT_MISO"},
         pinnames=["VCC", "GND", "LCD_CS", "LCD_RST", "LCD_RS", "SDI", "SCK", "LED", "SDO",
                   "CTP_SCL", "CTP_RST", "CTP_SDA", "CTP_INT", "SD_CS"], label_side="right"),
    dict(ref="JP1", value="Display MISO (open)", fp="Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical",
         pos=(47.625, 7.62), rot=-90, side="B", ref_pos=(48.895, 4.9), sym="Connector_Generic:Conn_01x02",
         pins={"1": "MISO", "2": "TFT_MISO"}, pinnames=["MISO", "TFT_SDO"]),
    dict(ref="J5", value="I2C (Wire2) / GPS", fp="Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical",
         pos=(36.195, 7.62), rot=-90, side="B", ref_pos=(40.005, 4.9), sym="Connector_Generic:Conn_01x04",
         pins={"1": "+3V3", "2": "GND", "3": "I2C2_SDA", "4": "I2C2_SCL"},
         pinnames=["3V3", "GND", "SDA2", "SCL2"]),
    dict(ref="J6", value="Panel buttons", fp="Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical",
         pos=(15.875, 7.62), rot=-90, side="B", ref_pos=(19.685, 4.9), sym="Connector_Generic:Conn_01x04",
         pins={"1": "BTN_SELECT", "2": "BTN_UP", "3": "BTN_DOWN", "4": "GND"},
         pinnames=["SEL", "UP", "DOWN", "GND"]),
    dict(ref="J7", value="Panel LEDs", fp="Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical",
         pos=(27.305, 7.62), rot=-90, side="B", ref_pos=(29.845, 4.9), sym="Connector_Generic:Conn_01x03",
         pins={"1": "LED_REC", "2": "LED_GPS", "3": "GND"},
         pinnames=["REC", "GPS", "GND"]),
    dict(ref="R1", value="330", fp="Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal",
         pos=(27.94, 17.78), rot=0, side="B", ref_pos=(31.0, 17.78), sym="Device:R", pins={"1": "LED_REC_DRV", "2": "LED_REC"}),
    dict(ref="R2", value="330", fp="Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal",
         pos=(27.94, 21.59), rot=0, side="B", ref_pos=(31.0, 21.59), sym="Device:R", pins={"1": "LED_GPS_DRV", "2": "LED_GPS"}),
] + [dict(ref=f"H{i+1}", value="M3 (board)", hide_ref=True, fp="MountingHole:MountingHole_3.2mm_M3", pos=xy, rot=0,
          side="F", sym="Mechanical:MountingHole", pins={}) for i, xy in enumerate(CORNER_HOLES)] \
  + [dict(ref=f"H{i+5}", value="M3 (display)", hide_ref=True, fp="MountingHole:MountingHole_3.2mm_M3", pos=xy, rot=0,
          side="F", sym="Mechanical:MountingHole", pins={}) for i, xy in enumerate(DISP_HOLES)]

def all_nets():
    s = set()
    for p in PARTS:
        s.update(p["pins"].values())
    return sorted(s)

def net_width(net):
    for _, (w, nets) in NETCLASS.items():
        if net in nets:
            return w
    return SIGNAL_W
