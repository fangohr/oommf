# FILE: log.tcl
#
#	General logging facility.
#
# Last modified on: $Date: 2015/09/30 04:26:33 $
# Last modified by: $Author: donahue $
#
# This file defines the Tcl command Oc_Log for directing messages from
# multiple sources and in multiple categories to multiple handlers for
# recording or display.
#
# This facility was inspired by a similar facility in Pool, a library
# of Tcl routines by Andreas Kupries <a.kupries@westend.com> which is
# available at <URL:http://www.westend.com/%7Ekupries/doc/pool/index.htm>.
#
# DOME: Consider procs for deleting Types and Sources.

proc Oc_StderrLogMessage {msg type src} {
   global errorInfo errorCode
   set ei $errorInfo
   set ec $errorCode
   set maxLines 20
   set mintext "<[pid]> [Oc_Main GetInstanceName] $src $type:\n$msg"
   set text $mintext
   if {[string compare $type error] == 0} {
      append text "\n----------- "
      append text [clock format [clock seconds] -format "%Y-%b-%d %T"]
      append text "\nStack:\n$ei\n-----------\n"
      append text "Additional info: $ec"
   }
   set newtext [join [lrange [split $text \n] 0 $maxLines] \n]
   if {[string compare $mintext $newtext] > 0} {
      # $newtext is truncation of $mintext
      # Force display of all of $mintext
      set newtext $mintext
   } elseif {[string compare $text $newtext] != 0} {
      append newtext "\n(message truncated)"
   }
   # We used to check for use of a Tk console and call
   # [tcl_puts] instead of [puts] here in that case, so
   # all messages went to the real stderr.  For some time
   # now, though, use of the Tk console has implied that
   # all the real standard channels are redirected to
   # null channels, so that no longer works.  
   #
   # Instead, we use a plain [puts stderr], which when the
   # Tk console is active will print the messages into that
   # console in red.
   puts stderr $newtext
}

proc Oc_ForceStderrDefaultMessage {} {
   proc Oc_DefaultMessage {msg type src} {
      Oc_StderrLogMessage $msg $type $src
   }
}

proc Oc_ForceNoTkDefaultMessage {} {
    global tcl_platform
    if {[string match windows $tcl_platform(platform)] \
            && [llength [info commands Oc_WindowsMessageBox]]} {
        proc Oc_DefaultMessage {msg type src} {
            global errorInfo errorCode
	    set ei $errorInfo
	    set ec $errorCode
            set maxLines 40
           set text "<[pid]> [Oc_Main GetInstanceName] $src $type:\n$msg"
	    if {[string compare $type error] == 0} {
                append text "\n----------- "
		append text [clock format [clock seconds] -format "%Y-%b-%d %T"]
		append text "\nStack:\n$ei\n-----------\n"
                append text "Additional info: $ec"
	    }
            set newtext [join [lrange [split $text \n] 0 $maxLines] \n]
            if {[string compare $text $newtext] != 0} {
                append newtext "\n(message truncated)"
            }
            Oc_WindowsMessageBox $newtext
        }
    } else {
	Oc_ForceStderrDefaultMessage
    }
}

