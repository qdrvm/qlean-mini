#!/bin/sh -eu
#
# Copyright Quadrivium LLC
# All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#

path="$1"
script_dir=$(dirname "$0")
if [ -e "$path" ]; then
  actual="$(cat "$path")"
else
  actual=""
fi
version=$("$script_dir/get_version.sh")
expected=$(cat <<EOF
// Auto-generated file
#include <string>

namespace lean {
  const std::string &buildVersion() {
    static const std::string buildVersion("$version");
    return buildVersion;
  }
}  // namespace lean
EOF
)
if [ "$actual" != "$expected" ]; then
  echo "$expected" > "$path"
fi
