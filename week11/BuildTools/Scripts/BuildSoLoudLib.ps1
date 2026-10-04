param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectDir,

    [Parameter(Mandatory = $true)]
    [string]$Configuration,

    [Parameter(Mandatory = $true)]
    [string]$Platform
)

$ErrorActionPreference = "Stop"

$ProjectRoot = [System.IO.Path]::GetFullPath($ProjectDir)
$SoLoudRoot = Join-Path $ProjectRoot "ThirdParty\SoLoud"
$SourceRoot = Join-Path $SoLoudRoot "src"
$IncludeRoot = Join-Path $SoLoudRoot "include"
$OutDir = Join-Path $SoLoudRoot "Lib\$Platform\$Configuration"
$ObjDir = Join-Path $ProjectRoot "Intermediate\ThirdParty\SoLoud\$Platform\$Configuration"
$LibPath = Join-Path $OutDir "SoLoud.lib"
$ManifestPath = Join-Path $OutDir "SoLoud.manifest"

$SourceDirs = @(
    "src\core",
    "src\audiosource\wav",
    "src\backend\miniaudio"
)

function Resolve-MsvcToolPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ToolName,

        [Parameter(Mandatory = $true)]
        [string]$Platform
    )

    $ToolCommand = Get-Command $ToolName -ErrorAction SilentlyContinue
    if ($ToolCommand) {
        return $ToolCommand.Source
    }

    $TargetArch = "x64"
    if ($Platform -eq "Win32") {
        $TargetArch = "x86"
    }

    $CandidateDirs = @()
    if ($env:VCToolsInstallDir) {
        $CandidateDirs += Join-Path $env:VCToolsInstallDir "bin\Hostx64\$TargetArch"
        $CandidateDirs += Join-Path $env:VCToolsInstallDir "bin\Hostx86\$TargetArch"
    }

    if ($env:VCINSTALLDIR) {
        $ToolsDir = Join-Path $env:VCINSTALLDIR "Tools\MSVC"
        if (Test-Path $ToolsDir) {
            $LatestToolset = Get-ChildItem -Path $ToolsDir -Directory | Sort-Object Name -Descending | Select-Object -First 1
            if ($LatestToolset) {
                $CandidateDirs += Join-Path $LatestToolset.FullName "bin\Hostx64\$TargetArch"
                $CandidateDirs += Join-Path $LatestToolset.FullName "bin\Hostx86\$TargetArch"
            }
        }
    }

    foreach ($CandidateDir in $CandidateDirs) {
        $CandidatePath = Join-Path $CandidateDir $ToolName
        if (Test-Path $CandidatePath) {
            return $CandidatePath
        }
    }

    return $null
}

function Get-RelativePath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BasePath,

        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $BaseFullPath = [System.IO.Path]::GetFullPath($BasePath)
    if (-not $BaseFullPath.EndsWith([System.IO.Path]::DirectorySeparatorChar)) {
        $BaseFullPath += [System.IO.Path]::DirectorySeparatorChar
    }

    $BaseUri = New-Object System.Uri($BaseFullPath)
    $PathUri = New-Object System.Uri([System.IO.Path]::GetFullPath($Path))
    return [System.Uri]::UnescapeDataString($BaseUri.MakeRelativeUri($PathUri).ToString()).Replace("/", "\")
}

function Get-UniqueFiles {
    param(
        [Parameter(Mandatory = $true)]
        [array]$Files
    )

    $Seen = @{}
    $Result = @()
    foreach ($File in $Files) {
        $FullName = [System.IO.Path]::GetFullPath($File.FullName)
        if (-not $Seen.ContainsKey($FullName)) {
            $Seen[$FullName] = $true
            $Result += $File
        }
    }

    return $Result
}

