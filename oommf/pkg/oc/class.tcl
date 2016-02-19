# FILE: class.tcl
#
# Defines a command 'Oc_Class' which support object-oriented programming 
# in Tcl.
#
# Last modified on: $Date: 2015/09/15 13:50:42 $
# Last modified by: $Author: dgp $
# 
# The set of procs found in every Oc_Class is:
# 
# $class GlobalName $common
#     Return Tcl global name for the actual variable.
# 
# $class Instances
#     Return list of instances of $class
# 
# $class New $iVarName ?{*}$options?
#     Create new instance of $class and store name in
#     variable $iVarName. Invoke constructor with $options
# 
# $class Options ?$pattern? ?$access?
#     Return valid args for either New or Configure/Cget
#     matching $pattern
# 
# $class Undefine
#     Destroy the $class
# 
# $class WinBasename
#     Utility for Tk naming conventions.
# 
# $class Methods ?$pattern?
# $class Procs ?$pattern?
# 
#     Unfinished. Broken. Do not use until they are completed.
# 
# The set of methods available for every Oc_Class instance is:
# 
# $instance Class
#     Return the class of $instance
# 
# $instance Configure ?{*}$options?
#     Instance variable query/set inspired by Tk
# 
# $instance Cget $option
#     Instance variable query inspired by Tk
# 
# $instance Delete
#     Destroy $instance.
# 
# $instance GlobalName $varName
# $instance WinBasename
# 
#     Just like their $class counterparts. 
# 
# DOME:
#
# ClassConfigure proc
#
# Reconsider value of indirect access to array commons and variables
#	What good does it do (perhaps inheritance support)?  
#	Aren't conventions enough?
#
# Carefully evaluate consequences of variable and common names 
#	containing spaces.  Are they broken?  
#	Should they be fixed or prevented?
#
# Improve robustness of calls to Undefine and to Delete.  Currently
#	some operations remain possible (setting elements in array
#	variables) after an instance is supposedly gone.
#
# Improve robustness of __Oc_Class_FixStackTrace.  Currently if
#	a real command plus class name is longer than Tcl's
#	errorInfo truncation length, or if the real command
#	is self-simiilar, it can fail.  
#
# Support procs Methods and Procs
#
# Support callback
#
# Split storage and command modifiers
#
# Add option interface.
#
# Add event declarations.
# 
# Add garbage collection
#
# Consider traces to discourage direct writes to the "hidden" data
#	structures
#
# Add boilerplate procs 'about' and 'help'
#	Extract documentation in class compiler step
#
# Perhaps a method to write a reproduction script
#
# factor out common code into utility procs
#
# Implement Tcl8.0 version w/namespaces.
#
# Hook to report global litter
#
# Template capability
#
########################################################################

