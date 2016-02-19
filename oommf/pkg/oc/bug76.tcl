# FILE: bug76.tcl
#
#	Patches for Tcl 7.6 / Tk 4.2
#
# Last modified on: $Date: 2002/10/25 05:22:21 $
# Last modified by: $Author: donahue $
#
# This file contains Tcl code which when sourced in a Tcl 7.6 interpreter
# will patch bugs in Tcl 7.6 fixed in later versions of Tcl.  It is not
# meant to be complete.  Bugs are patched here only as we discover the need
# to do so in writing other OOMMF software.

Oc_Log Log "Patching Tcl 7.6 bugs..." status

# There is a bug in [clock format] (fixed in Tcl 7.6p2) which causes it
# to fail if the value of the -format option does not contain a '%'.
rename clock Tcl7.6_clock
;proc clock {option args} {
    switch -glob -- $option {
        f* {
	    if {[string first $option format] != 0} {
		return [uplevel [list Tcl7.6_clock $option] $args]
	    }
	    array set tmp [lrange $args 1 end]
	    if {[catch {regexp % $tmp(-format)} hasPercent] || $hasPercent} {
		return [uplevel [list Tcl7.6_clock $option] $args]
	    } else {
		return $tmp(-format)
	    }
        }
        default {
            uplevel [list Tcl7.6_clock $option] $args
        }
    }
}

# The subcommands nativename and attributes were added to
# the Tcl command 'file' in Tcl version 8.0.  Here is an approximation
# for earlier Tcl versions:
rename file Tcl7.6_file
;proc file {option args} {
    # Not really the right way to accept unique abbreviations
    switch -glob -- $option {
        att* {
	    if {[string first $option attributes] != 0} {
		return [uplevel [list Tcl7.6_file $option] $args]
	    }
            return -code error "Tcl [package provide Tcl] does not support\
		    \[file attributes\].\n\tUpgrade to Tcl 8.0 to use it."
        }
        n* {
	    if {[string first $option nativename] != 0} {
		uplevel [list Tcl7.6_file $option] $args
	    }
	    if {![llength $args]} {
		return -code error "wrong # args: should be\
        		\"file nativename name ?arg ...?\""
	    }
	    set fcomps [file split [lindex $args 0]]
	    # Take care of tilde substitution
	    set first [lindex $fcomps 0]
	    if {[string match ~* $first]} {
		set first [file join [file dirname $first] [file tail $first]]
            }
	    set result [eval file join [list $first] [lrange $fcomps 1 end]]
	    global tcl_platform
	    if {[string match windows $tcl_platform(platform)]} {
		regsub -all -- / $result \\ result
	    }
	    return $result
        }
        default {
            uplevel [list Tcl7.6_file $option] $args
        }
    }
}

# The 'package unknown' handler distributed with Tcl 8.0 is far superior
# to those of earlier Tcl versions.  It is able to withstand errors in
# pkgIndex.tcl files.  It also uses the search path introduced in Tcl 7.6.
#
# The proc 'Oc_PkgUnknown' below is derived from the proc 
# 'tclPkgUnknown' distributed with Tcl 8.0.  
# NOMAC -- It has had Macintosh platform support stripped out.
#
# If the local Tcl/Tk installation uses a customized 'package unknown'
# command, then the Oc extension will also have to be customized to
# use that command.
;proc Oc_PkgUnknown {name version {exact {}}} {
    global auto_path tcl_platform env

    if {![info exists auto_path]} {
	return
    }
    for {set i [expr {[llength $auto_path] - 1}]} {$i >= 0} {incr i -1} {
	# we can't use glob in safe interps, so enclose the following
	# in a catch statement
	catch {
	    foreach file [glob -nocomplain [file join [lindex $auto_path $i] \
		    * pkgIndex.tcl]] {
		set dir [file dirname $file]
		if {[catch {source $file} msg]} {
		    tclLog "error reading package index file $file: $msg"
		}
	    }
        }
	set dir [lindex $auto_path $i]
	set file [file join $dir pkgIndex.tcl]
	# safe interps usually don't have "file readable", nor stderr channel
	if {[interp issafe] || [file readable $file]} {
	    if {[catch {source $file} msg] && ![interp issafe]}  {
		tclLog "error reading package index file $file: $msg"
	    }
	}
    }
}
package unknown Oc_PkgUnknown

