param(
    [Parameter(Mandatory = $true)][string]$ArchivePath,
    [string]$OutputDirectory = (Join-Path (Get-Location) 'MotoGP17-Gauge-Export'),
    [string]$Pattern = '(?i)(gauge|gfx[_\\/ -]?race)'
)

# Read-only extraction of selected resources from the supplied MotoGP 17 PC
# MIX variant. The original archive is never modified. Run beside gfxpak.mix:
# powershell -NoProfile -ExecutionPolicy Bypass -File .\extract-motogp17-gauge.ps1 -ArchivePath .\gfxpak.mix
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$source = (Resolve-Path -LiteralPath $ArchivePath).Path
$destination = [IO.Path]::GetFullPath($OutputDirectory)
$zip = $destination + '.zip'
if ((Test-Path -LiteralPath $destination) -or (Test-Path -LiteralPath $zip)) {
    throw 'Choose a new OutputDirectory; existing exports are preserved.'
}
$inputStream = [IO.File]::OpenRead($source)
$reader = [IO.BinaryReader]::new($inputStream)
$entries = [Collections.Generic.List[object]]::new()
try {
    $version = $reader.ReadUInt32()
    $count = $reader.ReadUInt32()
    $table = $reader.ReadUInt32()
    $variant = $reader.ReadUInt32()
    if ($version -ne 1 -or $variant -ne 1 -or $count -gt 100000 -or $table -lt 32 -or $table -ge $inputStream.Length) {
        throw 'This is not the observed MotoGP 17 PC MIX variant.'
    }
    $inputStream.Position = $table
    for ($index = 0; $index -lt $count; ++$index) {
        $length = $reader.ReadUInt32()
        if ($length -lt 1 -or $length -gt 4096) { throw 'Invalid MIX filename length.' }
        $bytes = $reader.ReadBytes($length)
        if ($bytes.Length -ne $length) { throw 'Truncated MIX filename.' }
        for ($i = 0; $i -lt $bytes.Length; ++$i) {
            $bytes[$i] = $bytes[$i] -bxor (($i * 58) -band 255)
        }
        $name = [Text.Encoding]::ASCII.GetString($bytes).TrimEnd([char]0)
        $entry = [pscustomobject]@{
            Index = $index; Name = $name
            Offset = $reader.ReadUInt32(); PackedBytes = $reader.ReadUInt32()
            Bytes = $reader.ReadUInt32(); Compression = $reader.ReadByte()
            External = $reader.ReadByte()
        }
        $null = $reader.ReadUInt64() # Original file timestamp; never used as a path.
        if ($entry.External -eq 0 -and ([uint64]$entry.Offset + $entry.PackedBytes -gt $table)) {
            throw "Invalid data extent: $name"
        }
        $entries.Add($entry)
    }
    $selected = @($entries | Where-Object { $_.Name -match $Pattern })
    $total = [uint64]0
    foreach ($entry in $selected) {
        if ($entry.External -ne 0 -or $entry.Compression -gt 1 -or $entry.Bytes -gt 67108864 -or $entry.PackedBytes -gt 67108864) {
            throw "Unsupported or oversized selected resource: $($entry.Name)"
        }
        $total += $entry.Bytes
    }
    if ($total -gt 134217728) { throw 'Selected resources exceed 128 MiB; use a more specific Pattern.' }
    $null = [IO.Directory]::CreateDirectory($destination)
    $entries | Export-Csv -LiteralPath (Join-Path $destination 'mix-inventory.csv') -NoTypeInformation -Encoding UTF8
    $hashes = [Collections.Generic.List[object]]::new()
    foreach ($entry in $selected) {
        $relative = $entry.Name.Replace('\', '/')
        if ([IO.Path]::IsPathRooted($relative) -or $relative.Contains(':') -or @($relative.Split('/') | Where-Object { $_ -eq '..' -or $_ -eq '' }).Count) {
            throw "Unsafe resource path: $relative"
        }
        $target = [IO.Path]::GetFullPath((Join-Path $destination $relative))
        $prefix = $destination.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
        if (-not $target.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw 'Resource escaped export directory.' }
        $null = [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))
        $inputStream.Position = $entry.Offset
        $packed = $reader.ReadBytes($entry.PackedBytes)
        if ($packed.Length -ne $entry.PackedBytes) { throw "Truncated resource: $relative" }
        $memory = [IO.MemoryStream]::new($packed, 0, $packed.Length, $false)
        $decoded = $memory
        $output = $null
        try {
            if ($entry.Compression -eq 1) {
                $decoded = [IO.Compression.DeflateStream]::new($memory, [IO.Compression.CompressionMode]::Decompress, $true)
            }
            $output = [IO.File]::Open($target, [IO.FileMode]::CreateNew)
            $buffer = [byte[]]::new(32768)
            $written = [uint64]0
            while (($read = $decoded.Read($buffer, 0, $buffer.Length)) -gt 0) {
                $written += $read
                if ($written -gt $entry.Bytes) { throw "Decoded size exceeds declaration: $relative" }
                $output.Write($buffer, 0, $read)
            }
            if ($written -ne $entry.Bytes) { throw "Decoded size mismatch: $relative" }
        } finally {
            if ($output) { $output.Dispose() }
            if ($decoded -ne $memory) { $decoded.Dispose() }
            $memory.Dispose()
        }
        $hashes.Add([pscustomobject]@{ Name = $entry.Name; Bytes = $entry.Bytes; SHA256 = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant() })
    }
    [pscustomobject]@{
        Archive = [IO.Path]::GetFileName($source)
        ArchiveSHA256 = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
        Pattern = $Pattern; ArchiveEntries = $count; SelectedEntries = $selected.Count
        Files = @($hashes.ToArray())
    } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $destination 'export-manifest.json') -Encoding UTF8
} finally {
    $reader.Dispose()
    $inputStream.Dispose()
}
[IO.Compression.ZipFile]::CreateFromDirectory($destination, $zip, [IO.Compression.CompressionLevel]::Optimal, $false)
Write-Host "Exported $($selected.Count) resources: $zip"
if ((Get-Item -LiteralPath $zip).Length -gt 26214400) {
    Write-Host 'ZIP exceeds 25 MiB. Share mix-inventory.csv first so the selection can be narrowed.'
}
