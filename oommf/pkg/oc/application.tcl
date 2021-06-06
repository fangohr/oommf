# FILE: application.tcl
#
# Last modified on: $Date: 2008/07/22 23:23:24 $
# Last modified by: $Author: donahue $
#
# TODO: Re-discover why the cache is destroyed each time.
#	Try to devise a scheme where it can be retained.

Oc_Class Oc_Application {

    common platformName

    common path {}

    # Which versions are available for each application name
    array common versions

    # Applications indexed by application name and version
    array common apps

    # Scratch array for partial results of the Resolve / Exec process.
    array common cache

    private common afterId
    ClassConstructor {
        set platformName [[Oc_Config RunPlatform] GetValue platform_name]
        global tcl_platform env
        # On all platforms, make sure child processes can find the init.tcl
        # file, just as the parent process did.
        # NOTE: Should we do likewise for Tk ?
        # WARNING: This may break things when one release of the Tcl
        # is used to launch a child process which uses a different release
        # of the Tcl interpreter.
        if {![info exists env(TCL_LIBRARY)]} {
            set env(TCL_LIBRARY) [info library]
        }
        # On Windows, make sure child processes can find tclXX.dll and
        # tkXX.dll just as the parent process did.  Put this directory
        # up at the front of the PATH, so we get *these* DLL's as
        # opposed to some others with the same name that might be
        # laying around elsewhere in the system (in particular, say
        # C:/cygwin/bin/)
        if {[string match windows $tcl_platform(platform)]} {
            set dir [Oc_DirectPathname [file dirname [info nameofexecutable]]]
            if {![info exists env(PATH)]} {
                set env(PATH) $dir
            } else {
                set apath [split $env(PATH) {;}]
                set index [lsearch -exact $apath $dir]
                if {$index >= 0} {
                    set apath [lreplace $apath $index $index]
                }
                set apath [linsert $apath 0 $dir]
                set env(PATH) [join $apath {;}]
            }
        }
        set afterId [after idle [list $class SetSearchPath [list [file \
                dirname [file dirname [file dirname [info script]]]]]]]
    }

    proc SetSearchPath {_path} {
        if {[info exists afterId]} {
            after cancel $afterId
            unset afterId
        }
        if {[catch {set _path [lrange $_path 0 end]} msg]} {
            return -code error "Can't set search path to '$_path'
	Value provided is not a valid Tcl list: $msg"
        }
        # Should we check that each element of the list is a directory?
        set path $_path
        $class Rescan
    }

    proc Rescan {} {
        # Clear away any existing Application Instances
        catch {unset versions}
        catch {unset apps}
        foreach a [$class Instances] {
            $a Delete
        }
        # Source all the appindex.tcl files relative to $path
        # They contain Application Define statements to create
        # Application Instances.
        foreach lib $path {
            foreach file [glob -nocomplain \
                    [file join $lib app * appindex.tcl]] {
                if {[catch {source $file} msg]} {
                    Oc_Log Log "Error sourcing application index\
                            file:\n\t$file:\n    $msg" warning $class
                }
            }
            foreach file [glob -nocomplain \
                    [file join $lib app * scripts appindex.tcl]] {
                if {[catch {source $file} msg]} {
                    Oc_Log Log "Error sourcing application index\
                            file:\n\t$file:\n    $msg" warning $class
                }
            }
            foreach file [glob -nocomplain \
                    [file join $lib app * $platformName appindex.tcl]] {
                if {[catch {source $file} msg]} {
                    Oc_Log Log "Error sourcing application index\
                            file:\n\t$file:\n    $msg" warning $class
                }
            }
        }
        # Add entries for tclsh, user_tclsh, and wish.
        # (user_tclsh is the Tcl shell interpreting this script.)
        set config [Oc_Config RunPlatform]
	if {![catch {$config Tclsh} mytclsh]} {
	    if {[catch {$config GetValue TCL_VERSION} ver]} {
		set ver [info tclversion]
	    }
	    [$class New _ -name tclsh -version $ver -machine $platformName \
		    -file $mytclsh -directory ""] Initialize
	}
	if {![catch {info nameofexecutable} usertclsh]} {
           set ver [info tclversion]
           [$class New _ -name user_tclsh -version $ver -machine $platformName \
               -file $usertclsh -directory ""] Initialize
	}
	if {![catch {$config Wish} mywish]} {
	    if {[catch {$config GetValue TK_VERSION} ver]} {
		set ver [Oc_Config TkVersion]
	    }
	    [$class New _ -name wish -version $ver -machine $platformName \
		    -file $mywish -directory ""] Initialize
	}
    }

    proc Define {definition} {
	# Warning: This routine assumes the working directory
	#   is not changed inside the evaluating script, or else
	#   the script was specified with an absolute pathname
	#   (otherwise 'indexFile' will be wrong).
        if {[string match {} [info script]]} {
            return -code error "'$class Define' must be in an appindex.tcl file"
        }
        set indexFile [Oc_DirectPathname [file join [pwd] [info script]]]
        # Apply syntactic sugar
        set argstring ""
        foreach line [split $definition \n] {
            if {[info complete $argstring]} {
                append argstring " $line"
            } else {
                append argstring \n$line
            }
        }
        array set temp [uplevel list $argstring]
        # Filter out any unsupported options
        set args {}
        foreach opt [$class Options * New] {
            catch {lappend args $opt $temp($opt)}
        }
        catch {unset temp}
        array set temp $args
        # Supply additional constructor options
        if {![info exists temp(-directory)]} {
            set temp(-directory) [file dirname $indexFile]
        }
        if {[catch {[eval $class New _ [array get temp]] Initialize} msg]} {
            return -code error $msg
        }
    }

    callback proc CompareVersions {v1 v2} {
	return [package vcompare $v1 $v2]
    }

    proc IsShell {appname} {
	if {[catch {$class SelectAppByKeys $appname} app]} {return 0}
	return [expr {![string compare [$app Cget -machine] $platformName]}]
    }

    proc Exec {keys args} {
        if {[catch {eval [linsert $keys 0 $class SelectAppByKeys]} app]} {
            return -code error $app
        }
        set code [catch {eval [linsert $args 0 $app Exec]} msg]
        catch {unset cache}
        if {$code} {
            set emsg "Error executing [$app Cget -name] version\
                    [$app Cget -version]:\n\t$msg"
            error $emsg $emsg
        }
        return $msg
    }

    proc CommandLine {keys args} {
        if {[catch {eval [linsert $keys 0 $class SelectAppByKeys]} app]} {
            return -code error $app
        }
        set code [catch {$app CommandLine} msg]
        catch {unset cache}
        if {$code} {
            set emsg "Error determining [$app Cget -name] version\
                    [$app Cget -version] command line:\n\t$msg"
            error $emsg $emsg
        }
	if {[string match bg [$app Cget -mode]]
		&& ![string match & [lindex $args end]]} {
	    lappend args &
	}
        return [concat $msg $args]
    }

    private proc SelectAppByKeys {args} {
        if {[catch {eval [linsert $args 0 $class Resolve]} app]} {
            set avail {}
            foreach a [$class Instances] {
                if {[$a WillRun] 
			&& [string compare $platformName [$a Cget -machine]]} {
                    lappend avail "[$a Cget -name] [$a Cget -version]"
                }
            }
            set navail {}
            set last ""
            foreach now [lsort -dictionary $avail] {
                if {[string compare $last $now]} {
                    lappend navail $now
                    set last $now
                }
            }
            if {[llength $navail]} {
                set avmsg "Available applications for platform\
                        '$platformName':\n\t[join $navail \n\t]"
            } else {
                set avmsg "Have you run pimake for platform '$platformName'?"
            }
            catch {unset cache}
            return -code error "Can't find application satisfying\
                    '$args'\nfor platform '$platformName':\n\t$app\n\n$avmsg"
        }
        return $app
    }

    private proc Resolve {args} {
        # This calls SetSearchPath in the event that the event
        # loop still hasn't had a chance to evaluate the SetSearchPath
        # call scheduled by the Class Constructor
        if {[info exists afterId]} {
            uplevel #0 [lindex [after info $afterId] 0]
        }
        if {[llength $args] == 0} {
            return ""
        }
        set name [string tolower [lindex $args 0]]
        if {[string match {} $name]} {
            return -code error "No application name specified."
        }
        set args [lrange $args 1 end]
        if {[set exact [string match -exact [lindex $args 0]]]} {
            set args [lrange $args 1 end]
        }
        set version [lindex $args 0]
        if {$exact} {
            if {[string match {} $version]} {
                return -code error "'-exact' option requires a version number"
            }
            if {![info exists apps($name,$version)]} {
                return -code error "No matching application in the search path."
            }
            set attempts ""
            foreach app $apps($name,$version) {
                if {[$app WillRun]} {
                    return $app
                }
                append attempts "$cache($app)\n"
            }
            return -code error "The following candidates were\
                    rejected:\n$attempts"
        }
        if {![info exists versions($name)]} {
            return -code error "No matching application in the search path."
        }
        set attempts ""
        foreach v [lsort -command {package vcompare} -decreasing \
                $versions($name)] {
            if {[$class Satisfies $v $version] \
                    && [info exists apps($name,$v)]} {
                foreach app $apps($name,$v) {
                    if {[$app WillRun]} {
                        return $app
                    }
                    append attempts "$cache($app)\n"
                }
            }
        }
        return -code error "The following candidates were rejected:\n$attempts"
    }

    private proc Satisfies {avail req} {
        if {[string match {} $req]} {
            return 1
        }
	return [package vsatisfies $avail $req]
    }

    const public variable machine
    const public variable name 
    const public variable version
    const public variable file
    const public variable mode = bg {
	if {![string match bg $mode] && ![string match fg $mode]} {
	    return -code error "Bad application mode: $mode. Should be bg or fg"
	}
    }

    const public variable aliases = {}
    const public variable directory
    const public variable options = {}

    Constructor {args} {
        eval $this Configure $args
    }

    method Initialize {} {
        foreach _ [concat [list $name] $aliases] {
            set n [string tolower $_]
            lappend apps($n,$version) $this
            if {![info exists versions($n)] \
                    || [lsearch -exact $versions($n) $version] < 0} {
                lappend versions($n) $version
            }
        }
        return {}
    }

    # execute this Application
    private method Exec {args} {
	if {[string match bg $mode] && ![string match & [lindex $args end]]} {
	    lappend args &
	}
        eval $cache($this) $args
    }

    private method CommandLine {} {
        set firstword [lindex $cache($this) 0]
        if {[string match exec $firstword]} {
            return [lrange $cache($this) 1 end]
        }
        return [concat [$firstword CommandLine] [lrange $cache($this) 2 end]]
    }
    
    private method WillRun {} {
       if {![info exists file] || [string match {} $file]} {
          set fileset 0
          set execfile [file join $directory $platformName $machine]
       } else {
          set fileset 1
          set execfile [file join $directory $file]
       }
        # Try to resolve the machine into a supporting shell application
        if {$fileset && ![catch {eval $class Resolve $machine} app]} {
            # Found a shell app which WillRun
            if {[file readable $execfile]} {
		set suboptions {}
		foreach o $options {
		    catch {lappend suboptions [uplevel #0 [list subst $o]]}
		}
                set cache($this) [concat [list $app Exec $execfile] $suboptions]
                return 1
            } else {
                set cache($this) "$execfile: permission denied"
                return 0
            }
        }
        # No shell app -- try as a binary
        if {!$fileset || [string match $platformName $machine]} {
            if {[file executable [lindex [auto_execok $execfile] 0]]} {
		set suboptions {}
		foreach o $options {
		    catch {lappend suboptions [uplevel #0 [list subst $o]]}
		}
                set cache($this) [concat [list exec $execfile] $suboptions]
                return 1
            } else {
                set cache($this) "$execfile: permission denied"
                return 0
            }
        } else {
            set cache($this) "$execfile: needs shell or platform '$machine'"
            return 0
        }
    }
       
}

