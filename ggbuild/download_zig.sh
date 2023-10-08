#! /usr/bin/env bash

set -eou pipefail

cd "$(dirname "$0")"

version="$(cat zig_version.txt)"

if [ -d "zig-$version" ]; then
	exit
fi

wget --continue "https://ziglang.org/download/$version/zig-linux-x86_64-$version.tar.xz"
echo "2d00e789fec4f71790a6e7bf83ff91d564943c5ee843c5fd966efc474b423047 zig-linux-x86_64-$version.tar.xz" | sha256sum -c
tar xf "zig-linux-x86_64-$version.tar.xz"
rm "zig-linux-x86_64-$version.tar.xz"
mv "zig-linux-x86_64-$version/" "zig-$version"
