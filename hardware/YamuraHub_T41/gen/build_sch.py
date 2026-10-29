"""Generate YamuraHub_T41.kicad_sch (+ project symbol library) from design.py.

Every pin gets a net label at its connection point (no wires), so the schematic is
a readable, ERC-able netlist that matches the board exactly. Unused pins get
no-connect flags."""
import os, sys, re, uuid, datetime
sys.path.insert(0, os.path.dirname(__file__))
import design as D

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.dirname(HERE)
KSYM = "C:/Program Files/KiCad/10.0/share/kicad/symbols/"
SCH = os.path.join(PROJ, "YamuraHub_T41.kicad_sch")
LOCAL_LIB = os.path.join(PROJ, "YamuraHub.kicad_sym")
ROOT_UUID = str(uuid.uuid5(uuid.NAMESPACE_URL, "yamurahub-t41-root"))
_uid_n = [0]
def uid():
    _uid_n[0] += 1
    return str(uuid.uuid5(uuid.NAMESPACE_URL, f"yamurahub-t41-{_uid_n[0]}"))

# ------------------------------------------------------------------ s-expressions
class Sym(str): pass                    # bare atom
def parse(text):
    tok = re.compile(r'\s*(\(|\)|"(?:\\.|[^"\\])*"|[^\s()"]+)')
    pos, stack, cur = 0, [], []
    while True:
        m = tok.match(text, pos)
        if not m: break
        t = m.group(1); pos = m.end()
        if t == "(": stack.append(cur); cur = []
        elif t == ")": done = cur; cur = stack.pop(); cur.append(done)
        elif t.startswith('"'): cur.append(t[1:-1].replace('\\"', '"').replace("\\\\", "\\"))
        else: cur.append(Sym(t))
    return cur[0]
def dump(e, ind=0):
    if isinstance(e, list):
        if not e: return "()"
        simple = all(not isinstance(x, list) for x in e)
        if simple: return "(" + " ".join(dump(x) for x in e) + ")"
        out = "(" + dump(e[0])
        for x in e[1:]: out += "\n" + "\t" * (ind + 1) + dump(x, ind + 1)
        return out + "\n" + "\t" * ind + ")"
    if isinstance(e, Sym): return str(e)
    if isinstance(e, (int, float)): return fmt(e)
    return '"' + str(e).replace("\\", "\\\\").replace('"', '\\"') + '"'
def fmt(v):
    s = f"{v:.4f}".rstrip("0").rstrip(".")
    return "0" if s in ("-0", "") else s
def find(e, key):
    return [x for x in e if isinstance(x, list) and x and x[0] == key]

_libs = {}
def libfile(name):
    if name not in _libs:
        path = LOCAL_LIB if name == "local" else KSYM + name + ".kicad_sym"
        _libs[name] = parse(open(path, encoding="utf-8").read())
    return _libs[name]
def lib_symbol(libid):
    """Flattened library symbol (handles 'extends'), renamed for embedding."""
    lib, name = libid.split(":")
    root = libfile(lib)
    syms = {x[1]: x for x in find(root, "symbol")}
    s = syms[name]
    ext = find(s, "extends")
    if ext:
        parent = lib_symbol(lib + ":" + ext[0][1])
        pname = ext[0][1]
        body = [x for x in parent[2:] if not (isinstance(x, list) and x[0] == "property")]
        body = [[x[0], x[1].replace(pname + "_", name + "_", 1)] + x[2:] if isinstance(x, list) and x[0] == "symbol" else x for x in body]
        props = find(s, "property")
        s = [Sym("symbol"), name] + props + body
    s = [Sym("symbol"), ("YamuraHub" if lib == "local" else lib) + ":" + name] + s[2:]
    return s
