$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'pinned_server_restore.ps1')

$Suffix = [Guid]::NewGuid().ToString('N')
$DataDir = Join-Path ([IO.Path]::GetTempPath()) "openpocketbase-pb-data-$Suffix"
$SourceDir = Join-Path ([IO.Path]::GetTempPath()) "openpocketbase-pb-restore-source-$Suffix"

try {
    $BackupDir = Join-Path $DataDir 'backups'
    New-Item -ItemType Directory -Path $BackupDir | Out-Null
    New-Item -ItemType Directory -Path $SourceDir | Out-Null
    [IO.File]::WriteAllText((Join-Path $DataDir 'data.db'), 'current')
    [IO.File]::WriteAllText((Join-Path $DataDir 'disposable.txt'), 'remove')
    [IO.File]::WriteAllText((Join-Path $SourceDir 'data.db'), 'restored')
    [IO.File]::WriteAllText((Join-Path $SourceDir 'snapshot.txt'), 'present')

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $BackupPath = Join-Path $BackupDir 'restore_test.zip'
    [IO.Compression.ZipFile]::CreateFromDirectory($SourceDir, $BackupPath)

    Invoke-PinnedPocketBaseWindowsRestore -DataDir $DataDir -BackupKey 'restore_test.zip'

    if ([IO.File]::ReadAllText((Join-Path $DataDir 'data.db')) -ne 'restored') {
        throw 'The restored database was not activated.'
    }
    if ([IO.File]::Exists((Join-Path $DataDir 'disposable.txt'))) {
        throw 'Current data survived the restore.'
    }
    if (-not [IO.File]::Exists((Join-Path $DataDir 'snapshot.txt'))) {
        throw 'The snapshot contents were not restored.'
    }
    if (-not [IO.File]::Exists((Join-Path $DataDir 'backups\restore_test.zip'))) {
        throw 'The backup archive was not preserved.'
    }

    'PASS - the Windows fixture restore replaced current data and preserved backups.'
}
finally {
    foreach ($Path in @($DataDir, $SourceDir)) {
        if ([IO.Directory]::Exists($Path)) {
            $FullPath = [IO.Path]::GetFullPath($Path).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
            $TempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
            if (-not (Split-Path -Parent $FullPath).Equals($TempRoot, [StringComparison]::OrdinalIgnoreCase) -or
                (Split-Path -Leaf $FullPath) -notmatch '^openpocketbase-pb-(data|restore-source)-[0-9a-f]{32}$') {
                throw "Refusing to remove unexpected test path '$FullPath'."
            }
            Remove-Item -LiteralPath $Path -Recurse -Force
        }
    }
}
