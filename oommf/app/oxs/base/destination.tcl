##########################################################################
# Class representing an output destination thread.  Maintains global
# lists for each protocol.
##########################################################################
Oc_Class Oxs_Destination {
   # Array directory maps "alias<oid:#>" to Oxs_Destination objects.
   array common directory

   # Array threadmap maps strings of the form "alias<oid:#>" to
   # Net_Thread objects.
   array common threadmap

   # Array destmap maps "oid,protocol" keys to Oxs_Destination objects.
   # This is used by the Oxs_Mif CreateSchedule command. This essentially
   # assumes that no apps have multiple servers using the same protocol.
   array common destmap

   const public variable protocol
   const public variable name       ;# alias<oid:#>

   # If you have an already existing Net_Thread object, then that
   # alone suffices to create an Oxs_Destination instance. OTOH, if
   # you want to create an Oxs_Destination with Net_Thread created
   # lazily by Oxs_Destination only if or when it is first used,
   # then instead call the Oxs_Destination constructor with settings
   # for name (above) and the following four values (all needed to
   # create an Net_Thread from scratch). These four following values
   # remain otherwise undefined.
   const public variable fullprotocol
   const public variable hostname
   const public variable accountname
   const public variable serverport

   # For internal use only
   const private variable sid       ;# Service ID, called "pid" in some
                                     # contexts. has form "oid:#".
   Constructor { args } {
      if {[llength $args]==1} {
         # Pre-created thread passed in. Query thread to fill instance
         # variables.
         set thread [lindex $args 0]
         set protocol [$thread Protocol]
         set sid [$thread Cget -pid]
         set name [$thread Cget -alias]<$sid>
         if {[info exists threadmap($name)]} {
            return -code error  -errorcode BadConstructor \
               "Duplicate destination name"
         }
         set threadmap($name) $thread
         Oc_EventHandler New _ $thread Delete [list $this ThreadDead] \
            -groups [list $this]
      } else {
         # Store info for later creation of thread
         $this Configure {*}$args
         set missing {}
         foreach var [list fullprotocol name hostname accountname serverport] {
            if {![info exists $var]} {
               lappend missing $var
            }
         }
         if {[llength $missing]>0} {
            return -code error -errorcode BadConstructor \
               "Unset options in Oxs_Destination constructor: $missing"
         }
         if {![regexp {OOMMF ([^ ]+) protocol ([0-9.]+)} $fullprotocol \
                    _ protocol version]} {
            return -code error -errorcode BadConstructor \
               "Bad Oxs_Destination fullprotocol option:\
                     \"$fullprotocol\". Should have form\
                     \"OOMMF <name> protocol <version>\""
         }
         if {![regexp {^.*<([0-9]+:[0-9]+)>} $name _ sid]} {
            return -code error -errorcode BadConstructor \
               "Bad Oxs_Destination name option: \"$name\";\
                should have form appname<oid:#>"
         }
         if {[info exists threadmap($name)]} {
            return -code error  -errorcode BadConstructor \
               "Duplicate destination name"
         }
      }
      if {![regexp {^([0-9]+):[0-9]+$} $sid _ oid]} {
            return -code error "Invalid sid format: \"$sid\""
      }
      set destmap($oid,$protocol) $name

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
      Oc_EventHandler Generate $this NewDestination
    }
    proc Find {o p} {
	# Find the destination from process $o speaking protocol $p
	return $destmap($o,$p)
    }
    proc Lookup {n} {
        return $directory($n)
    }
    private method CreateAutoReadyThread {} {
       set appname [string range $name 0 [string last "<" $name]-1]
       if {[catch {
          Net_Thread New thread \
             -hostname $hostname \
             -accountname $accountname -pid $sid \
             -enable_cache 1 -protocol $fullprotocol \
             -alias $appname -port $serverport
       } errmsg]} {
          return -code error -errorcode OxsThreadError \
             "Thread create error: $errmsg"
       }
       set threadmap($name) $thread
       Oc_EventHandler New _ $thread SafeClose [list $this DeadThread] \
          -groups [list $this $this-DeadThread]
       Oc_EventHandler New _ $thread Delete [list $this DeadThread] \
          -groups [list $this $this-DeadThread]
       return $thread
    }
    proc Thread {n} {
       if {[info exists threadmap($n)]} { return $threadmap($n) }
       # Else try to create an auto-ready (i.e., cache enabled) thread
       # object.
       ## Note: Prior to Tcl 8.5 there was a speed advantage to using
       ## 'catch' instead of 'info exists' if the variable existed. In
       ## Tcl 8.5 and 8.6 'info exists' is as fast or faster than
       ## 'catch' in all cases.
       if {![info exists directory($n)]} {
          return -code error -errorcode OxsUnknownDestination \
             "Unknown destination: $n"
       }
       return [$directory($n) CreateAutoReadyThread]
    }
    proc DeleteService { sid } {
       set matches [array names directory -glob "*<${sid}>"]
       switch [llength $matches] {
          0 { ;# No matches --- do nothing
          }
          1 { ;# One match --- delete it
             $directory([lindex $matches 0]) Delete
          }
          default { return -code error -errorcode OxsNonuniqueService \
                       "Multiple matching services: $matches" }
       }
       return [llength $matches]
    }
    private variable store = {}
    method Store {value} {
	set store $value
    }
    method Retrieve {} {
	return $store
    }
    method DeadThread {} {
       # Thread has died. In particular, there is not need (or, way)
       # to 'Send bye' through the thread, so remove it from the
       # threadmap list before calling the destructor.
       Oc_EventHandler DeleteGroup $this-DeadThread
       if {[info exists name]} {
          unset -nocomplain threadmap($name)
       }
       $this Delete
    }
    Destructor {
       Oc_EventHandler Generate $this Delete
       Oc_EventHandler DeleteGroup $this
       catch {
          # Remove $this from ${protocol}List
          upvar #0 ${protocol}List list
          set idx [lsearch -exact $list $name]
          set list [lreplace $list $idx $idx]
       }
       if {[info exists name]} {
          if {[info exists threadmap($name)]} {
             catch {$threadmap($name) Send bye}
             unset threadmap($name)
          }
          unset -nocomplain directory($name)
       }
       catch {
          # catch in case sid or protocol are undefined
          unset -nocomplain \
             destmap([string range $sid 0 [string first : $sid]-1],$protocol)
       }
    }
}