# Should eventually robustify this to duplicate [fcopy]'s error msgs, etc.
array set Tcl7.6_fcopy [list "" ""]
unset Tcl7.6_fcopy()
;proc Tcl7.6_fcopy {i o toCopy copied} {
    global Tcl7.6_fcopy
    fileevent $i readable {}

    # We need our [fcopy] replacement to be "binary-safe".
    # In Tcl 7.*, the only command which can perform a binary-safe output
    # to a channel is [unsupported0].  

    if {[catch {unsupported0 $i $o 1} written]} {
	set Tcl7.6_fcopy($i) [list $copied $written]
    } elseif {$written == 0} {
	# EOF on $i --> quit.
	set Tcl7.6_fcopy($i) $copied
    } else {
	incr toCopy -$written
	incr copied $written
	if {$toCopy == 0} {
	    # Copy reqest completed.
	    set Tcl7.6_fcopy($i) $copied
	} else {
	    # Keep working
	    fileevent $i readable [list Tcl7.6_fcopy $i $o $toCopy $copied]
	}
    }
}
;proc Tcl7.6_fcopyTrace {cmd n1 n2 op} {
    set val [uplevel 1 [list set ${n1}($n2)]]
    uplevel 1 [list unset ${n1}($n2)]
    uplevel #0 $cmd $val
}
;proc fcopy {in out args} {
    # Strange quirk: if [unsupported0] has negative request for number of
    # bytes to copy, it will copy until EOF
    set aa(-size) -1
    array set aa $args
    if {[catch {incr aa(-size) 0} msg]} {
	return -code error "bad -size argument: $msg"
    }
    if {[info exists aa(-command)]} {
	global Tcl7.6_fcopy
	if {![string match "" [fileevent $in readable]]} {
	    return -code error "can't fcopy from $in in background;\
		    fileevent in use:\n[fileevent $in readable]"
	}
	fileevent $in readable [list Tcl7.6_fcopy $in $out $aa(-size) 0] 
	trace variable Tcl7.6_fcopy($in) w \
		[list Tcl7.6_fcopyTrace $aa(-command)]
	return {}
    } else {
        return [uplevel [list unsupported0 $in $out $aa(-size)]]
    }
}

# If Tk is loaded...
# Patch a bug MJD found in the Tk 4.2 distribution
if { [string match 4.2 [package provide Tk]]} {
    # The distribution tkMenuFind source seems to be broken: it has
    # comments in an illegal position inside the 'switch' statement.
    # The following code is from Tk 4.2, but with the comments moved.
    proc tkMenuFind { w char } {
	global tkPriv
	set char [string tolower $char]
	
	foreach child [winfo child $w] {
	    switch [winfo class $child] {
		Menubutton {
		    set char2 [string index [$child cget -text]  \
			    [$child cget -underline]]
		    if {([string compare $char \
			    [string tolower $char2]] == 0) \
			    || ($char == "")} {
			if {[$child cget -state] != "disabled"} {
			    return $child
			}
		    }
		}
		default {
		    # The tag above used to be "Frame", but it was changed so
		    # that the code would work with Itcl 2.0, which apparently
		    # uses other classes of widgets to hold menubuttons.
		    set match [tkMenuFind $child $char]
		    if {$match != ""} {
			return $match
		    }
		}
	    }
	}
	return {}
    }
}

# Starting in Tk 8.0, the destroy command stopped returning an error when
# given a non-existent window as an argument.
if {[llength [info commands destroy]]} {
    rename destroy Tk4.2_destroy
    proc destroy {args} {
        catch {uplevel Tk4.2_destroy $args}
    }
}

# Starting in Tk 8.0, the grid command allowed configuration of
# a list of rows or columns, not just one.
if {[llength [info commands grid]]} {
    rename grid Tk4.2_grid
    proc grid {option args} {
        switch -glob -- $option {
            ro*  -
            col* {
		if {([string first $option rowconfigure] != 0)
			&& ([string first $option columnconfigure] != 0)} {
		    return [uplevel [list Tk4.2_grid $option] $args]
		}
                if {([llength $args] < 4)
                        || ([llength [lindex $args 1]] <= 1)} {
                    uplevel [list Tk4.2_grid $option] $args
                } else {
                    set master [lindex $args 0]
                    set indices [lindex $args 1]
                    set options [lrange $args 2 end]
                    foreach index $indices {
                        uplevel [list Tk4.2_grid $option $master $index] \
                                $options
                    }
                }
            }
            default {
                uplevel [list Tk4.2_grid $option] $args
            }
        }
    }
}

