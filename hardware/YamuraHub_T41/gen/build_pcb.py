"""Build YamuraHub_T41.kicad_pcb from design.py: custom Teensy footprint, placement,
nets, net classes, outline, silkscreen. Routing is done afterwards by route.py.
Run with KiCad's bundled python."""
import os, sys
import pcbnew
sys.path.insert(0, os.path.dirname(__file__))
import design as D

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.dirname(HERE)
LIBDIR = os.path.join(PROJ, "YamuraHub.pretty")
PCB = os.path.join(PROJ, "YamuraHub_T41.kicad_pcb")
MM = pcbnew.FromMM

def V(x, y): return pcbnew.VECTOR2I(MM(x), MM(y))

def make_teensy41():
    fp = pcbnew.FOOTPRINT(None)
    fp.SetFPID(pcbnew.LIB_ID("YamuraHub", "Teensy41"))
    fp.SetAttributes(pcbnew.FP_THROUGH_HOLE)
    fp.Reference().SetText("REF**"); fp.Value().SetText("Teensy41")
    for num, name in D.TEENSY_PINS.items():
        n = int(num)
        if n <= 24: x, y = -7.62, -29.21 + (n - 1) * 2.54
        else:       x, y = 7.62, 29.21 - (n - 25) * 2.54
        pad = pcbnew.PAD(fp)
        pad.SetNumber(num); pad.SetAttribute(pcbnew.PAD_ATTRIB_PTH)
        pad.SetShape(pcbnew.PAD_SHAPE_RECT if n == 1 else pcbnew.PAD_SHAPE_CIRCLE)
        pad.SetSize(V(1.7, 1.7)); pad.SetDrillSize(V(1.0, 1.0))
        pad.SetLayerSet(pad.PTHMask()); pad.SetPosition(V(x, y))
        fp.Add(pad)
    def rect(layer, x0, y0, x1, y1, w=0.12):
        for a, b in (((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)), ((x1, y1), (x0, y1)), ((x0, y1), (x0, y0))):
            s = pcbnew.PCB_SHAPE(fp); s.SetShape(pcbnew.SHAPE_T_SEGMENT)
            s.SetStart(V(*a)); s.SetEnd(V(*b)); s.SetLayer(layer); s.SetWidth(MM(w)); fp.Add(s)
    rect(pcbnew.F_SilkS, -8.89, -30.48, 8.89, 30.48)
    rect(pcbnew.F_Fab, -8.89, -30.48, 8.89, 30.48, 0.1)
    rect(pcbnew.F_CrtYd, -9.2, -31.0, 9.2, 31.0, 0.05)
    rect(pcbnew.F_SilkS, -2.6, -31.2, 2.6, -28.0)                 # USB connector
    rect(pcbnew.F_SilkS, -6.0, 26.0, 6.0, 30.48)                  # SD card slot
    for txt, y in (("USB", -26.5), ("SD", 24.5), ("TEENSY 4.1", 0)):
        t = pcbnew.PCB_TEXT(fp); t.SetText(txt); t.SetLayer(pcbnew.F_SilkS)
        sz = 1.0 if txt.startswith("TEENSY") else 1.2
        t.SetPosition(V(0, y)); t.SetTextSize(V(sz, sz)); t.SetTextThickness(MM(0.15))
        if txt.startswith("TEENSY"): t.SetTextAngle(pcbnew.EDA_ANGLE(90, pcbnew.DEGREES_T))
        fp.Add(t)
    # pin names on silkscreen, inside the rows
    for num, name in D.TEENSY_PINS.items():
        n = int(num)
        if n <= 24: x, y, j = -5.9, -29.21 + (n - 1) * 2.54, pcbnew.GR_TEXT_H_ALIGN_LEFT
        else:       x, y, j = 5.9, 29.21 - (n - 25) * 2.54, pcbnew.GR_TEXT_H_ALIGN_RIGHT
        if -27.5 < y < -23 or 22 < y: continue          # keep USB / SD labels clear
        t = pcbnew.PCB_TEXT(fp); t.SetText(name); t.SetLayer(pcbnew.F_SilkS)
        t.SetPosition(V(x, y)); t.SetTextSize(V(0.8, 0.8)); t.SetTextThickness(MM(0.12))
        t.SetHorizJustify(j); fp.Add(t)
    os.makedirs(LIBDIR, exist_ok=True)
    io = pcbnew.PCB_IO_MGR.FindPlugin(pcbnew.PCB_IO_MGR.KICAD_SEXP)
    io.FootprintSave(LIBDIR, fp)

def load_fp(spec):
    lib, name = spec.split(":")
    path = LIBDIR if lib == "YamuraHub" else D.KLIB + lib + ".pretty"
    fp = pcbnew.FootprintLoad(path, name)
    if fp is None: raise RuntimeError("missing footprint " + spec)
    fp.SetFPID(pcbnew.LIB_ID(lib, name))
    return fp

def text(board, s, x, y, size=1.2, layer=pcbnew.F_SilkS, angle=0, just=pcbnew.GR_TEXT_H_ALIGN_CENTER):
    t = pcbnew.PCB_TEXT(board); t.SetText(s); t.SetLayer(layer)
    t.SetPosition(V(x, y)); t.SetTextSize(V(size, size)); t.SetTextThickness(MM(size * 0.15))
    t.SetHorizJustify(just)
    if angle: t.SetTextAngle(pcbnew.EDA_ANGLE(angle, pcbnew.DEGREES_T))
    if layer == pcbnew.B_SilkS: t.SetMirrored(True)
    board.Add(t)