Oc_Class Oc_Log {
    array common db {, ""}

    proc AddType {type} {db} {
        if {[regexp , $type]} {
            return -code error \
                    "The character ',' is not allowed in a message type."
        }
        if {![info exists db(,$type)]} {
            foreach _ [array names db *,] {
                regexp ^(.*),$ $_ _ src
                array set db [list $src,$type $db($src,)]
            }
        }
    }

    proc Types {} {
	set result [list]
	foreach _ [array names db ,*] {
	    lappend result [string trimleft $_ ,]
	}
	return $result
    }

    proc AddSource {src} {db} {
        if {[regexp , $src]} {
            return -code error \
                    "The character ',' is not allowed in a message source."
        }
        if {![info exists db($src,)]} {
            foreach _ [array names db ,*] {
                regexp ^,(.*)$ $_ _ type 
                array set db [list $src,$type $db(,$type)]
            }
        }
    }

    proc Sources {} {
	set result [list]
	foreach _ [array names db *,] {
	    lappend result [string trimright $_ ,]
	}
	return $result
    }

    proc SetLogHandler {hdlr type {src ""}} {db} {
        if {![info exists db($src,$type)]} {
            return -code error "Unknown (source,type) pair: ($src,$type)"
        }
        set db($src,$type) $hdlr
    }

    proc GetLogHandler {type {src ""}} {db} {
        if {![info exists db($src,$type)]} {
            return -code error "Unknown (source,type) pair: ($src,$type)"
        }
        return $db($src,$type)
    }

    proc Log {msg {type ""} {src ""}} {db} {
	# prevent infinite loops when Oc_EventHandlers log messages
	if {[string compare Oc_EventHandler $src]} {
	    # Event generation is more extensible logging mechanism.
	    # Interested loggers can come and go by creating/destroying
	    # handlers.
	    Oc_EventHandler Generate $class Log -src $src -type $type -msg $msg
	}
        if {[info exists db($src,$type)]} {
           set handler $db($src,$type)
        } elseif {[info exists db(,$type)]} {
           # No handler registered for $src,$type pair; if $type is known,
           # then fallback to default handler for $type
           set handler $db(,$type)
        } else {
           return -code error "Unknown (source,type) pair: ($src,$type)"
        }
        if {[string match {} $handler]} {
            # No handler, do nothing
            return
        }

        global errorInfo errorCode
	set stack "$errorInfo\n($errorCode)"
        if {[catch {
                uplevel #0 [linsert $handler end $msg $type $src]
                } ret]} {
            if {[string match $class $src] && [string match error $type]} {
		# Registered handler for errors from this class failed.
		# Raising an error from this class would lead to another
		# failure -- infinite loop.  To avoid this, fall back on
		# the default message handler.
		if {[catch {
			uplevel #0 [list Oc_DefaultMessage $msg error $class]
			}]} {
		    # If even *that* fails, try writing to stderr
		    set msg [join [split $msg \n] \n\t]
		    puts stderr "<[pid]> $class error:\nNo usable handler\
			    to report message.  Message was:\n\t$msg"
		    flush stderr
		}
                return
            }
	    set msg [join [split $msg \n] \n\t]
	    set ret [join [split $ret \n] \n\t]
	    set stack [join [split $stack \n] \n\t]
	    set lstack [join [split "$errorInfo\n($errorCode)" \n] \n\t]
	    set fmsg "LogHandler '$handler' failed\n\treporting message\
                    of type '$type'\n\tfrom source '$src'.\n    Original\
                    message:\n\t$msg\n    LogHandler '$handler'\
		    error:\n\t$ret\n    Original stack:\n\t$stack\n   \
		    LogHandler stack:\n\t$lstack"
	    $class Log $fmsg error $class
        }
    }

    ClassConstructor {
        Oc_Log AddSource Oc_Class
	Oc_Log AddSource Oc_Log

        Oc_Log AddType panic	;# Fatal errors
        Oc_Log AddType error	;# Non-fatal errors reported via Tcl's error
				;# unwinding
        Oc_Log AddType warning	;# Non-fatal errors reported some other way
				;# (like from deep within the C++ code)
        Oc_Log AddType info 	;# A noteworthy, unusual event occurred, 
				;# but not an error.
        Oc_Log AddType status	;# The passing of a routine event
				;# (like an iteration count)
        Oc_Log AddType debug	;# Detailed messages useful only for
				;# development.

        Oc_ForceNoTkDefaultMessage

	foreach s {"" Oc_Class Oc_Log} {
            Oc_Log SetLogHandler Oc_DefaultMessage panic $s
            Oc_Log SetLogHandler Oc_DefaultMessage error $s
            Oc_Log SetLogHandler Oc_DefaultMessage warning $s
            Oc_Log SetLogHandler Oc_StderrLogMessage info $s
	}
    }

}

# Redefine bgerror to use Oc_Log
proc bgerror {msg} {
    global errorCode errorInfo

    if {[string match OC [lindex $errorCode 0]]} {
        Oc_Log Log $msg error [lindex $errorCode 1]
    } else {
        Oc_Log Log $msg error
    }
}

# Redefine tclLog (Tcl 8.0+) to use Oc_Log
proc tclLog {string} {
    Oc_Log Log $string warning
}

