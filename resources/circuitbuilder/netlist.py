def topo_order(graph, root_ids):
    """Return node ids reachable from root_ids in a valid evaluation
    (topological) order, gates only appear after all their inputs."""
    visited = set()
    order = []

    def visit(nid):
        if nid in visited:
            return
        visited.add(nid)
        n = graph.nodes[nid]
        for a in n.args:
            if n.kind != 'IN' and n.kind != 'CONST':
                visit(a)
        order.append(nid)

    for r in root_ids:
        visit(r)
    return order


def net_name(graph, nid):
    n = graph.nodes[nid]
    if n.kind == 'IN':
        name, idx = n.args
        return name if idx is None else f"{name}[{idx}]"
    if n.kind == 'CONST':
        return f"const{n.args[0]}"
    return f"g{nid}"


def render_netlist(graph):
    roots = list(graph.signals.values())
    order = topo_order(graph, roots)
    lines = []
    lines.append("// Auto-generated shared-gate netlist")
    lines.append("// (identical sub-expressions anywhere in the design share the same gate)")
    lines.append("")
    lines.append("// ---- inputs ----")
    for name, width in graph.inputs.items():
        if width is None:
            lines.append(f"input {name};")
        else:
            lines.append(f"input {name}[{width}];")
    lines.append("")
    lines.append("// ---- gates (topological order) ----")
    gate_count = 0
    for nid in order:
        n = graph.nodes[nid]
        if n.kind in ('IN', 'CONST'):
            continue
        gate_count += 1
        nm = net_name(graph, nid)
        if n.kind == 'NOT':
            lines.append(f"wire {nm} = NOT({net_name(graph, n.args[0])});")
        elif n.kind == 'AND':
            lines.append(f"wire {nm} = AND({', '.join(net_name(graph, a) for a in n.args)});  // {len(n.args)}-in")
        elif n.kind == 'OR':
            lines.append(f"wire {nm} = OR({', '.join(net_name(graph, a) for a in n.args)});  // {len(n.args)}-in")
        elif n.kind == 'XNOR':
            a, b = n.args
            lines.append(f"wire {nm} = XNOR({net_name(graph, a)}, {net_name(graph, b)});")
    lines.append("")
    lines.append("// ---- outputs ----")
    for name in graph.signal_order:
        lines.append(f"output {name} = {net_name(graph, graph.signals[name])};")

    # Stats over only the nodes actually reachable from the outputs (i.e.
    # what's really in the emitted netlist/circuit) -- not every node ever
    # constructed during simplification, some of which get discarded by
    # algebraic rewrites (e.g. "x & !x" collapses to a constant and the
    # intermediate NOT node it briefly created is never used again).
    kinds = {}
    for nid in order:
        k = graph.nodes[nid].kind
        kinds[k] = kinds.get(k, 0) + 1
    total_gates = sum(v for k, v in kinds.items() if k not in ('IN', 'CONST'))
    lines.append("")
    lines.append("// ---- stats (reachable nodes only) ----")
    lines.append(f"// distinct input bits used : {kinds.get('IN', 0)}")
    lines.append(f"// constants                : {kinds.get('CONST', 0)}")
    lines.append(f"// NOT gates                : {kinds.get('NOT', 0)}")
    lines.append(f"// AND gates                : {kinds.get('AND', 0)}")
    lines.append(f"// OR gates                 : {kinds.get('OR', 0)}")
    lines.append(f"// XNOR gates (bit compares): {kinds.get('XNOR', 0)}")
    lines.append(f"// TOTAL GATES              : {total_gates}")
    lines.append(f"// named output signals     : {len(graph.signal_order)}")
    return "\n".join(lines) + "\n"
