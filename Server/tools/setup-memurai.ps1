# Memurai(Windows 용 Redis 호환 서버)를 이 프로젝트에 맞게 설정한다.
#
#   관리자 권한 PowerShell 에서:
#     .\tools\setup-memurai.ps1
#
# 기본 설치 상태의 문제 두 가지를 고친다.
#   1. requirepass 가 없어 로컬의 아무 프로세스나 캐시를 읽고 쓸 수 있다.
#   2. maxmemory 가 물리 메모리 비율(수 GiB)이고 정책이 noeviction 이다.
#      캐시인데 메모리가 차면 오래된 키를 버리는 대신 쓰기가 실패한다.
#
# 벤더 설정 파일 내용은 건드리지 않고 끝에 표시된 블록만 덧붙인다.
# Redis 설정은 뒤에 나온 값이 이기므로 이걸로 충분하고, 되돌리려면 블록만 지우면 된다.

param(
    [string]$MaxMemory = '512mb',
    [switch]$Remove
)

$ErrorActionPreference = 'Stop'

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "관리자 권한이 필요합니다. PowerShell 을 '관리자 권한으로 실행' 후 다시 시도하세요."
}

$memuraiDir = Join-Path $env:ProgramFiles 'Memurai'
$confPath = Join-Path $memuraiDir 'memurai.conf'
$cliPath = Join-Path $memuraiDir 'memurai-cli.exe'

foreach ($required in @($confPath, $cliPath)) {
    if (-not (Test-Path $required)) {
        throw "찾을 수 없습니다: $required"
    }
}

$beginMarker = '# >>> HeavenHyperVoice begin >>>'
$endMarker = '# <<< HeavenHyperVoice end <<<'

# 기존 블록을 걷어낸다. 여러 번 실행해도 중복되지 않는다.
$lines = [System.IO.File]::ReadAllLines($confPath)
$kept = New-Object System.Collections.Generic.List[string]
$inBlock = $false
foreach ($line in $lines) {
    if ($line -eq $beginMarker) { $inBlock = $true; continue }
    if ($line -eq $endMarker) { $inBlock = $false; continue }
    if (-not $inBlock) { $kept.Add($line) }
}

if ($Remove) {
    $backup = "$confPath.bak"
    Copy-Item $confPath $backup -Force
    [System.IO.File]::WriteAllLines($confPath, $kept)
    Write-Host "설정 블록을 제거했습니다. 백업: $backup"
    Write-Host "적용하려면: Restart-Service Memurai"
    return
}

Write-Host 'Memurai 에 설정할 비밀번호를 입력하세요 (화면에 보이지 않습니다).'
$secure = Read-Host -AsSecureString 'password'
$plain = [Runtime.InteropServices.Marshal]::PtrToStringBSTR(
    [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure))

if ([string]::IsNullOrWhiteSpace($plain)) {
    throw '비밀번호가 비어 있습니다. 설정을 중단합니다.'
}
if ($plain -match '[\s"]') {
    throw '비밀번호에 공백이나 따옴표는 쓸 수 없습니다 (설정 파일 문법 제약).'
}

$backup = "$confPath.bak"
Copy-Item $confPath $backup -Force
Write-Host "백업: $backup"

$block = @(
    $beginMarker
    '# 이 블록은 tools\setup-memurai.ps1 이 관리한다. 직접 고치지 말 것.'
    ''
    '# 로컬의 아무 프로세스나 캐시에 접근하지 못하게 한다.'
    "requirepass $plain"
    ''
    '# 세션 레지스트리와 티켓 nonce 는 작다. 개발 머신에서 수 GiB 를 잡을 이유가 없다.'
    "maxmemory $MaxMemory"
    ''
    '# 기본값 noeviction 은 메모리가 차면 오래된 키를 버리는 대신 쓰기를 실패시킨다.'
    '# 우리가 넣는 키는 전부 TTL 이 있으므로 volatile-lru 로 충분하고,'
    '# 나중에 TTL 없는 키를 넣더라도 그것만은 보호된다.'
    'maxmemory-policy volatile-lru'
    ''
    '# 순수 캐시이므로 디스크 스냅샷을 끈다. 재시작하면 세션이 비지만,'
    '# 모든 키에 TTL 이 있어 클라이언트가 다시 붙으면 복구된다.'
    'save ""'
    $endMarker
)

$kept.Add('')
foreach ($line in $block) { $kept.Add($line) }
[System.IO.File]::WriteAllLines($confPath, $kept)

Write-Host '설정을 적용했습니다. 서비스를 재시작합니다.'
Restart-Service Memurai
Start-Sleep -Seconds 2

Write-Host ''
Write-Host '=== 확인 ==='
& $cliPath -a $plain --no-auth-warning CONFIG GET maxmemory
& $cliPath -a $plain --no-auth-warning CONFIG GET maxmemory-policy
& $cliPath -a $plain --no-auth-warning CONFIG GET save
Write-Host ''
Write-Host '=== 비밀번호 없이 접근하면 거부되어야 한다 ==='
& $cliPath PING 2>&1 | Select-Object -First 1

Write-Host ''
Write-Host '설정 파일에 비밀번호가 평문으로 들어갑니다. Program Files 는 로컬 사용자가'
Write-Host '읽을 수 있으므로, 이 조치는 "아무 프로세스나 캐시를 조작하는 것"을 막는 수준입니다.'
Write-Host '실제 배포에서는 Redis ACL 과 파일 권한을 함께 써야 합니다.'
