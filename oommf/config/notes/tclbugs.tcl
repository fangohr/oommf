#!/bin/sh
# \
exec wish "$0" ${1+"$@"}
#
# This script tests for the presence of bugs found in some releases
# of Tcl/Tk which break OOMMF.  Evaluate this script with
# wish to see if your Tcl/TK installation has those bugs
# fixed.  The output should be lines which look like:
#
# Test 1: Success
# 
# If any test reports failure, or if the program crashes, your
# Tcl/TK installation is buggy, and cannot run OOMMF.

proc fooGet {varName} {
  upvar $varName v
  set v 0
}

set foo(Get) fooGet

proc snafu {class proc args} {
  upvar #0 $class def
  uplevel $def($proc) $args
}

interp alias {} foo {} snafu foo

proc bar {} {
  set v bad
  foo Get v
  if {!$v} {
    puts "Test 1: Success"
  }
}

if {[catch bar msg]} {
   puts "Test 1: Fail: $msg"
}

if {![catch {package require Tk}]} {
    set t [text .t]
    $t insert end \n
    $t window create end -window [button $t.b]
    $t index end-2c
    puts "Test 2: Success"
}

exit 0
