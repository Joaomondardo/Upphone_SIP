<#
.SYNOPSIS
    Build do Upphone SIP (32 e 64 bits), com assinatura digital e instalador opcionais.

.DESCRIPTION
    Etapas, para cada plataforma pedida:
      1. bcg729 (G.729) e pjproject (libpjproject Release-Static)
      2. SQLite + SQLiteCpp (bibliotecas estaticas)
         (versoes fixadas em pins.ps1)
      3. Upphone.exe
      4. Copia para dist\<versao>\<plataforma>\ (versao portatil)
      5. Assinatura digital (se houver certificado)
      6. Instalador Inno Setup (se -Installer e o ISCC.exe existir)

.EXAMPLE
    .\build.ps1
    .\build.ps1 -Platforms x64 -Installer
    .\build.ps1 -Installer -PfxPath C:\certs\upp.pfx -PfxPassword (Read-Host -AsSecureString)
    .\build.ps1 -Installer -CertThumbprint 0123456789ABCDEF0123456789ABCDEF01234567
#>
param(
    [ValidateSet('Win32', 'x64')]
    [string[]]$Platforms = @('Win32', 'x64'),
    [switch]$Installer,
    [switch]$SkipPjproject,
    # aceita um pjproject em commit diferente do fixado em pins.ps1 (nao use para releases)
    [switch]$AllowOtherPjproject,
    # Assinatura: use um .pfx OU a impressao digital de um certificado instalado (inclusive token USB EV)
    [string]$PfxPath,
    [SecureString]$PfxPassword,
    [string]$CertThumbprint,
    [string]$TimestampUrl = 'http://timestamp.digicert.com'
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'pins.ps1')

$AppDir  = Split-Path -Parent $PSScriptRoot          # ...\pjproject\microsip
$PjDir   = Split-Path -Parent $AppDir                 # ...\pjproject
$TpDir   = Join-Path $PjDir 'third_party'
$DistDir = Join-Path $AppDir 'dist'

function Step($text) { Write-Host "`n==> $text" -ForegroundColor Cyan }

# ---------------------------------------------------------------- toolchain
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere.exe nao encontrado. Instale o Visual Studio Build Tools." }
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.ATLMFC -property installationPath
if (-not $vsPath) { throw "Nenhum Visual Studio com C++ e MFC encontrado (componente Microsoft.VisualStudio.Component.VC.ATLMFC)." }
$env:PATH = "$env:PATH;$(Split-Path $vswhere)"   # vcvarsall.bat precisa do vswhere no PATH
$msbuild = Join-Path $vsPath 'MSBuild\Current\Bin\MSBuild.exe'
$vcvars  = Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'

$toolset = $null
foreach ($vcDir in (Get-ChildItem (Join-Path $vsPath 'MSBuild\Microsoft\VC') -Directory | Sort-Object Name -Descending)) {
    $tsDir = Join-Path $vcDir.FullName 'Platforms\Win32\PlatformToolsets'
    if (Test-Path $tsDir) {
        $toolset = (Get-ChildItem $tsDir -Directory | Where-Object { $_.Name -match '^v\d+$' } |
            Sort-Object Name -Descending | Select-Object -First 1).Name
        if ($toolset) { break }
    }
}
if (-not $toolset) { throw "Nenhum toolset C++ (v14x) encontrado em $vsPath" }
$sdk = (Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\Include" -Directory |
    Where-Object { $_.Name -match '^10\.' } | Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1).Name

$version = ([regex]::Match((Get-Content (Join-Path $AppDir 'const.h') -Raw), '_GLOBAL_VERSION "([^"]+)"')).Groups[1].Value

Write-Host "Visual Studio : $vsPath"
Write-Host "Toolset / SDK : $toolset / $sdk"
Write-Host "Versao        : $version"

$msbuildCommon = @('-m', '-v:m', '-nologo', "-p:BuildToolset=$toolset", "-p:PlatformVersion=$sdk")

function Invoke-MSBuild($project, $configuration, $platform) {
    & $msbuild $project "-p:Configuration=$configuration" "-p:Platform=$platform" @msbuildCommon
    if ($LASTEXITCODE -ne 0) { throw "Falha no build de $project ($configuration|$platform)" }
}

