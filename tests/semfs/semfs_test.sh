#!/bin/sh
# ATF test script for semfs

. /usr/share/atf/sh.atf.inc

atf_test_case inject_and_read
inject_and_read_body() {
    echo "test.key:passed" > /dev/semfsctl
    atf_check -s exit:0 -o match:"test.key:passed" cat /dev/semfsctl
}

atf_test_case readonly_toggle
readonly_toggle_body() {
    sysctl vfs.semfs.readonly=1
    echo "test.readonly:fail" > /dev/semfsctl 2>/dev/null && atf_fail "Write unexpectedly allowed"
    sysctl vfs.semfs.readonly=0
}

atf_init_test_cases() {
    atf_add_test_case inject_and_read
    atf_add_test_case readonly_toggle
}