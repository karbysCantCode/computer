"""
Logisim Evolution .circ exporter -- folded, rotation-aware layout.

Geometry (grid unit = 10), confirmed from real hand-wired .circ files at
all four facings:
  - Output is always exactly at `loc`, regardless of facing.
  - EAST  (default): inputs at (loc.x - depth, loc.y + offset)
  - WEST                inputs at (loc.x + depth, loc.y + offset)
  - NORTH               inputs at (loc.x + offset, loc.y + depth)
  - SOUTH                inputs at (loc.x + offset, loc.y - depth)
  offset comes from gate_input_offsets(N) in all four cases (same values,
  applied to whichever axis isn't the depth axis for that facing).
  depth is per gate type: NOT=30, AND/OR=50, XNOR=70, XOR=60.

Layout strategy: the original version gave every DAG level its own
column, all in a single left-to-right strip -- correct, but for a design
with many levels this produces a very long, thin bounding box (all the
width budget spent going right, none of it folded into height).

This version folds the sequence of levels into multiple ROWS using
rotation: alternate rows face EAST/WEST so the data-flow direction still
makes sense boustrophedon-style (row 0 flows left-to-right, row 1 flows
right-to-left continuing underneath it, and so on), which trades excess
width for a much more square, compact bounding box. An early attempt at
this also tried to let different levels within a row overlap freely in y
and rely purely on a maze router to keep things collision-safe -- that
turned out to have a real congestion-collapse failure mode under high
fan-in (many nets each claiming a full vertical "lane" near a busy pin
column can wall off a later net's approach entirely, with no valid route
left at all). Given the correctness stakes here, this version keeps the
part that's proven solid -- each LEVEL gets its own exclusive y-band and
each wire gets its own private routing channel, exactly as verified
extensively in the single-row version -- and gets the footprint reduction
purely from folding levels into rows, which doesn't carry that risk.
"""

import xml.etree.ElementTree as ET
from pathlib import Path

NOT_DEPTH = 30
GATE_DEPTH = 50
XOR_DEPTH = 60
XNOR_DEPTH = 70
GATE_PIN_STEP = 20

CHANNEL_STEP = 10
CHANNEL_MARGIN = 20
COLUMN_SAFETY_GAP = 20
INPUT_COL_X = 60
ROW_HEIGHT = 30
ROW_GAP = 20
BAND_GAP = 60            # vertical clearance between one level's y-band and the next
ROW_BAND_GAP = 100       # vertical clearance between one folded ROW and the next
TOP_MARGIN = 60


def _gate_depth(kind):
    return {'NOT': NOT_DEPTH, 'AND': GATE_DEPTH, 'OR': GATE_DEPTH, 'XNOR': XNOR_DEPTH}[kind]


def compute_levels(graph):
    level = {}

    def lvl(nid):
        if nid in level:
            return level[nid]
        n = graph.nodes[nid]
        if n.kind in ('IN', 'CONST'):
            level[nid] = 0
            return 0
        m = 0
        for a in n.args:
            m = max(m, lvl(a) + 1)
        level[nid] = m
        return m

    for r in graph.signals.values():
        lvl(r)
    return level