def main():
    make_teensy41()
    board = pcbnew.BOARD()
    ds = board.GetDesignSettings()
    ds.SetCopperLayerCount(2)
    # net classes
    ns = ds.m_NetSettings
    dflt = ns.GetDefaultNetclass()
    dflt.SetClearance(MM(D.CLEARANCE)); dflt.SetTrackWidth(MM(D.SIGNAL_W))
    dflt.SetViaDiameter(MM(0.8)); dflt.SetViaDrill(MM(0.4))
    # pad nets come from the schematic netlist (kicad-cli sch export netlist), so the
    # board always matches the schematic; cross-check against design.py
    import re
    nl = open(os.path.join(PROJ, "gen", "out", "YamuraHub_T41.net"), encoding="utf-8").read()
    padnet = {}
    for blk in re.split(r'\(net\s*\(code', nl[nl.index("(nets"):])[1:]:
        name = re.search(r'\(name "([^"]*)"\)', blk).group(1)
        for ref, pin in re.findall(r'\(node\s*\(ref "([^"]+)"\)\s*\(pin "([^"]+)"\)', blk):
            padnet[(ref, pin)] = name
    for p in D.PARTS:
        for pin, n in p["pins"].items():
            assert padnet.get((p["ref"], pin)) == n, (p["ref"], pin, n, padnet.get((p["ref"], pin)))
    nets = {}
    for n in sorted(set(padnet.values())):
        ni = pcbnew.NETINFO_ITEM(board, n); board.Add(ni); nets[n] = ni
    for cname, (w, members) in D.NETCLASS.items():
        nc = pcbnew.NETCLASS(cname)
        nc.SetClearance(MM(D.CLEARANCE)); nc.SetTrackWidth(MM(w))
        nc.SetViaDiameter(MM(max(0.8, w + 0.3))); nc.SetViaDrill(MM(0.4 if w < 0.8 else 0.6))
        ns.SetNetclass(cname, nc)
        for m in members:
            ns.SetNetclassPatternAssignment(m, cname)
    # outline
    W, H = D.BOARD_W, D.BOARD_H
    for a, b in (((0, 0), (W, 0)), ((W, 0), (W, H)), ((W, H), (0, H)), ((0, H), (0, 0))):
        s = pcbnew.PCB_SHAPE(board); s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(V(*a)); s.SetEnd(V(*b)); s.SetLayer(pcbnew.Edge_Cuts); s.SetWidth(MM(0.1)); board.Add(s)
    # footprints
    for p in D.PARTS:
        fp = load_fp(p["fp"])
        fp.SetReference(p["ref"]); fp.SetValue(p["value"])
        fp.SetPosition(V(*p["pos"]))
        if p["rot"]: fp.SetOrientationDegrees(p["rot"])
        board.Add(fp)
        for pad in fp.Pads():
            n = padnet.get((p["ref"], pad.GetNumber()))
            if n: pad.SetNet(nets[n])
        fp.Value().SetVisible(False)
        fp.Reference().SetTextSize(V(1.0, 1.0)); fp.Reference().SetTextThickness(MM(0.15))
        if p.get("hide_ref"): fp.Reference().SetVisible(False)
        if "ref_pos" in p:
            fp.Reference().SetPosition(V(*p["ref_pos"])); fp.Reference().SetTextAngle(pcbnew.EDA_ANGLE(0, pcbnew.DEGREES_T))
        for s_, x_, y_ in p.get("silk", []):
            text(board, s_, x_, y_, 0.8)
    # silkscreen pin labels for modules / headers
    def label_pads(p):
        fp = board.FindFootprintByReference(p["ref"])
        pads = [pd for pd in fp.Pads() if pd.GetNumber().isdigit()]
        xs = {round(pd.GetPosition().x / 1e6, 2) for pd in pads}
        horiz = len(xs) > 1
        side = p.get("label_side", "below")
        for pd in pads:
            i = int(pd.GetNumber()) - 1
            if i >= len(p["pinnames"]): continue
            x, y = pd.GetPosition().x / 1e6, pd.GetPosition().y / 1e6
            r = max(pd.GetSize().x, pd.GetSize().y) / 2e6 + 0.75
            if horiz and side == "below":   # text reads upward, ends just below the pad
                text(board, p["pinnames"][i], x, y + r, 0.8, angle=90, just=pcbnew.GR_TEXT_H_ALIGN_RIGHT)
            elif horiz:                       # above: starts just above the pad
                text(board, p["pinnames"][i], x, y - r, 0.8, angle=90, just=pcbnew.GR_TEXT_H_ALIGN_LEFT)
            else:                             # vertical header: text to the left
                text(board, p["pinnames"][i], x - r, y, 0.8, just=pcbnew.GR_TEXT_H_ALIGN_RIGHT)
    for p in D.PARTS:
        if "pinnames" in p and p["ref"] != "A1":
            label_pads(p)
    text(board, D.TITLE + "  rev " + D.REV, 75, 50.8, 1.5)
    text(board, "AirLift module sits here", 75.5, 30, 1.0, layer=pcbnew.F_Fab)
    text(board, "CAN board (A3) sits here", 58, 56, 1.0, layer=pcbnew.F_Fab)
    text(board, "Wire A3 CANH/CANL to J2", 34.3, 48.0, 0.8)
    text(board, "JP1 open: display SDO not on MISO", 31.5, 13.97, 0.8, just=pcbnew.GR_TEXT_H_ALIGN_LEFT)
    text(board, "Cut Teensy VUSB-VIN pad", 105, 10.0, 0.8)
    pcbnew.SaveBoard(PCB, board)
    print("saved", PCB)

if __name__ == "__main__":
    main()
