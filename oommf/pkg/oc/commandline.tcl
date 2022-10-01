# FILE: commandline.tcl
#
# Last modified on: $Date: 2012/06/28 21:04:09 $
# Last modified by: $Author: donahue $
#
# [Oc_CommandLine] manages the parsing of the command line arguments to
# an application.
#
# Normally, the command line arguments are stored in the global list
# variable $argv.  The parsing of the command line arguments is done by
# evaluating [Oc_CommandLine Parse $argv] at global scope.
#
# The global list variable $argv holds a list of strings.  
# [Oc_CommandLine Parse $argv] searches through $argv, one string at a
# time, looking for an "option".  [Oc_CommandLine Parse] can distinguish
# an "option" from other strings because all options begin with a common
# prefix string, called the "switch".  By default, the switch is "-".
#
# The command [Oc_CommandLine Switch] returns the current switch used by
# [Oc_CommandLine] to tell an option from a non-option, and the command
# [Oc_CommandLine Switch $string] instructs [Oc_CommandLine] to use
# $string as the switch.
#
# Prior to evaluating [Oc_CommandLine Parse], an application will use
# the command [Oc_CommandLine Option] to define each command line option
# it wishes the parser to recognize, and how it wants the parser to
# process them.  Usage is:
#
# 	Oc_CommandLine Option <name> <argList> <script> <docString>
#
# <name> is the name of the option to recognize when prefixed by the
# switch string.  For example, when the switch string is "-" (the
# default) and <name> is "foo", then this evaluation of
# [Oc_CommandLine Option] describes how to process the option "-foo" in
# $argv.  
#
# If <name> is the same string value as [Oc_CommandLine Switch], the
# option is handled specially.  See below.  For now, assume another
# value for <name>.
#
# Each option takes some number of arguments as specified by <argList>.
# <argList> is a list of triples.  Each triple describes one argument
# the option expects.  The structure of the triple is:
#
#	{ <argName> <validationScript> <argDocString> }
#
# <argName> is the name by which the argument is known within the
# <validationScript>, the <script>, and the usage message returned
# by [Oc_CommandLine Usage].  If this is the N-th triple in <argList>
# for command line option "-foo", then the variable named <argName>
# will contain the value of the N-th string following the string
# "-foo" in the global list $argv when <validationScript> and <script>
# are evaluated by the parser.
#
# The <validationScript> is evaluated to check whether the value
# taken from $argv and assigned to the variable <argName> is
# acceptable.  <validationScript> should return 1 if the value is
# acceptable, 0 otherwise.  <validationScript> may access variables
# holding *all* the arguments to this option; it is not limited to 
# the value of the argument it is validating.  That way, multiple
# arguments to one option may be validated against one another.
# The <validationScript> must return 1 or 0, and must not raise an
# error.
#
# <argDocString> is used in the construction of the usage message.
#
# Back to the arguments of [Oc_CommandLine Option], <script> is
# to be evaluated by the parser when it finds this option in $argv.
# When <script> is evaluated, all the variables with names given by
# the <argName> elements of the triples in <argList> will contain
# values taken from the corresponding elements of $argv.
#
# By default, if multiple instances of the same option appear in
# $argv, <script> will still be evaluated only once, using the
# arguments which appear latest in $argv.  Although the arguments to
# earlier instances of the option will not be passed to <script>, they
# will still be validated.  The <script> may raise an error, which
# will be passed along as an error raised by [Oc_CommandLine Parse].
# This behavior can be changed to stack multiple instances of an
# option by specifying the "multi" opttype at the tail of the option
# declaration (see below).
# 
# <docString> is used in the construction of the usage message returned
# by [Oc_CommandLine Usage] which summarizes the options which 
# [Oc_CommandLine] has been told to recognize.  <docString> is optional.
# If it is omitted, the value of an empty string is assumed.
# 
# All of that description is probably not as useful as an example:
#
# 	Oc_CommandLine Option range {
#		{lo {expr {![catch {expr {$lo > 0}} res] && $res}} {> 0}}
#		{hi {expr {![catch {expr {$hi > $lo}} res] && $res}} {> lo}}
#	    } {
#		global rangeLow rangeHigh
#		set rangeLow $lo; set rangeHigh $hi
#	} "A range of positive numbers"
#
# When $argv contains: ... -range 1 5 ... then [Oc_CommandLine Parse $argv]
# will cause the global variables rangeLow and rangeHigh to be set to 1 and 5
# respectively.
#
# When $argv contains: ... -range -1 5 ... then [Oc_CommandLine Parse $argv]
# will cause an error message to be displayed before exiting the app:
# 
# <12345> appName appVersion Oc_CommandLine panic:
# Invalid argument lo=-1 in
#         -range lo hi
# Usage: appName [options]
# Available options:
#   -console                  Display console window (requires Tk)
#   -cwd         directory    Change working directory
#   -help                     Display this help message and exit
#   -tk          flag         Enable or disable Tk (Some apps require Tk)
#                             where flag = 0 or 1
#   -range       lo hi        A range of positive numbers
#                             where lo > 0
#                             where hi > lo
#   -version                  Display version information and exit
#   --                        End of options
#
# because the value for the argument "lo" to option "-range" fails its
# validation test.  This example shows where the various docStrings
# appear in the Usage message.  This example also shows the set of
# default options which [Oc_CommandLine] supplies.  Note especially
# the final option "--".  This is the special case where <name> has
# the same value as [Oc_CommandLine Switch] noted above.  That option
# always mean "End of options".  It can also handle further processing
# of the command line.  More on that below.
#
# When [Oc_CommandLine Parse] finds an option in $argv which takes N
# arguments, it consumes the next N elements of $argv.  If there are
# not N elements left in $argv, a fatal error will occur.  After
# handling of that option, the search for the next option will begin
# at the N+1st position following this option.  If the N+1st element
# does not begin with the switch, it is not an option.  That element
# of $argv is set aside for later processing, and the search continues
# with N+2, etc.  If an element of $argv is found during this search
# which does begin with the switch, but does not match any of the
# options for which [Oc_CommandLine Option] has established handling,
# a fatal error "Unrecognized option" will be reported.
#
# If the "end of option" option is encountered, [Oc_CommandLine Parse]
# will combine all the elements of $argv which were skipped in the
# search for options with the remainder of $argv which it has not yet
# searched.  If no "end of option" option is in $argv, the list of
# elements skipped will be assembled when the end of $argv is reached.
#
# The processing of this list of "leftover" command line arguments
# is performed according to the instructions provided with the
# command:
# Oc_CommandLine Option [Oc_CommandLine Switch] <argList> <script> <docString>
# This is the special case we skipped earlier.
#
# The list of all the leftover command line arguments are to be
# processed by this special "end of options" option.  The <script> and
# <docString> arguments play the same role as before.  The <argList>
# argument is still a list of triples, but the triples now take the
# form:
#	{ {<argName> <argType>} <validationScript> <argDocString> }
#
# <argType> is new here.  It allows greater flexibility in specifying the
# structure of the remaining command line arguments.  <argType> may take
# the values "required", "optional", or "list" or it may be omitted.  If
# it is omitted, the value of "required" is assumed.
#
# For regular options, all arguments are "required".  If an option takes
# 3 arguments, the next 3 elements in $argv are given to that option.
# The "end of options" option is permitted to take a variable number of
# arguments according to the <argType>s of its arguments.  It must receive
# values for all of its "required" arguments from $argv.  If there are not
# enough elements left in $argv to give values to all required arguments,
# a fatal error is reported.  After that any remaining elements in $argv
# are assigned as values to the "optional" arguments.  If there are no
# optional arguments, a fatal error "Too many arguments" is reported.
# Finally, the last argument may have the <argType> "list".  If so, then
# any remaining elements in $argv are gathered into a list and assigned
# as the value of that argument.
#
# As the paragraph above suggests, the arguments to the "end of options"
# option must be specified in order with all "required" arguments first,
# followed by all "optional" arguments, and ending with at most one
# "list" argument.
#
# Validation of arguments and processing of the "end of options" option
# is essentially the same as for other options.  The construction of the
# usage message differs a bit, but in a customary way.

