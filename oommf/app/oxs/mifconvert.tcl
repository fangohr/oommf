#!/bin/sh
#    v--Edit here if necessary \
exec tclsh "$0" ${1+"$@"}

global tcl_precision
if {[catch {set tcl_precision 0}]} {
   set tcl_precision 17
}

set progvers "MIF Conversion utility 1.2.0.4"

set infochan stdout
set errchan stderr

proc Usage { cid } {
    puts $cid "Usage: tclsh mifconvert.tcl \[OPTIONS] <infile> <outfile>"
    puts $cid "  Use \"-\" to read from/write to <stdin>/<stdout>."
    puts $cid "Available options:"
    puts $cid " -f, --force    force overwrite of <outfile>"
    puts $cid " --format fmt   output format; one of 1.1, 1.2, 2.1 (default)"
    puts $cid " -h, --help     display help message and exit"
    puts $cid " --nostagecheck disable stage count check in output file"
    puts $cid " -q, --quiet    no informational or warning messages"
    puts $cid " --unsafe       run embedded scripts in an unsafe interpreter"
    puts $cid " -v, --version  print version string and exit"
}

# Check for command line options
set index [lsearch -regexp $argv {^[-]+(h|help)$}]
if {$index>=0} {
    puts $infochan $progvers
    puts $infochan "Converts OOMMF files between MIF 1.1 and MIF 1.2,"
    puts $infochan "  and from MIF 1.x/2.0 to 2.1 formats."
    Usage $infochan
    exit 0
}
set index [lsearch -regexp $argv {^[-]+(v|version)$}]
if {$index>=0} {
    puts $infochan $progvers
    exit 0
}
set index [lsearch -regexp $argv {^[-]+(f|force)$}]
if {$index<0} {
    set forceflag 0
} else {
    set forceflag 1
    set argv [lreplace $argv $index $index]
}
set index [lsearch -regexp $argv {^[-]+format$}]
if {$index<0} {
    set output_type 2.1
} else {
    set fmtindex [expr {$index+1}]
    if {$fmtindex>=[llength $argv]} {
	puts $errchan "ERROR: Missing output format specification"
	Usage $errchan
	exit 1
    }
    set output_type [lindex $argv $fmtindex]
    if {[string compare $output_type 1.1]!=0 \
	    && [string compare $output_type 1.2]!=0 \
	    && [string compare $output_type 2.1]!=0} {
	puts $errchan "ERROR: Invalid output format specification"
	Usage $errchan
	exit 1
    }
    set argv [lreplace $argv $index $fmtindex]
}

set index [lsearch -regexp $argv {^[-]+nostagecheck$}]
if {$index<0} {
    set nostagecheckflag 0
} else {
    set nostagecheckflag 1
    set argv [lreplace $argv $index $index]
}
set index [lsearch -regexp $argv {^[-]+(q|quiet)$}]
if {$index<0} {
    set quietflag 0
} else {
    set quietflag 1
    set argv [lreplace $argv $index $index]
}
set index [lsearch -regexp $argv {^[-]+unsafe$}]
if {$index<0} {
    set unsafeflag 0
} else {
    set unsafeflag 1
    set argv [lreplace $argv $index $index]
}

# Check for usage request or bad args
if {[llength $argv]!=2} {
    puts $errchan "ERROR: Bad command line."
    Usage $errchan
    exit 2
}
set infilename [lindex $argv 0]
if {[string match "-" $infilename]} {
    set infilename {}  ;# Indicate stdin
}
set outfilename [lindex $argv 1]
if {[string match "-" $outfilename]} {
    set outfilename {} ;# Indicate stdout
}
if {[string compare $infilename $outfilename]==0 \
	&& ![string match {} $infilename]} {
    puts $errchan "ERROR: Input and output must be two different files."
    Usage $errchan
    exit 3
}

###########################################################
# Proc to converts pathname to an absolute path.  This code
# is stolen, err, borrowed from the Oc_DirectPathname proc
# in oommf/pkg/oc/procs.tcl
#
proc DirectPathname { pathname } {
    global DirectPathnameCache
    set canCdTo [file dirname $pathname]
    set rest [file tail $pathname]
    switch -exact -- $rest {
        .       -
        .. {
            set canCdTo [file join $canCdTo $rest]
            set rest ""
        }
    }
    if {[string match absolute [file pathtype $canCdTo]]} {
        set index $canCdTo
    } else {
        set index [file join [pwd] $canCdTo]
    }
    if {[info exists DirectPathnameCache($index)]} {
        return [file join $DirectPathnameCache($index) $rest]
    }
    if {[catch {set savedir [pwd]} msg]} {
        return -code error "Can't determine pathname for\n\t$pathname:\n\t$msg"
    }
    # Try to [cd] to where we can [pwd]
    while {[catch {cd $canCdTo}]} {
        switch -exact -- [file tail $canCdTo] {
            "" {
                # $canCdTo is the root directory, and we can't cd to it.
                # This means we know the direct pathname, even though we
                # can't cd to it or any of its ancestors.
                set DirectPathnameCache($index) $canCdTo        ;# = '/'
                return [file join $DirectPathnameCache($index) $rest]
            }
            . {
                # Do nothing.  Leave $rest unchanged
            }
            .. {
                # NOMAC: Assuming '..' means 'parent directory'
                # Don't want to shift '..' onto $rest.
                # Make recursive call instead.
                set DirectPathnameCache($index) [file dirname \
                        [DirectPathname [file dirname $canCdTo]]]
                return [file join $DirectPathnameCache($index) $rest]
            }
            default {
                ;# Shift one path component from $canCdTo to $rest
                set rest [file join [file tail $canCdTo] $rest]
            }
        }
        set canCdTo [file dirname $canCdTo]
        set index [file dirname $index]
    }
    # We've successfully changed the working directory to $canCdTo
    # Try to use [pwd] to get the direct pathname of the working directory
    catch {set DirectPathnameCache($index) [pwd]}
    # Shouldn't be a problem with a [cd] back to the original working directory
    cd $savedir
    if {![info exists DirectPathnameCache($index)]} {
        # Strange case where we could [cd] into $canCdTo, but [pwd] failed.
        # Try a recursive call to resolve matters.
        set DirectPathnameCache($index) [DirectPathname $canCdTo]
    }
    return [file join $DirectPathnameCache($index) $rest]
}

######################################################################
# This file really contains three separate converters, one for
# converting MIF 1.x to 1.x, one for converting MIF 1.x to 2.1,
# and one for converting MIF 2.0 to MIF 2.1.  First follows some
# common file handling code...

# Output file existence check
if {[string match {} $outfilename]} {
    set outfilename "<stdout>"
    set infochan stderr  ;# Where else?
} elseif {!$forceflag && [file exists $outfilename]} {
    puts $errchan "ERROR: Output file $outfilename already exists."
    puts $errchan "  Use -f flag to force overwrite."
    puts $errchan "ABORT"
    exit 7
}

# Open input file & determine file type
if {[string match {} $infilename]} {
    set infile stdin
    set infilename "<stdin>"
    set fullinname "<stdin>"
} else {
    set infile [open $infilename "r"]
    set fullinname [DirectPathname $infilename]
}
set ws "\[ \t\n\r\]"   ;# White space
set bs "\[^ \t\n\r\]"  ;# Black space
set major 0
set minor 0
set firstline [gets $infile]
if {![regexp -- "^#$ws*MIF$ws*(\[0-9\]+)\\.(\[0-9\]+)$ws*$" $firstline \
        dummy major minor]} {
    puts $errchan "Input file $infilename not in MIF format"
    close $infile
    exit 4
}

if {$major==1 && $minor==1} {
    set input_type 1.1
} elseif {$major==1 && $minor==2} {
    set input_type 1.2
} elseif {$major==2 && $minor==0} {
    set input_type 2.0
} elseif {$major==2 && $minor==1} {
    puts $errchan "ERROR: Input file $infilename already in MIF 2.1 format."
    close $infile
    exit 4
} else {
    puts $errchan "Input file $infilename in MIF $major.$minor format."
    puts $errchan " -- This format not supported by $progvers."
    close $infile
    exit 4
}

if {[string compare "${major}.${minor}" $output_type]==0} {
    if {!$quietflag} {
	puts $errchan "WARNING: Input file $infilename already in\
		MIF $output_type format."
    }
} elseif {[regexp {^1\.[12]$} $output_type] && $major!=1} {
    puts $errchan "ERROR: $progvers does not support conversion\
	    from MIF $input_type to MIF $output_type."
    close $infile
    exit 4
}


