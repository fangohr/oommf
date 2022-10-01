# FILE: any2ppm.tcl
#
# This script converts data in any image format understood by Tk's
# (possibly extended) [image photo] command to data representing the
# same image in the PPM P3 format, or any other format understood
# by [image photo].
#
# This file must be interpreted by the filtersh shell.

package require Oc 2	;# [Oc_TempFile]
package require Tk
package require If 2

Oc_ForceStderrDefaultMessage

Oc_Main SetAppName any2ppm
Oc_Main SetVersion 2.0b0

Oc_CommandLine Option console {} {}

Oc_CommandLine Option noinfo {} {
    global noinfo;  set noinfo 1
} {Suppress writing of progress info to stderr}
set noinfo 0

Oc_CommandLine Option o {outfile} {
    global outSpec;
    if {[string match {} $outfile]} {
	set outSpec "-"
    } else {
	set outSpec $outfile
    }
} {Write output to outfile ("-" for stdout)}
set outSpec ""

Oc_CommandLine Option f {} {
    global force_overwrite; set force_overwrite 1
} {Force overwrite of output files}
set force_overwrite 0

Oc_CommandLine Option format {fmt} {
   # NOTE: The Tk image handler named "PPM" is PPM P6 (binary)
   #       format.  The default Tk handler answers to either
   #       PPM or "PPM P6".  HOWEVER, some versions of the Img
   #       package on some platforms (Img 1.4 on Mac OS X) blow
   #       chunks if the format is "PPM P6".
   # NOTE: There is a bad bug in the Tcl 8.5.0 that shipped with
   #       Ubuntu 8.04 (Hardy Heron) which breaks regexp in switch.
   #       So we just use exact matching instead.
   global outFormat
   switch -exact -- [string tolower $fmt] {
      jpg -
      jpe -
      jpeg  {set fmt JPEG}
      "ppm p6" -
      p6    {set fmt PPM}
      "ppm p3" -
      ppm -
      p3    {set fmt P3}
      tif -
      tiff  {set fmt TIFF}
    }
    set outFormat $fmt
} {Output file format (default is PPM P3)}
set outFormat P3

Oc_CommandLine Option [Oc_CommandLine Switch] {
    {{infile list} {} {Input file(s).  If none or "", read from stdin.}}
    } {
    global inList
    set inList {}
    foreach elt $infile {
        # The "-all" switch to lsearch isn't in Tcl 7.5 (argh...)
        if {[string match "-" $elt]} {
            lappend inList {}
        } else {
            lappend inList $elt
        }
    }
} {End of options; next argument is infile}
set inList [list]

Oc_CommandLine Parse $argv

set errcount 0
#set BUFSIZ 250000       ;# Size of output write buffer
set BUFSIZ 16384       ;# Size of output write buffer

proc make_ppm_name { inname } {
    global outFormat force_overwrite
    # If outFormat is JPEG, the use .jpg as the output file
    # extension.  Use .tif for TIFF.  Otherwise use the leading
    # string of alphabetical characters (i.e., [a-z]), cast to
    # lowercase.  This provides the conventional extension for
    # all the cases I know about.
    set of [string tolower $outFormat]
    switch -regexp -- $of {
	{^jpe.*$}       {set ext jpg}
	{^tif.*$}       {set ext tif}
	{^ppm.*|p3|p6$} {set ext ppm}
        default {
	    if {![regexp -- {^ *([a-z]+)} $of dum ext]} {
		set ext ppm  ;# Safety default
	    }
	}
    }
    set outname [file rootname $inname].$ext
    if {!$force_overwrite && [file exists $outname]} {
        set basename $outname
        set outname {}
        for {set i 1} {$i<100} {incr i} {
            set testname ${basename}-$i
            if {![file exists $testname]} {
                set outname $testname
                break
            }
        }
    }
    return $outname
}

switch -exact -- $outSpec {
    {} {set outmode automatic}
    {-} {
        set outmode fixed
        set outname {}
        set outchan stdout
        fconfigure $outchan -translation auto \
                -buffering full -buffersize $BUFSIZ
    }
    default {
        set outmode fixed
        set outname $outSpec
    }
}