def pins_of(symexpr):
    out = []
    for sub in find(symexpr, "symbol"):
        for p in find(sub, "pin"):
            at = find(p, "at")[0]; num = find(p, "number")[0][1]; nm = find(p, "name")[0][1]
            out.append(dict(num=num, name=nm, x=float(at[1]), y=float(at[2]), ang=int(float(at[3])) if len(at) > 3 else 0, type=str(p[1])))
    return out

# ------------------------------------------------------------------ local symbols
def pin_sx(ptype, x, y, ang, name, num, length=2.54):
    return (f'(pin {ptype} line (at {fmt(x)} {fmt(y)} {ang}) (length {fmt(length)}) '
            f'(name "{name}" (effects (font (size 1.27 1.27)))) (number "{num}" (effects (font (size 1.27 1.27)))))')
def local_symbol(name, ref, value, pins, rect, desc, fp=""):
    x0, y0, x1, y1 = rect
    body = (f'(symbol "{name}" (exclude_from_sim no) (in_bom yes) (on_board yes)\n'
            f' (property "Reference" "{ref}" (at {fmt(x0)} {fmt(y0 + 1.27)} 0) (effects (font (size 1.27 1.27)) (justify left)))\n'
            f' (property "Value" "{value}" (at {fmt(x0)} {fmt(y1 - 1.27)} 0) (effects (font (size 1.27 1.27)) (justify left)))\n'
            f' (property "Footprint" "{fp}" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))\n'
            f' (property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))\n'
            f' (property "Description" "{desc}" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))\n'
            f' (symbol "{name}_0_1" (rectangle (start {fmt(x0)} {fmt(y0)}) (end {fmt(x1)} {fmt(y1)}) (stroke (width 0.254) (type default)) (fill (type background))))\n'
            f' (symbol "{name}_1_1"\n  ' + "\n  ".join(pins) + "))")
    return body
def build_local_lib():
    syms = []
    # Teensy 4.1: physical pin numbering 1..48, left/right as on the board
    tp = []
    def ttype(n, name):
        if name == "GND" or name == "VIN": return "power_in"
        if name == "3V3": return "power_out" if n == "46" else "passive"
        return "bidirectional"
    for i in range(24):
        n = str(i + 1); nm = D.TEENSY_PINS[n]; y = (12 - i) * 2.54
        tp.append(pin_sx(ttype(n, nm), -15.24, y, 0, nm if nm in ("GND", "3V3") else "D" + nm, n))
    for k in range(24):
        n = str(48 - k); nm = D.TEENSY_PINS[n]; y = (12 - k) * 2.54
        tp.append(pin_sx(ttype(n, nm), 15.24, y, 180, nm if nm in ("GND", "3V3", "VIN") else "D" + nm, n))
    syms.append(local_symbol("Teensy41", "A", "Teensy 4.1", tp, (-12.7, 33.02, 12.7, -30.48),
                             "PJRC Teensy 4.1 (48 edge pins, built-in SDIO microSD)", "YamuraHub:Teensy41"))
    ap = []
    types = ["power_in", "power_out", "power_in", "input", "output", "input", "input", "output", "input", "bidirectional", "input", "output"]
    for i, nm in enumerate(["VIN", "3Vo", "GND", "SCK", "MISO", "MOSI", "CS", "BUSY", "~{RST}", "GP0", "RXI", "TXO"]):
        ap.append(pin_sx(types[i], -10.16, (6 - i) * 2.54, 0, nm, str(i + 1)))
    syms.append(local_symbol("AirLift", "A", "Adafruit AirLift", ap, (-7.62, 17.78, 7.62, -15.24),
                             "Adafruit AirLift ESP32 WiFi co-processor breakout (4201), 12-pin header"))
    cp = []
    for i, (nm, ty) in enumerate([("3V3", "power_in"), ("GND", "power_in"), ("CTX", "input"), ("CRX", "output")]):
        cp.append(pin_sx(ty, -10.16, (2 - i) * 2.54, 0, nm, str(i + 1)))
    syms.append(local_symbol("CANBoard", "A", "SN65HVD230 CAN board", cp, (-7.62, 7.62, 7.62, -5.08),
                             "Waveshare SN65HVD230 CAN board, logic header (CANH/CANL wired to J2); has on-board 120R terminator"))
    txt = "(kicad_symbol_lib (version 20241209) (generator \"yamurahub_gen\") (generator_version \"10.0\")\n" + "\n".join(syms) + "\n)\n"
    open(LOCAL_LIB, "w", encoding="utf-8").write(txt)
    open(os.path.join(PROJ, "sym-lib-table"), "w").write(
        '(sym_lib_table\n  (version 7)\n  (lib (name "YamuraHub")(type "KiCad")(uri "${KIPRJMOD}/YamuraHub.kicad_sym")(options "")(descr "YamuraLog Hub project symbols"))\n)\n')

