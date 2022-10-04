# FILE: mag2hfield.tcl
#
# This file must be interpreted by the mmsolve shell.

package require Oc 2
package require Mms 2

Oc_Main SetAppName mag2hfield
Oc_Main SetVersion 2.0b0

Oc_CommandLine Option console {} {}

Oc_CommandLine Option fieldstep {
	{index {regexp {^[0-9]+$} $index} {is a decimal integer}} 
    } {
	global fieldstep_request; scan $index %d fieldstep_request
} {Which applied field step to use (default=0)}
set fieldstep_request 0   ;# Default field step

Oc_CommandLine Option energyfmt {
	{fmt {} {is a printf-style format string}} 
    } {
	global energyfmt_request; set energyfmt_request $fmt
} {Print format for energy data}
set energyfmt_request "%s"  ;# Default: dump full string

Oc_CommandLine Option data {
    {kind {
	foreach _ [split $kind ,] {
	    if {![regexp {energy|field|hnorm.*} $_]} {
		return 0
	    }
	}
	return 1
    } "is subset of {energy,field,hnorm<thickness>}"}
} {
    global data_request;  set data_request [split $kind ,]
} {Type of data to compute}
set data_request [list energy field]   ;# Default field step

Oc_CommandLine Option component {
    {request {
	foreach _ [split $request ,] {
	    if {![regexp all|anisotropy|demag|exchange|total|zeeman $_]} {
		return 0
	    }
	}
	return 1
    } "is subset of\n                             \
	    {all,anisotropy,demag,exchange,total,zeeman}"}
} {
    global component_request;  set component_request [split $request ,]
    if {[lsearch -exact $component_request all] != -1} {
	set component_request [list anisotropy demag exchange total zeeman]
    }
} {Which energy and/or H fields to compute}
set component_request "total"   ;# Default field component output

Oc_CommandLine Option [Oc_CommandLine Switch] {
	{mif_file {} {Problem specification file (MIF 1.x)}}
        {omf_file1 {} {Magnetization state file}}
        {{omf_file2 list} {} {Optional additional magnetization files}}
    } {
	upvar #0 mif_file globalmif_file; set globalmif_file $mif_file
	global omf_filelist
	set omf_filelist [concat [list $omf_file1] $omf_file2]
} {End of options; next argument is mif_file}

Oc_CommandLine Parse $argv

if {[string match {*.omf} $mif_file]} {
    puts stderr "Warning: Are .mif and .omf files in correct order?"
    puts stderr "   (.mif should be first.)"
}

# On Windows, invoke glob-style filename matching for omf file list.
# (On Unix, shells do this automatically.)
if {[string match windows $tcl_platform(platform)]} {
    set newlist {}
    foreach file $omf_filelist {
	if {[file exists $file] || [catch {glob -- $file} matches]} {
	    # Either $file exists as is, or glob returned empty list
	    lappend newlist $file
	} else {
	    set newlist [concat $newlist [lsort $matches]]
	}
    }
    set omf_filelist $newlist
}


mms_mif New mif
$mif Read $mif_file

foreach omf_file $omf_filelist {
    if {![file readable $omf_file]} {
	puts "Skipping file \"$omf_file\" (unreadable)"
	flush stdout
	continue
    }
    if {[llength $omf_filelist]>1} {
	puts "Input omf file: $omf_file"
    }
   $mif SetMagInitArgs [list avffile $omf_file]
    mms_solver New solver $mif
    $solver IncrementFieldStep $fieldstep_request

    regsub {(\.omf|\.ovf)$} $omf_file {} basename
    set heff_file ${basename}.ovf

    foreach component $component_request {
	set component [string tolower $component]
	puts -nonewline [format " %12s" "E $component"]
	if {[lsearch -exact $data_request energy]>=0} {
	    set energy [$solver GetComponentEnergyDensity $component]
	    puts -nonewline \
		[format ": $energyfmt_request J/m^3 " $energy]
	    flush stdout
	}
	set index [lsearch -glob $data_request hnorm*]
	if {$index>=0} {
	    set thick -1
	    regexp -- {hnorm *(..*)} [lindex $data_request $index] dummy thick
	    set hnorm [$solver GetFieldComponentNorm $component $thick]
	    puts -nonewline [format " hnorm: $hnorm"]
	    flush stdout
	}
	if {[lsearch -exact $data_request field]>=0} {
	    set out_file "${basename}-h${component}.ohf"
	    $solver WriteFieldComponentFile $component $out_file \
		"$component field component from $mif_file + $omf_file"
	    puts -nonewline " H ==> $out_file"
	    flush stdout
	}
	puts {}   ;# Newline
    }
    flush stdout
    catch { $solver Delete }
}

catch { $mif Delete }

# Without this, -tk 1 keeps grey box forever.
exit 0