######################################################################
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! #
# START OF MIF 1.x -> MIF 1.x CONVERTER BLOCK.                       #
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! #
######################################################################
if {[string compare "1.1" $output_type]==0 \
	|| [string compare "1.2" $output_type]==0} {

# Read input file 1 record at a time, and store in datalist.
set datalist []
set workline {}
set has_solvertype 0
while {[gets $infile line]>=0} {
    if {[regexp -- {^(.*)\\$} $line dummy leader]} {
        # Continued line
        append workline $leader
        continue
    }
    append workline $line
    lappend datalist $workline

    # No processing on comment or blank lines
    if {[string match {} $workline] || \
            [regexp -- "^$ws*#" $workline]} {
        set workline {}
        continue
    }

    # Otherwise parse into record id + parameter list
    if {![regexp -- {^([^:=]+)[:=]+(.+)$} $workline dummy rid params]} {
        puts $errchan "Invalid input line: $workline"
        exit 5
    }

    # Strip whitespace and case from record id
    regsub -all -- $ws $rid {} rid
    set rid [string tolower $rid]

    # Strip comments from parameter list
    set templist [split $params "#"]
    set tempparams {}
    foreach piece $templist {
        append tempparams $piece
        if {![catch {llength $tempparams} size] && $size>0} {
            break ;# Complete list, so pitch remainder (which is comment)
        }
        append tempparams "#"
    }
    if {[catch {llength $tempparams} size] || $size<1} {
        puts $errchan "Record $rid has invalid parameter list: $params"
        exit 6
    }
    set params [string trim $tempparams]

    # Extract info of interest
    switch -exact $rid {
	cellsize {
	    set cellsize_index [expr {[llength $datalist]-1}]
	    set cellsize_params $params
	}
	partthickness {
	    set thickness_index [expr {[llength $datalist]-1}]
	    set thickness_params $params
	}
        solvertype {
           if {[string compare 1.1 $output_type]==0} {
              # No "Solver Type" entry in MIF 1.1 format
              set datalist [lreplace $datalist end end]
           } else {
              set has_solvertype 1
           }
        }
    }
    set workline {}
}
close $infile

# If output is MIF 1.2, and no solver type record has been
# found, then add a default one.
if {[string compare 1.2 $output_type]==0 && !$has_solvertype} {
   lappend datalist "Solver Type: RKF54"
}

# Modify cellsize record as appropriate
if {[string compare 1.1 $output_type]==0} {
    if {[llength $cellsize_params]==3} {
	foreach {xsize ysize zsize} $cellsize_params break
	set thick [lindex $thickness_params 0]
	if {$xsize!=$ysize} {
	    puts $errchan "ERROR: Cell x length ($xsize)\
		    must match cell y length ($ysize)"
	    puts $errchan "  to meet output MIF 1.1 specification."
	    puts $errchan "ABORT"
	    exit 16
	} elseif {$zsize!=$thick} {
	    puts $errchan "ERROR: Cell thickness ($zsize)\
		    must match part thickness ($thick)"
	    puts $errchan "  to meet output MIF 1.1 specification."
	    puts $errchan "ABORT"
	    exit 16
	}
	set datalist [lreplace $datalist \
		$cellsize_index $cellsize_index "Cell Size:$xsize"]
    }
} elseif {[string compare 1.2 $output_type]==0} {
    if {[llength $cellsize_params]==1} {
	set xsize [lindex $cellsize_params 0]
	set thick [lindex $thickness_params 0]
	set datalist [lreplace $datalist \
		$cellsize_index $cellsize_index \
		"Cell Size:$xsize $xsize $thick"]
    }
}

# Dump data to output file
if {[string compare "<stdout>" $outfilename]==0} {
    set outfile stdout
} else {
    set outfile [open $outfilename "w"]
}
puts $outfile "# MIF $output_type"
puts $outfile "# This file converted from MIF $input_type format by $progvers"
puts $outfile "# Original file: $fullinname"
foreach record $datalist {
    puts $outfile $record
}
close $outfile
if {!$quietflag} {
    puts $infochan "TRANSLATED [list $infilename] -> [list $outfilename]"
}
######################################################################
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! #
# END OF MIF 1.x -> MIF 1.x CONVERTER BLOCK.                         #
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! #
######################################################################

######################################################################
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! #
# START OF MIF 1.x -> MIF 2.1 CONVERTER BLOCK.                       #
# The 2.0 converter is farther below.                                #
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! #
######################################################################
} elseif {[string compare "1.1" $input_type]==0 \
	|| [string compare "1.2" $input_type]==0} {
set PI [expr {4*atan(1.0)}]
set MU0 [expr {4*$PI*1e-7}]

set export_filenames {} ;# List of exported filenames

###########################################################
# Converts an absolute pathname to a pathname relative to basedir.
# NOTE: basedir must be a _directory_, not a file, and both
#  basedir and path must be absolute paths.
#
proc MakeRelativePath { basedir path } {
    if {![string match absolute [file pathtype $basedir]]} {
        return -code error "Input basedir not absolute: $basedir"
    }
    if {![string match absolute [file pathtype $path]]} {
        return -code error "Input file path not absolute: $path"
    }
    set basecomp [file split $basedir]
    set pathcomp [file split $path]

    if {[string compare [lindex $basecomp 0] [lindex $pathcomp 0]]!=0} {
        # Different volumes.  So punt and just return absolute path
        return $path
    }
    
    # Find length of common initial path segments
    set size 0
    foreach b $basecomp p $pathcomp {
        if {[string compare $b $p]==0} {
            incr size
        } else {
            break
        }
    }

    # Build up new path list
    set newpath {}
    for {set i $size} {$i<[llength $basecomp]} {incr i} {
        lappend newpath ..
    }

    return [eval file join $newpath [lrange $pathcomp $size end]]
}

##############################################################
# Converts a MIF 1.x pathname relative to basedir to a MIF 2.1
# pathname relative to outfile.  If oommfroot is not set, this
# proc just returns the original name, unchanged.  If the output
# file is <stdout>, then returns an absolute path.
#   Final name is automatically appended to the export_filenames
# list.
#
proc MifPathConvert { mif1name } {
    global oommfroot outfilename export_filenames
    # For safety, convert backslashes to forward slashes
    regsub -all \\\\ $mif1name / mif1name
    # Strip outer list wrapper, if any
    if {![catch {llength $mif1name} pieces] && $pieces==1} {
       set mif1name [lindex $mif1name 0]
       # Note: lindex does backslash substitution, so it is
       # important to replace backslashes before this step.
    }
    if {[info exists oommfroot] && \
           [string match relative [file pathtype $mif1name]]} {
       # Question: How should volumerelative paths be handled?
       set mif1name [DirectPathname [file join $oommfroot $mif1name]]
       if {![string compare "<stdout>" $outfilename]==0} {
          set outdir [DirectPathname [file dirname $outfilename]]
          set mif1name [MakeRelativePath $outdir $mif1name]
       }
    }
    lappend export_filenames $mif1name
    return $mif1name
}

###########################################################
# ExtractRecord finds the first record in the import list with id $rid.
# The record (both id and params) are removed from the list.  The
# return value is the param field for the record.  An error is thrown
# if no record id matches $rid.
#
proc ExtractRecord { listname rid } {
    upvar 1 $listname inlist
    set index [lsearch -exact $inlist $rid]
    if {$index<0} {
        error "Record id $rid not found in input list"
    }
    set params [lindex $inlist [expr {$index + 1}]]
    set inlist [lreplace $inlist $index [expr {$index + 1}]]
    return $params
}

###########################################################
# EatAllRecords is similar to ExtractRecord, except that if multiple
# record id matches are found, then all are removed and the param field
# for the last is returned.
#
proc EatAllRecords { listname rid } {
    upvar 1 $listname inlist
    set params [ExtractRecord inlist $rid]
    while {![catch {ExtractRecord $listname $rid} tmp]} {
        set params $tmp
    }
    return $params
}

#############################################################
# Routine to determine if a sub-grouped list can be collapsed.
# Returns new list, which will either be the original list
# or a collapsed version thereof.
#
proc CollapseList { grouped_list } {
    set collapse_list 1
    set check_value [lindex [lindex $grouped_list 0] 0]
    foreach sublist $grouped_list {
        if {[llength $sublist]==1} {
            # Single item
            if {$sublist != $check_value} {
                set collapse_list 0
                break;
            }
        } else {
            # Subgroup.  Assume only 1 level of nesting
            foreach elt \
                    [lrange $sublist 0 [expr {[llength $sublist]-2}]] {
                if {$elt != $check_value} {
                    set collapse_list 0
                    break;
                }
            }
        }
    }
    if {$collapse_list} {
        return $check_value
    }
    return $grouped_list
}

#############################################################
# Routine to test if a grouped list is actually flat.
#
proc IsFlat { grouped_list } {
    foreach sublist $grouped_list {
	if {[llength $sublist]>1} {
	    return 0  ;# Not flat
	}
    }
    return 1 ;# Yep, it's flat.
}

#############################################################
# Message output routine
proc MessageDump { chanid prefix msg } {
    global quietflag
    regsub -all -- "\n" $msg "\n$prefix" msg
    if {!$quietflag} {
	puts $chanid "$prefix$msg"
    }
}

#############################################################
# Routine that converts a MIF 1.1 FileSeq applied field
# specification into an explicit list of files.  The FileSeq
# command relies on a Tcl script in a supplied file; this
# script is interpreted in a slave interpreter, the "safeness"
# of which is governed by the "-unsafe" command line option.
#  Note: This routine opens tclfile from the OOMMF root directory,
# so the caller shouldn't modify its name from that specified
# in the MIF 1.1 file.  Also, MakeFileSeqList calls MifPathConvert
# on all the generated export filenames, so those can be used
# by the client as is.
proc MakeFileSeqList {tclfile procname fieldvalues} {
    global unsafeflag oommfroot infochan

    # Run this proc with oommfroot as working directory
    # This may be needed by some MIF 1.1 scripts
    set savedir [pwd]
    if {[info exists oommfroot]} {
	cd $oommfroot
    }

    if {$unsafeflag} {
	set slave [interp create]
	set errsafeflag {}
    } else {
	set slave [interp create -safe]
	set errsafeflag "\nYou may want to try running mifconvert\
		with the --unsafe option."
    }

    # Alias for Oc_Log command
    interp eval $slave [list proc Oc_Log args {OXSDump [lindex $args 1]}]
    interp alias $slave OXSDump {} MessageDump $infochan " fileseq output> "

    # Read file into a string.  Hopefully this isn't too big.
    # Note: tclfile is opened with cwd at OOMMF root directory.
    if {[catch {open $tclfile} chan]} {
	interp delete $slave
	cd $savedir
	return -code error \
		"ERROR: Unable to open FileSeq script file\
		$tclfile for reading."
    }
    set filestr [read $chan]
    close $chan

    # Source filestr
    if {[catch {$slave eval $filestr} errmsg]} {
	interp delete $slave
	cd $savedir
	return -code error \
		"ERROR sourcing FileSeq script file\
		$tclfile: $errmsg$errsafeflag"
    }

    # Loop through field values to build output list
    set namelist {}
    set stagenumber 0
    foreach field $fieldvalues {
	set cmd [linsert $field 0 $procname]
	lappend cmd $stagenumber
	if {[catch {$slave eval $cmd} result]} {
	    cd $savedir
	    return -code error \
		    "ERROR running command \"$cmd\"\
		    in FileSeq script file $tclfile: $result$errsafeflag"
	}
	lappend namelist $result
	incr stagenumber
    }

    interp delete $slave
    cd $savedir

    # Now that we've cd'd back to the MIF working directory,
    # we can use MifPathConvert to make proper MIF 2.1 paths
    set convertednames {}
    foreach name $namelist {
	lappend convertednames [MifPathConvert $name]
    }
    return $convertednames
}

###########################################################
# Strips trailing duplicates from a list, but otherwise
# retains order.  (Compare to 'lsort -unique'.)
proc StripDuplicates { list } {
    set i [expr {[llength $list]-1}]
    while {$i>0} {
	set key [lindex $list $i]
	if {[lsearch -exact [lrange $list 0 [expr {$i-1}]] $key]>=0} {
	    # Duplicate
	    set list [lreplace $list $i $i]
	}
	incr i -1
    }
    return $list
}


###########################################################

# See if we can determine OOMMF root directory.  This is needed
# because MIF 1.x filenames are taken relative to oommf root
set oommfroot \
        [file dirname [file dirname [file dirname \
        [DirectPathname [info script]]]]]
# If purported oommfroot directory contains oommf.tcl,
# then it's a good bet that's the right directory
if {![file exists [file join $oommfroot oommf.tcl]]} {
    unset oommfroot
    if {!$quietflag} {
	puts $errchan "\n"
	puts $errchan "WARNING: Unable to determine OOMMF root directory."
	puts $errchan "   Exporting embedded filenames without relative"
	puts $errchan "   directory conversion.\n"
    }
}

# Read input file, 1 line at a time.
set workline {}
while {[gets $infile line]>=0} {
    if {[regexp -- {^(.*)\\$} $line dummy leader]} {
        # Continued line
        append workline $leader
        continue
    }
    append workline $line

    # Skip comment and blank lines
    if {[string match {} $workline] || \
            [regexp -- "^$ws*#" $workline]} {
        set workline {}
        continue
    }

    # Otherwise parse into record id + parameter list
    if {![regexp -- {^([^:=]+)[:=]+(.+)$} $workline dummy rid params]} {
        puts $errchan "Invalid input line: $workline"
        exit 5
    }

    # Strip whitespace and case from record id
    regsub -all -- $ws $rid {} rid
    set rid [string tolower $rid]

    # Strip comments from parameter list
    set templist [split $params "#"]
    set tempparams {}
    foreach piece $templist {
        append tempparams $piece
        if {![catch {llength $tempparams} size] && $size>0} {
            break ;# Complete list, so pitch remainder (which is comment)
        }
        append tempparams "#"
    }
    if {[catch {llength $tempparams} size] || $size<1} {
        puts $errchan "Record $rid has invalid parameter list: $params"
        exit 6
    }
    set params [string trim $tempparams]

    # Store record id + parameter list in input list
    lappend inlist $rid $params

    set workline {}
}
close $infile

##############################################################
# Write header
if {[string compare "<stdout>" $outfilename]==0} {
    set outfile stdout
} else {
    set outfile [open $outfilename "w"]
}
puts $outfile "# MIF 2.1"
puts $outfile "# This file converted from MIF $input_type format by $progvers"
puts $outfile "# Original file: $fullinname"
if {![catch {ExtractRecord inlist "usercomment"} comment]} {
    puts $outfile "#\n# $comment"
    while {![catch {ExtractRecord inlist "usercomment"} comment]} {
        puts $outfile "# $comment"
    }
    puts $outfile "#"
}
puts $outfile "\nset PI \[expr {4*atan(1.)}]"
puts $outfile "set MU0 \[expr {4*\$PI*1e-7}]\n"

# Initialize random number generator, if requested
if {[catch {EatAllRecords inlist "randomizerseed"} randomseed]} {
    set randomseed 0
}
if {$randomseed!=0} {
    puts $outfile "RandomSeed $randomseed\n"
}

##############################################################
# Get mesh info
set xspan [EatAllRecords inlist "partwidth"]
set yspan [EatAllRecords inlist "partheight"]
set zspan [EatAllRecords inlist "partthickness"]
set cellsize [EatAllRecords inlist "cellsize"]
set Ms [EatAllRecords inlist "ms"]

if {[string compare 1.1 $input_type]==0} {
    if {[llength $cellsize]!=1} {
	puts $errchan "ERROR: Input cellsize record\
		does not conform to MIF 1.1 spec."
	puts $errchan "ABORT"
	exit 15
    }
    lappend cellsize $cellsize $zspan
}
if {[llength $cellsize]!=3} {
    puts $errchan "ERROR: Invalid input cellsize record."
    puts $errchan "ABORT"
    exit 15
}


##############################################################
# Get part shape info.  The number of parameters varies
# by shape.  The documentation indicates that a valid file
# should have the proper number of parameters, but the mifio.cc
# code ignores extra parameters, so do the same here.
if {[catch {EatAllRecords inlist "partshape"} part_shape]} {
    set part_shape rectangle
}
if {[string match -nocase "rectangle" [lindex $part_shape 0]]} {
    set Ms_str "$Ms"
} elseif {[string match -nocase "ellipse" [lindex $part_shape 0]]} {
    puts $outfile {
proc Ellipse { Ms x y z } {
    set xrad [expr {2.*$x - 1.}]
    set yrad [expr {2.*$y - 1.}]
    set test [expr {$xrad*$xrad+$yrad*$yrad}]
    if {$test>1.0} {return 0}
    return $Ms
}}
    set Ms_str \
       "{ Oxs_ScriptScalarField { atlas :atlas script {Ellipse $Ms} } }"
} elseif {[string match -nocase "ellipsoid" [lindex $part_shape 0]]} {
    puts $outfile {
proc Ellipsoid { Ms x y z } {
    set xrad [expr {2.*$x - 1.}]
    set yrad [expr {2.*$y - 1.}]
    set zrad [expr {2.*$z - 1.}]
    set test [expr {$xrad*$xrad+$yrad*$yrad+$zrad*$zrad}]
    if {$test>1.0} {return 0}
    return $Ms
}}
    set Ms_str \
       "{ Oxs_ScriptScalarField { atlas :atlas script {Ellipsoid $Ms} } }"
} elseif {[string match -nocase "mask*" $part_shape]} {
    set maskfile [string trim [string range $part_shape 4 end]]
    set maskfile [list [MifPathConvert $maskfile]]
    puts $outfile "Specify Oxs_ImageAtlas:atlas {
  xrange {0 $xspan}
  yrange {0 $yspan}
  zrange {0 $zspan}
  image $maskfile
  viewplane xy
  colormap {
      white nonmagnetic
      black magnetic
  }
}\n"
   set Ms_str "{Oxs_AtlasScalarField {
    atlas :atlas
    values {
      nonmagnetic  0
      magnetic     $Ms
  } } }"
    if {!$quietflag} {
	puts $errchan "WARNING: MIF 2 mask file does not support\
		mmsolve2D variable thickness model."
    }
} else {
    puts $errchan "ERROR: Unsupported part shape: $part_shape"
    puts $errchan "ABORT"
    puts $outfile "error {INCOMPLETE MIF FILE}"
    exit 8
}

