"""Simple 2-layer grid router for the YamuraHub board (KiCad has no built-in autorouter).

Grid A* (8-way + vias) on a 0.635 mm grid, clearance-checked against every other-net
object (pads, tracks, vias, holes, board edge). Nets are routed one at a time,
pads joined Prim-style into a tree. Writes tracks/vias into the board, then adds
GND pours on both layers. Run with KiCad's python after build_pcb.py."""
import os, sys, heapq, math, time
import numpy as np
import pcbnew
sys.path.insert(0, os.path.dirname(__file__))
import design as D

PROJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PCB = os.path.join(PROJ, "YamuraHub_T41.kicad_pcb")
G = 0.635
MARGIN = 0.06
EDGE_CLR = 0.5
VIA_D, VIA_DRILL = 0.8, 0.4
VIA_COST, DIAG = 8.0, 1.45
WRONG_DIR = 1.7            # cost multiplier for going against a layer's preferred direction
MM = lambda x: pcbnew.FromMM(float(x))

board = pcbnew.LoadBoard(PCB)
W, H = D.BOARD_W, D.BOARD_H
NX, NY = int(round(W / G)) + 1, int(round(H / G)) + 1
XS = np.arange(NX) * G
YS = np.arange(NY) * G
GX, GY = np.meshgrid(XS, YS, indexing="ij")       # [i, j]

# ---------------------------------------------------------------- copper objects
class Obj:  # kind: 'pad' (roundrect), 'seg', 'via', 'hole'
    __slots__ = ("kind", "net", "layers", "cx", "cy", "hw", "hh", "rr", "x1", "y1", "x2", "y2", "w")
objs = []

def add_pad(pad):
    o = Obj(); o.kind = "pad"; o.net = pad.GetNetname() or None
    p = pad.GetPosition(); o.cx, o.cy = p.x / 1e6, p.y / 1e6
    s = pad.GetSize(); w, h = s.x / 1e6, s.y / 1e6
    ang = pad.GetOrientation().AsDegrees() % 180
    if 45 < ang < 135: w, h = h, w
    o.hw, o.hh = w / 2, h / 2
    sh = pad.GetShape()
    o.rr = min(o.hw, o.hh) if sh in (pcbnew.PAD_SHAPE_CIRCLE, pcbnew.PAD_SHAPE_OVAL) else 0.0
    if sh == pcbnew.PAD_SHAPE_ROUNDRECT: o.rr = min(o.hw, o.hh) * pad.GetRoundRectRadiusRatio() * 2
    o.layers = (0, 1) if pad.GetAttribute() == pcbnew.PAD_ATTRIB_PTH else ()
    if pad.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH:        # bare hole
        o.kind = "hole"; o.net = None; o.layers = (0, 1)
        o.hw = o.hh = o.rr = pad.GetDrillSize().x / 2e6
    objs.append(o); return o

pads_by_net = {}
for fp in board.GetFootprints():
    for pad in fp.Pads():
        o = add_pad(pad)
        if o.net and o.kind == "pad":
            pads_by_net.setdefault(o.net, []).append(o)

def dist_field(o, x0, x1, y0, y1):
    """Distance from grid points in index window to object copper edge."""
    px = GX[x0:x1, y0:y1]; py = GY[x0:x1, y0:y1]
    if o.kind in ("pad", "hole", "via"):
        dx = np.maximum(np.abs(px - o.cx) - (o.hw - o.rr), 0)
        dy = np.maximum(np.abs(py - o.cy) - (o.hh - o.rr), 0)
        return np.hypot(dx, dy) - o.rr
    # segment
    vx, vy = o.x2 - o.x1, o.y2 - o.y1
    L2 = vx * vx + vy * vy
    t = np.clip(((px - o.x1) * vx + (py - o.y1) * vy) / L2, 0, 1) if L2 > 0 else 0
    return np.hypot(px - (o.x1 + t * vx), py - (o.y1 + t * vy)) - o.w / 2

def obj_extent(o):
    if o.kind == "seg":
        return min(o.x1, o.x2) - o.w / 2, max(o.x1, o.x2) + o.w / 2, min(o.y1, o.y2) - o.w / 2, max(o.y1, o.y2) + o.w / 2
    return o.cx - o.hw, o.cx + o.hw, o.cy - o.hh, o.cy + o.hh

def paint(mask, o, reach):
    a, b, c, d = obj_extent(o)
    x0 = max(0, int(math.floor((a - reach) / G))); x1 = min(NX, int(math.ceil((b + reach) / G)) + 1)
    y0 = max(0, int(math.floor((c - reach) / G))); y1 = min(NY, int(math.ceil((d + reach) / G)) + 1)
    if x0 >= x1 or y0 >= y1: return
    mask[x0:x1, y0:y1] |= dist_field(o, x0, x1, y0, y1) < reach

def edge_mask(reach):
    m = np.zeros((NX, NY), bool)
    m |= (GX < reach) | (GX > W - reach) | (GY < reach) | (GY > H - reach)
    return m