# ---------------------------------------------------------------- pre-requisitos
if (-not (Test-Path (Join-Path $PjDir 'pjsip-apps\build\libpjproject.vcxproj'))) {
    throw "Este repositorio deve estar dentro do pjproject, como <pjproject>\microsip. Use build\setup.ps1 (veja o README)."
}
$pjHead = Get-GitHead $PjDir
if (-not $pjHead) {
    Write-Warning "pjproject em $PjDir nao e um clone git: nao da para conferir se esta no commit fixado ($($Pins.pjproject.Commit)). Para um build reproduzivel use build\setup.ps1."
}
elseif ($pjHead -ne $Pins.pjproject.Commit) {
    $msg = "pjproject esta no commit $pjHead, mas o fixado em pins.ps1 e $($Pins.pjproject.Commit)."
    if ($AllowOtherPjproject) { Write-Warning $msg }
    else { throw "$msg Rode 'git -C $PjDir checkout $($Pins.pjproject.Commit)' ou use -AllowOtherPjproject." }
}
Write-Host "pjproject     : $(if ($pjHead) { $pjHead } else { 'sem git' })"
# config_site.h versionado neste repositorio -> pjproject
$configSite = Join-Path $PjDir 'pjlib\include\pj\config_site.h'
$configSiteRepo = Join-Path $PSScriptRoot 'pjproject\config_site.h'
if (-not (Test-Path $configSite) -or ((Get-FileHash $configSite).Hash -ne (Get-FileHash $configSiteRepo).Hash)) {
    Step 'Aplicando config_site.h no pjproject'
    Copy-Item $configSiteRepo $configSite -Force
}
# Correcoes do Upphone no pjproject fixado (build\pjproject\*.patch), aplicadas uma vez
foreach ($patch in Get-ChildItem (Join-Path $PSScriptRoot 'pjproject\*.patch') | Sort-Object Name) {
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    & git -C $PjDir apply --reverse --check $patch.FullName 2>$null
    $applied = ($LASTEXITCODE -eq 0)
    $ErrorActionPreference = $prev
    if (-not $applied) {
        Step "Aplicando $($patch.Name) no pjproject"
        Invoke-Git -C $PjDir apply $patch.FullName | Out-Null
    }
}

Step 'Dependencias (versoes fixas em pins.ps1)'
$sqliteSrc = Join-Path $TpDir 'SQLiteCpp-src'
Sync-PinnedRepo $Pins.SQLiteCpp.Url $sqliteSrc $Pins.SQLiteCpp.Commit

function Build-SQLite($platform) {
    $arch = if ($platform -eq 'x64') { 'x64' } else { 'x86' }
    $out  = Join-Path $TpDir "SQLiteCpp\$platform\Release"
    $obj  = Join-Path $env:TEMP "upphone_sqlitecpp_$platform"
    New-Item -ItemType Directory -Force $out, $obj | Out-Null
    Copy-Item (Join-Path $sqliteSrc 'include\SQLiteCpp') (Join-Path $TpDir 'SQLiteCpp') -Recurse -Force
    Copy-Item (Join-Path $sqliteSrc 'sqlite3\sqlite3.h') (Join-Path $TpDir 'SQLiteCpp\sqlite3.h') -Force
    $bat = Join-Path $obj 'build.bat'
    @"
@echo off
call "$vcvars" $arch >nul || exit /b 1
cd /d "$obj"
cl /nologo /c /O2 /MT /DNDEBUG /DSQLITE_ENABLE_COLUMN_METADATA "$sqliteSrc\sqlite3\sqlite3.c" || exit /b 1
lib /nologo /OUT:"$out\sqlite3.lib" sqlite3.obj || exit /b 1
cl /nologo /c /O2 /MT /EHsc /std:c++17 /DNDEBUG /DSQLITE_ENABLE_COLUMN_METADATA /I"$sqliteSrc\include" /I"$sqliteSrc\sqlite3" "$sqliteSrc\src\*.cpp" || exit /b 1
lib /nologo /OUT:"$out\SQLiteCpp.lib" Backup.obj Column.obj Database.obj Exception.obj Savepoint.obj Statement.obj Transaction.obj || exit /b 1
"@ | Set-Content $bat -Encoding ASCII
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    cmd /c "`"$bat`" 2>&1" | Out-Host
    $code = $LASTEXITCODE
    $ErrorActionPreference = $prev
    if ($code -ne 0) { throw "Falha no build do SQLite ($platform)" }
}

$bcgSrc = Join-Path $TpDir 'bcg729-src'
Sync-PinnedRepo $Pins.bcg729.Url $bcgSrc $Pins.bcg729.Commit

function Build-Bcg729($platform) {
    $arch = if ($platform -eq 'x64') { 'x64' } else { 'x86' }
    $out  = Join-Path $TpDir "bcg729\$platform"
    $obj  = Join-Path $env:TEMP "upphone_bcg729_$platform"
    New-Item -ItemType Directory -Force $out, $obj | Out-Null
    # pjmedia inclui <bcg729/encoder.h> a partir de third_party
    Copy-Item (Join-Path $bcgSrc 'include\bcg729\*.h') (Join-Path $TpDir 'bcg729') -Force
    $bat = Join-Path $obj 'build.bat'
    @"
@echo off
call "$vcvars" $arch >nul || exit /b 1
cd /d "$obj"
del /q *.obj 2>nul
cl /nologo /c /O2 /MT /DNDEBUG /DBCG729_STATIC /I"$bcgSrc\include" /I"$bcgSrc\src" "$bcgSrc\src\*.c" || exit /b 1
lib /nologo /OUT:"$out\bcg729.lib" *.obj || exit /b 1
"@ | Set-Content $bat -Encoding ASCII
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    cmd /c "`"$bat`" 2>&1" | Out-Null
    $code = $LASTEXITCODE
    $ErrorActionPreference = $prev
    if ($code -ne 0) { throw "Falha no build do bcg729 ($platform)" }
}

