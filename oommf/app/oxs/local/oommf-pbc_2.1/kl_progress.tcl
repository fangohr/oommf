set progress "0%"
if {[catch {label .l -text "Demag kernel computation progress"}]} {
   error "FATAL ERROR in kl_progress script: Tk required but not loaded"
}
frame .f -relief ridge -borderwidth 5
# Setting -borderwidth 0 can be reasonable on Windows platform,
#  where cind of border-blinking can be found
frame .f.l -background red -height 20 -width 1
frame .f.r -background blue -height 20 -width 1
label .f.t -textvariable progress -background red -foreground black

grid .f.l .f.r -sticky news
grid rowconfigure .f 0 -weight 1

pack .l .f -expand 1 -fill x -side top

wm minsize . 400 20

proc UpdateProgress { amt } {
   # amt is progress amount, in range 0 to 100.
   # amt must be an integer
   global progress
   set progress [format "%d%%" $amt]
   grid columnconfigure .f 0 -weight $amt
   grid columnconfigure .f 1 -weight [expr {100-$amt}]
   pack forget .f.t
   if { $amt < 50 } {
      .f.t configure -foreground white -background blue
      .f.l configure -width 0
      pack .f.t -in .f.r -expand 1
   } else {
      .f.t configure -foreground white -background red
      .f.r configure -width 0
      pack .f.t -in .f.l -expand 1
   }
}

proc CheckInput {} {
   if {[gets stdin line]>0} {
      if {[string match exit $line]} { exit }
      UpdateProgress $line
   }
   if {[eof stdin]} {
      exit
   }
}

UpdateProgress 0
update

fconfigure stdin -buffering line -blocking 0
fileevent stdin readable CheckInput

# Print out launch success code.
# This string is checked inside file kl_pbc_util.cc,
# in the "void DemagPBC::Init()" const function.
puts "kl_progress_OK"