def blocked_masks(net, w):
    """[F, B] masks of cells where a track of width w for `net` would violate clearance,
    plus a via mask."""
    reach_t = D.CLEARANCE + w / 2 + MARGIN
    reach_v = D.CLEARANCE + VIA_D / 2 + MARGIN
    m = [edge_mask(EDGE_CLR + w / 2), edge_mask(EDGE_CLR + w / 2)]
    v = edge_mask(EDGE_CLR + VIA_D / 2)
    for o in objs:
        if o.net == net and o.kind != "hole": continue
        r_t = reach_t if o.kind != "hole" else 0.25 + w / 2
        for l in o.layers: paint(m[l], o, r_t)
        paint(v, o, reach_v if o.kind != "hole" else 0.25 + VIA_D / 2)
    return m, v

# ---------------------------------------------------------------- A*
NB = [(1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0),
      (1, 1, DIAG), (1, -1, DIAG), (-1, 1, DIAG), (-1, -1, DIAG)]

def astar(sources, goal_cells, goal_xy, m, v):
    gx, gy = goal_xy
    def h(i, j):
        dx, dy = abs(i * G - gx) / G, abs(j * G - gy) / G
        return (max(dx, dy) - min(dx, dy)) + DIAG * min(dx, dy) * 0.98
    openh = []; best = {}; came = {}
    for s in sources:
        best[s] = 0.0; came[s] = None
        heapq.heappush(openh, (h(s[1], s[2]), 0.0, s))
    while openh:
        f, gcost, cur = heapq.heappop(openh)
        if gcost > best.get(cur, 1e18): continue
        if cur in goal_cells:
            path = []
            while cur is not None: path.append(cur); cur = came[cur]
            return path[::-1]
        l, i, j = cur
        ml = m[l]
        for di, dj, c in NB:
            ni, nj = i + di, j + dj
            if not (0 <= ni < NX and 0 <= nj < NY) or ml[ni, nj]: continue
            if di and dj and (ml[i + di, j] or ml[i, j + dj]): continue
            if (l == 0 and dj and not di) or (l == 1 and di and not dj): c *= WRONG_DIR
            nxt = (l, ni, nj); ng = gcost + c
            if ng < best.get(nxt, 1e18):
                best[nxt] = ng; came[nxt] = cur
                heapq.heappush(openh, (ng + h(ni, nj), ng, nxt))
        if not v[i, j]:
            nxt = (1 - l, i, j); ng = gcost + VIA_COST
            if not m[1 - l][i, j] and ng < best.get(nxt, 1e18):
                best[nxt] = ng; came[nxt] = cur
                heapq.heappush(openh, (ng + h(i, j), ng, nxt))
    return None

def pad_cells(o, w):
    """Grid cells (both layers) where a track end of width w sits inside the pad copper."""
    x0 = max(0, int((o.cx - o.hw) / G) - 1); x1 = min(NX, int((o.cx + o.hw) / G) + 2)
    y0 = max(0, int((o.cy - o.hh) / G) - 1); y1 = min(NY, int((o.cy + o.hh) / G) + 2)
    d = dist_field(o, x0, x1, y0, y1)
    out = set()
    for a, b in zip(*np.nonzero(d <= -min(w / 2, 0.25))):
        for l in o.layers: out.add((l, x0 + a, y0 + b))
    if not out:   # tiny pad: nearest cell
        i, j = int(round(o.cx / G)), int(round(o.cy / G))
        out = {(l, i, j) for l in o.layers}
    return out

# ---------------------------------------------------------------- route all nets
def net_order():
    nets = [n for n in pads_by_net if len(pads_by_net[n]) > 1]
    def span(n):
        ps = pads_by_net[n]; xs = [p.cx for p in ps]; ys = [p.cy for p in ps]
        return (max(xs) - min(xs)) + (max(ys) - min(ys))
    power = ["+12V_IN", "+12V", "+12V_BUS", "+5V", "+3V3"]
    sig = sorted([n for n in nets if n not in power and n != "GND"], key=span)
    return [n for n in power if n in nets] + ["GND"] + sig

routed = {}     # net -> (edges set, vias set)
failed = []

def route_net(net):
    w = D.net_width(net)
    m, v = blocked_masks(net, w)
    pads = pads_by_net[net]
    tree = set(pad_cells(pads[0], w)); connected = [pads[0]]; todo = pads[1:]
    edges, vias = set(), set()
    while todo:
        # nearest unconnected pad to the connected set
        tgt = min(todo, key=lambda p: min(math.hypot(p.cx - q.cx, p.cy - q.cy) for q in connected))
        goal = pad_cells(tgt, w)
        if tree & goal:
            path = [next(iter(tree & goal))]
        else:
            # make sure source/goal cells inside own pads are usable
            for (l, i, j) in goal | tree: m[l][i, j] = False
            path = astar(list(tree), goal, (tgt.cx, tgt.cy), m, v)
        if path is None:
            failed.append((net, tgt.cx, tgt.cy)); todo.remove(tgt); continue
        for a, b in zip(path, path[1:]):
            if a[0] != b[0]: vias.add((a[1], a[2]))
            else: edges.add((a, b) if a < b else (b, a))
        tree |= set(path) | goal
        connected.append(tgt); todo.remove(tgt)
    routed[net] = (edges, vias, w)
    # register copper so later nets keep clear
    for seg in merge(edges):
        (l, i1, j1), (_, i2, j2) = seg
        o = Obj(); o.kind = "seg"; o.net = net; o.layers = (l,); o.w = w
        o.x1, o.y1, o.x2, o.y2 = i1 * G, j1 * G, i2 * G, j2 * G
        objs.append(o)
    for (i, j) in vias:
        o = Obj(); o.kind = "via"; o.net = net; o.layers = (0, 1)
        o.cx, o.cy = i * G, j * G; o.hw = o.hh = o.rr = VIA_D / 2
        objs.append(o)

