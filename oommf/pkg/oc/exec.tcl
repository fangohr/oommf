# FILE: exec.tcl
#
# Last modified on: $Date: 2007/03/21 01:02:40 $
# Last modified by: $Author: donahue $
#

Oc_Class Oc_Exec {

    private common outChannel ""
    private common errChannel stderr

    Constructor {args} {
        return -code error "Instances of $class are not allowed"
    }

    proc SetOutChannel {o} {
	set outChannel $o
    }

    proc SetErrChannel {e} {
	set errChannel $e
    }

# May eventually want better processing of child's stderr than just
# passing it through to our stderr.  Making it work cross-platform
# and cross-Tcl-release, may be tricky though.
#
# Issues:
#	Want warnings and info messages written to stderr to be
#	displayed to user immediately.
#
#	However, it would be nice to catch panic messages, and
#	present them as the error message returned by the child
#	instead of the uninformative "child process exited abnormally"
#	
    proc Foreground {args} {
        if {![llength $args]} {return}
	set redir {<@ stdin >@}
	if {[string length $outChannel]} {
	    puts $outChannel [join $args]
	    lappend redir $outChannel
	} else {
	    lappend redir stdout
	}
	lappend redir 2>@ $errChannel
	exec {*}$redir {*}$args
    }
}

