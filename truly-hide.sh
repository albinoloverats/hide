#!/bin/sh

function usage
{
	echo "$0 <image> <file>"
	echo "$0 <image>"
	exit -1
}

function needed
{
	echo "You need encrypt installed, and valid defaults set in ~/.encryptrc"
	exit -1
}

test $# -ne 1 -a $# -ne 2 && usage
test -f $HOME/.encryptrc || needed

tmp=$(mktemp)
image=$1

if test $# -eq 2
then
	file=$2
	encrypt --nogui --raw $file $tmp

	out=$(basename $image)
	ext=${out##*.}
	out=${out%.*}

	./hide -f $image $tmp $out-steg.$ext
else
	./hide -f $image $tmp
	decrypt --nogui --raw $tmp .
fi

unlink $tmp
