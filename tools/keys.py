# Envía teclas (scancodes, como un teclado real) a la ventana del juego. Uso: keys.py <pid> <tecla:espera_ms> ...
import ctypes, ctypes.wintypes as W, sys, time
u32 = ctypes.WinDLL('user32', use_last_error=True)
SC = {'enter': 0x1C, 'esc': 0x01, 'space': 0x39, 'up': 0xC8, 'down': 0xD0, 'left': 0xCB, 'right': 0xCD, 'z': 0x2C, 'x': 0x2D, 'ctrl': 0x1D}
class KI(ctypes.Structure): _fields_ = [('wVk', W.WORD), ('wScan', W.WORD), ('dwFlags', W.DWORD), ('time', W.DWORD), ('dwExtraInfo', ctypes.c_size_t)]
class INP(ctypes.Structure): _fields_ = [('type', W.DWORD), ('ki', KI), ('pad', ctypes.c_byte * 8)]
def key(sc, up):
    ext = sc > 0xFF or sc in (0xC8, 0xD0, 0xCB, 0xCD)
    flags = 0x0008 | (0x0002 if up else 0) | (0x0001 if ext else 0)
    i = INP(1, KI(0, sc & 0x7F if ext else sc, flags, 0, 0))
    u32.SendInput(1, ctypes.byref(i), ctypes.sizeof(i))
pid = int(sys.argv[1])
found = []
@ctypes.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
def cb(h, l):
    p = W.DWORD(); u32.GetWindowThreadProcessId(h, ctypes.byref(p))
    if p.value == pid and u32.IsWindowVisible(h): found.append(h)
    return True
u32.EnumWindows(cb, 0)
def focus(h):
    # Windows no deja que un programa en segundo plano tome el foco: se engancha al hilo de la ventana activa.
    k32 = ctypes.WinDLL('kernel32')
    fg = u32.GetForegroundWindow()
    a = u32.GetWindowThreadProcessId(fg, None); b = k32.GetCurrentThreadId()
    u32.AttachThreadInput(b, a, True)
    u32.ShowWindow(h, 9); u32.BringWindowToTop(h); u32.SetForegroundWindow(h)
    u32.AttachThreadInput(b, a, False)
    time.sleep(0.6)
if found: focus(found[0])
print('al frente:', bool(found) and u32.GetForegroundWindow() == found[0])
print('window', found[:1])
for a in sys.argv[2:]:
    k, _, w = a.partition(':')
    if k == 'click': pass
    # Solo si el juego es la ventana activa: nunca escribir en otra ventana.
    if not found or u32.GetForegroundWindow() != found[0]: print('el juego no está al frente, se cancela'); sys.exit(1)
    if k == 'click':
        # click:x,y (relativo a la ventana) con el mouse real
        x, y = map(int, w.split(',')); w = '900'
        r = W.RECT(); u32.GetWindowRect(found[0], ctypes.byref(r))
        u32.SetCursorPos(r.left + x, r.top + y); time.sleep(0.15)
        u32.mouse_event(0x0002, 0, 0, 0, 0); time.sleep(0.08); u32.mouse_event(0x0004, 0, 0, 0, 0)
        time.sleep(int(w) / 1000); continue
    sc = SC[k]; key(sc, False); time.sleep(0.08); key(sc, True)
    time.sleep(int(w or 600) / 1000)