##############################################################
# Write mesh info
if {![string match -nocase "mask*" $part_shape]} {
    puts $outfile "Specify Oxs_BoxAtlas:atlas {
  xrange {0 $xspan}
  yrange {0 $yspan}
  zrange {0 $zspan}
}\n"
}

puts $outfile "Specify Oxs_RectangularMesh:mesh {
  cellsize [list $cellsize]
  atlas :atlas
}"

##############################################################
# Write crystalline anisotropy info 
set anisk1 [EatAllRecords inlist "k1"]
set anistype [EatAllRecords inlist "anisotropytype"]
if {[catch {EatAllRecords inlist "anisotropydir1"} anisdir1]} {
    set anisdir1 "1 0 0"
}
if {[catch {EatAllRecords inlist "anisotropydir2"} anisdir2]} {
    set anisdir2 "0 1 0"
}
if {[catch {EatAllRecords inlist "anisotropyinit"} anisinit]} {
    set anisinit "Constant"
}
set anisinit [string tolower $anisinit]
if {[string match uniformxy $anisinit]} {
    set axisinit_fmt {Oxs_PlaneRandomVectorField {
    min_norm 1.0
    max_norm 1.0
    plane_normal {0 0 1}
  } }
} elseif {[string match uniforms2 $anisinit]} {
    set axisinit_fmt {Oxs_RandomVectorField {
    min_norm 1.0
    max_norm 1.0
  } }
} else {
    # Assume anisinit == "constant"
    set axisinit_fmt {Oxs_UniformVectorField {
    norm 1
    vector {%s}
  } }
}

if {$anisk1 != 0.0} {
    if {[string match -nocase cubic $anistype]} {
        # Cubic anisotropy
        set axis1init_str [format $axisinit_fmt $anisdir1]
        set axis2init_str [format $axisinit_fmt $anisdir2]
        puts $outfile "
Specify Oxs_CubicAnisotropy {
  K1  $anisk1
  axis1 { $axis1init_str }
  axis2 { $axis2init_str }
}"
    } else {
        # Uniaxial anisotropy
        set axisinit_str [format $axisinit_fmt $anisdir1]
        puts $outfile "
Specify Oxs_UniaxialAnisotropy {
  K1  $anisk1
  axis { $axisinit_str }
}"
    }
}


##############################################################
# Write exchange info
set A [EatAllRecords inlist "a"]
puts $outfile "
Specify Oxs_UniformExchange {
  A  $A
}"

##############################################################
# Write demag info
set demag_type [EatAllRecords inlist "demagtype"]
if {![string match -nocase "none" $demag_type]} {
    if {[string match -nocase "fastpipe" $demag_type]} {
        puts $errchan "ERROR: 2D demag request, \"fastpipe\", not\
                supported.\nABORT"
        puts $outfile "error {INCOMPLETE MIF FILE}"
        exit 9
    }
    if {![string match -nocase "constmag" $demag_type]} {
	if {!$quietflag} {
	    puts $errchan "WARNING: Requested demag type, $demag_type,\
		    not supported.  Using constmag instead."
	}
    }
    puts $outfile "
Specify Oxs_Demag {}"
}


##############################################################
# Default stage control specs, and related info
if {[catch {string tolower [EatAllRecords inlist "solvertype"]} solver_type]} {
   set solver_type rkf54
}
if {[catch {EatAllRecords inlist "gyratio"} gamma]} {
    set gamma 2.21e5
}
if {[catch {EatAllRecords inlist "dampcoef"} alpha]} {
    set alpha 0.5
}
if {[string match cg $solver_type]} {
   # Convert mxh to mxHxm
   set torque_mult $Ms  ;# Convert to A/m
} else {
   # Convert mxh to dm_dt
   set torque_mult [expr {$Ms*$gamma*sqrt(1+$alpha*$alpha)}]
   set torque_mult [expr {$torque_mult*180e-9/$PI}] ;# Convert to deg/ns
}
if {[catch {EatAllRecords inlist "convergemxhvalue"} stop_mxh] && \
        [catch {EatAllRecords inlist "converge|mxh|value"} stop_mxh]} {
    set stop_mxh 1e-5
}
set default_control [list -torque $stop_mxh]
if {![catch {EatAllRecords inlist "defaultcontrolpointspec"} stop_spec]} {
    set default_control [string tolower [string trim $stop_spec]]
}
# NB: At this point, default_control is in MIF 1.0 units, i.e.,
#     |mxh| (dimensionless).


##############################################################
# Write applied field info.
# Field Range line in MIF 1.x has format
#     x0 y0 z0  x1 y1 z1  steps  (+optional control point specs)
set rangecount 0
set rangelist {}  ;# List of ranges.  Used for Oxs_UZeeman output.
set fieldslist {} ;# List of individual fields.
                  ## Used for Oxs_StageZeeman output of FileSeq input.
