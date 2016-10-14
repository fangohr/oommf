# FILE: bug75.tcl
#
#	Patches for Tcl 7.5 / Tk 4.1
#
# Last modified on: $Date: 2015/11/24 23:19:25 $
# Last modified by: $Author: donahue $
#
# This file contains Tcl code which when sourced in a Tcl 7.5 interpreter
# will patch bugs in Tcl 7.5 fixed in later versions of Tcl.  It is not
# meant to be complete.  Bugs are patched here only as we discover the need
# to do so in writing other OOMMF software.

Oc_Log Log "Patching Tcl 7.5 bugs..." status

if {[string match unix $tcl_platform(platform)]} {

    # The subcommands copy, delete, rename, and mkdir were added to
    # the Tcl command 'file' in Tcl version 7.6.  The following command
    # approximates them on Unix platforms.  It may not agree with
    # the Tcl 7.6+ command 'file' in all of its functionality (notably
    # the way it reports errors).  Further refinements should be made as
    # needed.
    rename file Tcl7.5_file
    proc file {option args} {
        switch -glob -- $option {
            c* {
		if {[string first $option copy] != 0} {
                    return [uplevel [list Tcl7.5_file $option] $args]
		}
                # Translate -force into -f
                if {[string match -force [lindex $args 0]]} {
                    set args [lreplace $args 0 0 -f]
                }
                uplevel exec cp $args
            }
            de* {
		if {[string first $option delete] != 0} {
                    return [uplevel [list Tcl7.5_file $option] $args]
		}
                if {[string match -force [lindex $args 0]]} {
                    set args [lreplace $args 0 0 -f]
                }
                catch {uplevel exec rm $args}
            }
            mk* {
		if {[string first $option mkdir] != 0} {
                    return [uplevel [list Tcl7.5_file $option] $args]
		}
                uplevel exec mkdir $args
            }
            ren* {
		if {[string first $option rename] != 0} {
                    return [uplevel [list Tcl7.5_file $option] $args]
		}
                if {[string match -force [lindex $args 0]]} {
                    set args [lreplace $args 0 0 -f]
                }
                uplevel exec mv $args
            }
            default {
                uplevel [list Tcl7.5_file $option] $args
            }
        }
    }

    # Use the auto_execok implementation from Tcl 7.6 and later.
    # Only need the Unix version.
    proc auto_execok name {
	global auto_execs env

	if {[info exists auto_execs($name)]} {
            return $auto_execs($name)
        }
        set auto_execs($name) ""
        if {[llength [file split $name]] != 1} {
            if {[file executable $name] && ![file isdirectory $name]} {
                set auto_execs($name) [list $name]
            }
            return $auto_execs($name)
        }
        foreach dir [split $env(PATH) :] {
            if {$dir == ""} {
                set dir .
            }
            set file [file join $dir $name]
            if {[file executable $file] && ![file isdirectory $file]} {
                set auto_execs($name) [list $file]
                return $auto_execs($name)
            }
        }
        return ""
    }

    # The [unknown] implementation of Tcl 7.5 calls [auto_execok]. 
    # Since we just changed the latter (above), we need to change
    # the former too.  Use [unknown] from Tcl 7.6:
    proc unknown args {
        global auto_noexec auto_noload env unknown_pending tcl_interactive
        global errorCode errorInfo

        # Save the values of errorCode and errorInfo variables, since they
        # may get modified if caught errors occur below.  The variables will
        # be restored just before re-executing the missing command.

        set savedErrorCode $errorCode
        set savedErrorInfo $errorInfo
        set name [lindex $args 0]
        if ![info exists auto_noload] {
            #
            # Make sure we're not trying to load the same proc twice.
            #
            if [info exists unknown_pending($name)] {
                return -code error "self-referential recursion\
			in \"unknown\" for command \"$name\"";
            }
            set unknown_pending($name) pending;
            set ret [catch {auto_load $name} msg]
            unset unknown_pending($name);
            if {$ret != 0} {
                return -code $ret -errorcode $errorCode \
                    "error while autoloading \"$name\": $msg"
            }
            if ![array size unknown_pending] {
                unset unknown_pending
            }
            if $msg {
                set errorCode $savedErrorCode
                set errorInfo $savedErrorInfo
                set code [catch {uplevel $args} msg]
                if {$code ==  1} {
                    #
                    # Strip the last five lines off the error stack (they're
                    # from the "uplevel" command).
                    #

                    set new [split $errorInfo \n]
                    set new [join [lrange $new 0 [expr [llength $new] - 6]] \n]
                    return -code error -errorcode $errorCode \
                            -errorinfo $new $msg
                } else {
                    return -code $code $msg
                }
            }
        }
        if {([info level] == 1) && ([info script] == "") \
                && [info exists tcl_interactive] && $tcl_interactive} {
            if ![info exists auto_noexec] {
                set new [auto_execok $name]
                if {$new != ""} {
                    set errorCode $savedErrorCode
                    set errorInfo $savedErrorInfo
                    return [uplevel exec >&@stdout <@stdin [list $new] \
			    [lrange $args 1 end]]
                }
            }
            set errorCode $savedErrorCode
            set errorInfo $savedErrorInfo
            if {$name == "!!"} {
                return [uplevel {history redo}]
            }
            if [regexp {^!(.+)$} $name dummy event] {
                return [uplevel [list history redo $event]]
            }
            if [regexp {^\^([^^]*)\^([^^]*)\^?$} $name dummy old new] {
                return [uplevel [list history substitute $old $new]]
            }
            set cmds [info commands $name*]
            if {[llength $cmds] == 1} {
                return [uplevel [lreplace $args 0 0 $cmds]]
            }
            if {[llength $cmds] != 0} {
                if {$name == ""} {
                    return -code error "empty command name \"\""
                } else {
                    return -code error \
                            "ambiguous command name \"$name\": [lsort $cmds]"
                }
            }
        }
        return -code error "invalid command name \"$name\""
    }

    # Changes to [grid] in Tk 4.2:
    #   * [grid columnconfigure] and [grid rowconfigure] added a
    #	  -pad option and a zero argument form
    #   * [grid bbox] now takes 0 to 2 ?column row? pairs;
    #	  used to take exactly 1.  Compatibility not implemented,
    #	  but more instructive error message provided.
    #   * [grid remove] is a new subcommand that removes a widget
    #	  from [grid] management, but remembers its configuration
    #	  in case [grid] gets control of it again later.  Implemented
    #	  in terms of [grid forget] and with use of a global array to
    #	  remember configurations.
    #	* [grid $slave ...] started recognizing x and ^ as the first
    #	  $slave in Tk 4.2.  Strangely, [grid configure] still does not
    # 	  allow this (See Bug 418644) so the (partial) compatibility
    #	  workaround for this is rather complicated.
    if {[llength [info commands grid]]} {
        array set Tk4.1_grid [list "" ""]
        unset Tk4.1_grid()
        rename grid Tk4.1_grid
	# Parse the arguments to [grid configure] to determine where
	# $slave arguments end and $option arguments begin
        proc Tk4.1_gridParseSlaves {args} {
            set firstOption 0
            foreach arg $args {
                switch -glob -- $arg {
                    -  -
                    ^  -
                    x  -
		    .* {
                        incr firstOption
                    }
                    default {
                        break
                    }
                }
            }
            set slaves [lrange $args 0 [expr {$firstOption-1}]]
            set options [lrange $args $firstOption end]
	    return [list $slaves $options]
        }
        proc grid {option args} {
            global Tk4.1_grid
            set myName [lindex [info level 0] 0]
            if {[string match .* $option]} {
                # Supply optional subcommand 'configure' and re-call
                return [uplevel [list $myName configure $option] $args]
            }
            if {[llength $args] == 0} {
                return -code error "wrong # args:\
                        should be \"$myName option arg ?arg ...?\""
            }
            if {[string match {[x^]} $option]} {
                foreach {slvs opts} [eval \
			[list Tk4.1_gridParseSlaves $option] $args] {break}
                if {[llength $opts] % 2} {
                    return -code error "extra option \"[lindex $opts end]\"\
                            (option with no value?)"
                }
                array set optArr $opts
                set numSlaves [llength $slvs]
                set defCol 0
                for {set i 0} {$i < $numSlaves} {incr i} {
                    set slv [lindex $slvs $i]
                    if {[string match {[x^]} $slv]} {
                        incr defCol
                        if {[string match ^ $slv]} {
                            # Fill in code here to expand the slave in
                            # the row above, if it ever turns out we need
                            # that feature.
			    error "Sorry, '^' not implemented"
                        }
                    } elseif {[string match .* $slv]} {
                        # Found a real window
                        if {[info exists optArr(-column)]} {
                            # Column explicitly given; relative placement
                            # stuff has no effect.  Call grid configure
                            # with first real window moved up to front
                            return [uplevel Tk4.1_grid configure \
                                    [lrange $slvs $i end] $opts]
                        }
                        # No column explicitly given; honor ^ and x
                        # First collect all trailing -
                        set rs [list $slv]
                        incr i
                        while {[string match - [lindex $slvs $i]]} {
                            lappend rs -
                            incr i
                        }
                        incr i -1
                        # grid the real window and its trailing -
                        uplevel Tk4.1_grid configure $rs [array get optArr] \
				-column $defCol
                        if {![info exists optArr(-row)]} {
                            # Capture the row to apply to other slaves
                            array set foo [Tk4.1_grid info $slv]
                            set optArr(-row) $foo(-row)
                        }
                        incr defCol [llength $rs]
                    } else {
                        return -code error "Must specify window\
                                before shortcut '-'."
                    }
                }
                return
            }
            switch -glob -- $option {
                b* {
		    if {[string first $option bbox] != 0} {
			return [uplevel [list Tk4.1_grid $option] $args]
		    }
                    switch -exact -- [llength $args] {
                        1 {
                           return -code error "\[$myName bbox\] syntax not\
                                   supported; Upgrade to Tk 4.2 or higher"
                        }
                        5 {
                           return -code error "\[$myName bbox \$column \$row\
				    \$column \$row\] syntax not supported;\
				    Upgrade to Tk 4.2 or higher"
                        }
                        default {
                            uplevel [list Tk4.1_grid bbox] $args
                        }
                    }
                }
                con* {
		    if {[string first $option configure] != 0} {
			return [uplevel [list Tk4.1_grid $option] $args]
		    }
                    foreach {slaves opts} \
			    [eval Tk4.1_gridParseSlaves $args] {break}
                    # First restore saved options from prior 'remove' if any
                    foreach slave $slaves {
                        if {[info exists Tk4.1_grid($slave)]} { 
                            uplevel [list Tk4.1_grid configure $slave] \
                                    [set Tk4.1_grid($slave)]
                            unset Tk4.1_grid($slave)
                        }
                    }
                    uplevel Tk4.1_grid configure $slaves $opts
                }
                f* {
		    if {[string first $option forget] != 0} {
			return [uplevel [list Tk4.1_grid $option] $args]
		    }
                    foreach slave $args {
                        catch {unset Tk4.1_grid($slave)}
                    }
                    uplevel Tk4.1_grid forget $args
                }
                re* {
		    if {[string first $option remove] != 0} {
			return [uplevel [list Tk4.1_grid $option] $args]
		    }
                    foreach slave $args {
                        set slaveConf [Tk4.1_grid info $slave]
                        if {[llength $slaveConf]} {
                            array set Tk4.1_grid [list $slave $slaveConf]
                        }
                        uplevel [list Tk4.1_grid forget $slave]
                    }
                }
                ro*  -
                col* {
		    if {(([string first $option rowconfigure] != 0)
			    && ([string first $option columnconfigure] != 0))
			    || ([llength $args] < 2)} {
			return [uplevel [list Tk4.1_grid $option] $args]
		    }
                    set master [lindex $args 0]
                    set index [lindex $args 1]
                    set options [lrange $args 2 end]
                    if {[llength $options] == 0} {
                        set retList [list -minsize]
                        lappend retList \
                                [Tk4.1_grid $option $master $index -minsize]
                        lappend retList -pad 0 -weight
                        lappend retList \
                                [Tk4.1_grid $option $master $index -weight]
                        return $retList
                    }
                    if {[llength $options] == 1} {
                        if {[string match -pad [lindex $options 0]]} {
                            return 0
                        } else {
                            uplevel [list Tk4.1_grid $option] $args
                        }
                    } elseif {[llength $options] % 2} {
			uplevel [list Tk4.1_grid $option] $args
                    } else {
			# Don't use an array to process options; that
			# will not preserve their order.
                        set passOpts {}
                        foreach {opt val} $options {
                            if {[string compare -pad $opt]} {
                                lappend passOpts $opt $val
                            }
                        }       
                        if {[llength $passOpts]} {
                            uplevel [list Tk4.1_grid $option $master $index] \
                                    $passOpts
                        }
                    }
                }
                default {
                    uplevel [list Tk4.1_grid $option] $args
                }
	    }
	}
    }

    if {[llength [info commands event]]==0} {
       # The Tk "event" command premiered in Tk 4.2.  Provide a NOP
       # version here so old code can hobble along w/o it.
       proc event args {}
    }
} else {
    Oc_Log Log "OOMMF software does not support 
Tcl version 7.5 on non-Unix platforms.  
Please upgrade your Tcl installation." panic 
    exit
}
    
# Continue with bug patches for the next Tcl version
source [file join [file dirname [info script]] bug76.tcl]