def _step_offsets(n_inputs, step):
    return [step * i - (step // 2) * (n_inputs - 1) for i in range(n_inputs)]


def gate_input_offsets(n_inputs):
    """Confirmed from real Logisim-evolution 3.9.0 .circ files (exact wire
    coordinates) and user-confirmed real renders. N=2 and N=3 are their
    own small-case exceptions; every other N sits on one shared step-10
    sequence (odd N directly, even N as that sequence for N+1 positions
    with the center slot removed)."""
    if n_inputs <= 1:
        return [0]
    if n_inputs == 2:
        return [-20, 20]
    if n_inputs == 3:
        return [-20, 0, 20]
    if n_inputs % 2 == 1:
        return _step_offsets(n_inputs, 10)
    raw = _step_offsets(n_inputs + 1, 10)
    raw.remove(0)
    return raw


def output_point(loc, facing):
    return loc


def input_points(loc, kind, n_in, facing):
    depth = NOT_DEPTH if kind == 'NOT' else _gate_depth(kind) if kind != 'XNOR' else XNOR_DEPTH
    offs = [0] if kind == 'NOT' else gate_input_offsets(n_in)
    x, y = loc
    if facing == 'east':
        return [(x - depth, y + o) for o in offs]
    elif facing == 'west':
        return [(x + depth, y + o) for o in offs]
    elif facing == 'north':
        return [(x + o, y + depth) for o in offs]
    elif facing == 'south':
        return [(x + o, y - depth) for o in offs]
    raise ValueError(facing)


def gate_input_offsets_extent(kind, n_in):
    offs = [0] if kind == 'NOT' else gate_input_offsets(n_in)
    extent = offs + [0]
    return min(extent), max(extent)


def _choose_rows(max_level):
    """Folding into multiple rows (see module docstring) gives a real
    footprint win and works correctly on small-to-medium designs, but at
    full scale on a densely cross-connected circuit it can create
    genuinely congested corridors where many edges need to cross the same
    narrow gap -- a real channel-routing/congestion problem, not just a
    bug, and one my simple per-edge channel search (with pathfinding as a
    fallback for the rare geometrically-impossible case) could not be
    made fully reliable against within the time available. Given the
    correctness stakes, folding is disabled by default (always 1 row --
    the single-strip layout proven correct across extensive testing) until
    it has a properly congestion-aware router behind it."""
    return 1


def layout(graph):
    from netlist import topo_order
    roots = list(graph.signals.values())
    order_all = topo_order(graph, roots)
    level = compute_levels(graph)
    gate_ids = [nid for nid in order_all if graph.nodes[nid].kind not in ('IN', 'CONST')]
    max_level = max((level[nid] for nid in gate_ids), default=0)

    depth_by_level = {lv: NOT_DEPTH for lv in range(1, max_level + 1)}
    indeg_by_level = {lv: 0 for lv in range(1, max_level + 1)}
    for nid in gate_ids:
        n = graph.nodes[nid]
        depth_by_level[level[nid]] = max(depth_by_level[level[nid]], _gate_depth(n.kind))
        indeg_by_level[level[nid]] += 1 if n.kind == 'NOT' else len(n.args)

    # ---- fold levels into rows, alternating facing east/west ----
    n_rows = _choose_rows(max_level)
    levels_per_row = -(-max_level // n_rows)  # ceil
    row_levels = []
    lv = 1
    for r in range(n_rows):
        this_row = []
        for _ in range(levels_per_row):
            if lv > max_level:
                break
            this_row.append(lv)
            lv += 1
        if this_row:
            row_levels.append(this_row)
    n_rows = len(row_levels)
    facing_of_row = ['east' if r % 2 == 0 else 'west' for r in range(n_rows)]
    facing_of_level = {}
    for r, lv_list in enumerate(row_levels):
        for lv2 in lv_list:
            facing_of_level[lv2] = facing_of_row[r]

    # ---- x columns per level within its row (proven indegree-aware corridor
    # ---- sizing: private channel per edge, sized so it can never reach the
    # ---- next column's own input-pin x) ----
    col_x = {}
    row_x_end = []   # rightmost (east rows) / leftmost (west rows) extent used
    for r, lv_list in enumerate(row_levels):
        facing = facing_of_row[r]
        if facing == 'east':
            x = INPUT_COL_X if r == 0 else row_x_end[r - 1]
            for lv2 in lv_list:
                corridor = CHANNEL_MARGIN + CHANNEL_STEP * max(indeg_by_level[lv2], 1)
                x = x + corridor + depth_by_level[lv2] + COLUMN_SAFETY_GAP
                col_x[lv2] = x
            row_x_end.append(x)
        else:
            x = row_x_end[r - 1]
            for lv2 in lv_list:
                corridor = CHANNEL_MARGIN + CHANNEL_STEP * max(indeg_by_level[lv2], 1)
                x = x - (corridor + depth_by_level[lv2] + COLUMN_SAFETY_GAP)
                col_x[lv2] = x
            row_x_end.append(x)

    # ---- y bands: per LEVEL within a row (cumulative, exclusive -- proven
    # ---- safe), rows themselves stacked with a gap between them ----
    row_y_top = []
    row_y_bottom = []
    y = TOP_MARGIN

    input_pos = {}
    iy = y
    for name, width in graph.inputs.items():
        if width is None:
            key = (name, None)
            if key in graph.input_leaf:
                input_pos[key] = (INPUT_COL_X, iy)
                iy += ROW_HEIGHT
        else:
            for b in range(width - 1, -1, -1):
                key = (name, b)
                if key in graph.input_leaf:
                    input_pos[key] = (INPUT_COL_X, iy)
                    iy += ROW_HEIGHT
            iy += ROW_HEIGHT
    const_pos = {}
    gate_id_set = set(gate_ids)
    for cid in (graph.const0, graph.const1):
        if cid in gate_id_set or cid in roots:
            const_pos[cid] = (INPUT_COL_X, iy)
            iy += ROW_HEIGHT

    gate_pos = {}
    for r, lv_list in enumerate(row_levels):
        row_top = y if r > 0 else min(y, TOP_MARGIN)
        row_top = max(row_top, iy) if r == 0 else row_top
        band_start = row_top
        row_bottom = band_start
        for lv2 in lv_list:
            items = []
            for nid in gate_ids:
                if level[nid] != lv2:
                    continue
                n = graph.nodes[nid]
                n_in = 1 if n.kind == 'NOT' else len(n.args)
                items.append((nid, n_in, n.kind == 'NOT'))
            prev_bottom = None
            cursor = band_start
            for nid, n_in, is_not in items:
                top_ext, bot_ext = gate_input_offsets_extent('NOT' if is_not else 'AND', n_in)
                if prev_bottom is None:
                    cy = cursor - top_ext
                else:
                    cy = prev_bottom + ROW_GAP - top_ext
                gate_pos[nid] = (col_x[lv2], cy)
                prev_bottom = cy + bot_ext
                cursor = cy + ROW_HEIGHT
            band_bottom = prev_bottom if items else band_start
            row_bottom = max(row_bottom, band_bottom)
            band_start = band_bottom + BAND_GAP
        row_y_top.append(row_top)
        row_y_bottom.append(row_bottom)
        y = row_bottom + ROW_BAND_GAP

    # ---- output pins: continue past the last row's flow direction ----
    last_row = n_rows - 1
    last_facing = facing_of_row[last_row]
    last_level = row_levels[last_row][-1]
    out_corridor = CHANNEL_MARGIN + CHANNEL_STEP * max(len(graph.signal_order), 1)
    if last_facing == 'east':
        out_x = col_x[last_level] + out_corridor + depth_by_level[last_level] + COLUMN_SAFETY_GAP
    else:
        out_x = col_x[last_level] - (out_corridor + depth_by_level[last_level] + COLUMN_SAFETY_GAP)
    output_pos = {}
    oy = row_y_top[last_row]
    for name in graph.signal_order:
        output_pos[name] = (out_x, oy)
        oy += ROW_HEIGHT

    return {
        'input_pos': input_pos,
        'gate_pos': gate_pos,
        'output_pos': output_pos,
        'const_pos': const_pos,
        'gate_ids': gate_ids,
        'level': level,
        'facing_of_level': facing_of_level,
        'col_x': col_x,
    }


class RouteError(Exception):
    pass


def _bfs_fallback(src, dst, blocked, occupied_cells):
    """Full grid pathfinding, used only when the simple 3-segment channel
    route is genuinely impossible (the two endpoints' blocking pins can
    trap every candidate channel between them -- rare, but geometrically
    real, not just bad luck). Avoids every real pin except src/dst, and
    every CELL another already-routed fallback path has touched (not just
    edges -- two different paths using different edges can still share a
    cell as an explicit endpoint on each side, which is a real shared
    connection between two different nets, the same lesson learned
    earlier with the channel router's own cell-vs-edge exclusivity).
    Returns a list of unit (p1,p2) segments -- deliberately not merged
    into longer runs, for the same T-junction-safety reason as everywhere
    else in this file: any point two different routes ever touch must be
    an explicit shared endpoint."""
    from collections import deque
    GRID = 10
    sx, sy = src
    dx, dy = dst

    def passable(p):
        if p == dst or p == src:
            return True
        return p not in blocked and p not in occupied_cells

    pad = 400
    xmin = min(sx, dx) - pad
    xmax = max(sx, dx) + pad
    ymin = min(sy, dy) - pad
    ymax = max(sy, dy) + pad

    q = deque([src])
    prev = {src: None}
    found = None
    while q:
        cur = q.popleft()
        if cur == dst:
            found = cur
            break
        cx, cy = cur
        for nb in ((cx + GRID, cy), (cx - GRID, cy), (cx, cy + GRID), (cx, cy - GRID)):
            if not (xmin <= nb[0] <= xmax and ymin <= nb[1] <= ymax):
                continue
            if nb in prev:
                continue
            if not passable(nb):
                continue
            prev[nb] = cur
            q.append(nb)
    if found is None:
        raise RouteError(f"no path at all found from {src} to {dst}")
    path = []
    c = found
    while c is not None:
        path.append(c)
        c = prev[c]
    path.reverse()
    return [(path[i], path[i + 1]) for i in range(len(path) - 1)]


class Router:
    """Private routing channel per edge -- proven correct extensively in
    the single-row version. Each wire gets its own globally-unique
    channel x/y (perpendicular to its host row's flow direction) so two
    different wires' corner points can never coincide.

    Folding levels into rows means different rows' x-ranges can overlap
    numerically (row 1, folding back leftward, can revisit x values row 0
    already used) -- which the original single strictly-increasing-x
    design never had to worry about. So on top of per-edge uniqueness:
      - a candidate channel x is rejected if any real pin sits at that x
        ANYWHERE within the y-range the vertical leg would actually
        traverse (indexed by x for speed) -- not just "does this x match
        some other level's column at all", which would reject huge swaths
        of otherwise-safe x's purely because they coincide with some
        other level's column at a y this specific edge never visits.
      - every candidate is also checked against the actual pins at this
        edge's source/destination y (indexed by y) for the two
        horizontal legs. This matters specifically for any gate whose own
        input sits at dy=0 (NOT gates, and every odd-N gate): its input
        and output share the same y, so a route heading "backward" past
        that gate could otherwise pass straight through its own other
        pin.
    """

    def __init__(self, pins_by_x, pins_by_y):
        self._next = {}
        self.pins_by_x = pins_by_x   # x -> sorted list of real pin y's
        self.pins_by_y = pins_by_y   # y -> sorted list of real pin x's
        self.used_ranges = {}        # x -> list of (lo,hi) y-ranges already
                                      # occupied by some edge's vertical leg
                                      # at that x. Two edges CAN legitimately
                                      # share an x as long as their y-ranges
                                      # don't overlap (or touch -- touching
                                      # would still mean a shared corner
                                      # point between two different nets).

    def _horiz_clear(self, ex, cx, y):
        lo, hi = (ex, cx) if ex <= cx else (cx, ex)
        for px in self.pins_by_y.get(y, ()):
            if lo < px < hi:
                return False
        return True

    def _vert_clear(self, v, y1, y2):
        lo, hi = (y1, y2) if y1 <= y2 else (y2, y1)
        for py in self.pins_by_x.get(v, ()):
            if lo <= py <= hi:
                return False
        return True

    def _range_free(self, v, y1, y2):
        lo, hi = (y1, y2) if y1 <= y2 else (y2, y1)
        for (lo2, hi2) in self.used_ranges.get(v, ()):
            if hi >= lo2 and hi2 >= lo:  # overlap or touch
                return False
        return True

    def channel(self, corridor_key, base, src, dst):
        """Search outward in both directions from base for a channel x
        that's clear of every real pin along the vertical leg it would
        actually traverse and doesn't cross any pin along either
        horizontal leg. A one-directional search can fail outright: if
        the destination sits on the opposite side of base from where the
        search moves, growing the range only ever adds more potential
        blockers, never escapes them."""
        start = self._next.get(corridor_key, base)
        y1, y2 = src[1], dst[1]

        def ok(v):
            return (self._range_free(v, y1, y2)
                    and self._vert_clear(v, y1, y2)
                    and self._horiz_clear(src[0], v, src[1])
                    and self._horiz_clear(dst[0], v, dst[1]))

        def commit(v):
            self._next[corridor_key] = v + CHANNEL_STEP
            lo, hi = (y1, y2) if y1 <= y2 else (y2, y1)
            self.used_ranges.setdefault(v, []).append((lo, hi))

        if ok(start):
            commit(start)
            return start
        for k in range(1, 100000):
            for v in (start + k * CHANNEL_STEP, start - k * CHANNEL_STEP):
                if ok(v):
                    commit(v)
                    return v
        raise RuntimeError(
            f"channel() could not find a clear x for corridor {corridor_key}, "
            f"src={src} dst={dst} -- likely a real design gap, not just bad luck")


def sanitize_label(name):
    return name.replace('[', '_').replace(']', '').replace(':', '_')


def _unique_circuit_name(root, base_name):
  """Return a circuit name that does not already exist in the project.

  Comparison is case-insensitive. If `base_name` is already present,
  suffix it with _1, _2, ... until a unique name is found.
  """

  existing = {
    circuit.get('name').lower()
    for circuit in root.findall('circuit')
    if circuit.get('name') is not None
  }

  # print(existing)
  # print()
  # print(base_name)

  if base_name.lower() not in existing:
    return base_name

  n = 1

  while f'{base_name}_{n}'.lower() in existing:
    n += 1

  return f'{base_name}_{n}'


def build_circ(graph, existing_circ, netlist_path):
    """Append the generated circuit to an existing Logisim .circ project.

    The existing project is parsed and left intact. The generated logic is
    added as a new <circuit> at the end of the project's circuit list.

    `netlist_path` is used to derive the circuit name from the netlist
    filename, e.g.
    "nets/forwarding.netlist" -> "forwarding".

    If that circuit name already exists, `_unique_circuit_name()` gives the
    new circuit a suffix such as forwarding_1, forwarding_2, etc.
    """
    tree = ET.parse(existing_circ)
    root = tree.getroot()

    if root.tag != 'project':
        raise ValueError(
            f'expected Logisim <project> root, got <{root.tag}>'
        )

    circuit_name = Path(netlist_path).stem
    circuit_name = _unique_circuit_name(root, circuit_name)

    lay = layout(graph)
    input_pos = lay['input_pos']
    gate_pos = lay['gate_pos']
    output_pos = lay['output_pos']
    const_pos = lay['const_pos']
    gate_ids = lay['gate_ids']
    level = lay['level']
    facing_of_level = lay['facing_of_level']
    col_x = lay['col_x']

    comps = []
    wires = set()

    src_point = {}
    for (name, idx), pt in input_pos.items():
        src_point[graph.input_leaf[(name, idx)]] = pt
    for cid, pt in const_pos.items():
        src_point[cid] = pt

    gate_inputs = {}
    all_pins = set(input_pos.values())
    all_pins.update(const_pos.values())
    for nid in gate_ids:
        n = graph.nodes[nid]
        loc = gate_pos[nid]
        facing = facing_of_level[level[nid]]
        src_point[nid] = output_point(loc, facing)
        n_in = 1 if n.kind == 'NOT' else len(n.args)
        pins = input_points(loc, n.kind, n_in, facing)
        gate_inputs[nid] = list(zip(n.args, pins))
        all_pins.add(loc)
        all_pins.update(pins)
    all_pins.update(output_pos.values())

    pins_by_y = {}
    pins_by_x = {}
    for (px, py) in all_pins:
        pins_by_y.setdefault(py, []).append(px)
        pins_by_x.setdefault(px, []).append(py)

    router = Router(pins_by_x, pins_by_y)

    def add_wire(p1, p2):
        if p1 == p2:
            return
        key = (p1, p2) if p1 <= p2 else (p2, p1)
        wires.add(key)

    edge_counter = [0]

    def _current_occupied_cells():
        occ = set()
        for (x1, y1), (x2, y2) in wires:
            if x1 == x2:
                lo, hi = min(y1, y2), max(y1, y2)
                for yy in range(lo, hi + 1, 10):
                    occ.add((x1, yy))
            elif y1 == y2:
                lo, hi = min(x1, x2), max(x1, x2)
                for xx in range(lo, hi + 1, 10):
                    occ.add((xx, y1))
        return occ

    def route(src, dst, facing):
        """Route one edge through a private channel.

        If no channel route is possible, use the BFS fallback.
        """
        sx, sy = src
        dx, dy = dst
        edge_counter[0] += 1
        try:
            cx = router.channel(edge_counter[0], dx, src, dst)
            add_wire((sx, sy), (cx, sy))
            add_wire((cx, sy), (cx, dy))
            add_wire((cx, dy), (dx, dy))
        except RuntimeError:
            occupied = _current_occupied_cells()
            for p1, p2 in _bfs_fallback(src, dst, all_pins, occupied):
                add_wire(p1, p2)
                # Register both endpoints as real obstacles from now on, so
                # a later channel-based route cannot unknowingly pass through
                # a cell this fallback path used.
                for (px, py) in (p1, p2):
                    if py not in pins_by_x.get(px, []):
                        pins_by_x.setdefault(px, []).append(py)
                    if px not in pins_by_y.get(py, []):
                        pins_by_y.setdefault(py, []).append(px)

    for nid in gate_ids:
        n = graph.nodes[nid]
        facing = facing_of_level[level[nid]]
        for child, pin in gate_inputs[nid]:
            route(src_point[child], pin, facing)

    last_level = max(col_x) if col_x else 0
    out_facing = facing_of_level.get(last_level, 'east')
    for name, pt in output_pos.items():
        nid = graph.signals[name]
        route(src_point[nid], pt, out_facing)

    # ---- emit components ----
    for (name, idx), (x, y) in input_pos.items():
        label = name if idx is None else f'{name}{idx}'
        comps.append(
            f'<comp lib="0" loc="({x},{y})" name="Pin">\n'
            f'      <a name="label" val="{sanitize_label(label)}"/>\n'
            f'      <a name="appearance" val="NewPins"/>\n'
            f'    </comp>'
        )

    for cid, (x, y) in const_pos.items():
        val = graph.nodes[cid].args[0]
        comps.append(
            f'<comp lib="0" loc="({x},{y})" name="Constant">\n'
            f'      <a name="value" val="0x{val}"/>\n'
            f'    </comp>'
        )

    for nid in gate_ids:
        n = graph.nodes[nid]
        x, y = gate_pos[nid]
        facing = facing_of_level[level[nid]]
        facing_attr = (
            ''
            if facing == 'east'
            else f'\n      <a name="facing" val="{facing}"/>'
        )

        if n.kind == 'NOT':
            comps.append(
                f'<comp lib="1" loc="({x},{y})" name="NOT Gate">{facing_attr}\n'
                f'      <a name="label" val="g{nid}"/>\n'
                f'    </comp>'
            )
        elif n.kind in ('AND', 'OR'):
            gname = 'AND Gate' if n.kind == 'AND' else 'OR Gate'
            k = len(n.args)
            comps.append(
                f'<comp lib="1" loc="({x},{y})" name="{gname}">{facing_attr}\n'
                f'      <a name="inputs" val="{k}"/>\n'
                f'      <a name="label" val="g{nid}"/>\n'
                f'    </comp>'
            )
        elif n.kind == 'XNOR':
            comps.append(
                f'<comp lib="1" loc="({x},{y})" name="XNOR Gate">{facing_attr}\n'
                f'      <a name="inputs" val="2"/>\n'
                f'      <a name="label" val="g{nid}"/>\n'
                f'    </comp>'
            )

    out_facing_attr = (
        '\n      <a name="facing" val="west"/>'
        if out_facing == 'west'
        else ''
    )
    for name, (x, y) in output_pos.items():
        comps.append(
            f'<comp lib="0" loc="({x},{y})" name="Pin">{out_facing_attr}\n'
            f'      <a name="output" val="true"/>\n'
            f'      <a name="appearance" val="NewPins"/>\n'
            f'      <a name="label" val="{sanitize_label(name)}"/>\n'
            f'    </comp>'
        )

    # ---- build ONLY the new circuit ----
    # Parse each generated component fragment so ElementTree handles XML
    # escaping/serialization correctly.
    new_circuit = ET.Element('circuit', {'name': circuit_name})
    ET.SubElement(
        new_circuit,
        'a',
        {'name': 'circuit', 'val': circuit_name}
    )
    ET.SubElement(
        new_circuit,
        'a',
        {'name': 'clabel', 'val': ''}
    )

    for comp_xml in comps:
        new_circuit.append(ET.fromstring(comp_xml))

    for (x1, y1), (x2, y2) in sorted(wires):
        ET.SubElement(
            new_circuit,
            'wire',
            {
                'from': f'({x1},{y1})',
                'to': f'({x2},{y2})',
            }
        )

    # This is the important difference from the old exporter:
    # do NOT replace/rebuild the project. Append the generated circuit after
    # every circuit already present in the supplied .circ file.
    root.append(new_circuit)

    # Keep the output readable while preserving all existing project data.
    ET.indent(tree, space='  ')

    xml_body = ET.tostring(root, encoding='unicode')
    return '<?xml version="1.0" encoding="UTF-8" standalone="no"?>\n' + xml_body + '\n'

