# Ninja + MSVC 빌드에는 개발자 환경(vcvars)이 필요하다.
# 이 스크립트는 VCPKG_ROOT 와 MSVC 환경을 자동으로 잡아준 뒤 CMake 프리셋을 실행한다.

param(
    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Debug',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

# vcvars64.bat 는 VCPKG_ROOT 를 Visual Studio 번들 vcpkg 로 덮어쓴다.
# 호출자가 지정한 값을 먼저 붙잡아 두고, vcvars 를 로드한 뒤에 되돌린다.
$requestedVcpkgRoot = $env:VCPKG_ROOT

# cl.exe 가 PATH 에 없으면 vcvars64.bat 를 실행해 환경변수를 이 세션으로 가져온다.
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found at $vswhere - is Visual Studio installed?"
    }

    # VS2022 (v17) 로 고정한다. 최신 툴셋을 쓰면 vcpkg 베이스라인의 spdlog 가
    # MSVC 19.51 에서 빌드되지 않는다. 여기를 풀려면 vcpkg 베이스라인부터 올릴 것.
    # HHV_VS_PATH 로 덮어쓸 수 있다.
    $vsPath = $env:HHV_VS_PATH
    if (-not $vsPath) {
        $vsPath = & $vswhere -latest -products * -version '[17.0,18.0)' `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath
    }
    if (-not $vsPath) {
        throw 'No Visual Studio 2022 installation with the x64 C++ toolset was found.'
    }

    $vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
    if (-not (Test-Path $vcvars)) {
        throw "vcvars64.bat not found at $vcvars"
    }

    Write-Host "Loading MSVC environment from $vcvars"
    cmd /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') {
            Set-Item -Path "Env:$($matches[1])" -Value $matches[2]
        }
    }
}

# vcvars 가 덮어썼을 수 있으므로 여기서 확정한다. 프로젝트 baseline 은 이 vcpkg 기준이다.
$env:VCPKG_ROOT = $requestedVcpkgRoot
if (-not $env:VCPKG_ROOT) {
    $candidates = @('C:\tools\vcpkg', 'C:\vcpkg', (Join-Path $env:USERPROFILE 'vcpkg'))
    foreach ($candidate in $candidates) {
        if (Test-Path (Join-Path $candidate 'scripts\buildsystems\vcpkg.cmake')) {
            $env:VCPKG_ROOT = $candidate
            break
        }
    }
}
if (-not $env:VCPKG_ROOT) {
    throw 'VCPKG_ROOT is not set and no vcpkg installation was found.'
}
Write-Host "VCPKG_ROOT = $env:VCPKG_ROOT"

if (-not (Get-Command ninja.exe -ErrorAction SilentlyContinue)) {
    throw 'ninja.exe was not found on PATH.'
}

$buildDir = Join-Path $PSScriptRoot 'build'
if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "Removing $buildDir"
    Remove-Item -Recurse -Force $buildDir
}

Push-Location $PSScriptRoot
try {
    cmake --preset windows-x64
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed ($LASTEXITCODE)" }

    cmake --build --preset $Config.ToLower()
    if ($LASTEXITCODE -ne 0) { throw "cmake build failed ($LASTEXITCODE)" }
}
finally {
    Pop-Location
}

Write-Host ''
Write-Host "Built $Config -> build\windows-x64\bin\$Config\"
