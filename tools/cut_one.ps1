# Recorta un botón redondo de una imagen con fondo blanco (círculo con borde suave).
param([string]$In, [string]$Out, [int]$X, [int]$Y, [int]$W, [int]$H, [float]$R)
Add-Type -AssemblyName System.Drawing
$src = [System.Drawing.Bitmap]::FromFile($In)
$b = New-Object System.Drawing.Bitmap $W, $H, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($b); $g.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, $W, $H), (New-Object System.Drawing.Rectangle $X, $Y, $W, $H), [System.Drawing.GraphicsUnit]::Pixel); $g.Dispose()
$cx = $W / 2.0; $cy = $H / 2.0
for ($yy = 0; $yy -lt $H; $yy++) { for ($xx = 0; $xx -lt $W; $xx++) {
  $d = [Math]::Sqrt(($xx - $cx) * ($xx - $cx) + ($yy - $cy) * ($yy - $cy))
  if ($d -gt $R + 1) { $b.SetPixel($xx, $yy, [System.Drawing.Color]::Transparent) }
  elseif ($d -gt $R) { $p = $b.GetPixel($xx, $yy); $b.SetPixel($xx, $yy, [System.Drawing.Color]::FromArgb([int](255 * ($R + 1 - $d)), $p)) }
} }
$b.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png); "ok"
