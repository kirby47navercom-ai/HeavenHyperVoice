# DataBase/ 의 마이그레이션을 순서대로 적용한다.
#
#   .\tools\apply-migrations.ps1                # 백업 후 root 로 전체 적용
#   .\tools\apply-migrations.ps1 -DryRun        # 실행할 파일만 확인
#   .\tools\apply-migrations.ps1 -User admin    # 다른 계정으로
#   .\tools\apply-migrations.ps1 -SkipBackup    # 백업 생략
#
# 이 스크립트는 매번 **모든** 파일을 다시 실행한다. 적용 여부를 먼저 조회하려면
# 비밀번호를 한 번 더 물어야 해서 그렇게 하지 않았다.
#
# 그래서 마이그레이션은 전부 여러 번 실행해도 안전해야 한다. 스키마를 바꾸는
# 구문은 information_schema 로 확인한 뒤 PREPARE 로 감싼다 — WHERE 로만 거르면
# 이미 사라진 컬럼을 참조하는 SELECT 가 파싱 단계에서 실패한다 (004 참고).
#
# Windows PowerShell 5.1 은 '<' 입력 리다이렉션을 지원하지 않으므로
# mysql 클라이언트의 source 명령을 쓴다.
#
# 비밀번호는 mysql 이 직접 콘솔에서 받는다. 이 스크립트는 비밀번호를
# 인자로도, 파일로도 다루지 않는다 (프로세스 목록에 노출되지 않게).
# 그래서 백업을 뜨면 비밀번호를 두 번 묻는다 (mysqldump 한 번, mysql 한 번).
#
# 004 처럼 컬럼을 드롭하는 마이그레이션은 되돌릴 수 없다. 적용 전에
# DataBase\backup\ 에 mysqldump 를 남긴다 (gitignore 됨).

param(
    [string]$User = 'root',
    [switch]$DryRun,
    [switch]$SkipBackup
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

if (-not $SkipBackup) {
    $dump = Join-Path (Split-Path -Parent $mysql) 'mysqldump.exe'
    if (-not (Test-Path $dump)) {
        throw "mysqldump.exe not found next to mysql.exe. Re-run with -SkipBackup to proceed without a backup."
    }

    $backupDir = Join-Path $migrationDir 'backup'
    if (-not (Test-Path $backupDir)) {
        New-Item -ItemType Directory -Path $backupDir | Out-Null
    }
    $backupFile = Join-Path $backupDir ("hhv-{0}.sql" -f (Get-Date -Format 'yyyyMMdd-HHmmss'))

    Write-Host ''
    Write-Host "backup: $backupFile"
    Write-Host '(비밀번호를 두 번 묻는다. 이건 mysqldump 쪽이다.)'

    # 스키마가 없으면 백업할 것도 없다. 최초 실행이 그렇다.
    & $dump -u $User -p --databases hhv --result-file=$backupFile
    if ($LASTEXITCODE -ne 0) {
        Remove-Item $backupFile -ErrorAction SilentlyContinue
        Write-Host 'mysqldump 실패 - 데이터베이스가 아직 없으면 정상이다.'
        Write-Host '이미 데이터가 있다면 중단하고 원인을 확인할 것.'
        $answer = Read-Host '백업 없이 계속할까? (y/N)'
        if ($answer -ne 'y') { throw 'aborted' }
    }
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
