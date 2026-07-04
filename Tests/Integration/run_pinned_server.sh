#!/usr/bin/env bash

set -euo pipefail

readonly pb_version="0.39.11"
readonly pb_sha256="9da6fbe11e82c5b1704e56f7457b24682e01c510206c29b798a458119fa2be20"
readonly script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly cache_root="${TMPDIR:-/tmp}/openpocketbase-v${pb_version}"
readonly archive_path="${cache_root}/pocketbase.zip"
readonly binary_path="${cache_root}/pocketbase"
readonly data_dir="$(mktemp -d "${TMPDIR:-/tmp}/openpocketbase-pb-data.XXXXXX")"
readonly listen_port="${OPENPOCKETBASE_TEST_PORT:-18091}"
readonly asset_url="https://github.com/pocketbase/pocketbase/releases/download/v${pb_version}/pocketbase_${pb_version}_darwin_arm64.zip"

mkdir -p "${cache_root}"
if [[ ! -f "${archive_path}" ]]; then
  curl --fail --location --retry 3 --output "${archive_path}" "${asset_url}"
fi

printf '%s  %s\n' "${pb_sha256}" "${archive_path}" | shasum -a 256 --check

if [[ ! -x "${binary_path}" ]]; then
  unzip -jo "${archive_path}" pocketbase -d "${cache_root}"
  chmod +x "${binary_path}"
fi

printf 'PocketBase v%s fixture listening at http://127.0.0.1:%s\n' "${pb_version}" "${listen_port}"
printf 'Set OPENPOCKETBASE_TEST_URL=http://127.0.0.1:%s for Unreal integration tests.\n' "${listen_port}"

exec "${binary_path}" serve \
  --http="127.0.0.1:${listen_port}" \
  --dev=false \
  --dir="${data_dir}" \
  --migrationsDir="${script_dir}/pb_migrations"
