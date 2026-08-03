[CmdletBinding()]
param(
    [string]$UnrealRoot = $env:UE_ROOT,
    [string]$BaseUrl = 'http://127.0.0.1:18094',
    [switch]$KeepPackage
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ScriptDir = [IO.Path]::GetFullPath((Split-Path -Parent $MyInvocation.MyCommand.Path))
$RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $ScriptDir '..\..'))
$TempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd(
    [IO.Path]::DirectorySeparatorChar,
    [IO.Path]::AltDirectorySeparatorChar)
$ProbeRoot = [IO.Path]::GetFullPath((Join-Path $TempRoot (
    'OPBW-{0}' -f [Guid]::NewGuid().ToString('N'))))
$ProbeSucceeded = $false

function Remove-VerifiedProbeRoot {
    param([string]$Path)

    $FullPath = [IO.Path]::GetFullPath($Path)
    $Parent = [IO.Path]::GetFullPath((Split-Path -Parent $FullPath)).TrimEnd(
        [IO.Path]::DirectorySeparatorChar,
        [IO.Path]::AltDirectorySeparatorChar)
    $Leaf = Split-Path -Leaf $FullPath
    if (-not $Parent.Equals($TempRoot, [StringComparison]::OrdinalIgnoreCase) -or
        $Leaf -notmatch '^OPBW-[0-9a-f]{32}$') {
        throw "Refusing to remove unexpected package probe directory '$FullPath'."
    }
    if ([IO.Directory]::Exists($FullPath)) {
        Remove-Item -LiteralPath $FullPath -Recurse -Force
    }
}

