# Generate icon.ico for Crosshair (dark rounded bg + green crosshair)
Add-Type -AssemblyName System.Drawing

function New-CrosshairBitmap([int]$size) {
    $bmp = New-Object System.Drawing.Bitmap($size, $size)
    $bmp.MakeTransparent()
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)

    # Rounded-rect dark background
    $pad = [Math]::Max(1, [int]($size * 0.02))
    $rad = [int]($size * 0.22)
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $x = $pad; $y = $pad; $w = $size - 2 * $pad; $h = $size - 2 * $pad
    $d = $rad * 2
    $path.AddArc($x, $y, $d, $d, 180, 90)
    $path.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $path.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
    $path.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $path.CloseFigure()
    $bg = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 24, 25, 29))
    $g.FillPath($bg, $path)

    # Crosshair: green ring + 4 ticks + center dot
    $green = [System.Drawing.Color]::FromArgb(255, 0, 255, 0)
    $penW = [Math]::Max(2, $size / 16.0)
    $pen = New-Object System.Drawing.Pen($green, $penW)
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $cx = $size / 2.0; $cy = $size / 2.0
    $ringR = $size * 0.19
    $g.DrawEllipse($pen, $cx - $ringR, $cy - $ringR, $ringR * 2, $ringR * 2)
    $tickIn = $size * 0.30
    $tickOut = $size * 0.42
    $g.DrawLine($pen, $cx, $cy - $tickIn, $cx, $cy - $tickOut)
    $g.DrawLine($pen, $cx, $cy + $tickIn, $cx, $cy + $tickOut)
    $g.DrawLine($pen, $cx - $tickIn, $cy, $cx - $tickOut, $cy)
    $g.DrawLine($pen, $cx + $tickIn, $cy, $cx + $tickOut, $cy)
    $dotR = [Math]::Max(1.5, $size * 0.045)
    $dotBrush = New-Object System.Drawing.SolidBrush($green)
    $g.FillEllipse($dotBrush, $cx - $dotR, $cy - $dotR, $dotR * 2, $dotR * 2)

    $g.Dispose()
    return $bmp
}

$icoPath = Join-Path $PSScriptRoot "icon.ico"
$stream = [System.IO.File]::Create($icoPath)
$writer = New-Object System.IO.BinaryWriter($stream)

$sizes = @(16, 24, 32, 48, 64, 128, 256)
$pngDataList = @()
foreach ($s in $sizes) {
    $bmp = New-CrosshairBitmap $s
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngDataList += , $ms.ToArray()
    $ms.Dispose()
    $bmp.Dispose()
}

# ICONDIR header
$writer.Write([UInt16]0)          # reserved
$writer.Write([UInt16]1)          # type = icon
$writer.Write([UInt16]$sizes.Count)

$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]
    $data = $pngDataList[$i]
    $writer.Write([Byte]($(if ($s -ge 256) { 0 } else { $s })))  # width
    $writer.Write([Byte]($(if ($s -ge 256) { 0 } else { $s })))  # height
    $writer.Write([Byte]0)        # palette
    $writer.Write([Byte]0)        # reserved
    $writer.Write([UInt16]1)      # planes
    $writer.Write([UInt16]32)     # bpp
    $writer.Write([UInt32]$data.Length)
    $writer.Write([UInt32]$offset)
    $offset += $data.Length
}
foreach ($data in $pngDataList) { $writer.Write($data) }

$writer.Close()
$stream.Close()
Write-Host "Created $icoPath"
