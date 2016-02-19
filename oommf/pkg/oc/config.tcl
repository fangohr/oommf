# FILE: config.tcl
#
# Last modified on: $Date: 2015/08/13 20:23:19 $
# Last modified by: $Author: donahue $

Oc_Class Oc_Config {

    common configDir
    common namesDir
    common cacheDir

    # The Oc_Config instance chosen which represents the platform on which 
    # this program is running
    common runPlatform
    common predefinedEnvVars

    ClassConstructor {
        set configDir [file join [file dirname [file dirname [file dirname \
                [Oc_DirectPathname [info script]]]]] config]
        set namesDir [file join $configDir names]
        set cacheDir [file join $configDir platforms]
        foreach fn [glob -nocomplain [file join $namesDir *.tcl]] {
            if {[file readable $fn]} {
                if {[catch {uplevel #0 [list source $fn]} msg]} {
                    Oc_Log Log "Error (1) sourcing $fn:\n\t$msg" warning $class
                }
            }
        }
        set detected {}
        global env
        if {[info exists env(OOMMF_PLATFORM)]} {
           # User override
           if {[catch {$class Lookup $env(OOMMF_PLATFORM)} result]} {
              set msg "Platform name from environment variable\
                       OOMMF_PLATFORM, \"$env(OOMMF_PLATFORM)\",\
                       is not recognized.\nEither correct the\
                       value or else unset it to get automatic\
                       platform detection."
              error $msg $msg
           }
           set detected [list $result]
        } else {
           foreach i [$class Instances] {
              if {[$i Detect]} {
                 lappend detected $i
              }
           }
        }

        # The default case here should be to provide for user selection
        # of the correct configuration name.  For now, just die.
        switch -exact -- [llength $detected] {
            0 {set runPlatform [$class New _ unknown {return 1}]}
            1 {set runPlatform [lindex $detected 0]}
            default {
                set msg "Multiple platform names are compatible with your\
                        computer:"
                foreach c $detected {
		    append msg \n\t[$c GetValue platform_name]
		}
		append msg "\nYou must edit the files in $namesDir .\n"
		append msg "See the `Advanced Installation' section of the\n"
		append msg "OOMMF User's Manual for instructions."
		error $msg $msg
            }
        }
	global env
        set predefinedEnvVars [concat [array names env OOMMF*] \
                     [array names env TCL*] [array names env TK*] \
                     [array names env OSTYPE] ]
        # NOTE: env(OSTYPE) is used in some of the cygwin environment
        #       discovery code; See the Oc_IsCygwinPlatform and
        #       Oc_IsCygwin64Platform procs in oommf/pkg/oc/procs.tcl.
	$runPlatform RecordTclTkConfiguration
	$runPlatform FindTclTkIncludes 

        # Local build options, optionally set in local platform
        # specific files in oommf/config/platforms/local/
        if {[catch {$runPlatform GetValue oommf_threads}]} {
           # Default is to follow Tcl value
           global tcl_platform
           if {[info exists tcl_platform(threaded)] \
                  && $tcl_platform(threaded)} {
              $runPlatform SetValue oommf_threads 1  ;# Do threads
           } else {
              $runPlatform SetValue oommf_threads 0  ;# No threads
           }
        }

        $runPlatform LoadCache
	# If Tcl and Tk were installed under different --prefix directories
	# the OOMMF applications will need help to find the Tk script library
	if {![info exists env(TK_LIBRARY)]
		&& ![catch {$runPlatform GetValue TCL_PREFIX} tcp]
		&& ![catch {$runPlatform GetValue TK_PREFIX} tkp]
		&& [string compare $tcp $tkp]} {
	    set env(TK_LIBRARY) [file join $tkp lib tk[$class TkVersion]]
	}
    }

    proc TkVersion {} {
	set ret [package provide Tk]
	if {[string length $ret]} {
	    return $ret
	}
	set ret [info tclversion]
	if {[package vcompare $ret 8] >= 0} {
	    return $ret
	}
       format "%g" [expr {$ret - 3.4}]  ;# Round slightly
    }

    proc Lookup {name} {
        foreach i [$class Instances] {
            if {[string match $name [$i GetValue platform_name]]} {
                return $i
            }
        }
        return -code error "Unknown configuration name: $name"
    }

    proc RunPlatform {} {runPlatform} {
        return $runPlatform
    }

    private variable name
    private variable detectScript

    private array variable values

    Constructor {_name _detectScript} {
        set name $_name
        if {![info complete $_detectScript]} {
            return -code error "detectScript must be a complete Tcl script"
        }
        set detectScript $_detectScript
        $this SetValue platform_name $name
    }

    method SetValue {feature value} {
        set values($feature) $value
    }

    method Features {glob} {
	return [array names values $glob]
    }

    method GetValue {feature} {
        # Do we already have a value?
        if {[info exists values($feature)]} {
            return $values($feature)
        }
	set msg "Unknown feature '$feature' in configuration '$name'\n\t"
	global env tcl_platform
	if {[string match TCL_* $feature]} {
	    if {[info exists env(OOMMF_TCL_CONFIG)]} {
		append msg "Something may be wrong with the Tcl configuration\
			file:\n\t\t$env(OOMMF_TCL_CONFIG)"
	    } else {
		if {![string match windows $tcl_platform(platform)]} {
		    append msg "No Tcl configuration file found\
			    (tclConfig.sh)\n\tSet the environment variable\
			    OOMMF_TCL_CONFIG to its absolute location."
		} else {
		    append msg "Have you edited [file join $cacheDir \
			    $name.tcl] ?"
		}
	    }
	} elseif {[string match TK_* $feature]} {
	    if {[info exists env(OOMMF_TK_CONFIG)]} {
		append msg "Something may be wrong with the Tk configuration\
			file:\n\t\t$env(OOMMF_TK_CONFIG)"
	    } else {
		if {![string match windows $tcl_platform(platform)]} {
		    append msg "No Tk configuration file found\
			    (tkConfig.sh)\n\tSet the environment variable\
			    OOMMF_TK_CONFIG to its absolute location."
		} else {
		    append msg "Have you edited [file join $cacheDir \
			    $name.tcl] ?"
		}
	    }
	} else {
	    append msg "Have you edited [file join $cacheDir $name.tcl] ?"
	}
        return -code error $msg
    }

    method Tclsh {} {
	global env
	if {[info exists env(OOMMF_TCLSH)]} {
	    return $env(OOMMF_TCLSH)
	}
	if {![catch {$this GetValue TCL_EXEC_PREFIX} epfx]} {
	    set v [$this GetValue TCL_VERSION]
	    lappend sl [file join $epfx bin tclsh$v]
	    lappend sl [file join $epfx bin tclsh[join [split $v .] ""]]
	    eval lappend sl [glob -nocomplain [file join $epfx bin *tclsh$v*]]
	    eval lappend sl [glob -nocomplain [file join $epfx bin \
		    *tclsh[join [split $v .] ""]*]]
	    foreach s $sl {
		set s [auto_execok $s]
		if {[llength $s]} {
		    set env(OOMMF_TCLSH) [lindex $s 0]
		    return $env(OOMMF_TCLSH)
		}
	    }
	}
	if {[regexp -nocase tclsh [file tail [info nameofexecutable]]]} {
	    set env(OOMMF_TCLSH) [info nameofexecutable]
	    return $env(OOMMF_TCLSH)
	}
	return -code error "Can't find a tclsh shell program"
    }

    method Wish {} {
	global env
	if {[info exists env(OOMMF_WISH)]} {
	    return $env(OOMMF_WISH)
	}
	if {![catch {$this GetValue TK_EXEC_PREFIX} epfx]} {
	    set v [$this GetValue TK_VERSION]
	    lappend sl [file join $epfx bin wish$v]
	    lappend sl [file join $epfx bin wish[join [split $v .] ""]]
	    eval lappend sl [glob -nocomplain [file join $epfx bin *wish$v*]]
	    eval lappend sl [glob -nocomplain [file join $epfx bin \
		    *wish[join [split $v .] ""]*]]
	    foreach s $sl {
		set s [auto_execok $s]
		if {[llength $s]} {
		    set env(OOMMF_WISH) [lindex $s 0]
		    return $env(OOMMF_WISH)
		}
	    }
	}
	if {[regexp -nocase wish [file tail [info nameofexecutable]]]} {
	    set env(OOMMF_WISH) [info nameofexecutable]
	    return $env(OOMMF_WISH)
	}
	if {[regexp -nocase tclsh [file tail [info nameofexecutable]]]} {
	    set p [file dirname [info nameofexecutable]]
	    set v [file tail [info nameofexecutable]]
	    regsub -nocase tclsh $v wish v
	    set s [file join $p $v]
	    set s [auto_execok $s]
	    if {[llength $s]} {
		set env(OOMMF_WISH) [lindex $s 0]
		return $env(OOMMF_WISH)
	    }
	}
	return -code error "Can't find a wish shell program"
    }

    method SnapshotDate {} {
       # If not a snapshot release, return empty string.
       # Otherwise, return string of the form yyyy.mm.dd
       # return "2009.10.15"
       return {2015.03.25}
    }

    method OommfApiIndex {} {
       # Date of most recent OOMMF API change.  Intended for use
       # primarily by external extension writers.  This value is
       # echoed in the ocport.h header by the Oc_MakePortHeader proc
       # in oc/procs.tcl. Format is "yyyymmdd".
       return 20150129
    }

    method Summary {} {
       global tcl_platform
       set ret "Platform Name:\t\t$name\n"
       append ret "Tcl name for OS:\t"
       append ret "$tcl_platform(os) $tcl_platform(osVersion)\n"
       append ret "C++ compiler:   \t"
       if {![catch {$this GetValue program_compiler_c++} c]} {
          set wd [auto_execok [lindex $c 0]]
          if {![string match {} $wd]} {
             append ret "$wd "
          } else {
             append ret "not found - \"[lindex $c 0]\""
          }
          append ret \n
          if {![catch {$this GetValue program_compiler_c++_banner_cmd} \
                   bcmd]} {
             if {![catch {eval $bcmd} banner] && [string length $banner]} {
                append ret " Version string:\t $banner\n"
             }
          }
          if {![catch {$this GetValue program_compiler_c++_cpu_arch} arch]} {
             append ret " Compiler target arch:\t $arch\n"
          }
       } else {
          append ret "none selected\n"
       }

       append ret "Shell details ---\n"
       set noe [Oc_DirectPathname [info nameofexecutable]]
       append ret " tclsh (running): \t$noe\n"
       set loopcount 0
       while {[string match link [file type $noe]]} {
          set noe [file join [file dirname $noe] \
                      [file readlink $noe]]
          set noe [Oc_DirectPathname $noe]
          append ret "                  \t(links to $noe)\n"
          if {[incr loopcount]>10} {
             append ret "<link resolution terminated>"
             break
          }
       }
       append ret "                  \t --> Version [info patchlevel]"
       if {[info exists tcl_platform(pointerSize)]} {
          append ret ", [expr {$tcl_platform(pointerSize)*8}] bit"
       } elseif {[string compare windows $tcl_platform(platform)]==0} {
          if {[string match amd64 $tcl_platform(machine)]} {
             append ret ", 64 bit"
          } elseif {[string match intel $tcl_platform(machine)] ||
                    [string match i?86  $tcl_platform(machine)]} {
             append ret ", 32 bit"
          }
       } elseif {[string compare unix $tcl_platform(platform)]==0} {
          if {[info exists tcl_platform(wordSize)]} {
             append ret ", [expr {$tcl_platform(wordSize)*8}] bit"
          }
       }
       if {[info exists tcl_platform(threaded)] && $tcl_platform(threaded)} {
          append ret ", threaded"
       } else {
          append ret ", not threaded"
       }
       append ret "\n"

       set tclinfoscript [file join [Oc_Main GetOOMMFRootDir] \
                             pkg oc tclinfo.tcl]
       if {![catch {$this Tclsh} tclsh]} {
          append ret " tclsh (OOMMF): \t$tclsh\n"
          if {![catch {exec $tclsh $tclinfoscript} target_tcl_info]} {
             append ret "                  \t --> Version "
             set target_tcl_info [regsub -all "\t" $target_tcl_info " "]
             if {[regexp {Tcl patchlevel *= *([0-9.]*)} \
                     $target_tcl_info dummy target_tcl_version]} {
                append ret "$target_tcl_version"
                unset target_tcl_version
             } else {
                append ret "?"
             }
             if {[regexp {tcl_platform\(pointerSize\) *= *([0-9.]*)} \
                     $target_tcl_info dummy pointerSize]} {
                append ret ", [expr {$pointerSize*8}] bit"
                unset pointerSize
             } elseif {[regexp {tcl_platform\(wordSize\) *= *([0-9.]*)} \
                     $target_tcl_info dummy wordSize]} {
                append ret ", [expr {$wordSize*8}] bit"
                unset wordSize
             }
             if {[regexp {tcl_platform\(threaded\) *= *([0-9]*)} \
                     $target_tcl_info dummy threaded] && $threaded} {
                append ret ", threaded"
             } else {
                append ret ", not threaded"
             }
             catch {unset threaded}
             append ret "\n"
          } else {
             append ret "                  \t --> RUN ERROR\n"
          }
          catch {unset target_tcl_info}
       } else {
          append ret " tclsh (OOMMF): \t(none)\n"
       }

       if {![catch {eval Oc_Application CommandLine filtersh \
                         [list $tclinfoscript] -tk 0} filtershcmd]} {
          while {[string match & [lindex $filtershcmd end]]} {
             set filtershcmd [lreplace $filtershcmd end end]
          }
          append ret " filtersh:           \t[lindex $filtershcmd 0]\n"
          if {![catch {eval exec $filtershcmd} target_tcl_info]} {
             append ret "                  \t --> Version "
             set target_tcl_info [regsub -all "\t" $target_tcl_info " "]
             if {[regexp {Tcl patchlevel *= *([0-9.]*)} \
                     $target_tcl_info dummy target_tcl_version]} {
                append ret "$target_tcl_version"
                unset target_tcl_version
             } else {
                append ret "?"
             }
             if {[regexp {tcl_platform\(pointerSize\) *= *([0-9.]*)} \
                     $target_tcl_info dummy pointerSize]} {
                append ret ", [expr {$pointerSize*8}] bit"
                unset pointerSize
             } elseif {[regexp {tcl_platform\(wordSize\) *= *([0-9.]*)} \
                           $target_tcl_info dummy wordSize]} {
                append ret ", [expr {$wordSize*8}] bit"
                unset wordSize
             }
             if {[regexp {tcl_platform\(threaded\) *= *([0-9]*)} \
                     $target_tcl_info dummy threaded] && $threaded} {
                append ret ", threaded"
             } else {
                append ret ", not threaded"
             }
             catch {unset threaded}
             append ret "\n"
          } else {
             # Run error.  Pull out whatever information we can.
             set work_info [regsub -all "\t\r" $target_tcl_info " "]
             if {[regexp {Tcl patchlevel *= *([0-9.]*)} \
                     $work_info dummy target_tcl_version]} {
                set msg \
                   "                  \t --> Version $target_tcl_version"
                if {[regexp {tcl_platform\(pointerSize\) *= *([0-9.]*)} \
                        $work_info dummy pointerSize]} {
                   append msg ", [expr {$pointerSize*8}] bit"
                   unset pointerSize
                   if {[regexp {tcl_platform\(wordSize\) *= *([0-9.]*)} \
                           $work_info dummy wordSize]} {
                      append msg ", [expr {$wordSize*8}] bit"
                      unset wordSize
                      if {[regexp {tcl_platform\(threaded\) *= *([0-9]*)} \
                              $work_info dummy threaded] && $threaded} {
                         append msg ", threaded"
                      } else {
                         append msg ", not threaded"
                      }
                      catch {unset threaded}
                   }
                }
                append ret "$msg\n"
             }
             set ws "\[ \n\t\r\]+"
             if {[regexp -- "tclinfo${ws}Oc_Config warning:${ws}(Tcl version mismatch:${ws}\[^\n\]+\n${ws}Running\[^\n\]+)" $target_tcl_info dummy report]} {
                set target_tcl_info $report
             }
             append ret "                  \t\
              *** filtersh RUN ERROR: $target_tcl_info\n"
             append ret "                  \t ***\
              This may indicate a Tcl/Tk library version mismatch ***\n"
          }
          catch {unset target_tcl_info}
       }
       unset filtershcmd

       append ret " tclConfig.sh:        \t"
       global env
       if {[info exists env(OOMMF_TCL_CONFIG)]} {
          append ret "$env(OOMMF_TCL_CONFIG)\n"
       } elseif {[string match windows $tcl_platform(platform)]} {
          append ret "not required on Windows\n"
       } else {
          append ret "none found\n"
       }
       append ret "                      \t --> Version "
       if {![catch {$this GetValue TCL_VERSION} v]} {
          if {![catch {$this GetValue TCL_PATCH_LEVEL} pl]} {
             append ret $v$pl
          } else {
             append ret $v
          }
       } else {
          append ret unknown
       }
       append ret "\n"

       catch {$this Wish} wish
       append ret " wish (OOMMF):        \t$wish\n"
       if {![catch {exec $wish $tclinfoscript} target_tcl_info]} {
          append ret "                  \t --> Version "
          set target_tcl_info [regsub -all "\t" $target_tcl_info " "]
          if {[regexp {Tcl patchlevel *= *([0-9.]*)} \
                  $target_tcl_info dummy target_tcl_version]} {
             append ret "$target_tcl_version"
             unset target_tcl_version
          } else {
             append ret "?"
          }
          append ret ", Tk "
          if {[regexp {Tk version *= *([0-9.]*)} \
                  $target_tcl_info dummy target_tk_version]} {
             append ret "$target_tk_version"
             unset target_tk_version
          } else {
             append ret "?"
          }
          if {[regexp {tcl_platform\(pointerSize\) *= *([0-9.]*)} \
                  $target_tcl_info dummy pointerSize]} {
             append ret ", [expr {$pointerSize*8}] bit"
             unset pointerSize
          } elseif {[regexp {tcl_platform\(wordSize\) *= *([0-9.]*)} \
                        $target_tcl_info dummy wordSize]} {
             append ret ", [expr {$wordSize*8}] bit"
             unset wordSize
          }
          if {[regexp {tcl_platform\(threaded\) *= *([0-9]*)} \
                  $target_tcl_info dummy threaded] && $threaded} {
             append ret ", threaded"
          } else {
             append ret ", not threaded"
          }
          catch {unset threaded}
          append ret "\n"
       }
       catch {unset target_tcl_info}
       unset tclinfoscript

       append ret " tkConfig.sh:         \t"
       global env
       if {[info exists env(OOMMF_TK_CONFIG)]} {
          append ret "$env(OOMMF_TK_CONFIG)\n"
       } elseif {[string match windows $tcl_platform(platform)]} {
          append ret "not required on Windows\n"
       } else {
          append ret "none found\n"
       }
       append ret "                      \t --> Tk Version "
       if {![catch {$this GetValue TK_VERSION} v]} {
          if {![catch {$this GetValue TK_PATCH_LEVEL} pl]} {
             append ret $v$pl
          } else {
             append ret $v
          }
       } else {
          append ret unknown
       }

        append ret    "\nOOMMF threads:         \t"
        if {[$this GetValue oommf_threads]} {
           append ret "Yes: Default thread count = [Oc_GetDefaultThreadCount]"
           if {![catch {$this GetValue use_numa} use_numa]} {
           append ret "\n  NUMA support:        \t "
              if {$use_numa} {
                 append ret "Yes: Default nodes = [Oc_GetDefaultNumaNodes]"
              } else {
                 append ret "No"
              }
           }
        } else {
           append ret "No"
        }

        append ret "\nOOMMF API index:       \t[$this OommfApiIndex]"

	Oc_TempFile New xxx
	append ret "\nTemp file directory: \t[$xxx Cget -directory]"
	$xxx Delete

	if {[llength $predefinedEnvVars]} {
	    append ret "\n\nRelevant environment variables:"
	    foreach e $predefinedEnvVars {
		append ret "\n  $e = $env($e)"
	    }
	}
	if {0 && [info exists tcl_platform(threaded)]} {
	    append ret "\nWARNING: Your installation of Tcl appears to be\
		thread-enabled.\nOOMMF does not support thread-enabled\
		Tcl.  If you have trouble with\nOOMMF, try installing\
		non-thread-enabled Tcl."
	}
	if {0 && ![catch {$this GetValue TK_DEFS} defs]} {
	    if {[string match *TCL_THREADS=1* $defs]} {
		append ret "\nWARNING: Your installation of Tk appears to be\
			thread-enabled.\nOOMMF does not support thread-enabled\
			operations.  If you have\ntrouble with\
			OOMMF, try installing non-thread-enabled Tk."
	    }
	}
	if {![info exists env(OOMMF_TCL_INCLUDE_DIR)]} {
	    append ret "\nWARNING: Your installation of Tcl appears\
		to be missing the header\nfile <tcl.h>.  Perhaps you\
		need to re-install Tcl, requesting a full\ninstallation\
		this time, or install the developers package for\
		Tcl?\nIf you are sure a Tcl header file is installed on\
		your system, then\nset the environment variable\
		OOMMF_TCL_INCLUDE_DIR to the directory\nthat contains\
		the Tcl header file."
	}
	if {![info exists env(OOMMF_TK_INCLUDE_DIR)]} {
	    append ret "\nWARNING: Your installation of Tk appears\
		to be missing the header\nfile <tk.h>.  Perhaps you\
		need to re-install Tk, requesting a full\ninstallation\
		this time, or install the developers package for\
		Tk?\nIf you are sure a Tk header file is installed on\
		your system, then\nset the environment variable\
		OOMMF_TK_INCLUDE_DIR to the directory\nthat contains\
		the Tk header file."
	}
	set code [catch {socket -server Oc_Accept -myaddr localhost 0} s]
	if {$code} {
	    append ret "\nWARNING: It appears that your computer does\
		    not have networking software enabled to support\
		    Internet (TCP/IP) communications.  Your computer\
		    does not have to be connected to the Internet to\
		    run OOMMF, but it does need basic networking\
		    capabilities installed."
	} else {
	    set code [catch {
		global Oc_Done
		proc Oc_Accept {args} {global Oc_Done; set Oc_Done 0}
		proc Oc_Fail {} {global Oc_Done; set Oc_Done 1}
		set c [socket -async localhost \
			[lindex [fconfigure $s -sockname] 2]]
		set e [after 15000 Oc_Fail]
		vwait Oc_Done
		after cancel $e
		catch {close $c}
		rename Oc_Fail {}
		rename Oc_Accept {}
		if {$Oc_Done} {
		    error Fail
		}
	    } errmsg]
	    unset Oc_Done
	    if {$code} {
		append ret "\nWARNING: Although it appears your computer\
			has networking software enabled, it is not\
			permitting socket communications.  OOMMF uses\
			socket communications and cannot run without them.\
			Perhaps you have a misconfigured firewall running?"
               append ret "\nERROR MESSAGE: $errmsg"
	    }
	}
	catch {close $s}
   


	if {[string match *WARNING* $ret]} {
	    append ret "\n\nPlease see the Installation section of the\
		    OOMMF User's Guide\nfor more information."
	}
	return $ret
    }

    private method Detect {} {detectScript} {
        eval $detectScript
    }

    private method RecordStandardWindowsTclTkConfiguration {root} {
	# Note that a "standard" Tcl/Tk configuration on Windows
	# is one where TCL_PREFIX, TK_PREFIX, TCL_EXEC_PREFIX,
	# and TK_EXEC_PREFIX are all the same directory, $root.
	#
	# There are reportedly other variations of installation on
	# Windows (TclPro?), but the best answer to bug report
	# concerning them is probably to advise use of something
	# like ActiveTcl that follows the "standard".
	#
	# Turn "Program Files" into Progra~1 -- This is technically
	# invalid, but no bug reports on it yet.
	set pathlist [list]
	foreach p [file split $root] {
	    if {[regexp " " $p]} {
		lappend pathlist [string range $p 0 5]~1
	    } else {
		lappend pathlist $p
	    }
	}
	set root [eval file join $pathlist]
	$this SetValue TCL_EXEC_PREFIX $root
	$this SetValue TCL_PREFIX $root
	$this SetValue TK_EXEC_PREFIX $root
	$this SetValue TK_PREFIX $root
	$this SetValue TK_XINCLUDES [file join $root include X11]
	regexp {^([0-9]+)\.([0-9]+)} [package provide Tcl] dummy tlma tlmi
	regexp {^([0-9]+)\.([0-9]+)} [$class TkVersion] dummy tkma tkmi
	$this SetValue TCL_VERSION [info tclversion]
	$this SetValue TCL_MAJOR_VERSION $tlma
	$this SetValue TCL_MINOR_VERSION $tlmi
	regsub {^[0-9]+\.[0-9]+} [info patchlevel] {} pl
	$this SetValue TCL_PATCH_LEVEL $pl
	$this SetValue TK_VERSION [$class TkVersion]
	$this SetValue TK_MAJOR_VERSION $tkma
	$this SetValue TK_MINOR_VERSION $tkmi
	if {[string length [package provide Tk]]} {
	    global tk_patchLevel
	    regsub {^[0-9]+\.[0-9]+} $tk_patchLevel {} pl
	}
	$this SetValue TK_PATCH_LEVEL $pl
	if {[regexp {^8\.0([.p]([0-9]+))?$} [info patchlevel] m mm patch]
		&& ($patch < 4)} {
	    set vc vc
	} else {
	    set vc ""
	}
	# The following are relative to TCL_EXEC_PREFIX, thus $root
	# in our "standard" Tcl/Tk install on Windows.
	$this SetValue TCL_VC_LIB_SPEC [file join $root lib \
		tcl$tlma$tlmi$vc.lib]
	$this SetValue TK_VC_LIB_SPEC [file join $root lib \
		tk$tkma$tkmi$vc.lib]
	# Add other *_LIB_SPEC (f.e. Borland, Watcom, ...) on request.
	# Compiler-specific configurations (f.e. TCL_LIBS) must be
	# completed in the config/platforms/*.tcl file.
    }

    private method RecordTclTkConfiguration {} {
	# Record values for a set of features which describe the
	# Tcl/Tk configuration.
	#
	global env tcl_platform
	# If there's a Windows registry available to us, and it contains
	# an entry holding the Tcl/Tk installation root, use it.
	if {[string match windows $tcl_platform(platform)]
		&& (![info exists env(OSTYPE)]
			|| ![string match cygwin* $env(OSTYPE)])
		&& ![catch {package require registry}]} {
	    # Registry available
	    set pfx [join {HKEY_LOCAL_MACHINE SOFTWARE} \\]
	    if {![catch {registry get [join [list $pfx Scriptics Tcl \
		    [package provide Tcl]] \\] Root} result]
		    || ![catch {registry get [join [list $pfx Scriptics Tcl \
		    [package provide Tcl]] \\] {}} result]
		    || ![catch {registry get [join [list $pfx Sun Tcl \
		    [package provide Tcl]] \\] Root} result]} {
		# Since registry entry is set, assume the standard Tcl/Tk
		# installation on Windows established by the installer
		# program from Sun/Scriptics or a clone.
		$this RecordStandardWindowsTclTkConfiguration $result
		return
	    } elseif {![info exists env(OOMMF_TCL_CONFIG)]
		    && ![info exists env(OOMMF_TK_CONFIG)]} {
		# We have a working registry package, so we're on Windows,
		# and the user isn't telling us to look elsewhere with
		# environment variables, but there's no entry in the
		# registry pointing to the Tcl/Tk root, so assume it's a
		# proper installation relative to [info library]
		set result [file dirname [file dirname [Oc_DirectPathname \
			[info library]]]]
		$this RecordStandardWindowsTclTkConfiguration $result
		return
	    }
	}
	# No registry information.  Look for tclConfig.sh.
	if {[info exists env(OOMMF_TCL_CONFIG)]} {
	    # Either the user or a parent app is trying to tell us
	    # what tclConfig.sh file to use.
	    if {![string match absolute \
		    [file pathtype $env(OOMMF_TCL_CONFIG)]]} {
		set msg "Error in environment variable:\nOOMMF_TCL_CONFIG =\
			$env(OOMMF_TCL_CONFIG)\nMust be absolute pathname"
		error $msg $msg
	    }
	    if {![file readable $env(OOMMF_TCL_CONFIG)]} {
		set msg "Error in environment variable:\nOOMMF_TCL_CONFIG =\
			$env(OOMMF_TCL_CONFIG)\nFile not readable"
		error $msg $msg
	    }
	} else {
	    catch {set env(OOMMF_TCL_CONFIG) [Oc_DirectPathname \
		    [$this FindTclConfig]]} msg
	}
	if {[info exists env(OOMMF_TCL_CONFIG)]} {
	    $this LoadConfigFile $env(OOMMF_TCL_CONFIG)
	    # Warn if we got a config file for a different Tcl release
	    if {[catch {$this GetValue TCL_PATCH_LEVEL} pl]
		    || [string match {} $pl]} {
		# No TCL_PATCH_LEVEL definition -- check TCL_VERSION only
		if {![string match [package provide Tcl] \
			[$this GetValue TCL_VERSION]]} {
		    global errorInfo
		    set errorInfo [Oc_StackTrace]
		    Oc_Log Log "Tcl version\
			    mismatch:\n\t$env(OOMMF_TCL_CONFIG) from\
			    [$this GetValue TCL_VERSION]\n\tRunning Tcl\
			    [package provide Tcl]" warning $class 
		}
            } else {
		# TCL_PATCH_LEVEL defined -- check it
		if {![string match [info patchlevel] \
			[$this GetValue TCL_VERSION]$pl]} {
		    global errorInfo
		    set errorInfo [Oc_StackTrace]
		    Oc_Log Log "Tcl version\
			    mismatch:\n\t$env(OOMMF_TCL_CONFIG) from\
			    [$this GetValue TCL_VERSION]$pl\n\tRunning Tcl\
			    [info patchlevel]" warning $class 
		}

	    }
        }
	# ...and look for tkConfig.sh
	if {[info exists env(OOMMF_TK_CONFIG)]} {
	    # Either the user or a parent app is trying to tell us
	    # what tkConfig.sh file to use.
	    if {![string match absolute \
		    [file pathtype $env(OOMMF_TK_CONFIG)]]} {
		set msg "Error in environment variable:\nOOMMF_TK_CONFIG =\
			$env(OOMMF_TK_CONFIG)\nMust be absolute pathname"
		error $msg $msg
	    }
	    if {![file readable $env(OOMMF_TK_CONFIG)]} {
		set msg "Error in environment variable:\nOOMMF_TK_CONFIG =\
			$env(OOMMF_TK_CONFIG)\nFile not readable"
		error $msg $msg
	    }
	} else {
	    catch {set env(OOMMF_TK_CONFIG) [Oc_DirectPathname \
		    [$this FindTkConfig]]}
	}
	if {[info exists env(OOMMF_TK_CONFIG)]} {
	    $this LoadConfigFile $env(OOMMF_TK_CONFIG)
	    # Warn if we got a config file for a different Tk release
	    if {[string length [package provide Tk]]
		    && ![catch {$this GetValue TK_PATCH_LEVEL} pl]
		    && ![string match {} $pl]} {
		# Check that patch levels match
		global tk_patchLevel
		if {![string match $tk_patchLevel \
			[$this GetValue TK_VERSION]$pl]} {
		    global errorInfo
		    set errorInfo [Oc_StackTrace]
		    Oc_Log Log "Tk version\
			    mismatch:\n\t$env(OOMMF_TK_CONFIG) from\
			    [$this GetValue TK_VERSION]$pl\n\tRunning Tk\
			    $tk_patchLevel" warning $class 
		}
            } else {
		if {![string match [$class TkVersion] \
			[$this GetValue TK_VERSION]]} {
		    global errorInfo
		    set errorInfo [Oc_StackTrace]
		    Oc_Log Log "Tk version\
			    mismatch:\n\t$env(OOMMF_TK_CONFIG) from\
			    [$this GetValue TK_VERSION]\n\tRunning Tk\
			    [$class TkVersion]" warning $class 
		}
	    }
        }
	if {[string match windows $tcl_platform(platform)]
		&& (![info exists env(OSTYPE)]
			|| ![string match cygwin* $env(OSTYPE)])
		&& (![info exists env(TERM)] 
		    || ![string match cygwin $env(TERM)])} {
	    set root [$this GetValue TCL_PREFIX]
	    set tlma [$this GetValue TCL_MAJOR_VERSION]
	    set tlmi [$this GetValue TCL_MINOR_VERSION]
	    set tkma [$this GetValue TK_MAJOR_VERSION]
	    set tkmi [$this GetValue TK_MINOR_VERSION]
	    if {[regexp {^8\.0([.p]([0-9]+))?$} [info patchlevel] m mm patch]
		&& ($patch < 4)} {
		set vc vc
	    } else {
		set vc ""
	    }
	    $this SetValue TCL_VC_LIB_SPEC [file join $root lib \
						tcl$tlma$tlmi$vc.lib]
	    $this SetValue TK_VC_LIB_SPEC [file join $root lib \
					       tk$tkma$tkmi$vc.lib]
	}
    }

    private method CheckTclIncludeVersion { filename } {
       # Import is the name of a tcl.h file.
       # Returns 1 if a TCL_VERSION record can be identified inside the
       # file and the version string matches [info tclversion]
       # Otherwise return 0.  Also returns 0 if the file cannot be
       # opened for reading.
       if {[catch {open $filename} f]} {
          return 0  ;# Can't open file
       }
       gets $f line
       while {![eof $f] && ![string match "#define*TCL_VERSION*" $line]} {
          gets $f line	    
       }
       close $f
       if {![regexp {TCL_VERSION[^0-9.]*([0-9.]+)[^0-9.]*$} \
                $line dummy verno]} {
          # Bad record format
          return 0
       }
       if {[string compare [info tclversion] $verno]!=0} {
          return 0  ;# Mismatch
       }
       return 1 ;# Version strings match
    }

    private method CheckTkIncludeVersion { filename } {
       # Import is the name of a tk.h file.
       # Returns 1 if a TK_VERSION record can be identified inside the
       # file and the version string matches [info tclversion]
       # Otherwise return 0.  Also returns 0 if the file cannot be
       # opened for reading.
       if {[catch {open $filename} f]} {
          return 0  ;# Can't open file
       }
       gets $f line
       while {![eof $f] && ![string match "#define*TK_VERSION*" $line]} {
          gets $f line	    
       }
       close $f
       if {![regexp {TK_VERSION[^0-9.]*([0-9.]+)[^0-9.]*$} \
                $line dummy verno]} {
          # Bad record format
          return 0
       }
       set checkver [info tclversion]
       if {$checkver < 8.0} {
          set checkver [format "%g" [expr {$checkver - 3.4}]]
          ## Round slightly
       }
       if {[string compare $checkver $verno]!=0} {
          return 0  ;# Mismatch
       }
       return 1 ;# Version strings match
    }

    private method FindTclTkIncludes {} {
       global env tcl_platform tcl_version
       if {[info exists env(OOMMF_TCL_INCLUDE_DIR)]} {
          if {![file readable \
                   [file join $env(OOMMF_TCL_INCLUDE_DIR) tcl.h]]} {
             set msg "Error in environment\
			variable:\nOOMMF_TCL_INCLUDE_DIR =\
			$env(OOMMF_TCL_INCLUDE_DIR)\ntcl.h\
			not found there, or not readable"
             error $msg $msg
          }
       } else {
          set search [list]
          if {[catch {::tcl::pkgconfig get includedir,runtime} idir] == 0} {
             lappend search $idir
          }
          if {[string match Darwin $tcl_platform(os)]} {
             lappend search /System/Library/Frameworks/Tcl.framework/Versions/$tcl_version/Headers
             lappend search /Library/Frameworks/Tcl.framework/Versions/$tcl_version/Headers
             lappend search /System/Library/Frameworks/Tcl.framework/Headers
             lappend search /Library/Frameworks/Tcl.framework/Headers
          }
          if {![catch {$this GetValue TCL_INC_DIR} incdir]} {
             lappend search $incdir
          }
          if {![catch {$this GetValue TCL_PREFIX} tp]} {
             lappend search [file join $tp include]
             lappend search \
                [file join $tp include tcl[$this GetValue TCL_VERSION]]
          }
          foreach dir $search {
             set ifile [file join $dir tcl.h]
             if {[file readable $ifile] \
                    && [$this CheckTclIncludeVersion $ifile]} {
                set env(OOMMF_TCL_INCLUDE_DIR) $dir
                break
             }
          }
       }
       if {[info exists env(OOMMF_TK_INCLUDE_DIR)]} {
          if {![file readable \
                   [file join $env(OOMMF_TK_INCLUDE_DIR) tk.h]]} {
             set msg "Error in environment\
			variable:\nOOMMF_TK_INCLUDE_DIR =\
			$env(OOMMF_TK_INCLUDE_DIR)\ntk.h\
			not found there, or not readable"
             error $msg $msg
          }
       } else {
          set search [list]
          if {[catch {::tcl::pkgconfig get includedir,runtime} idir] == 0} {
             lappend search $idir
          }
          if {[string match Darwin $tcl_platform(os)]} {
             lappend search /System/Library/Frameworks/Tk.framework/Versions/$tcl_version/Headers
             lappend search /Library/Frameworks/Tk.framework/Versions/$tcl_version/Headers
             lappend search /System/Library/Frameworks/Tk.framework/Headers
             lappend search /Library/Frameworks/Tk.framework/Headers
          }
          if {![catch {$this GetValue TK_INC_DIR} incdir]} {
             lappend search $incdir
          }
          if {![catch {$this GetValue TK_PREFIX} tp]} {
             lappend search [file join $tp include]
             lappend search \
                [file join $tp include tk[$this GetValue TK_VERSION]]
             lappend search \
                [file join $tp include tcl[$this GetValue TCL_VERSION]]
          }
          foreach dir $search {
             set ifile [file join $dir tk.h]
             if {[file readable $ifile] \
                    && [$this CheckTkIncludeVersion $ifile]} {
                set env(OOMMF_TK_INCLUDE_DIR) $dir
                break
             }
          }
       }
    }

    private method CheckTclConfigVersion { filename } {
       # Import is the name of a tclConfig.sh file.
       # Returns 1 if a TCL_VERSION record can be identified inside the
       # file and the version string matches [info tclversion]
       # Otherwise return 0.  Also returns 0 if the file cannot be
       # opened for reading.
       if {[catch {open $filename} f]} {
          return 0  ;# Can't open file
       }
       gets $f line
       while {![eof $f] && ![string match "TCL_VERSION=*" $line]} {
          gets $f line	    
       }
       close $f
       if {![regexp {^TCL_VERSION=[^0-9.]*([0-9.]+)[^0-9.]*$} \
                $line dummy verno]} {
          # Bad record format
          return 0
       }
       if {[string compare [info tclversion] $verno]!=0} {
          return 0  ;# Mismatch
       }
       return 1 ;# Version strings match
    }

    private method FindTclConfig {} {
       global tcl_pkgPath tcl_platform tcl_version
       set searchdirs {}
       # In Tcl 8.5 and later, libdir,runtime is first place to look
       if {[catch {::tcl::pkgconfig get libdir,runtime} libdir] == 0} {
          lappend searchdirs $libdir
       }
       # If tcl_pkgPath isn't broken :( ...
       if {[info exists tcl_pkgPath]} {
          set searchdirs [concat $searchdirs $tcl_pkgPath]
       }
       # Some systems (notably Debian) modify Tcl installations to
       # store tclConfig.sh in [info library], apparently so as to
       # avoid conflict between multiple Tcl versions.  Check parent
       # directory of this too.
       lappend searchdirs [info library] [file dirname [info library]]

       # Look
       foreach libdir $searchdirs {
          set cf [file join $libdir tclConfig.sh]
          if {[file readable $cf] && [$this CheckTclConfigVersion $cf]} {
             return $cf
          }
       }
       # No joy?  Check for tcl* subdirectories
       if {![regexp {^([^.]*(.[^.]*|$))} [package provide Tcl] dummy majmin]} {
          set majmin [info tclversion]
       }
       foreach {tclmajor tclminor} [split $majmin "."] break
       foreach libdir $searchdirs {
          foreach trydir [glob -nocomplain [file join $libdir tcl*]] {
             if {[file isdirectory $trydir]} {
                if {[regexp {^tcl[^0-9]*([0-9]+)[^0-9]*([0-9]*)} \
                        [file tail $trydir] dummy major minor]} {
                   if {$tclmajor != $major} { break }
                   if {[string match {} $minor] || $tclminor == $minor} {
                      set cf [file join $trydir tclConfig.sh]
                      if {[file readable $cf] \
                             && [$this CheckTclConfigVersion $cf]} {
                         return $cf
                      }
                   }
                } else {
                   # No numbers embedded in directory name.  Try anyway?!
                   set cf [file join $trydir tclConfig.sh]
                   if {[file readable $cf] \
                          && [$this CheckTclConfigVersion $cf]} {
                      return $cf
                   }
                }
             }
          }
       }

       # The framework installs of Tcl/Tk on Mac OSX are different from
       # anything else:
       if {[string match Darwin $tcl_platform(os)]} {
          set chklist {}
          foreach elt $searchdirs {
             if {[regexp {^(.*)/Resources/Scripts} $elt dummy prefix]} {
                lappend chklist $prefix
             } else {
                lappend chklist $elt
             }
          }
          lappend chklist \
             /System/Library/Frameworks/Tcl.framework/Versions/$tcl_version \
             /Library/Frameworks/Tcl.framework/Versions/$tcl_version \
             /System/Library/Frameworks/Tcl.framework \
             /Library/Frameworks/Tcl.framework
          foreach libdir $chklist {
             set cf [file join $libdir tclConfig.sh]
             if {[file readable $cf] \
                    && [$this CheckTclConfigVersion $cf]} {
                return $cf
             }
          }
       }

       # Try looking relative to tclsh executable
       set exec [file tail [info nameofexecutable]]
       # If we're running tclsh...
       if {[regexp -nocase tclsh $exec]} {
          set cf [$this RelToExec [info nameofexecutable] tclConfig.sh]
          if {[file readable $cf] && [$this CheckTclConfigVersion $cf]} {
             return $cf
          }
       }
       # Go searching for tclsh...
       set candshells [list]
       if {[regsub -nocase wish [file tail $exec] tclsh shell]} {
          lappend candshells [file join [file dirname $exec] $shell]
          lappend candshells $shell
       }
       if {[string compare $majmin [package provide Tcl]]!=0} {
          lappend candshells tclsh[package provide Tcl]
          lappend candshells tclsh[join [split [package provide Tcl] .] ""]
       }
       lappend candshells tclsh$majmin
       lappend candshells tclsh[join [split $majmin .] ""]
       lappend candshells tclsh ;# Last resort
       foreach shell $candshells {
          set shell [auto_execok $shell]
          if {[llength $shell]} {
             set cf [$this RelToExec [lindex $shell 0] tclConfig.sh]
             if {[file readable $cf] && [$this CheckTclConfigVersion $cf]} {
                return $cf
             }
          }
       }
       return -code error "tclConfig.sh not found"
    }

    private method CheckTkConfigVersion { filename } {
       # Import is the name of a tkConfig.sh file.
       # Returns 1 if a TK_VERSION record can be identified inside the
       # file and the version string matches [info tclversion]
       # Otherwise return 0.  Also returns 0 if the file cannot be
       # opened for reading.
       if {[catch {open $filename} f]} {
          return 0  ;# Can't open file
       }
       set f [open $filename]
       gets $f line
       while {![eof $f] && ![string match "TK_VERSION=*" $line]} {
          gets $f line	    
       }
       close $f
       if {![regexp {^TK_VERSION=[^0-9.]*([0-9.]+)[^0-9.]*$} \
                $line dummy verno]} {
          # Bad record format
          return 0
       }
       set checkver [info tclversion]
       if {$checkver < 8.0} {
          set checkver [format "%g" [expr {$checkver - 3.4}]]
          ## Round slightly
       }
       if {[string compare $checkver $verno]!=0} {
          return 0  ;# Mismatch
       }
       return 1 ;# Version strings match
    }

    private method FindTkConfig {} {
       # If we found tclConfig.sh, check for tkConfig.sh in same or
       # parallel location
       global env
       if {[info exists env(OOMMF_TCL_CONFIG)]} {
          set cdir [file dirname $env(OOMMF_TCL_CONFIG)]
          lappend checkdirs $cdir
          # On Darwin Framework builds, tkConfig.sh is in a parallel directory
          if {[regsub {Tcl\.framework} $cdir Tk.framework td]} {
             lappend checkdirs $td
          }
          # If last component in cdir has form "tcl*", then replace
          # "tcl" with "tk"
          set lc [file tail $cdir]
          if {[string match tcl* $lc]} {
             regsub {^tcl} $lc tk lc
             lappend checkdirs [file join [file dirname $cdir] $lc]
          }
          foreach td $checkdirs {
             set cf [file join $td tkConfig.sh]
             if {[file readable $cf] && [$this CheckTkConfigVersion $cf]} {
                return $cf
             }
          }
       }
       # Try TCL_EXEC_PREFIX too
       if {![catch {$this GetValue TCL_EXEC_PREFIX} epfx]} {
          set cf [$this RelToDir $epfx \
                     [file join tk[$class TkVersion] tkConfig.sh]]
          if {[file readable $cf] && [$this CheckTkConfigVersion $cf]} {
             return $cf
          }
       }

       # Otherwise, hunt around as in FindTclConfig
       global tcl_pkgPath tcl_platform tcl_version
       set searchdirs {}
       # In Tcl 8.5 and later, libdir,runtime is first place to look
       if {[catch {::tcl::pkgconfig get libdir,runtime} libdir] == 0} {
          lappend searchdirs $libdir
       }
       # If tcl_pkgPath isn't broken :( ...
       if {[info exists tcl_pkgPath]} {
          set searchdirs [concat $searchdirs $tcl_pkgPath]
       }
       # Some systems (notably Debian) modify Tcl installations to
       # store config files in [info library], apparently so as to
       # avoid conflict between multiple Tcl versions.  Check parent
       # directory of this too.
       lappend searchdirs [info library] [file dirname [info library]]
       # Some systems (notably Debian) modify Tk installations to
       # store tkConfig.sh in $::tk_library, apparently so as to
       # avoid conflict between multiple Tk versions.
       global tk_library
       if {[info exists tk_library]} {
          lappend searchdirs $tk_library [file dirname $tk_library]
       }

       # Look
       foreach libdir $searchdirs {
          set cf [file join $libdir tkConfig.sh]
          if {[file readable $cf] && [$this CheckTkConfigVersion $cf]} {
             return $cf
          }
       }
       # No joy?  Check for tcl* subdirectories
       if {![regexp {^([^.]*(.[^.]*|$))} [package provide Tcl] dummy majmin]} {
          set majmin [info tclversion]
       }
       foreach {tclmajor tclminor} [split $majmin "."] break
       foreach libdir $searchdirs {
          foreach trydir [glob -nocomplain [file join $libdir tk*] \
                             [file join $libdir tcl*]] {
             if {[file isdirectory $trydir]} {
                if {[regexp {^(tcl|tk)[^0-9]*([0-9]+)[^0-9]*([0-9]*)} \
                        [file tail $trydir] dummy prefix major minor]} {
                   if {$tclmajor != $major} { break }
                   if {[string match {} $minor] || $tclminor == $minor} {
                      set cf [file join $trydir tkConfig.sh]
                      if {[file readable $cf] \
                             && [$this CheckTkConfigVersion $cf]} {
                         return $cf
                      }
                   }
                } else {
                   # No numbers embedded in directory name.  Try anyway?!
                   set cf [file join $trydir tkConfig.sh]
                   if {[file readable $cf] \
                          && [$this CheckTkConfigVersion $cf]} {
                      return $cf
                   }
                }
             }
          }
       }

       # The framework installs of Tcl/Tk on Mac OSX are different from
       # anything else:
       if {[string match Darwin $tcl_platform(os)]} {
          set chklist {}
          foreach elt $searchdirs {
             if {[regexp {^(.*)/Resources/Scripts} $elt dummy prefix]} {
                lappend elt $prefix
             }
             regsub {Tcl\.framework} $elt Tk.framework elt
             lappend chklist $elt
          }
          lappend chklist \
             /System/Library/Frameworks/Tk.framework/Versions/$tcl_version \
             /Library/Frameworks/Tk.framework/Versions/$tcl_version \
             /System/Library/Frameworks/Tk.framework \
             /Library/Frameworks/Tk.framework
          foreach libdir $chklist {
             set cf [file join $libdir tkConfig.sh]
             if {[file readable $cf] && [$this CheckTkConfigVersion $cf]} {
                return $cf
             }
          }
       }

       # If we're running wish...
       set exec [info nameofexecutable]
       catch {set exec [Oc_ResolveLink $exec]}
       set exec [Oc_DirectPathname $exec]
       if {[regexp -nocase wish [file tail $exec]]} {
          # This check wrt the absolute, resolved path.  There
          # is a check farther below wrt just the tail.
          set cf [$this RelToExec $exec tkConfig.sh]
          if {[file readable $cf] && [$this CheckTkConfigVersion $cf]} {
             return $cf
          }
       }
       # Not in usual places, go searching for wish...
       set candshells [list]
       if {[regsub -nocase tclsh [file tail $exec] wish shell]} {
          lappend candshells [file join [file dirname $exec] $shell]
          lappend candshells $shell
       }
       if {[string compare $majmin [$class TkVersion]]!=0} {
          lappend candshells wish[$class TkVersion]
          lappend candshells wish[join [split [$class TkVersion] .] ""]
       }
       lappend candshells wish$majmin
       lappend candshells wish[join [split $majmin .] ""]
       if {[regexp -nocase wish [file tail $exec]]} {
          lappend candshells [file tail $exec]
       }
       lappend candshells wish ;# Last resort
       foreach shell $candshells {
          set shell [auto_execok $shell]
          if {[llength $shell]} {
             set checkexec [lindex $shell 0]
             catch {set checkexec [Oc_ResolveLink $checkexec]}
             set cf [$this RelToExec $checkexec tkConfig.sh]
             if {[file readable $cf] && [$this CheckTkConfigVersion $cf]} {
                return $cf
             }
          }
       }
       return -code error "tkConfig.sh not found"
    }

    private method RelToDir {dir cf} {
	global tcl_platform
	if {![info exists tcl_platform(wordSize)]} {
	    set liblist [list lib lib64]
	} else {
	    # If wordSize is 8, then this is a 64-bit build of Tcl, so try
	    # lib64 first.  Some platforms support both 32 and 64-bit
	    # binaries, and use lib for 32-bit libraries, lib64 for 64-bit
	    # libraries.  Other platforms just put all libraries into lib.
	    if {$tcl_platform(wordSize) == 8} {
		set liblist lib64
	    }
	    lappend liblist lib
	}
	foreach lib $liblist {
	    set test_cf [file join $dir $lib $cf]
	    if {[file readable $test_cf]} { break }
	}
	return $test_cf
    }

    private method RelToExec {exe cf} {
        set exe [file dirname [Oc_DirectPathname [file dirname $exe]]]
        return [$this RelToDir $exe $cf]
    }

    private method LoadConfigFile {fn} {
	set f [open $fn r]
	gets $f line
	while {![eof $f]} {
	    if {[regexp {^([A-Z0-9_]+)='([^']*)'} $line _ feat val]
	    || [regexp "^(\[A-Z0-9_]+)=(\[^ \t\r\n]*)" $line _ feat val]} {
		# Handle (some) shell variable substitutions
		regsub -all {\\} $val {\\\\} val
		regsub -all {\[} $val {\\[} val
		regsub -all {\${(T(CL|K)_[A-Z_]+)}} $val \
			{[$this GetValue "\1"]} val
		set val [subst -novariables $val]
		$this SetValue $feat $val
	    }
	    gets $f line
	}
	close $f
    }

    private method LoadCache {} {
        set fn [file join $cacheDir [string tolower \
                [$this GetValue platform_name]].tcl]
        if {[file readable $fn]} {
            if {[catch {uplevel #0 [list source $fn]} msg]} {
                Oc_Log Log "Error (2) sourcing $fn:\n\t$msg" warning $class
            }
        }
    }

}

