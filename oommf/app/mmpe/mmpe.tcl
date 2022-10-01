# FILE: mmpe.tcl
#
# This file must be interpreted by the omfsh shell.

# Default values for control variables
if {![info exists no_net]} {set no_net 0}
if {![info exists no_event_loop]} {set no_event_loop 0}

# Library support
package require Oc 2	;# [Oc_TempName]
package require Ow 2
if {!$no_net} { package require Net 2}
wm withdraw .

# Several places in this program rely on finding components
# relative to the mmpe directory.  Use Oc_DirectPathname
# to make certain we work with an absolute path, since
# otherwise everything breaks when the cwd changes.
set mmpe_directory [file dirname [Oc_DirectPathname [info script]]]

Oc_Main SetAppName mmProbEd
Oc_Main SetVersion 2.0b0
regexp \\\044Date:(.*)\\\044 {$Date: 2015/10/09 05:50:34 $} _ date
Oc_Main SetDate [string trim $date]
# regexp \\\044Author:(.*)\\\044 {$Author: donahue $} _ author
# Oc_Main SetAuthor [Oc_Person Lookup [string trim $author]]
Oc_Main SetAuthor [Oc_Person Lookup jeicke]
Oc_Main SetHelpURL [Oc_Url FromFilename [file join [file dirname \
        [file dirname [file dirname [Oc_DirectPathname [info \
        script]]]]] doc userguide userguide \
	Micromagnetic_Problem_Edito.html]]
Oc_Main SetDataRole producer

if {!$no_net} {
   Oc_CommandLine Option net {
      {flag {expr {![catch {expr {$flag && $flag}}]}} {= 0 or 1}}
   } {
      global no_net;  set no_net [expr {!$flag}]
   } {Disable/enable data to other apps; default=enabled}

   Oc_CommandLine ActivateOptionSet Net
}

Oc_CommandLine Parse $argv

set SmartDialogs 1


########################################################################
# Proc to trim whitespace from front and back of a string, but leaves
# escaped whitespace.
proc TrimFreeWhitespace { str } {
    set str [string trimleft $str]
    set exp {([^\\])}
    append exp "\[ \t\n\r\]+$"
    regsub $exp $str {\1} str
    return $str
}

########################################################################
# Proc to match record ID's (RID's) with standard mmProbEd ID.  This is
# necessary because the official MIF spec states that case and
# whitespace is ignored inside RID's.  If a match is found, then the
# standard form is returned, otherwise the original input is echoed
# back.  The "standard form" is generally cased and includes whitespace.
#   Note 1: The stdridlist and compridlist lists are initialized inside
# the MifKnow proc, which must be run before any calls to MatchRID.
# Effectively, stdridlist is filled from the state.dat file, and
# compridlist is derived from stdridlist.
#   Note 2: It would be preferable to maintain both stdridlist and
# compridlist together in one list, but then searching is a PITA because
# lsearch doesn't do searching on sub-list index.
proc MatchRID { rid } {
    global stdridlist compridlist
    regsub -all "\[ \t\n\r\]" $rid {} comprid
    set comprid [string tolower $comprid]
    set index [lsearch -exact $compridlist $comprid]
    if {$index<0} { return $rid }
    return [lindex $stdridlist $index]
}

########################################################################
# Proc to update data structures and widgets when MIF format changes.
# Those changes are:
#   1) In MIF 1.2, the only allowed "Demag Type" values are ConstMag
#       and None.
#   2) In MIF 1.1, "Cell Size" is a single scalar representing the x
#      and y dimension of the discretization cell.  In MIF 1.2, this
#      value is a 3 element list, consisting of x, y, and z dimensions,
#      separately.
#   3) MIF 1.2 supports a "Solver Type" record; allowed values are
#      euler, rk2, rk4, rkf54, and cg
# Item 3 doesn't affect the MifKnow array, because "Solver Type" just
# isn't available in MIF 1.1, so the edit window relaunch takes care
# of the necessary changes.
proc MifFormatChange { n1 n2 op } {
    global mif_format MifKnow
    switch -exact $mif_format {
	1.1 {
	    if {[info exists MifKnow(Cell\ Size)]} {
		set cellsize $MifKnow(Cell\ Size)
		if {[llength $cellsize]>2} {
		    # In 1.1 format, cellsize = {1 xysize}
		    # In 1.2 format, cellsize = {3 xsize ysize zsize}
		    set MifKnow(Cell\ Size) \
			    [list 1 [lindex $cellsize 1]]
		}
	    }
	}
	1.2 {
	    if {[info exists MifKnow(Demag\ Type)]} {
		set demag $MifKnow(Demag\ Type)
		switch -exact [string tolower [lindex $demag 1]] {
		    none {}
		    constmag {}
		    default {
			set MifKnow(Demag\ Type) [list 1 ConstMag]
		    }
		}
	    }
	    if {[info exists MifKnow(Cell\ Size)]} {
		set cellsize $MifKnow(Cell\ Size)
		if {[llength $cellsize]<4} {
		    # In 1.1 format, cellsize = {1 xysize}
		    # In 1.2 format, cellsize = {3 xsize ysize zsize}
		    set xysize [lindex $cellsize 1]
		    if {[info exists MifKnow(Part\ Thickness)]} {
			set zsize [lindex $MifKnow(Part\ Thickness) 1]
		    } else {
			set zsize $xysize ;# Punt
		    }
		    set MifKnow(Cell\ Size) [list 3 $xysize $xysize $zsize]
		}
	    }
	}
	default {
	    error "<mmpe.tcl:MifFormatChange>:\
		    Unsupported MIF output format: $mif_format"
	}
    }

    # Relaunch open edit window, if any
    global edit_dialog
    if {[info exists edit_dialog] && $edit_dialog(btn)>=0} {
	set btn $edit_dialog(btn) ;# Save, because this value
	   ## gets reset to -1 during the destruction of the edit
	   ## window
	destroy $edit_dialog(win)
        EntryChange $btn
    }
}

