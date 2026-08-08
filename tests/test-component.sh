#!/usr/bin/env sh
set -eu

if [ "$#" -ne 1 ]; then
  echo "usage: test-component.sh COMPONENT_XML" >&2
  exit 2
fi

component_xml=$1

grep -Fx "      <layout>us</layout>" "${component_xml}" >/dev/null
grep -Fx "      <layout_variant>intl</layout_variant>" "${component_xml}" >/dev/null
grep -Fx "      <layout_option>lv3:ralt_switch</layout_option>" "${component_xml}" >/dev/null
