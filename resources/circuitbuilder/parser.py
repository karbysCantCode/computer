import re

TOKEN_RE = re.compile(r'\s*(==|[A-Za-z_][A-Za-z0-9_]*|[0-9]+|[(){}\[\]:;=&+!,])')

def strip_comments(text):
    out_lines = []
    for line in text.splitlines():
        idx = line.find('//')
        if idx != -1:
            line = line[:idx]
        out_lines.append(line)
    return '\n'.join(out_lines)

def tokenize(text):
    text = strip_comments(text)
    pos = 0
    tokens = []
    while pos < len(text):
        m = TOKEN_RE.match(text, pos)
        if not m:
            if text[pos].isspace():
                pos += 1
                continue
            raise SyntaxError(f"Unexpected character {text[pos]!r} at pos {pos}")
        tok = m.group(1)
        tokens.append(tok)
        pos = m.end()
    return tokens


class ParseError(Exception):
    pass


# ---- AST node types (plain tuples for simplicity) ----
# ('input', name, width_or_None)
# ('assign', name, expr)
# expr forms:
#   ('ref', name, index_or_None)        index None = scalar ref
#   ('slice', name, hi, lo)             multi-bit reference (used only under 'eq')
#   ('not', expr)
#   ('and', [expr,...])
#   ('or',  [expr,...])
#   ('eq', lhs, rhs)   lhs/rhs are ('ref',...) or ('slice',...)

class Parser:
    def __init__(self, tokens):
        self.toks = tokens
        self.i = 0

    def peek(self):
        return self.toks[self.i] if self.i < len(self.toks) else None

    def next(self):
        t = self.peek()
        self.i += 1
        return t

    def expect(self, tok):
        t = self.next()
        if t != tok:
            raise ParseError(f"Expected {tok!r} but got {t!r} at token {self.i}")
        return t

    def parse_program(self):
        stmts = []
        while self.peek() is not None:
            stmts.append(self.parse_statement())
        return stmts

    def parse_statement(self):
        if self.peek() == 'input':
            self.next()
            name = self.next()
            width = None
            if self.peek() == '[':
                self.next()
                width = int(self.next())
                self.expect(']')
            self.expect(';')
            return ('input', name, width)
        else:
            name = self.next()
            self.expect('=')
            expr = self.parse_or()
            self.expect(';')
            return ('assign', name, expr)

    def parse_or(self):
        terms = [self.parse_and()]
        while self.peek() == '+':
            self.next()
            terms.append(self.parse_and())
        if len(terms) == 1:
            return terms[0]
        return ('or', terms)

    def parse_and(self):
        factors = [self.parse_not()]
        while self.peek() == '&':
            self.next()
            factors.append(self.parse_not())
        if len(factors) == 1:
            return factors[0]
        return ('and', factors)

    def parse_not(self):
        if self.peek() == '!':
            self.next()
            return ('not', self.parse_not())
        return self.parse_cmp()

    def parse_cmp(self):
        lhs = self.parse_atomexpr()
        if self.peek() == '==':
            self.next()
            rhs = self.parse_atomexpr()
            return ('eq', lhs, rhs)
        return lhs

    def parse_atomexpr(self):
        if self.peek() == '(':
            self.next()
            e = self.parse_or()
            self.expect(')')
            return e
        name = self.next()
        if name is None or not re.match(r'^[A-Za-z_]', name):
            raise ParseError(f"Expected identifier, got {name!r} at token {self.i}")
        if self.peek() == '[':
            self.next()
            hi = int(self.next())
            if self.peek() == ':':
                self.next()
                lo = int(self.next())
                self.expect(']')
                return ('slice', name, hi, lo)
            else:
                self.expect(']')
                return ('ref', name, hi)
        return ('ref', name, None)


def parse(text):
    return Parser(tokenize(text)).parse_program()
