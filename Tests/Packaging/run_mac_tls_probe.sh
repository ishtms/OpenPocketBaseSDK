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

cleanup()
{
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
    -AdditionalCookerOptions=-SkipZenStore \
    > "$probe_root/package.log" 2>&1

probe_binary="$probe_root/Saved/StagedBuilds/Mac/OpenPocketBaseSDKTests.app/Contents/MacOS/OpenPocketBaseSDKTests"
OPENPOCKETBASE_PACKAGE_TLS_ORIGIN=${OPENPOCKETBASE_PACKAGE_TLS_ORIGIN:-https://api.github.com} \
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

probe_succeeded=1
print "Packaged Mac HTTPS probe passed."
