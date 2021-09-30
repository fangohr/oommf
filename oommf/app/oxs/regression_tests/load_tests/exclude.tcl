#lappend excludelist {h2h 2 "Bad numerics? (Windows)"}
#lappend excludelist {h2h_edgefield {} "Bad numerics? (Windows)"}
lappend excludelist {polycubic {} "Bad numerics? (check Oxs_CGEvolve)"}
if {![file exists [file join $examples_dir h2h_leftedge_40x4.ohf]]} {
   lappend excludelist {h2h 0 "Missing support file h2h_leftedge_40x4.ohf"}
}
if {![file exists [file join $examples_dir h2h_leftedge_80x8.ohf]]} {
   lappend excludelist {h2h 1 "Missing support file h2h_leftedge_80x8.ohf"}
}
if {![file exists [file join $examples_dir h2h_leftedge_160x16.ohf]]} {
   lappend excludelist {h2h 2 "Missing support file h2h_leftedge_160x16.ohf"}
}
lappend excludelist {yoyo {} "Ill-posed?"}
# If unix, is X11 server accessible?
global no_display
if {$no_display} {
   lappend excludelist {imagelayers {} "No display"}
   lappend excludelist {luigi {} "No display"}
   lappend excludelist {luigiproc {} "No display"}
   lappend excludelist {oommf {} "No display"}
   lappend excludelist {rotatecenterstage {} "No display"}
   lappend excludelist {sample-reflect {} "No display"}
   lappend excludelist {sample-rotate {} "No display"}
}
# Note: The error bounds in pbcbrick are sloppy, and should be
#  tightened-up when the double-double library comes into use.
