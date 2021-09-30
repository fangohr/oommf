# FILE: console.tcl
#
#	Provides a console window.
#
# Last modified on: $Date: 2002/03/20 21:40:47 $
# Last modified by: $Author: dgp $
#
# This file is evaluated to provide a console window interface to the
# root Tcl interpreter of an OOMMF application.  It calls on a script 
# included with the Tk script library to do most of the work, making use 
# of Tk interface details which are only semi-public.  For this reason,
# there is some risk that future versions of Tk will no longer support
# this script.  That is why this script has been isolated in a file of 
# its own.

########################################################################
# If the Tcl 'consoleinterp' is already in the interpreter, then this
# file has already been sourced.
########################################################################
if {[llength [info commands consoleinterp]]} {
    return
}

########################################################################
# Check Tcl/Tk support
########################################################################
package require Tcl 8.5-

if {[catch {package require Tk 8}]} {
    if {[catch {package require Tk 4.1}]} {
        return -code error "Tk required but not loaded."
    }
}

set _ [file join $tk_library console.tcl]
if {![file readable $_]} {
    return -code error "File not readable: $_"
}

########################################################################
# Provide the support which the Tk library script console.tcl assumes
########################################################################
# 1. Create an interpreter for the console window widget and load Tk.
#    You can configure the console from inside the interpreter using
#    the 'console eval' command. For example,
#       console eval [list .console configure -background green]
#    NB: The 'eval' subcommand to the 'console' command is not
#    documented in the help message. Remove if desired.
set consoleInterp [interp create]
$consoleInterp eval [list set tk_library $tk_library]
$consoleInterp alias exit exit
load "" Tk $consoleInterp

$consoleInterp eval {
   proc ShowTags { w } {
      # Intended for development.
      set tags [$w tag names]
      foreach t $tags {
         set nv {}
         foreach opt [$w tag configure $t] {
            set n [lindex $opt 0]
            set val [$w tag cget $t $n]
            if {![string match {} $val] && ![string match system* $val]} {
               lappend nv [list $n $val]
            }
         }
         foreach p $nv {
            puts [format "%15s %15s %15s" $t {*}$p]
         }
      }
   }
   proc AutoSetTagColors { w } {
      set shade 0
      foreach component [winfo rgb $w [$w cget -background]] {
         set shade [expr {$shade + $component}]
      }
      set shade [expr {$shade/double(3*65535)}]
      if {$shade<0.5} {
         DarkTags $w
      } else {
         LightTags $w
      }
   }
   proc LightBackground { w } {
      $w configure \
         -background #FFFFFF -foreground #000000 \
         -insertbackground #000000 -insertofftime 0
   }
   proc LightTags { w } {
      # Set text tags appropriate for a light colored background.
      $w tag configure  stderr   -foreground       red
      $w tag configure   stdin   -foreground      blue
      $w tag configure  prompt   -foreground   #8F4433
      $w tag configure    proc   -foreground   #008800
      $w tag configure     var   -background   #FFC0D0
      $w tag configure   blink   -background   #FFFF00
      $w tag configure    find   -background   #FFFF00
   }
   proc DarkBackground { w } {
      $w configure \
         -background #1E1E1E -foreground #FFFFFF \
         -insertbackground #FFFFFF -insertofftime 0
   }
   proc DarkTags { w } {
      # Set text tags appropriate for a dark colored background.
      $w tag configure  stderr   -foreground       red
      $w tag configure   stdin   -foreground   #44aaff
      $w tag configure  prompt   -foreground   #8FFF33
      $w tag configure    proc   -foreground   #FF88FF
      $w tag configure     var   -background   #440000
      $w tag configure   blink   -background   #EEEEEE
      $w tag configure    find   -background   #EEEEEE
   }
}

# 2. A command 'console' in the application interpreter
;proc console {sub {optarg {}}} [subst -nocommands {
    switch -exact -- \$sub {
        title {
            $consoleInterp eval wm title . [list \$optarg]
        }
        lightmode {
            # Colored text in console can be hard to read in macOS dark
            # mode (and others?), so provide a fallback with a white
            # background. The insert entries control cursor color and
            # blink rate.
            $consoleInterp eval LightBackground .console
            $consoleInterp eval LightTags .console
        }
        darkmode {
            $consoleInterp eval DarkBackground .console
            $consoleInterp eval DarkTags .console
        }

        hide {
            $consoleInterp eval wm withdraw .
        }
        show {
            $consoleInterp eval wm deiconify .
        }
        eval {
            $consoleInterp eval \$optarg
        }
        showtags {
           $consoleInterp eval ShowTags .console
        }
        default {
            error "bad option \\\"\$sub\\\": should be\
                   hide, show, lightmode, darkmode, or title"
        }
    }
}]