set stagecount 0
set lastfield {0 0 0}
set torque_control {}
set time_control {}
set iteration_control {}
while {![catch {ExtractRecord inlist "fieldrange"} range]} {
    incr rangecount

    array set specarr [list -torque 0 -time 0 -iteration 0]
    if {[llength $range]>7} {
        array set specarr [lrange $range 7 end]
        set range [lrange $range 0 6]
    } else {
        if {[llength $range]!=7} {
            puts $errchan "ERROR: Invalid Field Range record: $range"
            puts $errchan " Bad input file: Too few elements in range record."
            puts $errchan "ABORT"
            puts $outfile "error {INCOMPLETE MIF FILE}"
            exit 10
        }
        array set specarr $default_control
    }
    set specarr(-torque) [expr {$specarr(-torque)*$torque_mult}]


    lappend rangelist $range
    set interval_count [lindex $range 6]
    if {$interval_count==0} {
        incr stagecount
        set lastfield [lrange $range 0 2]
	lappend fieldslist $lastfield
        set repeat_count 1
    } else {
        set repeat_count $interval_count
	set startfield [lrange $range 0 2]
        if {$stagecount==0 || \
                [lindex $lastfield 0]!=[lindex $startfield 0] || \
                [lindex $lastfield 1]!=[lindex $startfield 1] || \
                [lindex $lastfield 2]!=[lindex $startfield 2]} {
            incr stagecount
            incr repeat_count
	    lappend fieldslist $startfield
        }
        incr stagecount $interval_count
        set lastfield [lrange $range 3 5]
	for {set i 1} {$i<$interval_count} {incr i} {
	    # Construct intermediate fields and add to fieldslist
	    set wgtb [expr {double($i)/double($interval_count)}]
	    set wgta [expr {1.0-$wgtb}]
	    set tempfield {}
	    foreach x $startfield y $lastfield {
		lappend tempfield [expr {$wgta*$x+$wgtb*$y}]
	    }
	    lappend fieldslist $tempfield
	}
	lappend fieldslist $lastfield
    }

    if {$repeat_count==1} {
        lappend torque_control    $specarr(-torque) 
        lappend time_control      $specarr(-time)
        lappend iteration_control $specarr(-iteration)
    } else {
        lappend torque_control    [list $specarr(-torque)    $repeat_count]
        lappend time_control      [list $specarr(-time)      $repeat_count]
        lappend iteration_control [list $specarr(-iteration) $repeat_count]
    }
}
if {$rangecount<1} {
    array set specarr [list -torque 0 -time 0 -iteration 0]
    array set specarr $default_control
    set specarr(-torque) [expr {$specarr(-torque)*$torque_mult}] ;# Convert
    ## stopping criterion from |mxh| (dimensionless) to |dm/dt| (deg/ns).
    set torque_control    $specarr(-torque) 
    set time_control      $specarr(-time)
    set iteration_control $specarr(-iteration)
    set rangelist [list [list 0 0 0 0 0 0 0]]
    set fieldslist [list [list 0 0 0]]
}

if {![catch {EatAllRecords inlist "fieldrangecount"} check]} {
    if {$check != [llength $rangelist]} {
	puts $errchan "ERROR: Specified \"Field Range Count\" = $check,\
		but [llength $rangelist] ranges detected."
	puts $errchan "ABORT"
	puts $outfile "error {INCOMPLETE MIF FILE}"
	exit 11
    }
}

# If each entry in torque_control (resp. time_control,
# iteration_control) is the same, then collapse list to
# a single value.
foreach ctrlname [list torque_control time_control iteration_control] {
    upvar 0 $ctrlname ctrl
    set ctrl [CollapseList $ctrl]
    if {![IsFlat $ctrl]} {
	lappend ctrl ":expand:"  ;# Append expand keyword
    }
}

# Parse Field Type record
set appfield {}
if {[catch {EatAllRecords inlist "fieldtype"} munch]} {
    # No fieldtype records; use default
    lappend appfield "uniform"
} else {
    set fieldtyperecord $munch ;# Save in case of error
    set fieldtype [string tolower [lindex $munch 0]]
    if {![string match multi $fieldtype]} {
        # Single field type; just lowercase routine name
        lappend appfield [lreplace $munch 0 0 $fieldtype]
    } else {
        # Multiple field types
        set routinecount [lindex $munch 1]
        set munch [lreplace $munch 0 1] ;# Dump "multi" keyword
	                                ## and routinecount
        while {[llength $munch]>0} {
            # Extract routine name and parameters, downcase
            # routine name, and append list to "appfield"
            set paramcount [lindex $munch 0]
            set routine [lrange $munch 1 $paramcount]
            set name [string tolower [lindex $routine 0]]
            lappend appfield [lreplace $routine 0 0 $name]

            # Toss current routine
            set munch [lreplace $munch 0 $paramcount]
        }
        if {[llength $appfield]!=$routinecount} {
            puts $errchan "ERROR: Invalid Field Type record: $fieldtyperecord"
            puts $errchan " Multi field type routinecount=$routinecount,\
		    but [llength $appfield] field routines found."
            puts $errchan "ABORT"
            puts $outfile "error {INCOMPLETE MIF FILE}"
            exit 11
        }
    }
}

# Write out applied field block for each "appfield"
set fieldcount 0
foreach routine $appfield {
    set type [lindex $routine 0]
    switch -exact -- $type {
        uniform {
            set nofield 1
            if {[llength $rangelist]>1} {
                set nofield 0
            } elseif {[llength $rangelist]==1} {
                # Skip if range list is {0 0 0 0 0 0 0}
                foreach x [lindex $rangelist 0] {
                    if {$x != 0.0} {
                        set nofield 0
                        break
                    }
                }
            }
            if {!$nofield} {
                puts $outfile \
                        "\nSpecify Oxs_UZeeman:extfield$fieldcount \[subst {"
                puts $outfile \
                        "  comment {Field values in Tesla; scale to A/m}"
                puts $outfile {  multiplier [expr {1/$MU0}]}
                puts $outfile "  Hrange {"
                foreach range $rangelist {
                    puts $outfile "    [list $range]"
                }
                puts $outfile "  }\n}]"
            }
        }
        onefile {
	    set filename [MifPathConvert [lindex $routine 1]]
	    if {[llength $routine]<3} {
		set mult 1.0
	    } else {
		set mult [lindex $routine 2]
	    }
	    set mult [expr {$mult/$MU0}] ;# Convert from Tesla to A/m
	    puts $outfile "
Specify Oxs_FixedZeeman:extfield$fieldcount {
  field { Oxs_FileVectorField {
    atlas :atlas
    file [list $filename]
    multiplier $mult
  } }
}"
	}
        fileseq {
	    set tclfile [lindex $routine 1]
	    set procname [lindex $routine 2]
	    if {[llength $routine]<4} {
		set mult 1.0
	    } else {
		set mult [lindex $routine 3]
	    }
	    set mult [expr {$mult/$MU0}] ;# Convert from Tesla to A/m
	    if {[catch {MakeFileSeqList $tclfile $procname $fieldslist} \
		    filelist]} {
		puts $errchan "ERROR: Unable to process FileSeq directive."
		puts $errchan " ==>$filelist"
		puts $errchan "ABORT"
		puts $outfile "error {INCOMPLETE MIF FILE}"
		exit 12
	    }
	    puts $outfile "
Specify Oxs_StageZeeman:extfield$fieldcount {
  files [list $filelist]
  multiplier $mult
}"
	}
        default {
            puts $errchan \
                    "ERROR: Unsupported applied field type: $type"
            puts $errchan "ABORT"
            puts $outfile "error {INCOMPLETE MIF FILE}"
            exit 12
        }
    }
    incr fieldcount
}

##############################################################
# Write evolver info
if {[catch {EatAllRecords inlist "doprecess"} do_precess]} {
    set do_precess 1
}
if {[catch {EatAllRecords inlist "mintimestep"} min_timestep]} {
    set min_timestep {}
}
if {![string match {} $min_timestep]} {
    set min_timestep "\n  min_timestep $min_timestep"
}
if {[catch {EatAllRecords inlist "maxtimestep"} max_timestep]} {
    set max_timestep {}
}
if {![string match {} $max_timestep]} {
    set max_timestep "\n  max_timestep $max_timestep"
}
switch $solver_type {
   cg {
      puts $outfile "\nSpecify Oxs_CGEvolve:evolver {}"
   }
   euler {
      puts $outfile "
Specify Oxs_EulerEvolve:evolver {
  do_precess $do_precess
  gamma_LL $gamma
  alpha $alpha$min_timestep$max_timestep
}"
   }
   default {
      puts $outfile "
Specify Oxs_RungeKuttaEvolve:evolver {
  do_precess $do_precess
  gamma_LL $gamma
  alpha $alpha$min_timestep$max_timestep
  method $solver_type
}"
   }
}