Oc_Class Oc_CommandLine {

    # Might want to make this a public common when Oc_Classes have a
    # boilerplate Configure proc.  For now, use the Switch proc.
    common switch -

    array common body
    array common valid
    array common arg
    array common doc
    array common opttype  ;# Either "single" or "multi" for each option.
                          ## Ignored for the "switch" option.

    ClassConstructor {
	$class Option help {} {
	    Oc_Log Log [Oc_CommandLine Usage] info
	    exit
	} "Display this help message and exit"
	$class Option version {} {
	    Oc_Log Log "[Oc_Main GetAppName] [Oc_Main GetVersion]" info
	    exit
	} "Display version information and exit"
       $class Option $switch {} "\#do nothing" "End of options" single
    }

    private proc Panic {msg} {
	Oc_Log Log $msg\n[$class Usage] panic $class
	exit 1
    }

    proc Option {name argList script {docString ""} {type single}} {
	if {[string length $script] == 0} {
	    catch {unset arg($name) body($name) doc($name) valid($name)}
	    return
	}
	if {[catch {set argList [lrange $argList 0 end]}]} {
	    return -code error "Argument list for $name is not a valid list"
	}
	set docList [list $docString]
	set validList [list]
	set argSpec [list]
	foreach triple $argList {
	    if {[catch {set triple [lrange $triple 0 end]}]} {
	        return -code error \
			"Argument '$triple' for $name is not a valid list"
	    }
	    lappend argSpec [lindex $triple 0]
	    set validBody [string trim [lindex $triple 1]]
	    if {[string length $validBody]} {
		lappend validList $validBody
	    } else {
		lappend validList {return 1}
	    }
	    set docString [string trim [lindex $triple 2]]
	    if {[string compare $switch $name]} {
		if {[string length $docString]} {
		    lappend docList "where [lindex $triple 0] $docString"
		}
	    } else {
		lappend docList $docString
	    }
	}

        if {[string match "multi*" $type]} {
           set type multi
           lappend docList "(Multiple instances of this option are stacked)"
        } else {
           set type single
        }

	set arg($name) $argSpec
	set body($name) $script
	set doc($name) $docList
	set valid($name) $validList
        set opttype($name) $type
    }

    proc ActivateOptionSet { name } {
       switch -exact [string tolower $name] {
          tk {
             Oc_CommandLine Option colormap window "\#" \
                "Specify \"new\" to use private colormap (Tk)"
             Oc_CommandLine Option display display "\#" "Display to use (Tk)"
             Oc_CommandLine Option geometry geometry "\#" \
                "Initial geometry for window (Tk)"
             Oc_CommandLine Option name name "\#" "Name to use for application (Tk)"
             Oc_CommandLine Option sync {} "\#" "Use synchronous display mode (Tk)"
             Oc_CommandLine Option visual visual "\#" "Visual for main window (Tk)"
             Oc_CommandLine Option use {{id {} "is ID of window to embed in"}} "\#" \
                "Embed application in window (Tk)"
          }
          standard {
             Oc_CommandLine Option console {} [format {
                set file {%s}
                if {[file readable $file]} {
                   if {[catch {uplevel #0 [list source $file]} msg]} {
                      return -code error "Can't create console: $msg"
                   }
                } else {
                   return -code error "Can't create console: File not readable: $file"
                }
             } [file join [file dirname [info script]] console.tcl] ] \
                "Display console window (requires Tk)"
             Oc_CommandLine Option cwd {directory} {
                # Note: Don't use validationScript to check for error;
                # working directory may change between parsing of
                # command line and option implementation scripts. (In
                # particular, see the "-root" option to pimake.) Instead
                # catch any errors in implementation script and mimic
                # validationScript response.
                if {[set errcode [catch {cd $directory} errmsg]]} {
                   puts stderr "Error in -cwd option: $errmsg"
                   puts stderr [Oc_CommandLine Usage]
                   exit $errcode
                }
             } "Change working directory"
          }
          net {
             Oc_CommandLine Option nickname {
                {name {regexp {[^0-9]} $name}}
             } {
                Oc_Option Get Net_Account nicknames tempnames
                lappend tempnames $name
                Oc_Option Add * Net_Account nicknames $tempnames
             } {Nickname to assign to instance; default none\
                   (requires networking)} {multi}
          }
          default {
             set msg "Invalid option set name: \"$name\""
             error $msg $msg
          }
       }
    }

    proc Parse {argv} {
	set rest [list]
	set order [list]
	# In case option handling tries to redefine $switch
	set sswitch $switch
	while {[llength $argv]} {
	    set option [lindex $argv 0]
	    set argv [lrange $argv 1 end]
	    if {[string first $sswitch $option]} {
		# Not an option switch
		lappend rest $option
	    } else {
		# An option switch.  Strip leading switch characters
		set option [string range $option [string length $sswitch] end]
		if {[string compare $sswitch $option] == 0} {
		    # Double switch ==> End of option processing
		    break
		}
		set matches [array names body $option*]
		if {[llength $matches] > 1} {
		    # Exact match overrides ambiguity
		    if {[info exists body($option)]} {
			set matches [list $option]
		    } else {
			set tmp [join $matches ", $sswitch"]
			$class Panic "Ambiguous option $sswitch$option matches\
				$sswitch$tmp"
		    }
		}
		if {[llength $matches] == 0} {
		    $class Panic "Unknown option: $sswitch$option"
		}
		set match [lindex $matches 0]
		set numargs [llength $arg($match)]
		set optargs [lrange $argv 0 [expr {$numargs - 1}]]
		if {[llength $argv] < $numargs} {
		    $class Panic "Argument shortage for $sswitch$match"
		}
		set argv [lrange $argv $numargs end]
		# Call validation scripts
		foreach a $arg($match) v $valid($match) o $optargs {
		    proc _${class}_Validate $arg($match) $v
		    set code [catch {eval _${class}_Validate $optargs} result]
		    rename _${class}_Validate {}
		    if {$code} {
			global errorInfo
			error "Programming error\nError in validation script\
				for '$sswitch$match $a'\n\t$result" $errorInfo
		    }
		    if {[catch {expr {$result && $result}}]} {
			set msg "Programming error\nValidation script return\
				for '$sswitch$match $a' is not 0 or 1."
			error $msg $msg
		    }
		    if {!$result} {
			$class Panic "Invalid argument $a=$o\
				 in\n\t$sswitch$match $arg($match)"
		    }
		}
		# Arguments are valid.  Save for later evaluation
                if {[string match "single" $opttype($match)]} {
                   if {[info exists save($match)]} {
                      set idx [lsearch -exact $order $match]
                      set order [lreplace $order $idx $idx]
                   }
                   set save($match) $optargs
                } else {
                   lappend save($match) $optargs
                }
                lappend order $match
	    }
	}
	# Step through options in order
	foreach opt $order {
	    proc _${class}_HandleOption $arg($opt) $body($opt)
            if {[string match "single" $opttype($opt)]} {
               set optargs $save($opt)
            } else {
               set optargs [lindex $save($opt) 0]
               set save($opt) [lreplace $save($opt) 0 0]
            }
            set code [catch {eval _${class}_HandleOption $optargs} result]
	    rename _${class}_HandleOption {}
	    if {$code} {
		global errorInfo
		error $result "$errorInfo\n   \
			(command line option '$sswitch$opt $optargs')"
	    }
	}
	# Collect all remaining arguments into $rest
	eval lappend rest $argv
	# Create argument specification for procs
	set argSpec [list]
	set bodyPrefix ""
	set optcount 0
	set min 0
	foreach namePair $arg($sswitch) {
	    set name [lindex $namePair 0]
	    set type [lindex $namePair 1]
	    switch -- $type {
		optional {
		    lappend argSpec [list $name {}]
		    incr optcount
		}
		list {
		    lappend argSpec args
		    set bodyPrefix "set $name \$args;"
		    incr optcount
		    break
		}
		default {
		    if {$optcount} {
			set msg "Programming error\nRequired argument $name\
				follows optional argument(s)."
			error $msg $msg
		    }
		    lappend argSpec $name
		    incr min
		}
	    }
	}
	if {$optcount + $min < [llength $arg($sswitch)]} {
	    set msg "Programming error\nList argument $name must be last"
	    error $msg $msg
	}
	if {[llength $rest] < $min} {
	    $class Panic "Missing required argument:\
		    [lindex [lindex $arg($sswitch) [llength $rest]] 0]"
	}
	if {[string compare args [lindex $argSpec end]] 
		&& [llength $rest] > $min + $optcount} {
	    $class Panic "Too many arguments"
	}

# Would be more correct, it seems, to not do any processing when there
# are no optional arguments given, but that seems to break stuff (FileSoure,
# mmDisp).  Consider enabling this code when there's time to test.
#	if {([llength $rest] == 0) && ($min == 0)} {
#	    # No additional arguments, and none required.  Just return.
#	    return
#	}
#
	# Run the validation scripts
	foreach namePair $arg($sswitch) v $valid($sswitch) o $rest {
	    set name [lindex $namePair 0]
	    proc _${class}_Validate $argSpec $bodyPrefix$v
	    set code [catch {eval _${class}_Validate $rest} result]
	    rename _${class}_Validate {}
	    if {$code} {
		global errorInfo
		error "Programming error\nError in validation script\
			for '$name'\n\t$result" $errorInfo
	    }
	    if {[catch {expr {$result && $result}}]} {
		set msg "Programming error\nValidation script return\
			for '$name' is not 0 or 1."
		error $msg $msg
	    }
	    if {!$result} {
		$class Panic "Invalid argument $name=$o"
	    }
	    if {[string match list [lindex $namePair 1]]} {
		break
	    }
	}
	# Process the rest of the command line
	proc _${class}_Process $argSpec $bodyPrefix$body($sswitch)
	set code [catch {eval _${class}_Process $rest} result]
	rename _${class}_Process {}
	if {$code} {
	    global errorInfo
	    error $result "$errorInfo\n    (command line arguments\
		    '$arg($sswitch)')"
	}
    }

    proc Usage {} {
	set ret "Usage: [Oc_Main GetAppName] "
	if {[Oc_Application IsShell [Oc_Main GetAppName]]} {
	    append ret {[script] }
	}
	set optList [array names body]
	set idx [lsearch -exact $optList $switch]
	set optList [lsort [lreplace $optList $idx $idx]]
	if {[llength $optList]} {
	    append ret {[options]}
	}
	set optcount 0
	set min 0
	foreach namePair $arg($switch) {
	    set name [lindex $namePair 0]
	    set type [lindex $namePair 1]
	    switch -- $type {
		optional {
		    append ret " \[$name"
		    incr optcount
		}
		list {
		    append ret " \[$name ..."
		    incr optcount
		    break
		}
		default {
		    if {$optcount} {
			set msg "Programming error\nRequired argument $name\
				follows optional argument(s)."
			error $msg $msg
		    }
		    incr min
		    append ret " $name"
		}
	    }
	}
	if {$optcount + $min < [llength $arg($switch)]} {
	    set msg "Programming error\nList argument $name must be last"
	    error $msg $msg
	}
	while {$optcount} {append ret \]; incr optcount -1}
	if {[llength $arg($switch)]} {
	    append ret "\nwhere"
	}
	foreach namePair $arg($switch) d [lrange $doc($switch) 1 end] {
	    append ret "\n  [format %-13s [lindex $namePair 0]]$d"
	}
	if {![llength $optList]} {
	    return $ret
	}
	append ret "\nAvailable options:"
	foreach opt $optList {
	    append ret "\n  [format %-13s\ %-13s $switch$opt $arg($opt)]"
	    append ret [lindex $doc($opt) 0]
	    foreach d [lrange $doc($opt) 1 end] {
		append ret "\n                            $d"
	    }
	}
	if {[llength $arg($switch)]} {
	    append ret "\n  [format %-13s%-13s $switch$switch ""]"
	    append ret [lindex $doc($switch) 0]
	}
	return $ret
    }

    proc AvailableOptions {} {
	return [array names body]
    }

    proc Switch {args} {
	if {[llength $args]} {
	    set old $switch
	    set switch [lindex $args 0]
	    set body($switch) $body($old)
	    set valid($switch) $valid($old)
	    set arg($switch) $arg($old)
	    set doc($switch) $doc($old)
	    set opttype($switch) $opttype($old)
	    catch {unset arg($old) body($old) doc($old) valid($old) opttype($old)}
	}
	return $switch
    }

    Constructor {args} {
        return -code error "Can't create instances of $class"
    }

}

