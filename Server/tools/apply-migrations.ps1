# DataBase/ 의 마이그레이션을 순서대로 적용한다.
#
#   .\tools\apply-migrations.ps1              # root 로 전체 적용
#   .\tools\apply-migrations.ps1 -DryRun      # 실행할 파일만 확인
#   .\tools\apply-migrations.ps1 -User admin  # 다른 계정으로
#
# Windows PowerShell 5.1 은 '<' 입력 리다이렉션을 지원하지 않으므로
# mysql 클라이언트의 source 명령을 쓴다.
#
# 비밀번호는 mysql 이 직접 콘솔에서 받는다. 이 스크립트는 비밀번호를
# 인자로도, 파일로도 다루지 않는다 (프로세스 목록에 노출되지 않게).

param(
    [string]$User = 'root',
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'

$serverRoot = Split-Path -Parent $PSScriptRoot
$migrationDir = Join-Path $serverRoot 'DataBase'

if (-not (Test-Path $migrationDir)) {
    throw "migration directory not found: $migrationDir"
}

$mysql = (Get-Command mysql.exe -ErrorAction SilentlyContinue).Source
if (-not $mysql) {
    $candidates = @(
        "$env:ProgramFiles\MySQL\MySQL Server 8.0\bin\mysql.exe",
        "$env:ProgramFiles\MySQL\MySQL Server 8.4\bin\mysql.exe"
    )
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) { $mysql = $candidate; break }
    }
}
if (-not $mysql) {
    throw 'mysql.exe not found. Add the MySQL bin directory to PATH or install MySQL Server.'
}

# *.template.sql 은 비밀번호 자리가 비어 있는 템플릿이므로 건너뛴다.
$migrations = Get-ChildItem $migrationDir -Filter '*.sql' |
    Where-Object { $_.Name -notlike '*.template.sql' } |
    Sort-Object Name

if (-not $migrations) {
    Write-Host 'No migrations to apply.'
    return
}

Write-Host "mysql : $mysql"
Write-Host "user  : $User"
Write-Host 'migrations:'
foreach ($m in $migrations) { Write-Host "  $($m.Name)" }

if ($DryRun) {
    Write-Host ''
    Write-Host 'Dry run - nothing was applied.'
    return
}

# source 는 백슬래시를 이스케이프로 해석하므로 슬래시로 바꾼다.
# 한 번의 호출로 묶어 비밀번호 프롬프트가 한 번만 뜨게 한다.
$script = ($migrations | ForEach-Object {
    'source ' + ($_.FullName -replace '\\', '/') + ';'
}) -join ' '

Write-Host ''
& $mysql -u $User -p -e $script
if ($LASTEXITCODE -ne 0) {
    throw "mysql exited with code $LASTEXITCODE"
}

Write-Host ''
Write-Host 'All migrations applied.'
