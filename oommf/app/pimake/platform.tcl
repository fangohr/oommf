# FILE: platform.tcl
#
# The Platform class -- no instances, just a way to provide a "major minor"
# style of Tcl command.

Oc_Class Platform {

    public common cflags {}
    public common lflags {}

    const common configuration
    const common platformName

    array common linkopts
    array common compopts
    array common mlopts

    ClassConstructor {
        set configuration [Oc_Config RunPlatform]
        set platformName [$configuration GetValue platform_name]
        set linkopts(obj) {Platform Objects}
        set linkopts(out) {Platform Executables}
        set linkopts(lib) {Platform LinkerLibs}
        set compopts(out) {Platform Objects}
        set compopts(src) {Platform AbsoluteSrc}
        set mlopts(obj) {Platform Objects}
        set mlopts(out) {Platform StaticLibrary}
    }

#    proc EvalSafeList {l} {
#        eval list \{[join $l "\} \{"]\}
#    }
#
#    The attempt above may be sufficient for our purposes, but it can't
#    handle unmatched open brace as an element.  The replacement is safe.
    proc EvalSafeList {l} {
        set r {}
        foreach e $l {
            lappend r $e
        }
        return $r
    }

    proc Name {} {
        return $platformName
    }

    proc Specific {files} {
        set ret {}
        foreach f $files {
            lappend ret [file join $platformName $f]
        }
        return $ret
    }

    proc Executables {stemlist} {
        set ret {}
        set script [$configuration GetValue script_filename_executable]
        if {[string length $script]} {
            foreach stem $stemlist {
                lappend ret [file join $platformName [eval $script $stem]]
            }
        }
        return $ret
    }

    proc Intermediate {stemlist} {
        set ret {}
	if {[catch {
	    set scripts [$configuration GetValue script_filename_intermediate]
	}]} {
	    return $ret
	}
        foreach script $scripts {
            if {[string length $script]} {
                foreach stem $stemlist {
                    lappend ret [file join $platformName [eval $script $stem]]
		}
            }
        }
        return $ret
    }

    proc Objects {stemlist} {
        set ret {}
        set script [$configuration GetValue script_filename_object]
        if {[string length $script]} {
            foreach stem $stemlist {
                lappend ret [file join $platformName [eval $script $stem]]
            }
        }
        return $ret
    }

    # DOME: Guard against spaces in stems
    # DOME: Reconsider ../../ form vs. absolute.
    proc StaticLibraries {stemlist} {
       set ret {}
       set script [$configuration GetValue script_filename_static_library]
       if {[string length $script]} {
          foreach stem $stemlist {
             set pkgdir [file join .. .. pkg $stem]
             if {[file isdirectory $pkgdir]} {
                lappend ret [file join $pkgdir $platformName \
                                 [eval $script $stem]]
             } elseif {![catch {$configuration GetValue program_linker_extra_lib_dirs} libdirs]} {
                # stem is not a recognized pkg stem; see if it is
                # an external library in one of the extra library
                # directories.
                set extrascripts {}
                if {![catch {$configuration GetValue program_linker_extra_lib_scripts} extras]} {
                   set extrascripts $extras
                }
                set extra_candidates {}
                set standard_candidates {}
                foreach dir $libdirs {
                   set dir [Oc_DirectPathname $dir]
                   # Test "extra" scripts, if any
                   foreach testscript $extrascripts {
                      set test [file join $dir [eval $testscript $stem]]
                      foreach match [glob -nocomplain $test] {
                         if {[lsearch -exact \
                               [concat $extra_candidates $standard_candidates] \
                                  $match] == -1} {
                            lappend extra_candidates $match
                         }
                      }
                   }
                   # Also see what the standard library script turns up
                   set test [file join $dir [eval $script $stem]]
                   foreach match [glob -nocomplain $test] {
                      if {[lsearch -exact \
                               [concat $extra_candidates $standard_candidates] \
                               $match] == -1} {
                         lappend standard_candidates $match
                      }
                   }
                }
                set candidates [concat $extra_candidates $standard_candidates]
                if {[llength $extra_candidates]==1} {
                   # If there is exactly one match from the extras
                   # script, use it.
                   lappend ret [lindex $extra_candidates 0]
                } elseif {[llength $candidates]==1} {
                   # Otherwise, if there is exactly one match from
                   # both scripts, use it.
                   lappend ret [lindex $candidates 0]
                } elseif {[llength $candidates]>1} {
                   # Multiple matches.  What to do?
                   error "Multiple matches to library stem $stem: $candidates"
                }
             }
          }
       }
       return $ret
    }

    proc AbsoluteSrc {stemlist} {
        # Assume stemlist has only one stem
        set ret {}
        lappend ret [file join [pwd] [lindex $stemlist 0]]
        return $ret
    }

    proc StaticLibrary {stem} {
        set ret {}
        set script [$configuration GetValue script_filename_static_library]
        if {[string length $script]} {
            lappend ret [file join $platformName [eval $script $stem]]
        }
        return $ret
    }

    proc LinkerLibs {stemlist} {
        set ret {}
        foreach stem $stemlist {
            if {[string match tk $stem]} {
                if {![$configuration GetValue use_tk]} { continue }
                set ret [concat $ret [$configuration GetValue TK_LIB_SPEC]]
                set ret [concat $ret [$configuration GetValue TK_LIBS]]
		continue
            }
            if {[string match tcl $stem]} {
                set ret [concat $ret [$configuration GetValue TCL_LIB_SPEC]]
                set ret [concat $ret [$configuration GetValue TCL_LIBS]]
		if {[catch {$configuration GetValue TCL_PACKAGE_PATH} pp]} {
		    continue
		}
		set libdir [lindex $pp 0]
		if {[string match */Resources/Scripts $libdir]} {
		    set libdir [file dirname [file dirname $libdir]]
		}
                if {[catch {
                   $configuration GetValue program_linker_rpath
                } rpath_script]} {
                   continue
                }
                if {![catch {
                   $configuration GetValue cross_compile_target_tcl_rpath
                } target_rpath] && ![catch {
                   $configuration GetValue program_linker_rpathlink
                } rpathlink_script]} {
                   # Cross-compile link with separate rpath
                   set ret [concat $ret [eval $rpath_script $target_rpath]]
                   set ret [concat $ret [eval $rpathlink_script $libdir]]
                } else {
                   # Usual link
                   set ret [concat $ret [eval $rpath_script $libdir]]
                }
                continue
            }
            if {[$configuration GetValue program_linker_uses_-L-l]} {
  	       set libdir [file dirname [$class StaticLibraries [list $stem]]]
	       if {[file isdirectory $libdir]} {
		  # If $libdir is not a directory, then assume $stem is
		  # a system library, and so doesn't need a -L option.
		  lappend ret -L$libdir
	       }
	       lappend ret -l$stem
            } elseif {[$configuration GetValue program_linker_uses_-I-L-l]} {
  	       set libdir [file dirname [$class StaticLibraries [list $stem]]]
	       if {[file isdirectory $libdir]} {
		  # If $libdir is not a directory, then assume $stem
		  # is a system library, and so doesn't need the -I
		  # and -L options.
		  set hdir [file dirname $libdir]
		  set header [file join $hdir $stem.h]
		  if {[file readable $header]} {
		     foreach dir [[CSourceFile New _ $header] DepPath] {
                        lappend ret -I$dir
		     }
		  }
		  lappend ret -ptr
		  lappend ret [file join $libdir tr]
		  lappend ret -L$libdir
	       }
	       lappend ret -l$stem
            } else {
	       set lf [$class StaticLibraries [list $stem]]
	       if {[file isfile $lf]} {
		  lappend ret $lf
	       } else {
		  # Assume this is a system library
		  lappend ret $stem
	       }
            }
        }
        if {![catch {$configuration GetValue program_linker_extra_args} flags]} {
	   foreach elt $flags {
	      if {[lsearch -exact $ret $elt]==-1} {
		 lappend ret $elt
	      }
	   }
        }
        return $ret
    }

    proc Compile {language args} {
        set pfx program_compiler_[string tolower $language]
        set compcmd [$configuration GetValue $pfx]
        set args [concat $cflags $args]
        while {[string match -* [lindex $args 0]]} {
            regexp ^-(.*)$ [lindex $args 0] _ opt
            set val [lindex $args 1]
            set args [lrange $args 2 end]

            # Use the platform as an escape to pass in command line
            # stuff on a single platform only
            if {[string match $platformName $opt]} {
                foreach word $val {
                    lappend compcmd $word
                }
                continue
            }

            # Pre-process the option value, if applicable
            if {[info exists compopts($opt)]} {
                #             /-- 'eval' breaks apart contents
                #             |                /- 'list' preserves argument
                #             v                v
                set val [eval $compopts($opt) [list \
                        [Platform EvalSafeList $val]]]
                #                 ^
                #                 \- Protect [eval] from any "\n" or ";" 
                #                    in the value of 'val'
            }

            if {[catch {
                    $configuration GetValue ${pfx}_option_$opt
                } optScript] || ![string length $optScript]} {
                # Ignore any options not supplied by this configuration
                continue
            }
            foreach elt $val {
                set compcmd [concat $compcmd [eval $optScript [list $elt]]]
            }
        }
        if {[llength $args]} {
            return -code error "Expected option, but saw '[lindex $args 0]'"
        }
        if {[catch {eval Oc_Exec Foreground $compcmd} msg]} {
            global errorCode
            return -code error -errorcode $errorCode $msg
        }
    }

    proc Link {args} {
        set linkcmd [$configuration GetValue program_linker]
        set args [concat $lflags $args]
        set verifyargs $args
        while {[string match -* [lindex $args 0]]} {
            regexp ^-(.*)$ [lindex $args 0] _ opt
            set val [lindex $args 1]
            set args [lrange $args 2 end]

            # Use the platform as an escape to pass in command line
            # stuff on a single platform only
            if {[string match $platformName $opt]} {
                foreach word $val {
                    lappend linkcmd $word
                }
                continue
            }

            # Pre-process the option value, if applicable
            if {[info exists linkopts($opt)]} {
                set val [eval $linkopts($opt) [list \
                        [Platform EvalSafeList $val]]]
            }

            if {[catch {
                    $configuration GetValue program_linker_option_$opt
                } optScript] || ![string length $optScript]} {
                # Ignore any options not supplied by this configuration
                continue
            }

            foreach elt $val {
                set linkcmd [concat $linkcmd [eval $optScript [list $elt]]]
            }
        }
        if {[llength $args]} {
            return -code error "Expected option, but saw '[lindex $args 0]'"
        }
	# Workaround for ${DBGX} values sprinkled in old broken
	# tclConfig.sh and tkConfig.sh files.
	regsub -all {\${DBGX}} $linkcmd {} linkcmd
	#
        if {[catch {eval Oc_Exec Foreground $linkcmd} msg]} {
            global errorCode
            set ec $errorCode
            eval $class LinkVerify $verifyargs
            return -code error -errorcode $ec $msg
        } else {
            eval $class LinkVerify $verifyargs
        }
    }

    proc LinkVerify {args} {
        while {[string match -* [lindex $args 0]]} {
            if {[string match -out [lindex $args 0]]} {
                VerifyExecutable [lindex $args 1]
            }
            set args [lrange $args 2 end]
        }
    }

    proc MakeLibrary {args} {
        set mlcmd [$configuration GetValue program_libmaker]
        while {[string match -* [lindex $args 0]]} {
            regexp ^-(.*)$ [lindex $args 0] _ opt
            set val [lindex $args 1]
            set args [lrange $args 2 end]

            # Use the platform as an escape to pass in command line
            # stuff on a single platform only
            if {[string match $platformName $opt]} {
                foreach word $val {
                    lappend mlcmd $word
                }
                continue
            }

            # Pre-process the option value, if applicable
            if {[info exists mlopts($opt)]} {
                set val [eval $mlopts($opt) [list \
                        [Platform EvalSafeList $val]]]
            }

            if {[catch {
                    $configuration GetValue program_libmaker_option_$opt
                } optScript] || ![string length $optScript]} {
                # Ignore any options not supplied by this configuration
                continue
            }
            foreach elt $val {
                set mlcmd [concat $mlcmd [eval $optScript [list $elt]]]
            }

        }
        if {[llength $args]} {
            return -code error "Expected option, but saw '[lindex $args 0]'"
        }
        if {[catch {eval Oc_Exec Foreground $mlcmd} msg]} {
            global errorCode
            return -code error -errorcode $errorCode $msg
        }
    }

    proc IndexLibrary {libstem} {
        if {[catch {$configuration GetValue TCL_RANLIB} ilcmd]} {
            return
        }
        if {[string match : $ilcmd]} {
            return
        }
        set ilcmd [concat $ilcmd [Platform StaticLibrary $libstem]]
        if {[catch {eval Oc_Exec Foreground $ilcmd} msg]} {
            global errorCode
            return -code error -errorcode $errorCode $msg
        }
    }

    Constructor {} {
        return -code error "Don't make instances of class 'Platform'"
    }
}

