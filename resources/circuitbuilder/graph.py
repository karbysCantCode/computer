"""
Shared-gate boolean optimizer.

Strategy: build every expression into a single global DAG using
"structural hash-consing" (a.k.a. structural / common-subexpression sharing):
every node is canonicalized (children sorted for commutative ops, nested
AND/OR flattened, double-negation removed, constants folded, idempotent /
complementary terms collapsed) and looked up in a global table keyed by
its canonical shape. If an identical shape already exists anywhere in the
design -- even if it came from a totally different output signal -- the
existing gate is reused instead of building a new one.

This is the "boolean algebra" optimizer requested (as opposed to a
per-output two-level espresso minimizer): it does global multi-output
common subexpression elimination, which is what actually matters for a
file like this one, where the same sub-expressions
(e.g. "exr[4]&exr[3] + exr[4]&!exr[2]") are repeated verbatim many times
across many different output equations.
"""

from dataclasses import dataclass, field


@dataclass
class Node:
    id: int
    kind: str            # 'IN' | 'NOT' | 'AND' | 'OR' | 'XNOR' | 'CONST'
    args: tuple          # meaning depends on kind (see below)
    # IN:    args = (signal_name, bit_index_or_None)
    # NOT:   args = (child_id,)
    # AND:   args = tuple(sorted(child_ids))
    # OR:    args = tuple(sorted(child_ids))
    # XNOR:  args = tuple(sorted((a,b)))
    # CONST: args = (0,) or (1,)


