# Lista las constantes float de .rdata que usa el código de Vascolet.exe en un rango de direcciones.
import struct, sys
f = open(r'C:\Users\Anderson\Desktop\1\Vascolet\Vascolet.exe', 'rb').read()
pe = struct.unpack_from('<I', f, 0x3c)[0]; n = struct.unpack_from('<H', f, pe + 6)[0]; opt = struct.unpack_from('<H', f, pe + 20)[0]
secs = {}
for i in range(n):
    name, vs, va, rs, ra = struct.unpack_from('<8sIIII', f, pe + 24 + opt + i * 40)
    secs[name.rstrip(b'\0').decode()] = (va, vs, ra, rs)
rva, rvs, rra, rrs = secs['.rdata']; tva, tvs, tra, trs = secs['.text']
rd = f[rra:rra + rrs]; code = f[tra:tra + trs]
def fv(a):
    o = a - 0x400000 - rva
    return struct.unpack_from('<f', rd, o)[0] if 0 <= o < len(rd) - 4 else None
for arg in sys.argv[1:]:
    lo, hi = [int(x, 16) for x in arg.split('-')]
    print('== %x-%x' % (lo, hi))
    for va in range(lo, hi):
        i = va - 0x400000 - tva
        a = struct.unpack_from('<I', code, i)[0]
        if 0x400000 + rva <= a < 0x400000 + rva + rrs and (a & 3) == 0 and code[i - 2] in (0xd9, 0xd8, 0xdc, 0xdd):
            v = fv(a)
            if v is not None and abs(v) < 1e6 and (v == 0 or abs(v) > 1e-4):
                print('%08x %g' % (va, v))
