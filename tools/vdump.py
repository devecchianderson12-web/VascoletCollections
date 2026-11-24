# Lee de la memoria del juego original (corriendo) las tablas de sprites, animaciones y texturas.
# Es la misma idea que tools/xadump.cpp del XA, pero en Python.
# Uso: python vdump.py <pid> <comando> [args]  (el juego tiene que estar abierto)
#   comandos: singletons <desde> <hasta> | raw <va> <n> [rt] | map <obj> | script <archivo>
import ctypes, ctypes.wintypes as W, struct, subprocess, sys, time, os

k32 = ctypes.WinDLL('kernel32', use_last_error=True)
psapi = ctypes.WinDLL('psapi', use_last_error=True)
k32.OpenProcess.restype = W.HANDLE
k32.ReadProcessMemory.argtypes = [W.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]

P = None
BASE = 0x400000
IMG = 0x200000

def R(va): return va - 0x400000 + BASE if 0x400000 <= va < 0x400000 + IMG else va
def S(rt): return rt - BASE + 0x400000 if BASE <= rt < BASE + IMG else rt
def in_image(rt): return BASE <= rt < BASE + IMG

def rd(a, n):
    buf = ctypes.create_string_buffer(n); got = ctypes.c_size_t(0)
    if not k32.ReadProcessMemory(P, ctypes.c_void_p(a), buf, n, ctypes.byref(got)) or got.value != n: return None
    return buf.raw
def u32(a):
    b = rd(a, 4); return struct.unpack('<I', b)[0] if b else 0
def f32(a):
    b = rd(a, 4); return struct.unpack('<f', b)[0] if b else 0.0

def rtti(obj):
    vt = u32(obj)
    if not in_image(vt): return ''
    col = u32(vt - 4)
    if not in_image(col): return ''
    td = u32(col + 12)
    if not in_image(td): return ''
    name = rd(td + 8, 255) or b''
    name = name.split(b'\0')[0]
    if not name.startswith(b'.?'): return ''
    s = name[4:].decode('latin1')
    return s[:-2] if s.endswith('@@') else s

def wstr(s):
    size, cap = u32(s + 0x14), u32(s + 0x18)
    if size > 512: return '<bad>'
    p = s + 4 if cap < 8 else u32(s + 4)
    b = rd(p, size * 2) if size else b''
    return (b or b'').decode('utf-16le', 'replace')

def map_entries(obj):
    out = []
    for off in range(0, 0x44, 4):
        head, size = u32(obj + off), u32(obj + off + 4)
        if head < 0x10000 or size == 0 or size > 5000: continue
        root = u32(head + 4)
        if root < 0x10000 or u32(root + 4) != head: continue
        def collect(node, d):
            if node == head or node < 0x10000 or d > 64 or len(out) > 4000: return
            collect(u32(node), d + 1)
            out.append((wstr(node + 0x0c), u32(node + 0x0c + 0x1c)))
            collect(u32(node + 8), d + 1)
        collect(root, 0)
        break
    return out

def singletons(lo, hi):
    for a in range(lo, hi, 4):
        v = u32(R(a))
        if v < 0x10000: continue
        n = rtti(v)
        if n: print('%08x -> %08x %s' % (a, v, n))

tex_name, frame_owner = {}, {}

def textures(obj):
    for k, v in map_entries(obj):
        tex_name[u32(v)] = k
        print('TEX', k)

def imagemaps(obj):
    for k, im in map_entries(obj):
        n, arr = u32(im), u32(im + 4)
        if n <= 0 or n > 512: print('IMAGEMAP %s 0' % k); continue
        print('IMAGEMAP %s %d' % (k, n))
        for i in range(n):
            fr = u32(arr + i * 4)
            frame_owner[fr] = (k, i)
            v = struct.unpack('<7f', rd(fr + 4, 28) or b'\0' * 28)
            print('F %d %s %s' % (i, tex_name.get(u32(fr), '?'), ' '.join('%g' % x for x in v)))

def anims(obj):
    for k, a in map_entries(obj):
        fps, flags, cnt, total = u32(a + 4), u32(a + 8), u32(a + 0x28), u32(a + 0x30)
        print('ANIM %s %u %08x %u %u' % (k, fps, flags, cnt, total))
        head = u32(a + 0x24); node = u32(head); i = 0
        while i < cnt and node != head and node > 0x10000:
            kf = u32(node + 8)
            w = struct.unpack('<13I', rd(kf, 52) or b'\0' * 52)
            f = [struct.unpack('<f', struct.pack('<I', x))[0] for x in w]
            own = frame_owner.get(w[0], ('?', -1))
            print('K %s %d %u %g |%s' % (own[0], own[1], w[7], f[12], ''.join(' %g' % f[j] for j in (1, 2, 3, 4, 5, 6, 8, 9, 10))))
            node = u32(node); i += 1

def raw(a, n):
    b = rd(a, n) or b''
    for i in range(0, len(b) - 3, 4):
        v = struct.unpack_from('<I', b, i)[0]
        print('  +%03x %08x %d %s' % (i, S(v), v, rtti(v) if 0x10000 < v < 0x7fff0000 else ''))

def main():
    # Uso: python vdump.py <pid del juego ya abierto> <comando> [args]
    global P, BASE
    pid, cmd = int(sys.argv[1]), sys.argv[2:]
    P = k32.OpenProcess(0x0410, False, pid)
    mods = (W.HMODULE * 64)(); need = W.DWORD()
    if psapi.EnumProcessModulesEx(P, mods, ctypes.sizeof(mods), ctypes.byref(need), 1) and need.value:
        BASE = mods[0] or 0x400000
    print('# base %08x' % BASE)
    c = cmd[0]
    if c == 'singletons': singletons(int(cmd[1], 16), int(cmd[2], 16))
    elif c == 'raw': raw(R(int(cmd[1], 16)) if len(cmd) < 4 else int(cmd[1], 16), int(cmd[2]))
    elif c == 'map': [print(k, '%08x' % v, rtti(v)) for k, v in map_entries(int(cmd[1], 16))]
    elif c == 'script':
        for line in open(cmd[1]):
            line = line.split('#')[0].split()
            if not line: continue
            print('###', ' '.join(line))
            obj = int(line[1], 16)
            {'textures': textures, 'imagemaps': imagemaps, 'anims': anims}[line[0]](obj)

main()
