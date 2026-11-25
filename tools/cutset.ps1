# Recorta un juego de botones táctiles (4 círculos: atrás, pausa, joystick, salto) de una imagen con fondo blanco.
# El joystick se separa en base (sin perilla) y perilla.
param([string]$In, [string]$Name, [string]$OutDir, [string]$Spec)
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing @"
using System; using System.Drawing; using System.Drawing.Imaging;
public static class Cs {
  public static Bitmap Circle(Bitmap src, float cx, float cy, float r) {
    int s = (int)Math.Ceiling(r * 2 + 4); var b = new Bitmap(s, s, PixelFormat.Format32bppArgb);
    float ox = cx - s / 2f, oy = cy - s / 2f;
    for (int y = 0; y < s; y++) for (int x = 0; x < s; x++) {
      float sx = ox + x, sy = oy + y, d = (float)Math.Sqrt((sx - cx) * (sx - cx) + (sy - cy) * (sy - cy));
      if (d > r + 1 || sx < 0 || sy < 0 || sx >= src.Width || sy >= src.Height) continue;
      var c = src.GetPixel((int)sx, (int)sy);
      b.SetPixel(x, y, d > r ? Color.FromArgb((int)(255 * (r + 1 - d)), c) : c);
    }
    return b;
  }
  public static void Disk(Bitmap src, float cx, float cy, float r, Color c) {
    for (int y = (int)(cy - r - 2); y <= cy + r + 2; y++) for (int x = (int)(cx - r - 2); x <= cx + r + 2; x++) {
      float d = (float)Math.Sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
      if (d <= r) src.SetPixel(x, y, c);
      else if (d <= r + 1.5f) { var o = src.GetPixel(x, y); float k = (d - r) / 1.5f; src.SetPixel(x, y, Color.FromArgb(255, (int)(c.R * (1 - k) + o.R * k), (int)(c.G * (1 - k) + o.G * k), (int)(c.B * (1 - k) + o.B * k))); }
    }
  }
}
"@
$src = [System.Drawing.Bitmap]::FromFile($In)
$p = $Spec.Split(',') | ForEach-Object { [float]$_ }
# Spec: back cx,cy,r  pause cx,cy,r  joy cx,cy,r  jump cx,cy,r  knob r  fillSampleDist
function Out($bmp, $part) { $bmp.Save("$OutDir\touch_$($Name)_$part.png", [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose() }
Out ([Cs]::Circle($src, $p[0], $p[1], $p[2])) "back"
Out ([Cs]::Circle($src, $p[3], $p[4], $p[5])) "pause"
Out ([Cs]::Circle($src, $p[9], $p[10], $p[11])) "jump"
Out ([Cs]::Circle($src, $p[6], $p[7], $p[12])) "knob"
$fill = $src.GetPixel([int]($p[6] + $p[13]), [int]$p[7])
$copy = New-Object System.Drawing.Bitmap $src
[Cs]::Disk($copy, $p[6], $p[7], $p[12] + 3, $fill)
Out ([Cs]::Circle($copy, $p[6], $p[7], $p[8])) "base"
"$Name ok (relleno $fill)"
