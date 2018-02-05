# FILE: csourcefile.tcl
#
# The CSourceFile class -- instances of this class represent files of
# source code in the C/C++ language.  These instances are created to
# track the compile-time dependencies among them which imply what
# options are required to be passed to the compiler.

Oc_Class CSourceFile {

    # A map from the direct, absolute name of a file to the
    # CSourceFile instance which represents it.
    array common index

    common oommfExtDir

    const common configuration
    const common platformName

    # sysdepath is analogous to deppath, but for system directories.
    const common sysdeppath

    ClassConstructor {
        set configuration [Oc_Config RunPlatform]
        set platformName [$configuration GetValue platform_name]
	if {[catch {$configuration GetValue \
			program_compiler_c++_system_include_path} \
		 sysdeppath]} {
	    set sysdeppath {}
	}
	# Remove any trailing slashes from paths
	set tmppath [list]
	foreach _ $sysdeppath {
	    lappend tmppath [string trimright $_ /]
	}
	set sysdeppath $tmppath
    }

    # Direct pathname of this source code file
    const public variable name

    # List of directories to search for header files
    const public variable inc = {} {
	set tmp [list]
	foreach _ $inc {
	    lappend tmp [Oc_DirectPathname $_]
	}
	set inc $tmp
    }

    # List of directories (direct paths) which contain files on which this
    # file depends (via recursive tracing of #include)
    private variable deppath

    # List of files (direct paths) on which this
    # file depends (via recursive tracing of #include)
    private variable dependencies
    private variable done

    Constructor {filename args} {
        set name [Oc_DirectPathname $filename]
        if {[info exists index($name)]} {
            set retval $index($name)
            $this Delete
            return $retval
        }
        set index($name) $this
	eval $this Configure $args
        $this FindIncludes
        set done 1
#puts "File '$name' depends on:"
#foreach d $dependencies {
#puts "\t$d"
#}
#puts "DepPath = $deppath"
    }

    private proc PathMatch { elt pathlist } {
	# Uses the 'file nativename' call to see if
	# elt is in pathlist.  As compared to doing a
	# string-based comparison (a la lsearch), this
	# fixes some problems on Cygtel arising from
	# directory aliasing
	set elt [file nativename $elt]
	foreach name $pathlist {
	    if {[string compare $elt [file nativename $name]]==0} {
		return 1
	    }
	}
	return 0
    }

    private method AddDependency {newpath code} {
        if {[file readable $newpath]} {
            foreach npd [[$class New _ $newpath -inc $inc] Dependencies] {
                if {[lsearch -exact $dependencies $npd] == -1} {
                    lappend dependencies $npd
                }
            }
            foreach npp [$index($newpath) DepPath] {
		if {![CSourceFile PathMatch $npp $deppath] && \
			![CSourceFile PathMatch $npp $sysdeppath]} {
                    lappend deppath $npp
                }
            }
            return -code $code
        }
    }

    method FindIncludes {} {
        if {[catch {open $name r} f]} {
            error "Can't open C/C++ source code file '$name':
	$f"
        }
        set dependencies [list $name]
	set deppath [linsert $inc 0 [file dirname $name]]

	# Remove system directories from deppath.
	set tmppath [list]
	foreach npp $deppath {
	    if {![CSourceFile PathMatch $npp $sysdeppath]} {
		lappend tmppath $npp
	    }
	}
	set deppath $tmppath

        set code [catch {
        while {[gets $f line] >= 0} {
            if {[regexp "^\[ \t\]*#\[ \t\]*include\[ \t\]+\"(\[^\"\]+)\"" \
                    $line match newfile]} {
                set searchpath {}
                set newfile [string trim $newfile]
                foreach dir $deppath {
                    lappend searchpath $dir
                    set newpath [file join $dir $newfile]
                    $this AddDependency $newpath break
                    lappend searchpath [file join $dir $platformName]
                    set newpath [file join $dir $platformName $newfile]
                    $this AddDependency $newpath break
                }
                if {![info exists index($newpath)]} {
                    # Haven't yet found the #included file.
                    if {[regexp {^([^.]*)\.h$} $newfile _ stem]} {
                        # Assume it is the public header file of an extension.
                        # By convention, it has the same name as that extension.
                        # For now, the directory containing that extension also
                        # has that name.  Later we may need to deal with 
                        # version numbers and resolving the most recent 
                        # extension release at this point.
                        #
                        # First, find the OOMMF extension directory
			# This is a hack for OOMMF code only!
	                if {![info exists oommfExtDir]} {
			    set pelist [file split $name]
			    set pelist [lreplace $pelist end end]
			    while {[llength $pelist]} {
				set dir [eval file join $pelist [list app]]
				if {![file isdirectory $dir]} {
			            set pelist [lreplace $pelist end end]
				    continue
				}
				set dir [eval file join $pelist [list pkg]]
				if {[file isdirectory $dir]} {
				    set oommfExtDir $dir
				    break
				}
			        set pelist [lreplace $pelist end end]
			    }
			    if {![info exists oommfExtDir]} {
				set msg "Can't find OOMMF extension headers"
				error $msg $msg
			    }
			}
                        set dir [file join $oommfExtDir $stem]
                        if {[file isdirectory $dir]} {
                            lappend searchpath $dir
                            set newpath [file join $dir $newfile]
                            $this AddDependency $newpath ok
                        }
                    }
                }
                if {![info exists index($newpath)]} {
                    # Couldn't find the #include file.
                    # See if there's a MakeRule target to build it.
                    # Search in same places we searched for files.
                    foreach dir $deppath {
                        set newpath [file join $dir $newfile]
                        if {[string length [MakeRule FindRule $newpath]]} {
                            if {[lsearch -exact $dependencies $newpath] == -1} {
                                lappend dependencies $newpath
                            }
                            break
                        }
                        set newpath [file join $dir $platformName $newfile]
                        if {[string length [MakeRule FindRule $newpath]]} {
                            if {[lsearch -exact $dependencies $newpath] == -1} {
                                lappend dependencies $newpath
                            }
                            set npp [file join $dir $platformName]
			    if {[lsearch -exact $deppath $npp] == -1 && \
				    [lsearch -exact $sysdeppath $npp] == -1} {
				lappend deppath $npp
			    }
                            break
                        }
                    }
                    # Assume extension public header files always exist.
                    # If part of them needs to be built, that part should
                    # be built in another file to be #included.
                    #
                    # At this point if we still haven't found the file,
                    # give up and report the error.
                    if {![string length [MakeRule FindRule $newpath]]} {
                        error "Can't find #include dependency \"$newfile\"
located in file '$name'.
Searched the following directories:
$searchpath"
                    }
                }
            } elseif {[regexp "^\[ \t\]*#\[ \t\]*include\[ \t\]+<(\[^>\]+)>" \
                    $line match newfile] && ([string match tcl.h $newfile] \
                    || [string match tk.h $newfile])} {
                if {[catch {$configuration GetValue use_tk} use_tk]} {
                   set use_tk 1   ;# Default is to build with Tk
                }
		if {[string match tcl.h $newfile]} {
		    global env
		    set includeDir $env(OOMMF_TCL_INCLUDE_DIR)
		} elseif {$use_tk && [string match tk.h $newfile]} {
		    global env
		    set includeDir $env(OOMMF_TK_INCLUDE_DIR)
		}
                if {$use_tk || ![string match tk.h $newfile]} {
                   # Don't include tk.h in dependencies if use_tk is false
                   set newpath [file join $includeDir $newfile]
                   if {[lsearch -exact $dependencies $newpath] == -1} {
                      lappend dependencies $newpath
                   }
                   if {[lsearch -exact $deppath $includeDir] == -1 && \
                          [lsearch -exact $sysdeppath $includeDir] == -1} {
                      # Be aggressive about putting Tcl and Tk directories
                      # at front of include list
                      set deppath [linsert $deppath 0 $includeDir]
                   }
                }
                if {$use_tk && [string match tk.h $newfile]} {
                    set xinc [$configuration GetValue TK_XINCLUDES]
                    if {[string match {#*} $xinc]} {
                       # File tkConfig.sh in versions of Tk prior to 8.4 have
                       #    TK_XINCLUDES='# no special path needed'
                       set xinc {}
                    }
		    foreach npp $xinc {
			regsub -- {^-I} $npp {} npp
			set npp [string trim $npp \"]
			if {[lsearch -exact $deppath $npp] == -1 \
				&& [lsearch -exact $sysdeppath $npp] == -1 \
				&& ![string match #* $npp] \
				&& [string length $npp]} {
			    lappend deppath $npp
			}
		    }
		}
	    } elseif {[regexp -nocase \
                    "/\\\*\[ \t\]*end\[ \t\]+includes?\[ \t\]*\\\*/" \
                    $line]} {
	        # The above is a liberal match against lines including
	        # the sequence '/* End includes */'
	        break
            } elseif {[regexp "^\[ \t\]*#\[ \t\]*include\[ \t\]+<(\[^>\]+)>" \
			   $line match newfile]} {
	       # Still no match found.  Try adding in "extra" include
	       # directories.  We add these only as a last resort, and
	       # at the end, to minimize risk of interference with
	       # system include directories (in particular, we don't
	       # want to look for Tcl/Tk headers in these
	       # directories).
	       if {![catch {$configuration GetValue \
				program_compiler_extra_include_dirs} idirs]} {
		  set newpath {}
		  foreach npp $idirs {
                     set npp [Oc_DirectPathname $npp]
		     regsub -- {^-I} $npp {} npp
		     set npp [string trim $npp \"]
		     if {[lsearch -exact $deppath $npp] == -1 \
			     && [lsearch -exact $sysdeppath $npp] == -1 \
			     && ![string match #* $npp] \
			     && [string length $npp]} {
			set newpath [file join $npp $newfile]
			$this AddDependency $newpath break
			set newpath [file join $npp $platformName $newfile]
			$this AddDependency $newpath break
		     }
		  }
		  if {[string length $newpath] && \
			  [info exists index($newpath)]} {
		     # Found it
		     lappend deppath $npp
		  }
	       }
	    }
        }
        } msg]
        catch {close $f}
        if {$code} {
            error "Error finding includes in $name:
	$msg"
        } 
    }

    Destructor {
        if {[info exists name]} {
            if {[string match $this $index($name)]} {
                unset index($name)
            }
        }
    }

    method Dependencies {} {
        if {[info exists done]} {
            return $dependencies
        }
        error "Circular dependency detected:
'$name' ultimately #includes itself"
    }

    method DepPath {} {
        return $deppath
    }

}
