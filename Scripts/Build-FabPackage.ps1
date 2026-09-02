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
    New-Item -ItemType Directory -Path (Join-Path $StagePlugin "Content") -Force | Out-Null

    Copy-RequiredItem "$PluginName.uplugin" $StagePlugin
    Copy-RequiredItem "Readme.md" $StagePlugin
    Copy-RequiredItem "LICENSE" $StagePlugin
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
    $StagedDescriptor | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $StagedDescriptorPath -Encoding UTF8

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
            "$PluginName/Readme.md",
            "$PluginName/LICENSE"
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
            "/Source/OpenPocketBaseSDKPublicHeaders/"
        )
        foreach ($Fragment in $ForbiddenFragments) {
            if ($Entries | Where-Object { $_.Contains($Fragment) }) {
                throw "Archive contains forbidden path fragment '$Fragment'."
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
