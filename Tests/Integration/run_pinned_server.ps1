[CmdletBinding()]
param(
    [string]$PocketBasePath,
    [string]$ProjectDir,
    [ValidateRange(1, 65535)]
    [int]$Port = 18094,
    [ValidateRange(0, 10)]
    [int]$MaxRestarts = 3,
    [ValidateRange(1, 120)]
    [int]$ReadyTimeoutSeconds = 20
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$PinnedVersion = '0.39.11'
$FixtureIdentity = 'openpocketbase-fixture@example.com'
$ScriptDir = [IO.Path]::GetFullPath((Split-Path -Parent $MyInvocation.MyCommand.Path))
$RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $ScriptDir '..\..'))
$MigrationDir = [IO.Path]::GetFullPath((Join-Path $ScriptDir 'pb_migrations'))
$HooksDir = [IO.Path]::GetFullPath((Join-Path $ScriptDir 'pb_hooks'))
$TempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
$ExpectedDataDir = [IO.Path]::GetFullPath((Join-Path $TempRoot ("openpocketbase-pb-data-{0}" -f [Guid]::NewGuid().ToString('N'))))
$DataDir = $null
$CredentialPath = $null
$CredentialTempPath = $null
$CredentialExisted = $false
$PreviousCredentialBytes = $null
$CreatedRuntimeDir = $false
$ServerProcess = $null
$PreviousFixtureIdentity = [Environment]::GetEnvironmentVariable('OPENPOCKETBASE_FIXTURE_SUPERUSER_EMAIL', 'Process')
$PreviousFixturePassword = [Environment]::GetEnvironmentVariable('OPENPOCKETBASE_FIXTURE_SUPERUSER_PASSWORD', 'Process')
$RunningOnWindows = [Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT

. (Join-Path $ScriptDir 'pinned_server_restore.ps1')

function Resolve-PocketBaseExecutable {
    param(
        [string]$RequestedPath,
        [string]$RequestedProjectDir
    )

    $Candidates = New-Object System.Collections.Generic.List[string]
    if (-not [string]::IsNullOrWhiteSpace($RequestedPath)) {
        $Candidates.Add($RequestedPath)
    }

    $EnvironmentPath = [Environment]::GetEnvironmentVariable('OPENPOCKETBASE_POCKETBASE_EXE', 'Process')
    if (-not [string]::IsNullOrWhiteSpace($EnvironmentPath)) {
        $Candidates.Add($EnvironmentPath)
    }

    if (-not [string]::IsNullOrWhiteSpace($RequestedProjectDir)) {
        $Candidates.Add((Join-Path $RequestedProjectDir 'PocketBase\pocketbase.exe'))
    }

    $Candidates.Add((Join-Path (Split-Path -Parent $RepositoryRoot) 'PB_Testing\PocketBase\pocketbase.exe'))

    $Command = Get-Command 'pocketbase.exe' -ErrorAction SilentlyContinue
    if ($null -ne $Command) {
        $Candidates.Add($Command.Source)
    }

    foreach ($Candidate in $Candidates) {
        if ([string]::IsNullOrWhiteSpace($Candidate)) {
            continue
        }

        $FullPath = [IO.Path]::GetFullPath($Candidate)
        if ([IO.File]::Exists($FullPath)) {
            return $FullPath
        }
    }

    throw 'PocketBase v0.39.11 was not found. Pass -PocketBasePath or set OPENPOCKETBASE_POCKETBASE_EXE.'
}

function Assert-AvailablePort {
    param([int]$ListenPort)

    if ($ListenPort -eq 18091) {
        throw 'Port 18091 is reserved for the persistent PB_Testing server. Use the isolated default port 18094.'
    }

    $Listener = $null
    try {
        $Listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, $ListenPort)
        $Listener.Start()
    }
    catch {
        throw "Port $ListenPort is already in use or cannot be bound on 127.0.0.1."
    }
    finally {
        if ($null -ne $Listener) {
            $Listener.Stop()
        }
    }
}

