#!/bin/sh

set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/hide-tests.XXXXXX")
trap 'rm -rf -- "$tmp"' EXIT HUP INT TERM

cd "$root"
printf 'hide image plugin test payload\n' > "$tmp/payload"

for format in bmp jpeg png tiff webp
do
	./hide "original.$format" "$tmp/payload" "$tmp/hidden.$format" >/dev/null 2>&1
	./hide "$tmp/hidden.$format" "$tmp/recovered.$format" >/dev/null 2>&1
	cmp "$tmp/payload" "$tmp/recovered.$format"
done

: > "$tmp/empty"
./hide original.png "$tmp/empty" "$tmp/empty.png" >/dev/null 2>&1
./hide "$tmp/empty.png" "$tmp/empty.recovered" >/dev/null 2>&1
test ! -s "$tmp/empty.recovered"

printf 'not an image\n' > "$tmp/unsupported"
if ./hide "$tmp/unsupported" "$tmp/payload" "$tmp/unsupported.out" >/dev/null 2>&1
then
	echo "Unsupported input unexpectedly succeeded" >&2
	exit 1
fi

head -c 12 original.png > "$tmp/truncated.png"
if ./hide "$tmp/truncated.png" "$tmp/rejected" >/dev/null 2>&1
then
	echo "Truncated PNG unexpectedly succeeded" >&2
	exit 1
else
	status=$?
	if [ "$status" -ge 128 ]
	then
		echo "Truncated PNG terminated abnormally (status $status)" >&2
		exit 1
	fi
fi

echo "hide: main application and BMP/JPEG/PNG/TIFF/WebP round trips passed"
