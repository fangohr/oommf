# FILE: exec.tcl
#
# Last modified on: $Date: 2007/03/21 01:02:40 $
# Last modified by: $Author: donahue $
#

Oc_Class Oc_Exec {

    private common outChannel ""
    private common errChannel stderr
    private array common done

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
	if {[string length $outChannel]} {
	    puts $outChannel [join $args]
	}
        foreach word $args {
            if {[set stdoutRedirected [string match >* $word]]} {break}
        }
        if {1 || $stdoutRedirected} {
	    if {[string length $outChannel]} {
		set cmd [list exec <@ stdin >@ $outChannel 2>@ $errChannel]
	    } else {
		set cmd [list exec <@ stdin >@ stdout 2>@ $errChannel]
	    }
            if {[catch {eval $cmd $args} res]} {
		global errorCode
		error $res $res $errorCode
            }
        } else {
	    # NOTE: This branch disabled 29-Mar-2000 because of weirdness
	    #  with processes being unable to exit on Windows (which we
	    #  were not able to fully explain).
	    # NOTE: We do not use [exec >@stdout] here because that would
	    # block, not allowing other events to be handled.
	    # As written, [Oc_Exec Foreground] blocks, but it also
	    # processes events because it contains a [vwait].
	    #
	    # Having decided to use fileevents to trannsfer the child's
	    # stdout to out stdout, [fcopy] is the best choice, if it
	    # is available.  Tcl provides it in releases 8.x, and
	    # Oc provides a replacement for Tcl 7.x, which has rather
	    # poor performance, but is functional.
	    #
            set f [open [concat | <@ stdin 2>@ stderr $args] r]
	    set stdoutconfig [fconfigure stdout]
	    # Copy the data as soon as the child provides it, and copy
	    # it in a "binary-safe" manner.
	    fconfigure $f -buffering none -eofchar {} -translation binary
	    fconfigure stdout -buffering none -eofchar {} -translation binary
	    fcopy $f stdout -command [list $class Quit $f]

	    # Must check the existence of done($f) in case the fcopy above
	    # has already called the Quit method.  Observed that problem
	    # on Windows.   When that happens, the blocking appears to
	    # shift to the [close] command below.
	    if {![info exists done($f)]} {
		# catch below to cover transient "would wait forever" problems
		catch {vwait [$class GlobalName done]($f)}
	    }
	    unset done($f)
	    eval fconfigure stdout $stdoutconfig
            if {[catch {close $f} res]} {
		global errorCode
		if {[string match CHILD* [lindex $errorCode 0]]} {
		    append res ": [lindex $errorCode 3]"
		}
		error $res $res $errorCode
            }
        }
    }

    callback proc Quit {f args} {
	set done($f) 1
    }

}

