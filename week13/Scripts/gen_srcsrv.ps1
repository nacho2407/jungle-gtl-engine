param(
    [string]$Project,
    [string]$SourceServer,
    [string]$RawList,
    [string]$SrcInfo
)

$lines    = Get-Content $RawList
$mappings = @()

# PDB에 박힌 실제 루트 경로를 raw_files.txt에서 동적으로 추출
$anchor = $null
foreach ($line in $lines) {
    $idx = $line.ToLower().IndexOf("\$Project\".ToLower())
    if ($idx -ge 0) {
        $anchor = $line.Substring(0, $idx + $Project.Length + 2)
        Write-Host "[INFO] Detected anchor from PDB: $anchor"
        break
    }
}

if (-not $anchor) {
    Write-Host "[ERROR] Could not detect project root from PDB source paths."
    exit 1
}

foreach ($line in $lines) {
    if ($line -match 'source files') { continue }
    if ($line -match '\.(pdb|exe|obj|lib|dll)$') { continue }

    $idx = $line.ToLower().IndexOf($anchor.ToLower())
    if ($idx -ge 0) {
        $rel = $line.Substring($idx + $anchor.Length)
        $mappings += ($line.Trim() + '*' + $rel)
    }
}

$header = @(
    'SRCSRV: ini ------------------------------------------------',
    'VERSION=2',
    'INDEXVERSION=2',
    'VERCTRL=fs',
    'SRCSRV: variables ------------------------------------------',
    "SOURCE_SHARE=$SourceServer\$Project",
    'SRCSRVTRG=%SOURCE_SHARE%\%var2%',
    'SRCSRVCMD=cmd /c copy /Y "%SRCSRVTRG%" "%targ%"',
    'SRCSRV: source files ---------------------------------------'
)
$footer = @('SRCSRV: end ------------------------------------------------')

# UTF8 without BOM
$utf8NoBom = New-Object System.Text.UTF8Encoding $false
[System.IO.File]::WriteAllLines($SrcInfo, ($header + $mappings + $footer), $utf8NoBom)

Write-Host ("[INFO] Indexed " + $mappings.Count + " source files.")

Write-Host "--- srcsrv.txt preview (first 15 lines) ---"
Get-Content $SrcInfo | Select-Object -First 15 | ForEach-Object { Write-Host $_ }
Write-Host "--------------------------------------------"

if ($mappings.Count -eq 0) { exit 1 }