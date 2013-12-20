#!/bin/sh

srcdir="$(dirname "$0")"
[ "$(echo "$srcdir" | cut -c1)" = '/' ] || srcdir="$PWD/$srcdir"

die() { echo "$*"; exit 1; }

# Import the makerelease.lib
# http://bues.ch/gitweb?p=misc.git;a=blob_plain;f=makerelease.lib;hb=HEAD
for path in $(echo "$PATH" | tr ':' ' '); do
	[ -f "$MAKERELEASE_LIB" ] && break
	MAKERELEASE_LIB="$path/makerelease.lib"
done
[ -f "$MAKERELEASE_LIB" ] && . "$MAKERELEASE_LIB" || die "makerelease.lib not found."

hook_get_version()
{
	local file="$1/host/pymoistcontrol/util.py"
	local v="$(cat "$file" | grep -e VERSION | head -n1 | awk '{print $3;}' | cut -d'"' -f2)"
	version=v"$v"
}

hook_testbuild()
{
	for f in $(find . -name '*.c' -o -name '*.h'); do
		# Check CR/LF
		file "$f" | grep -qEe '(CRLF line terminators)|(assembler source text)' || {
			die "ERROR: '$(basename "$f")' is not in DOS format."
		}
	done
}

hook_pre_tarball()
{
	# Build the hex file before packing the tarball.
	default_hook_testbuild "$2"/firmware/
	cp "$2"/firmware/moistcontrol.hex "$2"/

	# Move the documentation
	mv "$2"/schematics/moistcontrol.pdf "$2"/schaltplan.pdf
	mv "$2"/doc/dokumentation.pdf "$2"/

	cd "$2"/firmware
	make clean
}

project=moistcontrol
default_compress=zip
makerelease "$@"
