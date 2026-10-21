"""Lists an MSVC x86 executable's virtual function tables from its RTTI.

Each class with RTTI has a type descriptor (its mangled name, ".?AV...", 8 bytes
in), a complete object locator per vtable (signature 0, the subobject's offset,
the constructor displacement offset, the type descriptor, the class hierarchy),
and each vtable is preceded by a pointer to its locator. Prints, tab-separated:
class, vtable address, subobject offset, slot offset, function address.
"""
import struct, sys

def main(path):
    d = open(path, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    n = struct.unpack_from('<H', d, pe + 6)[0]
    opt = struct.unpack_from('<H', d, pe + 20)[0]
    base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
    secs = []
    for i in range(n):
        o = pe + 24 + opt + 40 * i
        vs, va, rs, ra = struct.unpack_from('<IIII', d, o + 8)
        flags = struct.unpack_from('<I', d, o + 36)[0]
        secs.append((d[o:o + 8].rstrip(b'\0').decode(), va, max(vs, rs), ra, rs, flags))

    def off(addr):
        rva = addr - base
        for name, va, size, ra, rs, flags in secs:
            if va <= rva < va + rs:
                return ra + rva - va
        return None

    def is_code(addr):
        rva = addr - base
        return any(va <= rva < va + size and flags & 0x20000000 for _, va, size, _, _, flags in secs)

    def demangle(name):
        # ".?AVFoo@Bar@@" -> "Bar::Foo"; templates are left mangled.
        body = name[4:]
        if body.endswith('@@'):
            body = body[:-2]
        parts = [p for p in body.split('@') if p]
        return '::'.join(reversed(parts)) if not any(c in body for c in '?$') else name

    # Type descriptors: the ".?AV"/".?AU" names, 8 bytes into each.
    tds = {}
    for marker in (b'.?AV', b'.?AU'):
        i = d.find(marker)
        while i >= 0:
            end = d.index(b'\0', i)
            for name, va, size, ra, rs, flags in secs:
                if ra <= i - 8 < ra + rs:
                    tds[base + va + (i - 8 - ra)] = demangle(d[i:end].decode('latin1'))
            i = d.find(marker, end)

    # Complete object locators: signature 0, then a pointer to a known descriptor.
    cols = {}
    for name, va, size, ra, rs, flags in secs:
        if flags & 0x20000000:
            continue
        for p in range(ra, ra + rs - 20, 4):
            sig, offset, cd, td = struct.unpack_from('<IIII', d, p)
            if sig == 0 and td in tds and offset < 0x10000:
                cols[base + va + (p - ra)] = (tds[td], offset)

    # Vtables: a pointer to a locator, then pointers to code.
    rows = []
    for name, va, size, ra, rs, flags in secs:
        if flags & 0x20000000:
            continue
        for p in range(ra, ra + rs - 8, 4):
            col = struct.unpack_from('<I', d, p)[0]
            if col not in cols:
                continue
            vt = base + va + (p + 4 - ra)
            cls, offset = cols[col]
            slot = 0
            while True:
                o = off(vt + slot * 4)
                if o is None:
                    break
                f = struct.unpack_from('<I', d, o)[0]
                if not is_code(f) or (slot and struct.unpack_from('<I', d, o)[0] in cols):
                    break
                rows.append((cls, vt, offset, slot * 4, f))
                slot += 1
                if slot > 400:
                    break
    for cls, vt, offset, slot, f in rows:
        print(f'{cls}\t{vt:08x}\t{offset}\t0x{slot:x}\t{f:08x}')
    print(f'{len(tds)} type descriptors, {len(cols)} locators, '
          f'{len(set((r[1]) for r in rows))} vtables, {len(rows)} slots', file=sys.stderr)

main(sys.argv[1])
