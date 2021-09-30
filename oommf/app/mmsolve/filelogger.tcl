
Oc_Class FileLogger {
    common filename ""

    proc Log {msg type src} {
	catch {
	    # Use default message reporter as well
	    Oc_DefaultLogMessage $msg $type $src
	}
	if {[string match "" $filename]} {return}
	if {[catch {open $filename a} chanid]} {
	    puts stderr "Error in FileReport: unable to\
		    open error log file $filename: $chanid"
	} else {
	    set instanceinfo [Oc_Main GetInstanceName]
            set iipid "[lindex $instanceinfo 0]<[pid]>"
            set iiver [lrange $instanceinfo 1 end]
            catch {
		puts $chanid "\[$iipid [clock format [clock seconds]\
			-format %H:%M:%S\ %Y-%m-%d]\] $iiver $src $type:\
			$msg"
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