##############################################################
# Write driver info
if {[catch {EatAllRecords inlist "initmag"} init_mag]} {
    set init_mag "random"
}
set init_name [string tolower [lindex $init_mag 0]]
switch -exact -- $init_name {
    "random" {
        set m0_str "{ Oxs_RandomVectorField { min_norm 1  max_norm 1 } }"
    }
    "uniform" {
        if {[llength $init_mag]!=3} {
            puts $errchan "ERROR: Invalid mag init record: $init_mag"
            puts $errchan "ABORT"
            puts $outfile "error {INCOMPLETE MIF FILE}"
            exit 13
        }
        set theta [expr {[lindex $init_mag 1]*$PI/180.}]
        set phi [expr {[lindex $init_mag 2]*$PI/180.}]
        set x [expr {sin($theta)*cos($phi)}]
        if { abs($x) < 1e-16 } { set x 0.0 }
        set y [expr {sin($theta)*sin($phi)}]
        if { abs($y) < 1e-16 } { set y 0.0 }
        set z [expr {cos($theta)}]
        if { abs($z) < 1e-16 } { set z 0.0 }
        set m0_str "{ $x $y $z }"
    }
    "avffile" {
        set filename [string trim [string range $init_mag 7 end]]
        set filename [list [MifPathConvert $filename]]
        set m0_str "{ Oxs_FileVectorField {
    atlas :atlas
    norm  1
    file $filename
  } }"
    }
    "1domain" {
        puts $outfile {
set sqrthalf [expr {sqrt(0.5)}]
proc OneDomain { cellsize x y z xrange yrange zrange } {
   # script_args relpt span
   global sqrthalf
   set sum [expr {$x*$xrange + $y*$yrange - 0.5*$cellsize}]
   if { $yrange > $xrange } {
      if {$sum < $xrange || $sum>$yrange + $cellsize} {
         set vx $sqrthalf
         set vy $sqrthalf
      } else {
         set vx 0.0
         set vy 1.0
      }
   } else {
      if {$sum < $yrange || $sum>$xrange + $cellsize} {
         set vx $sqrthalf
         set vy $sqrthalf
      } else {
         set vx 1.0
         set vy 0.0
      }
   }
   return [list $vx $vy 0.0]
}}
        set xcellsize [lindex $cellsize 0]
        set m0_str [list [subst { Oxs_ScriptVectorField {
    atlas :atlas
    script {OneDomain $xcellsize}
    script_args {relpt span}
    norm 1
  } }]]
    }
    "4domain" {
        puts $outfile {
proc FourDomain { x y z xrange yrange zrange } {
    set x [expr {$x*$xrange}]
    set y [expr {$y*$yrange}]
    set halfang 0.70710678
    if {$xrange>=$yrange} {
        # Part width larger than height
        set matcherror [expr {$xrange/65536.0}]
        if {$x<0.5*($yrange-$matcherror)} {
            if {abs($y-$x)<$matcherror} {
                return "$halfang -$halfang 0"
            } elseif {abs($x+$y-$yrange)<$matcherror} {
                return "-$halfang -$halfang 0"
            } elseif {$y<$x} {
                return "1 0 0"
            } elseif {$y>$yrange-$x} {
                return "-1 0 0"
            } else {
                return "0 -1 0"
            }
        } elseif {$xrange-0.5*($yrange-$matcherror)<$x} {
            if {abs($y+$x-$xrange)<$matcherror} {
                return "$halfang $halfang 0"
            } elseif {abs($y-$yrange+$xrange-$x)<$matcherror} {
                return "-$halfang $halfang 0"
            } elseif {$y<$xrange-$x} {
                return "1 0 0"
            } elseif {$y>$x-$xrange+$yrange} {
                return "-1 0 0"
            } else {
                return "0 1 0"
            }
        } else {
            if {abs($y-0.5*$yrange)<$matcherror} {
                return "0 0 1"   ;# Arbitrarily pick +z
            } elseif {$y<0.5*$yrange} {
                return "1 0 0"
            } else {
                return "-1 0 0"
            }
        }
    } else {
        # Part height larger than width
        set matcherror [expr {$yrange/65536.0}]
        if {$y<0.5*($xrange-$matcherror)} {
            if {abs($x-$y)<$matcherror} {
                return "$halfang -$halfang 0"
            } elseif {abs($y+$x-$xrange)<$matcherror} {
                return "$halfang $halfang 0"
            } elseif {$x<$y} {
                return "0 -1 0"
            } elseif {$x>$xrange-$y} {
                return "0 1 0"
            } else {
                return "1 0 0"
            }
        } elseif {$yrange-0.5*($xrange-$matcherror)<$y} {
            if {abs($x+$y-$yrange)<$matcherror} {
                return "-$halfang -$halfang 0"
            } elseif {abs($x-$xrange+$yrange-$y)<$matcherror} {
                return "-$halfang $halfang 0"
            } elseif {$x<$yrange-$y} {
                return "0 -1 0"
            } elseif {$x>$y-$yrange+$xrange} {
                return "0 1 0"
            } else {
                return "-1 0 0"
            }
        } else {
            if {abs($x-0.5*$xrange)<$matcherror} {
                return "0 0 1"   ;# Arbitrarily pick +z
            } elseif {$x<0.5*$xrange} {
                return "0 -1 0"
            } else {
                return "0 1 0"
            }
        }
    }
}}
        set m0_str "{ Oxs_ScriptVectorField \
        { atlas :atlas script FourDomain script_args {relpt span} norm  1 } }"
    }
    "7domain" {
        puts $outfile {
proc SevenDomain { cellsize x y z xrange yrange zrange } {
   # script_args relpt span
   set i [expr {int(round(($x*$xrange-0.5*$cellsize)/$cellsize))}]
   set j [expr {int(round(($y*$yrange-0.5*$cellsize)/$cellsize))}]
   set Nx [expr {int(round($xrange/$cellsize))}]
   set Ny [expr {int(round($yrange/$cellsize))}]
   if {$Ny>$Nx} {
      set slope [expr {double($Ny-1)/double(2*($Nx-1))}]
      set half [expr {double($Ny-1)/2.0}]
      set adj 0.0
      if {$Nx % 2 == 0} { set adj 0.5 }
      set test1 0 ; set test2 0 ; set test3 0 ; set test4 0
      if {$j<$half-$i*$slope-$adj}  { set test1 1 }
      if {$j<$i*$slope-$adj}        { set test2 1 }
      if {$j>$half+$i*$slope+$adj}  { set test3 1 }
      if {$j>$Ny-1-$i*$slope+$adj}  { set test4 1 }
      if {($test1 && $test2) || ($test3 && $test4)} {
         set vx -1.0 ; set vy  0.0
      } elseif {$test1 || $test4} {
         set vx  0.0 ; set vy  1.0
      } elseif {$test2 || $test3} {
         set vx  0.0 ; set vy -1.0
      } else {
         set vx  1.0 ; set vy  0.0
      }
   } else {
      set slope [expr {double($Nx-1)/double(2*($Ny-1))}]
      set half [expr {double($Nx-1)/2.0}]
      set adj 0.0
      if {$Ny % 2 == 0} { set adj 0.5 }
      set test1 0 ; set test2 0 ; set test3 0 ; set test4 0
      if {$i<$half-$j*$slope-$adj}  { set test1 1 }
      if {$i<$j*$slope-$adj}        { set test2 1 }
      if {$i>$half+$j*$slope+$adj}  { set test3 1 }
      if {$i>$Nx-1-$j*$slope+$adj}  { set test4 1 }
      if {($test1 && $test2) || ($test3 && $test4)} {
         set vx  0.0 ; set vy -1.0
      } elseif {$test1 || $test4} {
         set vx  1.0 ; set vy  0.0
      } elseif {$test2 || $test3} {
         set vx -1.0 ; set vy  0.0
      } else {
         set vx  0.0 ; set vy  1.0
      }
   }
   return [list $vx $vy 0.0]
}}
        set xcellsize [lindex $cellsize 0]
        set m0_str [list [subst { Oxs_ScriptVectorField {
    atlas :atlas
    script {SevenDomain $xcellsize}
    script_args {relpt span}
    norm 1
  } }]]
    }
    "vortex" {
        puts $outfile {
proc Vortex { cellsize x y z xspan yspan zspan} {
    # script_args: relpt span
    set xrad [expr {($x-0.5)*$xspan}]
    set yrad [expr {($y-0.5)*$yspan}]
    set normsq [expr {$xrad*$xrad+$yrad*$yrad}]
    if {$normsq < $cellsize*$cellsize} {return "0 0 1"}
    return [list [expr {-1*$yrad}] $xrad 0]
}}
        set m0_str [list [subst { Oxs_ScriptVectorField {
    atlas :atlas
    script {Vortex [lindex $cellsize 0]}
    script_args {relpt span}
    norm 1
  } }]]
    }
    "exvort" {
        puts $outfile {
proc Exvort { cellsize x y z xspan yspan zspan} {
    # script_args: relpt span
    set xrad [expr {($x-0.5)*$xspan}]
    set yrad [expr {($y-0.5)*$yspan}]
    set normsq [expr {$xrad*$xrad+$yrad*$yrad}]
    if {$normsq < $cellsize*$cellsize} {return "0 0 1"}
    return [list $yrad $xrad 0]
}}
        set m0_str [list [subst { Oxs_ScriptVectorField {
    atlas :atlas
    script {Exvort [lindex $cellsize 0]}
    script_args {relpt span}
    norm 1
  } }]]
    }
    "bloch" {
        if {[llength $init_mag]!=2} {
            puts $errchan "ERROR: Invalid mag init record: $init_mag"
            puts $errchan "ABORT"
            puts $outfile "error {INCOMPLETE MIF FILE}"
            exit 13
        }
        set theta [lindex $init_mag 1]
        if {$theta == 0} {
           set thetacos 1.0
           set thetasin 0.0
        } elseif {$theta == 90} {
           set thetacos 0.0
           set thetasin 1.0
        } elseif {$theta == 180} {
           set thetacos -1.0
           set thetasin  0.0
        } elseif {$theta == 270} {
           set thetacos  0.0
           set thetasin -1.0
        } else {
           set thetacos [expr {cos($theta*$PI/180.)}]
           set thetasin [expr {sin($theta*$PI/180.)}]
        }
        puts $outfile {
proc Bloch { thetacos thetasin rxcellsize x y z } {
    set xrad [expr {2.*$x-1.}]
    if {$xrad < -1*$rxcellsize} {
       set vy [expr {-1*$thetacos}]
       set vz [expr {-1*$thetasin}]
    } elseif {$xrad < $rxcellsize} {
       set vy $thetasin
       set vz [expr {-1*$thetacos}]
    } else {
       set vy $thetacos
       set vz $thetasin
    }
    return [list 0.0 $vy $vz]
}}
        set rxcellsize [expr {[lindex $cellsize 0]/$xspan}]
        set m0_str [list [subst { Oxs_ScriptVectorField {
    atlas :atlas
    script {Bloch $thetacos $thetasin $rxcellsize}
    norm 1
  } }]]
    }
    "neel" {
        if {[llength $init_mag]!=3} {
            puts $errchan "ERROR: Invalid mag init record: $init_mag"
            puts $errchan "ABORT"
            puts $outfile "error {INCOMPLETE MIF FILE}"
            exit 13
        }
        set theta [lindex $init_mag 1]
        if {$theta == 0} {
           set thetacos 1.0
           set thetasin 0.0
        } elseif {$theta == 90} {
           set thetacos 0.0
           set thetasin 1.0
        } elseif {$theta == 180} {
           set thetacos -1.0
           set thetasin  0.0
        } elseif {$theta == 270} {
           set thetacos  0.0
           set thetasin -1.0
        } else {
           set thetacos [expr {cos($theta*$PI/180.)}]
           set thetasin [expr {sin($theta*$PI/180.)}]
        }
        set width_prop [lindex $init_mag 2]
        set tx [expr {$thetacos/$xspan}]
        set ty [expr {$thetasin/$yspan}]
        set part_width [expr {1.0/sqrt($tx*$tx+$ty*$ty)}]
        set wall_width [expr {$width_prop*$part_width}]
        set wall_scale [expr {$PI/$wall_width}]
        puts $outfile {
proc Neel { thetacos thetasin scale x y z xspan yspan zspan} {
    # script_args: relpt span
    set x [expr {($x-0.5)*$xspan}]
    set y [expr {($y-0.5)*$yspan}]
    set off [expr {($thetacos*$x + $thetasin*$y)*$scale}]
    set xproj [expr {1/sqrt(1+$off*$off)}]
    set yproj [expr {$off*$xproj}]
    set vx [expr {$thetacos*$xproj - $thetasin*$yproj}]
    set vy [expr {$thetasin*$xproj + $thetacos*$yproj}]
    return [list $vx $vy 0.0]
}}
        set m0_str [list [subst { Oxs_ScriptVectorField {
    atlas :atlas
    script {Neel $thetacos $thetasin $wall_scale}
    script_args {relpt span}
    norm 1
  } }]]
    }
    "rightleft" {
        puts $outfile {
proc RightLeft { x y z} {
    # script_args: relpt
    if {$x<0.5} {
       set vx 1.0
    } else {
       set vx -1.0
    }
    return [list $vx 0. 0.]
}}
        set m0_str [list [subst { Oxs_ScriptVectorField {
    atlas :atlas
    script RightLeft
    script_args relpt
  } }]]
    }
    "crot" {
        if {[llength $init_mag]!=2} {
            puts $errchan "ERROR: Invalid mag init record: $init_mag"
            puts $errchan "ABORT"
            puts $outfile "error {INCOMPLETE MIF FILE}"
            exit 13
        }
        set theta [lindex $init_mag 1]
        if {$theta == 0} {
           set thetacos 1.0
           set thetasin 0.0
        } elseif {$theta == 90} {
           set thetacos 0.0
           set thetasin 1.0
        } elseif {$theta == 180} {
           set thetacos -1.0
           set thetasin  0.0
        } elseif {$theta == 270} {
           set thetacos  0.0
           set thetasin -1.0
        } else {
           set thetacos [expr {cos($theta*$PI/180.)}]
           set thetasin [expr {sin($theta*$PI/180.)}]
        }
        puts $outfile {
proc CRot { thetacos thetasin xcellsize wallrad x y z xspan yspan zspan} {
    # script_args: relpt span 
    set x [expr {($x-0.5)*$xspan}]
    set y [expr {($y-0.5)*$yspan}]
    set xoff [expr {$x*$thetacos+$y*$thetasin}]
    set yoff [expr {$y*$thetacos-$x*$thetasin}]
    set vx [set vy [set vz 0.0]]
    if {$xoff<-0.75*$xcellsize} {
       set vz -1
    } elseif {$xoff>1.75*$xcellsize \
       || ($xoff>0.75*$xcellsize && abs($yoff)<=$wallrad)} {
       set vz 1
    } elseif {$yoff>$wallrad} {
       set vx $thetacos
       set vy $thetasin
    } elseif {$yoff<-1*$wallrad} {
       set vx [expr {-1*$thetacos}]
       set vy [expr {-1*$thetasin}]
    } else {
       set vx [expr {-1*$thetasin}]
       set vy $thetacos
    }
    return [list $vx $vy $vz]
}}
        set xcellsize [lindex $cellsize 0]
        if {$yspan*abs($thetasin)<$xspan*abs($thetacos)} {
           set wallrad [expr {0.5*$yspan/abs($thetacos)}]
        } else {
           set wallrad [expr {0.5*$xspan/abs($thetasin)}]
        }
        set wallrad [expr {$wallrad-2.25*$xcellsize}]
        set m0_str [list [subst { Oxs_ScriptVectorField {
    atlas :atlas
    script {CRot $thetacos $thetasin $xcellsize $wallrad}
    script_args {relpt span}
    norm 1
  } }]]
    }
    "upoutdownheadtohead" {
        if {[llength $init_mag]!=2} {
            puts $errchan "ERROR: Invalid mag init record: $init_mag"
            puts $errchan "ABORT"
            puts $outfile "error {INCOMPLETE MIF FILE}"
            exit 13
        }
        set rwidth [lindex $init_mag 1]
        if {$rwidth<0.0 || $rwidth>1.0} {
            puts $errchan "ERROR: Invalid mag init record: $init_mag"
            puts $errchan "  \"width\" parameter must be between 0 and 1."
            puts $errchan "ABORT"
            puts $outfile "error {INVALID MIF FILE}"
            exit 14
        }
        puts $outfile {
proc UpOutDownHeadToHead { rwidth x y z } {
     set vy [set vz 0.0]
     if {$y<0.5*(1-$rwidth)} {
        set vy 1.0
     } elseif {$y>0.5*(1+$rwidth)} {
        set vy -1.0
     } else {
        set vz 1.0
     }
    return [list 0.0 $vy $vz]
}}
        set m0_str [list [subst { Oxs_ScriptVectorField {
    atlas :atlas
    script {UpOutDownHeadToHead $rwidth}
  } }]]
    }
    "uprightdownheadtohead" {
        if {[llength $init_mag]!=2} {
            puts $errchan "ERROR: Invalid mag init record: $init_mag"
            puts $errchan "ABORT"
            puts $outfile "error {INCOMPLETE MIF FILE}"
            exit 13
        }
        set rwidth [lindex $init_mag 1]
        if {$rwidth<0.0 || $rwidth>1.0} {
            puts $errchan "ERROR: Invalid mag init record: $init_mag"
            puts $errchan "  \"width\" parameter must be between 0 and 1."
            puts $errchan "ABORT"
            puts $outfile "error {INVALID MIF FILE}"
            exit 14
        }
        puts $outfile {
proc UpRightDownHeadToHead { rwidth x y z } {
     set vx [set vy 0.0]
     if {$y<0.5*(1-$rwidth)} {
        set vy 1.0
     } elseif {$y>0.5*(1+$rwidth)} {
        set vy -1.0
     } else {
        set vx 1.0
     }
    return [list $vx $vy 0.0]
}}
        set m0_str [list [subst { Oxs_ScriptVectorField {
    atlas :atlas
    script {UpRightDownHeadToHead $rwidth}
  } }]]
    }
    "inleftoutrot" {
        if {[llength $init_mag]!=2} {
            puts $errchan "ERROR: Invalid mag init record: $init_mag"
            puts $errchan "ABORT"
            puts $outfile "error {INCOMPLETE MIF FILE}"
            exit 13
        }
        set theta [lindex $init_mag 1]
        if {$theta == 0} {
           set thetacos 1.0
           set thetasin 0.0
        } elseif {$theta == 90} {
           set thetacos 0.0
           set thetasin 1.0
        } elseif {$theta == 180} {
           set thetacos -1.0
           set thetasin  0.0
        } elseif {$theta == 270} {
           set thetacos  0.0
           set thetasin -1.0
        } else {
           set thetacos [expr {cos($theta*$PI/180.)}]
           set thetasin [expr {sin($theta*$PI/180.)}]
        }
        puts $outfile {
proc InLeftOutRot { thetacos thetasin xcellsize wallrad x y z xspan yspan zspan} {
    # script_args: relpt span 
    set x [expr {($x-0.5)*$xspan}]
    set y [expr {($y-0.5)*$yspan}]
    set xoff [expr {$x*$thetacos+$y*$thetasin}]
    set yoff [expr {$y*$thetacos-$x*$thetasin}]
    set vx [set vy [set vz 0.0]]
    if {$xoff<-0.75*$xcellsize} {
       set vz -1
    } elseif {$xoff>0.75*$xcellsize} {
       set vz 1
    } else {
       set vx [expr {-1*$thetacos}]
       set vy [expr {-1*$thetasin}]
    }
    return [list $vx $vy $vz]
}}
        set xcellsize [lindex $cellsize 0]
        if {$yspan*abs($thetasin)<$xspan*abs($thetacos)} {
           set wallrad [expr {0.5*$yspan/abs($thetacos)}]
        } else {
           set wallrad [expr {0.5*$xspan/abs($thetasin)}]
        }
        set wallrad [expr {$wallrad-2.25*$xcellsize}]
        set m0_str [list [subst { Oxs_ScriptVectorField {
    atlas :atlas
    script {InLeftOutRot $thetacos $thetasin $xcellsize $wallrad}
    script_args {relpt span}
    norm 1
  } }]]
    }
    "spiral" {
        if {[llength $init_mag]!=3} {
            puts $errchan "ERROR: Invalid mag init record: $init_mag"
            puts $errchan "ABORT"
            puts $outfile "error {INCOMPLETE MIF FILE}"
            exit 13
        }
        set theta [lindex $init_mag 1]
        if {$theta == 0} {
           set thetacos 1.0
           set thetasin 0.0
        } elseif {$theta == 90} {
           set thetacos 0.0
           set thetasin 1.0
        } elseif {$theta == 180} {
           set thetacos -1.0
           set thetasin  0.0
        } elseif {$theta == 270} {
           set thetacos  0.0
           set thetasin -1.0
        } else {
           set thetacos [expr {cos($theta*$PI/180.)}]
           set thetasin [expr {sin($theta*$PI/180.)}]
        }
        set period [lindex $init_mag 2]
        if {$period<0.0 || $period>1.0} {
            puts $errchan "ERROR: Invalid mag init record: $init_mag"
            puts $errchan "  \"period\" parameter must be between 0 and 1."
            puts $errchan "ABORT"
            puts $outfile "error {INVALID MIF FILE}"
            exit 14
        }

        set tx [expr {$thetacos/$xspan}]
        set ty [expr {$thetasin/$yspan}]
        set part_width [expr {1.0/sqrt($tx*$tx+$ty*$ty)}]
        set period [expr {$period*$part_width}]
        if {$period<1e-12} {set period 1e-12}
        set wall_scale [expr {2*$PI/$period}]
        puts $outfile {
proc Spiral { thetacos thetasin scale cellsize x y z xspan yspan zspan} {
    # script_args: relpt span
    set x [expr {($x-0.5)*$xspan-0.5*$cellsize}]
    set y [expr {($y-0.5)*$yspan-0.5*$cellsize}]
    set off [expr {($thetacos*$x + $thetasin*$y)*$scale}]
    set xproj [expr {cos($off)}]
    set yproj [expr {sin($off)}]
    set vx [expr {$thetacos*$xproj - $thetasin*$yproj}]
    set vy [expr {$thetasin*$xproj + $thetacos*$yproj}]
    return [list $vx $vy 0.0]
}}
        set m0_str [list [subst { Oxs_ScriptVectorField {
    atlas :atlas
    script {Spiral $thetacos $thetasin $wall_scale [lindex $cellsize 0]}
    script_args {relpt span}
    norm 1
  } }]]
    }
    default {
        puts $errchan "ERROR: Unsupported mag init: $init_mag"
        puts $errchan "ABORT"
        puts $outfile "error {INCOMPLETE MIF FILE}"
        exit 13
    }
}

