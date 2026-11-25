# Lee varias veces campos float de un objeto del juego original. Uso: sample.py <pid> <obj hex> <ms total> <offsets hex...>
import sys, time, struct
src = open(__file__.replace('sample.py', 'vdump.py'), encoding='utf-8').read().rsplit('main()', 1)[0]
exec(src)
pid, obj, total = int(sys.argv[1]), int(sys.argv[2], 16), int(sys.argv[3])
offs = [int(o, 16) for o in sys.argv[4:]]
P = k32.OpenProcess(0x0410, False, pid)
globals()['P'] = P
t0 = time.time()
while (time.time() - t0) * 1000 < total:
    vals = [struct.unpack('<f', rd(obj + o, 4) or b'\0\0\0\0')[0] for o in offs]
    print('%5d ' % ((time.time() - t0) * 1000) + ' '.join('%9.2f' % v for v in vals))
    time.sleep(0.015)
