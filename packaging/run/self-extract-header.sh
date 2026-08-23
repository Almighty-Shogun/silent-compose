#!/usr/bin/env sh
set -eu

tmp=$(mktemp -d)
cleanup() { rm -rf "$tmp"; }
trap cleanup EXIT INT TERM

sed '1,/^__SC_ARCHIVE_BELOW__$/d' "$0" | tar -xz -C "$tmp"
dir=$(find "$tmp" -mindepth 1 -maxdepth 1 -type d | head -1)
"$dir/install.sh" "$@"
exit $?

__SC_ARCHIVE_BELOW__