#*************************************************************************#
#ReadFile read a file into an input  buffer.  Does not parse the file
# NOTE: The file is split at newlines and stored as a list (1 line per
#       element) in the global variable "inbuff".  The return value is
#       the number of lines.
# NOTE 2: No, *really*, the main result (the contents of the file) are
#       passed back in a global variable.  Best guess is that the
#       original author was disappointed that Tcl doesn't support
#       FORTRAN common blocks.  What do you think Don, should we submit
#       that as a TIP?
proc ReadFile {filename} {
    global inbuff
    if {[file readable $filename] && [file size $filename]} {
	set file1Id [open $filename "r"]
	set data [read -nonewline $file1Id]
	regsub -all "\\\\\n\[ \t\r\n]*" $data " " data
	set inbuff [split $data "\n"]
	close $file1Id
    } else {
	Oc_Log Log "Empty file or file not readable" warning
	set inbuff [list]
    }
    return [llength $inbuff]
}
#*************************************************************************#
proc ChangeMif { Ref NewValue } {
    global MifKnow
    set temp [lindex $MifKnow($Ref) 0]
    if { $temp != "1" } {
        set MifKnow($Ref) [lreplace $NewValue 0 -1 $temp]
    } elseif {[string match "*Output Format" $Ref]} {
	set MifKnow($Ref) [list 1 $NewValue]
    } elseif {[string match "User Comment" $Ref]} {
	set MifKnow($Ref) [list 1 $NewValue]
    } else {
	set MifKnow($Ref) [linsert $NewValue 0 1]
    }
    set actualLength [llength $MifKnow($Ref)]
    set specMaxLength [lindex $MifKnow($Ref) 0]
    incr actualLength -1;
    if {$actualLength > $specMaxLength} {
	Oc_Log Log "Value for '$Ref' has too many parameters" warning
    }
}
#*************************************************************************#
#set up an array that contains the entries of a MifFile
#What is needed in a mif file is stored in state.dat
#Also initializes MatchRID lists.
proc KnowWhat {filename} {
 global inbuff MifKnow KnowOrder MifUnknown
 global stdridlist compridlist
 set listsize [ReadFile $filename]
 catch {unset MifKnow} ;# Force wipe out of any old data
 catch {unset stdridlist}
 catch {unset compridlist}
 set i 0
 foreach line $inbuff {
  regexp {^[^#]*} $line shortbuff
  set shortbuff [split $shortbuff :]
  if {[llength $shortbuff]<2} { continue } ;# Probably a comment line
  set rid [string trim [lindex $shortbuff 0]]
  lappend stdridlist $rid
  regsub -all "\[ \t\n\r\]" $rid {} comprid
  set comprid [string tolower $comprid]
  lappend compridlist $comprid
  set MifKnow($rid) [string trim [lindex $shortbuff 1]]
  set KnowOrder([incr i]) $rid
 }
 # Add a couple of deprecated/variant records to MatchRID lists
 lappend stdridlist "Converge |mxh| Value"
 lappend compridlist "convergemxhvalue"
 lappend stdridlist "Converge |mxh| Value"
 lappend compridlist "convergetorquevalue"
 # Add special "no record" item
 lappend stdridlist None
 lappend compridlist none

 catch {unset MifUnknown} ;# Wipe out any old unknown data
}

#*************************************************************************#
#Reads and understands the mif file <filename> (must run KnowWhat first)
#(needs ReadFile) puts result in array MifKnow
proc ParseFile {filename} {
    global inbuff MifKnow FieldState MifUnknown mif_format
    set listsize [ReadFile $filename]
    set FieldCount 0
    set prefix {} ; set suffix {}
    if {![regexp {^# *MIF *([0-9]+).([0-9]+)} \
	    [lindex $inbuff 0] dum major minor]} {
	error "<mmpe.tcl:ParseFile>: Input file \"$filename\"\
		is not in MIF format."
    }
    if {$major!=1} {
	error "<mmpe.tcl:ParseFile>: Input file \"$filename\"\
		is not in MIF 1.x format."
    }
    if {$minor>2} {
	error "<mmpe.tcl:ParseFile>: Input file \"$filename\"\
		claims to be in MIF 1.$minor format; this program\
		only supports MIF formats up to 1.2"
    }
    if {$minor==2} {
	set mif_format 1.2
    } else {
	set mif_format 1.1
    }
    for {set i 1} {$i<$listsize} {incr i} {
	set shortbuff [lindex $inbuff $i]

	# Handle comments
	set wsp "\[ \t\n\r\]*"
	if {[regexp -- "^${wsp}#${wsp}(.*)" $shortbuff fluff rest]} {
	    # This is a comment line.  However, do special processing
	    # for "Material Name" lines:
	    if {![regexp -nocase -- "^material${wsp}name${wsp}\[:=\]" $rest]} {
		continue  ;# Ordinary comment, so skip
	    }
	    set shortbuff $rest  ;# This is a "Material Name" line, so
	    ## continue parsing with leading comment character stripped.
	}

	# Remove unquoted comments
	set templist [split $shortbuff "#"]
	set firstbuff {}
	foreach piece $templist {
	    append firstbuff $piece
	    if {![catch {llength $firstbuff} size] && $size>0} {
		break ;# Complete list, so pitch remainder (which is comment)
	    }
	    append firstbuff "#"
	}

	set firstbuff [TrimFreeWhitespace $firstbuff]

	# Pick out record id and parameters
	if {[string length $firstbuff]!=0} {
	    if {![regexp -- {^([^:=]+)([:=]+)(.+)} $firstbuff fluf \
		    prefix fluf2 suffix]} {
		error "<mmpe.tcl:Parsefile>:\
			Incomprehensible input line: $shortbuff"
	    }
	    set prefix [MatchRID $prefix]
	    set suffix [string trimleft $suffix]
	    if {[string match {} $prefix] || [string match {} $suffix]} {
		error "<mmpe.tcl:Parsefile>: Invalid input line: $shortbuff"
	    }
	    if {[info exists MifKnow($prefix)]} {
		if {[string match "Field Range" $prefix]} {
		    incr FieldCount
		    set FieldState($FieldCount) $suffix
		}
		ChangeMif $prefix $suffix
	    } else {
		lappend MifUnknown $prefix $suffix
	    }
	}
    }
    ChangeMif "Field Range Count" $FieldCount
}

#*************************************************************************#
proc CallBack {filename num} {
    global MifKnow MifBak SaveMatFileFlag
    foreach i [array names MifBak] {set MifKnow($i) $MifBak($i)}
    if {[info exists SaveMatFileFlag]} {
	set SaveMatFileFlag 0
    }
}
#*************************************************************************#
proc radiobuttonandpack {name bchange btext command {bvar {}} {opts {}}} {
    #type was zero for this button
    #procedure make a radiobutton and packs the result
    set name [string tolower $name]
    frame $name
    radiobutton $name.button -text $bchange \
	    -value $btext -command $command -anchor w
    if {![string match {} $bvar]} {
	$name.button configure -variable "$bvar"
    }
    if {![string match {} $opts]} {
	eval {$name.button configure} $opts
    }
    pack $name.button -side left
}
#*************************************************************************#
proc labelandpack {name label} {
      #$type==1
      #just a label
      set name [string tolower $name]
      frame $name
      label $name.label -text $label -anchor w
      pack $name.label -side left
}
#*************************************************************************#
proc labelandradiobutton {name label bchange btext command {bvar {}}} {

     #if {$type==2}
      #label and radiobutton
      set name [string tolower $name]
      frame $name
      label $name.label -text "$label:" -anchor e
      radiobutton $name.button -text\
                  $bchange -value $btext -command $command
      if {![string match {} $bvar]} {
	  $name.button configure -variable "$bvar"
      }
      pack $name.label -side left
      pack $name.button -side right
}
#*************************************************************************#
proc checklabelentry {name label bchange btext width command command2 args} {
    #if {$type==3}
    #check + label + entry
    set name [string tolower $name]
    frame $name
    set a ".[lindex [split $name .] 1].[lindex [split $name .] 2]"
    frame $name.buttonframe
    frame $name.buttonprop -height 0 \
	    -width [expr [Ow_CharInfo GetAveWidth]*$width]
    radiobutton $name.button -text $bchange \
	    -value $btext -command $command2
    label $name.label -text "$label:" -width $width -anchor e
    eval  {entry $name.entry -width $width -relief sunken} $args
    bind $name.entry <Return> "$command; focus $a.next.ok; break"
    bind $name.entry <Tab> $command	
    bind $name.entry <Button-1> "Set $name"
    pack $name.buttonframe -side left
    pack $name.buttonprop -in $name.buttonframe -side top
    pack $name.button -in $name.buttonframe -side left
    pack $name.label -side left
    pack $name.entry -side right -fill x -expand 1
    $name.entry config -state disabled
    Oc_SetShiftTabBinding $name.entry $command
}
#*************************************************************************#
proc label+entry { name label width1 arglist command } {
    #if $type==4
    #label + entry thing
    set name [string tolower $name]
    frame $name
    set a ".[lindex [split $name .] 1].[lindex [split $name .] 2]"
    label $name.label -text "$label:" -width $width1 -anchor e
    eval  {entry $name.entry -relief sunken} $arglist
    pack $name.label -side left
    pack $name.entry -side right -fill x -expand 1

    bind $name.entry <Return> "$command;focus $a.next.ok;break"
    bind $name.entry <Button-1> "Set $name"
    bind $name.entry <Tab> $command
    Oc_SetShiftTabBinding $name.entry $command
}
#**********************************************************************#
proc radiobibutton { name label label2 bchange btext \
	width1 arglist command command2 } {
    #if {$type==5}
    #check + label + entry + label + entry
    set name [string tolower $name]
    frame $name
    set a ".[lindex [split $name .] 1].[lindex [split $name .] 2]"
    radiobutton $name.button -text\
	    $bchange -value $btext -command $command2
    label $name.label -text "$label:" -width $width1 -anchor e
    eval  {entry $name.entry -relief sunken -width $width1} $arglist
    label $name.label2 -text "$label2:" -width $width1 -anchor e
    eval  {entry $name.entry2 -relief sunken -width $width1} $arglist
    pack $name.button -side left
    pack $name.entry $name.label $name.entry2 $name.label2 -side right
    bind $name.entry <Tab> $command
    Oc_SetShiftTabBinding $name.entry $command
    #bind $name.entry <<INVOKE>> $command
    bind $name.entry <Return> "$command;focus $a.next.ok;break"
    bind $name.entry <Button-1> "Set $name"
    bind $name.entry2 <Tab> $command
    Oc_SetShiftTabBinding $name.entry2 $command
    #bind $name.entry2 <<INVOKE>> $command
    bind $name.entry2 <Return> "$command;focus $a.next.ok;break"
    bind $name.entry2 <Button-1> "Set $name"
    $name.entry config -state disabled
    $name.entry2 config -state disabled
}
#**********************************************************************#
proc radiomultientrybutton {name label label2 bchange btext \
	width1 command} {
    #if {$type==6}
    global val
    set name [string tolower $name]
    frame $name
    set a ".[lindex [split $name .] 1].[lindex [split $name .] 2]"
    set ButtonState $name.entry
    set textind [split $label ,]
    #N label + entries
    set lwidth [string length "[lindex $textind 0]:"]
    label $name.label -text "[lindex $textind 0]:" \
	    -width $lwidth -anchor e
    eval  {entry $name.entry -relief sunken -width $width1}
    pack $name.label -side left
    pack $name.entry -side left -fill x -expand 1
    #bind $name.entry <<INVOKE>> $command
    bind $name.entry <Return> "$command;focus $a.next.ok;break"
    bind $name.entry <Button-1> "Set $name"
    bind $name.entry <Tab> $command
    Oc_SetShiftTabBinding $name.entry $command
    set b2 ${name}.entry
    $b2 config -textvar val($b2)
    for {set j 2} {$j<$label2} {incr j} {
	set b2 ${name}.entry$j
	set b3 ${name}.label$j	
	set lwidth [string length "[lindex $textind [expr {$j-1}]]:"]
	label $b3 -text "[lindex $textind [expr {$j-1}]]:" \
		-width $lwidth -anchor e
	eval  {entry $b2 -relief sunken -width $width1}
	pack $b3 -side left
	pack $b2 -side left -fill x -expand 1
	$b2 config -textvar val($b2)
	bind $b2 <Tab> $command
	Oc_SetShiftTabBinding $b2 $command
	bind $b2 <Return> "$command;focus $a.next.ok;break"
	bind $b2 <Button-1> "Set $name"
    }
}
#**********************************************************************#
# The creatively named "Set" command executes the <Tab> binding on the
# last window inside $name to have focus.  If that window was an entry
# box, then this writes the value into the associated variable.  Here
# $name should be the edit dialog frame directly beneath .change,
# e.g., .change.geoms.  This is the window stored in the global
# edit_dialog variable.
proc Set {name} {
    set a [focus -lastfor $name]
    eval [bind $a <Tab>]
}
#*************************************************************************#
proc ScaleButton {name btext ButtonNow width1 width2 command args} {
    global vals Dchange MifKnow
    frame $name
    set b2 ${name}.scale
    scale $b2 -from 0 -to 10 -length 600 -variable vals($ButtonNow) \
	    -orient horizontal -label $btext \
	    -tickinterval 0 -showvalue true \
	    -command  "NewField $name $ButtonNow $width1 $width2 \
	    $args"
    pack $b2 -fill x -expand 1
}
#*************************************************************************#
proc NewField {name ButtonNow width1 width2 args}  {
 global MifKnow Dchange vals val FieldState
 if {[info commands ${name}.2]!=""} {destroy ${name}.2}
 if {$vals($ButtonNow)==0} {
     set MifKnow($Dchange($ButtonNow)) "1"
 } else {
     set MifKnow($Dchange($ButtonNow)) "1 $vals($ButtonNow)"
 }
 #puts "datanow: $MifKnow($Dchange($ButtonNow))"
 frame ${name}.2
 if {$vals($ButtonNow)>0} {
     pack [frame ${name}.2.topmargin -height 3] -side top
     set lf [frame ${name}.2.labels -borderwidth 3]
     pack $lf -side top -fill x -expand 1
     pack [label ${lf}.count -text "  #" -width 16] -side right
     pack [label ${lf}.bend -text   "   B_end (Tesla)"] \
	  [label ${lf}.bstart -text "  B_start (Tesla)"] \
	 -side right -fill x -expand 1
 }
 for {set j 1} {$j<=$vals($ButtonNow)} {incr j} {
    set b2 ${name}.2.$j
    radiomultientrybutton $b2 "x,y,z,x,y,z,steps" 8 $ButtonNow 4 $width1 \
			  "NewVal3 $ButtonNow $j ${b2}.entry"
    set val($b2) "0.0"
    pack $b2
    if { [info exists FieldState($j)]} {
         #puts $FieldState($j)
    } else {
       set FieldState($j) "0 0 0 0 0 0 1" }
    #puts $FieldState($j)
    set k 1
    set bname $b2
    set b3 $b2.entry
    while {[info commands $b3]!=""} {
         set val($b3) [lindex $FieldState($j) [expr {$k-1}]]
	   #puts $f "Mnow: $MNow: [lindex $MNow $ii]:$k:$b2"
         incr k
         set b3 ${bname}.entry$k
    }
    Set $b2
 }
 pack  ${name}.2 -side bottom
 pack $name -side top -fill x
}
#*************************************************************************#
proc NewVal { ButtonNow }  {
 global MifKnow Dchange vals
 set i 1
 set j 1
 set ListElms [llength $MifKnow($Dchange($ButtonNow))]
 if {$ListElms>$j} {
    set MifKnow($Dchange($ButtonNow)) \
       [lreplace $MifKnow($Dchange($ButtonNow)) $j $j $vals($ButtonNow)]
 } else {
    lappend MifKnow($Dchange($ButtonNow)) $vals($ButtonNow)
 }
}
#*************************************************************************#
proc NewVal3 { ButtonNow offset ButtonState} {
 global FieldState Dchange val MifKnow
 set ButtonRef $ButtonState
 set i 1
 set j 0
 set fv $offset
 #[lrange $flipped 1 end]
 #puts "$fv"
 if { [info exists FieldState($fv)]} {
   #puts $FieldState($fv)
} else {
   set FieldState($fv) "0 0 0 0 0 0 1" }
 while {[info commands $ButtonRef]!=""} {
   incr i
   set ListElms [llength $FieldState($fv)]
   #puts "$ListElms $val($ButtonRef) $ButtonRef"
   if {$ListElms>$j} {
      set FieldState($fv) [lreplace $FieldState($fv) $j $j $val($ButtonRef)]

   } else {
     lappend FieldState($fv) $val($ButtonRef)
   }
    incr j
    set ButtonRef ${ButtonState}$i
 }	
}
#*************************************************************************#
proc ListButtons {name matfile } {
    global mmpe_directory MatTypesFile
    global MatTypes MatTypesList
    global BaseMatTypes BaseMatTypesList
    global MatTypesWidgets
    # BaseMatTypesList is the input from the standard (distributed)
    # oommf/app/mmpe/materials file.  MatTypesList is the working
    # materials set built from the base list, local additions from
    # the oommf/app/mmpe/local/materials file, and interactive
    # changes.

    set MatTypesFile $matfile

    set input [list]
    set BaseMatTypesList [list]
    catch {unset BaseMatTypes}

    # Read default (i.e., OOMMF distributed) materials file
    # These entries are noted with the "Default" keyword
    # as their last entry.
    set matfile [file join $mmpe_directory $MatTypesFile]
    if {[file readable $matfile]} {
	set file1Id [open $matfile "r"]
	while { [gets $file1Id data]>=0} {
	    regsub {#.*$} $data {} data   ;# Trim comments
            set data [string trim $data]
	    if {[llength $data]==0} {
		continue  ;# Skip empty records
	    }
	    set Name [string trim [lindex $data 0]]
	    set data [lreplace $data 0 0 $Name]
	    lappend input $data
	    lappend BaseMatTypesList $Name
            set BaseMatTypes($Name) $data
	}
	close $file1Id
    }
    if {[llength $input] == 0} {
	Oc_Log Log "Empty or unreadable material file: \"$matfile\"" warning
    }

    # Read local extensions to the materials file
    # These entries are noted with the "Extra" keyword
    # as their last entry.
    set extra_matfile [file join $mmpe_directory local $MatTypesFile]
    if {[file readable $extra_matfile]} {
	set file2Id [open $extra_matfile "r"]
	while { [gets $file2Id data]>=0} {
	    regsub {#.*$} $data {} data   ;# Trim comments
	    set data [string trim $data]
	    if {[catch {llength $data} len] || $len==0} {
		continue  ;# Skip empty or bad records
	    }
	    set Name [string trim [lindex $data 0]]
	    set data [lreplace $data 0 0 $Name]
	    lappend input $data
	}
	close $file2Id
    }

    catch {unset MatTypesWidgets}

    frame $name -relief raised
    set b2 ${name}.list
    menubutton $b2 -text "Material Types" -menu ${b2}.menu\
            -relief raised -underline 0
    pack $b2 -side left -padx 10 -pady 10

    set m1 [menu ${name}.list.menu -tearoff 0]

    set MatTypesWidgets(Menu) $m1

    set b2 $name
    button ${b2}.add -text "Add" \
	    -command "UpdateMatTypes $m1 Add"
    button ${b2}.replace -text "Replace" \
	    -command "UpdateMatTypes $m1 Replace"
    button ${b2}.delete -text "Delete" \
	    -command "UpdateMatTypes $m1 Delete"
    pack ${b2}.add ${b2}.replace ${b2}.delete -side left -fill x

    set MatTypesWidgets(Add)     ${b2}.add
    set MatTypesWidgets(Replace) ${b2}.replace
    set MatTypesWidgets(Delete)  ${b2}.delete

    catch {unset MatTypes}
    set MatTypesList [list]
    foreach locallist $input {
	set Name [lindex $locallist 0]
	if {[set index [lsearch -exact $MatTypesList $Name]]>=0} {
	    # Remove previous entry
	    set MatTypesList [lreplace $MatTypesList $index $index]
	}
	if {[llength $locallist]<5} {
	    # Treat short records as delete (empty) requests
	    if {[info exists MatTypes($Name)]} {
		unset MatTypes($Name)
	    }
	} else {
	    set MatTypes($Name) $locallist
	    lappend MatTypesList $Name
	}
    }

    # Build menu for all remaining (i.e., non-empty) material types
    foreach Name $MatTypesList {
	$m1 add radio -label $Name \
		-variable selected_material_type \
		-command "SetMaterial [list $Name]"
    }

    # GIANT KLUDGE (Mar-2004): Ideally, we'd like to set the
    #  remaining MatTypesWidgets values at the point where those
    #  widgets are created.  But the program flow that creates
    #  these widgets is a convoluted mess, running a command from
    #  the upper segment matmenu file inside the EntryChange proc
    #  using data from the lower segment of the matmenu file.  We
    #  can't put the MatTypesWidgets setting inside the EntryChange
    #  proc because it is a general purpose routine, unless we
    #  want to store widget names for all edit dialogs.  We can't
    #  put the MatTypesWidgets setting inside the matmenu file,
    #  because the symbolic name of the widget is not passed to
    #  the matmenu command, but rather to EntryChange which
    #  creates a buttonname to pass back to the matmenu command.
    # So, instead we will just hardcode the entry widget names
    #  here.  The original code had the anisotropy type widget
    #  names hardcoded into the SetMaterial proc, and anyway
    #  the vals array names are also hardcoded (vals(2)=Name,
    #  vals(3)=Ms, ...), so we aren't adding any extra brittleness
    #  to the code.
    set MatTypesWidgets(Name) .change.mats.2.material.entry
    set MatTypesWidgets(Ms)   .change.mats.3.ms.entry
    set MatTypesWidgets(A)    .change.mats.4.a.entry
    set MatTypesWidgets(K1)   .change.mats.5.k1.entry
    set MatTypesWidgets(Ktype,uniaxial)	.change.mats.7.uniaxial.button
    set MatTypesWidgets(Ktype,cubic)    .change.mats.7.cubic.button

    # Tie variables changes to SyncMatDisplay.
    # Traces are removed when vals are unset in EntryChange
    # proc before new edit dialog is created.
    global vals
    SyncMatDisplay
    foreach id {2 3 4 5} name {Name Ms A K1} {
	trace variable vals($id) w SyncMatDisplay
    }

    # Arrange to have changes (if any) in the material types
    # saved to disk on exit of this edit dialog.  This save
    # is disabled in the CallBack proc if the Cancel button
    # is invoked.
    bind .change.mats <Destroy> "+SaveMatTypes"
}
#*************************************************************************#
# Writes material type information to "local" materials file
proc SaveMatTypes {} {
    global SaveMatFileFlag
    if {![info exists SaveMatFileFlag] || !$SaveMatFileFlag} {
	return  ;# No changes noted
    }
    set SaveMatFileFlag 0

    global mmpe_directory MatTypesFile
    global MatTypes MatTypesList
    global BaseMatTypes BaseMatTypesList

    set worklist $MatTypesList  ;# Local working copy

    # Add null records for materials in base list missing from current list
    foreach name $BaseMatTypesList {
	if {[lsearch -exact $worklist $name]<0} { lappend worklist $name }
    }

    # Find and remove initial sequence of working list that is met
    # by base list.
    set lastindex -1
    set workindex -1
    foreach name $worklist {
	set index [lsearch -exact $BaseMatTypesList $name]
	if {$index <= $lastindex || ![info exists MatTypes($name)] || \
		[llength $MatTypes($name)] \
		!= [llength $BaseMatTypes($name)] } {
	    break  ;# End of initial match sequence
	}
	# Candidate match.  See if all fields agree.
	set match 1
	foreach fieldA $MatTypes($name) fieldB $BaseMatTypes($name) {
	    if {$fieldA != $fieldB} { set match 0; break }
	}
	if { !$match } {
	    # End of matching sequence found
	    break
	}
	# Otherwise, we have a match up to this point
	set lastindex $index
	incr workindex
    }
    set worklist [lreplace $worklist 0 $workindex] ;# Strip initial seq.
    
    set extra_matfile [file join $mmpe_directory local $MatTypesFile]
    if {[llength $worklist]<1 && ![file exists $extra_matfile]} {
	return  ;# Nothing to save
    }

    # Setup header and find column widths
    set header [list {# Name} { M_s } {  A  } {  K  } { anisotropy type}]
    set colsize {}
    foreach a $header { lappend colsize [string length $a] }
    foreach name $worklist {
	set tmplen [list]
	if {[info exists MatTypes($name)]} {
	    set typelist $MatTypes($name)
	} else {
	    set typelist [list $name]
	}
	foreach a $colsize b $typelist {
	    set b [string length [list $b]]
	    if {$a>$b} {
		lappend tmplen $a
	    } else {
		lappend tmplen $b
	    }
	}
	set colsize $tmplen
    }

    # Write worklist to file
    if {![file exists [file dirname $extra_matfile]]} {
	file mkdir [file dirname $extra_matfile]
    }
    if {[catch {open $extra_matfile "w"} fileId]} {
	error "<mmpe.tcl:SaveMatTypes>:\
		Unable to open local material file \"$extra_matfile\":\
		$fileId"
    }
    puts $fileId \
	    "#################################################################"
    puts $fileId "# FILE: [file join oommf/app/mmpe/local $MatTypesFile]"
    puts $fileId "# This file has the same format as the distributed"
    puts $fileId "#   [file join oommf/app/mmpe $MatTypesFile]"
    puts $fileId "# file, q.v."
    puts $fileId "# Lines with only a name field hide that entry."
    puts $fileId "#"
    catch {unset line}
    foreach w $colsize v $header {
	if {![info exists line]} {
	    set line [format "%-*s" $w $v]
	} else {
	    append line [format " %*s" $w $v]
	}
    }
    puts $fileId $line              ;# Write header
    foreach name $worklist {        ;# Write material rows
	if {[info exists MatTypes($name)]} {
	    catch {unset line}
	    foreach w $colsize v $MatTypes($name) {
		if {![info exists line]} {
		    set line [format "%-*s" $w [list $v]]
		} else {
		    append line [format " %*s" $w [list $v]]
		}
	    }
	    puts $fileId $line
	} else {
	    # Output blank record
	    puts $fileId [list $name]
	}
    }
    close $fileId
}
#*************************************************************************#
proc UpdateMatTypes { menu cmd } {
    global MatTypes MatTypesList
    global vals selectedButton_anistype selected_material_type
    global SaveMatFileFlag

    set Name  [string trim $vals(2)]
    set Ms    $vals(3)
    set A     $vals(4)
    set K1    $vals(5)
    set Ktype $selectedButton_anistype
    switch -exact $cmd {
	Add {
	    set MatTypes($Name) [list $Name $Ms $A $K1 $Ktype]
	    $menu add radio -label $Name \
		    -variable selected_material_type \
		    -command "SetMaterial [list $Name]"
	    lappend MatTypesList $Name
	    set selected_material_type $Name
	}
	Replace {
	    set MatTypes($Name) [list $Name $Ms $A $K1 $Ktype]
	    set selected_material_type $Name
	}
	Delete {
	    unset MatTypes($Name)
	    set index [lsearch -exact $MatTypesList $Name]
	    $menu delete $index
	    set MatTypesList [lreplace $MatTypesList $index $index]
	}
	default {
	    error "<mmpe.tcl:UpdateMatTypes>:\
		    Unrecognized command: $cmd"
	}
    }
    SyncMatDisplay
    set SaveMatFileFlag 1
}
###########################################################################
# The SyncMatDisplay proc keeps the "Material Types" pull-down menu
# in-sync with values in the rest of the "Material Parameters" edit
# dialog box, and also activates the relevant Add/Replace/Delete
# buttons.  It is called by traces on the relevant vals() variables.
#
# The behavior is:
#
#  Case 1: The $vals(2) (Name) entry box is empty
#     --> All of the Add/Replace/Delete button are inactive.
#     --> The Materials Type box shows no selection.
#     --> If this routine is *not* called due to a change
#         in the Name variable (i.e., vals(2)), and if the
#         data values (Ms, A, K1, Ktype) match those of an
#         existing (stored) Material Type, then the Name entry
#         widget is filled with the name of that type, in black.
#         This kicks us out of this case, and activates the Delete
#         and Materials Type selection buttons.
#
#  Case 2: The $vals(2) (Name) entry box is non-empty, but doesn't
#     match any stored Material Type.
#     --> Only the Add button is active.
#     --> Text in Name entry widget is red.
#     --> The Materials Type box shows no selection.
#
#  Case 3: The $vals(2) (Name) entry box is non-empty, matches that
#     of a stored Material Type, and the data values (Ms, A, K1, Ktype)
#     match the stored values.
#     --> Only the Delete button is active.
#     --> Text in Name entry widget is black.
#     --> The Materials Type box shows the selection.
#
#  Case 4: The $vals(2) (Name) entry box is non-empty, matches that
#     of a stored Material Type, but the data values (Ms, A, K1, Ktype)
#     don't match the stored values.
#     --> Only the Replace button is active.
#     --> Text in Name entry widget is red.
#     --> The Materials Type box shows no selection.
#
# Global variables:
#   The "Material Types" values are stored in the MatTypes array.
#     Each array MatTypes($Name) entry holds an ordered list of
#        $Name $Ms $A $K1 $Ktype
#   The MatTypesList is a list of the material type Names, in the
#     same order as displayed in the Material Types menu.
#   The MatTypesWidgets array ties material type properties to
#     entry widgets (Name, Ms, A, K1), radio buttons
#     (Ktype,uniaxial and Ktype,cubic), and action buttons (Add,
#     Replace, Delete).
#   The material type currently selected in the "Material Types" menu
#     radio button list is in the global variable
#        selected_material_type,
#     with values identical to the $Name's.
#   The vals array and selectedButton_anistype hold edit dialog
#   working values:
#        Name  --- $vals(2)  
#        Ms    --- $vals(3)  
#        A     --- $vals(4)  
#        K1    --- $vals(5)  
#        Ktype --- $selectedButton_anistype
#
proc SyncMatDisplay { args } {
    global MatTypes MatTypesList MatTypesWidgets
    global selected_material_type vals selectedButton_anistype
    if {[catch { set Name  [string trim $vals(2)]
                 set Ms    $vals(3)
                 set A     $vals(4)
                 set K1    $vals(5)
                 set Ktype $selectedButton_anistype}]} {
            # vals array setup not completed
            return
    }
    if {[string match {} $Name]} {
	# Case 1
	$MatTypesWidgets(Add) configure -state disabled
	$MatTypesWidgets(Replace) configure -state disabled
	$MatTypesWidgets(Delete) configure -state disabled
	set selected_material_type {}
	if {[llength $args]!=3 \
		|| [string compare vals [lindex $args 0]]!=0 \
		|| [string compare 2 [lindex $args 1]]!=0} {
	    foreach cName $MatTypesList {
		foreach {xName cMs cA cK1 cKtype} $MatTypes($cName) { break }
		if {$Ms==$cMs && $A==$cA && $K1==$cK1 && $Ktype==$cKtype} {
		    # Material properties match found
		    $MatTypesWidgets(Name) delete 0 end
		    $MatTypesWidgets(Name) insert end $cName
		    $MatTypesWidgets(Name) configure -foreground black
		    set selected_material_type $cName
		    $MatTypesWidgets(Delete) configure -state normal
		    break
		}
	    }
	}
    } elseif {![info exists MatTypes($Name)]} {
	# Case 2
	$MatTypesWidgets(Add) configure -state normal
	$MatTypesWidgets(Replace) configure -state disabled
	$MatTypesWidgets(Delete) configure -state disabled
	$MatTypesWidgets(Name) configure -foreground red
	set selected_material_type {}
    } else {
	foreach {cName cMs cA cK1 cKtype} $MatTypes($Name) { break }
	if {$Ms==$cMs && $A==$cA && $K1==$cK1 && $Ktype==$cKtype} {
	    # Case 3
	    $MatTypesWidgets(Add) configure -state disabled
	    $MatTypesWidgets(Replace) configure -state disabled
	    $MatTypesWidgets(Delete) configure -state normal
	    $MatTypesWidgets(Name) configure -foreground black
	    set selected_material_type $Name
	} else {
	    # Case 4
	    $MatTypesWidgets(Add) configure -state disabled
	    $MatTypesWidgets(Replace) configure -state normal
	    $MatTypesWidgets(Delete) configure -state disabled
	    $MatTypesWidgets(Name) configure -foreground red
	    set selected_material_type {}
	}
    }
}

#*************************************************************************#
proc SetMaterial {name} {
    global MatTypes MatTypesWidgets vals
    set typeinfo $MatTypes($name)
    set vals(2) [lindex $typeinfo 0]; NewVal 2   ;# Name
    set vals(3) [lindex $typeinfo 1]; NewVal 3   ;# Ms
    set vals(4) [lindex $typeinfo 2]; NewVal 4   ;# A
    set vals(5) [lindex $typeinfo 3]; NewVal 5   ;# K1
    set Ktype [lindex $typeinfo 4]               ;# Anistropy type
    if { [string compare $Ktype uniaxial]==0 } {
	$MatTypesWidgets(Ktype,uniaxial) invoke
    } elseif {  [string compare $Ktype cubic]==0} {
	$MatTypesWidgets(Ktype,cubic) invoke
    }
}
#*************************************************************************#
proc DetMaterial { } {
    global MatTypes MifKnow vals selected_material_type
    # Below, vals(2) is the "Material Name" value.
    set Ms [lindex $MifKnow(Ms) 1]
    set A  [lindex $MifKnow(A)  1]
    set K1 [lindex $MifKnow(K1) 1]
    set Ktype [lindex $MifKnow(Anisotropy\ Type) 1]
    if {[string match {} $Ms] || [string match {} $A] \
	    || [string match {} $K1] || [string match {} $Ktype]} {
	set vals(2) {}
	set selected_material_type {}
	return
    }

    foreach name [array names MatTypes] {
	foreach {cName cMs cA cK1 cKtype} $MatTypes($name) break
	if {$Ms==$cMs && $A==$cA && $K1==$cK1 \
		&& [string compare $Ktype $cKtype]==0} {
	    # Match
	    set vals(2) $cName
	    set selected_material_type $cName
	    return
	}
    }

    # If we get here, then no match has been found
    set vals(2) "Custom"
    set selected_material_type {}

    return
}

#*************************************************************************#
proc NewVal2 { ButtonNow offset ButtonState} {
 global MifKnow Dchange val
 set ButtonRef $ButtonState
 #puts " $MifKnow($Dchange($ButtonNow)) "
 set i 1
 set j $offset
 while {[info commands $ButtonRef]!=""} {
   incr i
   set ListElms [llength $MifKnow($Dchange($ButtonNow))]
   #puts "$ListElms $val($ButtonRef) $ButtonRef"
   if {$ListElms>$j} {
      set MifKnow($Dchange($ButtonNow)) \
      [lreplace $MifKnow($Dchange($ButtonNow)) $j $j $val($ButtonRef)]
   } else {
     lappend MifKnow($Dchange($ButtonNow)) $val($ButtonRef)
   }
    incr j
    set ButtonRef ${ButtonState}$i
 }	
}
#*************************************************************************#
proc DisableEntry { ButtonNow } {
  global MifKnow Dchange ButtonState
   set b2 ""
   set NULL ""
   if {$ButtonState!=$NULL} {
    $ButtonState config -textvar $NULL
    set last [$ButtonState index end]
    $ButtonState delete 0 $last
    $ButtonState config -state disabled
    set b2 ${ButtonState}2
   }
   if {[info commands $b2]!=""} {
     $b2 config -textvar $NULL
     set last [$b2 index end]
     $b2 delete 0 $last
     $b2 config -state disabled
   }
}
#*************************************************************************#
proc ChangeMif2 { Ref ButtonNow Command} {
 global MifKnow Dchange ButtonState
 set idea .change
 Set $idea
 focus $idea
 set index $Dchange($ButtonNow)
 if {[llength $MifKnow($index))]>1} {
   set MifKnow($index) [lreplace $MifKnow($index) 1 end $Ref]
 } else {
   lappend MifKnow($index) $Ref
 }
 if {$Command!="NULL"} {
  DisableEntry $Command
 }
}
#*************************************************************************#
# I think the oldbutton vs. Ref comparisons are broken, but I'm not sure
# how to fix them, because, surprisingly, the extensive comments in this
# proc don't explain what this proc is suppose to do.  No doubt, the
# original author felt the expressive name "ChangeMif3" was
# self-documenting.
proc ChangeMif3 { Ref ButtonNow name} {
 global MifKnow Dchange ButtonState val
 DisableEntry $ButtonNow
 set idea .change
 Set $idea
 set oldbutton [lindex $MifKnow($Dchange($ButtonNow)) 1]
 if {[string tolower $oldbutton]!=[string tolower $Ref]} {
     set ListElms [llength $MifKnow($Dchange($ButtonNow))]
     if {$ListElms>=2} {
	 set MifKnow($Dchange($ButtonNow)) \
		 [lreplace $MifKnow($Dchange($ButtonNow)) 1 $ListElms $Ref]
     } else {
	 lappend MifKnow($Dchange($ButtonNow)) $Ref
     }
 }
 set ButtonState $name.entry
 set b2 $ButtonState
 set i 1
 set j 2
 while {[info commands $b2]!=""} {
   $b2 config -state normal
    if {[string tolower $oldbutton]!=[string tolower $Ref]} {
       set val($b2) "0.0"
       lappend MifKnow($Dchange($ButtonNow)) $val($b2)
    } else {
       set val($b2) [lindex $MifKnow($Dchange($ButtonNow)) $j]

    }
    $b2 config -textvar val($b2)

   incr i
   incr j
   set b2 ${ButtonState}$i
 }
 focus $name.entry
}
#*************************************************************************#
proc SplitNToss {filename} {
 # Note: This routine reads edit dialog "menu" files, e.g.,
 #  matmenu, demmenu, ...
 global inbuff MenuState
 set MenuState [list]
 set listsize [ReadFile $filename]
 set firstbuff {}
 set FieldCount 0
 for {set i 0} {$i<$listsize} {incr i} {
   set shortbuff [string trim [lindex $inbuff $i]]
   regexp {^[^#]*} $shortbuff firstbuff
   if {[string length $firstbuff]!=0} {
       set firstbuff [string trim [lindex [split $firstbuff :] 1 ]]
       #puts $firstbuff
       if {[string length $firstbuff]!=0} {
	   lappend MenuState $firstbuff
       }
   }
 }
}
#*************************************************************************#
proc EntryChange {ButtonNumber} {
 global inbuff MifKnow Mcontents val vals
 global Dchange MenuState ButtonState MifBak
 global TopButton mif_format

 # "val" and "vals" are unfortunate choices for global variable names
 # as they may be accidentally used elsewhere for temporary use.  Add
 # a little robustness by insuring they can be used as array
 # variables:
 if {[info exists val] && ![array exists val]} {
    unset val
 }
 if {[info exists vals] && ![array exists vals]} {
    unset vals
 }


 set ButtonState ""
 set NULL "NULL"

 set size [expr {[array size TopButton]-1}]
 if {$ButtonNumber<0}     {set ButtonNumber $size}
 if {$ButtonNumber>$size} {set ButtonNumber 0}
 set btn_info $TopButton($ButtonNumber)
 set window_title [lindex $btn_info 0]
 switch -exact $mif_format {
     1.1 { set filename [lindex $btn_info 1] }
     1.2 { set filename [lindex $btn_info 2] }
     default {
	 error "<mmpe.tcl:EntryChange>:\
		 Unsupported MIF output format: $mif_format"
     }
 }

 set prev [expr {$ButtonNumber-1}]
 set next [expr {$ButtonNumber+1}]

 catch {unset MifBak}
 foreach i [array names MifKnow] {set MifBak($i) $MifKnow($i)}

 SplitNToss $filename

 #sets name of window
 wm geometry .change {}  ;# Display window at widget requested size
 set a .change[lindex $MenuState 0]

 frame $a ;# This frame fills toplevel .change
 after idle [list wm title .change "[wm title .]: $window_title"]

 global edit_dialog
 set edit_dialog(btn) $ButtonNumber
 set edit_dialog(win) $a
 bind $a <Destroy> {set edit_dialog(btn) -1}

 #determines number of commands
 set ComNum [lindex $MenuState 1]
 #reads commands and options into a command array
 set i 2
 #puts "Reading commands"
 for {set Com 0} {$Com<$ComNum} {incr Com} {
   #puts $Com
   set comands($Com) [lindex $MenuState $i]
   incr i
   set options($Com) [lindex $MenuState $i]
   incr i
 }
 #reads number of data changes and sets up frames
 set Types [lindex $MenuState $i]
 # variable Types holds number of mif changes in menu

 incr i
 catch { unset vals }
 for {set ButtonNow 1} {$ButtonNow<=$Types} {incr ButtonNow} {
  frame $a.$ButtonNow
  #Dchange gives the part of the mif entry the widget changes.

  set tag [MatchRID [lindex $MenuState $i]]
  if {[string match None $tag]} {
   set Dchange($ButtonNow) {}
   set MNow {}
   set vals($ButtonNow) {}
  } else {
   set Dchange($ButtonNow) $tag
   set MNow $MifKnow($tag)
   set vals($ButtonNow) [lindex $MNow 1]
  }
  incr i

  #entries hold the number of options for a given MifChange
  set entries [lindex $MenuState $i]
  incr i
  set OptionMax [expr $entries]
  for {set j 0} {$j<$OptionMax} {incr j} {
    set shortbuff [split [lindex $MenuState $i] ! ]
    #Options Holds name & value of Menu choice
    set Options($j) [lindex $shortbuff 0]
    incr i
    #parses choice information from data files
    set title  [lindex $shortbuff 0]
    set comnum [lindex $shortbuff 1]
    set label  [lindex $shortbuff 2]
    set label2 [lindex $shortbuff 3]
    if {[lindex $shortbuff 4]==""} {
      set offset 1
    } else {
      set offset [lindex $shortbuff 4]
    }
    set name [string tolower [lindex $title 0]]
    #puts "command number: "
    #puts $comnum
    set buttonname $a.$ButtonNow.$name
    eval $comands($comnum)
    set cname "pack $buttonname $options($comnum)"
    eval $cname
    # The next line use to read
    #  set check [string tolower [split [lindex $vals($ButtonNow) 0]]]
    # but this leads to problems if $vals($ButtonNow) is not a valid
    # list.  This is possible for some user input free text fields,
    # such as "user comment".  I (mjd) don't fully comprehend the
    # purpose of this "check" (AFAICT it is properly active when
    # setting up radio buttons in edit dialogs), but the following
    # seems to work okay at present:
    set check [string tolower $vals($ButtonNow)]
    #invokes buttons that should be set because mif entry is defined for
    #button
    if {$check==$title} {
	 set b2 $a.$ButtonNow.$name.button
	 if {[info commands $b2]!=""} {
        set cname  "$b2 invoke"	
        eval $cname
       }
    }
    set ii $offset
    set k 1
    set b2 $buttonname.entry	
    while {[info commands $b2]!=""} {
	set val($b2) [lindex $MNow $ii]
	incr ii
	incr k
	set b2 ${buttonname}.entry$k
    }
 }
 pack $a.$ButtonNow -fill x -expand 0
 }
 frame $a.next -borderwidth 5
 button $a.next.next -text "Next" \
        -command "Set $a; destroy $a ; EntryChange $next"
 button $a.next.prev -text "Previous" \
        -command "Set $a; destroy $a ; EntryChange $prev"
 button $a.next.ok -text "Ok" -command "Set $a; destroy .change"
 button $a.next.reset -text "Cancel" \
        -command "[list CallBack $filename $ButtonNumber] ; destroy .change"
 if {$prev<0} { $a.next.prev configure -state disabled }
 if {$next>$size} { $a.next.next configure -state disabled }
 pack $a.next.next $a.next.prev $a.next.ok $a.next.reset \
        -side left -expand true
 pack $a.next -side bottom -fill x
 #bind $a.next.ok <Return> "destroy .change"
 bind $a.next.reset <Return> "destroy .change ; \
	 [list CallBack $filename $ButtonNumber]"
 pack $a -fill both -expand 1
}
#*************************************************************************#
proc SaveMifFile {savefilename} {
 global MifKnow FieldState KnowOrder MifUnknown mif_format
 set file2Id [open $savefilename "w"]
 puts $file2Id "# MIF $mif_format"
 for {set one 1} {$one<[array size KnowOrder]} {incr one} {
    set index $KnowOrder($one)
    set flipped $MifKnow($index)
    set value [lindex $flipped 1]
    if {$value!="" && $index!="Field Range" \
	    && $index!="Material Name" && $index!="Field Range Count" \
            && $index!="Solver Type" \
            && $index!="User Comment" \
	    && [regexp "Output Format" $index]==0} {
	puts $file2Id "$index:[lrange $flipped 1 end]"
    } elseif {$index=="Material Name" && $value!=""} {
        puts $file2Id "# $index:[lrange $flipped 1 end]"
    } elseif {$index=="Solver Type" && $value!="" && $mif_format>=1.2} {
        puts $file2Id "$index:[lrange $flipped 1 end]"
    } elseif {$index=="User Comment" && $value!=""} {
       puts $file2Id "$index:[join [lrange $flipped 1 end]]"
    } elseif {[regexp "Output Format" $index]==1 && $value!=""} {
	puts $file2Id "$index:[join [lrange $flipped 1 end] ]"
    }
    if {$index=="Field Range Count"} {
	for {set iiii 1} {$iiii <= $value} {incr iiii} {
	    set pals $FieldState($iiii)
	    puts $file2Id "Field Range: $pals"
	}
    }
 }
 # Dump unrecognized entries
 if {[info exists MifUnknown] && [llength $MifUnknown]>0} {
     puts $file2Id "\n# Input file entries unrecognized by mmProbEd ---"
     foreach {name value} $MifUnknown {
	 puts $file2Id "$name : $value"
     }
 }
 close $file2Id
}
#**************************************************************************#
proc SaveButton { btn item } {
    if { [string match disabled [$btn entrycget $item -state]] } {
        return
    }
    global oommf SmartDialogs
    Ow_FileDlg New dialog -callback SaveCallback \
	    -dialog_title "Save File -- [Oc_Main GetTitle]" \
	    -selection_title "Save MIF File As..." \
	    -select_action_id "SAVE" \
	    -filter "*.mif" \
	    -menu_data [list $btn $item] \
	    -smart_menu $SmartDialogs
    # Set icon
    Ow_SetIcon [$dialog Cget -winpath]
}
proc SaveCallback { widget actionid args } {
    if {[string match DELETE $actionid]} {
        eval [join $args]
        return
    }
    if {[string match SAVE $actionid]} {
        SaveMifFile [$widget GetFilename]
    } else {
        return "ERROR (proc SaveCallback): Invalid actionid: $actionid"
    }
}
proc LoadButton { btn item } {
    if { [string match disabled [$btn entrycget $item -state]] } {
        return
    }
    global oommf SmartDialogs
    Ow_FileDlg New dialog -callback LoadCallback \
	    -dialog_title "Open File -- [Oc_Main GetTitle]" \
	    -selection_title "Open MIF File..." \
	    -select_action_id "OPEN" \
	    -filter "*.mif" \
	    -file_must_exist 1 \
	    -menu_data [list $btn $item] \
	    -smart_menu $SmartDialogs
    # Set icon
    Ow_SetIcon [$dialog Cget -winpath]
}
proc LoadCallback { widget actionid args } {
    global resetfilepath initfilepath
    if {[string match DELETE $actionid]} {
        eval [join $args]
        return
    }
    if {[string match OPEN $actionid]} {
	# Close open edit window, if any
	global edit_dialog
	set ed_btn -1
	if {[info exists edit_dialog] && $edit_dialog(btn)>=0} {
	    set ed_btn $edit_dialog(btn) ;# Save, because this value
	    ## gets reset to -1 during the destruction of the edit
	    ## window
	    destroy $edit_dialog(win)
	}
	KnowWhat $resetfilepath
        ParseFile [$widget GetFilename]

	# Relaunch open edit window, if required
	if {$ed_btn>=0} {
	    EntryChange $ed_btn
	}
    } else {
        return "ERROR (proc LoadCallBack): Invalid actionid: $actionid"
    }
    return
}
#**************************************************************************#
proc CallWin {procedurename button_number} {
    if {[winfo exists .change]} {
	global edit_dialog
	Set $edit_dialog(win)
	destroy $edit_dialog(win)
    } else {
	toplevel .change
	after idle "Ow_SetIcon .change"
    }
    $procedurename $button_number
}
#**************************************************************************#
#top level window
set menubar .menubar
foreach {menuname optmenu helpmenu} \
	[Ow_MakeMenubar . $menubar File Options Help] break
frame .menubuttons -borderwidth 4 -relief ridge
if {[Ow_IsAqua]} {
   # Add some padding to allow space for Aqua window resize
   # control in lower righthand corner
   .menubuttons configure -relief flat -padx 6 -pady 6
}
$menuname add command -label "Open..." -command\
           [list LoadButton $menuname "Open..."] -underline 0
$menuname add command -label "Save as..." -command\
           [list SaveButton $menuname "Save as..."] -underline 0
$menuname add separator
$menuname add command -label Exit -command { exit } -underline 1

$optmenu add radiobutton -label "MIF 1.1" -underline 6 \
	-value 1.1 -variable mif_format
$optmenu add radiobutton -label "MIF 1.2" -underline 6 \
	-value 1.2 -variable mif_format
set mif_format 1.1 ;# Default format
trace variable mif_format w MifFormatChange

Ow_StdHelpMenu $helpmenu
global resetfilepath initfilepath
set resetfilepath [file join $mmpe_directory state.dat]
set initfilepath  [file join $mmpe_directory init.mif]
KnowWhat $resetfilepath
ParseFile $initfilepath
global TopButton
set listsize [ReadFile [file join $mmpe_directory topmenu]]
for {set i 0} {$i<$listsize} {incr i} {
    set shortbuff [split [lindex $inbuff $i] : ]

    set field0 [string trim [lindex $shortbuff 0]]
    set field1 [string trim [lindex $shortbuff 1]]
    set field2 [string trim [lindex $shortbuff 2]]
    set field3 [string trim [lindex $shortbuff 3]]

    set buttoname [string tolower ".menubuttons.$field0"]
    set buttonspecs($i) $field1
    set filename11 [file join $mmpe_directory $field2]
    set filename12 [file join $mmpe_directory $field3]
    set TopButton($i) [list $field0 $filename11 $filename12]
    button $buttoname -text $field0 -command [list CallWin $field1 $i]
    if {![file readable $filename11] || ![file readable $filename12]} {
	# Might want to enable/disable button depend on value of
	# mif_format, and update on change thereof.  But
	# both filename11 and filename12 should be readable, and
	# if not then there are presumably more serious problems
	# at hand.
	$buttoname configure -state disabled
    }
    pack $buttoname  -fill x
}
pack .menubuttons -side bottom -fill x -expand 1

# Set minimum primary window width
set menuwidth [Ow_GuessMenuWidth $menubar]
set bracewidth [Ow_SetWindowTitle . [Oc_Main GetInstanceName]]
if {$bracewidth<$menuwidth} {
   set bracewidth $menuwidth
}
set brace [canvas .brace -width $bracewidth -height 0 -borderwidth 0 \
        -highlightthickness 0]
pack $brace -side top
Oc_EventHandler New _ Net_Account NewTitle [subst -nocommands {
  $brace configure -width \
    [expr {%winwidth<$menuwidth ? $menuwidth : %winwidth}]
}]


#
update idletasks
proc Die {win} {
    if {![string match . $win]} { return }  ;# Child destroy event
    exit
}
bind . <Destroy> "+Die %W"
wm protocol . WM_DELETE_WINDOW { exit }
Oc_EventHandler New _ Oc_Main Exit {proc Die win {}}

#**************************************************************************#
# networking
if {!$no_net} {
    package require Net 2
    Net_Protocol New protocol -name "OOMMF ProbEd protocol 0.1"
    $protocol AddMessage start GetProbDescFile {} {
	set fn [Oc_TempName tmp .mif]
        SaveMifFile $fn
        return [list start [list 0 $fn]]
    }
    Net_Server New server -protocol $protocol -alias [Oc_Main GetAppName]
    $server Start 0
} else {
   package forget Net
}

wm title . [Oc_Main GetInstanceName]
wm iconname . [wm title .]
Ow_SetIcon .
wm deiconify .
## Docs say this should happen automatically, but at least some
## WM's don't track this.

raise .     ;# Bring to top of display window stack

if {!$no_event_loop} { vwait omf_forever }
