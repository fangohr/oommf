# FILE: config.tcl
#
# Last modified on: $Date: 2016/01/30 01:12:25 $
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

        # Default thread options.  These can be changed in the
        # platform specific files under oommf/config/platforms/local/
        if {[catch {$runPlatform GetValue oommf_threads}]} {
           # Default is to follow Tcl value, provided the Tcl version is
           # 8.3 or later.  (Much of the Tcl library thread support API,
           # e.g., Tcl_CreateThread, first appears in Tcl 8.3.)
           global tcl_platform
           foreach {major minor} [split [info tclversion] .] break
           if {[info exists tcl_platform(threaded)] \
                  && $tcl_platform(threaded) \
                  && ($major>8 || ($major==8 && $minor>2))} {
              $runPlatform SetValue oommf_threads 1  ;# Do threads
           } else {
              $runPlatform SetValue oommf_threads 0  ;# No threads
           }
        }

        # Check to see if we are in the midst of building OOMMF.
        # Default is no.
        if {[catch {set env(OOMMF_BUILD_ENVIRONMENT_NEEDED)} \
                build_environment_needed]} {
           set build_environment_needed 0
        }

        if {$build_environment_needed} {
           $class DetermineBuildEnvironment
        } else {
           $runPlatform LoadCache
        }
        return

    }

    proc DetermineBuildEnvironment {} {
       global env
       # Determine Tcl and Tk layout.  The oommf/config/platform/* file
       # presumably hasn't been read yet, so we don't know if the user
       # has requested a no-Tk build or not.  Look for Tk regardless
       # (the TK_LIB config value is referenced by the generic platform
       # template), and dump results later if use_tk is zero.
       $runPlatform RecordTclTkConfiguration
       $runPlatform FindTclTkIncludes 

       # Once the config/platform file has been read, we can check for
       # cross-compiling and use_tk settings and adjust appropriately.
       $runPlatform LoadCache
       if {[catch {$runPlatform GetValue use_tk} use_tk]} {
          # Default is to build with Tk
          $runPlatform SetValue use_tk [set use_tk 1]
       }
       if {[llength [$runPlatform Features cross_compile*]]>0} {
          if {[catch {$runPlatform GetValue cross_compile} cross_compile]} {
             $runPlatform SetValue cross_compile [set cross_compile 1]
          }
          # Feature "cross_compile" is a master switch; if 0 then all
          # cross_compiler options (including itself) are removed.
          # OTOH, if cross_compile is undefined, then the existence of
          # any other cross_compile* feature defines cross_compile to
          # true.
          if {!$cross_compile} {
             foreach elt [$runPlatform Features cross_compile*] {
                $runPlatform UnsetValue $elt
             }
          }
       }
       if {![catch {$runPlatform GetValue cross_compile_host_tcl_config} \
                tclconfig]} {
          # Cross-compiling in effect; get values from specified
          # tclConfig.sh file.  Don't choke if tclConfig.sh doesn't
          # exist --- this may just mean that we're running on the
          # target machine rather than the build host.
          if {![file exists $tclconfig]} {
             catch {unset env(OOMMF_TCL_CONFIG)}
          } else {
             $runPlatform LoadConfigFile [set env(OOMMF_TCL_CONFIG) $tclconfig]
          }
       }
       if {![catch {$runPlatform GetValue cross_compile_host_tcl_include_dir} \
                tclincludedir]} {
          set env(OOMMF_TCL_INCLUDE_DIR) $tclincludedir
       }
       if {!$use_tk} {
          # Non-Tk build.  Fill in dummy values.
          catch {unset env(OOMMF_TK_CONFIG)}
          catch {unset env(OOMMF_TK_INCLUDE_DIR)}
          catch {unset env(TK_LIBRARY)}
          $runPlatform SetValue TK_VERSION "0.0"
          $runPlatform SetValue TK_MAJOR_VERSION 0
          $runPlatform SetValue TK_MINOR_VERSION 0
          $runPlatform SetValue TK_PATCH_LEVEL ".0"
          $runPlatform SetValue TK_RELEASE_LEVEL 0
          $runPlatform SetValue TK_RELEASE_SERIAL 0
       } else {
          if {![catch {$runPlatform GetValue cross_compile_host_tk_config} \
                   tkconfig]} {
             # Cross-compiling in effect; get values from specified
             # tkConfig.sh file.  Don't choke if tkConfig.sh doesn't
             # exist --- this may just mean that we're running on the
             # target machine rather than the build host.
             if {![file exists $tkconfig]} {
                catch {unset env(OOMMF_TK_CONFIG)}
             } else {
                $runPlatform LoadConfigFile \
                   [set env(OOMMF_TK_CONFIG) $tkconfig]
             }
          }
          if {![catch {$runPlatform GetValue cross_compile_host_tk_include_dir} \
                   tkincludedir]} {
             set env(OOMMF_TK_INCLUDE_DIR) $tkincludedir
          }
       }

       # If Tcl and Tk were installed under different --prefix directories
       # the OOMMF applications will need help to find the Tk script library
       if {$use_tk && ![info exists env(TK_LIBRARY)]
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

    method UnsetValue {feature} {
       catch {unset values($feature)}
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
       return {}
    }

    method OommfApiIndex {} {
       # Date of most recent OOMMF API change.  Intended for use
       # primarily by external extension writers.  This value is
       # echoed in the ocport.h header by the Oc_MakePortHeader proc
       # in oc/procs.tcl. Format is "yyyymmdd".
       return 20181207
    }

    method CrossCompileSummary {} {
       global tcl_platform env
       set ret "Platform Name:\t\t$name (cross-compile)\n"
       append ret "C++ compiler:   \t"
       set compiler_found 0
       if {![catch {$this GetValue program_compiler_c++_name} cn]} {
          append ret "$cn"
          set compiler_found 1
       } elseif {![catch {$this GetValue program_compiler_c++} c]} {
          set compilertest [lindex $c 0]
          set wd [auto_execok $compilertest]
          if {![string match {} $wd]} {
             append ret "$wd"
             set compiler_found 1
          } else {
             append ret "not found - \"[lindex $c 0]\""
          }
       } else {
          append ret "none selected"
       }
       append ret "\n"
       if {$compiler_found} {
          if {![catch {$this GetValue program_compiler_c++_banner_cmd} \
                   bcmd]} {
             if {![catch {eval $bcmd} banner] && [string length $banner]} {
                append ret " Version string:\t $banner\n"
             }
          }
          if {![catch {$this GetValue program_compiler_c++_cpu_arch} arch]} {
             append ret " Compiler target arch:\t $arch\n"
          }
       }
       append ret "Shell details ---\n"
       set noe [Oc_DirectPathname [info nameofexecutable]]
       append ret " tclsh (host):        \t$noe\n"

       append ret " tclConfig.sh:        \t"
       if {[info exists env(OOMMF_TCL_CONFIG)]} {
          append ret "$env(OOMMF_TCL_CONFIG)\n"
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
       } else {
          append ret "not found\n"
       }
       if {[catch {$this GetValue use_tk} use_tk]} {
          set use_tk 1  ;# Default is to build with Tk
       }
       append ret " tkConfig.sh:         \t"
       if {!$use_tk} {
          append ret "Build without Tk requested.\n"
       } else {
          if {[info exists env(OOMMF_TK_CONFIG)]} {
             append ret "$env(OOMMF_TK_CONFIG)\n"
          } else {
             append ret "not found\n"
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
       }

        append ret    "\nOOMMF threads:         \t"
        if {[$this GetValue oommf_threads]} {
           append ret "Yes: Default thread count = [Oc_GetDefaultThreadCount]"
           set threadlimit [Oc_GetThreadLimit]
           if {![string match {} $threadlimit]} {
              append ret " (limit is $threadlimit)"
           }
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

       if {[info exists env(OOMMF_TCL_CONFIG)] && \
              ![info exists env(OOMMF_TCL_INCLUDE_DIR)]} {
	    append ret "\nWARNING: Your installation of Tcl appears\
		to be missing the header\nfile <tcl.h>.  Perhaps you\
		need to re-install Tcl, requesting a full\ninstallation\
		this time, or install the developers package for\
		Tcl?\nIf you are sure a Tcl header file is installed on\
		your system, then\nset the environment variable\
		OOMMF_TCL_INCLUDE_DIR to the directory\nthat contains\
		the Tcl header file."
	}
	if {$use_tk && [info exists env(OOMMF_TCL_CONFIG)] && \
               ![info exists env(OOMMF_TK_INCLUDE_DIR)]} {
	    append ret "\nWARNING: Your installation of Tk appears\
		to be missing the header\nfile <tk.h>.  Perhaps you\
		need to re-install Tk, requesting a full\ninstallation\
		this time, or install the developers package for\
		Tk?\nIf you are sure a Tk header file is installed on\
		your system, then\nset the environment variable\
		OOMMF_TK_INCLUDE_DIR to the directory\nthat contains\
		the Tk header file."
	}

	if {[string match *WARNING* $ret]} {
	    append ret "\n\nPlease see the Installation section of the\
		    OOMMF User's Guide\nfor more information."
	}
	return $ret
    }
    method Summary {} {
       global env
       set env(OOMMF_BUILD_ENVIRONMENT_NEEDED) 1
       $class DetermineBuildEnvironment

       if {![catch {$this GetValue cross_compile} xc] && $xc} {
          return [$this CrossCompileSummary]
       }

       global tcl_platform
       set ret "Platform Name:\t\t$name\n"
       append ret "Tcl name for OS:\t"
       append ret "$tcl_platform(os) $tcl_platform(osVersion)\n"
       append ret "C++ compiler:   \t"
       set compiler_found 0
       if {![catch {$this GetValue program_compiler_c++_name} cn]} {
          append ret "$cn"
          set compiler_found 1
       } elseif {![catch {$this GetValue program_compiler_c++} c]} {
          set compilertest [lindex $c 0]
          set wd [auto_execok $compilertest]
          if {![string match {} $wd]} {
             append ret "$wd"
             set compiler_found 1
          } else {
             append ret "not found - \"[lindex $c 0]\""
          }
       } else {
          append ret "none selected"
       }
       append ret "\n"
       if {$compiler_found} {
          if {![catch {$this GetValue program_compiler_c++_banner_cmd} \
                   bcmd]} {
             if {![catch {eval $bcmd} banner] && [string length $banner]} {
                append ret " Version string:\t $banner\n"
                if {[string match Darwin $tcl_platform(os)] && \
                       [string match *Xcode* $banner]} {
                   # Xcode in use on Mac OS X; check if command line
                   # tools are installed.
                   set havetools 0
                   if {[file exists /usr/bin/gcc]} {
                      set cmdtoolpath /usr/bin
                      set havetools 1
                   } else {
                      set cmdtoolpath /Library/Developer/CommandLineTools
                      if {[file isdirectory $cmdtoolpath]} {
                         set gccpath [file join $cmdtoolpath usr bin gcc]
                         if {[file exists $gccpath]} {
                            set havetools 1
                         }
                      }
                   }
                   if {$havetools} {
                      append ret \
                      " Xcode command line tools installed under $cmdtoolpath\n"
                   } else {
                      append ret \
                      " *** WARNING: Xcode command line tools not found. ***\n"
                   }
                }
             }
          }
          if {![catch {$this GetValue program_compiler_c++_cpu_arch} arch]} {
             append ret " Compiler target arch:\t $arch\n"
          }
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
             regsub -all "\t" $target_tcl_info " " target_tcl_info
             if {[regexp {Tcl patchlevel *= *([0-9.]*)} \
                     $target_tcl_info dummy target_tcl_version]} {
                append ret "$target_tcl_version"
                unset target_tcl_version
             } else {
                append ret "?"
             }
             if {[regexp {tcl_platform\(pointerSize\) *= *([0-9]+)} \
                     $target_tcl_info dummy pointerSize]} {
                append ret ", [expr {$pointerSize*8}] bit"
                unset pointerSize
             } elseif {[regexp {tcl_platform\(wordSize\) *= *([0-9]+)} \
                     $target_tcl_info dummy wordSize]} {
                append ret ", [expr {$wordSize*8}] bit"
                unset wordSize
             }
             if {[regexp {tcl_platform\(threaded\) *= *([0-9]+)} \
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
             regsub -all "\t" $target_tcl_info " " target_tcl_info
             if {[regexp {Tcl patchlevel *= *([0-9.]*)} \
                     $target_tcl_info dummy target_tcl_version]} {
                append ret "$target_tcl_version"
                unset target_tcl_version
             } else {
                append ret "?"
             }
             if {[regexp {tcl_platform\(pointerSize\) *= *([0-9]+)} \
                     $target_tcl_info dummy pointerSize]} {
                append ret ", [expr {$pointerSize*8}] bit"
                unset pointerSize
             } elseif {[regexp {tcl_platform\(wordSize\) *= *([0-9]+)} \
                           $target_tcl_info dummy wordSize]} {
                append ret ", [expr {$wordSize*8}] bit"
                unset wordSize
             }
             if {[regexp {tcl_platform\(threaded\) *= *([0-9]+)} \
                     $target_tcl_info dummy threaded] && $threaded} {
                append ret ", threaded"
             } else {
                append ret ", not threaded"
             }
             catch {unset threaded}
             append ret "\n"
          } else {
             # Run error.  Pull out whatever information we can.
             regsub -all "\t\r" $target_tcl_info " " work_info
             if {[regexp {Tcl patchlevel *= *([0-9.]*)} \
                     $work_info dummy target_tcl_version]} {
                set msg \
                   "                  \t --> Version $target_tcl_version"
                if {[regexp {tcl_platform\(pointerSize\) *= *([0-9]+)} \
                        $work_info dummy pointerSize]} {
                   append msg ", [expr {$pointerSize*8}] bit"
                   unset pointerSize
                   if {[regexp {tcl_platform\(wordSize\) *= *([0-9]+)} \
                           $work_info dummy wordSize]} {
                      append msg ", [expr {$wordSize*8}] bit"
                      unset wordSize
                      if {[regexp {tcl_platform\(threaded\) *= *([0-9]+)} \
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
          append ret "none (not required on Windows)\n"
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

       if {[catch {$this GetValue use_tk} use_tk]} {
          set use_tk 1  ;# Default is to build with Tk
       }
       if {!$use_tk} {
          append ret " wish (OOMMF):        \tBuild without Tk requested.\n"
       } else {
          catch {$this Wish} wish
          append ret " wish (OOMMF):        \t$wish\n"
          if {![catch {exec $wish $tclinfoscript} target_tcl_info]} {
             append ret "                  \t --> Version "
             regsub -all "\t" $target_tcl_info " " target_tcl_info
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

          append ret " tkConfig.sh:         \t"
          global env
          if {[info exists env(OOMMF_TK_CONFIG)]} {
             append ret "$env(OOMMF_TK_CONFIG)\n"
          } elseif {[string match windows $tcl_platform(platform)]} {
             append ret "none (not required on Windows)\n"
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
       }
       unset tclinfoscript

        append ret    "\nOOMMF threads:         \t"
        if {[$this GetValue oommf_threads]} {
           append ret "Yes: Default thread count = [Oc_GetDefaultThreadCount]"
           set threadlimit [Oc_GetThreadLimit]
           if {![string match {} $threadlimit]} {
              append ret " (limit is $threadlimit)"
           }
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
	if {$use_tk && ![info exists env(OOMMF_TK_INCLUDE_DIR)]} {
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

    private method SetWindowsTclTkLibValues {} {
       # Sets TCL_LIB_SPEC and TK_LIB_SPEC config values, provided they
       # are not already set. If you want overwrite preset values, you
       # must unset them first.
       set need_tcl_lib_spec [set need_tk_lib_spec 1]
       if {![catch {$this GetValue TCL_LIB_SPEC}]} {
          set need_tcl_lib_spec 0
       }
       if {![catch {$this GetValue TK_LIB_SPEC}]} {
          set need_tk_lib_spec 0
       }
       if {$need_tcl_lib_spec==0 && $need_tk_lib_spec==0} {
          return
       }

       # Assumes the following config values are already set:
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

       # Tcl provides support for accessing the Windows registry, and in
       # principle we might get some information about the Tcl install
       # from that, but there doesn't currently (2020) appear to be any
       # commonality between Tcl installers on registry settings. OTOH,
       # some Windows Tcl installations (but not all) include a
       # tclConfig.sh file. This file typically has build as opposed to
       # install information, but the TCL_LIB_FILE setting should be
       # correct. Use environment variable OOMMF_TCL_CONFIG if set,
       # otherwise use FindTclConfig method.
       global env
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
          if {[catch {set env(OOMMF_BUILD_ENVIRONMENT_NEEDED)} \
               build_environment_needed]} {
             set build_environment_needed 0
          }
          if {!$build_environment_needed \
                 && ![$this CheckTclConfigVersion $env(OOMMF_TCL_CONFIG)]} {
             global errorInfo
             set errorInfo [Oc_StackTrace]
             Oc_Log Log "Tcl version mismatch:\n\t$env(OOMMF_TCL_CONFIG)\
               version info doesn't match running Tcl version\
               [package provide Tcl]" warning $class 
          }
       } else {
          if {![catch {$this FindTclConfig} _]} {
             set env(OOMMF_TCL_CONFIG) [Oc_DirectPathname $_]
          }
       }
       if {[info exists env(OOMMF_TCL_CONFIG)]} {
          set config_dict [$this ReadConfigFile $env(OOMMF_TCL_CONFIG)]
          if {[dict exists $config_dict TCL_LIB_FILE]} {
             set tlf [dict get $config_dict TCL_LIB_FILE]
	     set test [file join $root lib $tlf]
	     if {$need_tcl_lib_spec && [file exists $test]} {
                $this SetValue TCL_LIB_SPEC $test
                set need_tcl_lib_spec 0
             }
             if {[regsub TCL $test TK test] || [regsub {(T|t)cl} $test {\1k} test]} {
                if {$need_tk_lib_spec && [file exists $test]} {
                   $this SetValue TK_LIB_SPEC $test
                   set need_tk_lib_spec 0
                }
             }
          }
          if {$need_tcl_lib_spec==0 && $need_tk_lib_spec==0} {
             return
          }
       }

       # The following are relative to TCL_EXEC_PREFIX, thus $root
       # in our "standard" Tcl/Tk install on Windows.
       set Tcllib_base tcl$tlma$tlmi
       set Tklib_base tk$tkma$tkmi
       foreach elt {Tcl Tk} {
          if {[set need_[string tolower $elt]_lib_spec] == 0} {
             continue
          }
	  set base [set ${elt}lib_base]
	  set test [file join $root lib $base$vc.lib]
	  if {![file exists $test]} {
	     # Not there; try "t" (threaded) suffix
	     set test [file join $root lib ${base}t.lib]
	     if {![file exists $test]} {
		# Still no luck; see what we can find
		set check [file join $root lib ${base}*.lib]
		if {[llength $check]==1} {
		   set test [lindex $check 0]
		} else {
		   if {[llength $check]==0} {
		      set msg "Unable to locate $elt lib in\
                          folder [file join $root lib]"
		   } else {
		      set msg "Unable to identify unique $elt lib in\
                          folder [file join $root lib]"
		   }
		   error $msg $msg
		}
	     }
	  }
	  $this SetValue [string toupper $elt]_LIB_SPEC $test
       }
    }

    private method RecordWindowsTclTkConfiguration {} {
       # Note that a "standard" Tcl/Tk configuration on Windows
       # is one where TCL_PREFIX, TK_PREFIX, TCL_EXEC_PREFIX,
       # and TK_EXEC_PREFIX are all the same directory, $root.
       #
       # There are reportedly other variations of installation on
       # Windows (TclPro?), but the best answer to bug report
       # concerning them is probably to advise use of something
       # like ActiveTcl that follows the "standard".

       set root [file dirname [file dirname \
                  [Oc_DirectPathname [info library]]]]

       # The following commented block creates a short-form file name
       # without spaces, for example, changing "Program Files" into
       # Progra~1. This is not technically valid (in some cases the
       # short form could end with ~2, ~3, etc. instead of ~1), and
       # moreover shouldn't be needed. But it was in place for older
       # code so it is retained here in case of emergency.
       #
       # set pathlist [list]
       # foreach p [file split $root] {
       #     if {[regexp " " $p]} {
       #         lappend pathlist [string range $p 0 5]~1
       #     } else {
       #         lappend pathlist $p
       #     }
       # }
       # set root [eval file join $pathlist]

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
       $this SetWindowsTclTkLibValues

       # Add other *_LIB_SPEC (f.e. Borland, Watcom, ...) on request.
       # Compiler-specific configurations (f.e. TCL_LIBS) must be
       # completed in the config/platforms/*.tcl file.
    }

    private method RecordTclTkConfiguration {} {
	# Record values for a set of features which describe the
	# Tcl/Tk configuration.
	global env tcl_platform

        # Special handling for Windows
	if {[string match windows $tcl_platform(platform)]
		&& (![info exists env(OSTYPE)]
             || ![string match cygwin* $env(OSTYPE)])} {
           $this RecordWindowsTclTkConfiguration
           return
        }

	# Not Windows or cygwin.  Look for tclConfig.sh.
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

        # Check to see if we are in the midst of building OOMMF.
        # Default is no.
        if {[catch {set env(OOMMF_BUILD_ENVIRONMENT_NEEDED)} \
                build_environment_needed]} {
           set build_environment_needed 0
        }
	if {[info exists env(OOMMF_TCL_CONFIG)]} {
           $this LoadConfigFile $env(OOMMF_TCL_CONFIG)
           # Warn if we got a config file for a different Tcl release,
           # unless we are in the midst of a build.  (Cross-compile
           # builds in particular may have different build and run Tcl.)
           if {!$build_environment_needed} {
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
           # Warn if we got a config file for a different Tk release,
           # unless we are in the midst of a build.  (Cross-compile
           # builds in particular may have different build and run Tk.)
           if {!$build_environment_needed} {
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
        }
	if {[string match windows $tcl_platform(platform)]
		&& (![info exists env(OSTYPE)]
			|| ![string match cygwin* $env(OSTYPE)])
		&& (![info exists env(TERM)] 
		    || ![string match cygwin $env(TERM)])} {
	   $this SetWindowsTclTkLibValues
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
       if {[package vcompare $checkver 9.0] < 0 && [string compare $checkver $verno]!=0} {
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
             if {[string match Darwin $tcl_platform(os)]} {
                lappend search "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/$idir"
             }
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
             if {[string match Darwin $tcl_platform(os)]} {
                lappend search "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/$idir"
                if {[regsub Tcl.framework $idir Tk.framework _]} {
                   lappend search "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/$_"
                }
             }
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
          if {[info exists env(OOMMF_TCL_INCLUDE_DIR)]} {
             if {[lsearch -exact $search $env(OOMMF_TCL_INCLUDE_DIR)]<0} {
                lappend search $env(OOMMF_TCL_INCLUDE_DIR)
             }
             if {[regsub -all Tcl $env(OOMMF_TCL_INCLUDE_DIR) Tk _]} {
                if {[lsearch -exact $search $_]<0} {
                   lappend search $_
                }
             }
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
       # (except on Windows, where ::tcl::pkgconfig provide build as
       # opposed to install info)
       if {![string match windows $tcl_platform(platform)] && \
            [catch {::tcl::pkgconfig get libdir,runtime} libdir] == 0} {
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
          if {[catch {::tcl::pkgconfig get libdir,runtime} libdir] == 0} {
             lappend chklist "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/$libdir"
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
       if {[package vcompare $checkver 9.0] < 0 && [string compare $checkver $verno]!=0} {
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

    private method ReadConfigFile {fn} {
        # Returns a dictionary filled with values from the *Config.sh file $fn.
	set f [open $fn r]
	gets $f line
        set config_dict [dict create]
	while {![eof $f]} {
	    if {[regexp {^([A-Z0-9_]+)='([^']*)'} $line _ feat val]
	    || [regexp "^(\[A-Z0-9_]+)=(\[^ \t\r\n]*)" $line _ feat val]} {
		# Handle (some) shell variable substitutions
		regsub -all {\\} $val {\\\\} val
		regsub -all {\[} $val {\\[} val
		regsub -all {\${(T(CL|K)_[A-Z_]+)}} $val \
			{[$this GetValue "\1"]} val
		set val [subst -novariables $val]
                # Special case: TCL_RANLIB
                # The ranlib specified in tclConfig.sh comes from the
                # Tcl build system environment and might not exist on
                # the OOMMF build system.  Test, and replace with a
                # fallback if possible.
                if {[string compare TCL_RANLIB $feat]==0 \
                       && [string match {} [auto_execok $val]]} {
                   # Bad ranlib
                   if {![string match {} [auto_execok [file tail $val]]]} {
                      # Dropping path yields an executable command
                      set val [file tail $val]
                   } elseif {[string match *ranlib* $val] \
                                && ![string match {} [auto_execok ranlib]]} {
                      # Last chance try, bare "ranlib", is executable
                      set val ranlib
                   }
                }
                dict set config_dict $feat $val
            }
	    gets $f line
	}
        close $f
        return $config_dict
    }      

    private method LoadConfigFile {fn} {
        set config_dict [$this ReadConfigFile $fn]
        dict for {feat val} $config_dict {
           $this SetValue $feat $val
        }
    }

    private method LoadCache {} {
       set fn [file join $cacheDir [string tolower \
               [$this GetValue platform_name]].tcl]
       if {[file readable $fn]} {
          if {[catch {uplevel #0 [list source $fn]} msg]} {
             Oc_Log Log "Error (2) sourcing $fn:\n\t$msg" warning $class
          } else {
             # If the platform file was sourced for building, then
             # TCL_LIBS and TK_LIBS should defined.  The latter requires
             # the former, so remove duplicate elements from the latter.
             # Special case: On Mac OS X, the link line includes
             # elements of the form "-framework Cocoa", which looks like
             # two elements in the tklibs list, but is actually one
             # logical argument.  If the "-framework" flag is removed
             # but not the target, then you can get a link line with a
             # bare "Cocoa" (for example) argument, which is wrong.
             # Patch this by spackling the two together before the match
             # and separating them afterward.
             #
             # NOTE: There may be additional problems, such as -L paths
             # being stripped from tklibs because they appear also in
             # tcllibs, but the Tk libs are put first on link line, so
             # what happens if the -L paths don't appear until later?
             # We made need a smarter solution here.
             if {![catch {$this GetValue TCL_LIBS} tcllibs] \
                    && ![catch {$this GetValue TK_LIBS} tklibs]} {
                # Mac OS X patch
                regsub -all -- {-framework *([^ ]+)} $tcllibs \
                   {-framework_\1} tcllibs
                regsub -all -- {-framework *([^ ]+)} $tklibs \
                   {-framework_\1} tklibs
                set reducedlibs {}
                foreach elt $tklibs {
                   if {[lsearch -exact $tcllibs $elt]<0} {
                      lappend reducedlibs $elt
                   }
                }
                regsub -all -- {-framework_([^ ]+)} $reducedlibs \
                   {-framework \1} reducedlibs
                $this SetValue TK_LIBS $reducedlibs
             }
          }
       }
    }

}

