# 인증 티켓 서명용 Ed25519 키 쌍을 Server/certs 에 만든다.
#
#   auth.key  개인키 - LoginServer 만 갖는다. 티켓 발급에 쓴다.
#   auth.pub  공개키 - 검증하는 서버들(채팅/음성/게이트웨이)에 배포한다.
#
# 개발 전용이다. certs/ 는 .gitignore 로 막혀 있다.

$ErrorActionPreference = 'Stop'

$serverRoot = Split-Path -Parent $PSScriptRoot
$certDir = Join-Path $serverRoot 'certs'
$privateKey = Join-Path $certDir 'auth.key'
$publicKey = Join-Path $certDir 'auth.pub'

New-Item -ItemType Directory -Force -Path $certDir | Out-Null

$openssl = (Get-Command openssl.exe -ErrorAction SilentlyContinue).Source
if (-not $openssl) {
    $candidates = @(
        (Join-Path $env:ProgramFiles 'Git\usr\bin\openssl.exe'),
        (Join-Path $env:ProgramFiles 'Git\mingw64\bin\openssl.exe')
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

& $openssl genpkey -algorithm ed25519 -out $privateKey
if ($LASTEXITCODE -ne 0) { throw "openssl genpkey failed ($LASTEXITCODE)" }

& $openssl pkey -in $privateKey -pubout -out $publicKey
if ($LASTEXITCODE -ne 0) { throw "openssl pkey -pubout failed ($LASTEXITCODE)" }

Write-Host ''
Write-Host "Wrote $privateKey  (private - LoginServer only)"
Write-Host "Wrote $publicKey  (public  - distribute to verifying servers)"