set loopOpen 0
if {[llength $inList]==0} {
    set inList [list {}]  ;# Use empty string to denote stdin
} elseif {[string match windows $tcl_platform(platform)]} {
    # On Windows, invoke glob-style filename matching.
    # (On Unix, shells do this automatically.)
    set newlist {}
    foreach file $inList {
	if {[file exists $file] || [catch {glob -- $file} matches]} {
	    # Either $file exists as is, or glob returned empty list
	    lappend newlist $file
	} else {
	    set newlist [concat $newlist [lsort $matches]]
	}
    }
    set inList $newlist
}

foreach inname $inList {
    if {[string match {} $inname]} {
        # Read from stdin
        if {!$noinfo} { puts stderr "Processing input from stdin" }
        fconfigure stdin -translation binary
        set data [read stdin]
        if {[catch {set pic [image create photo -data $data]} errmsg]} {
	    # Unable to process data using 'photo -data' option.  Try
	    # using the -file option instead
            Oc_TempFile New temp -stem any2ppm
            set tempname [$temp AbsoluteName]
            $temp Claim
            $temp Delete
	    set tempchan [open $tempname w]
            fconfigure $tempchan -translation binary
            if {[catch {puts -nonewline $tempchan $data} msg]} {
                puts stderr "FATAL ERROR: $msg"
                catch {close $tempchan}
                catch {file delete $tempname}
                exit [incr errcount]
            }
            close $tempchan
	    if {[catch {set pic [image create photo -file $tempname]} msg]} {
		puts stderr "ERROR: $errmsg"
		puts stderr "ERROR: $msg"
		incr errcount
		file delete $tempname
		continue
	    }
            file delete $tempname
        }
    } else {
        # Read from file
        if {![file readable $inname]} {
            puts stderr "Unable to open input file $inname; Skipping."
            incr errcount
            continue
        }
        if {!$noinfo} { puts stderr "Processing input file $inname" }
        if {[catch {set pic [image create photo -file $inname]} errmsg]} {
            puts stderr "ERROR: $errmsg"
            incr errcount
            continue
        }
    }

    if {!$noinfo} { puts stderr "Processing output..." }
    if {[string match automatic $outmode]} {
        if {[string match {} $inname]} {
            set outname {}
            set outchan stdout
        } else {
            set outname [make_ppm_name $inname]
            if {[string match {} $outname]} {
                puts stderr "Unable to generate unique output name;\
			Skipping."
                incr errcount
                continue
            }
        }
    }

    # Use Tk image photo write command.
    if {[string match {} $outname]} {
	# Use temporary file to fake stdout output
	Oc_TempFile New temp -stem any2ppm
	set tempname [$temp AbsoluteName]
	$temp Claim
	$temp Delete
    } else {
	set tempname $outname
    }
    if {[catch {$pic write $tempname -format $outFormat} errmsg]} {
	puts stderr " $outFormat WRITE ERROR: $errmsg"
	incr errcount
	catch {file delete $tempname}
	exit [incr errcount]
    }
    image delete $pic
    if {[string match {} $outname]} {
	# Copy results from $tempname to $outchan
	# (which *should* be stdout!)
	set tempchan [open $tempname r]
	fconfigure $tempchan -translation binary
	fconfigure $outchan -translation binary
	if {[catch {fcopy $tempchan $outchan} msg]} {
	    puts stderr "FATAL ERROR: $msg"
	    catch {close $tempchan}
	    catch {file delete $tempname}
	    exit [incr errcount]
	}
	close $tempchan
	file delete $tempname
	flush $outchan
    }       

    if {!$noinfo} {
        if {[info exists outchan] && [string match stdout $outchan]} {
            puts stderr "Output written to <stdout>"
        } elseif {[info exists outname]} {
            puts stderr "Output written to $outname"
        } else {
            puts stderr "Unknown output (programming error?)"
        }
    }

}

exit $errcount