# 3. Alias a command 'consoleinterp' in the console window interpreter 
#	to cause evaluation of the command 'consoleinterp' in the
#	application interpreter.
;proc consoleinterp {sub cmd} {
    switch -exact -- $sub {
        eval {
            uplevel #0 $cmd
        }
        record {
            history add $cmd
            catch {uplevel #0 $cmd} retval
            return $retval
        }
        default {
            error "bad option \"$sub\": should be eval or record"
        }
    }
}
if {[package vsatisfies [package provide Tk] 4]} {
    $consoleInterp alias interp consoleinterp
} else {
    $consoleInterp alias consoleinterp consoleinterp
}

# 4. Bind the <Destroy> event of the application interpreter's main
#    window to kill the console (via tkConsoleExit)
bind . <Destroy> [list +if {[string match . %W]} [list catch \
        [list $consoleInterp eval tkConsoleExit]]]

# 5. Redefine the Tcl command 'puts' in the application interpreter
#    so that messages to stdout and stderr appear in the console.
rename puts tcl_puts
;proc puts {args} [subst -nocommands {
    switch -exact -- [llength \$args] {
        1 {
            if {[string match -nonewline \$args]} {
		if {[catch {uplevel 1 [linsert \$args 0 tcl_puts]} msg]} {
		    regsub -all tcl_puts \$msg puts msg
		    return -code error \$msg
        	}
            } else {
                $consoleInterp eval [list tkConsoleOutput stdout \
			"[lindex \$args 0]\n"]
            }
        }
        2 {
            if {[string match -nonewline [lindex \$args 0]]} {
                $consoleInterp eval [list tkConsoleOutput stdout \
			[lindex \$args 1]]
            } elseif {[string match stdout [lindex \$args 0]]} {
                $consoleInterp eval [list tkConsoleOutput stdout \
			"[lindex \$args 1]\n"]
            } elseif {[string match stderr [lindex \$args 0]]} {
                $consoleInterp eval [list tkConsoleOutput stderr \
			"[lindex \$args 1]\n"]
            } else {
		if {[catch {uplevel 1 [linsert \$args 0 tcl_puts]} msg]} {
		    regsub -all tcl_puts \$msg puts msg
		    return -code error \$msg
        	}
            }
        }
        3 {
            if {![string match -nonewline [lindex \$args 0]]} {
		if {[catch {uplevel 1 [linsert \$args 0 tcl_puts]} msg]} {
		    regsub -all tcl_puts \$msg puts msg
		    return -code error \$msg
        	}
            } elseif {[string match stdout [lindex \$args 1]]} {
                $consoleInterp eval [list tkConsoleOutput stdout \
			[lindex \$args 2]]
            } elseif {[string match stderr [lindex \$args 1]]} {
                $consoleInterp eval [list tkConsoleOutput stderr \
			[lindex \$args 2]]
            } else {
		if {[catch {uplevel 1 [linsert \$args 0 tcl_puts]} msg]} {
		    regsub -all tcl_puts \$msg puts msg
		    return -code error \$msg
        	}
            }
        }
        default {
	    if {[catch {uplevel 1 [linsert \$args 0 tcl_puts]} msg]} {
                regsub -all tcl_puts \$msg puts msg
                return -code error \$msg
            }
        }
    }
}]
$consoleInterp alias puts puts

# 6. No matter what Tk_Main says, insist that this is an interactive shell
set tcl_interactive 1

########################################################################
# Evaluate the Tk library script console.tcl in the console interpreter
########################################################################
$consoleInterp eval source [list [file join $tk_library console.tcl]]
$consoleInterp eval {
    if {![llength [info commands tkConsoleExit]]} {
	tk::unsupported::ExposePrivateCommand tkConsoleExit
    }
}
$consoleInterp eval {
    if {![llength [info commands tkConsoleOutput]]} {
	tk::unsupported::ExposePrivateCommand tkConsoleOutput
    }
}
if {[string match 8.3.4 $tk_patchLevel]} {
    # Workaround bug in first draft of the tkcon enhancments
    $consoleInterp eval {
	bind Console <Control-Key-v> {}
    }
}
# Restore normal [puts] if console widget goes away...
proc Oc_RestorePuts {slave} {
    rename puts {}
    rename tcl_puts puts
    interp delete $slave
}
$consoleInterp alias Oc_RestorePuts Oc_RestorePuts $consoleInterp
$consoleInterp eval {
    bind Console <Destroy> +Oc_RestorePuts
}

# Setup default text coloring
$consoleInterp eval AutoSetTagColors .console
unset consoleInterp

console title "[wm title .] Console"
