##########################################################################
# Class representing a scheduled event
##########################################################################
Oc_Class Oxs_Schedule {

    private array common index

    # Add an interface to change these
    private common events {Step Stage Done}

    # Is this 
    private array variable active
    private array variable frequency

    const public variable output
    const public variable destination

    Constructor {args} {
        eval $this Configure $args
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
        trace variable active wu [subst { uplevel #0 {
            set "Schedule-$op-$d-active" \
            \[[list array get [$this GlobalName active]]]
        } ;# }]
        trace variable frequency wu [subst { uplevel #0 {
            set "Schedule-$op-$d-frequency" \
            \[[list array get [$this GlobalName frequency]]]
        } ;# }]
        foreach e $events {
            set active($e) 0
            set frequency($e) 1
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
    proc Set {o d w e v} {
        $index($o,$d) Set$w $e $v
    }
    method Send {e} {
       if {[string compare Done $e]!=0} {
          # Done event does not have a count
          upvar #0 [string tolower $e] count
          set f $frequency($e)
          if {$f != $count  &&  $f > 0  &&  $count % $f} {
             return
          }
       }
       if {[catch {Oxs_Output Send $output $destination} msg]} {
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
    method SetActive {event value} {
        set active($event) $value
        Oc_EventHandler DeleteGroup $this-$event
        if {$value} {
            Oc_EventHandler New _ Oxs $event [list $this Send $event] \
                    -groups [list $this-$event $this]
        }
    }
    method SetFrequency {event value} {
        set frequency($event) $value
    }
}
