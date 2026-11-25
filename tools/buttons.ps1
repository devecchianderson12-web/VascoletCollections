# Recorta los botones táctiles de la imagen del usuario (fondo blanco) y les quita el fondo.
param([string]$In, [string]$OutDir)
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing @"
using System; using System.Drawing; using System.Drawing.Imaging; using System.Collections.Generic;
public static class Btn {
  // Recorte con relleno desde los bordes: los píxeles casi blancos conectados al borde quedan transparentes.
  public static Bitmap Cut(Bitmap src, Rectangle r) {
    var b = new Bitmap(r.Width, r.Height, PixelFormat.Format32bppArgb);
    using (var g = Graphics.FromImage(b)) g.DrawImage(src, new Rectangle(0, 0, r.Width, r.Height), r, GraphicsUnit.Pixel);
    int w = b.Width, h = b.Height; var bg = new bool[w, h]; var q = new Queue<Point>();
    for (int x = 0; x < w; x++) { q.Enqueue(new Point(x, 0)); q.Enqueue(new Point(x, h - 1)); }
    for (int y = 0; y < h; y++) { q.Enqueue(new Point(0, y)); q.Enqueue(new Point(w - 1, y)); }
    while (q.Count > 0) {
      var p = q.Dequeue();
      if (p.X < 0 || p.Y < 0 || p.X >= w || p.Y >= h || bg[p.X, p.Y]) continue;
      var c = b.GetPixel(p.X, p.Y);
      if (Math.Min(c.R, Math.Min(c.G, c.B)) < 225) continue;
      bg[p.X, p.Y] = true;
      q.Enqueue(new Point(p.X + 1, p.Y)); q.Enqueue(new Point(p.X - 1, p.Y)); q.Enqueue(new Point(p.X, p.Y + 1)); q.Enqueue(new Point(p.X, p.Y - 1));
    }
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
      var c = b.GetPixel(x, y);
      if (bg[x, y]) { b.SetPixel(x, y, Color.Transparent); continue; }
      int n = 0;
      for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) { int xx = x + dx, yy = y + dy; if (xx >= 0 && yy >= 0 && xx < w && yy < h && bg[xx, yy]) n++; }
      if (n > 0) b.SetPixel(x, y, Color.FromArgb(Math.Max(60, 255 - n * 28), c));
    }
    return b;
  }
  // Pinta un círculo con un color (para tapar la perilla en la base del joystick).
  public static void Disk(Bitmap b, float cx, float cy, float r, Color c) {
    for (int y = 0; y < b.Height; y++) for (int x = 0; x < b.Width; x++) {
      float d = (float)Math.Sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
      if (d <= r) b.SetPixel(x, y, c);
      else if (d <= r + 1.5f) { var o = b.GetPixel(x, y); float k = (d - r) / 1.5f; b.SetPixel(x, y, Color.FromArgb(255, (int)(c.R * (1 - k) + o.R * k), (int)(c.G * (1 - k) + o.G * k), (int)(c.B * (1 - k) + o.B * k))); }
    }
  }
}
"@
$src = [System.Drawing.Bitmap]::FromFile($In)
function Save($name, $x, $y, $w, $h) { $b = [Btn]::Cut($src, (New-Object System.Drawing.Rectangle $x, $y, $w, $h)); $b.Save("$OutDir\$name", [System.Drawing.Imaging.ImageFormat]::Png); $b }
(Save "touch_back.png" 0 40 96 96).Dispose()
(Save "touch_pause.png" 1750 40 100 96).Dispose()
(Save "touch_jump.png" 1684 430 152 142).Dispose()
(Save "touch_knob.png" 116 616 130 124).Dispose()
$base = Save "touch_base.png" 38 538 284 280
# La base sin la perilla: se pinta el círculo oscuro del medio.
$mid = $base.GetPixel(70, 140)
[Btn]::Disk($base, 143, 140, 66, $mid)
$base.Save("$OutDir\touch_base.png", [System.Drawing.Imaging.ImageFormat]::Png); $base.Dispose()
"mid color $mid"
