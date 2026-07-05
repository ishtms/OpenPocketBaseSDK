#!/bin/zsh

set -euo pipefail

if [[ -z "${UE_ROOT:-}" ]]; then
    print -u2 "UE_ROOT must point to an Unreal Engine 5.8 installation."
    exit 2
fi

script_dir=${0:A:h}
sdk_root=${script_dir:h:h}
package_parent=${OPENPOCKETBASE_PACKAGE_PARENT:-${TMPDIR:-/tmp}}
probe_root=$(mktemp -d "${package_parent%/}/OpenPocketBaseSDKPackageHost.XXXXXX")
keep_package=${OPENPOCKETBASE_KEEP_PACKAGE:-0}
probe_succeeded=0
pocketbase_pid=""

cleanup()
{
    if [[ -n $pocketbase_pid ]]; then
        kill "$pocketbase_pid" 2>/dev/null || true
        wait "$pocketbase_pid" 2>/dev/null || true
    fi
    if [[ $probe_succeeded -eq 1 && $keep_package -ne 1 &&
          $probe_root == ${package_parent%/}/OpenPocketBaseSDKPackageHost.* ]]; then
        find "$probe_root" -depth -delete
    else
        print "Package probe artifacts: $probe_root"
    fi
}
trap cleanup EXIT

mkdir -p "$probe_root/Plugins/OpenPocketBaseSDK"
rsync -a "$sdk_root/Tests/HostProject/" "$probe_root/"
rsync -a \
    --exclude='.git' \
    --exclude='Binaries' \
    --exclude='DerivedDataCache' \
    --exclude='Intermediate' \
    --exclude='Saved' \
    --exclude='Tests/HostProject' \
    "$sdk_root/" \
    "$probe_root/Plugins/OpenPocketBaseSDK/"

"$UE_ROOT/Engine/Binaries/ThirdParty/DotNet/10.0/mac-arm64/dotnet" \
    "$UE_ROOT/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll" \
    OpenPocketBaseSDKTestsEditor Mac Development \
    -Project="$probe_root/OpenPocketBaseSDKTests.uproject" \
    -NoUBA \
    -WaitMutex \
    -NoHotReload \
    > "$probe_root/package.log" 2>&1

"$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun \
    -WaitForUATMutex \
    -project="$probe_root/OpenPocketBaseSDKTests.uproject" \
    -noP4 \
    -platform=Mac \
    -clientconfig=Development \
    -build \
    -cook \
    -stage \
    -pak \
    -unattended \
    -utf8output \
    -NoDebugInfo \
    -NoCompileEditor \
    -UbtArgs="-NoUBA -WaitMutex" \
    -AdditionalCookerOptions=-SkipZenStore \
    >> "$probe_root/package.log" 2>&1

probe_binary="$probe_root/Saved/StagedBuilds/Mac/OpenPocketBaseSDKTests.app/Contents/MacOS/OpenPocketBaseSDKTests"
transfer_port=${OPENPOCKETBASE_PACKAGE_TRANSFER_PORT:-18092}
OPENPOCKETBASE_TEST_PORT=$transfer_port \
    "$sdk_root/Tests/Integration/run_pinned_server.sh" \
    > "$probe_root/pocketbase.log" 2>&1 &
pocketbase_pid=$!
transfer_origin="http://127.0.0.1:$transfer_port"
transfer_ready=0
for attempt in {1..100}; do
    if curl --fail --silent "$transfer_origin/api/health" >/dev/null 2>&1; then
        transfer_ready=1
        break
    fi
    sleep 0.1
done
if [[ $transfer_ready -ne 1 ]]; then
    print -u2 "The pinned transfer server did not become ready."
    exit 3
fi

OPENPOCKETBASE_PACKAGE_TLS_ORIGIN=${OPENPOCKETBASE_PACKAGE_TLS_ORIGIN:-https://api.github.com} \
OPENPOCKETBASE_PACKAGE_TRANSFER_ORIGIN=$transfer_origin \
    "$probe_binary" \
    -unattended \
    -NullRHI \
    -nosplash \
    -stdout \
    -FullStdOutLogOutput \
    > "$probe_root/tls_probe.log" 2>&1

if ! grep -q 'OPENPOCKETBASE_PACKAGED_TLS_SUCCESS' "$probe_root/tls_probe.log"; then
    print -u2 "The packaged process did not report trusted HTTPS success."
    exit 3
fi

if ! grep -q 'OPENPOCKETBASE_PACKAGED_SECURE_STORAGE_SUCCESS' "$probe_root/tls_probe.log"; then
    print -u2 "The packaged process did not report secure-storage success."
    exit 4
fi

if ! grep -q 'OPENPOCKETBASE_PACKAGED_TRANSFER_SUCCESS' "$probe_root/tls_probe.log"; then
    print -u2 "The packaged process did not report transfer success."
    exit 5
fi

probe_succeeded=1
print "Packaged Mac HTTPS, secure-storage, and transfer probes passed."
