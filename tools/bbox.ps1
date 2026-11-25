# Cajas de los objetos no blancos en cada cuarto de la imagen.
param([string]$In)
Add-Type -AssemblyName System.Drawing
$b = [System.Drawing.Bitmap]::FromFile($In); $W = $b.Width; $H = $b.Height
foreach ($q in @(@(0,0),@(1,0),@(0,1),@(1,1))) {
  $x0=[int]($q[0]*$W/2); $y0=[int]($q[1]*$H/2); $minx=99999;$miny=99999;$maxx=0;$maxy=0
  for ($y=$y0; $y -lt $y0+$H/2; $y+=2) { for ($x=$x0; $x -lt $x0+$W/2; $x+=2) { $c=$b.GetPixel($x,$y); if ($c.R+$c.G+$c.B -lt 600) { if($x -lt $minx){$minx=$x}; if($y -lt $miny){$miny=$y}; if($x -gt $maxx){$maxx=$x}; if($y -gt $maxy){$maxy=$y} } } }
  "$($q -join ',') : x $minx-$maxx y $miny-$maxy  centro $([int](($minx+$maxx)/2)),$([int](($miny+$maxy)/2)) r $([int](($maxx-$minx)/2))"
}
