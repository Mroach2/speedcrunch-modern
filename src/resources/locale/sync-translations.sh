#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
src_dir="$(cd "${script_dir}/../.." && pwd)"
pro_file="${src_dir}/speedcrunch.pro"

if [[ ! -f "${pro_file}" ]]; then
    echo "error: cannot find ${pro_file}" >&2
    exit 1
fi

find_tool() {
    local override="${1:-}"
    shift
    if [[ -n "${override}" ]] && command -v "${override}" >/dev/null 2>&1; then
        echo "${override}"
        return 0
    fi

    local candidate
    for candidate in "$@"; do
        if command -v "${candidate}" >/dev/null 2>&1; then
            echo "${candidate}"
            return 0
        fi
    done

    return 1
}

lupdate_bin="$(find_tool "${LUPDATE:-}" lupdate-qt6 lupdate)" || {
    echo "error: could not find lupdate (tried: lupdate-qt6, lupdate)" >&2
    exit 1
}

lrelease_bin="$(find_tool "${LRELEASE:-}" lrelease-qt6 lrelease)" || {
    echo "error: could not find lrelease (tried: lrelease-qt6, lrelease)" >&2
    exit 1
}

echo "Using lupdate: ${lupdate_bin}"
echo "Using lrelease: ${lrelease_bin}"
echo "Project file: ${pro_file}"

cd "${src_dir}"
"${lupdate_bin}" -no-obsolete speedcrunch.pro
"${lrelease_bin}" speedcrunch.pro

echo "Done: TS updated and QM rebuilt."