function New-InputManifest {
    param(
        [Parameter(Mandatory = $true)]
        [array]$Files,

        [Parameter(Mandatory = $true)]
        [array]$BuildSettings
    )

    $Lines = @()
    $Lines += "ManifestVersion=2"
    $Lines += $BuildSettings

    foreach ($File in ($Files | Sort-Object FullName)) {
        $RelativePath = Get-RelativePath -BasePath $ProjectRoot -Path $File.FullName
        $Hash = (Get-FileHash -Path $File.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        $Lines += "Input=$RelativePath|$($File.Length)|$Hash"
    }

    return ($Lines -join "`n")
}

$SourceFiles = @()
foreach ($SourceDir in $SourceDirs) {
    $FullSourceDir = Join-Path $SoLoudRoot $SourceDir
    if (Test-Path $FullSourceDir) {
        $SourceFiles += Get-ChildItem -Path $FullSourceDir -File | Where-Object { $_.Extension -eq ".cpp" -or $_.Extension -eq ".c" }
    }
}

$ClPath = Resolve-MsvcToolPath -ToolName "cl.exe" -Platform $Platform
$LibToolPath = Resolve-MsvcToolPath -ToolName "lib.exe" -Platform $Platform
if (-not $ClPath -or -not $LibToolPath) {
    throw "cl.exe and lib.exe must be available. Install the Visual Studio C++ workload and build through Visual Studio, MSBuild, or a Visual Studio developer command prompt."
}

if ($SourceFiles.Count -eq 0) {
    throw "No SoLoud source files were found under $SourceRoot."
}

$RuntimeFlag = "/MD"
if ($Configuration -eq "Debug") {
    $RuntimeFlag = "/MDd"
}

$Defines = @(
    "/DWITH_MINIAUDIO",
    "/D_CRT_SECURE_NO_WARNINGS",
    "/DNOMINMAX"
)

if ($Configuration -eq "Debug") {
    $Defines += "/D_DEBUG"
}
else {
    $Defines += "/DNDEBUG"
}

$BaseArgs = @(
    "/nologo",
    "/c",
    "/W3",
    "/utf-8",
    "/bigobj",
    $RuntimeFlag,
    "/I$IncludeRoot",
    "/I$SourceRoot\core",
    "/I$SourceRoot\audiosource\wav",
    "/I$SourceRoot\backend\miniaudio",
    "/I$SoLoudRoot"
) + $Defines

$CppArgs = @(
    "/std:c++20",
    "/permissive-",
    "/EHa"
) + $BaseArgs

$CArgs = @(
    "/TC"
) + $BaseArgs

$ManifestBaseArgs = @(
    "/nologo",
    "/c",
    "/W3",
    "/utf-8",
    "/bigobj",
    $RuntimeFlag,
    "/IThirdParty\SoLoud\include",
    "/IThirdParty\SoLoud\src\core",
    "/IThirdParty\SoLoud\src\audiosource\wav",
    "/IThirdParty\SoLoud\src\backend\miniaudio",
    "/IThirdParty\SoLoud"
) + $Defines

$ManifestCppArgs = @(
    "/std:c++20",
    "/permissive-",
    "/EHa"
) + $ManifestBaseArgs

$ManifestCArgs = @(
    "/TC"
) + $ManifestBaseArgs

$InputFiles = @()
$InputFiles += $SourceFiles
$InputFiles += Get-ChildItem -Path $IncludeRoot -Include "*.h", "*.hpp" -Recurse -File
$InputFiles += Get-ChildItem -Path $SourceRoot -Include "*.h", "*.hpp" -Recurse -File
$InputFiles = Get-UniqueFiles -Files $InputFiles

$BuildSettings = @(
    "Library=SoLoud",
    "Configuration=$Configuration",
    "Platform=$Platform",
    "Runtime=$RuntimeFlag",
    "SourceDirs=$($SourceDirs -join ';')",
    "CppArgs=$($ManifestCppArgs -join ';')",
    "CArgs=$($ManifestCArgs -join ';')"
)
$InputManifest = New-InputManifest -Files $InputFiles -BuildSettings $BuildSettings

$NeedsBuild = -not (Test-Path $LibPath)
if (-not $NeedsBuild) {
    if (-not (Test-Path $ManifestPath)) {
        $NeedsBuild = $true
    }
    else {
        $ExistingManifest = (Get-Content -Path $ManifestPath -Raw).Trim()
        if ($ExistingManifest -ne $InputManifest.Trim()) {
            $NeedsBuild = $true
        }
    }
}

if (-not $NeedsBuild) {
    Write-Host "SoLoud.lib is up to date: $LibPath"
    exit 0
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
New-Item -ItemType Directory -Force -Path $ObjDir | Out-Null

$ObjectFiles = @()
foreach ($SourceFile in $SourceFiles) {
    $ObjectFiles += Join-Path $ObjDir ([System.IO.Path]::ChangeExtension($SourceFile.Name, ".obj"))
}

$CppFiles = $SourceFiles | Where-Object { $_.Extension -eq ".cpp" }
$CFiles = $SourceFiles | Where-Object { $_.Extension -eq ".c" }

if ($CppFiles.Count -gt 0) {
    $CompileResponsePath = Join-Path $ObjDir "SoLoud.cpp.cl.rsp"
    $CompileResponse = @()
    $CompileResponse += $CppArgs
    $CompileResponse += "/Fo$ObjDir\"
    foreach ($SourceFile in $CppFiles) {
        $CompileResponse += "`"$($SourceFile.FullName)`""
    }

    Set-Content -Path $CompileResponsePath -Value $CompileResponse -Encoding ASCII

    & $ClPath "@$CompileResponsePath"
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to compile SoLoud C++ static library sources."
    }
}

if ($CFiles.Count -gt 0) {
    $CompileResponsePath = Join-Path $ObjDir "SoLoud.c.cl.rsp"
    $CompileResponse = @()
    $CompileResponse += $CArgs
    $CompileResponse += "/Fo$ObjDir\"
    foreach ($SourceFile in $CFiles) {
        $CompileResponse += "`"$($SourceFile.FullName)`""
    }

    Set-Content -Path $CompileResponsePath -Value $CompileResponse -Encoding ASCII

    & $ClPath "@$CompileResponsePath"
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to compile SoLoud C static library sources."
    }
}

$LibResponsePath = Join-Path $ObjDir "SoLoud.lib.rsp"
$LibResponse = @()
$LibResponse += "/OUT:$LibPath"
foreach ($ObjectFile in $ObjectFiles) {
    $LibResponse += "`"$ObjectFile`""
}

Set-Content -Path $LibResponsePath -Value $LibResponse -Encoding ASCII

& $LibToolPath /nologo "@$LibResponsePath"
if ($LASTEXITCODE -ne 0) {
    throw "Failed to create $LibPath."
}

Set-Content -Path $ManifestPath -Value $InputManifest -Encoding ASCII
Write-Host "Built SoLoud.lib: $LibPath"
