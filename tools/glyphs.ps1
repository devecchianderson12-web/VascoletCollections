# Busca las letras en una planilla de fuente (filas y columnas separadas por espacio transparente).
param([string]$In, [int]$Gap = 3)
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing @"
using System; using System.Drawing; using System.Collections.Generic; using System.Text;
public static class Gl {
  public static string Run(Bitmap b, int gap) {
    int w = b.Width, h = b.Height; var op = new bool[w, h];
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) op[x, y] = b.GetPixel(x, y).A > 24;
    var sb = new StringBuilder(); int y0 = -1;
    for (int y = 0; y <= h; y++) {
      bool any = false; if (y < h) for (int x = 0; x < w && !any; x++) any = op[x, y];
      if (any && y0 < 0) y0 = y;
      if (!any && y0 >= 0) {
        int y1 = y; int x0 = -1, empty = 0; var row = new List<string>();
        for (int x = 0; x <= w; x++) {
          bool col = false; if (x < w) for (int yy = y0; yy < y1 && !col; yy++) col = op[x, yy];
          if (col) { if (x0 < 0) x0 = x; empty = 0; }
          else if (x0 >= 0 && ++empty >= gap) { row.Add((x0) + "," + (x - empty + 1 - x0)); x0 = -1; empty = 0; }
        }
        sb.AppendLine("ROW " + y0 + " " + (y1 - y0) + " n=" + row.Count + " : " + string.Join(" ", row));
        y0 = -1;
      }
    }
    return sb.ToString();
  }
}
"@
$b = [System.Drawing.Bitmap]::FromFile($In); [Gl]::Run($b, $Gap)
