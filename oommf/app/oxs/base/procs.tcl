# FILE: procs.tcl
#
# Misc. Tcl support code for Oxs
#

# Proc to read a file.  This is a wrapper for use by safe
# interpreters, and also provides image conversion facility.
# The second argument should be one of "binary", "auto" or
# "image".  The first two govern the translation mode of the
# opened file.  Selecting "image" causes the input to be
# converted to mimic the body of a PPM P3 text file---the first
# 3 whitespace separated values are width, height, and maxvalue.
# After this is an array or pixels, r g b.  The difference
# compared to a straight P3 file is that the leading "P3" is
# removed, and there are no comments.  The output can be treated
# as a list and used directly.
#
# The return value is a string representing the read data.
#
proc Oxs_ReadFile { file {translation auto} } {
    if {[string match image $translation]} {
       # Read and convert to simple integer list representation
       return [Nb_ImgObj $file]
    } elseif {[string match floatimage $translation]} {
       # Read and convert to simple floating point list representation
       return [Nb_ImgObj -double $file]
    } else {
	# Standard file read
	set chanid [open $file r]
	if {[string match binary $translation]} {
	    fconfigure $chanid -translation binary
	}
	fconfigure $chanid -buffering full -buffersize 50000
	set filestr [read $chanid]
	close $chanid
	return $filestr
    }
}

# Restricted glob, for use by slightly unsafe interpreters.
# This version does not allow access to directories other
# than the current working directory.
proc Oxs_RGlob { args } {
    if {[llength $args]<1} {
	return -code error "wrong # args: should be\
		\"Glob ?switches? name ?name ...?\""
    }
    set types {}
    for {set index 0} {$index<[llength $args]} {incr index} {
	set option [lindex $args $index]
	if {![string match -* $option]} { break }
	if {[string match -- $option]} {
	    incr index
	    break
	}
	switch -exact -- $option {
	    -types {
		if {$index+1>=[llength $args]} {
		    return -code error "missing argument to \"-types\""
		}
		incr index
		set types [lindex $args $index]
	    }
	    default {
		return -code error "bad option \"$option\":\
			must be -types or --"
	    }
	}
    }

    set matchlist {}
    set filelist [glob -nocomplain -types $types *]
    foreach file $filelist {
	foreach pat [lrange $args $index end] {
	    if {[string match $pat $file]} {
		lappend matchlist $file
		break
	    }
	}
    }
    return $matchlist
}

# Checkpoint timestamp
proc Oxs_CheckpointTimestamp {} {
   if {![catch {clock milliseconds} now]} {
      set chktime [expr {($now - int(round(1000*[Oxs_GetCheckpointAge]))+500)/1000}]
   } else {
      # For pre-8.5 Tcl
      set now [clock seconds]
      set chktime [expr {$now - int(round([Oxs_GetCheckpointAge]))}]
   }
   return [clock format $chktime]
}
