#!/usr/bin/env bash

set -euo pipefail

readonly pb_version="0.39.11"
readonly pb_sha256="9da6fbe11e82c5b1704e56f7457b24682e01c510206c29b798a458119fa2be20"
readonly script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly cache_root="${TMPDIR:-/tmp}/openpocketbase-v${pb_version}"
readonly archive_path="${cache_root}/pocketbase.zip"
readonly binary_path="${cache_root}/pocketbase"
readonly data_dir="$(mktemp -d "${TMPDIR:-/tmp}/openpocketbase-pb-data.XXXXXX")"
readonly credential_file="${data_dir}/admin-credentials.json"
readonly listen_port="${OPENPOCKETBASE_TEST_PORT:-18091}"
readonly asset_url="https://github.com/pocketbase/pocketbase/releases/download/v${pb_version}/pocketbase_${pb_version}_darwin_arm64.zip"
readonly fixture_superuser_email="openpocketbase-fixture@example.com"

cleanup() {
  if [[ "${data_dir}" == "${TMPDIR:-/tmp}"/openpocketbase-pb-data.* ]]; then
    rm -rf -- "${data_dir}"
  fi
}

trap cleanup EXIT

mkdir -p "${cache_root}"
if [[ ! -f "${archive_path}" ]]; then
  curl --fail --location --retry 3 --output "${archive_path}" "${asset_url}"
fi

printf '%s  %s\n' "${pb_sha256}" "${archive_path}" | shasum -a 256 --check

if [[ ! -x "${binary_path}" ]]; then
  unzip -jo "${archive_path}" pocketbase -d "${cache_root}"
  chmod +x "${binary_path}"
fi

umask 077
fixture_superuser_password="$(openssl rand -hex 32)"
printf '{"email":"%s","password":"%s"}\n' \
  "${fixture_superuser_email}" \
  "${fixture_superuser_password}" > "${credential_file}"
export OPENPOCKETBASE_FIXTURE_SUPERUSER_EMAIL="${fixture_superuser_email}"
export OPENPOCKETBASE_FIXTURE_SUPERUSER_PASSWORD="${fixture_superuser_password}"

printf 'PocketBase v%s fixture listening at http://127.0.0.1:%s\n' "${pb_version}" "${listen_port}"
printf 'Set OPENPOCKETBASE_TEST_URL=http://127.0.0.1:%s for Unreal integration tests.\n' "${listen_port}"
printf 'Set OPENPOCKETBASE_ADMIN_CREDENTIAL_FILE=%s for privileged integration tests.\n' "${credential_file}"

"${binary_path}" serve \
  --http="127.0.0.1:${listen_port}" \
  --automigrate=false \
  --dev=false \
  --dir="${data_dir}" \
  --migrationsDir="${script_dir}/pb_migrations"