########################################################################
# The main command for this file: Oc_Class
########################################################################
# The proc 'Oc_Class' takes two arguments 'class' and 'definition'.
# It creates a global array with the name _$class and stores in it all
# the data necessary to describe the class defined by the argument
# 'definition'.
#
proc Oc_Class {class definition} {
    global _Oc_Class
    upvar #0 _$class classDef
    if {[info exists classDef]} {
        return -code error "Can't create Oc_Class '$class':\n\tGlobal\
                namespace conflict."
    }

    # Enable error logging from this class as a message source
    if {[string compare Oc_Log $class]} {
        Oc_Log AddSource $class
    }

    # DOME: Consider the alternative of evaluating $definition in a
    #	slave interpreter with appropriate commands.
    #	PRO: Fewer commands polluting the master interp cmd namespace
    #		Likely better performance than parsing at Tcl level.
    #		Evaluation order of initialization of commons would be
    #		correct automatically.
    #	CON: Complications of inter-interpreter communications
    #	PRO?/CON?: Would allow arbitrary Tcl code to be evaluated within
    #		the class definition.
    # 
    #	For now, we'll do all the parsing in the main interpreter
    #
    # Parse the class definition into a sequence of commands
    if {[catch {__Oc_ClassParseIntoCommands $definition commands} m]} {
        error $m "$m\n    (syntax error in Oc_Class '$class'\
                definition)" [list OC $class]
    }

    # DOME: When inheritance is supported, we'll need to detect an 
    #	'inherit' command on the command list at this point and
    #	direct the subcommand search to a different alias to handle
    #	the more complicated (and lower-performance) case of classes
    #	which support inheritance.
    #
    # Register the command defined by this Oc_Class
    interp alias {} $class {} __Oc_ClassEvalSimpleProc $class 
    set _Oc_Class(#$class) $class
    set classDef(#nextInstance) 0
    set classDef(#instances) {}

    # DOME: When inheritance is supported, consider the alternative of
    #	not having (as many) built-in procs and methods with boiler
    # 	plate implementations, but instead placing these implementations
    #	in an Oc_Class named Oc_Object which is the root of an 
    #	inheritance hierarchy.
    #
    #	For now, keep the built-in/boiler plate approach.
    #
    # Cache the boiler plate commands so we don't pay the cost of
    # constructing them for every class.
    foreach {i type} {P Proc M Method} {
        if {[catch {
            foreach _ $_Oc_Class(boilerplate${type}s) {
                set classDef($i$_) __Oc_ClassBoilerplate$type$_
            }
        }]} {
            foreach _ [info commands __Oc_ClassBoilerplate$type*] {
                regexp ^__Oc_ClassBoilerplate${type}(.*)$ $_ match _
                lappend _Oc_Class(boilerplate${type}s) $_
                set classDef($i$_) __Oc_ClassBoilerplate$type$_
            }
        }
    }

    # The default ClassConstructor (empty)
    set classDef(HClassConstructor) __${class}_HClassConstructor
    proc __${class}_HClassConstructor {class} {
    }

    # The default ClassDestructor (empty)
    set classDef(HClassDestructor) __${class}_HClassDestructor
    proc __${class}_HClassDestructor {class} {
    }

    # The default Constructor (empty)
    set classDef(HConstructor) __${class}_HConstructor
    proc __${class}_HConstructor {class this} {
        return {}
    }

    # The default Destructor (empty)
    set classDef(HDestructor) __${class}_HDestructor
    proc __${class}_HDestructor {class this} {
    }

    # There are 16 types of storage identifiers (variables/commons),
    # as follows: [A|S][C|M][U|R][C|V] where
    #   A|S --> Array or Scalar
    #   C|M --> Constant or Mutable
    #   U|R --> pUblic or pRivate
    #   C|V --> Common or Variable
    # Keep a list of each type as part of the class definition.
    # Initialize lists to be empty
    foreach type {ACUV AMUV SCUV SMUV ACUC AMUC SCUC SMUC
                  ACRV AMRV SCRV SMRV ACRC AMRC SCRC SMRC} {
        set classDef($type) {}
    }

    # Evaluate the sequence of commands to define the class.  
    # Add the class name as an argument to the evaluation procs.
    foreach cmd $commands {
        set keyword [lindex $cmd 0]
        if {[catch {
            __Oc_ClassDefine$keyword $class [lrange $cmd 1 end]
        } msg]} {
            if {![string match {} \
                    [info commands __Oc_ClassDefine$keyword]]} {
                set msg "Syntax error: $msg\n\tin the following text:\
                        \n-----\n$cmd\n-----"
                error $msg "$msg\n    (syntax error in Oc_Class\
                        '$class' definition)[__Oc_ClassCallUndefine \
                        $class]" [list OC $class]
            } else {
                set msg "Syntax error: Unknown keyword '$keyword'\n\t\
                        in the following text:\n-----\n$cmd\n-----"
                error $msg "$msg\n    (syntax error in Oc_Class\
                        '$class' definition)[__Oc_ClassCallUndefine \
                        $class]" [list OC $class]
            }
        }
    }

    # The second pass completes the construction of the class definition
    # based on the work done by the evaluation of the commands above
    foreach _id [array names classDef _*] {
        set id [string range $_id 1 end]
        lappend classDef($classDef($_id)) $id
        if {[info exists classDef(V$id)]} {
            __Oc_ClassInstanceProcGeneratingProc __${class}_V$id {} \
                    $classDef(V$id) ""
            set classDef(V$id) __${class}_V$id
        }
        unset classDef($_id)
    }

    # Assign default values to all commons which have them
    # First the scalars...
    foreach type {SCUC SMUC SCRC SMRC} {
        foreach common $classDef($type) {
            if {[info exists classDef(=$common)]} {
                set classDef(_$common) $classDef(=$common)
            }
        }
    }
    # Then the arrays...
    foreach type {ACUC AMUC ACRC AMRC} {
        foreach common $classDef($type) {
            set classDef(_$common) __${class}_A$common
            global $classDef(_$common)
            if {[info exists classDef(=$common)]} {
                if {[catch {
                    array set $classDef(_$common) $classDef(=$common)
                } msg]} {
                    set msg "Invalid default value for '$class' array\
                            common '$common':\n\
                            \t\{$classDef(=$common)\}\n\t$msg"
                    error $msg "$msg\n    (error in Oc_Class '$class'\
                            definition)[__Oc_ClassCallUndefine \
                            $class]" [list OC $class]
                }
            }
        }
    }

    # For all public commons, query the Oc_Option database.
    # First the scalars...
    foreach type {SCUC SMUC} {
        foreach common $classDef($type) {
            Oc_Option Get $class $common classDef(_$common)
        }
    }
    # Then the public arrays...
    foreach type {ACUC AMUC} {
        foreach common $classDef($type) {
            if {![Oc_Option Get $class $common tmp]} {
                if {[catch {
                    array set $classDef(_$common) $tmp
                } msg]} {
                    set msg "Invalid configuration value for '$class'\
                            array common '$common':\n\
                            \t\{$classDef(=$common)\}\n\t$msg"
                    error $msg "$msg\n    (error in Oc_Class '$class'\
                            configuration)[__Oc_ClassCallUndefine \
                            $class]" [list OC $class]
                }
            }
        }
    }

    # DOME: Consider the possibility of allowing arguments to the
    # ClassConstructor -- then template classes could be supported.
    # Would need some re-design of the Oc_Class command though.

    # Try to evaluate the ClassConstructor
    if {[catch {uplevel 1 [list $classDef(HClassConstructor) $class]} msg]} {
        # The ClassConstructor failed, save the error state.
        global errorInfo errorCode
        foreach {cei cec} [list $errorInfo $errorCode] {}
        eval [__Oc_ClassFixStackTrace $msg $class \
                $classDef(HClassConstructor) \
                "'$class' ClassConstructor" \
                [__Oc_ClassCallUndefine $class] $cei $cec]
    }

    # Check that all const commons are initialized, and use trace
    # to force them to keep their constant value from now on.
    set constants {}
    foreach type {SCUC SCRC ACUC ACRC} {
	eval lappend constants $classDef($type)
    }
    if {![llength $constants]} {
	return
    }
    set ec 0
    Oc_Option Get Oc_Class enforceConst ec
    if {[catch {expr {$ec && $ec}}]} {
	set msg "Bad value for Oc_Class option 'enforceConst':\n\t$ec\nMust"
	set ec 0
	append msg " be a boolean.\nSetting Oc_Class option 'enforceConst'"
	append msg " to:\n\t$ec"
	Oc_Log Log $msg warning Oc_Class
	Oc_Option Add * Oc_Class enforceConst $ec
    }
    if {$ec} {
        global _$class
        foreach type {SCUC SCRC} {
            foreach constCommon $classDef($type) {
                if {[info exists classDef(_$constCommon)]} {
                    trace variable _${class}(_$constCommon) wu [list \
                            __Oc_ClassConstScalar _$class \
                            _$constCommon $classDef(_$constCommon)]
                } else {
                    set msg "No value provided for const common\
                            '$constCommon' in Oc_Class '$class'"
                    error $msg $msg[__Oc_ClassCallUndefine $class] \
                            [list OC $class]
                }
            }
        }
        foreach type {ACUC ACRC} {
            foreach constCommon $classDef($type) {
                global $classDef(_$constCommon)
                if {[array exists $classDef(_$constCommon)]} {
                    trace variable $classDef(_$constCommon) wu \
                            [list __Oc_ClassConstArray _$class \
                            _$constCommon [array get \
                            $classDef(_$constCommon)]]
                } else {
                    set msg "No array value provided for const common\
                            '$constCommon' in class '$class'"
                    error $msg $msg[__Oc_ClassCallUndefine $class] \
                            [list OC $class]
                }
            }
        }
    }
}

########################################################################
# Utility commands which support Oc_Class and the commands
# created by it.
########################################################################

# The following proc parses a class definition body into a sequence of 
# commands.  Each command in the sequence becomes one element in a list
# stored in the variable in the calling frame with the name $resultName.
;proc __Oc_ClassParseIntoCommands {body resultName} {
    upvar $resultName commands
    set commands {}
    set chunk ""
    foreach line [split $body "\n"] {
        append chunk $line
        if {[info complete $chunk]} {
            # $chunk ends in a complete Tcl command, and none of the
            # newlines within it end a complete Tcl command.  If there
            # are multiple Tcl commands in $chunk, they must be 
            # separated by semi-colons.
            set cmd ""
            foreach part [split $chunk ";"] {
                append cmd $part
                if {[info complete $cmd]} {
                    set cmd [string trimleft $cmd]
                    # Drop empty commands and comments
                    if {![string match {} $cmd] \
                            && ![string match #* $cmd]} {
                        lappend commands $cmd
                    }
                    if {[string match #* $cmd]} {
                        set cmd "#;"
                    } else {
                        set cmd ""
                    }
                } else {
                    # No complete command yet.  
                    # Replace semicolon and continue
                    append cmd ";"
                }
            }
            set chunk ""
        } else {
            # No end of command yet.  Put the newline back and continue
            append chunk "\n"
        }
    }
    if {![string match {} [string trimright $chunk]]} {
        return -code error "Can't parse Oc_Class definition body into a\
                sequence of commands.\n\tIncomplete\
                command:\n-----\n$chunk\n-----"
    }
}

# When a proc (subcommand) of a class is invoked, the following routine
# finds the corresponding proc to evaluate and does so.  This routine
# is called when there is no inheritance to deal with.
;proc __Oc_ClassEvalSimpleProc {class proc args} {
    upvar #0 _$class classDef
    switch [catch {
	uplevel 1 [linsert $args 0 $classDef(P$proc) $class]
    } result] {
        0 {	;# ok
            return $result
        }
        1 {	;# error
            # Something went wrong - diagnose
            if {![string match Undefine $proc]} {
                # After Undefine, classDef is gone 
                if {![array exists classDef]} {
                    set msg "Bad 'class' argument '$class' in call to\
                            __Oc_ClassEvalSimpleProc"
                    error $msg $msg {OC Oc_Class}
                }
                if {![info exists classDef(P$proc)]} {
                    # DOME: Add list of valid procs to the error message
                    return -code error "Invalid class proc '$proc' for\
                            class '$class'"
                }
                if {[string match {} [info commands \
                        $classDef(P$proc)]]} {
                    set msg "Missing class proc implementation\
                            '$classDef(P$proc)'\n\t for '$class $proc'"
                    error $msg $msg {OC Oc_Class}
                }
            }
            if {[string match Undefine $proc]} {
                # Undefine unsets classDef
                eval [__Oc_ClassFixStackTrace $result $class \
                        __Oc_ClassBoilerplateProcUndefine \
                        "'$class' proc '$proc'"]
            } else {
                eval [__Oc_ClassFixStackTrace $result $class \
                        $classDef(P$proc) "'$class' proc '$proc'"]
            }
        }
        2 {	;# return
            return -code 2 $result
        }
        3 {	;# break
            return -code 3 $result
        }
        4 {	;# continue
            return -code 4 $result
        }
        default {
            return -code error -errorcode [list OC $class] "Unsupported\
                    return code (> 4) from '$class' proc '$proc'"
        }
    }
# NOTE: Some explanation of the last few arms of the switch statement
# above is in order.  It would be more compact, and would allow support
# for extended return codes (> 4), if the last few arms could be
# written as a default arm:
# default {
#     return -code $code $result
# }
# However, this would require that we set the value of a variable 'code'
# to be the return value of the [catch].  This variable setting step
# would be additional overhead slowing down *every* call made to this 
# proc.  We've chosen to favor speedy execution covering all return 
# codes built into Tcl over support for extended return codes.  If some 
# other code uses extended return codes and needs to operate with 
# Oc_Class, support for those return codes may be explicitly added 
# above, as for return, break, and continue.
# An enhancement to the [switch] command which made the value of its
# string argument available to the bodies through some substitution 
# mechanism (like & in [regsub]) would make this tradeoff unnecessary.
}

# When a method of a class instance is invoked, the following routine
# finds the corresponding proc to evaluate and does so.  This routine
# is called when there is no inheritance to deal with.
;proc __Oc_ClassEvalSimpleMethod {class this method args} {
    upvar #0 _$class classDef
    switch [catch {
        uplevel 1 [linsert $args 0 $classDef(M$method) $class $this]
    } result] {
        0 {	;# ok
            return $result
        }
        1 {	;# error
            # Something went wrong - diagnose
            #
            # DOME: Handle case where [uplevel] above unsets classDef.
            #		(say, by calling the class proc Undefine)
            if {![array exists classDef]} {
                set msg "Bad 'class' argument '$class' in call to\
                        __Oc_ClassEvalSimpleMethod"
                error $msg $msg {OC Oc_Class}
            }
            if {![info exists classDef(M$method)]} {
                # DOME: Add list of valid methods to the error message
                return -code error "Invalid instance method '$method'\
                        for class '$class'"
            }
            if {[string match {} [info commands $classDef(M$method)]]} {
                set msg "Missing instance method implementation\
                        '$classDef(M$method)'\n\tfor '$class' instance\
                        method '$method'"
                error $msg $msg {OC Oc_Class}
            }
            eval [__Oc_ClassFixStackTrace $result $class \
                    $classDef(M$method) \
                    "'$class' instance method '$method'"]
        }
        2 {	;# return
            return -code 2 $result
        }
        3 {	;# break
            return -code 3 $result
        }
        4 {	;# continue
            return -code 4 $result
        }
        default {
            return -code error -errorcode [list OC $class] "Unsupported\
                    return code (> 4) from '$class' method '$method'"
        }
    }
    # NOTE: See the note at the bottom of __Oc_ClassEvalSimpleProc.
}

;proc __Oc_ClassConstScalar {array slot value name1 name2 ops} {
    upvar #0 $array foo
    if {[array exists foo]} {
        set foo($slot) $value
        if {[string match u $ops]} {
            global $array
            trace variable ${array}($slot) wu \
                    [list __Oc_ClassConstScalar $array $slot $value]
        }
        return -code error "\n\tAttempt to modify const variable or\
                common '[string range $slot 1 end]'"
    }
}

;proc __Oc_ClassConstArray {array slot value name1 name2 ops} {
    upvar #0 $array foo
    if {[array exists foo]} {
        global $foo($slot)
        # Temporarily suppress traces
        trace vdelete $foo($slot) wu [list __Oc_ClassConstArray $array \
                $slot $value]
        if {[string match w $ops]} {
            # Remove bogus entry
            upvar #0 $foo($slot) link
            catch {unset link($name2)}
        }
        # Restore constant value
        array set $foo($slot) $value
        # Restore traces
        trace variable $foo($slot) wu [list __Oc_ClassConstArray \
                $array $slot $value]
        return -code error "\n\tAttempt to modify const array variable\
                or common '[string range $slot 1 end]'"
    }
}

;proc __Oc_ClassCallUndefine {class} {
    if {[catch {$class Undefine}]} {
        global errorInfo
        return "\n    (undefining Oc_Class '$class'...)\n====> Error in\
                '$class Undefine'\n====> Start stack\
                trace\n$errorInfo\n<==== End stack trace\n<==== Error\
                in '$class Undefine'"
    }
    return "\n    (undefining Oc_Class '$class'...)"
}

;proc __Oc_ClassFixStackTrace {msg class realCmd fakeCmd {extra {}}
        {ei {}} {ec {}}} {
    if {![string length $ec]} {
        global errorCode
        set ec $errorCode
        if {![string length $ei]} {
            global errorInfo
            set ei $errorInfo
        }
    }
    # If no one else has claimed the error, claim it.
    if {[string match NONE [lindex $ec 0]]} {
        set ec [list OC $class]
    }
    regsub -all "\"$realCmd\"" $msg $fakeCmd msg
    set index [string last "\n\"$realCmd $class" $ei]
    set prefix [string trimright [string range $ei 0 $index]]
    # This case is awkward.  It makes the result too long, but is
    # the only may to be sure the $extra info gets place on the
    # stack trace.
    if {[regexp {while (compil|execut)ing$} $prefix]} {
        return [list return -code error -errorcode $ec $msg$extra]
    }
    if {[regexp {invoked from within$} $prefix]} {
        set index [string last "\n" $prefix]
        set prefix [string trimright [string range $prefix 0 $index]]
    }
    if {[regexp { line [0-9]+\)$} $prefix match]} {
        set index [string last ( $prefix]
        set prefix [string range $prefix 0 $index]
        append prefix $fakeCmd$match
    }
    return [list return -code error -errorinfo $prefix$extra \
            -errorcode $ec $msg]
}

########################################################################
# End of utility commands 
########################################################################
# boilerplate implementations of the built-in procs and methods
# provided with every Oc_Class
########################################################################

;proc __Oc_ClassBoilerplateProcGlobalName {class varname} {
    # Still needed after events/options are in place?
    upvar #0 _$class classDef
    # Search through all the commons for any named $varname
    foreach type {ACUC AMUC ACRC AMRC} {
        if {[lsearch -exact $classDef($type) $varname] >= 0} {
            return $classDef(_$varname)
        }
    }
    foreach type {SCUC SMUC SCRC SMRC} {
        if {[lsearch -exact $classDef($type) $varname] >= 0} {
            return _${class}(_$varname)
        }
    }
    return -code error "Invalid argument: varname:\
            '$varname'\n\t'$varname' is not a '$class' common"
}

;proc __Oc_ClassBoilerplateProcInstances {class} {
    upvar #0 _$class classDef
    return $classDef(#instances)
}

;proc __Oc_ClassBoilerplateProcMethods {class {pattern *} 
        {access callable}} {
    # This will probably require modification to support inheritance
    # How to get boilerplate into these lists?
    upvar #0 _$class classDef
    switch -- $access {
        callback {
            set l $classDef(#callbackMethods)
        }
        public {
            set l $classDef(#publicMethods)
        }
        private {
            set l $classDef(#privateMethods)
        }
        all {
            set l [concat $classDef(#publicMethods) \
                    $classDef(#privateMethods) \
                    $classDef(#callbackMethods)] 
        }
        callable {
            if {![catch {info level -1} caller] && \
                    ([string match $class [lindex $caller 0]] || \
                     [lsearch -exact $classDef(#instances) \
                     [lindex $caller 0]] >= 0)} {
                set l [concat $classDef(#publicMethods) \
                        $classDef(#privateMethods) \
                        $classDef(#callbackMethods)]
            } else {
                set l $classDef(#publicMethods)
            }
        }
        default {
            return -code error "Invalid argument: access:\
                    '$access'\n\tMust be one of:\n\tcallback public\
                    private all callable"
        }
    }
    set ret {}
    foreach m $l {
        if {[string match $pattern $m]} {
            lappend ret $m
        }
    }
    return [lsort $ret]
}

;proc __Oc_ClassBoilerplateProcNew {class objref args} {
    upvar #0 _$class classDef
    upvar $objref object
    # Make a global array to hold the instance state.
    set this _${class}$classDef(#nextInstance)
    incr classDef(#nextInstance)
    upvar #0 _$this state
    if {[array exists state]} {
        return -code error "Can't create '$class' instance\
                '$this':\n\tGlobal namespace conflict."
    }

    # Define the instance command
    interp alias {} $this {} __Oc_ClassEvalSimpleMethod $class $this

    # Create default Bindtags
    Oc_EventHandler Bindtags $this [list $this $class]

    # Assign default values to all variables which have them
    # First the scalars...
    foreach type {SCUV SMUV SCRV SMRV} {
        foreach variable $classDef($type) {
            if {[info exists classDef(=$variable)]} {
                set state(_$variable) $classDef(=$variable)
            }
        }
    }
    # Then the arrays...
    foreach type {ACUV AMUV ACRV AMRV} {
        foreach variable $classDef($type) {
            set state(_$variable) __${this}_A$variable
            global $state(_$variable)
            if {[info exists classDef(=$variable)]} {
                if {[catch {
                    array set $state(_$variable) $classDef(=$variable)
                } msg]} {
                    set msg "Invalid default value for '$class' array\
                            variable '$variable':\n\
                            \t\{$classDef(=$variable)\}\n\t$msg"
                    error $msg "$msg\n    (error in Oc_Class '$class'\
                            definition)\n    (deleting new '$class'\
                            instance...)[__Oc_ClassCallDelete $class \
                            $this]"
                }
            }
        }
    }

    # Try to evaluate the Constructor
    if {[catch {
        uplevel 1 [linsert $args 0 $classDef(HConstructor) $class $this]
    } msg]} {
        # The Constructor failed, save the error state.
        global errorInfo errorCode
        foreach {cei cec} [list $errorInfo $errorCode] {}
        eval [__Oc_ClassFixStackTrace $msg $class \
                $classDef(HConstructor) "'$class' Constructor" \
                "\n    (deleting new '$class'\
                instance...)[__Oc_ClassCallDelete $class $this]" \
                $cei $cec]
    }

    ####################################################################
    # The variable 'msg' holds the return value from the Constructor
    #
    # If the Constructor doesn't return a value (that is, it returns the
    # default empty string), then 'New' should return the value of the
    # 'this' pointer it's been building up.  This is the usual case.
    #
    # However, if the Constructor returns a value, then 'New' will 
    # return that value as the value of the 'this' pointer.  This 
    # mechanism allows the Constructor to be written so that a call to 
    # 'New' doesn't return a pointer to a new instance, but a pointer to
    # an existing instance.  This is useful for implementing classes in 
    # which the objects have identity, and there should be no identical 
    # copies floating around.  Of course, in these cases, there's a 
    # (possibly half-constructed) instance pointed to by 'this' which 
    # must be disposed of.  That task is left to the Constructor.  It 
    # creates the need for cleanup, so it is stuck with the burden of 
    # that cleanup.
    ####################################################################
    if {![string match {} $msg]} {
        set object $msg
        return $msg
    }
    # Check that all const variables are initialized, and use trace
    # to force them to keep their constant value from now on.
    set constants {}
    foreach type {SCUV SCRV ACUV ACRV} {
	eval lappend constants $classDef($type)
    }
    if {[llength $constants]} {
    set ec 0
    Oc_Option Get Oc_Class enforceConst ec
    if {[catch {expr {$ec && $ec}}]} {
	set msg "Bad value for Oc_Class option 'enforceConst':\n\t$ec\nMust"
	set ec 0
	append msg " be a boolean.\nSetting Oc_Class option 'enforceConst'"
	append msg "to:\n\t$ec"
	Oc_Log Log $msg warning Oc_Class
	Oc_Option Add * Oc_Class enforceConst $ec
    }
    if {$ec} {
        global _$this
        foreach type {SCUV SCRV} {
            foreach constVar $classDef($type) {
                if {[info exists state(_$constVar)]} {
                    trace variable _${this}(_$constVar) wu [list \
                            __Oc_ClassConstScalar _$this _$constVar \
                            $state(_$constVar)]
                } else {
                    set msg "No value provided for const variable\
                            '$constVar' in '$class' Constructor"
                    error $msg "$msg\n    (deleting new '$class'\
                            instance...)[__Oc_ClassCallDelete $class \
                            $this]" [list OC $class]
                }
            }
        }
        foreach type {ACUV ACRV} {
            foreach constVar $classDef($type) {
                global $state(_$constVar)
                if {[array exists $state(_$constVar)]} {
                    trace variable $state(_$constVar) wu \
                            [list __Oc_ClassConstArray _$this \
                            _$constVar [array get $state(_$constVar)]]
                } else {
                    set msg "No value provided for const array variable\
                            '$constVar' in '$class' Constructor"
                    error $msg "$msg\n    (deleting new '$class'\
                            instance...)[__Oc_ClassCallDelete $class \
                            $this]" [list OC $class]
                }
            }
        }
    }
    }
    lappend classDef(#instances) $this
    set object $this
    return $this
}

;proc __Oc_ClassBoilerplateProcOptions {class {pattern *} 
        {access Configure}} {
    # This will probably require modification to support inheritance
    upvar #0 _$class classDef
    switch -- $access {
        Configure {
            set l [concat $classDef(AMUV) $classDef(SMUV)]
        }
        New {
            set l [concat $classDef(AMUV) $classDef(SMUV) \
                    $classDef(ACUV) $classDef(SCUV)]
        }
        default {
            return -code error "Invalid argument: access:\
                    '$access'\n\tMust be one of:\n\tConfigure New"
        }
    }
    set ret {}
    foreach m $l {
        if {[string match $pattern $m]} {
            lappend ret -$m
        }
    }
    return [lsort $ret]
}

;proc __Oc_ClassBoilerplateProcProcs {class {pattern *} 
        {access callable}} {
    # This will probably require modification to support inheritance
    # How to get boilerplate into these lists?
    upvar #0 _$class classDef
    switch -- $access {
        public {
            set l $classDef(#publicProcs)
        }
        private {
            set l $classDef(#privateProcs)
        }
        all {
            set l [concat $classDef(#publicProcs) \
                    $classDef(#privateProcs)]
        }
        callable {
            if {![catch {info level -1} caller] && \
                    ([string match $class [lindex $caller 0]] || \
                     [lsearch -exact $classDef(#instances) \
                     [lindex $caller 0]] >= 0)} {
                set l [concat $classDef(#publicProcs) \
                        $classDef(#privateProcs)]
            } else {
                set l $classDef(#publicProcs)
            }
        }
        default {
            return -code error "Invalid argument: access:\
                    '$access'\n\tMust be one of:\n\tpublic private all\
                    callable"
        }
    }
    set ret {}
    foreach m $l {
        if {[string match $pattern $m]} {
            lappend ret $m
        }
    }
    return [lsort $ret]
}

;proc __Oc_ClassBoilerplateProcUndefine {class} {
    upvar #0 _$class classDef
    # When inheritance is supported, Undefine all descendents.
    # Delete all instances of this class
    set extra ""
    set delErr 0
    while {[llength [set i [$class Instances]]]} {
        set ret [__Oc_ClassCallDelete $class [lindex $i 0]]
        if {[string length $ret]} {
            append extra $ret
            set delErr 1
        }
    }

    # Try to evaluate the ClassDestructor
    if {[set desErr [catch {
        uplevel 1 [list $classDef(HClassDestructor) $class]} msg]]
    } {
        global errorCode errorInfo
        foreach {dei dec} [list $errorInfo $errorCode] {}
    }

    # Try to complete destruction, error or not.
    catch {rename $class {}}
    # Unset any array commons
    foreach type {ACUC AMUC ACRC AMRC} {
        foreach var $classDef($type) {
            global $classDef(_$var)
            catch {unset $classDef(_$var)}
        }
    }
    # Clear away any proc definitions which exist solely to support 
    # this class
    foreach pfx {H M P} {
        foreach i [array names classDef $pfx*] {
            if {[string match _${class}_* $classDef($i)]} {
                catch {rename $classDef($i) {}}
            }
        }
    }
    set realProc $classDef(HClassDestructor)
    catch {unset classDef}
    global _Oc_Class
    catch {unset _Oc_Class(#$class)}
    # Then, if there were errors, report them.
    if {$desErr} {
        eval [__Oc_ClassFixStackTrace $msg $class $realProc "'$class'\
                ClassDestructor" "\n    (deleting all '$class'\
                instances...)$extra\n    ('$class' undefinition may be\
                incomplete)" $dei $dec]
    }
    if {$delErr} {
        error "Errors deleting '$class' instances\n    (see stack trace\
                for details)" "Errors deleting '$class' instances\n   \
                (deleting all '$class' instances...)$extra\n    (while\
                undefining '$class')" [list OC $class]
    }
}

;proc __Oc_ClassBoilerplateProcWinBasename {class} {
    return w$class
}

;proc __Oc_ClassBoilerplateMethodClass {class this} {
  return $class
}

;proc __Oc_ClassBoilerplateMethodConfigure {class this args} {
    # Improve error messages to report valid options?
    upvar #0 _$this state
    upvar #0 _$class classDef
    set numargs [llength $args]
    if {!$numargs} {
        # No arguments: return list of triples, 
        # one for each public variable.
        # 	1st element:	-$var
        #	2nd element:	<default value>
        #	3rd element:	<current value>
        set retval {}
        foreach type {ACUV AMUV} {
            foreach var $classDef($type) {
                set triple -$var
                if {[catch {lappend triple $classDef(=$var)}]} {
                    lappend triple <undefined>
                }
                if {[catch {
                    global $state(_$var)
                    lappend triple [array get $state(_$var)]
                }]} {
                    lappend triple <undefined>
                }
                lappend retval $triple
            }
        }
        foreach type {SCUV SMUV} {
            foreach var $classDef($type) {
                set triple -$var
                if {[catch {lappend triple $classDef(=$var)}]} {
                    lappend triple <undefined>
                }
                if {[catch {lappend triple $state(_$var)}]} {
                    lappend triple <undefined>
                }
                lappend retval $triple
            }
        }
        return $retval
    }
    if {$numargs == 1} {
        # Given one argument, it must be of the form -$var, where var 
        # holds the name of a public variable.  If not, return an error.  
        # If so, return the triple for that public variable.
        #
        # DOME: report valid options in error message?
        if {![string match -* $args]} {
            return -code error "Invalid option: $args"
        }
        set var [string range $args 1 end]
        foreach type {ACUV AMUV} {
            if {[lsearch -exact $classDef($type) $var] >= 0} {
                set triple -$var
                if {[catch {lappend triple $classDef(=$var)}]} {
                    lappend triple <undefined>
                }
                if {[catch {
                    global $state(_$var)
                    lappend triple [array get $state(_$var)]
                }]} {
                    lappend triple <undefined>
                }
                return $triple
            }
        }
        foreach type {SCUV SMUV} {
            if {[lsearch -exact $classDef($type) $var] >= 0} {
                set triple -$var
                if {[catch {lappend triple $classDef(=$var)}]} {
                    lappend triple <undefined>
                }
                if {[catch {lappend triple $state(_$var)}]} {
                    lappend triple <undefined>
                }
                return $triple
            }
        }
        return -code error "Invalid option: $args"
    }
    # Given more than one argument, it must be a list of option value 
    # pairs.  For each pair, if the variable is public, set that 
    # variable to the given value, and run the configuration script 
    # associated with the variable.  Const variable may only be set by 
    # Configure when the call is made within the Constructor.
    #
    # Use a foreach construction so that the order of setting values is
    # given by the order of the argument list.
    set index 0
    foreach {opt val} $args {
        incr index 2
        # Check option validity: Start with -
        if {![string match -* $opt]} {
            return -code error "Invalid option: $opt"
        }
        set var [string range $opt 1 end]
        # Check that we have a value to assign
        if {$index > $numargs} {
            return -code error "No value provided for option '$opt'"
        }

        set found 0
        foreach type {AMUV ACUV} {
            if {[lsearch -exact $classDef($type) $var] >= 0} {
                set found 1
                # Save old value, if any 
                global $state(_$var)
                if {[array exists $state(_$var)]} {
                    set oldval [array get $state(_$var)]
                } else {
                    catch {unset oldval}
                }
                if {[catch {array set $state(_$var) $val} msg]} {
                    regsub -all $state(_$var) $msg $var msg
                    return -code error "$msg\n    (...setting '$class'\
                            array variable '$var' to:\n\{$val\}\n    )"
                }
                if {[info exists classDef(V$var)]} {
                    # If there's a validation script, run it
                    if {[catch {
                        uplevel 1 [list $classDef(V$var) $class $this]
                    } msg]} {
                        # Error in validation -> save error state
                        global errorInfo errorCode
                        foreach {ei ec} [list $errorInfo $errorCode] {}
                        # restore old value...
                        catch {unset $state(_$var)}
                        catch {array set $state(_$var) $oldval}
                        # ... and report the error
                        eval [__Oc_ClassFixStackTrace $msg $class \
                                $classDef(V$var) "'$class' array\
                                variable '$var' validation script" \
                                "\n    (...validating new value for\
                                option '$opt':\n\{$val\}\n    )" \
                                $ei $ec]
                    }
                }
                break
            }
        }
        if {$found} {
            continue
        }
        foreach type {SMUV SCUV} {
            if {[lsearch -exact $classDef($type) $var] >= 0} {
                set found 1
                # Save old value, if any 
                if {[info exists state(_$var)]} {
                    set oldval $state(_$var)
                } else {
                    catch {unset oldval}
                }
                if {[catch {set state(_$var) $val} msg]} {
                    regsub -all state\\(_$var\\) $msg $var msg
                    return -code error "$msg\n    (...setting '$class'\
                            variable '$var' to '$val')"
                }
                if {[info exists classDef(V$var)]} {
                    # If there's a validation script, run it
                    if {[catch {
                        uplevel 1 [list $classDef(V$var) $class $this]
                    } msg]} {
                        # Error in validation -> save error state...
                        global errorInfo errorCode
                        foreach {ei ec} [list $errorInfo $errorCode] {}
                        # restore old value...
                        if {[catch {set state(_$var) $oldval}]} {
                            catch {unset state(_$var)}
                        }
                        # ... and report the error
                        eval [__Oc_ClassFixStackTrace $msg $class \
                                $classDef(V$var) "'$class' variable\
                                '$var' validation script" \
                                "\n    (...validating new value\
                                '$val' for option '$opt')" $ei $ec]
                    }
                }
                break
            }
        }
        if {$found} {
            continue
        } 
        return -code error "Invalid option: $opt"
    }
    return {}
}
  
;proc __Oc_ClassBoilerplateMethodCget {class this opt} {
    # DOME: Include valid opts information in error message?
    upvar #0 _$this state
    upvar #0 _$class classDef
    if {![string match -* $opt]} {
        return -code error "Invalid option: '$opt'"
    }
    set var [string range $opt 1 end]
    foreach type {ACUV AMUV} {
        if {[lsearch -exact $classDef($type) $var] >= 0} {
            global $state(_$var)
            if {[catch {set retval [array get $state(_$var)]}]} {
                set retval <undefined>
            }
            return $retval
        }
    }
    foreach type {SCUV SMUV} {
        if {[lsearch -exact $classDef($type) $var] >= 0} {
            if {[catch {set retval $state(_$var)}]} {
                set retval <undefined>
            }
            return $retval
        }
    }
    return -code error "Invalid option: '$opt'"
}

;proc __Oc_ClassBoilerplateMethodDelete {class this} {
    upvar #0 _$this state _$class classDef
    # Try to evaluate the Destructor
    if {[set de [catch {
        uplevel 1 [list $classDef(HDestructor) $class $this]
    } msg]]} {
        global errorCode errorInfo
        foreach {dei dec} [list $errorInfo $errorCode] {}
    }
    # Try to complete destruction, error or not.
    Oc_EventHandler Bindtags $this {}
    catch {rename $this {}}
    set i [lsearch -exact $classDef(#instances) $this]
    if {$i >= 0} {
        set classDef(#instances) [lreplace $classDef(#instances) $i $i]
    }
    # Unset any array variables
    foreach type {ACUV AMUV ACRV AMRV} {
        foreach var $classDef($type) {
	    # Guard against errors in case of double Delete
            catch {
		global $state(_$var)
		unset $state(_$var)
	    }
        }
    }
    catch {unset state}
    # Then, if there was an error, report it.
    if {$de} {
        eval [__Oc_ClassFixStackTrace $msg $class \
                $classDef(HDestructor) "'$class' Destructor" \
                "\n    (instance deletion may be incomplete)" $dei $dec]
    }
}

;proc __Oc_ClassBoilerplateMethodGlobalName {class this varname} {
    upvar #0 _$this state
    upvar #0 _$class classDef
    # Search through all the variables for any named $varname
    foreach type {ACUV AMUV ACRV AMRV} {
        if {[lsearch -exact $classDef($type) $varname] >= 0} {
            return $state(_$varname)
        }
    }
    foreach type {SCUV SMUV SCRV SMRV} {
        if {[lsearch -exact $classDef($type) $varname] >= 0} {
            return _${this}(_$varname)
        }
    }
    if {[catch {$class GlobalName $varname} retval]} {
        return -code error "Invalid argument: varname:\
                '$varname'\n\t'$varname' is not a '$class' common or\
                variable"
    } else {
        return $retval
    }
}

;proc __Oc_ClassBoilerplateMethodWinBasename {class this} {
    return w$this
}

########################################################################
# End of boilerplate section
########################################################################
# Boilerplate utilities
########################################################################
;proc __Oc_ClassCallDelete {class this} {
    if {[catch {$this Delete}]} {
        global errorInfo
        return "\n====> Error in '$this Delete' ('$class'\
                instance)\n====> Start stack\
                trace\n$errorInfo\n<==== End stack\
                trace\n<==== Error in '$this Delete' ('$class'\
                instance)"
    }
    return ""
}

########################################################################
# End of boilerplate utilities
########################################################################
# Upper layer procs which interpret an Oc_Class definition and 
# construct the data structures which support the class via
# calls to lower layer procs
########################################################################

;proc __Oc_ClassDefinearray {class rgs} {
    set cmd [lindex $rgs 0]
    if {[catch {
        __Oc_ClassDefineLower$cmd A?? $class [lrange $rgs 1 end]
    } msg]} {
        if {[string match {} \
                [info commands __Oc_ClassDefineLower$cmd]]} {
            error "keyword 'array' cannot be applied to '$cmd'"
        } else {
            error $msg
        }
    }
}

# For now 'callback' is synonym for 'public'
;proc __Oc_ClassDefinecallback {class rgs} {
    set cmd [lindex $rgs 0]
    if {[catch {
        __Oc_ClassDefineLower$cmd ??U $class [lrange $rgs 1 end]
    } msg]} {
        if {[string match {} \
                [info commands __Oc_ClassDefineLower$cmd]]} {
            error "keyword 'callback' cannot be applied to '$cmd'"
        } else {
            error $msg
        }
    }
}

;proc __Oc_ClassDefineconst {class rgs} {
    set cmd [lindex $rgs 0]
    if {[catch {
        __Oc_ClassDefineLower$cmd ?C? $class [lrange $rgs 1 end]
    } msg]} {
        if {[string match {} \
                [info commands __Oc_ClassDefineLower$cmd]]} {
            error "keyword 'const' cannot be applied to '$cmd'"
        } else {
            error $msg
        }
    }
}

;proc __Oc_ClassDefineprivate {class rgs} {
    set cmd [lindex $rgs 0]
    if {[catch {
        __Oc_ClassDefineLower$cmd ??R $class [lrange $rgs 1 end]
    } msg]} {
        if {[string match {} \
                [info commands __Oc_ClassDefineLower$cmd]]} {
            error "keyword 'private' cannot be applied to '$cmd'"
        } else {
            error $msg
        }
    }
}

;proc __Oc_ClassDefinepublic {class rgs} {
    set cmd [lindex $rgs 0]
    if {[catch {
        __Oc_ClassDefineLower$cmd ??U $class [lrange $rgs 1 end]
    } msg]} {
        if {[string match {} \
                [info commands __Oc_ClassDefineLower$cmd]]} {
            error "keyword 'public' cannot be applied to '$cmd'"
        } else {
            error $msg
        }
    }
}

# Variables are non-const, private, scalar by default
interp alias {} __Oc_ClassDefinevariable \
        {} __Oc_ClassDefineLowervariable SMR

# Commons are non-const, private, scalar by default
interp alias {} __Oc_ClassDefinecommon \
        {} __Oc_ClassDefineLowercommon SMR

# Methods are public by default
interp alias {} __Oc_ClassDefinemethod \
        {} __Oc_ClassDefineLowermethod ??U

# Procs are public by default
interp alias {} __Oc_ClassDefineproc {} __Oc_ClassDefineLowerproc ??U

;proc __Oc_ClassDefineConstructor {class rgs} {
    if {[llength $rgs] == 2} {
        foreach {arglist body} $rgs {}
    } else {
        error "Constructor usage:\n\tConstructor <argument list> <body>"
    }
    # Be sure Constructor returns a value
    append body {;return {}}
    upvar #0 _$class classDef
    __Oc_ClassInstanceProcGeneratingProc $classDef(HConstructor) \
            $arglist $body ""
}

;proc __Oc_ClassDefineDestructor {class rgs} {
    if {[llength $rgs] != 1} {
        error "Destructor usage:\n\tDestructor <body>"
    }
    upvar #0 _$class classDef
    __Oc_ClassInstanceProcGeneratingProc $classDef(HDestructor) {} \
            [lindex $rgs 0] ""
}

;proc __Oc_ClassDefineClassConstructor {class rgs} {
    if {[llength $rgs] != 1} {
        error "ClassConstructor usage:\n\tClassConstructor <body>"
    }
    upvar #0 _$class classDef
    __Oc_ClassClassProcGeneratingProc $classDef(HClassConstructor) {} \
            [lindex $rgs 0] ""
}

;proc __Oc_ClassDefineClassDestructor {class rgs} {
    if {[llength $rgs] != 1} {
        error "ClassDestructor usage:\n\tClassDestructor <body>"
    }
    upvar #0 _$class classDef
    __Oc_ClassClassProcGeneratingProc $classDef(HClassDestructor) {} \
            [lindex $rgs 0] ""
}

;proc __Oc_ClassDefinerename {class rgs} {
    if {[llength $rgs] != 3} {
        error "rename usage:\n\trename method <oldName> <newName>\
                \n\trename proc <oldName> <newName>"
    }
    upvar #0 _$class classDef
    foreach {type old new} $rgs {}
    switch $type {
        method {
            set t M
        }
        proc {
            set t P
        }
        default {
            error "rename usage:\n\trename method <oldName> <newName>\
                    \n\trename proc <oldName> <newName>"
        }
    }
    if {![info exists classDef($t$old)]} {
        error "keyword 'rename' must rename an existing proc or method"
    }
    set classDef($t$new) $classDef($t$old)
    unset classDef($t$old)
}

########################################################################
# End of class definition interpretation upper layer section
########################################################################
# Lower layer procs which construct the data structures which 
# support a class definition
########################################################################

;proc __Oc_ClassDefineLowerarray {qual class rgs} {
    set cmd [lindex $rgs 0]
    set newqual A[string range $qual 1 2]
    if {[catch {
        __Oc_ClassDefineLower$cmd $newqual $class [lrange $rgs 1 end]
    } msg]} {
        if {[string match {} \
                [info commands __Oc_ClassDefineLower$cmd]]} {
            error "keyword 'array' cannot be applied to '$cmd'"
        } else {
            error $msg
        }
    }
}

;proc __Oc_ClassDefineLowercallback {qual class rgs} {
    set cmd [lindex $rgs 0]
    set newqual [string range $qual 0 1]U
    if {[catch {
        __Oc_ClassDefineLower$cmd $newqual $class [lrange $rgs 1 end]
    } msg]} {
        if {[string match {} \
                [info commands __Oc_ClassDefineLower$cmd]]} {
            error "keyword 'callback' cannot be applied to '$cmd'"
        } else {
            error $msg
        }
    }
}

;proc __Oc_ClassDefineLowerconst {qual class rgs} {
    set cmd [lindex $rgs 0]
    set newqual [string index $qual 0]C[string index $qual 2]
    if {[catch {
        __Oc_ClassDefineLower$cmd $newqual $class [lrange $rgs 1 end]
    } msg]} {
        if {[string match {} \
                [info commands __Oc_ClassDefineLower$cmd]]} {
            error "keyword 'const' cannot be applied to '$cmd'"
        } else {
            error $msg
        }
    }
}

;proc __Oc_ClassDefineLowerprivate {qual class rgs} {
    set cmd [lindex $rgs 0]
    set newqual [string range $qual 0 1]R
    if {[catch {
        __Oc_ClassDefineLower$cmd $newqual $class [lrange $rgs 1 end]
    } msg]} {
        if {[string match {} \
                [info commands __Oc_ClassDefineLower$cmd]]} {
            error "keyword 'private' cannot be applied to '$cmd'"
        } else {
            error $msg
        }
    }
}

;proc __Oc_ClassDefineLowerpublic {qual class rgs} {
    set cmd [lindex $rgs 0]
    set newqual [string range $qual 0 1]U
    if {[catch {
        __Oc_ClassDefineLower$cmd $newqual $class [lrange $rgs 1 end]
    } msg]} {
        if {[string match {} \
                [info commands __Oc_ClassDefineLower$cmd]]} {
            error "keyword 'public' cannot be applied to '$cmd'"
        } else {
            error $msg
        }
    }
}

;proc __Oc_ClassDefineLowervariable {qual class rgs} {
    set numargs [llength $rgs]
    if {$numargs == 1} {
        foreach name $rgs {}
    } elseif {$numargs == 2} {
        foreach {name script} $rgs {}
    } elseif {$numargs == 3} {
        foreach {name equalsign default} $rgs {}
    } elseif {$numargs == 4} {
        foreach {name equalsign default script} $rgs {}
    } else {
        error "variable usage:\n\t?<modifiers>? variable <name> ?=\
                <default_value>? ?<validation_script>?"
    }
    if {[info exists equalsign] && [string compare = $equalsign]} {
        error "variable usage:\n\t?<modifiers>? variable <name> ?=\
                <default_value>? ?<validation_script>?"
    }
    if {[string match {\?} [string index $qual 0]]} {
        set qual S[string range $qual 1 2]
    }
    if {[string match {\?} [string index $qual 1]]} {
        set qual [string index $qual 0]M[string index $qual 2]
    }
    if {[string match {\?} [string index $qual 2]]} {
        set qual [string range $qual 0 1]R
    }
    upvar #0 _$class classDef
    if {[info exists script] \
            && ![string match U [string index $qual 2]]} {
        error "Only public variables take validation scripts"
    }
    set classDef(_$name) ${qual}V
    if {[info exists default]} {
	# Consider changing this:
        set classDef(=$name) $default
        # to this:
        # set classDef(=$name) [subst $default]
    } else {
        catch {unset classDef(=$name)}
    }
    if {[info exists script]} {
        set classDef(V$name) $script
    } else {
        catch {unset classDef(V$name)}
    }
}

;proc __Oc_ClassDefineLowercommon {qual class rgs} {
    set numargs [llength $rgs]
    if {$numargs == 1} {
        foreach name $rgs {}
    } elseif {$numargs == 2} {
        foreach {name default} $rgs {}
    } else {
        error "common usage:\n\t?<modifiers>? common <name>\
                ?<default_value>?"
    }
    if {[string match {\?} [string index $qual 0]]} {
        set qual S[string range $qual 1 2]
    }
    if {[string match {\?} [string index $qual 1]]} {
        set qual [string index $qual 0]M[string index $qual 2]
    }
    if {[string match {\?} [string index $qual 2]]} {
        set qual [string range $qual 0 1]R
    }
    upvar #0 _$class classDef
    set classDef(_$name) ${qual}C
    if {[info exists default]} {
        set classDef(=$name) $default
    } else {
        catch {unset classDef(=$name)}
    }
    catch {unset classDef(V$name)}
}

# A preamble to pre-pend to each private method.  It tests the
# caller to verify that it has access to call the private method.
# Strange symbols are used as variable names to reduce the likelihood
# that variable used in this preamble code will conflict with the names
# of the class commons.
#
# DOME: This verifies only the same class -- it will need changes to 
# support inheritance
array set _Oc_Class {
privatePreamble {
    if {[catch {info level -1} +] || \
            (![regexp "_$class\[0-9]+" [lindex ${+} 1]] && \
             ![string match $class [lindex ${+} 1]])} {
        return -code error "Call to private method or proc from outside\
                class '$class'"
    }
    catch {unset +}
}
}

;proc __Oc_ClassDefineLowermethod {qual class rgs} {
    set numargs [llength $rgs]
    if {$numargs == 3} {
        foreach {name arglist body} $rgs {}
    } elseif {$numargs == 4} {
        foreach {name arglist varlist body} $rgs {}
    } else {
        error "method usage:\n\t?<access>? method <name> <argument\
                list> ?<variable list>? <body>"
    }
    if {[string match A [string index $qual 0]]} {
        error "keyword 'array' cannot be applied to 'method'"
    }
    if {[string match C [string index $qual 1]]} {
        error "keyword 'const' cannot be applied to 'method'"
    }
    # methods are public by default
    if {[string match {\?} [string index $qual 2]]} {
        set qual [string range $qual 0 1]U
    }
    upvar #0 _$class classDef
    set classDef(M$name) __${class}_M$name
    if {[string match R [string index $qual 2]]} {
	set ep 0
	Oc_Option Get Oc_Class enforcePrivate ep
	if {[catch {expr {$ep && $ep}}]} {
	    set msg "Bad value for Oc_Class option 'enforcePrivate':"
	    append msg "\n\t$ep\nMust be a boolean.\nSetting Oc_Class"
	    set ep 0
	    append msg " option 'enforcePrivate' to:\n\t$ep"
	    Oc_Log Log $msg warning Oc_Class
	    Oc_Option Add * Oc_Class enforcePrivate $ep
	}
	if {$ep} {
            global _Oc_Class
            set access $_Oc_Class(privatePreamble)
	} else {
            set access ""
	}
    } else {
        set access ""
    }

    if {![info exists varlist]} {
        __Oc_ClassInstanceProcGeneratingProc $classDef(M$name) \
                $arglist $body $access
    } else {
        set aclist {}
        set sclist {}
        set avlist {}
        set svlist {}
        foreach var $varlist {
            if {![info exists classDef(_$var)]} {
                error "'$var' in variable list is not a common name"
            }
            switch -glob -- $classDef(_$var) {
                A??V {
                    lappend avlist $var
                }
                S??V {
                    lappend svlist $var
                }
                A??C {
                    lappend aclist $var
                }
                S??C {
                    lappend sclist _${class}(_$var) $var
                }
            }
        }
        set preamble [format {
    set upvarlist {%s}
        } $sclist]
        if {[llength $aclist]} {
            append preamble [format {
    upvar #0 _$class classDef
    foreach varName {%s} {
        lappend upvarlist $classDef(_$varName) $varName
    }
            } $aclist]
        }
        if {[llength $svlist]} {
            append preamble [format {
    foreach varName {%s} {
        append upvarlist " \"_\${this}(_$varName)\" \"$varName\""
    }
            } $svlist]
        }
        if {[llength $avlist]} {
            append preamble [format {
    append body "\nglobal _\$this"
    foreach varName {%s} {
        append upvarlist " \[set \"_\${this}(_$varName)\"\]\
                \"$varName\""
    }
            } $avlist]
        }
        __Oc_ClassInstanceProcGeneratingProc $classDef(M$name) \
                $arglist $body $access $preamble
    }
}

########################################################################
# EXAMPLE
# Oc_Class Foo {
#     private method Bar {grok barsoom} {
#         cmd1 $grok $c $v
#         cmd2 $ac($barsoom) $av($barsoom)
#     } 
#     common c
#     array common ac
#     variable v
#     array variable av
# }
#
# The actual proc supporting the private method Bar of class Foo will 
# be one created by the following literal proc definition (modulo 
# escaped \n):
# proc __Foo_MBar {class this grok barsoom} {
# global _$this
# upvar #0 __Foo_Aac ac _Foo(_c) c "_${this}(_v)" "v" \
#         [set "_${this}(_av)"] "av"
#
#     if {[catch {info level -1} +] || \
#             (![regexp "_$class\[0-9]+" [lindex $+ 1]] && \
#              ![string match $class [lindex $+ 1]])} {
#         error "Call to private method or proc from outside class"
#     }
#     catch {unset +}
#
#         cmd1 $grok $c $v
#         cmd2 $ac($barsoom) $av($barsoom)
# }
#
# However, at the time __Oc_ClassDefineLowermethod is evaluated, all the
# commons and variables of the Oc_Class Foo are not necessarily known,
# so this literal proc definition cannot be constructed.  Instead, we 
# construct a proc which will construct the proc above and then evaluate
# it, the first time the class method is evaluated.  Future evaluations
# will use the redefinition of the proc directly, so the penalty of
# run-time construction is paid only once, and after that we have
# maximum possible performance given the requirements of Tcl-only
# coding, availability of all commons in the constructed proc
# definition, and the indirect access to array variables.  Further
# improvements might attack those assumptions.
#
# Here's what the example proc-constructing proc should look like,
# literally:
# proc __Foo_MBar {class this args} {
#     upvar #0 _$class classDef
#     set upvarlist {}
#     set needGlobal 0
#     foreach arrayType {ACUC AMUC ACRC AMRC} {
#         foreach varName $classDef($arrayType) {
#             lappend upvarlist $classDef(_$varName) $varName
#         }
#     }
#     foreach scalarType {SCUC SMUC SCRC SMRC} {
#         foreach varName $classDef($scalarType) {
#             lappend upvarlist _${class}(_$varName) $varName
#         }
#     }
#     foreach scalarType {SCUV SMUV SCRV SMRV} {
#         foreach varName $classDef($scalarType) {
#             append upvarlist " \"_\${this}(_$varName)\" \"$varName\""
#         }
#     }
#     foreach arrayType {ACUV AMUV ACRV AMRV} {
#         foreach varName $classDef($arrayType) {
#             append upvarlist " \[set \"_\${this}(_$varName)\"\]\
#                     \"$varName\""
#             set needGlobal 1
#         }
#     }
#     if {$needGlobal} {
#         append body "\nglobal _\$this"
#     }
#     if {[string length $upvarlist]} {
#         append body "\nupvar #0 $upvarlist\n"
#     }
#
#     append body {
#     if {[catch {info level -1} +] || \
#             (![regexp "_$class\[0-9]+" [lindex $+ 1]] && \
#              ![string match $class [lindex $+ 1]])} {
#         return -code error "Call to private method or proc from\
#                 outside class '$class'"
#     }
#     catch {unset +}
#     }
#     append body {
#         cmd1 $grok $c $v
#         cmd2 $ac($barsoom) $av($barsoom)
#     }
#     proc __Foo_MBar {class this grok barsoom} $body
# set code [catch {uplevel 1 [linsert $args 0 __Foo_MBar $class $this]} res]
# if {$code != 1} {
#     return -code $code $res
# }
# eval [__Oc_ClassFixStackTrace $res $class __Foo_MBar \"__FooMBar\"]
# }
#
# So constructing this proc-constructing proc is the task of
# __Oc_ClassDefineLowermethod.  It also constructs the variations
# to implement public methods and the restriction to a list of 
# specified commons.
########################################################################

;proc __Oc_ClassDefineLowerproc {qual class rgs} {
    set numargs [llength $rgs]
    if {$numargs == 3} {
        foreach {name arglist body} $rgs {}
    } elseif {$numargs == 4} {
        foreach {name arglist varlist body} $rgs {}
    } else {
        error "proc usage:\n\t?<access>? proc <name> <argument list>\
                ?<variable list>? <body>"
    }
    if {[string match A [string index $qual 0]]} {
        error "keyword 'array' cannot be applied to 'proc'"
    }
    if {[string match C [string index $qual 1]]} {
        error "keyword 'const' cannot be applied to 'proc'"
    }
    # procs are public by default
    if {[string match {\?} [string index $qual 2]]} {
        set qual [string range $qual 0 1]U
    }

    upvar #0 _$class classDef
    set classDef(P$name) __${class}_P$name
    if {[string match R [string index $qual 2]]} {
	set ep 0
	Oc_Option Get Oc_Class enforcePrivate ep
	if {[catch {expr {$ep && $ep}}]} {
	    set msg "Bad value for Oc_Class option 'enforcePrivate':"
	    append msg "\n\t$ep\nMust be a boolean.\nSetting Oc_Class"
	    set ep 0
	    append msg " option 'enforcePrivate' to:\n\t$ep"
	    Oc_Log Log $msg warning Oc_Class
	    Oc_Option Add * Oc_Class enforcePrivate $ep
	}
	if {$ep} {
            global _Oc_Class
            set access $_Oc_Class(privatePreamble)
	} else {
            set access ""
	}
    } else {
        set access ""
    }

    if {![info exists varlist]} {
        __Oc_ClassClassProcGeneratingProc $classDef(P$name) $arglist \
                $body $access
    } else {
        set arraylist {}
        set upvarlist {}
        foreach var $varlist {
            if {![info exists classDef(_$var)]} {
                error "'$var' in variable list is not a common"
            }
            switch -glob -- $classDef(_$var) {
                A??C {
                    lappend arraylist $var
                }
                S??C {
                    lappend upvarlist _${class}(_$var) $var
                }
                default {
                    error "'$var' in variable list is not a common"
                }
            }
        }
        set preamble [format {
    set upvarlist {%s}
        } $upvarlist]
        if {[llength $arraylist]} {
            append preamble [format {
    upvar #0 _$class classDef
    foreach varName {%s} {
        lappend upvarlist $classDef(_$varName) $varName
    }
            } $arraylist]
        }
        __Oc_ClassClassProcGeneratingProc $classDef(P$name) $arglist \
                $body $access $preamble
    }
}

########################################################################
# EXAMPLE
# Oc_Class Foo {
#     private proc Bar {grok barsoom} {
#         cmd1 $grok $c
#         cmd2 $ac($barsoom)
#     } 
#     common c
#     array common ac
# }
#
# The actual proc supporting the private proc Bar of class Foo will be 
# one created by the following literal proc definition (modulo escaped 
# "\n"):
# proc __Foo_PBar {class grok barsoom} {
# upvar #0 __Foo_Aac ac _Foo(_c) c
#
#     if {[catch {info level -1} +] || \
#             (![regexp "_$class\[0-9]+" [lindex $+ 1]] && \
#              ![string match $class [lindex $+ 1]])} {
#         error "Call to private method or proc from outside class"
#     }
#     catch {unset +}
#
#         cmd1 $grok $c
#         cmd2 $ac($barsoom)
# }
#
# However, at the time __Oc_ClassDefineLowerproc is evaluated, all the
# commons of the Oc_Class Foo are not necessarily known, so this literal
# proc definition cannot be constructed.  Instead, we construct a proc
# which will construct the proc above, then evaluate it the first time
# the class proc is evaluated.  Future evaluations will use the 
# redefinition of the proc directly, so the penalty of run-time 
# construction is paid only once, and after that we have maximum 
# possible performance given the requirements of Tcl-only coding and 
# availability of all commons in the constructed proc definition.
#
# Here's what the example proc-constructing proc should look like,
# literally:
# proc __Foo_PBar {class args} {
#     upvar #0 _$class classDef
#     set upvarlist {}
#     foreach arrayType {ACUC AMUC ACRC AMRC} {
#         foreach varName $classDef($arrayType) {
#             lappend upvarlist $classDef(_$varName) $varName
#         }
#     }
#     foreach scalarType {SCUC SMUC SCRC SMRC} {
#         foreach varName $classDef($scalarType) {
#             lappend upvarlist _${class}(_$varName) $varName
#         }
#     }
#     if {[llength $upvarlist]} {
#         append body "\nupvar #0 $upvarlist\n"
#     }
#
#     append body {
#     if {[catch {info level -1} +] || \
#             (![regexp "_$class\[0-9]+" [lindex $+ 1]] && \
#              ![string match $class [lindex $+ 1]])} {
#         return -code error "Call to private method or proc from\
#                 outside class '$class'"
#     }
#     catch {unset +}
#     }
#     append body {
#         cmd1 $grok $c
#         cmd2 $ac($barsoom)
#     }
#     proc __Foo_PBar {class grok barsoom} $body
# set code [catch {uplevel 1 [linsert $args 0 __Foo_PBar $class]} res]
# if {$code != 1} {
#     return -code $code $res
# }
# eval [__Oc_ClassFixStackTrace $res $class __Foo_PBar \"__FooPBar\"]
# }
#
# So constructing this proc-constructing proc is the task of
# __Oc_ClassDefineLowerproc.  It also constructs the variations
# to implement public procs and the restriction to a list of 
# specified commons.
########################################################################
;proc __Oc_ClassClassProcGeneratingProc {name arglist body access 
        {preamble {}}} {
    if {![string length $preamble]} {
        set realbody {
    upvar #0 _$class classDef
    set upvarlist {}
    foreach arrayType {ACUC AMUC ACRC AMRC} {
        foreach varName $classDef($arrayType) {
            lappend upvarlist $classDef(_$varName) $varName
        }
    }
    foreach scalarType {SCUC SMUC SCRC SMRC} {
        foreach varName $classDef($scalarType) {
            lappend upvarlist _${class}(_$varName) $varName
        }
    }
        }
    } else {
        set realbody $preamble
    }
    append realbody [format {
    if {[llength $upvarlist]} {
        append body "\nupvar #0 $upvarlist\n"
    }
    append body {%s}
    append body {%s}
    } $access $body]
    append realbody "proc $name {[concat class $arglist]} \$body\n"
    append realbody "set code \[catch {uplevel 1 \[linsert \$args 0\
	    $name \$class\]} res\]\n"
    append realbody "if {\$code != 1} {\n    return -code \$code\
            \$res\n}\n"
    append realbody "eval \[__Oc_ClassFixStackTrace \$res \$class $name\
            \\\"$name\\\"\]"
    proc $name {class args} $realbody
}

;proc __Oc_ClassInstanceProcGeneratingProc {name arglist body access 
        {preamble {}}} {
    if {![string length $preamble]} {
        set realbody {
    upvar #0 _$class classDef
    set upvarlist {}
    set needGlobal 0
    foreach arrayType {ACUC AMUC ACRC AMRC} {
        foreach varName $classDef($arrayType) {
            lappend upvarlist $classDef(_$varName) $varName
        }
    }
    foreach scalarType {SCUC SMUC SCRC SMRC} {
        foreach varName $classDef($scalarType) {
            lappend upvarlist _${class}(_$varName) $varName
        }
    }
    foreach scalarType {SCUV SMUV SCRV SMRV} {
        foreach varName $classDef($scalarType) {
            append upvarlist " \"_\${this}(_$varName)\" \"$varName\""
        }
    }
    foreach arrayType {ACUV AMUV ACRV AMRV} {
        foreach varName $classDef($arrayType) {
            append upvarlist " \[set \"_\${this}(_$varName)\"\]\
                    \"$varName\""
            set needGlobal 1
        }
    }
    if {$needGlobal} {
        append body "\nglobal _\$this"
    }
        }	;# end [set realbody]
    } else {
        set realbody $preamble
    }
    append realbody [format {
    if {[string length $upvarlist]} {
        append body "\nupvar #0 $upvarlist\n"
    }
    append body {%s}
    append body {%s}
    } $access $body]
    append realbody "proc $name {[concat class this $arglist]} \$body\n"
    append realbody "set code \[catch {uplevel 1 \[linsert \$args 0\
	    $name \$class \$this\]} res\]\n"
    append realbody "if {\$code != 1} {\n    return -code \$code\
            \$res\n}\n"
    append realbody "eval \[__Oc_ClassFixStackTrace \$res \$class $name\
            \\\"$name\\\"\]"
    proc $name {class this args} $realbody
}

proc Oc_IsClass {name} {
    global _Oc_Class
    return [info exists _Oc_Class(#$name)]
}