function New-EphemeralPassword {
    $Bytes = New-Object byte[] 32
    $Generator = [Security.Cryptography.RandomNumberGenerator]::Create()
    try {
        $Generator.GetBytes($Bytes)
    }
    finally {
        $Generator.Dispose()
    }

    return [BitConverter]::ToString($Bytes).Replace('-', '').ToLowerInvariant()
}

function ConvertTo-QuotedProcessArgument {
    param([string]$Value)

    return '"' + $Value.Replace('"', '\"') + '"'
}

function Get-SafeLogTail {
    param(
        [string[]]$Paths,
        [string]$Secret
    )

    foreach ($Path in $Paths) {
        if (-not [IO.File]::Exists($Path)) {
            continue
        }

        Get-Content -LiteralPath $Path -Tail 20 | ForEach-Object {
            if ([string]::IsNullOrEmpty($Secret)) {
                $_
            }
            else {
                $_.Replace($Secret, '<redacted>')
            }
        }
    }
}

function Wait-PocketBaseReady {
    param(
        [Diagnostics.Process]$Process,
        [string]$BaseUrl,
        [int]$TimeoutSeconds
    )

    $Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $Deadline) {
        if ($Process.HasExited) {
            return $false
        }

        try {
            $Response = Invoke-WebRequest -UseBasicParsing -Uri "$BaseUrl/api/health" -TimeoutSec 1
            if ($Response.StatusCode -eq 200) {
                return $true
            }
        }
        catch {
        }

        Start-Sleep -Milliseconds 150
    }

    return $false
}

function Stop-OwnedProcess {
    param([Diagnostics.Process]$Process)

    if ($null -eq $Process -or $Process.HasExited) {
        return
    }

    Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
    $Process.WaitForExit(5000) | Out-Null
}

function New-PinnedAdminAuthorizationHeaders {
    param(
        [string]$BaseUrl,
        [string]$Identity,
        [string]$Password
    )

    $Body = @{
        identity = $Identity
        password = $Password
    } | ConvertTo-Json -Compress
    $Response = Invoke-RestMethod -Method Post -Uri "$BaseUrl/api/collections/_superusers/auth-with-password" -ContentType 'application/json' -Body $Body -TimeoutSec 5
    return @{ Authorization = "Bearer $($Response.token)" }
}

function Find-PinnedRestoreRequestKey {
    param(
        [string]$BaseUrl,
        [hashtable]$Headers,
        [DateTime]$NotBeforeUtc
    )

    try {
        $Page = Invoke-RestMethod -Method Get -Uri "$BaseUrl/api/logs?page=1&perPage=50&sort=-created" -Headers $Headers -TimeoutSec 5
    }
    catch {
        return $null
    }

    foreach ($Item in $Page.items) {
        $Created = $Item.PSObject.Properties['created']
        $Data = $Item.PSObject.Properties['data']
        if ($null -eq $Created -or $null -eq $Data) {
            continue
        }

        $CreatedUtc = [DateTime]::MinValue
        if (-not [DateTime]::TryParse([string]$Created.Value, [ref]$CreatedUtc) -or $CreatedUtc.ToUniversalTime() -lt $NotBeforeUtc) {
            continue
        }

        $Method = $Data.Value.PSObject.Properties['method']
        $Status = $Data.Value.PSObject.Properties['status']
        $Url = $Data.Value.PSObject.Properties['url']
        if ($null -ne $Method -and $Method.Value -eq 'POST' -and
            $null -ne $Status -and [int]$Status.Value -eq 204 -and
            $null -ne $Url -and [string]$Url.Value -match '^/api/backups/([a-z0-9_-]+\.zip)/restore$') {
            return $Matches[1]
        }
    }

    return $null
}

