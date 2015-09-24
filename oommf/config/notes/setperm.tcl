#!/bin/sh
# FILE: setperm.tcl
#
# Script to reset some file permissions that get lost when transported
# across a DOS filesystem.  This script *must* be run from the OOMMF
# home directory.
#
#    Bootstrap trick \
exec tclsh "$0" ${1+"$@"}

array set patarr {
    ./app/mmdisp/*/avftoovf       755 
    ./app/mmdisp/*/avftovio       755 
    ./app/mmdisp/*/mmdisp         755 
    ./app/mmpe/filesource.tcl     755 
    ./app/mmpe/mmpe.tcl           755 
    ./app/mmsolve/*/mmsolve       755 
    ./app/omfsh/*/omfsh           755 
    ./app/oommf/oommf.tcl         755 
    ./app/pimake/pimake.tcl       755 
    ./dvl/setperm.tcl             755
    ./ext/oc/*/varinfo            755
}

puts "Setting file permissions"
puts " Mode File"
foreach pat [lsort [array names patarr]] {
    set perm $patarr($pat)
    foreach file [lsort [glob -nocomplain -- $pat]] {
        if {![catch {exec chmod $perm $file} errmsg]} {
            puts [format " %4s %s" $perm $file]
        } else {
            puts stderr "Error setting file $file mode to $perm: $errmsg"
        }
    }
}
