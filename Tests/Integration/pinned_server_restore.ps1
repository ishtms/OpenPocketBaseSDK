Set-StrictMode -Version Latest

function Resolve-PinnedPocketBaseDataDirectory {
    param([Parameter(Mandatory = $true)][string]$DataDir)

    $FullPath = [IO.Path]::GetFullPath($DataDir).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    $TempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    $Parent = [IO.Path]::GetFullPath((Split-Path -Parent $FullPath)).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    $Leaf = Split-Path -Leaf $FullPath
    if (-not $Parent.Equals($TempRoot, [StringComparison]::OrdinalIgnoreCase) -or
        $Leaf -notmatch '^openpocketbase-pb-data-[0-9a-f]{32}$') {
        throw "Refusing to restore unexpected PocketBase data directory '$FullPath'."
    }

    return $FullPath
}

function Assert-PinnedPocketBaseRestoreWorkingPath {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$ExpectedPattern
    )

    $FullPath = [IO.Path]::GetFullPath($Path).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    $TempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    $Parent = [IO.Path]::GetFullPath((Split-Path -Parent $FullPath)).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    if (-not $Parent.Equals($TempRoot, [StringComparison]::OrdinalIgnoreCase) -or
        (Split-Path -Leaf $FullPath) -notmatch $ExpectedPattern) {
        throw "Refusing to use unexpected restore working path '$FullPath'."
    }
}

function Remove-PinnedPocketBaseRestoreDirectory {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$ExpectedPattern
    )

    if (-not [IO.Directory]::Exists($Path)) {
        return
    }

    Assert-PinnedPocketBaseRestoreWorkingPath -Path $Path -ExpectedPattern $ExpectedPattern
    Remove-Item -LiteralPath $Path -Recurse -Force
}

function Invoke-PinnedPocketBaseWindowsRestore {
    param(
        [Parameter(Mandatory = $true)][string]$DataDir,
        [Parameter(Mandatory = $true)][string]$BackupKey
    )

    if ($BackupKey.Length -lt 5 -or $BackupKey.Length -gt 150 -or
        $BackupKey -notmatch '^[a-z0-9_-]+\.zip$') {
        throw "Unsafe PocketBase restore backup key '$BackupKey'."
    }

    $ResolvedDataDir = Resolve-PinnedPocketBaseDataDirectory -DataDir $DataDir
    $BackupDir = [IO.Path]::GetFullPath((Join-Path $ResolvedDataDir 'backups'))
    $BackupPath = [IO.Path]::GetFullPath((Join-Path $BackupDir $BackupKey))
    if (-not (Split-Path -Parent $BackupPath).Equals($BackupDir, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.File]::Exists($BackupPath)) {
        throw "PocketBase restore backup '$BackupKey' was not found."
    }

    $Suffix = [Guid]::NewGuid().ToString('N')
    $StageDir = Join-Path ([IO.Path]::GetTempPath()) "openpocketbase-pb-restore-$Suffix"
    $OldDataDir = Join-Path ([IO.Path]::GetTempPath()) "openpocketbase-pb-restore-old-$Suffix"
    Assert-PinnedPocketBaseRestoreWorkingPath -Path $StageDir -ExpectedPattern '^openpocketbase-pb-restore-[0-9a-f]{32}$'
    Assert-PinnedPocketBaseRestoreWorkingPath -Path $OldDataDir -ExpectedPattern '^openpocketbase-pb-restore-old-[0-9a-f]{32}$'

    $ExtractedDir = Join-Path $StageDir 'extracted'
    $PreservedBackupsDir = Join-Path $StageDir 'backups'
    $DataMoved = $false
    $RestoreInstalled = $false
    New-Item -ItemType Directory -Path $ExtractedDir | Out-Null

    try {
        Copy-Item -LiteralPath $BackupDir -Destination $PreservedBackupsDir -Recurse

        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $Archive = [IO.Compression.ZipFile]::OpenRead($BackupPath)
        try {
            $ExtractedRoot = [IO.Path]::GetFullPath($ExtractedDir).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
            $ExtractedPrefix = $ExtractedRoot + [IO.Path]::DirectorySeparatorChar
            foreach ($Entry in $Archive.Entries) {
                if ([string]::IsNullOrWhiteSpace($Entry.FullName) -or $Entry.FullName -match '[\x00-\x1f\x7f]') {
                    throw 'The PocketBase restore archive contains an unsafe entry.'
                }

                $RelativePath = $Entry.FullName.Replace([char]'/', [IO.Path]::DirectorySeparatorChar)
                if ([IO.Path]::IsPathRooted($RelativePath)) {
                    throw 'The PocketBase restore archive contains an absolute entry.'
                }

                $TargetPath = [IO.Path]::GetFullPath((Join-Path $ExtractedRoot $RelativePath))
                if (-not $TargetPath.StartsWith($ExtractedPrefix, [StringComparison]::OrdinalIgnoreCase)) {
                    throw 'The PocketBase restore archive contains a traversal entry.'
                }

                if ([string]::IsNullOrEmpty($Entry.Name)) {
                    New-Item -ItemType Directory -Path $TargetPath -Force | Out-Null
                    continue
                }

                $TargetParent = Split-Path -Parent $TargetPath
                New-Item -ItemType Directory -Path $TargetParent -Force | Out-Null
                [IO.Compression.ZipFileExtensions]::ExtractToFile($Entry, $TargetPath, $false)
            }
        }
        finally {
            $Archive.Dispose()
        }

        if (-not [IO.File]::Exists((Join-Path $ExtractedDir 'data.db'))) {
            throw 'The PocketBase restore archive does not contain data.db.'
        }

        Move-Item -LiteralPath $ResolvedDataDir -Destination $OldDataDir
        $DataMoved = $true
        Move-Item -LiteralPath $ExtractedDir -Destination $ResolvedDataDir
        Move-Item -LiteralPath $PreservedBackupsDir -Destination (Join-Path $ResolvedDataDir 'backups')
        $RestoreInstalled = $true
    }
    finally {
        if (-not $RestoreInstalled -and $DataMoved) {
            if ([IO.Directory]::Exists($ResolvedDataDir)) {
                Remove-Item -LiteralPath $ResolvedDataDir -Recurse -Force
            }
            Move-Item -LiteralPath $OldDataDir -Destination $ResolvedDataDir
            $DataMoved = $false
        }

        if ($RestoreInstalled) {
            Remove-PinnedPocketBaseRestoreDirectory -Path $OldDataDir -ExpectedPattern '^openpocketbase-pb-restore-old-[0-9a-f]{32}$'
        }
        Remove-PinnedPocketBaseRestoreDirectory -Path $StageDir -ExpectedPattern '^openpocketbase-pb-restore-[0-9a-f]{32}$'
    }
}