function Copy-TrackedWorktree {
    param(
        [string]$SourceRoot,
        [string]$HostDestination,
        [string]$PluginDestination
    )

    $TrackedFiles = @(& git -C $SourceRoot -c core.quotepath=false ls-files --full-name)
    if ($LASTEXITCODE -ne 0) {
        throw "Git could not enumerate tracked files under '$SourceRoot'."
    }

    $HostPrefix = 'Tests/HostProject/'
    foreach ($TrackedFile in $TrackedFiles) {
        $RelativePath = $TrackedFile.Replace('\', '/')
        $SourcePath = Join-Path $SourceRoot $RelativePath
        if (-not [IO.File]::Exists($SourcePath)) {
            continue
        }

        if ($RelativePath.StartsWith($HostPrefix, [StringComparison]::Ordinal)) {
            $DestinationPath = Join-Path $HostDestination $RelativePath.Substring($HostPrefix.Length)
        }
        else {
            $DestinationPath = Join-Path $PluginDestination $RelativePath
        }

        [IO.Directory]::CreateDirectory((Split-Path -Parent $DestinationPath)) | Out-Null
        [IO.File]::Copy($SourcePath, $DestinationPath, $true)
    }
}

try {
    if ([string]::IsNullOrWhiteSpace($UnrealRoot)) {
        $UnrealRoot = 'C:\Program Files\Epic Games\UE_5.8'
    }
    $UnrealRoot = [IO.Path]::GetFullPath($UnrealRoot)
    $Ubt = Join-Path $UnrealRoot 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll'
    $Dotnet = Join-Path $UnrealRoot 'Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe'
    if (-not [IO.File]::Exists($Dotnet)) {
        $Dotnet = Join-Path $UnrealRoot 'Engine\Binaries\ThirdParty\DotNet\8.0.300\win-x64\dotnet.exe'
    }
    $RunUat = Join-Path $UnrealRoot 'Engine\Build\BatchFiles\RunUAT.bat'
    foreach ($RequiredPath in @($Ubt, $Dotnet, $RunUat)) {
        if (-not [IO.File]::Exists($RequiredPath)) {
            throw "Required Unreal tool was not found at '$RequiredPath'."
        }
    }

    $Health = Invoke-RestMethod -Method Get -Uri "$BaseUrl/api/health" -TimeoutSec 3
    $FixtureSignature = Invoke-RestMethod -Method Get -Uri "$BaseUrl/api/openpocketbase-test/periodic" -TimeoutSec 3
    if ($Health.code -ne 200 -or $FixtureSignature -ne "chunk-0`nchunk-1`nchunk-2`nchunk-3`n") {
        throw "The server at '$BaseUrl' is not the pinned PocketBase fixture."
    }

    Copy-TrackedWorktree `
        -SourceRoot $RepositoryRoot `
        -HostDestination $ProbeRoot `
        -PluginDestination (Join-Path $ProbeRoot 'Plugins\OpenPocketBaseSDK')

    $StagedPluginDescriptorPath = Join-Path $ProbeRoot 'Plugins\OpenPocketBaseSDK\OpenPocketBaseSDK.uplugin'
    $StagedPluginDescriptor = Get-Content -LiteralPath $StagedPluginDescriptorPath -Raw | ConvertFrom-Json
    $StagedPluginDescriptor.Modules = @($StagedPluginDescriptor.Modules | Where-Object {
        $_.Name -notin @('OpenPocketBaseSDKTests', 'OpenPocketBaseSDKPublicHeaders')
    })
    $StagedPluginDescriptorJson = $StagedPluginDescriptor | ConvertTo-Json -Depth 20
    Set-Content -LiteralPath $StagedPluginDescriptorPath -Value $StagedPluginDescriptorJson -Encoding UTF8

    $Project = Join-Path $ProbeRoot 'OpenPocketBaseSDKTests.uproject'
    $PackageLog = Join-Path $ProbeRoot 'package.log'
    $EditorBuildArguments = @(
        $Ubt,
        'OpenPocketBaseSDKTestsEditor',
        'Win64',
        'Development',
        "-Project=$Project",
        '-NoUBA',
        '-WaitMutex',
        '-NoHotReload'
    )
    & $Dotnet @EditorBuildArguments *> $PackageLog
    if ($LASTEXITCODE -ne 0) {
        throw "The Win64 package probe editor build failed. See '$PackageLog'."
    }

    $PackageArguments = @(
        'BuildCookRun',
        '-WaitForUATMutex',
        "-project=$Project",
        '-noP4',
        '-platform=Win64',
        '-clientconfig=Development',
        '-build',
        '-cook',
        '-stage',
        '-pak',
        '-unattended',
        '-utf8output',
        '-NoDebugInfo',
        '-NoCompileEditor',
        '-UbtArgs=-NoUBA -WaitMutex',
        '-AdditionalCookerOptions=-SkipZenStore'
    )
    & $RunUat @PackageArguments *>> $PackageLog
    if ($LASTEXITCODE -ne 0) {
        throw "The Win64 package probe packaging step failed. See '$PackageLog'."
    }

    $ProbeBinary = Join-Path $ProbeRoot 'Saved\StagedBuilds\Windows\OpenPocketBaseSDKTests.exe'
    if (-not [IO.File]::Exists($ProbeBinary)) {
        throw "The staged probe executable was not found at '$ProbeBinary'."
    }

    $ProbeLog = Join-Path $ProbeRoot 'streaming_probe.log'
    $ProbeErrorLog = Join-Path $ProbeRoot 'streaming_probe.error.log'
    $PreviousOrigin = $env:OPENPOCKETBASE_PACKAGE_RAW_STREAM_ORIGIN
    try {
        $env:OPENPOCKETBASE_PACKAGE_RAW_STREAM_ORIGIN = $BaseUrl
        $ProcessArguments = @{
            FilePath = $ProbeBinary
            ArgumentList = @('-unattended', '-NullRHI', '-nosplash', '-stdout', '-FullStdOutLogOutput')
            PassThru = $true
            WindowStyle = 'Hidden'
            RedirectStandardOutput = $ProbeLog
            RedirectStandardError = $ProbeErrorLog
        }
        $Process = Start-Process @ProcessArguments
    }
    finally {
        $env:OPENPOCKETBASE_PACKAGE_RAW_STREAM_ORIGIN = $PreviousOrigin
    }

    if (-not $Process.WaitForExit(30000)) {
        Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
        throw "The packaged Win64 streaming probe timed out. See '$ProbeLog'."
    }
    $Process.WaitForExit()
    $Process.Refresh()
    $ProbeExitCode = $Process.ExitCode

    $ProbeOutput = @(
        if ([IO.File]::Exists($ProbeLog)) { Get-Content -LiteralPath $ProbeLog -Raw }
        if ([IO.File]::Exists($ProbeErrorLog)) { Get-Content -LiteralPath $ProbeErrorLog -Raw }
    ) -join "`n"
    if ($null -ne $ProbeExitCode -and $ProbeExitCode -ne 0) {
        throw "The packaged Win64 streaming probe exited with code $ProbeExitCode. See '$ProbeLog'."
    }
    foreach ($Marker in @(
        'OPENPOCKETBASE_PACKAGED_RAW_STREAMING_STARTED',
        'OPENPOCKETBASE_PACKAGED_STREAMING_TIMEOUT_STARTED',
        'OPENPOCKETBASE_PACKAGED_STREAMING_SUCCESS'
    )) {
        if (-not $ProbeOutput.Contains($Marker)) {
            throw "The packaged Win64 probe did not report '$Marker'. See '$ProbeLog'."
        }
    }

    $ProbeSucceeded = $true
    Write-Output 'Packaged Win64 incremental streaming probe passed.'
}
finally {
    if ($ProbeSucceeded -and -not $KeepPackage) {
        Remove-VerifiedProbeRoot $ProbeRoot
    }
    else {
        Write-Output "Package probe artifacts: $ProbeRoot"
    }
}
