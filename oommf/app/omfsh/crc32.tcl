# FILE: crc32.tcl
#
# This file must be evaluated by omfsh

package require Oc 2
package require Nb 2
Oc_ForceStderrDefaultMessage
catch {wm withdraw .}

Oc_Main SetAppName crc32
Oc_Main SetVersion 2.0b0

Oc_CommandLine Option console {} {} ;# Remove -console option
Oc_CommandLine Option tk {} {} ;# Remove -tk option

Oc_CommandLine Option v {
        {level {regexp {^[0-9]+$} $level} {is a decimal integer}}
    } {
        global verbosity;  scan $level %d verbosity
} {Verbosity level (default = 1)}
set verbosity 1

Oc_CommandLine Option binary {} {
        global mode; set mode binary
} {Read files as binary (default)}
set mode binary

Oc_CommandLine Option text {} {
        global mode; set mode text
} {Read files as text, e.g., do newline translation}

Oc_CommandLine Option hex {} {
        global output_format; set output_format hex
} {Write output in hexadecimal}

Oc_CommandLine Option decimal {} {
        global output_format; set output_format decimal
} {Write output in decimal (default)}
set output_format decimal

Oc_CommandLine Option [Oc_CommandLine Switch] {
        {{file list} {} {Input file(s), or none to read stdin}}
    } {
        global infiles; set infiles $file
} {End of options; next argument is file(s)}
set infiles {}

Oc_CommandLine Parse $argv

if {[string compare text $mode]==0} {
    set crcheader "CRC-32t"
} else {
    set crcheader "CRC-32b"
}

if {[string compare hex $output_format]==0} {
    set outfmt "%08X"
    set hexout 1
} else {
    set outfmt "%10u"
    set hexout 0
}

set errcount 0
if {[llength $infiles]==0} {
    # Read input from stdin
    if {[string compare text $mode]!=0} {
	fconfigure stdin -translation binary
    }
    if {[catch {Nb_ComputeCRCChannel stdin} crc]} {
	puts $crc
	incr errcount
    } else {
	# Nb_ComputeCRCChannel command returns a two
	# element list, crc + size.
	if {$verbosity>0} {
	    puts [format "<stdin> %s: $outfmt, size: %lu bytes" \
		    $crcheader [lindex $crc 0] [lindex $crc 1]]
	} else {
	    puts [format $outfmt [lindex $crc 0]]
	}
    }
    exit $errcount
}

# On Windows, invoke glob-style filename matching.
# (On Unix, shells do this automatically.)
if {[string match windows $tcl_platform(platform)]} {
    set newlist {}
    foreach file $infiles {
	if {[file exists $file] || [catch {glob -- $file} matches]} {
	    # Either $file exists as is, or glob returned empty list
	    lappend newlist $file
	} else {
	    set newlist [concat $newlist [lsort $matches]]
	}
    }
    set infiles $newlist
}

set maxnamewidth 6
foreach file $infiles {
    if {[string length $file]>$maxnamewidth} {
	set maxnamewidth [string length $file]
    }
}

if {$verbosity>0} {
    set namebar {}
    for {set i 0} {$i<$maxnamewidth} {incr i} {
	append namebar "-"
    }
    if {$hexout} {
	puts [format "\n%*s   %s      Size" $maxnamewidth "File " $crcheader]
	puts [format "%*s  --------  ------------" $maxnamewidth $namebar]
    } else {
	puts [format "\n%*s    %s       Size" $maxnamewidth "File " $crcheader]
	puts [format "%*s  ----------  ------------" $maxnamewidth $namebar]
    }
}

# Loop through input files
foreach file $infiles {
    set result [format "%*s  " $maxnamewidth $file]
    if {[catch {open $file r} chan]} {
	append result $chan
	incr errcount
    } else {
	if {[string compare text $mode]!=0} {
	    fconfigure $chan -translation binary
	}
        fconfigure $chan -buffering full -buffersize 65536
	if {[catch {Nb_ComputeCRCChannel $chan} crc]} {
	    append result $crc
	    incr errcount
	} else {
	    # Nb_ComputeCRCChannel command returns a two
	    # element list, crc + size.
	    append result [format $outfmt [lindex $crc 0]]
	    if {$verbosity>0} {
		append result [format "  %12lu" [lindex $crc 1]]
	    }
	}
    }
    puts $result
}
if {$verbosity>0} {
    if {$hexout} {
	puts [format "%*s  --------  ------------" $maxnamewidth $namebar]
    } else {
	puts [format "%*s  ----------  ------------" $maxnamewidth $namebar]
    }
}

exit $errcount
