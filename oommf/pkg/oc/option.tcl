# FILE: option.tcl
#
#	An option database for configuring Oc_Classes.
#
# Last modified on: $Date: 2015/11/04 06:01:37 $
# Last modified by: $Author: donahue $
#
# This file defines the Tcl command Oc_Option as the interface to an option
# database for Oc_Classes.  It plays a similar role for Oc_Classes as the
# Tk command [option] plays for Tk windows.

Oc_Class Oc_Option {

    array common db
    common classConstructorDone 0

    ClassConstructor {
        # Source the option configuration file(s)
        set configDir [file join [file dirname [file dirname [file dirname \
                [Oc_DirectPathname [info script]]]]] config]
        set fn [file join $configDir options.tcl]
        if {[file readable $fn]} {
           if {[catch {source $fn} msg]} {
              set msg [join [split $msg \n] \n\t]
              set classConstructorDone 1 ;# We're as done as we're
              ## going to get. If we leave classConstructorDone set to
              ## zero, then Oc_Log is likely to raise an avalanche of
              ## additional errors. Raising one error should be enough!
              Oc_Log Log "Error sourcing options file:\n   \
                 $fn:\n\t$msg\nOption setting may be incomplete." \
                 warning Oc_Option
            }
        }
	set classConstructorDone 1
    }

    proc ClassConstructorDone {} {
       # Useful for handling errors occuring during initialization
       return $classConstructorDone
    }

    proc Add {app className option value} {

        if {![string match -nocase $app [Oc_Main GetAppName]]} {
            return
        }

        if {[regexp , $className]} {
            return -code error "Class pattern may not contain ','"
        }

        lappend db($className,) $option
        set len [llength $db($className,)]
        if {[lsearch -exact $db($className,) $option] + 1 < $len} {
            set db($className,) [lrange $db($className,) 0 [expr {$len-2}]]
        } else {
            set db($className,) [lsort $db($className,)]
        }

        set db($className,$option) $value
        if {![string length $className]} {
            return
        }
        if {[llength [set cmds [info commands $className]]] > 1} {
            return -code error "Ambiguous class pattern '$className': $cmds"
        }
        if {[llength $cmds]} {
            set cmd [lindex $cmds 0]
            if {[Oc_IsClass $cmd] && ![catch {$cmd Configure -$option}]} {
                $cmd Configure -$option $value
            }
            return
        }

        # Might want to set an eventHandler here to be triggered by the
        # creation of the Oc_Class $cmd which would call its Configure
        # proc.  Currently no such event is generated.  Also, one should
        # be careful to register only one handler per option.  Even better
        # would be to merge all handlers into one.
        #
        # For now, queries of Oc_Option are hard coded into the
        # implementation of the Oc_Class command.

    }

    proc Get {className args} {
        # This proc can be called with 1, 2, or 3 args, i.e.,
        #    Oc_Option Get class ?option? ?varName?
        # If called with one arg, that arg is used as a glob-pattern and
        # the return is a list of all classes matching that pattern.  If
        # called with two args, the second arg is used as a glob-pattern
        # and the return is a list of all options matching that pattern
        # for the specified class. If called with three args then the
        # return value is 0 if the class+option pair is defined and the
        # variable specified by the third arg is filled with the
        # class+option value.  If the class+option pair is not defined
        # then the return value is 1.
	if {!$classConstructorDone} {
	    global errorInfo errorCode
	    set errorCode NONE
	    set errorInfo "No stack available"
            return -code error "'$class Get $className $args' called\nbefore\
		$class ClassConstructor completed.\nCaller:\
		[lindex [info level -1] 0]"
	}
        set retList {}
        if {[llength $args] == 0} {
            foreach c [array names db $className,] {
                regsub ,$ $c {} c
                lappend retList $c
            }
            return $retList
        }
        set option [lindex $args 0]
        if {[llength $args] == 1} {
            if {![info exists db($className,)]} {
                return $retList
            }
            foreach o $db($className,) {
                if {[string match $option $o]} {
                    lappend retList $o
                }
            }
            return $retList
        }
        set varName [lindex $args 1]
        if {[llength $args] == 2} {
            upvar $varName v
            return [catch {set v $db($className,$option)}]
        }
        return -code error "Usage: $class Get class ?option? ?varName?"
    }

    proc GetValue { className option } {
       # Reduced version of proc Get, that simply returns the
       # value of the option if it exists, or otherwise returns
       # an error.
       if {[info exists db(${className},${option})]} {
          return $db(${className},${option})
       }
       return -code error \
          "Pair ${className},${option} not in Oc_Option database."
    }

    Constructor {} {
        return -code error "Instances of $class are not allowed"
    }

}

