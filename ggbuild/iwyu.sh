#! /bin/sh
include-what-you-use \
	-D_LIBCPP_HAS_NO_VENDOR_AVAILABILITY_ANNOTATIONS \
	-D__PRFCHIINTRIN_H \
	-Iggbuild/zig-0.11.0/lib/libcxx/include \
	-Iggbuild/zig-0.11.0/lib/include \
	-Iggbuild/zig-0.11.0/lib/libc/musl/include \
	-Iggbuild/zig-0.11.0/lib/libc/include/x86_64-linux-musl \
	-Iggbuild/zig-0.11.0/lib/libc/include/generic-musl \
	-Wno-shift-op-parentheses \
	-Isource \
	-Ilibs \
	-std=c++20 "$1"