# ------------------------------------------------------------------ placement on the sheet (A3, mm)
SCH_POS = {
    "A1": (200.66, 139.7), "A2": (309.88, 63.5), "A3": (309.88, 116.84), "J2": (309.88, 142.24),
    "J3": (365.76, 132.08), "J8": (365.76, 177.8), "J4": (66.04, 63.5), "JP1": (66.04, 116.84),
    "J1": (30.48, 208.28), "D1": (63.5, 205.74), "D4": (81.28, 226.06), "F1": (101.6, 190.5),
    "C1": (101.6, 226.06), "U1": (132.08, 205.74), "C2": (157.48, 226.06), "C3": (177.8, 226.06),
    "J5": (309.88, 185.42), "J6": (309.88, 210.82), "J7": (309.88, 236.22),
    "R1": (226.06, 210.82), "R2": (254.0, 210.82),
    "H1": (350.52, 251.46), "H2": (360.68, 251.46), "H3": (370.84, 251.46), "H4": (381.0, 251.46),
    "H5": (350.52, 264.16), "H6": (360.68, 264.16), "H7": (370.84, 264.16), "H8": (381.0, 264.16),
}
LABEL_ANG = {0: 180, 180: 0, 90: 270, 270: 90}

def prop(name, val, x, y, hide=False, ang=0):
    p = [Sym("property"), name, val, [Sym("at"), x, y, ang],
         [Sym("effects"), [Sym("font"), [Sym("size"), 1.27, 1.27]]]]
    if hide: p.insert(4, [Sym("hide"), Sym("yes")])
    return p

