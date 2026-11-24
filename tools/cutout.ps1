# Recorta el emblema + título de "La Máquina del Tiempo" de menu.jpg y le saca el fondo rojo
# (relleno por inundación desde los bordes sobre los píxeles rojos/naranjas/amarillos).
param([string]$In, [string]$Out, [int]$X, [int]$Y, [int]$W, [int]$H)
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing @"
using System; using System.Drawing; using System.Drawing.Imaging; using System.Collections.Generic;
public static class Cut {
  static bool Bg(Color c) { return c.R > 140 && c.B < 120 && c.R >= c.G && (c.R - c.B) > 90; }
  public static Bitmap Run(Bitmap src, Rectangle r) {
    var b = new Bitmap(r.Width, r.Height, PixelFormat.Format32bppArgb);
    using (var g = Graphics.FromImage(b)) g.DrawImage(src, new Rectangle(0, 0, r.Width, r.Height), r, GraphicsUnit.Pixel);
    int w = b.Width, h = b.Height; var px = new Color[w, h]; var bg = new bool[w, h];
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) px[x, y] = b.GetPixel(x, y);
    var q = new Queue<Point>();
    for (int x = 0; x < w; x++) { q.Enqueue(new Point(x, 0)); q.Enqueue(new Point(x, h - 1)); }
    for (int y = 0; y < h; y++) { q.Enqueue(new Point(0, y)); q.Enqueue(new Point(w - 1, y)); }
    while (q.Count > 0) {
      var p = q.Dequeue();
      if (p.X < 0 || p.Y < 0 || p.X >= w || p.Y >= h || bg[p.X, p.Y] || !Bg(px[p.X, p.Y])) continue;
      bg[p.X, p.Y] = true;
      q.Enqueue(new Point(p.X + 1, p.Y)); q.Enqueue(new Point(p.X - 1, p.Y)); q.Enqueue(new Point(p.X, p.Y + 1)); q.Enqueue(new Point(p.X, p.Y - 1));
    }
    // Borde suave: los píxeles que tocan el fondo quedan semitransparentes.
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
      if (bg[x, y]) { b.SetPixel(x, y, Color.Transparent); continue; }
      int n = 0;
      for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        int xx = x + dx, yy = y + dy; if (xx >= 0 && yy >= 0 && xx < w && yy < h && bg[xx, yy]) n++; }
      if (n > 0) { var c = px[x, y]; b.SetPixel(x, y, Color.FromArgb(255 - n * 20, c)); }
    }
    return b;
  }
}
"@
$src = [System.Drawing.Bitmap]::FromFile($In)
$res = [Cut]::Run($src, (New-Object System.Drawing.Rectangle $X, $Y, $W, $H))
$res.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png); "ok"