function Remove-VerifiedDataDirectory {
    param(
        [string]$Path,
        [string]$ExpectedPath,
        [string]$ExpectedParent
    )

    if ([string]::IsNullOrWhiteSpace($Path) -or -not [IO.Directory]::Exists($Path)) {
        return
    }

    $FullPath = [IO.Path]::GetFullPath($Path)
    $FullExpectedPath = [IO.Path]::GetFullPath($ExpectedPath)
    $FullParent = [IO.Path]::GetFullPath((Split-Path -Parent $FullPath)).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    $Leaf = Split-Path -Leaf $FullPath
    if (-not $FullPath.Equals($FullExpectedPath, [StringComparison]::OrdinalIgnoreCase) -or
        -not $FullParent.Equals($ExpectedParent, [StringComparison]::OrdinalIgnoreCase) -or
        $Leaf -notmatch '^openpocketbase-pb-data-[0-9a-f]{32}$') {
        throw "Refusing to remove unexpected data directory '$FullPath'."
    }

    Remove-Item -LiteralPath $FullPath -Recurse -Force
}

function Restore-CredentialFile {
    param(
        [string]$Path,
        [string]$ExpectedPath,
        [bool]$Existed,
        [byte[]]$PreviousBytes,
        [string]$TemporaryPath
    )

    if (-not [string]::IsNullOrWhiteSpace($TemporaryPath) -and [IO.File]::Exists($TemporaryPath)) {
        Remove-Item -LiteralPath $TemporaryPath -Force
    }

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    $FullPath = [IO.Path]::GetFullPath($Path)
    if (-not $FullPath.Equals([IO.Path]::GetFullPath($ExpectedPath), [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to modify unexpected credential path '$FullPath'."
    }

    if ($Existed) {
        [IO.File]::WriteAllBytes($FullPath, $PreviousBytes)
    }
    elseif ([IO.File]::Exists($FullPath)) {
        Remove-Item -LiteralPath $FullPath -Force
    }
}

try {
    Assert-AvailablePort -ListenPort $Port

    if (-not [IO.Directory]::Exists($MigrationDir) -or -not [IO.Directory]::Exists($HooksDir)) {
        throw 'The tracked PocketBase migration or hook directory is missing.'
    }

    $ResolvedProjectDir = $null
    if (-not [string]::IsNullOrWhiteSpace($ProjectDir)) {
        $ResolvedProjectDir = [IO.Path]::GetFullPath($ProjectDir)
        if (-not [IO.Directory]::Exists($ResolvedProjectDir)) {
            throw "Project directory '$ResolvedProjectDir' does not exist."
        }
    }

    $ResolvedPocketBasePath = Resolve-PocketBaseExecutable -RequestedPath $PocketBasePath -RequestedProjectDir $ResolvedProjectDir
    $VersionText = (& $ResolvedPocketBasePath --version 2>&1 | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $VersionText -notmatch "(^|\s)$([Regex]::Escape($PinnedVersion))($|\s)") {
        throw "Expected PocketBase $PinnedVersion, but '$ResolvedPocketBasePath' reported '$VersionText'."
    }

    $DataDir = $ExpectedDataDir
    New-Item -ItemType Directory -Path $DataDir | Out-Null

    $Password = New-EphemeralPassword
    [Environment]::SetEnvironmentVariable('OPENPOCKETBASE_FIXTURE_SUPERUSER_EMAIL', $FixtureIdentity, 'Process')
    [Environment]::SetEnvironmentVariable('OPENPOCKETBASE_FIXTURE_SUPERUSER_PASSWORD', $Password, 'Process')

    $BaseUrl = "http://127.0.0.1:$Port"
    if ($null -ne $ResolvedProjectDir) {
        $RuntimeDir = [IO.Path]::GetFullPath((Join-Path $ResolvedProjectDir '.runtime'))
        $ExpectedRuntimeDir = [IO.Path]::GetFullPath((Join-Path $ResolvedProjectDir '.runtime'))
        if (-not $RuntimeDir.Equals($ExpectedRuntimeDir, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Unexpected runtime directory '$RuntimeDir'."
        }

        if (-not [IO.Directory]::Exists($RuntimeDir)) {
            New-Item -ItemType Directory -Path $RuntimeDir | Out-Null
            $CreatedRuntimeDir = $true
        }

        $CredentialPath = [IO.Path]::GetFullPath((Join-Path $RuntimeDir 'admin-credentials.json'))
        $ExpectedCredentialPath = [IO.Path]::GetFullPath((Join-Path $ResolvedProjectDir '.runtime\admin-credentials.json'))
        if (-not $CredentialPath.Equals($ExpectedCredentialPath, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Unexpected credential path '$CredentialPath'."
        }

        $CredentialExisted = [IO.File]::Exists($CredentialPath)
        if ($CredentialExisted) {
            $PreviousCredentialBytes = [IO.File]::ReadAllBytes($CredentialPath)
        }

        $CredentialTempPath = "$CredentialPath.$PID.tmp"
        $Credential = [ordered]@{
            baseUrl = $BaseUrl
            identity = $FixtureIdentity
            email = $FixtureIdentity
            password = $Password
        }
        $CredentialJson = ($Credential | ConvertTo-Json -Compress) + [Environment]::NewLine
        [IO.File]::WriteAllText($CredentialTempPath, $CredentialJson, (New-Object Text.UTF8Encoding($false)))
        Move-Item -LiteralPath $CredentialTempPath -Destination $CredentialPath -Force
        $CredentialTempPath = $null
    }

    $MigrationOutputLog = Join-Path $DataDir 'migration.stdout.log'
    $MigrationErrorLog = Join-Path $DataDir 'migration.stderr.log'
    $MigrationArguments = @(
        'migrate',
        'up',
        "--dir=$(ConvertTo-QuotedProcessArgument $DataDir)",
        "--migrationsDir=$(ConvertTo-QuotedProcessArgument $MigrationDir)",
        "--hooksDir=$(ConvertTo-QuotedProcessArgument $HooksDir)",
        '--automigrate=false',
        '--dev=false'
    )
    $MigrationProcess = Start-Process -FilePath $ResolvedPocketBasePath -ArgumentList $MigrationArguments -PassThru -Wait -WindowStyle Hidden -RedirectStandardOutput $MigrationOutputLog -RedirectStandardError $MigrationErrorLog
    if ($MigrationProcess.ExitCode -ne 0) {
        Get-SafeLogTail -Paths @($MigrationOutputLog, $MigrationErrorLog) -Secret $Password
        throw "PocketBase migrations failed with exit code $($MigrationProcess.ExitCode)."
    }

    Write-Host "PocketBase $PinnedVersion isolated fixture: $BaseUrl"
    if ($null -ne $CredentialPath) {
        Write-Host "Blueprint credentials: .runtime/admin-credentials.json"
    }
    else {
        Write-Host 'No Blueprint credential file was written. Pass -ProjectDir to enable privileged Blueprint tests.'
    }
    Write-Host 'Press Ctrl+C to stop the fixture and remove its temporary data.'

    $RestartCount = 0
    while ($true) {
        $ServerOutputLog = Join-Path $DataDir ("server-{0}.stdout.log" -f $RestartCount)
        $ServerErrorLog = Join-Path $DataDir ("server-{0}.stderr.log" -f $RestartCount)
        $ServerArguments = @(
            'serve',
            "--http=127.0.0.1:$Port",
            "--dir=$(ConvertTo-QuotedProcessArgument $DataDir)",
            "--migrationsDir=$(ConvertTo-QuotedProcessArgument $MigrationDir)",
            "--hooksDir=$(ConvertTo-QuotedProcessArgument $HooksDir)",
            '--hooksWatch=false',
            '--automigrate=false',
            '--dev=false'
        )
        $ServerProcess = Start-Process -FilePath $ResolvedPocketBasePath -ArgumentList $ServerArguments -PassThru -WindowStyle Hidden -RedirectStandardOutput $ServerOutputLog -RedirectStandardError $ServerErrorLog

        if (-not (Wait-PocketBaseReady -Process $ServerProcess -BaseUrl $BaseUrl -TimeoutSeconds $ReadyTimeoutSeconds)) {
            $ExitDescription = if ($ServerProcess.HasExited) { "exit code $($ServerProcess.ExitCode)" } else { 'the readiness timeout' }
            Stop-OwnedProcess -Process $ServerProcess
            Get-SafeLogTail -Paths @($ServerOutputLog, $ServerErrorLog) -Secret $Password
            throw "PocketBase did not become ready before $ExitDescription."
        }

        if ($RestartCount -eq 0) {
            Write-Host 'PocketBase is ready.'
        }
        else {
            Write-Host "PocketBase restarted after restore ($RestartCount of $MaxRestarts)."
        }

        $PendingRestoreKey = $null
        $RestoreNotBeforeUtc = [DateTime]::UtcNow.AddSeconds(-2)
        $RestoreHeaders = $null
        if ($RunningOnWindows) {
            $RestoreHeaders = New-PinnedAdminAuthorizationHeaders -BaseUrl $BaseUrl -Identity $FixtureIdentity -Password $Password
        }
        $NextRestorePollUtc = [DateTime]::UtcNow
        while (-not $ServerProcess.WaitForExit(250)) {
            if ($RunningOnWindows -and [DateTime]::UtcNow -ge $NextRestorePollUtc) {
                $NextRestorePollUtc = [DateTime]::UtcNow.AddSeconds(1)
                $PendingRestoreKey = Find-PinnedRestoreRequestKey -BaseUrl $BaseUrl -Headers $RestoreHeaders -NotBeforeUtc $RestoreNotBeforeUtc
                if (-not [string]::IsNullOrWhiteSpace($PendingRestoreKey)) {
                    Write-Host "PocketBase restore request '$PendingRestoreKey' detected. Stopping the Windows fixture to apply it."
                    Stop-OwnedProcess -Process $ServerProcess
                    break
                }
            }
        }
        $ExitCode = $ServerProcess.ExitCode
        $ServerProcess = $null
        if (-not [string]::IsNullOrWhiteSpace($PendingRestoreKey)) {
            Invoke-PinnedPocketBaseWindowsRestore -DataDir $DataDir -BackupKey $PendingRestoreKey
        }
        if ($RestartCount -ge $MaxRestarts) {
            Get-SafeLogTail -Paths @($ServerOutputLog, $ServerErrorLog) -Secret $Password
            throw "PocketBase exited with code $ExitCode after reaching the restart limit."
        }

        $RestartCount++
        if (-not [string]::IsNullOrWhiteSpace($PendingRestoreKey)) {
            Write-Host 'PocketBase stopped for the Windows restore. Restarting the isolated fixture for verification.'
        }
        else {
            Write-Host "PocketBase exited with code $ExitCode. Restarting the isolated fixture for restore verification."
        }
        Start-Sleep -Milliseconds 500
    }
}
finally {
    Stop-OwnedProcess -Process $ServerProcess

    if ($null -ne $CredentialPath) {
        $ExpectedCredentialPath = [IO.Path]::GetFullPath((Join-Path $ResolvedProjectDir '.runtime\admin-credentials.json'))
        Restore-CredentialFile -Path $CredentialPath -ExpectedPath $ExpectedCredentialPath -Existed $CredentialExisted -PreviousBytes $PreviousCredentialBytes -TemporaryPath $CredentialTempPath

        if ($CreatedRuntimeDir -and [IO.Directory]::Exists((Split-Path -Parent $CredentialPath))) {
            $RemainingEntries = @(Get-ChildItem -LiteralPath (Split-Path -Parent $CredentialPath) -Force)
            if ($RemainingEntries.Count -eq 0) {
                Remove-Item -LiteralPath (Split-Path -Parent $CredentialPath) -Force
            }
        }
    }

    [Environment]::SetEnvironmentVariable('OPENPOCKETBASE_FIXTURE_SUPERUSER_EMAIL', $PreviousFixtureIdentity, 'Process')
    [Environment]::SetEnvironmentVariable('OPENPOCKETBASE_FIXTURE_SUPERUSER_PASSWORD', $PreviousFixturePassword, 'Process')
    Remove-VerifiedDataDirectory -Path $DataDir -ExpectedPath $ExpectedDataDir -ExpectedParent $TempRoot
}