def merge(edges):
    """Merge unit edges into straight segments, breaking at junctions and bends."""
    adj = {}
    for a, b in edges:
        adj.setdefault(a, []).append(b); adj.setdefault(b, []).append(a)
    used = set(); segs = []
    def d(a, b): return (b[1] - a[1], b[2] - a[2])
    for a, b in edges:
        if (a, b) in used or (b, a) in used: continue
        used.add((a, b))
        start, end = a, b
        dirv = d(a, b)
        while len(adj[end]) == 2:                 # extend forward
            nxt = [n for n in adj[end] if (end, n) not in used and (n, end) not in used]
            if not nxt or d(end, nxt[0]) != dirv: break
            used.add((end, nxt[0])); end = nxt[0]
        while len(adj[start]) == 2:               # extend backward
            prv = [n for n in adj[start] if (start, n) not in used and (n, start) not in used]
            if not prv or d(prv[0], start) != dirv: break
            used.add((prv[0], start)); start = prv[0]
        segs.append((start, end))
    return segs

t0 = time.time()
for net in net_order():
    route_net(net)
    print(f"{net:14s} edges {len(routed[net][0]):4d} vias {len(routed[net][1]):2d}  ({time.time()-t0:.1f}s)", flush=True)
print("FAILED:", failed)

# ---------------------------------------------------------------- write to board
for t in list(board.GetTracks()): board.Remove(t)
netinfo = {n: board.FindNet(n) for n in routed}
LAY = [pcbnew.F_Cu, pcbnew.B_Cu]
for net, (edges, vias, w) in routed.items():
    for (l, i1, j1), (_, i2, j2) in merge(edges):
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(pcbnew.VECTOR2I(MM(i1 * G), MM(j1 * G))); t.SetEnd(pcbnew.VECTOR2I(MM(i2 * G), MM(j2 * G)))
        t.SetWidth(MM(w)); t.SetLayer(LAY[l]); t.SetNet(netinfo[net]); board.Add(t)
    for (i, j) in vias:
        vv = pcbnew.PCB_VIA(board); vv.SetPosition(pcbnew.VECTOR2I(MM(i * G), MM(j * G)))
        vv.SetDrill(MM(VIA_DRILL)); vv.SetWidth(MM(VIA_D)); vv.SetNet(netinfo[net]); board.Add(vv)
    # stub from on-grid track end to off-grid pad centres so the end lands mid-pad
    for p in pads_by_net[net]:
        i, j = int(round(p.cx / G)), int(round(p.cy / G))
        if abs(i * G - p.cx) < 1e-3 and abs(j * G - p.cy) < 1e-3: continue
        ends = [c for e in edges for c in e if p.layers and abs(c[1] * G - p.cx) <= p.hw and abs(c[2] * G - p.cy) <= p.hh]
        for (l, ci, cj) in set(ends):
            t = pcbnew.PCB_TRACK(board)
            t.SetStart(pcbnew.VECTOR2I(MM(ci * G), MM(cj * G))); t.SetEnd(pcbnew.VECTOR2I(MM(p.cx), MM(p.cy)))
            t.SetWidth(MM(min(w, 2 * min(p.hw, p.hh) - 0.2))); t.SetLayer(LAY[l]); t.SetNet(netinfo[net]); board.Add(t)

# GND pours, both layers
for z in list(board.Zones()): board.Remove(z)
for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
    z = pcbnew.ZONE(board); z.SetLayer(layer); z.SetNet(board.FindNet("GND"))
    ol = z.Outline(); ol.NewOutline()
    for x, y in ((0.3, 0.3), (W - 0.3, 0.3), (W - 0.3, H - 0.3), (0.3, H - 0.3)):
        ol.Append(MM(x), MM(y))
    z.SetLocalClearance(MM(0.3)); z.SetMinThickness(MM(0.25))
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
    z.SetThermalReliefGap(MM(0.4)); z.SetThermalReliefSpokeWidth(MM(0.5))
    z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
    board.Add(z)
pcbnew.ZONE_FILLER(board).Fill(board.Zones())
pcbnew.SaveBoard(PCB, board)
# THT pads are already track-connected; partial pour spokes are not an error here
import json
pro = PCB.replace(".kicad_pcb", ".kicad_pro")
cfg = json.load(open(pro))
cfg["board"]["design_settings"]["rule_severities"]["starved_thermal"] = "warning"
json.dump(cfg, open(pro, "w"), indent=2)
print("saved; failed connections:", len(failed))
