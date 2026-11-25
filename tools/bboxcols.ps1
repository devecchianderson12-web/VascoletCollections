# Caja de píxeles visibles (alfa) dentro de franjas de columnas: -Ranges "0-220,220-442,442-1024"
param([string]$In, [string]$Ranges)
Add-Type -AssemblyName System.Drawing
$b = [System.Drawing.Bitmap]::FromFile($In)
foreach ($r in $Ranges.Split(',')) {
  $a = $r.Split('-'); $x0 = [int]$a[0]; $x1 = [Math]::Min([int]$a[1], $b.Width)
  $minx=99999;$miny=99999;$maxx=0;$maxy=0
  for ($y = 0; $y -lt $b.Height; $y += 2) { for ($x = $x0; $x -lt $x1; $x += 2) { if ($b.GetPixel($x,$y).A -gt 40) { if($x -lt $minx){$minx=$x}; if($y -lt $miny){$miny=$y}; if($x -gt $maxx){$maxx=$x}; if($y -gt $maxy){$maxy=$y} } } }
  "$r : $minx,$miny  ${maxx},$maxy  ($($maxx-$minx+2)x$($maxy-$miny+2))"
}
