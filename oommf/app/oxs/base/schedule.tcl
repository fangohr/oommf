##########################################################################
# Class representing a scheduled event
##########################################################################
Oc_Class Oxs_Schedule {

    private array common index ;# Maps output+destination pairs to
                               ## Oxs_Schedule instances.

    private common events {Step Stage Done} ;# Supported events
    private common mif_interp {}            ;# MIF child interpreter

    # Selection control
    private array variable active     ;# Boolean
    private array variable frequency  ;# Non-negative integer
    private array variable filter_script ;# filter script

    const public variable output
    const public variable destination

    Constructor {args} {
        $this Configure {*}$args
        # Only one schedule per output, destination pair
        if {![catch {set index($output,$destination)} exists]} {
            $this Delete
            array set a $args
            set index($a(-output),$a(-destination)) $exists
            return $exists
        }
        set index($output,$destination) $this
        regsub -all {:+} $output : op
        regsub -all "\[ \r\t\n]+" $op _ op
        regsub -all {:+} $destination : d
        regsub -all "\[ \r\t\n]+" $d _ d
        trace add variable active {write unset} [subst { uplevel #0 {
            set "Schedule-$op-$d-active" \
              \[[list array get [$this GlobalName active]]\]
        } ;\# }]
        trace add variable frequency {write unset} [subst { uplevel #0 {
            set "Schedule-$op-$d-frequency" \
            \[[list array get [$this GlobalName frequency]]\]
        } ;\# }]
        foreach e $events {
           set active($e) 0
           set frequency($e) 1
           set filter_script($e) {}
        }
        # Remove the Schedule whenever destination goes away.
        Oc_EventHandler New _ [Oxs_Destination Lookup $destination] Delete \
                [list $this Delete] -groups [list $this]

        # If an output goes away, then most likely a new problem has
        # been loaded.  We don't delete a schedule just because an
        # output goes away, because it may become valid again if the new
        # problem also has this output.  Instead, the Oxs_Schedule
        # RemoveInvalid proc should be called after the new problem is
        # loaded to remove any hanging schedules.
    }
    Destructor {
        Oc_EventHandler Generate $this Delete
        Oc_EventHandler DeleteGroup $this
        catch {unset index($output,$destination)}
    }
    proc ProblemInit { new_mif_interp } {
       if {[string compare $mif_interp $new_mif_interp]!=0} {
          set mif_interp $new_mif_interp
          # New child interp; clear any existing filter_scripts.
          # These scripts would have been set up in conjunction with
          # the old interp and anyway are probably invalid with the
          # new interp.
          foreach sched_instance [Oxs_Schedule Instances] {
             upvar #0 [$sched_instance GlobalName filter_script] fs
             foreach e [array names fs] {
                set fs($e) {}
             }
          }
       }
    }
    proc RemoveInvalid {} {
       # Runs through the list of all schedules, and deletes any with
       # invalid output or destination.  This routine is intended for
       # use right after a problem has been loaded, to clean up any
       # schedules left over from a previous problem which are invalid
       # for the new problem.
       foreach elt [Oxs_Schedule Instances] {
          set output [$elt Cget -output]
          set destination [$elt Cget -destination]
          if {[catch {Oxs_Output Lookup $output}]} {
             # output does not exist
             $elt Delete
          } elseif {[catch {Oxs_Destination Lookup $destination}]} {
             # destination does not exist
             $elt Delete
          }
       }
    }
    method Send {e} {
       upvar #0 [string tolower $e] count
       if {[string compare Done $e]!=0} {
          # Done event does not have a count
          set f $frequency($e)
          if {($f == 0  || ($count % $f) != 0) && $count != 0 } {
             # Documented f==0 behavior is to output iff count==0.
             return
          }
       }
       if {![string match {} $filter_script($e)]} {
          set cmd $filter_script($e)
          if {[catch {interp eval $mif_interp [list {*}$cmd $count]} result]} {
             return -code error "Error processing MIF scheduling script\
                                 $cmd: $result"
          }
          if {!$result} { ;# Script should return true if output is wanted
             return
          }
       }
       if {[catch {Oxs_Output Send $output $destination $e} msg]} {
          # If error is due to missing output or destination,
          # then delete the current schedule and swallow the
          # error.  Otherwise, pass the error upwards.
          global errorCode
          if {[string compare $errorCode OxsUnknownOutput] == 0 || \
                 [string compare $errorCode OxsUnknownDestination] == 0} {
             $this Delete
          } else {
             return -code error -errorcode $errorCode $msg
          }
       }
    }
    proc Set {o d w e v} {
       # Imports are output, destination, what, event, and value,
       # where "what" is either Active or Filter, and value is
       # a two element list, {frequency filter_script}.
        $index($o,$d) Set$w $e $v
    }
    method SetActive {event value} {
        set active($event) $value
        Oc_EventHandler DeleteGroup $this-$event
        if {$value} {
            Oc_EventHandler New _ Oxs $event [list $this Send $event] \
                    -groups [list $this-$event $this]
        } else {
           # Toggling active value resets filter script. (This allows
           # interactive override of MIF file Schedule commands.)
           set filter_script($event) {}
        }
    }
    method SetFilter {event value} {
       set frequency($event) [lindex $value 0]
       set filter_script($event) [lindex $value 1]
    }
    method SetFrequency {event freqvalue} {
       # Used by the interactive interface; changes frequency
       # without affecting the filter_script.
       set frequency($event) $freqvalue
    }

}
