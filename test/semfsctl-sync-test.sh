#!/bin/sh
# Regression test for semfs module

echo "Injecting test keys..."
cat <<EOF > /dev/semfsctl
test.one:alpha
test.two:beta
test.three:gamma
EOF

echo "Reading back entries..."
cat /dev/semfsctl | grep test.

echo "Toggling readonly..."
sysctl vfs.semfs.readonly=1
echo "test.readonly:locked" > /dev/semfsctl 2>/dev/null && echo "Unexpected write allowed!" || echo "Write blocked (expected)."
sysctl vfs.semfs.readonly=0

echo "Clearing state..."
echo "!RESET" > /dev/semfsctl