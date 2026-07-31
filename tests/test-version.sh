#!/usr/bin/env sh
set -eu

if [ "$#" -ne 2 ]; then
  echo "usage: test-version.sh PROGRAM VERSION" >&2
  exit 2
fi

program=$1
version=$2
expected="Silent Compose ${version}"
actual=$("${program}" --version)

if [ "${actual}" != "${expected}" ]; then
  echo "expected: ${expected}" >&2
  echo "actual:   ${actual}" >&2
  exit 1
fi