set basename [EatAllRecords inlist "baseoutputfilename"]
set basename [list [MifPathConvert $basename]]

if {![catch {EatAllRecords inlist "magnetizationoutputformat"} magout]} {
    set magout [string tolower $magout]
    set vector_format "\n  vector_field_output_format [list $magout]"
} else {
    if {![catch {EatAllRecords inlist "totalfieldoutputformat"} fieldout]} {
        set fieldout [string tolower $fieldout]
        set vector_format "\n  vector_field_output_format [list $fieldout]"
    } else {
        set vector_format "\n  vector_field_output_format {binary 4}"
    }
}

if {![catch {EatAllRecords inlist "datatableoutputformat"} fmt]} {
    set scalar_format "\n  scalar_output_format [list $fmt]"
} else {
    set scalar_format {}
}

if {[llength $torque_control]>1 || [lindex $torque_control 0] != 0} {
   if {[string match cg $solver_type]} {
      set stopping_torque "\n  stopping_mxHxm [list $torque_control]"
   } else {
      set stopping_torque "\n  stopping_dm_dt [list $torque_control]"
   }
} else {
    set stopping_torque {}
}
if {[llength $time_control]>1 || [lindex $time_control 0] != 0} {
    set stopping_time "\n  stopping_time [list $time_control]"
} else {
    set stopping_time {}
}
if {[llength $iteration_control]>1 || [lindex $iteration_control 0] != 0} {
    set stopping_its "\n  stage_iteration_limit [list $iteration_control]"
} else {
    set stopping_its {}
}
if {$nostagecheckflag} {
    set stage_count_check "\n  stage_count_check 0"
    ;## stage_count_check new in MIF 2.1.
} else {
    set stage_count_check {}
}

