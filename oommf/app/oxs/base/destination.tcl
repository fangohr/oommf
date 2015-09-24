##########################################################################
# Class representing an output destination thread.  Maintains global
# lists for each protocol.
##########################################################################
Oc_Class Oxs_Destination {
    array common directory
    array common index
    array common find
    const public variable name
    const public variable protocol
    const private variable oid

    Constructor {thread} {
        set pid [$thread Cget -pid] 
	set oid [lindex [split $pid :] 0]
        set name [$thread Cget -alias]<$pid>
        if {[info exists index($name)]} {
            return -code error "Duplicate destination name"
        }
        set index($name) $thread
        set protocol [$thread Protocol]
	set find($oid,$protocol) $name
        upvar #0 ${protocol}List list
        lappend list $name
        set list [lsort -dictionary $list]
        set directory($name) $this 
        foreach o [Oxs_Output Instances] {
            if {[string compare $protocol [$o Protocol]] == 0} {
                Oxs_Schedule New _ -output [$o Cget -name] \
                        -destination $name
            }
        }
        if {[string compare $protocol DataTable] == 0} {
            Oxs_Schedule New _ -output DataTable -destination $name
        }
        Oc_EventHandler New _ $thread Delete [list $this Delete] \
                -groups [list $this]
    }
    Destructor {
        Oc_EventHandler Generate $this Delete
        Oc_EventHandler DeleteGroup $this
        upvar #0 ${protocol}List list
        set idx [lsearch -exact $list $name]
        set list [lreplace $list $idx $idx]
        catch {unset index($name)}
        catch {unset directory($name)}
        catch {unset find($oid,$protocol)}
    }
    proc Find {o p} {
	# Find the destination from process $o speaking protocol $p
	return $find($o,$p)
    }
    proc Lookup {n} {
        return $directory($n)
    }
    proc Thread {n} {
        return $index($n)
    }
    private variable store = {}
    method Store {value} {
	set store $value
    }
    method Retrieve {} {
	return $store
    }
}

