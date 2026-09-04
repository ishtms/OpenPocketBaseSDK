[CmdletBinding()]
param(
    [string]$UnrealRoot = "C:\Program Files\Epic Games\UE_5.8",
    [string]$OutputRoot = "",
    [switch]$SkipBuildPlugin
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$PluginName = "OpenPocketBaseSDK"
$SourceDescriptorPath = Join-Path $RepoRoot "$PluginName.uplugin"
$SourceDescriptor = Get-Content -LiteralPath $SourceDescriptorPath -Raw | ConvertFrom-Json
$VersionName = [string]$SourceDescriptor.VersionName

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $RepoRoot "Dist\Fab"
}
$OutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null

$DriveRoot = [System.IO.Path]::GetPathRoot($RepoRoot)
$TempBase = Join-Path $DriveRoot "OPBFabTemp"
$TempLeaf = [guid]::NewGuid().ToString("N")
$TempRoot = Join-Path $TempBase $TempLeaf
$StageRoot = Join-Path $TempRoot "SourcePackage"
$StagePlugin = Join-Path $StageRoot $PluginName
$ValidationRoot = Join-Path $TempRoot "BuildPluginValidation"

function Copy-RequiredItem {
    param(
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    $Source = Join-Path $RepoRoot $RelativePath
    if (-not (Test-Path -LiteralPath $Source)) {
        throw "Required package input is missing: $RelativePath"
    }
    Copy-Item -LiteralPath $Source -Destination $Destination -Recurse -Force
}

try {
    New-Item -ItemType Directory -Path $StagePlugin -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $StagePlugin "Source") -Force | Out-Null

    Copy-RequiredItem "$PluginName.uplugin" $StagePlugin
    Copy-RequiredItem "Scripts\FabPackageReadme.md" (Join-Path $StagePlugin "Readme.md")
    Copy-RequiredItem "Config" $StagePlugin
    Copy-RequiredItem "Resources" $StagePlugin

    $Modules = @(
        "OpenPocketBaseSDK",
        "OpenPocketBaseSDKOffline",
        "OpenPocketBaseSDKAdmin",
        "OpenPocketBaseSDKEditor"
    )
    foreach ($Module in $Modules) {
        Copy-RequiredItem "Source\$Module" (Join-Path $StagePlugin "Source")
    }

    $StagedDescriptorPath = Join-Path $StagePlugin "$PluginName.uplugin"
    $StagedDescriptor = Get-Content -LiteralPath $StagedDescriptorPath -Raw | ConvertFrom-Json
    $StagedDescriptor.Modules = @($StagedDescriptor.Modules | Where-Object {
        $_.Name -notin @("OpenPocketBaseSDKTests", "OpenPocketBaseSDKPublicHeaders")
    })
    $StagedDescriptor.Installed = $false
    if ($null -eq $StagedDescriptor.PSObject.Properties["EngineVersion"]) {
        $StagedDescriptor | Add-Member -NotePropertyName "EngineVersion" -NotePropertyValue "5.8.0"
    } else {
        $StagedDescriptor.EngineVersion = "5.8.0"
    }
    if ($StagedDescriptor.CanContainContent) {
        throw "Fab package descriptor must set CanContainContent to false when no content is shipped."
    }
    foreach ($Module in @($StagedDescriptor.Modules)) {
        $AllowList = $Module.PSObject.Properties["PlatformAllowList"]
        $DenyList = $Module.PSObject.Properties["PlatformDenyList"]
        $HasAllowList = $null -ne $AllowList -and @($AllowList.Value).Count -gt 0
        $HasDenyList = $null -ne $DenyList -and @($DenyList.Value).Count -gt 0
        if (-not $HasAllowList -and -not $HasDenyList) {
            throw "Module '$($Module.Name)' has no populated PlatformAllowList or PlatformDenyList."
        }
    }
    $StagedDescriptor | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $StagedDescriptorPath -Encoding UTF8

    $CodeExtensions = @(".c", ".cc", ".cpp", ".h", ".hpp", ".mm", ".cs")
    $MissingCopyright = @(
        Get-ChildItem -LiteralPath (Join-Path $StagePlugin "Source") -Recurse -File |
            Where-Object {
                $_.Extension -in $CodeExtensions -and
                ((Get-Content -LiteralPath $_.FullName -TotalCount 5) -join "`n") -notmatch
                    "Copyright\s+2026\s+Ishtmeet Singh"
            }
    )
    if ($MissingCopyright.Count -gt 0) {
        $RelativeMissing = @($MissingCopyright | ForEach-Object {
            $_.FullName.Substring($StagePlugin.Length).TrimStart("\", "/")
        })
        throw "Code files are missing the required copyright header: $($RelativeMissing -join ', ')"
    }

    if (-not $SkipBuildPlugin) {
        $RunUAT = Join-Path $UnrealRoot "Engine\Build\BatchFiles\RunUAT.bat"
        if (-not (Test-Path -LiteralPath $RunUAT)) {
            throw "RunUAT was not found at '$RunUAT'. Pass -UnrealRoot or use -SkipBuildPlugin."
        }

        & $RunUAT BuildPlugin "-Plugin=$StagedDescriptorPath" "-Package=$ValidationRoot" -Rocket
        if ($LASTEXITCODE -ne 0) {
            throw "Unreal BuildPlugin validation failed with exit code $LASTEXITCODE."
        }
    }

    $ZipName = "$PluginName-$VersionName-UE5.8.zip"
    $ZipPath = Join-Path $OutputRoot $ZipName
    $ChecksumPath = "$ZipPath.sha256"
    Remove-Item -LiteralPath $ZipPath -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $ChecksumPath -Force -ErrorAction SilentlyContinue

    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $ZipStream = [System.IO.File]::Open($ZipPath, [System.IO.FileMode]::CreateNew)
    try {
        $ZipArchive = New-Object System.IO.Compression.ZipArchive(
            $ZipStream,
            [System.IO.Compression.ZipArchiveMode]::Create,
            $true
        )
        try {
            foreach ($Item in Get-ChildItem -LiteralPath $StageRoot -Recurse -Force) {
                $RelativePath = $Item.FullName.Substring($StageRoot.Length).TrimStart("\", "/").Replace("\", "/")
                if ($Item.PSIsContainer) {
                    $null = $ZipArchive.CreateEntry("$RelativePath/")
                    continue
                }

                $Entry = $ZipArchive.CreateEntry(
                    $RelativePath,
                    [System.IO.Compression.CompressionLevel]::Optimal
                )
                $Entry.LastWriteTime = $Item.LastWriteTime
                $EntryStream = $Entry.Open()
                $FileStream = [System.IO.File]::OpenRead($Item.FullName)
                try {
                    $FileStream.CopyTo($EntryStream)
                }
                finally {
                    $FileStream.Dispose()
                    $EntryStream.Dispose()
                }
            }
        }
        finally {
            $ZipArchive.Dispose()
        }
    }
    finally {
        $ZipStream.Dispose()
    }

    $Archive = [System.IO.Compression.ZipFile]::OpenRead($ZipPath)
    try {
        $Entries = @($Archive.Entries | ForEach-Object { $_.FullName.Replace("\", "/") })
        $UnexpectedRoots = @($Entries | ForEach-Object { ($_ -split "/")[0] } | Sort-Object -Unique | Where-Object { $_ -ne $PluginName })
        if ($UnexpectedRoots.Count -gt 0) {
            throw "Archive contains unexpected top-level entries: $($UnexpectedRoots -join ', ')"
        }

        $RequiredEntries = @(
            "$PluginName/$PluginName.uplugin",
            "$PluginName/Config/FilterPlugin.ini",
            "$PluginName/Resources/Icon128.png",
            "$PluginName/Readme.md"
        )
        foreach ($RequiredEntry in $RequiredEntries) {
            if ($RequiredEntry -notin $Entries) {
                throw "Archive is missing required entry '$RequiredEntry'."
            }
        }

        foreach ($Module in $Modules) {
            if (-not ($Entries | Where-Object { $_ -like "$PluginName/Source/$Module/*" })) {
                throw "Archive is missing source module '$Module'."
            }
        }

        $ForbiddenFragments = @(
            "/.git/",
            "/Binaries/",
            "/Intermediate/",
            "/Tests/",
            "/Website/",
            "/Source/OpenPocketBaseSDKTests/",
            "/Source/OpenPocketBaseSDKPublicHeaders/",
            "/.codex/",
            "/.agents/",
            "/.claude/",
            "/.cursor/",
            "/temp/",
            "/tmp/"
        )
        foreach ($Fragment in $ForbiddenFragments) {
            if ($Entries | Where-Object { $_.Contains($Fragment) }) {
                throw "Archive contains forbidden path fragment '$Fragment'."
            }
        }

        $ForbiddenEntryPatterns = @(
            "(^|/)(LICENSE|LICENCE|COPYING|NOTICE)(\..*)?$",
            "(^|/)(AGENTS|CLAUDE|CODEX)\.md$",
            "(^|/)copilot-instructions\.md$"
        )
        foreach ($Pattern in $ForbiddenEntryPatterns) {
            if ($Entries | Where-Object { $_ -match $Pattern }) {
                throw "Archive contains a forbidden file matching '$Pattern'."
            }
        }
    }
    finally {
        $Archive.Dispose()
    }

    $Hash = Get-FileHash -LiteralPath $ZipPath -Algorithm SHA256
    "$($Hash.Hash.ToLowerInvariant())  $ZipName" | Set-Content -LiteralPath $ChecksumPath -Encoding ascii

    Write-Host "Fab source package: $ZipPath"
    Write-Host "SHA-256: $ChecksumPath"
    if ($SkipBuildPlugin) {
        Write-Warning "Unreal BuildPlugin validation was skipped."
    } else {
        Write-Host "Unreal BuildPlugin validation: passed"
    }
}
finally {
    $ResolvedTempBase = [System.IO.Path]::GetFullPath($TempBase).TrimEnd("\")
    $ResolvedTempRoot = [System.IO.Path]::GetFullPath($TempRoot)
    $ResolvedTempParent = [System.IO.Path]::GetFullPath((Split-Path -Parent $ResolvedTempRoot)).TrimEnd("\")
    $ResolvedTempLeaf = Split-Path -Leaf $ResolvedTempRoot
    if ($ResolvedTempParent.Equals($ResolvedTempBase, [System.StringComparison]::OrdinalIgnoreCase) -and
        $ResolvedTempLeaf -match "^[0-9a-f]{32}$") {
        Remove-Item -LiteralPath $ResolvedTempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
