##########################################################################
# Class representing script interface to an output
##########################################################################
Oc_Class Oxs_Output {
   private array common closers
   private array common index
   private array common directory ;# Maps output names such as
   ## "Oxs_Demag::Field" to the corresponding Oxs_Output object.

   # Map from the "type" of data provided by an Output to the protocol
   # needed in order to send that data type out.
   private array common protocol {
      "vector field"  vectorField
      "scalar field"  scalarField
      DataTable       DataTable
   }

   # The last_id array records the state id for DataTable output last
   # written to each destination.  This is used to protect against
   # sending duplicate data. (See also member variables check_id and
   # check_exports used for field output.)
   private array common last_id

   # Dictionary used to pass context to output filename scripts
   private variable params

   # We assume that all Oxs_Output objects existing at the same time
   # are provided by the same loaded problem, i.e. the same MIF,
   # same driver, etc.
   # If Oxs ever allows multiple problems to be loaded at once,
   # re-think this.
   private common scalars {}
   private common driver

   # Dictionary of unclaimed temporary files.
   # The field output types (vector field and scalar field) write a
   # temporary file to disk and send the name of that file to the
   # server (e.g., mmDisp or mmArchive). Once the server acknowledges
   # receipt of the message, it becomes the responsibility of the
   # server to dispose of the temporary file. The tmpfiles dictionary
   # keeps track of files written to disk that have not (yet) been
   # claimed by the server.  In the case of a controlled shutdown,
   # the output class automatically deletes any unclaimed files.
   # To make housekeeping easy, each key in tmpfiles is a Net_Thread
   # id, and the value associated with each key is a nested dictionary
   # indexed by the Net_Link id associated with the send request.
   private common tmpfiles

   # Formats for embedding integer counts into filenames
   public variable iteration_fmt = "%07d"
   public variable stage_fmt = "%02d"

   # Data to protect against multiple writes of the same data to the
   # same file. See also common variable last_id. (Note: If check_exports
   # list becomes long, might want to consider changing to a dict.)
   private variable check_id = 0
   private variable check_exports = {}

   const public variable handle
   const public variable name
   const private variable type
   const public variable units
   const public variable filename_script = {}

   ClassConstructor {
      trace add variable index {write unset} [subst { uplevel #0 {
         set opAssocList \[[list array get [$class GlobalName index]]\]
      } ;\# }]
      array set index [list DataTable DataTable]
      array set directory [list DataTable DataTable]
      foreach d [Oxs_Destination Instances] {
         if {[string compare DataTable [$d Cget -protocol]] == 0} {
            Oxs_Schedule New _ -output DataTable \
               -destination [$d Cget -name]
         }
      }

      # Initialize temp file tracking. Note that ThreadExitCleanup
      # is tied to Net_Thread Delete, not Net_Thread SafeClose.
      # (See ThreadExitCleanup notes for details.)
      set tmpfiles [dict create]
      Oc_EventHandler New _ Net_Thread Delete \
         [list $class ThreadExitCleanup %object] \
         -groups $class

      # When the problem is released, output handles become invalid,
      # so Oxs_Output instances must be destroyed.
      Oc_EventHandler New _ Oxs Release [list $class DeleteAll]
   }
   proc ThreadExitCleanup { thread } {
      # This should be triggered on thread Delete, not thread
      # SafeClose.  The Oxs_FlushData proc generates a thread
      # SafeClose event on each thread, and then vwaits on thread
      # DeleteEnd. The thread calls its connection SafeClose, which
      # waits for replies from all outstanding messages before
      # calling its destructor which triggers thread Delete.
      if {[dict exists $tmpfiles $thread]} {
         # tmpfiles is a dictionary indexed by Net_Thread ids.  Each
         # tmpfile value is itself a dictionary. The subdictionary is
         # a list of temporary files indexed by the Net_Link message
         # id used to send the temporary filename to the
         # archive/display field server.
         set filedict [dict get $tmpfiles $thread]
         if {[dict size $filedict]>0} {
            file delete {*}[dict values $filedict]
         }
         dict unset tmpfiles $thread
      }
   }
   proc DeleteAll {} {
      # remove trace on index so we don't send message for every
      # individual object destruction
      trace remove variable index {write unset} [subst { uplevel #0 {
         set opAssocList \[[list array get [$class GlobalName index]]\]
      } ;\# }]
      foreach i [$class Instances] {
         $i Delete
      }
      global opAssocList
      set opAssocList [array get index]
      # re-establish trace on index
      trace add variable index {write unset} [subst { uplevel #0 {
         set opAssocList \[[list array get [$class GlobalName index]]\]
      } ;\# }]
   }
   Constructor {_handle} {
      set handle $_handle
      set params [dict create state_id 0]
      set namedict [dict create {*}[Oxs_OutputNames $handle]]
      dict update namedict owner owner name id type type units units \
         filename_script script {
            set name "$owner:$id"
            if {[info exists index($name)]} {
               return -code error "Duplicate output name: $name"
            }
            if {[string match scalar $type]} {
               if {[lsearch -exact $scalars $name] >= 0} {
                  return -code error "Duplicate output name: $name"
               }
               lappend scalars $name
               set directory($name) $this
            } else {
               # Register directly with the output list
               if {![info exists protocol($type)]} {
                  return -code error "No known protocol for sending output\
                            of type $type"
               }
               dict set params ptype $protocol($type)
               set index($name) $protocol($type)
               set directory($name) $this
               foreach d [Oxs_Destination Instances] {
                  if {[string compare $index($name) [$d Cget -protocol]] == 0} {
                     Oxs_Schedule New _ -output $name \
                        -destination [$d Cget -name]
                  }
               }
               if {[info exists script]} {
                  set filename_script $script
               }
            }
         }
      if {![info exists driver]} {
         set driver [Oxs_DriverName]
         Oc_EventHandler New _ Oxs Release \
            [list unset [$class GlobalName driver]] -oneshot 1
      }

      # Stage value format, for embedding into filenames
      foreach {step stage number_of_stages} [Oxs_GetRunStateCounts] break
      if {$number_of_stages>99} {
         set digits [expr {int(floor(log(double($number_of_stages))))+1}]
         set stage_fmt "%0${digits}d"
      }
      dict set params mif_interp [Oxs_GetMifInterpName]
      dict set params units         $units
      dict set params iteration_fmt $iteration_fmt
      dict set params stage_fmt     $stage_fmt
      dict set params name          $name
      dict set params saniname      [Oxs_Output SanitizeFilename $name]
   }
   Destructor {
      Oc_EventHandler Generate $this Delete
      Oc_EventHandler DeleteGroup $this
      set idx [lsearch -exact $scalars $name]
      if {$idx < 0} {
         unset index($name)
      } else {
         set scalars [lreplace $scalars $idx $idx]
      }
      catch {unset directory($name)}
   }

   proc GetOutputNames {} {
      return [array names directory]
   }

   proc Lookup {n} {
      return $directory($n)
   }

   method Protocol {} {
      if {[info exists protocol($type)]} {
         return $protocol($type)
      }
      return ""
   }

   private proc SendDataTable {dest thread event} {
      # The first two imports are Oxs_Destination and Net_Thread
      # objects. The event import is one of Step, Stage, Done, and
      # Interactive.
      #
      # NB: If DataTable output is sent to two mmArchive with the same
      #     basename, then both will write to the same file resulting in
      #     doubling and interleaving of data, headers, and trailers.

      # Protect against sending duplicates
      set id [Oxs_GetCurrentStateId]
      if {[string compare "Interactive" $event] != 0} {
         # Non-interactive event. Protect against multiple sends.
         if {[catch {set last_id(DataTable,$dest)} checkid]} {
            set checkid 0
         }
         if {$id == $checkid} {
            return -1   ;# Data already sent.  Otherwise, the return
            ## value is the Net_Thread message id.
         }
         set last_id(DataTable,$dest) $id ;# Record current data id
      }
      # Data to send, including file destination header
      set basename [$dest Cget -basename]
      set header [list @Filename:$basename.odt {} 0]
      set triples [Oxs_GetAllScalarOutputs]
      set triples [linsert $triples 0 $header]

      # We're sending DataTable data.  Set up an event to guarantee
      # the table gets closed when the problem ends.
      if {![info exists closers($thread)]} {
         Oc_EventHandler New closers($thread) Oxs Cleanup \
            "[list $thread Send DataTable [list [list @Filename: \
                    {} 0]]]; [list unset [$class GlobalName \
                    closers]($thread)]" -groups [list $thread] -oneshot 1
      }
      return [$thread Send DataTable $triples]
   }

   proc SanitizeFilename { namestring } {} {
      # Tweaks namestring to make it filesystem friendly:
      # 1) Replace each colon or run of colons with a hyphen
      # 2) Replace whitespace with an underscore
      # 3) Replace each forward and backward slash with an underscore
      regsub -all {:+} $namestring {-} filename
      regsub -all "\[ \r\n\t]+" $filename {_} filename
      regsub -all {[/\\]} $filename {_} filename
      return $filename
   }

   proc DefaultFieldFilename { import_params } {} {
      # Copy needed values out of import_params. Use foreach + 'dict
      # get' rather than 'dict with' or 'dict update' because 'with' and
      # 'update' write back to the dict at closing, which forces a copy
      # of import_params. (import_params is copy-on-write)
      # NB: The third (optional) argument to 'proc', here the empty
      #     string '{}', explicitly declares which class variables are
      #     visible inside the proc.  If this argument is missing then
      #     all variables are brought in, which (a) invokes some
      #     overhead, and (b) invites name clashes. For example, without
      #     this argument naming the import argument "params" instead of
      #     "import_params" raises a 'variable "params" already exists'
      #     error.
      foreach v {iteration iteration_fmt stage stage_fmt units \
                    saniname ptype basename} {
         set $v [dict get $import_params $v]
      }
      set iteration [format $iteration_fmt $iteration]
      set stage [format $stage_fmt $stage]
      set ext .ovf ;# Default
      switch -exact -- $units {
         T {
            set ext .obf ;# vector field in Tesla
         }
         A/m {
            switch -glob -- [string tolower $saniname] {
               *field* {set ext .ohf}
               *magnetization* {set ext .omf}
               default {set ext .ovf}
            }
         }
         J/m^3  {
            set ext .oef ;# energy density field in Joules/meter^3
         }
         {} {
            if {[string compare vectorField $ptype]==0} {
               set ext .omf ;# spin (M/Ms) field
            }
         }
      }
      return $basename-$saniname-$stage-$iteration$ext
   }
   proc FillParams { id } {
      # Return list of thoses part of params which depend on state_id but
      # not the individual Oxs_Output instance.
      set idx [lsearch -glob $scalars $driver:Iteration]
      set iteration [expr {int([Oxs_OutputGet \
                                   [$directory([lindex $scalars $idx]) Cget -handle]])}]
      set idx [lsearch -glob $scalars $driver:Stage]
      set stage [expr {int([Oxs_OutputGet \
                               [$directory([lindex $scalars $idx]) Cget -handle]])}]
      return [list state_id $id iteration $iteration stage $stage]
   }
   method FillParams { dest thread event } {
      # NB: This code as written leaves in place previous values. In
      #     principle one could here completely delete params and
      #     rebuild from scratch. That would protect against reuse of
      #     out-of-date values, but may be somewhat slower (untested).
      set id [Oxs_GetCurrentStateId]
      if {$id != [dict get $params state_id]} {
         # Refill base part of params
         set updatelist [Oxs_Output FillParams $id]
      }
      lappend updatelist event $event basename [$dest Cget -basename]
      set params [dict replace $params {*}$updatelist]
   }

   method Send {dest thread event} {
      # The first two imports are Oxs_Destination, and Net_Thread
      # objects. The event import is one of Step, Stage, Done, and
      # Interactive.

      # Send field output to $thread.
      $this FillParams $dest $thread $event

      # Construct filename
      if {[string match {} $filename_script]} {
         # Default file naming (handles scalar fields and vector fields)
         set filename [Oxs_Output DefaultFieldFilename $params]
      } else {
         # User file naming script
         set mif_interp [dict get $params mif_interp]
         if {[catch {
            interp eval $mif_interp [list $filename_script $params]
         } filename]} {
            return -code error "Error processing MIF script\
                                $filename_script: $filename"
         }
         set filename [Oxs_Mif MakeFullPath $filename] ;# Otherwise
         ## a relative path is going to be interpreted relative to the
         ## current directory of mmArchive (or whatever the destination
         ## application is) instead of relative to the MIF file.
      }

      # Protect against repeats to one destination output associated
      # with a given output filename. Note that if an output is sent to
      # two different mmArchive objects, two separate temporary files
      # will be written, and each mmArchive will move their file to the
      # same destination. This code assumes that the file system move
      # operation is atomic.
      set export_id [list $dest $filename]
      set id [dict get $params state_id]
      if { $id != $check_id } {
         set check_id $id   ;# New state; reset check status
         set check_exports [list $export_id]
      } else {
         # Note: The default filename script always returns the same
         # filename for a given state_id, so in principle the repeat
         # check could be performed w/o constructing filename.
         if {[lsearch -exact $check_exports $export_id]<0} {
            # New export. Store export_id and send data.
            lappend check_exports $export_id
         } elseif {[string compare "Interactive" $event] != 0} {
            return -1  ;# Data already sent, so ignore repeats.
            ## (The non-error return value is the Net_Thread message
            ## id.) A special case is interactive requests, which are
            ## always honored.
         }
      }

      # Piece together a temporary file name
      set temp [Oc_TempName [file tail [dict get $params basename]] \
                   [file extension $filename]]
      Oxs_OutputGet $handle $temp

      # Send data
      set msgid [$thread Send datafile [list $temp $filename]]
      dict set tmpfiles $thread $msgid $temp
      Oc_EventHandler New _ $thread Reply${msgid} \
         [list dict unset [$class GlobalName tmpfiles] $thread $msgid] \
         -groups [list $thread]
      return $msgid
   }

   proc Send {outputname destname event} {
      # Imports outputname and destname are output and destination names
      # as displayed in the Oxsii and Boxsi interfaces. An example pair is
      # "Oxs_Demag::Field", "mmArchive<2:2>".
      #
      # Supported events are Step, Stage, Done, and Interactive.
      if {[catch {
         set output $directory($outputname)
         set thread [Oxs_Destination Thread $destname]
         set dest   [Oxs_Destination Lookup $destname]
      } errmsg]} {
         return -code error -errorcode OxsUnknownOutput \
            "Oxs_Output Send error: $errmsg"
      }

      # DataTable output gets special handling
      if {[string compare DataTable $output]==0} {
         return [$class SendDataTable $dest $thread $event]
      }

      return [$output Send $dest $thread $event]
   }
}
