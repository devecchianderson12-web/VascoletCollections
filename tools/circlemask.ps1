# Deja solo un círculo de la imagen (borde suave); lo de afuera queda transparente.
param([string]$File, [float]$Cx, [float]$Cy, [float]$R)
Add-Type -AssemblyName System.Drawing
$b = [System.Drawing.Bitmap]::FromFile($File); $c = New-Object System.Drawing.Bitmap $b; $b.Dispose()
for ($y = 0; $y -lt $c.Height; $y++) { for ($x = 0; $x -lt $c.Width; $x++) {
  $d = [Math]::Sqrt(($x - $Cx) * ($x - $Cx) + ($y - $Cy) * ($y - $Cy))
  if ($d -gt $R + 1) { $c.SetPixel($x, $y, [System.Drawing.Color]::Transparent) }
  elseif ($d -gt $R) { $p = $c.GetPixel($x, $y); $c.SetPixel($x, $y, [System.Drawing.Color]::FromArgb([int]($p.A * ($R + 1 - $d)), $p)) }
} }
$c.Save($File, [System.Drawing.Imaging.ImageFormat]::Png); "ok"
