# FILE: main.tcl
#
# There should be exactly one instance of the Oc_Class Oc_Main.
# That instance is meant to represent any information about
# the currently running application.  So far, its main purpose
# is to generate an 'Exit' event which other objects can handle
# with special code to clean up their operations before an
# application terminates.
#
# Last modified on: $Date: 2015/11/04 06:01:36 $
# Last modified by: $Author: donahue $

Oc_Class Oc_Main {

    private common thisFile
    private common pid
    private common oid
    private common start
    private common blockShutdown 0
    private common blockLibShutdown 0
    private common blockExit 0
    private common dataRole unspecified
    ClassConstructor {
       # Set thisFile immediately, so that oommf/config/options.tcl
       # can access GetOOMMFRootDir.  The subsequent Oc_EventHandler
       # calls trigger the loading of oommf/pkg/oc/option.tcl, which
       # in turn loads oommf/config/options.tcl, so we want thisFile
       # defined before that.
       set thisFile [Oc_DirectPathname [info script]]
       Oc_EventHandler Bindtags $class $class
       Oc_EventHandler Bindtags Oc_Log Oc_Log
       set pid [pid]
       set oid {} ;# Default oid is none.  If a Net_Account object is
       ## created, it will reset this to the OID returned by the account
       ## server.
       set start [clock seconds]:[format %x \
                                     [expr {[clock clicks] & 0x7fffffff}]]
    }

    Constructor {args} {
        return -code error "Instances of $class are not allowed"
    }

    proc GetStart {} {
        return $start
    }

    proc HasTk {} {
        string length [package provide Tk]
    }

    proc Initialize {} {
        if {[llength [info commands _${class}_Exit]]} {return}
        rename exit _${class}_Exit
        if {[lsearch [interp aliases] exit] == -1} {
           interp alias {} exit {} $class Exit
        } else {
            #proc exit args "[list eval $class Exit] \$args"
            # Above is broken (fortunately in a way that did not matter)
            proc exit args [format {
                    uplevel 1 [linsert $args 0 {%s} Exit]} [list $class]]
        }
        global argv0 env auto_path
        if {[info exists env(OOMMF_ROOT)] && [info exists argv0]} {
            set appPath [Oc_DirectPathname [file join [pwd] $argv0]]
            set root [Oc_DirectPathname $env(OOMMF_ROOT)]
            foreach re [file split $root] ape [file split $appPath] {
                if {[string match "" $re]} {return}
                if {[string compare $re $ape]} {
                    Oc_Log Log "OOMMF_ROOT is $env(OOMMF_ROOT)\nRunning app\
                        is $appPath\nIs that what you intend?" warning
                    return
                }
            }
        }
        if {[$class HasTk]} {
           # Assign root window to group ".".  Some of the child
           # top-levels in oommf/pkg/ow/ are assigned to group ".", and
           # to get window raising to work properly in some window
           # managers you need toplevels assigned to the same group.
           wm group . .
        }
    }

    proc Preload {args} {
        foreach cmd $args {
            if {[string compare [lindex [info commands $cmd] 0] $cmd] == 0} {
                continue
            }
            if {![auto_load $cmd]} {
                return -code error "Command '$cmd' not loaded"
            }
        }
    }

    proc Exit {{exitcode 0}} {
        # This proc raises three Oc_Events, "Oc_Main Shutdown",
        # "Oc_Main LibShutdown", and "Oc_Main Exit". The first should
        # be used by applications, and the second by code under
        # oommf/pkg/ (i.e., library code). These handlers should
        # employ the Block/AllowShutdown and Block/AllowLibShutdown
        # controls as necessary to ensure proper processing for
        # event-loop based operations. As an example, the Net library
        # uses the LibShutdown event to close all open socket
        # connections. The final event, "Oc_Main Exit" should be used
        # sparingly, and ideally with straight-line code that doesn't
        # enter the event loop. There are, however, Block/AllowExit
        # controls that can be used if necessary.

        Oc_Log Log "Exiting application..." status
        # Don't want to re-enter; remove wm protocol WM_DELETE_WINDOW if
        # it points here and otherwise make [exit] an error so we catch
        # any other attempts.
        if {![catch {wm protocol . WM_DELETE_WINDOW} cmd]
            && [string compare [string trim $cmd] exit]==0} {
           wm protocol . WM_DELETE_WINDOW {}
           ## Don't generate an "exit during exit handling" error if the
           ## user double-clicks the window exit button.
        }
        # Report and handle recursive exit calls.
        proc exit args [format {
           proc exit args {
              # Throw error one time only---exit for real on second pass
              puts stderr "tried to \"exit \$args\" during\
                           exit handling: Level [info level]"
              _%s_Exit 999
           }
           return -code error "tried to \"exit $args\" during\
                               exit handling: Level [info level]"
        } $class]

        Oc_EventHandler Generate $class Shutdown
        while {$blockShutdown > 0} {
            vwait [$class GlobalName blockShutdown]
        }

        Oc_EventHandler Generate $class LibShutdown
        while {$blockLibShutdown > 0} {
            vwait [$class GlobalName blockLibShutdown]
        }

        Oc_EventHandler Generate $class Exit
        while {$blockExit > 0} {
            vwait [$class GlobalName blockExit]
        }
        _${class}_Exit $exitcode
    }

    proc BlockShutdown {} {
        incr blockShutdown
    }
    proc AllowShutdown {} {
        incr blockShutdown -1
    }

    proc BlockLibShutdown {} {
        incr blockLibShutdown
    }
    proc AllowLibShutdown {} {
        incr blockLibShutdown -1
    }

    proc BlockExit {} {
        incr blockExit
    }
    proc AllowExit {} {
        incr blockExit -1
    }

    private common appName
    proc SetAppName {n {reload_options {}} } {
       # In principle, options may be different for different
       # application names, so we may want to reload the options
       # database. Or maybe not. The "reload_options" argument allows
       # the caller to specify whether to reload the options database or
       # not. The default is to do a case-insensitive match against the
       # current application name. Accepted values for reload_options
       # are "reload", "noreload", and "".
       #
       # NOTE: The Oc_Option Add command has signature
       #   proc Add {app className option value}
       # Any Oc_Option Add command for which 'app' does not match
       # -nocase against 'GetAppName' is perforce discarded, so if
       # appName is changed without reloading the options database then
       # options matching the original name will be (still) available,
       # but options matching the new name will not. Typically the
       # options database is read during initialization, prior to the
       # sourcing of the main script, and the initial appName is not set
       # by the main script but rather from either 'tk appname' or the
       # script filename (set Oc_Main GetAppName for details).
       if {[string compare reload $reload_options]==0} {
          set reload 1
       } elseif {[string compare noreload $reload_options]==0} {
          set reload 0
       } elseif {[string compare {} $reload_options]==0} {
          # Decide based on name
          if {[string compare -nocase [Oc_Main GetAppName] $n]==0} {
             # NB: If appName is not set then GetAppName creates a
             #     default name based on the source file name.
             set reload 0
          } else {
             set reload 1
          }
       } else {
          return -code error "invalid argument \"$reload_options\".\
                    Should be one of \"reload\", \"noreload\", or \"\"."
       }
       set appName $n
       if {$reload} {
          update idletasks ;# Run any previously scheduled tasks before
          ## reloading options database. Whether this is correct or not,
          ## there are 'update idletasks' calls inside the error
          ## handling mechanism (for display formatting), and triggering
          ## pre-existing tasks in the midst of error handling --- with
          ## a potentially malformed options database --- is asking for
          ## trouble.
          Oc_Option Undefine
          Oc_Option Get foo
       }
       $class SetInstanceName

       # Init C++ Oc_LogSupport interface. This mimics the log prefix
       # used in Oc_Class Oc_FileLogger (q.v.). (Only do this if the
       # C portion of the oc package is loaded.)
       if {[llength [info commands Oc_InitLogPrefix]]} {
          global tcl_platform
          regsub {[.].*$} [info hostname] {} hostname
          if {[catch {set tcl_platform(user)} username]} {
             set username {}
          } else {
             set username "-[string trim $username]"
          }
          Oc_InitLogPrefix "$appName<[pid]-$hostname$username>"
       }

       return $appName
    }

    proc GetAppName {} {
        if {[info exists appName]} {
            return $appName
        }
#        set app [Oc_Application Self]
#        if {[string length $app]} {
#            return [$class SetAppName [$app Cget -name]]
#        }
        if {[$class HasTk]} {
            return [file rootname [lindex [tk appname] 0]]
        }
        global argv0
        if {[info exists argv0]} {
            return [file tail [file rootname $argv0]]
        }
        return Unknown
    }

    private common instanceName
    proc SetInstanceName {} {
        #set instanceName "<$pid> [$class GetAppName] [$class GetVersion]"
        set instanceName "[$class GetAppName] [$class GetVersion]"
        Oc_SetPanicHeader $instanceName:\n
        return $instanceName
    }

    proc GetInstanceName {} {
        if {[info exists instanceName]} {
            return $instanceName
        }
        return [$class SetInstanceName]
    }

    # Used for title bars
    proc GetTitle {} {
       set title [$class GetAppName]
       if {![string match {} $oid]} {
          set title "<$oid> $title"
       }
       return $title
    }

    private common helpURL
    proc SetHelpURL {url} {
        if {[catch {$url Class} msg] || ![string match Oc_Url $msg]} {
            return -code error "argument must be instance of class Oc_Url"
        }
        return [set helpURL $url]
    }

    proc GetHelpURL {} {
        if {[info exists helpURL]} {
            return $helpURL
        }
        return [Oc_Url FromFilename [file join [file dirname [file dirname \
                [file dirname $thisFile]]] doc userguide userguide]]
    }

    private common licenseFile
    proc SetLicenseFile {n} {
        return [set licenseFile $n]
    }
    proc GetLicenseFile {} {
        if {[info exists licenseFile]} {
            return $licenseFile
        }
        return [file join [file dirname [file dirname \
                [file dirname $thisFile]]] LICENSE]
    }

    proc SetPid {p} {
        set pid $p
        #$class SetInstanceName
        return $pid
    }
    proc GetPid {} {
        return $pid
    }

    proc SetOid {o} {
        set oid $o
        return $oid
    }
    proc GetOid {} {
        return $oid
    }

    private common version
    proc SetVersion {v} {
        set version $v
        $class SetInstanceName
        return $version
    }
    proc GetVersion {} {
        if {[info exists version]} {
            return $version
        }
#        set app [Oc_Application Self]
#        if {[string length $app]} {
#            return $class SetVersion [$app Cget -version]
#        }
        return ""
    }

    private common date
    proc SetDate {d} {
        return [set date $d]
    }
    proc GetDate {} {
        if {[info exists date]} {
            return $date
        }
        return unknown
    }

    private common author
    proc SetAuthor {a} {
        if {[catch {$a Class} msg] || ![string match Oc_Person $msg]} {
            return -code error "argument must be instance of class Oc_Person"
        }
        return [set author $a]
    }
    proc GetAuthor {} {
        if {[info exists author]} {
            return $author
        }
        return [Oc_Person Lookup unknown]
    }

    private common extraInfo
    proc SetExtraInfo {i} {
        return [set extraInfo $i]
    }
    proc GetExtraInfo {} {
        if {[info exists extraInfo]} {
            return $extraInfo
        }
        return ""
    }

    proc GetOOMMFRootDir {} {
        return [file dirname [file dirname [file dirname $thisFile]]]
    }

    proc SetDataRole { role } {
       switch -exact $role {
          producer { set dataRole producer }
          consumer { set dataRole consumer }
          default {
             return -code error "Data role must be one\
                                 of \"producer\" or \"consumer\""
          }
       }
    }

    proc GetDataRole {} {
       return $dataRole
    }

}