# If Tk is loaded, use a better message default reporting routine
if {[Oc_Main HasTk]} {
    proc Oc_DefaultMessage {msg type src} {
	global errorInfo errorCode
	foreach {ei ec} [list $errorInfo $errorCode] {break}
        if {[catch {winfo exists .} result] || !$result} {
            # Main application window '.' has been destroyed.
            Oc_ForceNoTkDefaultMessage
	    foreach {errorInfo errorcode} [list $ei $ec] {break}
            set code [catch {
                    uplevel 1 [list Oc_DefaultMessage $msg $type $src]
                    } result]
            return -code $code $result
        }
        option add *Dialog.msg.wrapLength 6i startupFile
        switch $type {
            info -
            status -
            debug {
                set default Continue
                set options [list Continue]
                set bitmap info
            }
            warning {
                set default Continue
                set options [list Die Stack Continue]
                set bitmap warning
                set stackoptions [list Die Continue]
                set stackdefault Continue
            }
            panic {
                set default Die
                set options [list Die Stack]
                set bitmap error
                set stackoptions [list Die]
                set stackdefault Die
            }
            error -
            default {
                set default Die
                set options [list Die Stack Continue]
                set bitmap error
                set stackoptions [list Die Continue]
                set stackdefault Die
            }
        }
        set i 0
        set switchbody {}
        foreach opt $options {
            if {[string match $opt $default]} {
                set defnum $i
                lappend switchbody -1 -
            }
            lappend switchbody $i
            switch $opt {
                Continue {
                    lappend switchbody {}
                }
                Die {
                    if {[string match panic $type]} {
                        lappend switchbody {}
                    } else {
                        lappend switchbody {exit 1}
                    }
                }
                Stack {
                    lappend switchbody {
                        set si 0
                        set sswitchbody {}
                        foreach opt $stackoptions {
                            if {[string match $opt $stackdefault]} {
                                set sdefnum $si
                                lappend sswitchbody -1 -
                            }
                            lappend sswitchbody $si
                            switch $opt {
                                Continue {
                                    lappend sswitchbody {}
                                }
                                Die {
                                    if {[string match panic $type]} {
                                        lappend sswitchbody {}
                                    } else {
                                        lappend sswitchbody {exit 1}
                                    }
                                }
                            }
                            incr si
                        }
                        switch [eval [list tk_dialog .ocdefaultmessage \
                                "[Oc_Main GetInstanceName] Stack Trace" \
                                "[Oc_Main GetInstanceName] Stack:\
                                 \n$ei\n----------\nAdditional\
                                 info: $ec" info $sdefnum] \
                                 $stackoptions] $sswitchbody
                    }
                }
            }
            incr i
        }
        switch [eval [list tk_dialog .ocdefaultmessage \
                "<[pid]> [Oc_Main GetInstanceName] $src $type:" \
                "<[pid]> [Oc_Main GetInstanceName] $src $type:\n$msg" \
                $bitmap $defnum] $options] $switchbody
    }
}

# Alternative file logger.  Sample usage:
#  Oc_FileLogger SetFile $logfile
#  Oc_Log SetLogHandler [list Oc_FileLogger Log] panic
#  Oc_Log SetLogHandler [list Oc_FileLogger Log] error
#  Oc_Log SetLogHandler [list Oc_FileLogger Log] warning
#  Oc_Log SetLogHandler [list Oc_FileLogger Log] info
#  Oc_Log SetLogHandler [list Oc_FileLogger Log] status
Oc_Class Oc_FileLogger {
   common filename {}

   proc Log {msg type src} {
      global errorInfo errorCode tcl_platform

      # Note: In Tcl 8.5.7 (others?), the clock format command
      # below may reset errorInfo.  So make a copy.
      set ei $errorInfo
      set ec $errorCode

      if {[string match {} $filename]} {return}

      regsub -- "\n+\$" $msg {} msg ;# Strip trailing newlines, if any.

      regsub {[.].*$} [info hostname] {} hostname
      if {[catch {set tcl_platform(user)} username]} {
         set username {}
      } else {
         set username "-[string trim $username]"
      }
      set instanceinfo [Oc_Main GetInstanceName]
      set iipid "[lindex $instanceinfo 0]<[pid]-$hostname$username>"
      set iiver [lrange $instanceinfo 1 end]
      set st [string trim "$src $type"]
      if {[catch {open $filename a} chanid]} {
         puts stderr "Error in Oc_FileLogger: unable to\
                        open error log file $filename: $chanid"
      } else {
         catch {
            if {![catch {clock milliseconds} ms]} {
               set secs [expr {$ms/1000}]
               set ms [format {%03d} [expr {$ms - 1000*$secs}]]
               set timestamp [clock format $secs \
                                  -format "%H:%M:%S.$ms %Y-%m-%d"]
            } else {
               set secs [clock seconds]
               set timestamp [format [clock format $secs \
                                          -format %H:%M:%S.???\ %Y-%m-%d]]
            }
            puts $chanid "\n\[$iipid $timestamp\] $iiver $st:\n$msg"
            # If we're not logging from an error, errorInfo and
            # errorCode will be detritus from some previous command,
            # likely a catch.  We might want to find a better way to
            # filter the following error output.
            if {[regexp {panic|error|warning} $type]} {
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
