# 로컬 개발용 자체 서명 인증서를 Server/certs 에 만든다.
# 자체 서명이라 개발 전용이다. certs/ 는 .gitignore 로 막혀 있다.

$ErrorActionPreference = 'Stop'

$serverRoot = Split-Path -Parent $PSScriptRoot
$certDir = Join-Path $serverRoot 'certs'
$certFile = Join-Path $certDir 'server.crt'
$keyFile = Join-Path $certDir 'server.key'

New-Item -ItemType Directory -Force -Path $certDir | Out-Null

$openssl = (Get-Command openssl.exe -ErrorAction SilentlyContinue).Source
if (-not $openssl) {
    $candidates = @(
        (Join-Path $env:ProgramFiles 'Git\usr\bin\openssl.exe'),
        (Join-Path $env:VCPKG_ROOT 'installed\x64-windows\tools\openssl\openssl.exe')
    )
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            $openssl = $candidate
            break
        }
    }
}
if (-not $openssl) {
    throw 'openssl.exe not found. Install Git for Windows or add OpenSSL to PATH.'
}

Write-Host "Using $openssl"

& $openssl req -x509 -newkey rsa:2048 -nodes -days 3650 `
    -subj '/CN=localhost' `
    -addext 'subjectAltName=DNS:localhost,IP:127.0.0.1' `
    -keyout $keyFile -out $certFile
if ($LASTEXITCODE -ne 0) {
    throw "openssl failed ($LASTEXITCODE)"
}

Write-Host ''
Write-Host "Wrote $certFile"
Write-Host "Wrote $keyFile"
