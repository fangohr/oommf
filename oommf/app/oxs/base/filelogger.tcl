
Oc_EventHandler Bindtags FileLogger FileLogger
Oc_Class FileLogger {
    common filename ""

    proc Log {msg type src} {
       global errorInfo errorCode
       # Note: In Tcl 8.5.7 (others?), the clock format command
       # below may reset errorInfo.  So make a copy.
       set ei $errorInfo
       set ec $errorCode
       regsub -- "\n+\$" $msg {} msg ;# Strip trailing newlines, if any.

       set instanceinfo [Oc_Main GetInstanceName]
       set iipid "[lindex $instanceinfo 0]<[pid]>"
       set iiname [lrange $instanceinfo 1 end]
       set st [string trim "$src $type"]

       if {![string match infolog $type]} {
          catch {
             set text "\n----------- "
             append text [clock format [clock seconds] -format "%Y-%b-%d %T"]
             append text "\n$iipid $st: $msg"
             if {![string match info* $type]} {
                append text "\n-----------"
                append text "\nStack:\n$ei\n-----------\n"
                append text "Additional info: $ec"
             }
             puts stderr $text
             flush stderr
          }
       }
       if {[string match "" $filename]} {return}
       if {[catch {open $filename a} chanid]} {
          puts stderr "Error in FileLogger: unable to\
                        open error log file $filename: $chanid"
       } else {
          catch {
             puts $chanid "\n\[$iipid [clock format [clock seconds] \
			-format %H:%M:%S\ %Y-%m-%d]\] $iiname $st:\n$msg"
             if {![string match info* $type]} {
                set text "-----------\n"
                append text "Stack:\n$ei\n-----------\n"
                append text "Additional info: $ec"
                puts $chanid $text
             }
          }
          close $chanid
       }
    }

    proc SetFile {name} {
	# Use name == "" to disable file logging.  This proc will
	# create the file if it does not already exist.
	# NOTE: Might want to change this in the future to use a
	#  filename stack with PushFile and PopFile member functions,
	#  to allow code to temporarily change (and unchange) logging
	#  file.
	if {[catch {open $name a+} chanid]} {
	    if {0} {
		# Disable error message for beta release.  This
		# should be tied to a global "debug" level.
		Oc_Log Log "Error in FileReport: unable to\
			open error log file $name: $chanid" error
		# Should we set filename to "", or leave it
		# pointing to wherever it was last?  Changing it
		# is arguably correct, but leaving it alone is
		# probably safer and more useful.
	    }
	} else {
	    close $chanid
	    set filename $name
	}
    }

    proc GetFile {} {
	return $filename
    }

    Constructor {args} {
	return -error "$class does not create instances"
    }
}