if {[string match cg $solver_type]} {
   set driver "Oxs_MinDriver"
   set stopping_time {}
} else {
   set driver "Oxs_TimeDriver"
}

puts $outfile "
Specify $driver {
  basename $basename$vector_format$scalar_format
  evolver :evolver
  mesh :mesh$stopping_torque$stopping_time$stopping_its
  stage_count $stagecount$stage_count_check
  Ms $Ms_str
  m0 $m0_str
}"

##############################################################
# Dump unused fields
if {[llength $inlist]>0} {
    puts $outfile "\n
##########################################
# Unused fields:
Ignore {"
    foreach {id val} $inlist { puts $outfile "  $id: $val" }
    puts $outfile "}"
}

close $outfile
if {!$quietflag} {
    puts $infochan "TRANSLATED [list $infilename] -> [list $outfilename]"
    puts $infochan " Please check [list $outfilename]\
	    for correctness and additional edits."
    foreach {id val} $inlist {
	puts $infochan " -ignored input-> $id: $val"
    }
    if {[llength $export_filenames]>0} {
	puts $infochan " Also check these embedded file path(s):"
	foreach f [StripDuplicates $export_filenames] {
	    puts $infochan "    [list $f]"
	}
    }
}
######################################################################
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! #
# END OF MIF 1.1 -> MIF 2.1 CONVERTER BLOCK.                         #
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! #
######################################################################

} elseif {[string compare "2.0" $input_type]==0} {
######################################################################
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! #
# START OF MIF 2.0 -> MIF 2.1 CONVERTER BLOCK.                       #
# The 1.1 converter is above.                                        #
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! #
######################################################################

# Read input file
set filestr [read $infile]
close $infile

proc MakePrettyInitString {initstr margin} {
    # Assumes initstr is a list, and returns a nicely formatted
    # string representation for that list.  The output begins
    # and ends a newline, unless initstr is empty.
    # NOTE: Might want to add brace level parsing, to more
    #  accurately adjust indents to control visual formatting.
    if {[llength $initstr]==0} { return "" }
    if {[llength $initstr]%2!=0} {
	global outfile errchan
	puts $errchan "ERROR: Init string has an non-even element count."
	puts $outfile "error {INCOMPLETE MIF FILE}"
	puts $errchan "ABORT"
	exit 6
    }
    set marstr [format "%*s" $margin {}]
    set outstr "\n"
    foreach {label value} $initstr {
	append outstr $marstr
	set tmpstr "[list $label] [list $value]"
	# For multi-line values, adjust margin indent on last line.
	regsub -- "(.*\n)\[ \t]*\}" $tmpstr "\\1$marstr\}" tmpstr
	append outstr "$tmpstr\n"
    }
    regsub -all -- "\{\{" $outstr "\{ \{" outstr
    regsub -all -- "\}\}" $outstr "\} \}" outstr
    regsub -all -- "\{(\[^ ])" $outstr "\{ \\1" outstr
    regsub -all -- "(\[^ \n])\}" $outstr "\\1 \}" outstr
    return $outstr
}

proc ReplaceLabel {initstring oldlabel newlabel} {
    # Replaces each appearance of "oldlabel" with "newlabel".
    # Currently the match type is "exact", but this could change
    # if we find a reason to do so.
    set newstring {}
    foreach {label value} $initstring {
	if {[string compare $oldlabel $label]==0} {
	    set label $newlabel
	}
	lappend newstring $label $value
    }
    return $newstring
}

proc RectangularSection {name initstring} {
    global rectangular_section
    set rectangular_section($name) $initstring
}

proc SectionAtlas { atlasname init_string } {
    global rectangular_section atlas_list
    set external_sections {}
    set internal_sections {}
    foreach {label value} $init_string {
	if {[llength $value]==1} {
	    # External section reference.  Oxs_SectionAtlas assigns its
            # own name to external sections, whereas Oxs_MultiAtlas uses
            # the external atlas' own name.
	    set name [lindex $value 0]
	    set istr $rectangular_section($name)
 	    unset rectangular_section($name) ;# Remove from section list
	    set newname "Oxs_BoxAtlas:$label"
	    lappend external_sections \
		    [list Specify $newname "${istr}"]
	    lappend internal_sections $newname
	} else {
	    # Inline section reference.  Use Oxs_SectionAtlas label
	    # as new atlas name.  Assume new section is an 
	    # Oxs_RectangularSection, since that is the only legal
	    # MIF 2.0 section type.
	    set istr [lindex $value 1]
	    lappend internal_sections \
		    [list "Oxs_BoxAtlas:$label" $istr]
	}
    }
    set outstr {}
    foreach block $external_sections {
	append outstr $block "\n\n"
    }

    regsub -- {^Oxs_SectionAtlas} $atlasname "Oxs_MultiAtlas" atlasname
    lappend atlas_list $atlasname
    append outstr "Specify " [list $atlasname] " {\n"
    foreach block $internal_sections {
	set tmpstr [list $block]
	# For multi-line values, adjust margin indent on last line.
	regsub -- "(.*\n)\[ \t]*\}" $tmpstr "\\1  \}" tmpstr
	append outstr "  atlas " $tmpstr "\n"
    }
    append outstr "}\n"
    # Borrow some tricks from MakePrettyInitString
    regsub -all -- "\{\{" $outstr "\{ \{" outstr
    regsub -all -- "\}\}" $outstr "\} \}" outstr
    regsub -all -- "\{(\[^ ])" $outstr "\{ \\1" outstr
    regsub -all -- "(\[^ \n])\}" $outstr "\\1 \}" outstr
    return $outstr
}

proc Exchange6Ngbr { name initstr } {
    set newstr {}
    foreach {lab val} $initstr {
	if {[string compare "A" $lab]==0} {
	    # Input list is a list of triples.
	    # Remove one level.
	    set val "\n   [join $val "\n   "]\n"
	}
	lappend newstr $lab $val
    }
    set outstr "Specify [list $name] \{"
    append outstr "[MakePrettyInitString $newstr 2]\}\n"
    return $outstr
}

proc StandardDriver {name initstr} {
    global misc_key nostagecheckflag
    regsub {^Oxs_StandardDriver} $name "Oxs_TimeDriver" name
    set outstr "Specify [list $name] \{"
    if {[info exists misc_key(basename)]} {
	append outstr "\n  basename [list $misc_key(basename)]"
	unset misc_key(basename)
    }
    if {$nostagecheckflag} {
	append outstr "\n  stage_count_check 0"
	;## stage_count_check new in MIF 2.1.
    }	
    regsub -- "number_of_stages" $initstr "stage_count" initstr
    append outstr "[MakePrettyInitString $initstr 2]\}\n"
    return $outstr
}

set AAS_call_count 0
proc DumpScriptAtlasAdapterScript { chan } {
    global AAS_call_count
    if {$AAS_call_count==0} {
	puts $chan "proc ScriptAtlasAdapterScript { args } \{"
	puts $chan "  return \[expr {\[uplevel 1 \$args\]+1}\]"
	puts $chan "\}\n"
    }
    incr AAS_call_count
}

proc FixupScriptAtlasInitString { initstr } {
    # There are 2 differences between ScriptAtlas scripts
    # in MIF 2.0 and MIF 2.1.  First, the default args in
    # 2.1 is just "relpt", as opposed to "rawpt minpt maxpt"
    # in 2.0.  Secondly, in 2.1 return index 0 is reserved
    # for the "universe" region, and the user-specified
    # regions are number from 1.  In 2.0 the volume outside
    # the atlas is not interpreted as a region, and the
    # user-specified regions are numbered from 0.  So in
    # doing the conversion from 2.0 to 2.1, we need to
    # introduce a patch-up proc that bumps the returned
    # region index up by one.
    set newstr {}
    foreach {lab val} $initstr {
	if {[string compare script $lab]==0} {
	    set val [concat ScriptAtlasAdapterScript $val]
	}
	lappend newstr $lab $val
    }
    set initstr $newstr
    lappend initstr "script_args" [list rawpt minpt maxpt]
    return $initstr
}

proc FixupAtlasFieldInitString { initstr } {
    # Adjusts bracing of "value" value string.
    set newstr {}
    foreach {lab val} $initstr {
	if {[string compare "values" $lab]==0} {
	    # In MIF 2.0 format, each $val item is a region-value
	    # list; in MIF 2.1, the region portion is broken out.
	    # Note too that the import region-value list element
	    # has length 2 for scalar fields, 4 for vector fields.
	    set newval "\n"
	    foreach elt $val {
		append newval "      "
		append newval [list [lindex $elt 0] [lrange $elt 1 end]]
		append newval "\n"
	    }
	    set val $newval
	}
	lappend newstr $lab $val
    }
    return $newstr
}


# Routine to insert bounding box and script_arg info into script
# field object initialization strings.  The first import, "src"
# is used solely for error message reporting.
proc FixupScriptFieldInitString { src initstr } {
    global atlas_list errchan
    if {[llength $atlas_list]<1} {
	global outfile
	puts $errchan \
		"ERROR: No atlas found to use as bounding box for $src"
	puts $outfile "error {INCOMPLETE MIF FILE}"
	puts $errchan "ABORT"
	exit 5
    }
    set atlas [lindex $atlas_list end]
    if {[llength $atlas_list]>1} {
	if {!$quietflag} {
	    puts $errchan \
		  "WARNING: Using atlas $atlas as bounding box for $src"
	    puts $errchan "  This choice is not unique."
	}
    }
    lappend initstr "atlas" $atlas
    lappend initstr "script_args" [list rawpt minpt maxpt]
    return $initstr
}

#####################
# Begin conversion
  
# Drop "Init" from ScalarFieldInit and VectorFieldInit
regsub -all -- {(Oxs_[^ ]+ScalarField)Init} $filestr {\1} filestr
regsub -all -- {(Oxs_[^ ]+VectorField)Init} $filestr {\1} filestr

# Change "full name" (i.e., class name + instance name) references to
# Oxs_SectionAtlas objects to use the new class name Oxs_MultiAtlas.
# Note that after this change, the processing code below needs to use
# the new name "Oxs_MultiAtlas" to process existing Oxs_SectionAtlas
# Specify blocks.
regsub -all -- {Oxs_SectionAtlas} $filestr {Oxs_MultiAtlas} filestr


# Create slave interpreter to evaluate input file in.
if {$unsafeflag} {
    set slave [interp create]
    set errsafeflag {}
} else {
    set slave [interp create -safe]
    set errsafeflag "\nYou may want to try running mifconvert\
	    with the --unsafe option."
}


# Produce self-recording versions of some common interpreter
# state-setting commands in the slave interpreter.  The recordings
# will be later dumped into the MIF output file.
#   Details: The command, e.g., "set", is first renamed in the
# slave to "setORIG".  Then an alias is setup in the slave so that
# "set" in the slave actually runs "setMASTER" in the parent (master)
# interpreter.  "setMASTER" appends the set command to the global
# array cmdlist, and then turns around and calls "setORIG" in the
# slave.  BTW, the path of the slave interpreter is passed to
# the parent-side proc as the first argument.  Since there is only
# one slave, the parent-side proc could just access the global
# variable "slave", but this way is a little nicer if we ever
# decide to reuse this code elsewhere.
#   Commands affected: set, array, append, lappend, proc
foreach cmd [list set array append lappend proc] {
    interp eval $slave rename $cmd ${cmd}ORIG
    interp alias $slave $cmd {} ${cmd}MASTER $slave
}

proc setMASTER {slave varname args} {
    global cmdlist
    if {[llength $args]>0} {
	lappend cmdlist [concat [list set $varname] $args]
    }
    interp eval $slave setORIG [list $varname] $args
}

proc arrayMASTER {slave option arrayName args} {
    global cmdlist
    if {[string compare set $option]==0} {
	lappend cmdlist [concat [list array set $arrayName] $args]
    }
    interp eval $slave arrayORIG [list $option $arrayName] $args
}

proc appendMASTER {slave varname args} {
    global cmdlist
    lappend cmdlist [concat [list append $varname] $args]
    interp eval $slave appendORIG [list $varname] $args
}

proc lappendMASTER {slave varname args} {
    global cmdlist
    lappend cmdlist [concat [list lappend $varname] $args]
    interp eval $slave lappendORIG [list $varname] $args
}

proc procMASTER {slave name arguments body} {
    global cmdlist
    lappend cmdlist [list proc $name $arguments $body]
    interp eval $slave procORIG [list $name $arguments $body]
}

# Add extra commands
proc Specify {name initstr} {
    global cmdlist
    lappend cmdlist [list Specify $name $initstr]
}
interp alias $slave Specify {} Specify

proc Miscellaneous {misclist} {
    global misc_key
    foreach {key value} $misclist {
	set misc_key($key) $value
    }
}
interp alias $slave Miscellaneous {} Miscellaneous

proc ClearSpec {args} {
    global errchan
    puts $errchan "ClearSpec command not implemented by mifconvert"
    puts $errchan "ABORT"
    exit 3
}
interp alias $slave ClearSpec {} ClearSpec

proc Report {args} {
    global errchan
    puts $errchan "Report command not implemented by mifconvert"
    puts $errchan "ABORT"
    exit 3
}
interp alias $slave Report {} Report

# Source filestr
if {[catch {interp eval $slave $filestr} errmsg]} {
    puts $errchan "ERROR evaluating input file $infilename:"
    puts $errchan "--------------------"
    puts $errchan $errmsg
    puts $errchan "--------------------$errsafeflag"
    puts $errchan "ABORT"
    exit 4
}

# Move "min_timestep" and "max_timestep", if any, out
# of driver and place in evolver holding area.
set evolver_extra_bits {}
set cmdindex [lsearch -regexp $cmdlist \
	"^Specify Oxs_StandardDriver(\[ \t\n\]|:)"]
if {$cmdindex<0} {
    puts $errchan "FATAL ERROR: Invalid input file;\
	    no Oxs_StandardDriver Specify block."
    puts $errchan "ABORT"
    exit 4
}
set driverblock [lindex $cmdlist $cmdindex]
set driver_initstr [lindex $driverblock 2]
# Search, store and remove min/max_timestep options from driver_initstr
set dindex [lsearch -regexp $driver_initstr {m(in|ax)_timestep}]
set start_index 0
while {$dindex>=$start_index} {
    if {$dindex%2==0} {
	# timestep request found
	set tmpindex [expr {$dindex+1}]
	lappend evolver_extra_bits [lindex $driver_initstr $dindex] \
		[lindex $driver_initstr $tmpindex]
	set driver_initstr [lreplace $driver_initstr $dindex $tmpindex]
	incr dindex -1
    }
    set start_index [expr {$dindex+1}]
    set str [lrange $driver_initstr $start_index end]
    set dindex [expr {$start_index \
	    + [lsearch -regexp $str "m(in|ax)_timestep"]}]
}
# Stick possibly modified driver_initstr back into cmdlist.
set driverblock [lreplace $driverblock 2 2 $driver_initstr]
set cmdlist [lreplace $cmdlist $cmdindex $cmdindex $driverblock]

###########################
# Open output file and dump header
if {[string compare "<stdout>" $outfilename]==0} {
    set outfile stdout
} else {
    set outfile [open $outfilename "w"]
}
puts $outfile "# MIF 2.1"
puts $outfile "# This file converted from MIF 2.0 format by $progvers"
puts $outfile "# Original file: $fullinname"
puts $outfile "#\n"

# Write random seed request, if any
if {[info exists misc_key(seed)]} {
    if {$misc_key(seed)==0} {
	# In MIF 2.0, a seed value of 0 means to take a seed
	# from the system clock.  In MIF 2.1 this functionality
	# is produced by calling RandomSeed with no parameter.
	puts $outfile "RandomSeed\n"
    } else {
	puts $outfile "RandomSeed $misc_key(seed)\n"
    }
    unset misc_key(seed)
}

# List of known atlases.  This is setup in the SectionAtlas proc,
# and used in FixupScriptFieldInitString proc.
set atlas_list {}

# Loop through cmdlist
foreach cmd $cmdlist {
    if {[string compare "Specify" [lindex $cmd 0]]!=0} {
	# Self recorded command
	puts $outfile "$cmd\n"
    } else {
	# Specify block
	set name [lindex $cmd 1]
	set initstr [lindex $cmd 2]

	# Substitutions in initstr
	set newinitstr {}
	foreach {label value} $initstr {
	    # Change "ignore" with "comment"
	    if {[string compare "ignore" $label]==0} {
		set label "comment"
	    }
	    # Clean up embedded script field objects
	    if {[llength $value]==2} {
		set type [lindex $value 0]
		set valstr [lindex $value 1]
		if {[regexp -- {Oxs_Script(Scalar|Vector)Field($|:)} $type]} {
		    set valstr [FixupScriptFieldInitString \
			    "$name/$label/$type" $valstr]
		    set valstr "[MakePrettyInitString $valstr 4]  "
		    set value [list $type $valstr]
		} elseif {[regexp -- {Oxs_Atlas(Scalar|Vector)Field($|:)} \
			$type]} {
		    set valstr [FixupAtlasFieldInitString $valstr]
		    set valstr "[MakePrettyInitString $valstr 4]  "
		    set value [list $type $valstr]
		}
	    }
	    lappend newinitstr $label $value
	}
	set initstr $newinitstr
	unset newinitstr

	# Handle object specific modifications
	switch -glob -- $name {
	    Oxs_AtlasScalarField* -
	    Oxs_AtlasVectorField* {
		set initstr [FixupAtlasFieldInitString $initstr]
		puts -nonewline $outfile "Specify [list $name] \{"
		puts $outfile "[MakePrettyInitString $initstr 2]\}\n"
	    }
	    Oxs_ScriptScalarField* -
	    Oxs_ScriptVectorField* {
		set initstr [FixupScriptFieldInitString $name $initstr]
		puts -nonewline $outfile "Specify [list $name] \{"
		puts $outfile "[MakePrettyInitString $initstr 2]\}\n"
	    }
	    Oxs_RectangularSection* {
		RectangularSection $name $initstr
	    }
	    Oxs_ScriptAtlas* {
		DumpScriptAtlasAdapterScript $outfile
		set initstr [FixupScriptAtlasInitString $initstr]
		puts -nonewline $outfile "Specify [list $name] \{"
		puts $outfile "[MakePrettyInitString $initstr 2]\}\n"
	    }
           Oxs_MultiAtlas* {
                # --- Oxs_SectionAtlas blocks ---
                # In MIF 2.0 files, the only atlas type is
                # Oxs_SectionAtlas There is code above that does a
                # string replace on all occurrences of
                # "Oxs_SectionAtlas", changing them to "Oxs_MultiAtlas".
                # This is done to catch any references to an atlas that
                # uses the full name (class + instance name), but as a
                # side effect this also changes the label in the Specify
                # block.  So we have to match here against
                # "Oxs_MultiAtlas" instead of "Oxs_SectionAtlas"
		puts $outfile [SectionAtlas $name $initstr]
	    }
	    Oxs_Exchange6Ngbr* {
		puts $outfile [Exchange6Ngbr $name $initstr]
	    }
	    Oxs_UZeeman* {
		set initstr [ReplaceLabel $initstr Hscale multiplier]
		puts -nonewline $outfile "Specify [list $name] \{"
		puts $outfile "[MakePrettyInitString $initstr 2]\}\n"
	    }
	    Oxs_EulerEvolve* {
		set initstr [concat $initstr $evolver_extra_bits]
		set initstr [ReplaceLabel $initstr gamma gamma_LL]
		puts -nonewline $outfile "Specify [list $name] \{"
		puts $outfile "[MakePrettyInitString $initstr 2]\}\n"
	    }
	    Oxs_StandardDriver* {
		puts $outfile [StandardDriver $name $initstr]
	    }
	    default {
		puts -nonewline $outfile "Specify [list $name] \{"
		puts $outfile "[MakePrettyInitString $initstr 2]\}\n"
	    }
	}
    }
}

# Dump left-over misc_keys
set misc_key_list [array names misc_key]
if {[llength $misc_key_list]>0} {
    puts $outfile "Ignore \{"
    puts $outfile " WARNING: Unrecognized Miscellaneous block values ---"
    foreach key $misc_key_list {
	puts $outfile "  [list $key] [list $misc_key($key)]"
    }
    puts $outfile "\}\n"
}
close $outfile
if {!$quietflag} {
    puts $infochan "TRANSLATED [list $infilename] -> [list $outfilename]"
    puts $infochan " Please check [list $outfilename]\
	    for correctness and additional edits."
    foreach key $misc_key_list {
	puts $infochan \
		" -ignored Miscellaneous input-> $key: $misc_key($key)"
    }
}

######################################################################
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! #
# END OF MIF 2.0 -> MIF 2.1 CONVERTER BLOCK.                         #
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! #
######################################################################
} else {
    puts $errchan "PROGRAM ERROR:\
	    Unrecognized input file type: $input_type"
    close $infile
    puts $errchan "ABORT"
    exit 99
}
