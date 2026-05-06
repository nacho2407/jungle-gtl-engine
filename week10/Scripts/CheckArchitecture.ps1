param(
    [switch]$FailOnViolation
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$SourceRoot = Join-Path $Root "NipsEngine/Source"
$EngineRoot = Join-Path $SourceRoot "Engine"
$EditorEngineHeader = Join-Path $SourceRoot "Editor/EditorEngine.h"

if (!(Test-Path $SourceRoot)) {
    Write-Error "Source root not found: $SourceRoot"
}

$Violations = New-Object System.Collections.Generic.List[string]

function Add-Violation {
    param([string]$Message)
    $script:Violations.Add($Message) | Out-Null
}

function Get-SourceFiles {
    param([string]$Path)
    Get-ChildItem -Path $Path -Recurse -File -Include *.h,*.cpp
}

Write-Host "== Architecture Check =="
Write-Host "Root: $Root"

Write-Host ""
Write-Host "-- Engine -> Editor includes --"
$EngineEditorIncludes = Get-SourceFiles $EngineRoot | Select-String -Pattern '#include\s+"Editor/' -ErrorAction SilentlyContinue
if ($EngineEditorIncludes) {
    foreach ($Match in $EngineEditorIncludes) {
        $Relative = Resolve-Path -Path $Match.Path -Relative
        $Line = $Match.Line.Trim()
        $Message = "${Relative}:$($Match.LineNumber): $Line"
        Write-Host $Message
        Add-Violation "Engine includes Editor: $Message"
    }
}
else {
    Write-Host "OK"
}

Write-Host ""
Write-Host "-- Legacy EditorEngine undo wrappers --"
$LegacyUndoPatterns = @(
    'bool\s+CaptureUndoSnapshot\s*\(',
    'bool\s+Undo\s*\(',
    'bool\s+Redo\s*\(',
    'bool\s+RestoreUndoHistoryIndex\s*\(',
    'void\s+ClearUndoHistory\s*\('
)

if (Test-Path $EditorEngineHeader) {
    $HeaderText = Get-Content -Path $EditorEngineHeader
    foreach ($Pattern in $LegacyUndoPatterns) {
        $Matches = $HeaderText | Select-String -Pattern $Pattern
        foreach ($Match in $Matches) {
            $Relative = Resolve-Path -Path $EditorEngineHeader -Relative
            $Line = $Match.Line.Trim()
            $Message = "${Relative}:$($Match.LineNumber): $Line"
            Write-Host $Message
            Add-Violation "Legacy EditorEngine undo API: $Message"
        }
    }

    if ($Violations | Where-Object { $_ -like "Legacy EditorEngine undo API:*" }) {
        # Already printed above.
    }
    else {
        Write-Host "OK"
    }
}
else {
    Add-Violation "EditorEngine.h not found: $EditorEngineHeader"
}

Write-Host ""
Write-Host "-- Summary --"
Write-Host ("Violations: {0}" -f $Violations.Count)

if ($Violations.Count -gt 0) {
    Write-Host ""
    Write-Host "Known violations are allowed during transition unless -FailOnViolation is passed."
}

if ($FailOnViolation -and $Violations.Count -gt 0) {
    exit 1
}

exit 0
