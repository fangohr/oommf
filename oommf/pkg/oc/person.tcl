# FILE: person.tcl
#
# Last modified on: $Date: 2007/03/21 01:02:41 $
# Last modified by: $Author: donahue $
#

Oc_Class Oc_Person {

    common configDir
    common personsDir 
    common cacheDir

    ClassConstructor {
        set configDir [file join [file dirname [file dirname [file dirname \
                [Oc_DirectPathname [info script]]]]] config]
        set personsDir [file join $configDir persons]
        foreach fn [glob -nocomplain [file join $personsDir *.tcl]] {
            if {[file readable $fn]} {
                if {[catch {uplevel #0 [list source $fn]} msg]} {
                    Oc_Log Log "Error sourcing $fn:
	$msg" warning Oc_Person
                }
            }
        }
        $class New _ -id unknown -name Unknown -email ""
    }

    proc Lookup {id} {
        foreach i [$class Instances] {
            if {[string match $id [$i Cget -id]]} {
                return $i
            }
        }
        return [$class Lookup unknown]
    }

    public variable id
    public variable name
    public variable email

    Constructor {args} {
        eval $this Configure $args
    }

}

