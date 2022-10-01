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

# Sort list of files by increasing date, for use by slightly unsafe
# interpreters.  All files must exist.
proc Oxs_DateSort { files } {
   if {[llength $files]<2} { return $files }
   set augfiles [list]
   foreach f $files {
      if {[catch {file mtime $f} modtime]} {
         return -code error "bad filename \"$f\": $modtime"
      }
      lappend augfiles [list $modtime $f]
   }
   set result [list]
   foreach pair [lsort -dictionary $augfiles] {
      # With dictionary sort, if two files have the same timestamp
      # then a secondary sort by name occurs, except that files with
      # embedded spaces will sort using space protection characters
      # (probably curly braces).
      lappend result [lindex $pair 1]
   }
   return $result
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

# Routine to flush pending log messages. Used for cleanup on problem exit.
proc Oxs_FlushLog {} {
   foreach id [after info] {
      foreach {script type} [after info $id] break
      if {[string match idle $type] && [string match *Oc_Log* $script]} {
         uplevel #0 $script
      }
   }
}

# Routine to flush pending data messages. Used for cleanup on problem exit.
proc Oxs_FlushData {} {
   global OxsFlushData_open_connections
   set OxsFlushData_open_connections [dict create]
   # Explicitly close each data destination.  Note that at the very
   # bottom a socket close occurs, which is a blocking operation.
   foreach elt [Net_Thread Instances] {
      if {![catch {$elt Protocol} proto]} {
         # If thread is in an uninitialized (partially shutdown)
         # state, then Protocol might not be set.
         # Note: AFAIK there isn't any reason for Oxs to hold a
         #       Net_Thread on the client side to a GeneralInterface
         #       server, but it shouldn't hurt to handle that case
         #       either.
         switch -exact $proto {
            GeneralInterface -
            DataTable -
            vectorField -
            scalarField {
               dict set OxsFlushData_open_connections \
                  $elt [list $proto [$elt Id]]
               Oc_EventHandler New _ $elt DeleteEnd \
                   "dict unset OxsFlushData_open_connections $elt" -oneshot 1
               $elt Send bye
            }
            default {
               Oc_Log Log "Unrecognized Net_Thread protocol \"$proto\"\
                         will not be flushed" warning
            }
         }
      }
   }
   # If desired, the following stanza can be moved to the
   # very end of the shutdown process.
   dict set OxsFlushData_open_connections TIMEOUT 0
   while {[dict size $OxsFlushData_open_connections] > 1} {
      set failsafe [after 120000 \
         dict set OxsFlushData_open_connections TIMEOUT 1]
      vwait OxsFlushData_open_connections
      after cancel $failsafe
      if {[dict get $OxsFlushData_open_connections TIMEOUT]} {
         dict unset OxsFlushData_open_connections TIMEOUT
         set msg "WARNING: Timeout waiting to flush data\
                  to the following channel(s):\n"
         foreach key [dict keys $OxsFlushData_open_connections] {
            lassign [dict get $OxsFlushData_open_connections $key] proto id
            append msg " --> $id, protocol=$proto\n"
         }
         Oc_Log Log $msg warning
      }
   }
}
