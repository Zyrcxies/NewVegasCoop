"""Minimal 32-bit minidump reader for our crash dumps: exception, registers, modules, and return addresses on the crashing stack
(our plugin's mapped to function names with NVCoop.map). Usage: python dmp.py file.dmp [map]"""
import bisect
import re
import struct
import sys

data = open(sys.argv[1], 'rb').read()
sig, ver, nstreams, dir_rva = struct.unpack_from('<4sIII', data, 0)
streams = {}
for i in range(nstreams):
    t, size, rva = struct.unpack_from('<III', data, dir_rva + 12 * i)
    streams[t] = (size, rva)


def mdstring(rva):
    n = struct.unpack_from('<I', data, rva)[0]
    return data[rva + 4: rva + 4 + n].decode('utf-16-le')


modules = []
size, rva = streams[4]
n = struct.unpack_from('<I', data, rva)[0]
for i in range(n):
    off = rva + 4 + 108 * i
    base, msize = struct.unpack_from('<QI', data, off)
    name_rva = struct.unpack_from('<I', data, off + 20)[0]
    modules.append((base, msize, mdstring(name_rva).split('\\')[-1]))


def modof(addr):
    for base, msize, name in modules:
        if base <= addr < base + msize:
            return name, addr - base
    return None, addr


memory = []
if 9 in streams:
    size, rva = streams[9]
    nr, base_rva = struct.unpack_from('<QQ', data, rva)
    hdr = struct.unpack_from('<I', data, rva + 4)
    nr, = struct.unpack_from('<Q', data, rva)
    base_rva, = struct.unpack_from('<Q', data, rva + 8)
    off = base_rva
    for i in range(nr):
        start, sz = struct.unpack_from('<QQ', data, rva + 16 + 16 * i)
        memory.append((start, sz, off))
        off += sz
if 5 in streams:
    size, rva = streams[5]
    n = struct.unpack_from('<I', data, rva)[0]
    for i in range(n):
        start, sz, mrva = struct.unpack_from('<QII', data, rva + 4 + 16 * i)
        memory.append((start, sz, mrva))


# Thread stacks (ThreadListStream: each thread's stack is a memory descriptor of its own).
if 3 in streams:
    size, rva = streams[3]
    n = struct.unpack_from('<I', data, rva)[0]
    for i in range(n):
        off = rva + 4 + 48 * i
        start, sz, mrva = struct.unpack_from('<QII', data, off + 24)
        memory.append((start, sz, mrva))


def read(addr, n):
    for start, sz, off in memory:
        if start <= addr and addr + n <= start + sz:
            return data[off + addr - start: off + addr - start + n]
    return None


size, rva = streams[6]
tid = struct.unpack_from('<I', data, rva)[0]
code, flags, rec, eaddr = struct.unpack_from('<IIQQ', data, rva + 8)
nparams = struct.unpack_from('<I', data, rva + 8 + 24)[0]
params = struct.unpack_from('<15Q', data, rva + 8 + 32)
ctx_size, ctx_rva = struct.unpack_from('<II', data, rva + 8 + 152)
print(f'exception {code:08X} at {eaddr:08X} {modof(eaddr)} thread {tid}; info {[hex(p) for p in params[:nparams]]}')
ctx = data[ctx_rva: ctx_rva + ctx_size]
# x86 CONTEXT: edi esi ebx edx ecx eax at 0x9C.., ebp 0xB4, eip 0xB8, esp 0xC4
regs = dict(zip(['edi', 'esi', 'ebx', 'edx', 'ecx', 'eax', 'ebp', 'eip'], struct.unpack_from('<8I', ctx, 0x9C)))
regs['esp'] = struct.unpack_from('<I', ctx, 0xC4)[0]
print(' '.join(f'{k}={v:08X}' for k, v in regs.items()))

symbols = []
if len(sys.argv) > 2:
    for line in open(sys.argv[2], errors='ignore'):
        m = re.match(r'\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{8})\s', line)
        if m:
            symbols.append((int(m.group(2), 16), m.group(1)))
    symbols.sort()
ours = next((b for b, s, nme in modules if nme.lower() == 'nvcoop.dll'), None)


def name(addr):
    mod, off = modof(addr)
    if mod and mod.lower() == 'nvcoop.dll' and symbols:
        pref = 0x10000000 + off  # preferred base in the map
        i = bisect.bisect_right([s[0] for s in symbols], pref) - 1
        if i >= 0:
            return f'NVCoop!{symbols[i][1]}+{pref - symbols[i][0]:X}'
    return f'{mod}+{off:X}' if mod else f'{addr:08X}'


stack = next((read(regs['esp'], n) for n in (0x2000, 0x1000, 0x800, 0x400, 0x100) if read(regs['esp'], n)), b'')
print('stack (code addresses only):')
shown = 0
for i in range(0, len(stack) - 3, 4):
    v = struct.unpack_from('<I', stack, i)[0]
    mod, off = modof(v)
    if mod and (mod.lower().endswith('.exe') or mod.lower().endswith('.dll')) and shown < 40:
        print(f'  esp+{i:04X}: {v:08X} {name(v)}')
        shown += 1

# The first thread is usually the game's main one: what it was doing at the time (when another thread crashed).
if 3 in streams:
    size, rva = streams[3]
    tid0 = struct.unpack_from('<I', data, rva + 4)[0]
    if tid0 != tid:
        start, sz, mrva = struct.unpack_from('<QII', data, rva + 4 + 24)
        main_stack = data[mrva: mrva + sz]
        print(f'main thread {tid0} (code addresses only):')
        shown = 0
        for i in range(0, len(main_stack) - 3, 4):
            v = struct.unpack_from('<I', main_stack, i)[0]
            mod, off = modof(v)
            if mod and (mod.lower().endswith('.exe') or mod.lower().endswith('.dll')) and shown < 30:
                print(f'  +{i:04X}: {v:08X} {name(v)}')
                shown += 1