class Graph:
    def __init__(self):
        self.nodes = {}          # id -> Node
        self._table = {}         # canonical key -> id
        self._next_id = 0
        self.inputs = {}         # signal_name -> width (None => scalar, 1 bit)
        self.input_leaf = {}     # (signal_name, bit_index_or_None) -> node id
        self.signals = {}        # assigned-signal name -> node id (in program order)
        self.signal_order = []
        self.const0 = self._new('CONST', (0,))
        self.const1 = self._new('CONST', (1,))

    # ---------- low level ----------
    def _new(self, kind, args):
        key = (kind, args)
        if key in self._table:
            return self._table[key]
        nid = self._next_id
        self._next_id += 1
        self.nodes[nid] = Node(nid, kind, args)
        self._table[key] = nid
        return nid

    def is_const(self, nid, val=None):
        n = self.nodes[nid]
        if n.kind != 'CONST':
            return False
        return val is None or n.args[0] == val

    # ---------- inputs ----------
    def declare_input(self, name, width):
        self.inputs[name] = width  # None = 1-bit scalar

    def input_bit(self, name, index):
        """index is None for scalar inputs, or an int bit index."""
        key = (name, index)
        if key in self.input_leaf:
            return self.input_leaf[key]
        nid = self._new('IN', key)
        self.input_leaf[key] = nid
        return nid

    # ---------- gate builders (all perform boolean-algebra simplification) ----------
    def mk_not(self, a):
        if self.is_const(a, 0):
            return self.const1
        if self.is_const(a, 1):
            return self.const0
        n = self.nodes[a]
        if n.kind == 'NOT':
            return n.args[0]  # double negation elimination
        return self._new('NOT', (a,))

    def _flatten(self, kind, ids):
        out = []
        for i in ids:
            n = self.nodes[i]
            if n.kind == kind:
                out.extend(n.args)
            else:
                out.append(i)
        return out

    def mk_and(self, ids):
        ids = self._flatten('AND', ids)
        s = set(ids)
        if any(self.is_const(i, 0) for i in s):
            return self.const0
        s = {i for i in s if not self.is_const(i, 1)}
        # complementary pair: x & !x -> 0
        negs = {self.nodes[i].args[0] for i in s if self.nodes[i].kind == 'NOT'}
        if s & negs:
            return self.const0
        if not s:
            return self.const1
        if len(s) == 1:
            return next(iter(s))
        key = tuple(sorted(s))
        return self._new('AND', key)

    def mk_or(self, ids):
        ids = self._flatten('OR', ids)
        s = set(ids)
        if any(self.is_const(i, 1) for i in s):
            return self.const1
        s = {i for i in s if not self.is_const(i, 0)}
        negs = {self.nodes[i].args[0] for i in s if self.nodes[i].kind == 'NOT'}
        if s & negs:
            return self.const1
        if not s:
            return self.const0
        if len(s) == 1:
            return next(iter(s))
        key = tuple(sorted(s))
        return self._new('OR', key)

    def mk_xnor(self, a, b):
        if a == b:
            return self.const1
        if self.is_const(a) and self.is_const(b):
            va = self.nodes[a].args[0]
            vb = self.nodes[b].args[0]
            return self.const1 if va == vb else self.const0
        if self.is_const(a, 1):
            return b
        if self.is_const(b, 1):
            return a
        if self.is_const(a, 0):
            return self.mk_not(b)
        if self.is_const(b, 0):
            return self.mk_not(a)
        key = tuple(sorted((a, b)))
        return self._new('XNOR', key)

    def mk_eq_vec(self, bits_a, bits_b):
        assert len(bits_a) == len(bits_b)
        xn = [self.mk_xnor(a, b) for a, b in zip(bits_a, bits_b)]
        return self.mk_and(xn)

    # ---------- expression evaluation from AST ----------
    def bits_for(self, name, hi, lo):
        """Return list of node ids for name[hi:lo] inclusive, MSB..LSB, resolving
        either a declared input vector or (if used) a scalar."""
        return [self.input_bit(name, b) for b in range(hi, lo - 1, -1)]

    def eval_expr(self, expr):
        tag = expr[0]
        if tag == 'ref':
            _, name, idx = expr
            if name in self.signals:
                if idx is not None:
                    raise ValueError(f"signal {name} is scalar, cannot index [{idx}]")
                return self.signals[name]
            if name in self.inputs:
                width = self.inputs[name]
                if width is None:
                    if idx is not None:
                        raise ValueError(f"input {name} is a scalar, cannot index")
                    return self.input_bit(name, None)
                else:
                    if idx is None:
                        raise ValueError(f"input {name} is a vector, needs an index")
                    return self.input_bit(name, idx)
            raise ValueError(f"unknown identifier {name!r}")
        elif tag == 'slice':
            raise ValueError("a bit-slice can only appear as an operand of ==")
        elif tag == 'not':
            return self.mk_not(self.eval_expr(expr[1]))
        elif tag == 'and':
            return self.mk_and([self.eval_expr(e) for e in expr[1]])
        elif tag == 'or':
            return self.mk_or([self.eval_expr(e) for e in expr[1]])
        elif tag == 'eq':
            lhs, rhs = expr[1], expr[2]
            bits_a = self._operand_bits(lhs)
            bits_b = self._operand_bits(rhs)
            if len(bits_a) != len(bits_b):
                raise ValueError(f"== operands have different widths: {lhs} vs {rhs}")
            return self.mk_eq_vec(bits_a, bits_b)
        else:
            raise ValueError(f"unknown expr tag {tag}")

    def _operand_bits(self, expr):
        if expr[0] == 'slice':
            _, name, hi, lo = expr
            return self.bits_for(name, hi, lo)
        elif expr[0] == 'ref':
            _, name, idx = expr
            if idx is not None:
                return [self.input_bit(name, idx)]
            if name in self.inputs and self.inputs[name] is not None:
                w = self.inputs[name]
                return self.bits_for(name, w - 1, 0)
            return [self.eval_expr(expr)]
        else:
            raise ValueError("== operand must be a plain identifier or slice")

    def run_program(self, stmts):
        for s in stmts:
            if s[0] == 'input':
                self.declare_input(s[1], s[2])
            else:
                _, name, expr = s
                nid = self.eval_expr(expr)
                self.signals[name] = nid
                self.signal_order.append(name)

    # ---------- stats ----------
    def stats(self):
        kinds = {}
        for n in self.nodes.values():
            kinds[n.kind] = kinds.get(n.kind, 0) + 1
        return kinds
