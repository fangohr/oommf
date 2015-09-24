# FILE: fieldrange.tcl
#
# Applied nominal field step control.
#

Oc_Class mms_fieldrange {
    private variable index_count = 0  ;# Total number of field values.
    ## This is one more than the number of steps

    private variable index_current = 0  ;# Current index, >=0 and
    ## <$index_count

    private variable field_list = {} ;# List of field values, indexed
    ## by index_current.  Each value is itself a 3-tuple: Hx Hy Hz.

    private variable ctrl_list = {} ;# List of list of
    ## 2-tuples: -type value, where -type is one of -torque, -time,
    ## OR -iteration.  There should be one (possibly empty) list of
    ## 2-tuples per index.

    Constructor { args } {
	eval $this Configure $args
    }
    method GetFieldIndex {} { index_current } {
	return $index_current
    }
    method GetFieldCount {} { index_count } {
	return $index_count
    }
    method SetFields { rangelist } {
	# rangelist is a list of 7+-tuples.  Each 7+-tuple
	# consists of a start and stop field (each a 3-vector), a
	# step count, and optional control point specifications.

	set rangecount [llength $rangelist]

	if {$rangecount<1} {
	    # Empty list.  Default to 0 field, no steps,
	    # no control point specs.
	    set rangelist [list [list 0. 0. 0. 0. 0. 0. 0]]
	    set rangecount 1
	}

	# Remove any preexisting data
	set field_list {}
	set ctrl_list {}

	# Loop through ranges
	for {set j 0} {$j<$rangecount} {incr j} {
	    set temp [lindex $rangelist $j]
	    if {[llength $temp]<7} {
		error "Bad list in proc range"
	    }
	    set ax [lindex $temp 0]
	    set ay [lindex $temp 1]
	    set az [lindex $temp 2]
	    set stepcount [expr double([lindex $temp 6])]
	    set ctrls [lrange $temp 7 end]

	    # If this is a range (i.e., stepcount>0), skip
	    # over the first point if it specifies the same
	    # field as the previous step.
	    set skipstep 0
	    if { $stepcount>0 && $j>0 } {
		foreach { xold yold zold } [lindex $field_list end] {}
		if { $xold == $ax && $yold == $ay && $zold == $az } {
		    set skipstep 1
		}
	    }
	    if {!$skipstep} {
		lappend field_list [list $ax $ay $az]
		lappend ctrl_list $ctrls
	    }

	    # Put in rest of range
	    if {$stepcount>0} {
		# If stepcount<=0, then ignore second endpoint
		set bx [lindex $temp 3]
		set by [lindex $temp 4]
		set bz [lindex $temp 5]
		set delx [expr ($bx-$ax)/$stepcount]
		set dely [expr ($by-$ay)/$stepcount]
		set delz [expr ($bz-$az)/$stepcount]
		for {set k 1} {$k<$stepcount} {incr k} {
		    lappend field_list [list \
			    [expr $ax + $delx*$k] \
			    [expr $ay + $dely*$k] \
			    [expr $az + $delz*$k]  ]
		    lappend ctrl_list $ctrls
		}
		lappend field_list [list $bx $by $bz]
		lappend ctrl_list $ctrls
	    }
	}

	# Set class indexing variables
	set index_count [llength $field_list]
	set index_current 0
    }
    method Reset {} {
	set index_current 0
    }
    method StepField { {amount {}} } { index_current index_count } {
	if {[string match {} $amount]} {
	    set amount 1
	}
	incr index_current $amount
	if {$index_current<=0} {
	    set index_current 0
	} elseif {$index_current>=$index_count} {
	    set index_current [expr $index_count-1]
	}
	return $index_current
    }
    method GetField {{index {}}}  {index_current index_count field_list} {
	# Note: Multiple, sequential indices may return the same field
	if {$index_count<1} {
	    return "0. 0. 0."     ;# Return 0 field if empty
	}
	if {[string match {} $index]} {
	    set index $index_current
	}
	if {$index<0} {
	    set index 0
	} elseif {$index>=$index_count} {
	    set index [expr $index_count-1]
	}
	return [lindex $field_list $index]
    }
    method GetControlSpec { {index {}} } \
	    { index_count index_current ctrl_list } {
	if {$index_count<1} {
	    return {}     ;# No input ranges
	}
	if {[string match {} $index]} {
	    set index $index_current
	}
	if {$index<0} {
	    set index 0
	} elseif {$index>=$index_count} {
	    set index [expr $index_count-1]
	}
	return [lindex $ctrl_list $index]
    }
    Destructor {}
}