def main():
    build_local_lib()
    _libs.pop("local", None)
    lib_syms, items, used = {}, [], set()
    def place(libid, ref, value, fp, x, y, pinnets, props_extra=(), on_board=True, in_bom=True):
        if libid not in lib_syms: lib_syms[libid] = lib_symbol(libid)
        sexpr = lib_syms[libid]
        pins = pins_of(sexpr)
        embed = sexpr[1]
        s = [Sym("symbol"), [Sym("lib_id"), embed], [Sym("at"), x, y, 0], [Sym("unit"), 1],
             [Sym("exclude_from_sim"), Sym("no")], [Sym("in_bom"), Sym("yes" if in_bom else "no")],
             [Sym("on_board"), Sym("yes" if on_board else "no")], [Sym("dnp"), Sym("no")],
             [Sym("uuid"), uid()],
             prop("Reference", ref, x, y - 3.81 - (33 if ref == "A1" else 0)),
             prop("Value", value, x, y + 3.81 + (33 if ref == "A1" else 0)),
             prop("Footprint", fp, x, y, hide=True), prop("Datasheet", "", x, y, hide=True)]
        s += list(props_extra)
        seen = set()
        for p in pins:
            if p["num"] in seen: continue
            seen.add(p["num"]); s.append([Sym("pin"), p["num"], [Sym("uuid"), uid()]])
        s.append([Sym("instances"), [Sym("project"), "YamuraHub_T41",
                  [Sym("path"), "/" + ROOT_UUID, [Sym("reference"), ref], [Sym("unit"), 1]]]])
        items.append(s)
        done = set()
        for p in pins:
            px, py = x + p["x"], y - p["y"]
            key = (round(px, 3), round(py, 3))
            if key in done: continue
            done.add(key)
            net = pinnets.get(p["num"])
            if net:
                a = LABEL_ANG[p["ang"] % 360]
                just = "left bottom" if a in (0, 90) else "right bottom"
                items.append([Sym("global_label"), net, [Sym("shape"), Sym("passive")], [Sym("at"), px, py, a],
                              [Sym("fields_autoplaced"), Sym("yes")],
                              [Sym("effects"), [Sym("font"), [Sym("size"), 1.27, 1.27]], [Sym("justify"), Sym("left" if a in (0, 90) else "right")]],
                              [Sym("uuid"), uid()],
                              [Sym("property"), "Intersheetrefs", "${INTERSHEET_REFS}", [Sym("at"), px, py, 0],
                               [Sym("effects"), [Sym("font"), [Sym("size"), 1.27, 1.27]], [Sym("hide"), Sym("yes")]]]])
            else:
                items.append([Sym("no_connect"), [Sym("at"), px, py], [Sym("uuid"), uid()]])
    for p in D.PARTS:
        x, y = SCH_POS[p["ref"]]
        libid = p["sym"].replace("local:", "local:")
        place(libid, p["ref"], p["value"], p["fp"], x, y, p["pins"], in_bom=not p["ref"].startswith("H"))
    # power flags on externally-sourced nets
    for i, net in enumerate(["+12V_IN", "+12V", "GND"]):
        place("power:PWR_FLAG", f"#FLG0{i+1}", "PWR_FLAG", "", 30.48 + 20.32 * i, 30.48, {"1": net}, in_bom=False, on_board=False)
    # notes
    notes = [
        (25.4, 274.32, "Pin map from YamuraHubV6.1 src/main.h. SD card: Teensy 4.1 built-in slot (SD.begin(BUILTIN_SDCARD)); display SD_CS not connected."),
        (25.4, 279.4, "JP1 open by default: ST7796 SDO does not tri-state and would contend with the AirLift on MISO. A3 (Waveshare) has a fixed 120R terminator: remove it if both DB9s are used (hub mid-bus)."),
        (25.4, 284.48, "Teensy 4.1: cut the VUSB-VIN pad so USB and the 12V supply are not tied together. CAN = FlexCAN_T4FD<CAN3> on pins 30 (CRX3) / 31 (CTX3)."),
    ]
    for x, y, t in notes:
        items.append([Sym("text"), t, [Sym("exclude_from_sim"), Sym("no")], [Sym("at"), x, y, 0],
                      [Sym("effects"), [Sym("font"), [Sym("size"), 1.6, 1.6]], [Sym("justify"), Sym("left"), Sym("bottom")]],
                      [Sym("uuid"), uid()]])
    doc = [Sym("kicad_sch"), [Sym("version"), Sym("20250610")], [Sym("generator"), "eeschema"],
           [Sym("generator_version"), "10.0"], [Sym("uuid"), ROOT_UUID], [Sym("paper"), "A3"],
           [Sym("title_block"), [Sym("title"), D.TITLE], [Sym("date"), datetime.date.today().isoformat()],
            [Sym("rev"), D.REV], [Sym("company"), "Yamura Motors LLC"],
            [Sym("comment"), 1, "2-layer through-hole carrier: Teensy 4.1, AirLift, SN65HVD230 CAN board, 4in SPI display"]],
           [Sym("lib_symbols")] + list(lib_syms.values())] + items + \
          [[Sym("sheet_instances"), [Sym("path"), "/", [Sym("page"), "1"]]], [Sym("embedded_fonts"), Sym("no")]]
    open(SCH, "w", encoding="utf-8").write(dump(doc) + "\n")
    print("saved", SCH)

if __name__ == "__main__":
    main()
