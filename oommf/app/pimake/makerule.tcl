# FILE: makerule.tcl
#
# The MakeRule class -- instances of this class are created by pimake
# by evaluating makerules.tcl files.  All the real work of pimake is
# performed by the methods and procs of this class.
#
# DOME: Fix value of $rulesFile when processing default rules.
#	Perhaps reduce calls to Oc_DirectPathname within proc Define
#	by intelligently tracking what rulesFile is being sourced.
########################################################################

Oc_Class MakeRule {

    # The offset between the tickers of the Tcl commands 
    # 'file' and 'clock'
    const common offset

    const common defaultRules

    array common options

    common pfx ""

    proc SetOptions {optlist} {
        array set options $optlist
    }

    common numErrors

    proc NumErrors {} {
        return $numErrors
    }

    ClassConstructor {
        Oc_TempFile New f 
        set offset [expr {[file mtime [$f AbsoluteName]] \
                - [clock seconds]}]
        $f Delete
        set drfile [file join [file dirname [info script]] \
                makerules.config]
        if {![file readable $drfile] \
                || [catch {
                        set f [open $drfile r]
                        set defaultRules [read $f]
                        close $f
                    }]} {
            set defaultRules {}
        }
        set numErrors 0
    }

    # A map from a target to the rule which updates it
    array common ruleWhichMakes

    # Keep track of which makerules files have been read
    array common filesRead

    # Keep track of which targets have been built
    array common built

    # The default target to build
    common defaultTarget

    # This proc builds the requested target (in direct pathname form), 
    # and returns the time at which the target was built.
    proc Build {{target {}}} {
        if {[string match {} $target]} {
            set target $defaultTarget
        }
	if {[info exists options(-d)]} {
	    puts stdout "${pfx}Target: $target"
	}
        if {[info exists built($target)]} {
            if {[string match pending $built($target)]} {
                Oc_Log Log "Circular dependency:\n\t'$target'\
                        (ultimately) depends on itself.\n\tDropping\
                        that dependency." warning $class
                # Didn't build target, so don't make a timestamp.
                # Report back that this target is very old so that it
                # won't trigger other updates.
                return 0
            }
            # The target has already been built.  
            # Return its update time.
            return $built($target)
        }
        # The target hasn't been built yet.  
        # Find the rule for how to build it.
        global errorInfo errorCode
        if {[catch {$class FindRule $target} rule]} {
            foreach {ei ec} [list $errorInfo $errorCode] {}
            set emsg $rule
            set prune "\n    ('$class' proc 'FindRule'"
        }
        if {![info exists emsg] && [string match {} $rule]} {
            # No rule to make this target.  See if it's a file.
            if {[file exists $target]} {
                # Save and return the file's timestamp 
                # as the update time for this target.
                if {[file isdirectory $target]} {
                    set built($target) 0
                } else {
                    set built($target) [file mtime $target]
                }
                return $built($target)
            }
            set emsg "Don't know how to make '$target'"
            set ei "$emsg\n"
            set ec NONE
            set prune "\n"
            incr numErrors
        }
        if {![info exists emsg]} {
            # Invoke the rule to build the target
            set built($target) pending	;# safeguard against circularity
            if {![catch {$rule Invoke $target} msg]} {
                set built($target) $msg
                return $built($target)
            }
            foreach {ei ec} [list $errorInfo $errorCode] {}
            set emsg $msg
            set prune "\n    ('$class' instance method 'Invoke'"
        }
        set built($target) -1
        set index [string last $prune $ei]
        set ei [string trimright [string range $ei 0 $index]]
        error $emsg "$ei\n    (while building target '$target')" $ec
    }

    proc Define {definition} {
        if {[string match {} [info script]]} {
            return -code error "'$class Define' must be in a makerules file"
        }
        set rulesFile [Oc_DirectPathname [file join [pwd] [info script]]]
        set filesRead($rulesFile) 1
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
        if {[info exists temp(-ruleType)] \
                && [string match default $temp(-ruleType)]} {
            set directory [pwd]
        } else {
            set directory [file dirname $rulesFile]
        }
        array set temp [list -rulesFile $rulesFile -directory $directory]
        if {[catch {[eval $class New _ [array get temp]] Initialize} msg]} {
            return -code error $msg
        }
    }

    # Search for rule which has the argument as a target.
    proc FindRule { target } {
        # If we already know a rule for this target, return it.
        if {[info exists ruleWhichMakes($target)]} {
            return $ruleWhichMakes($target)
        } 
        # There could be a rule for this target in another makerules
        # file.  We search for the file 'makerules.tcl' starting in the
        # directory of the target upward until
        #  (1) We find a rule for the target
        #  (2) We find a directory w/o a makerules.tcl file
        #  (3) Sourcing makerules.tcl returns "continue"
        #  (4) We reach the root of the file system
        # The second option is meant to detect climbing past the start
        # of the oommf install tree, but would also fail if target lies
        # in the platform-specific directory that (usually) doesn't
        # contain a makerules.tcl file. To allow for this we start up
        # one level if the target directory doesn't have makerules.tcl.
        set dir [file dirname $target]
        if {![file exists [file join $dir makerules.tcl]]} {
            set dir [file dirname $dir]
        }
        while {![info exists ruleWhichMakes($target)]
                 && [string compare $dir [file dirname $dir]]} {
            set rulesFile [file join $dir makerules.tcl]
            if {[catch {$class SourceRulesFile $rulesFile} msg]} {
                global errorInfo errorCode
                set index [string last "\n    ('$class' proc\
                        'SourceRulesFile'" $errorInfo]
                set ei [string trimright \
                        [string range $errorInfo 0 $index]]
                error $msg "$ei\n    (while finding rule for target\
                        '$target')" $errorCode
            }
            if {$msg == 0} {
                # Either no makerules.tcl file in this directory,
                # or else it returned "continue" when sourced.
                break
            }
            set dir [file dirname $dir]
        }
        # Check again to see if we now know a rule for the target.
        if {[info exists ruleWhichMakes($target)]} {
            return $ruleWhichMakes($target)
        }  else {
            return ""
        }
    }

    proc Offset {} {
        return $offset
    }

    proc SourceRulesFile {{rulesFile {}}} {
        if {[string match {} $rulesFile]} {
            set rulesFile makerules.tcl
        }
        set rulesFile [Oc_DirectPathname [file join [pwd] $rulesFile]]
        if {[info exists filesRead($rulesFile)]} {
            # We've already read the file.  No need to read it again.
            # Tell caller we've read the file.
            return $filesRead($rulesFile)
        }
        if {[file readable $rulesFile] && [file isfile $rulesFile]} {
            set filesRead($rulesFile) 1
            set savedir [pwd]
            set code [catch {
                cd [file dirname $rulesFile]
                source $rulesFile
                eval $defaultRules
            } msg] 
            global errorInfo errorCode
            foreach {ei ec} [list $errorInfo $errorCode] {}
            cd $savedir
            if {$code == 1} { ;# error
                incr numErrors
            } elseif {$code == 4} {	;# continue
                set filesRead($rulesFile) 0
                return 0
            } else {
                return 1
	    }
        } else {
            set filesRead($rulesFile) 0
            return 0
        }
        set index [string last "\n    invoked from within" $ei]
        set ei [string trimright [string range $ei 0 $index]]
        error $msg "$ei\n    (reading makerules from file\
                '$rulesFile')" $ec
    }

    # A Tcl list of the targets of this MakeRule. 
    # Targets must be strings which have the form of relative pathnames 
    # which do not include the component '..'
    # This guarantees that each target refers to a (possibly mythical) file 
    # in or below the directory containing the makerules file in which this 
    # MakeRule is defined.
    #
    # PORTABILITY NOTE: The validation script will need revision if we ever
    # port to a platform on which it is some pathname component other than
    # '..' which means 'parent of this directory'.
    const public variable targets {
        if {[catch {set targets [lrange $targets 0 end]} msg]} {
            error "Invalid targets list: $targets
	Not a valid Tcl list: $msg"
        }
        foreach _ $targets {
            if {![string match relative [file pathtype $_]]} {
                error "Invalid target: $_
	All targets of a MakeRule must appear to be relative pathnames."
            }
            if {[lsearch -exact [file split $_] ..] >= 0} {
                error "Invalid target: $_
	MakeRule targets may not contain '..' as a pathname component."
            }
        }
    }

    # A Tcl list of the dependencies of the MakeRule, as given in the
    # MakeRule definition.
    # Each dependency should be a string in the form of a legal pathname,
    # which will be interpreted relative to the directory containing the
    # makerules file containing the definition of this MakeRule.
    const public variable dependencies = {} {
        foreach _ $dependencies {
            if {[catch {file split $_} __]} {
                error "Invalid dependency: $_
	$__"
            }
        }
    }
    # The dependencies of the rule expressed as direct, absolute pathnames
    private variable directDependencies

    # The script to evaluate to update the targets when they are out of
    # date compared with the dependencies.
    const public variable script = {} {
        if {![info complete $script]} {
            error "Value of -script option is not a complete Tcl script:
	$script"
        }
    }

    # The absolute pathname of the directory relative to which the
    # targets and dependencies are treated as filenames
    const public variable directory {
        # Possibly add other requirements here?
        if {[string match absolute [file pathtype $directory]] \
                && [file isdirectory $directory]} {
            # Ok!
        } else {
            error \
"Value of -directory option is not the absolute pathname of a directory:
	$directory"
        }
    }

    # The file in which this rule is defined.
    const public variable rulesFile 

    # The type of rule this is (plain or default)
    const public variable ruleType = plain {
        if {![string match plain $ruleType] \
                && ![string match default $ruleType]} {
            error "legal values: plain default"
        }
    }

    Constructor {args} {
        eval $this Configure $args
    }

    method Initialize {} {
        foreach t $targets {
            set absTarget [file join $directory $t]
            if {![info exists defaultTarget]} {
                set defaultTarget $absTarget
            }
            if {[info exists ruleWhichMakes($absTarget)]} {
                if {[string match plain $ruleType]} {
                    Oc_Log Log "Overriding previous rule for\
                            '$absTarget':\n\nOld rule\
                            was:\n[$ruleWhichMakes($absTarget) \
                            Description]\n\nNew rule is: [$this \
                            Description]\n" warning $class
                    set ruleWhichMakes($absTarget) $this
                }
            } else {
                set ruleWhichMakes($absTarget) $this
            }
        }
        set directDependencies {}
        foreach d $dependencies {
            lappend directDependencies [Oc_DirectPathname \
                    [file join $directory $d]]
        }
    }

    method Description {} {
        if {[string match default $ruleType]} {
            set msg "Default rule:\n"
        } else {
            set msg "In '$rulesFile':\n"
        }
        append msg "\tTargets: $targets\n\tDependencies:\
                $dependencies\n\tScript: $script"
        return $msg
    }

    method BuildDependencies {} {
        set latest 0
        foreach dep $directDependencies {
            if {[catch {
                set time [$class Build $dep]
            } msg]} {
                if {[info exists options(-k)]} {
                    Oc_Log Log "Continuing after\
                            error:\n-----\n$msg\n-----" warning
                    # Prevent building of dependent targets
                    set time -1
                } else {
                    global errorInfo errorCode
                    foreach {ei ec} [list $errorInfo $errorCode] {}
                    set emsg $msg
                    set prune "\n    ('$class' proc 'Build'"
                    break
                }
            }
            if {$latest >= 0} {
                if {$time < 0} {
                    set latest -1
                } elseif {$time > $latest} {
                    set latest $time
                }
            }
        }
        if {![info exists emsg]} {
            return $latest
        }
        set index [string last $prune $ei]
        set ei [string trimright [string range $ei 0 $index]]
        error $emsg $ei $ec
    }

    method Invoke {target} {
        global errorInfo errorCode
	if {[info exists options(-d)]} {
	    puts stdout "$pfx depends on:"
	    append pfx "  "
	}
        if {[catch {$this BuildDependencies} latest]} {
            foreach {ei ec} [list $errorInfo $errorCode] {}
            set emsg $latest
            set prune "\n    ('$class' instance method\
                    'BuildDependencies'"
        }
	if {[info exists options(-d)]} {
	    set pfx [string range $pfx 0 [expr {[string length $pfx] - 3}]]
	}
        # All dependencies up to date. 'latest' holds the most recent
        # modification time of any of them.  Check if that is more 
        # recent than the modification time of our target.
        if {![info exists emsg] \
                && ((![file exists $target] && $latest >= 0) \
                    || ([file isfile $target] \
                        && [file mtime $target] < $latest))} {
            # Be sure the script is evaluated with the current working
            # directory set to the directory containing the file which
            # contains the definition of this MakeRule
            set savedir [pwd]
            cd $directory
            set prior $numErrors
            set code [catch {uplevel #0 $script} msg]
            foreach {ei ec} [list $errorInfo $errorCode] {}
            cd $savedir
            if {$code == 1} {
                # Don't double count errors when script invoke MakeRule
                # methods or procs
                if {$prior == $numErrors} {
                    incr numErrors
                }
                # Only halt processing on an error (return code = 1)
                # Ignore other return codes: ok, break, continue, return
                if {[info exists options(-i)]} {
                    Oc_Log Log "Ignoring error:\n-----\n$msg\n-----" \
                            warning 
                } else {
                    set emsg $msg
                    set prune "\n    (\"uplevel"
                }
            }
        }
        # The script was evaluated if necessary, report to caller the
        # build time of the target.
        if {![info exists emsg]} {
            if {[file isfile $target]} {
                return [file mtime $target]
            }
            if {[file isdirectory $target]} {
                return 0
            }
            return [expr {[clock seconds] + $offset}]
        }
        set index [string last $prune $ei]
        set ei [string trimright [string range $ei 0 $index]]
        error $emsg $ei $ec
    }

}