# The -title option for menu instances is new in Tk 8.0.
if {[llength [info commands menu]]} {
    rename menu Tk4.2_menu
    proc menu {args} {
        set cmd [uplevel [list Tk4.2_menu [lindex $args 0]]]
        rename $cmd Tk4.2_$cmd
        proc $cmd {option args} {
            set myName [lindex [info level 0] 0]
            switch -glob -- $option {
                cg* {
		    if {[string first $option cget] != 0} {
			return [uplevel [list Tk4.2_$myName $option] $args]
		    }
                    if {[llength $args] == 1
                            && [string match -title [lindex $args 0]]} {
                        return ""
                    } else {
                        uplevel [list Tk4.2_$myName $option] $args
                    }
                }
                co* {
		    if {[string first $option configure] != 0} {
			return [uplevel [list Tk4.2_$myName $option] $args]
		    }
                    switch -exact -- [llength $args] {
                        0 {
                            set options [uplevel [list Tk4.2_$myName configure]]
                            lappend options {-title title Title {} {}}
                            return [lsort $options]
                        }
                        1 {
                            if {[string match -title [lindex $args 0]]} {
                                return {-title title Title {} {}}
                            } else {
                                uplevel [list Tk4.2_$myName configure] $args
                            }
                        }
                        default {
                            set bg [Tk4.2_$myName cget -bg]
                            if {[llength $args] % 2} {
                                set odd [lindex $args end]
                                set args [lreplace $args end end]
                            }
                            set passArgs [list -bg $bg]
                            foreach {opt val} $args {
                                if {[string compare -title $opt]} {
                                    lappend passArgs $opt $val
                                }
                            }
                            if {[info exists odd] 
                                    && [string compare -title $odd]} {
                                uplevel [list Tk4.2_$myName configure] \
                                        $passArgs [list $odd]
                            } elseif {[info exists odd]} {
                                uplevel [list Tk4.2_$myName configure] \
                                        $passArgs
                                return -code error \
                                        "value for \"-title\" missing"
                            } else {
                                uplevel [list Tk4.2_$myName configure] \
                                        $passArgs
                            }
                        }
                    }
                }
                default {
                    uplevel [list Tk4.2_$myName $option] $args
                }
            }
        }
        uplevel [list $cmd configure] [lrange $args 1 end]
        return $cmd
    }
}

# The -dictionary option to lsort was added in Tcl 8.0
rename lsort Tcl7.6_lsort
;proc lsort {args} {
    set dictindex [lsearch -exact $args "-dictionary"]
    if {$dictindex>=0 && $dictindex < [expr {[llength $args]-1}]} {
	set args [lreplace $args $dictindex $dictindex \
		-command Tcl7.6_lsort_dictcompare]
    }
    uplevel Tcl7.6_lsort $args
}
;proc Tcl7.6_lsort_dictcompare { instr1 instr2 } {
    set str1 [string tolower $instr1]
    set str2 [string tolower $instr2]
    set lindex 0
    set rindex 0
    set zerodiff 0
    # It is rather annoying to have to do the
    # character-by-character tests below, but
    # it seems the only way to get the corner
    # cases correct.
    while {1} {
	# Skip over identical text
	set char1 [string index $str1 $lindex]
	set char2 [string index $str2 $rindex]
	while {[set chkstr [string compare $char1 $char2]] == 0} {
	    if {[string match {} $char1]} {
		if {$zerodiff != 0} { return $zerodiff }
		return [string compare $instr1 $instr2]
	    }
	    set char1 [string index $str1 [incr lindex]]
	    set char2 [string index $str2 [incr rindex]]
	}
	if {![string match {[0-9]} $char1] || \
		![string match {[0-9]} $char2]} {
	    return $chkstr
	}
	# Embedded numeric substring
	regexp -- {^(0*)([0-9]+)} [string range $str1 $lindex end] \
		dummy lzeros lnum
	regexp -- {^(0*)([0-9]+)} [string range $str2 $rindex end] \
		dummy rzeros rnum
	set diff [expr {$lnum-$rnum}]
	if {$diff!=0} {return $diff}
	set zerodiff \
		[expr {[string length $lzeros]-[string length $rzeros]}]
	incr lindex [string length $lzeros]
	incr lindex [string length $lnum]
	incr rindex [string length $rzeros]
	incr rindex [string length $rnum]
    }
    if {$zerodiff != 0} { return $zerodiff }
    return [string compare $instr1 $instr2]
}


# On pre-Tcl 8.0 + Windows, use "fixed" font by default.
# (Not sure we want to keep this...)
if {[llength [info commands option]] && \
	[string match windows $tcl_platform(platform)]} {
    catch { option add *font "fixed" }
}

# Continue with bug patches for the next Tcl version
source [file join [file dirname [info script]] bug80.tcl]