# ---------------------------------------------------------------- assinatura
$signtool = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\signtool.exe" -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Select-Object -First 1
$doSign = [bool]($PfxPath -or $CertThumbprint)

function Sign-File($file) {
    if (-not $doSign) { return }
    if (-not $signtool) { throw 'signtool.exe nao encontrado (Windows SDK).' }
    $signArgs = @('sign', '/fd', 'SHA256', '/tr', $TimestampUrl, '/td', 'SHA256', '/d', 'Upphone SIP')
    if ($PfxPath) {
        $signArgs += @('/f', $PfxPath)
        if ($PfxPassword) {
            $plain = [Runtime.InteropServices.Marshal]::PtrToStringAuto([Runtime.InteropServices.Marshal]::SecureStringToBSTR($PfxPassword))
            $signArgs += @('/p', $plain)
        }
    }
    else {
        $signArgs += @('/sha1', $CertThumbprint)
    }
    & $signtool.FullName @signArgs $file
    if ($LASTEXITCODE -ne 0) { throw "Falha ao assinar $file" }
}

# ---------------------------------------------------------------- build
$results = @()
foreach ($platform in $Platforms) {
    # antes do pjproject: o pjmedia precisa dos cabecalhos do bcg729
    Step "bcg729 / G.729 ($platform)"
    Build-Bcg729 $platform

    if (-not $SkipPjproject) {
        Step "pjproject ($platform)"
        Invoke-MSBuild (Join-Path $PjDir 'pjsip-apps\build\libpjproject.vcxproj') 'Release-Static' $platform
    }

    Step "SQLite ($platform)"
    Build-SQLite $platform

    Step "Upphone ($platform)"
    Invoke-MSBuild (Join-Path $AppDir 'microsip.vcxproj') 'Release' $platform

    $built = if ($platform -eq 'x64') { Join-Path $AppDir 'x64\Release\Upphone.exe' } else { Join-Path $AppDir 'Release\Upphone.exe' }
    if (-not (Test-Path $built)) { throw "Executavel nao encontrado: $built" }

    $target = Join-Path $DistDir "$version\$platform"
    New-Item -ItemType Directory -Force $target | Out-Null
    Copy-Item $built (Join-Path $target 'Upphone.exe') -Force
    Sign-File (Join-Path $target 'Upphone.exe')
    $results += Join-Path $target 'Upphone.exe'
}

# ---------------------------------------------------------------- instalador
if ($Installer) {
    $iscc = @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:ProgramFiles\Inno Setup 6\ISCC.exe", "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe") |
        Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $iscc) { throw 'Inno Setup 6 (ISCC.exe) nao encontrado. Instale com: winget install JRSoftware.InnoSetup' }
    foreach ($platform in $Platforms) {
        Step "Instalador ($platform)"
        $source = Join-Path $DistDir "$version\$platform"
        & $iscc "/DAppVersion=$version" "/DPlatform=$platform" "/DSourceDir=$source" "/O$DistDir\$version" (Join-Path $PSScriptRoot 'Upphone.iss')
        if ($LASTEXITCODE -ne 0) { throw "Falha ao gerar o instalador ($platform)" }
        $setup = Join-Path $DistDir "$version\Upphone-SIP-$version-$platform-setup.exe"
        Sign-File $setup
        $results += $setup
    }
}

Step 'Pronto'
$results | ForEach-Object { Write-Host "  $_" }
if (-not $doSign) { Write-Host "`nAviso: arquivos NAO assinados (passe -PfxPath ou -CertThumbprint)." -ForegroundColor Yellow }
