# FILE: mif.tcl
#
# Class for holding micromagnetic problem spec
#
# The input file should look like
#
#       # MIF 2.1
#       Specify Energy:UniformExchange: {
#           A 21e-12
#       }
#       Specify Energy:UniformAnisotropy: {
#           Type Uniaxial
#           K1   0.5e3
#           Axis 1 0 0
#       }
#
# This is evaluated inside a slave interpreter, where "unsafe" commands
# are likely not available (depending upon the setting of the "MIFinterp
# safety" config option).
#
Oc_Class Oxs_Mif {
    # regression_test_level defaults to 0, which means not a regression
    # test. Level 1 indicates a load test, level 2 or higher is a debug
    # test. See proc SetRegressionTestLevel for details on setting up a
    # regression test run.
    private common regression_test_level 0
    private common regression_test_basename {}

    private variable speckeys           ;# Ordered list of keys
    private array variable spec
    private variable destkeys = {}      ;# Ordered list of keys
    private array variable dest
    private array variable extra
    private array variable assign
    private array variable assigned
    private array variable schedule
    private variable random_seed = {}

    private array variable oid_wait     ;# PIDs awaiting OID assignment

    # mif_interp is slave interpreter in which the MIF file is
    # eval'ed.  Normally this slave is deleted along with Oxs_Mif
    # object deletion, but there are some cases where errors occurring
    # inside the slave lead to destruction of the Oxs_Mif object,
    # but we don't won't to delete the slave while it is inside
    # an eval.  The mif_interp_nodelete variable protects against
    # this.
    const public variable mif_interp
    private variable mif_interp_nodelete = 0

    private variable mif_filename
    private variable parameters
    private array variable tmp_params

    private variable safety_level
    private variable mif_crc
    private variable mif_file_contents = {}
    private variable mif_major_version  ;# *Effective* file version, as stored
    private variable mif_minor_version  ;# in mif_file_contents.  May differ
    ## from version in file, if mifconvert was automatically called to
    ## up-convert.

    private array variable options

    Constructor { args } {
        eval $this Configure $args
        set speckeys {}

        # Setup slave interpreter
        if {[Oc_Option Get MIFinterp safety safety_level]} {
            return -code error \
                    "Option value \"MIFinterp safety\" not set\
                    (see file oommf/config/options.tcl)."
        }
        if {[string match unsafe $safety_level]} {
            set mif_interp [interp create]
        } else {
            set mif_interp [interp create -safe]
        }

        # Make sure tcl_precision is set appropriately in slave
        # (Probably only an issue in Tcl earlier than 8.0.)
        # Note: A bug in Tcl 8.0 causes
        #    interp eval $mif_interp info exists tcl_precision
        # to report 0, even though
        #    interp eval $mif_interp set tcl_precision
        # returns 17.  Moreover, in Tcl 8.0 and later, it is
        # illegal to reset tcl_precision from inside a safe
        # interpreter.
        if {[catch {interp eval $mif_interp set tcl_precision} _] || \
               (0<$_ && $_<17)} {
           interp eval $mif_interp set tcl_precision 17
        }

        # Add basic aliases
        interp alias $mif_interp Specify {} $this CreateExtObject
        interp alias $mif_interp ClearSpec {} $this UnsetSpec
        interp alias $mif_interp Report {} $this Status
        interp eval $mif_interp [list proc Ignore args {}]
        interp alias $mif_interp RandomSeed {} $this RandomSeed
        interp alias $mif_interp Random {} Oc_UnifRand
        interp alias $mif_interp ReadRNGState {} Oc_ReadRNGState
        interp alias $mif_interp OOMMFRootDir {} Oc_Main GetOOMMFRootDir
        interp alias $mif_interp Parameter {} $this SetParameter
        interp alias $mif_interp Destination {} $this Destination
        interp alias $mif_interp Schedule {} $this Schedule
        interp alias $mif_interp GetMifFilename {} $this GetFilename
        interp alias $mif_interp GetMifParameters {} $this GetParameters
        interp alias $mif_interp GetOptions {} $this GetAllOptions
        interp alias $mif_interp SetOptions {} $this SetOptions
        interp alias $mif_interp EvalScalarField {} Oxs_EvalScalarField
        interp alias $mif_interp EvalVectorField {} Oxs_EvalVectorField
        interp alias $mif_interp QueryState {} Oxs_QueryState
        interp alias $mif_interp GetStateData {} Oxs_Mif GetStateData
        interp alias $mif_interp GetAtlasRegions {} Oxs_GetAtlasRegions
        interp alias $mif_interp GetAtlasRegionByPosition \
           {} Oxs_GetAtlasRegionByPosition
        if {![string match safe $safety_level]} {
            # Add extras to "custom" and "unsafe" interpreters
            if {[Oc_Application CompareVersions [info tclversion] 8.0]<0} {
               # Hack for 7.6 auto loader bug
               catch {auto_load Oxs_ReadFile}
               catch {auto_load Oxs_RGlob}
               catch {auto_load Oxs_DateSort}
            }
            interp alias $mif_interp ReadFile {} Oxs_ReadFile
            interp alias $mif_interp RGlob {} Oxs_RGlob
            interp alias $mif_interp DateSort {} Oxs_DateSort
        } else {
            # In safe interpreters, set up "extra" commands
            # to raise errors
            interp alias $mif_interp ReadFile {} $this Missing ReadFile
            interp alias $mif_interp RGlob {} $this Missing RGlob
            interp alias $mif_interp DateSort {} $this Missing DateSort
        }
        # Make sure child interp uses same atan2 function as parent. (In
        # particular, as concerns behavior of atan2(0.,0.).)
        interp alias $mif_interp ::tcl::mathfunc::atan2 {} ::tcl::mathfunc::atan2

        # Initialize "guaranteed to exist" options
        set options(basename) oxs
        set options(scalar_output_format) "%.17g"
        set options(scalar_field_output_format) "binary 8"
        set options(scalar_field_output_meshtype) rectangular
        set options(scalar_field_output_writeheaders) 1
        set options(vector_field_output_format) "binary 8"
        set options(vector_field_output_meshtype) rectangular
        set options(vector_field_output_writeheaders) 1
        ## NOTE 1: The basename option is reset to a form based on
        ##    the MIF filename when a file is loaded.  This may
        ##    of course be overridden by a SetOptions command in
        ##    the MIF file itself.
        ## NOTE 2: The scalar/vector_field_output_writeheaders options
        ##    are currently nop placeholders. In the future, the
        ##    DataTable communication protocol may be extended to
        ##    support raw output, in which case output.tcl can be
        ##    modified to query this value and adjust its output stream
        ##    accordingly.

        ## Let child know if regression test is running.
        if {$regression_test_level>0} {
           interp eval $mif_interp set regression_test $regression_test_level
           $this RandomSeed 72 ;# Default initialization
        } else {
           $this RandomSeed ;# Initialize random number generators from
                            ## system clock
        }
    }

    method KillOids {killtags} {
       if {([llength $killtags] == 1)
           && [string match all [lindex $killtags 0]]} {
          set killtags [array names assign]
       }
       set oidlist {}
       foreach tag $killtags {
          if {[catch {set assign($tag)} oid]} {
             Oc_Log Log "No such tag: $tag\nTags are:\
                        [array names assign]" warning
          } else {
             lappend oidlist $oid
          }
       }
       return $oidlist
    }

    callback method Status {m} {
        Oc_Log Log $m info $class
    }

    method Destination {tag instance args} {
        # Used to "declare" a destination in a MIF file.
        # The $tag is a name that is associated with that destination
        # for use in [Schedule] commands in the same MIF file.
        # $instance is the name of the program instance to run.
        # It has the form $app(:$name)? where $app is the name of a
        # program, and $name is an identifier for the particular instance.

        # $args is a list of 2*N + M arguments, where N is the number
        # of name-value pairs holding optional extra arguments, and
        # M is 0 or 1 depending on the presence of a final "new"
        # argument.

        set new ""
        if {[llength $args] % 2} {
            # odd number of args, first is "new"
            set new [lindex $args 0]
            set args [lreplace $args 0 0]
        }

        if {[lsearch -exact $destkeys $tag] != -1} {
            return -code error "Duplicate definition of Destination \"$tag\""
        }
        set parts [split $instance :]
        if {[llength $parts] == 0} {
            return -code error "No application for Destination \"$tag\""
        }
        set app [lindex $parts 0]
        if {[catch {Oc_Application CommandLine $app}]} {
            # NOTE: this tests what programs we know how to launch,
            # which might not necessarily be the same as what programs
            # are registered with the account server, but assume the same
            # for now.
            return -code error "Unknown application \"$app\" for\
                    Destination \"$tag\""
        }
        set name [join [lrange $parts 1 end] :]
        if {[string match new $new] && [regexp {^[0-9]+$} $name]} {
           return -code error \
              "Destination nicknames must contain non-numeric character;\
               Invalid nickname: $name"
        }
        lappend destkeys $tag
        set dest($tag) [list $app $name [string match new $new]]
        set extra($tag) $args
    }

    method Schedule {o d e {f {}} } {
        # Establish an initial schedule to set output named $o to destination
        # with tag $d on the event $e with frequency $f.  This amounts to
        # setting up for the creation of an Oxs_Schedule instance.

        # Would like to check for unknown outputs, but we don't know
        # them yet.  They will appear during problem reset.
        if {[lsearch -exact $destkeys $d] == -1} {
            return -code error "Unknown Destination Tag \"$d\""
        }
        set e [string toupper \
                [string index $e 0]][string tolower [string range $e 1 end]]
        if {[lsearch -exact {Step Stage Done} $e] == -1} {
            return -code error "Unknown Event \"$e\""
        }
        if {[string compare Done $e] == 0 && [string match {} $f]} {
           set f 1  ;# Done event doesn't require a frequency count
        }
        if {[catch {incr f 0}] || ($f < 0)} {
            return -code error "Frequency must be a non-negative integer,\
                    not \"$f\""
        }
        if {[string compare Done $e] == 0 && ($f > 1)} {
            return -code error "Frequency for Done event must have value 1,\
                    not \"$f\""
        }
        # Only one schedule per (output, dest, event) triple.
        if {[info exists schedule($o,$d,$e)]} {
            return -code error "Multiple schedules for sending\
                    \"$o\" to \"$d\" on \"$e\" events"
        }
        set schedule($o,$d,$e) [list $o $d $e $f]
    }

    method CreateSchedule {} {
        set script ""
        foreach key [array names schedule] {
            # Get the arguments of the [Schedule] command.
            # Here o is the output object, tag is the destination tag,
            # e is the event and f is frequency.
            foreach {o tag e f} $schedule($key) {break}

            # Verify that $o names an output
            if {[catch {Oxs_Output Lookup $o} output]} {
                Oc_Log Log "Error in \"Schedule $o $tag $e $f\":\
                        no such output \"$o\"" error
                continue
            }

            # Transform the destination tag into an Oxs_Destination name
            # The destination tag directly determines an OID.
            set oid $assign($tag)

            # The OID identifies a running process.  The service protocol
            # sought by the output determines which service of that process
            # we want to use as a destination.

            # NB: At this point we know that the application we seek is
            # known to the account server; we do not know if the service
            # has been registered there, let alone whether an
            # Oxs_Destination instance exists here to represent it!
            #
            # Ack!  DataTable is special!
            if {[catch {
               if {[string match DataTable $output]} {
                  set d [Oxs_Destination Find $oid DataTable]
               } else {
                  set d [Oxs_Destination Find $oid [$output Protocol]]
               }
               [Oxs_Destination Lookup $d] Store $extra($tag)
            }]} {
               # A destination could not be found; wait until another
               # destination is created and try again.
               Oc_EventHandler New _ Oxs_Destination NewDestination \
                  [list $this CreateSchedule] -oneshot 1 -groups [list $this]
               return
            }

            # Set up the schedule
            append script "
            [list Oxs_Schedule Set $o $d Frequency $e $f]
            [list Oxs_Schedule Set $o $d Active $e 1]\n"
         }
       eval $script
       Oxs_Schedule RemoveInvalid
       Oc_EventHandler Generate $this ScheduleReady
    }

    method SetupSchedule {acct} {
        # Once the destination tags are all assigned values, create the
        # schedule:
        if {[llength $destkeys]} {
            Oc_EventHandler New _ $this DestinationTagsAssigned \
                    [list $this CreateSchedule] -oneshot 1 -groups [list $this]
            $this FindDestinations $acct $destkeys
        } else {
            $this CreateSchedule
        }
    }

    method LaunchNewDestination {acct app tag {claim 0}} {
        # Launch the application in the background; get the process id
        set pid [Oc_Application Exec $app \
                    > [[Oc_Config RunPlatform] GetValue path_device_null] \
                    2> [[Oc_Config RunPlatform] GetValue path_device_null] &]

        # Retrieve the OID for that process id from the account server
        set qid [$acct Send getoid $pid]
        Oc_EventHandler New _ $acct Reply$qid \
                [list $this GetOidReply $acct $tag $pid $claim] -oneshot 1 \
                -groups [list $this $acct]
    }

    method NotifyNewOid {acct tag p claim} {
        set reply [$acct Get]
        if {![string match notify [lindex $reply 0]]} {
            return
        }
        if {![string match newoid [lindex $reply 1]]} {
            return
        }
        set pid [lindex $reply 2]
        if {[string compare $pid $p]} {
            return
        }
        Oc_EventHandler DeleteGroup NotifyNewOid-$pid
        unset oid_wait($pid)
        if {[array size oid_wait] == 0} {
           Oc_EventHandler DeleteGroup AccountServerReset-$this
        }

        # If we claimed a name, make that association
        set oid [lindex $reply 3]
        if {$claim} {
                foreach {app name new} $dest($tag) break
                set qid [$acct Send associate $app:$name $oid]
                Oc_EventHandler New _ $acct Reply$qid \
                        [list $this AssociateCheck $acct $name $oid] \
                        -oneshot 1 -groups [list $this $acct]
        }

        $this Assign $tag $oid
        return -code return
    }

    method AccountServerReset { acct } {
       # If account server crashes, then this method resets
       # NotifyNewOid requests for launched PIDs still awaiting OID
       # assignment.
       #   If the account server goes down again time while this
       # method is still resetting from a previous account server
       # crash, then this routine may be recursively called.  It's not
       # clear if that is a problem or not...
       if {[array size oid_wait]==0} { return }
       array set oid_wait_copy [array get oid_wait]
       unset oid_wait ;# GetOidReply may be called while this
                      ## method is running.
       foreach pid [array names oid_wait_copy] {
          Oc_EventHandler DeleteGroup [list NotifyNewOid-$pid]
          foreach {tag claim} $oid_wait_copy($pid) break
          set qid [$acct Send getoid $pid]
          Oc_EventHandler New _ $acct Reply$qid \
              [list $this GetOidReply $acct $tag $pid $claim] -oneshot 1 \
              -groups [list $this $acct]
       }
    }

    method GetOidReply {acct tag pid claim} {
        set reply [$acct Get]
        if {[lindex $reply 0]} {
            # The process ID was not known by the account server, but
            # a watch has been set to notify us when it shows up.  Set
            # up to receive that notification
            Oc_EventHandler New _ $acct Readable \
                    [list $this NotifyNewOid $acct $tag $pid $claim] \
                    -groups [list $this NotifyNewOid-$pid]
            if {[array size oid_wait]==0} {
               # Register handler for account server crash.
               Oc_EventHandler New _ $acct NewAccountServer \
                   [list $this AccountServerReset $acct] \
                   -groups [list $this AccountServerReset-$this $acct] \
                   -oneshot 1
            }
            set oid_wait($pid) [list $tag $claim]
        } else {
            # We got the OID
            set oid [lindex $reply 1]

            # If we claimed a name, make that association
            if {$claim} {
                foreach {app name new} $dest($tag) break
                set qid [$acct Send associate $app:$name $oid]
                Oc_EventHandler New _ $acct Reply$qid \
                        [list $this AssociateCheck $acct $name $oid] \
                        -oneshot 1 -groups [list $this $acct]
            }

            # Assign the OID to the destination tag
            $this Assign $tag $oid
        }
    }

    method AssociateCheck {acct name oid} {
        # All this does is report an error message in case of trouble
        set reply [$acct Get]
        if {[lindex $reply 0]} {
            set msg "associate $name $oid failed: [lindex $reply 1]"
            error $msg $msg
        }
    }

    method Assign {tag oid} {
        if {[info exists assigned($oid)]} {
            return -code error "Attempt to assign OID \"$oid\" to two tags"
        }
        set assign($tag) $oid
        set assigned($oid) $tag
        if {[array size assign] == [llength $destkeys]} {
            # We have as many OID assignments as tags; all must be assigned
            Oc_EventHandler Generate $this DestinationTagsAssigned
        }
    }

    method FindDestinations {acct taglist} {
        # Make sure we know what process corresponds to each
        # Destination tag...
        # The "dest" array maps tag -> (app, name) as recorded
        # from Destination commands

        # Construct the "find" command
        set cmd [list $acct Send find]
        set seeking [list]
        foreach tag $taglist {
            foreach {app name new} $dest($tag) break
            if {$new} {
                $this LaunchNewDestination $acct $app $tag
            } else {
                lappend seeking $tag
                if {[string length $name]} {
                    lappend cmd $app:$name
                } else {
                    lappend cmd $app:*
                }
            }
        }

        # Issue the "find" command to the account server.
        set qid [eval $cmd]
        Oc_EventHandler New _ $acct Reply$qid \
            [list $this FindReply $acct $seeking] \
            -groups [list $this $acct] -oneshot 1
    }

    method FindReply {acct seeking} {
        set reply [$acct Get]
        if {[lindex $reply 0]} {
            set msg "Error in find reply: [join [lrange $reply 1 end]]"
            error $msg $msg
        }

        set answers [lindex $reply 1]
        foreach tag $seeking {
            set candidates [lindex $answers 0]
            set answers [lrange $answers 1 end]
            foreach {app n new} $dest($tag) break

            foreach oid $candidates {
                if {![catch {$this Assign $tag $oid}]} {break}
            }
            if {![info exists assign($tag)]} {
                # None of them worked.  Launch a new one instead.
                # If the name was specific, we should claim the name.
                # If not, then there's just no app of the right kind
                # running and we should launch one.
                if {[string length $n]} {
                    $this NameClaim $acct $app $tag $n
                } else {
                    $this LaunchNewDestination $acct $app $tag
                }
            }
        }
    }

    method NameClaim {acct app tag name} {
        set qid [$acct Send claim $app:$name]
        Oc_EventHandler New _ $acct Reply$qid \
            [list $this ClaimReply $acct $app $tag $name] \
            -groups [list $this $acct] -oneshot 1
    }

    method ClaimReply {acct app tag name} {
        set reply [$acct Get]
       if {[lindex $reply 0]==0} {
          # We claimed the name; Launch the program;
          $this LaunchNewDestination $acct $app $tag 1
       } elseif {[lindex $reply 0]==1} {
          # We lost the race;  someone else got the name.
          # Set up to receive the notice when the name is associated
          Oc_EventHandler New _ $acct Readable \
             [list $this NotifyClaim $acct $tag $name] \
             -groups [list NotifyClaim-$name]
       } else {
          # Some other error condition
          set msg "Error in claim reply: [join [lrange $reply 1 end]]"
          error $msg $msg
       }
    }

    method NotifyClaim {acct tag n} {
        set reply [$acct Get]
        if {![string match notify [lindex $reply 0]]} {
            return
        }
        if {![string match claim [lindex $reply 1]]} {
            return
        }
        set name [lindex $reply 2]
        if {[string compare $name $n]} {
            return
        }
        Oc_EventHandler DeleteGroup NotifyClaim-$name
        $this FindDestinations $acct [list $tag]
        return -code return
    }

    method Missing { name args } {
        # Used for aliases missing from safe interpreters
        return -code error \
                "Command \"$name\" is not available in safe interpreter.\
                Safe level determined by option value \"MIFinterp safety\"\
                in file oommf/config/options.tcl."
    }

    method RandomSeed { args } {
       # Stores random seed.  Uses InitRandom to feed seed to both Tcl
       # interpreters and C-level random number generators. If called
       # with no args or with an empty string then a seed is generated
       # from the system clock.
       if {[llength $args] > 1} {
          return -code error \
             "wrong # args: should be \"$this RandomSeed ?arg?\""
       }
       if {[llength $args]==0 || [string match {} [lindex $args 0]]} {
          if {$regression_test_level!=1} {
             Oc_Srand   ;# Use clock-based seed
             set random_seed [expr {round([Oc_UnifRand]*((1<<31)-1))}]
             ;## random_seed is an integer determined by clock-based seed
          }
          ## If regression_test_level == 1 ("load" test drawn from
          ## oxs/examples/) then clock-based seeding is disabled, and
          ## the default seed set in the Oxs_Mif constructor is
          ## retained.
       } else {
          set random_seed [lindex $args 0]
       }
       # Issue: We want to have an as-specified seed value for use
       # during the processing of the MIF file, and creation of
       # the Oxs_Ext objects, and yet have a reproducible seed
       # for use at the ProbReset (i.e., Oxs_Ext::Init) point.
       # So, we call InitRandom here with random_seed as specified,
       # and then increment random_seed for future use.
       $this InitRandom
       incr random_seed
    }

    method InitRandom {} {
       # Initializes random number generators in both Tcl interpreters
       # and at C-level.
       if {![string is integer -strict $random_seed]} {
          return -code error \
             "Oc_Class Oxs_Mif method InitRandom:\
              Invalid random_seed; \"$random_seed\" is not a 32-bit integer"
       }
       Oc_Srand $random_seed ;# Set C-level random number generator

       # There is a bug in srand Obj handling prior to Tcl 8.4.  If the
       # value of random_seed is a literal value, and if that *literal
       # value* (as opposed to random_seed itself) has been assigned a
       # double representation, then srand may pull out the double
       # representation instead of the integer one.  This yields the
       # error message
       #
       #     can't use floating-point value as argument to srand
       #
       # This error can be generated, for example, by having a Tcl
       # script in the input MIF file return the list "[list 0 1 2]"
       # for interpretation as a list of doubles by
       # Oxs_ScriptVectorField.  If any of the literals 0, 1, or 2 are
       # used as literal imports to RandomSeed in the MIF file, then
       # this bug may be exercised.
       #
       # Work around by appending an empty string to cause
       # re-interpretation.  We've only seen this problem in the child
       # interpreter, but there doesn't seem any reason to not protect
       # the srand call in the master too.
       expr {srand([concat $random_seed {}])} ;# RNG in master interp
       interp eval $mif_interp "expr {srand(\[concat $random_seed {}])}"
       ## RNG in slave
    }

    method GetSpecKeys {} {
        return $speckeys
    }

    method GetSpecValue { name } {
       # "name" here is a key into the spec array.  If this method is
       # ever exposed to the user, we may want to check for a ":" and
       # append one if necessary (e.g., see SetSpecValue method below).
       # At present (2008-04) this method is only accessed by one branch
       # in the ReadMif method below, and in the Oxs_Director::ProbInit
       # function in director.cc
       return $spec($name)
    }

    method NormalizeSpecKey { key } {
       if {[string first ":" $key]<0} {
          # Append default instance name (empty string)
          append key ":"
       }
       return $key
    }

    method SetSpecValue { key args } {
        set ac [llength $args]
        if {$ac != 1} {
            return -code error \
                    "Bad argument count ($ac) in Specify block (KEY $key)"
        }
        set key [$this NormalizeSpecKey $key]
        if {[info exists spec($key)]} {
            return -code error "Key $key already in use"
        }
        lappend speckeys $key
        set spec($key) [lindex $args 0]
    }

    method UnsetSpec { key } {
        # If $key is {}, then clears out all specs.  This does not
        # raise an error even if there are no specs.
        #   If $key is not {}, then removes only that spec, from
        # both the spec array and the speckeys list.  This operation
        # will raise an error if the spec $key is not set.
        #   NB: This method is *not* the inverse of CreateExtObject,
        # because it does not affect the registration or existence
        # of any Oxs_Ext objects in the director.
        if {[string match {} $key]} {
           if {[info exists spec]} {unset spec}
            set speckeys {}
        } else {
            set key [$this NormalizeSpecKey $key]
            unset spec($key)
            set keyindex [lsearch -exact $speckeys $key]
            if {$keyindex>=0} {
                set speckeys [lreplace $speckeys $keyindex $keyindex]
            }
        }
    }
    # It might also be useful to have a 'ReplaceSpec' method
    # that does an Unset + Set without changing key order.

    method Clear {} {
        $this UnsetSpec {}
    }

   method CreateExtObject { spec_key spec_init_str } {
      # Stores the key and init string in this' speckeys list and spec
      # array, and, for MIF version 2.2 files, creates a new Oxs_Ext
      # object.  The new Oxs_Ext object is stored and registered with
      # the director.  For MIF version 2.1 files, Oxs_Ext object
      # creation is delayed until after all Specify blocks are parsed.
      # see NOTE below.
      #   NB: The Clear and UnsetSpec methods can be used to
      # remove a spec from this mif object, but the Oxs_Ext
      # object still exists and is registered with the director.
      # At present (Sept 2008) there is no support for removing
      # individual Oxs_Ext from the director.

      set spec_key [$this NormalizeSpecKey $spec_key]

      set version_2_1 0
      if {$mif_major_version==2 && $mif_minor_version==1} {
         set version_2_1 1
      }

      # NOTE: In MIF version 2.2, the MIF Specify command stores the
      # Specify keys and init strings, and calls the
      # Oxs_ExtCreateAndRegister command to create and register the
      # Oxs_Ext object with the director.  This means that the Oxs_Ext
      # object is itself available beyond that point in the MIF file
      # (through, for example, Oxs_EvalScalarField and
      # Oxs_EvalVectorField commands).  In contrast, in MIF version 2.1,
      # the Specify command only stores the Specify keys and init
      # strings, but does not call Oxs_ExtCreateAndRegister.  This means
      # that the contents of the init strings are not processed until
      # after the whole MIF file has been sourced.  This allows more
      # flexibility in the ordering of top-level objects (such as
      # support procs) in the MIF file, but on the other hand means that
      # Oxs_ExtScalar/VectorFields cannot be accessed outside of Specify
      # blocks.
      #    BTW, because of the way v2.2 files are processed, the MIF
      # SetOptions command can be called multiple times inside a MIF 2.2
      # file, and each Specify block will read whatever options were
      # last set before it.

      if {$version_2_1} {
         # MIF version 2.1
         if {[regexp {^Oxs_[^:]*Driver} $spec_key]} {
            # Tweak for 2.1 format: move various options out of driver
            # Spec block and into SetOptions array.
            set driverkey $spec_key
            set initstr $spec_init_str
            foreach label [list basename \
                              vector_field_output_format \
                              vector_field_output_meshtype \
                              vector_field_output_writeheaders \
                              scalar_field_output_format \
                              scalar_field_output_meshtype \
                              scalar_field_output_writeheaders \
                              scalar_output_format \
                              scalar_output_writeheaders] {
               # Find matches to $label in initstr.  Any match
               # which has an odd index is a value rather than
               # a label (not likely, but just in case).  Copy
               # all others into option array, and remove from
               # initstr.
               set striplist {}
               set optlist {}

               # For Tcl 8.4 and later, the following stanza
               # can be replaced with
               #  set matchlist [lsearch -exact -all $initstr $label]
               set matchlist {}
               set tmplist $initstr
               while {[set i [lsearch -exact $tmplist $label]]>=0} {
                  lappend matchlist $i
                  set tmplist [lrange $tmplist [incr i] end]
               }

               foreach i $matchlist {
                  if {$i%2 == 0} {
                     set striplist [linsert $striplist 0 $i]
                     ## Build index list in reverse order
                     lappend optlist [lindex $initstr $i]
                     lappend optlist [lindex $initstr [expr {$i+1}]]
                  }
               }
               if {[llength $optlist]>0} {
                  $this SetOptions $optlist
                  foreach i $striplist {
                     set initstr [lreplace $initstr $i [expr {$i+1}]]
                  }
               }
            }
            set spec_init_str $initstr
         }
         $this SetSpecValue $spec_key $spec_init_str
         # For this MIF format, calls to Oxs_ExtCreateAndRegister
         # occur at the end of the ReadMif method.
      } else {
         # Version 2.2 or later
         $this SetSpecValue $spec_key $spec_init_str
         Oxs_ExtCreateAndRegister $spec_key $spec_init_str
      }
   }

    method SetParameter { param_name args } {
        if {[$mif_interp eval [list info exists $param_name]]} {
            return -code error "Parameter \"$param_name\" already set"
        }
        if {[info exists tmp_params($param_name)]} {
            # Set parameter using tmp_params common value.
            set val $tmp_params($param_name)
            $mif_interp eval [list set $param_name $val]
            unset tmp_params($param_name)
        } else {
            # Parameter not mentioned in tmp_params; use default.
            if {[llength $args]<1} {
                return -code error "Parameter \"$param_name\" not set\
                        on command line, and no default value specified."
            } elseif {[llength $args]==1} {
                $mif_interp eval [list set $param_name [lindex $args 0]]
            } else {
                return -code error "wrong # args: should be\
                        \"Parameter [list $param_name] ?default_value?\""
            }
        }
    }

    method GetFilename { args } {
       # Returns filename used during most recent ReadMif
       # operation.
       if {[llength $args]>0} {
          return -code error "wrong # args: should be\
                        \"GetMifFilename\""
       }
       return $mif_filename
    }

    method GetParameters { args } {
       # Returns parameter string used during most recent ReadMif
       # operation.
       if {[llength $args]>0} {
          return -code error "wrong # args: should be\
                        \"GetMifParameters\""
       }
       return $parameters
    }

    method CheckParameters { checkparam_list } {
        # Returns 1 if checkparam_list agrees with the parameters
        # as stored in the instance variable parameters, or 0
        # if there are any differences.
        if {[llength $checkparam_list] != [llength $parameters]} {
            return 0
        }
        array set check_params $checkparam_list
        array set tmp_params $parameters
        set tmp_names [array names tmp_params]
        foreach name $tmp_names {
            if {![info exists check_params($name)]} {
                # Name in $parameters missing from $check_params
                return 0
            }
            if {[string compare $check_params($name) $tmp_params($name)] \
                    != 0 } {
                # Values disagree
                return 0
            }
        }
        # If we get here, then all name-value pairs in $parameters
        # appear in $checkparam_list, with the same values.  Since
        # we already checked that both lists have the same number of
        # elements, we know the converse holds too.
        return 1
    }

    method GetCrc {} {
        # Returns CRC of file read during most recent ReadMif
        # operation.  The CRC is computed on the buffer filled
        # via the Tcl "read" command, i.e., after newline
        # translations (if any).
        return $mif_crc
    }

    method GetFileContents {} {
       return $mif_file_contents
    }

    proc GetStateData { args } {
       # Interface to Oxs_QueryState with wildcard support
       # Usage:
       #    GetStateData [-glob -exact -regexp] [-pairs] -- \
       #        state_id [pattern ...]
       # If no patterns are specified then the list of keys is
       # returned.

       # Scan args for option flags
       set match_type -glob
       set return_pairs 0
       for {set i 0} {$i<[llength $args]} {} {
          set elt [lindex $args $i]
          switch -exact $elt {
             -- {
                set args [lreplace $args $i $i]
                break ;# End option processing
             }
             -exact -
             -glob -
             -regexp {
                set match_type $elt
                set args [lreplace $args $i $i]
             }
             -pairs {
                set return_pairs 1
                set args [lreplace $args $i $i]
             }
             default {
                incr i
             }
          }
       }

       if {[llength $args]<1} {
          error "wrong # args: should be \"GetStateData\
           ?-glob -exact -regexp -pairs --? state_id ?pattern ...?\""
       }
       set patterns [lassign $args state_id]

       if {[llength $patterns]==0} {
          # Keys request
          return [Oxs_QueryState $state_id keys]
       }

       # If match request is -exact, then pass keys straight through
       # to Oxs_QueryState
       if {[string compare -exact $match_type]==0} {
          # If match request is -exact then no key matching needed
          return [Oxs_QueryState $state_id values {*}$patterns]
       }

       # Get state keys
       set keys [Oxs_QueryState $state_id keys]

       set match_keys {}
       set nomatch {}
       foreach elt $patterns {
          set matched [lsearch -all -inline $match_type $keys $elt]
          if {[llength $matched]==0} {
             lappend nomatch $elt
          } else {
             lappend match_keys {*}$matched
          }
       }
       if {[llength $nomatch]>0} {
          error "No match for key request(s): $nomatch"
       }
       set pairs [Oxs_QueryState $state_id values {*}$match_keys]
       if {$return_pairs} {
          return $pairs
       }
       set vals {}
       foreach {k v} $pairs {
          lappend vals $v
       }
       return $vals
    }

    method NotHere { cmdname args } {
       return -code error "MIF command \"$cmdname\" not available in\
                  MIF ${mif_major_version}.${mif_minor_version} format"
    }

    method ReadMif { filename params } {
        # Fill spec from filename, using mif_interp to source the file.

        set filename [Oc_DirectPathname $filename]  ;# Insure full path is used
        set mif_filename $filename
        set parameters $params

        # Re-initialize default basename, using mif_filename
        set options(basename) [file rootname [file tail $mif_filename]]

        # Empty out any previous specifications (should we do this?)
        $this Clear

        # Reset tmp_params array
        if {[info exists tmp_params]} {unset tmp_params}
        array set tmp_params $parameters

        # Read file into a string.  Hopefully this isn't too big.
        set chan [open $filename]
        fconfigure $chan -encoding utf-8
        set filestr [read $chan]
        close $chan

        # Check that file is in MIF format
        set ws "\[ \t\n\r\]"   ;# White space
        set major 0
        set minor 0
        if {![regexp -- "^#$ws*MIF$ws*(\[0-9\]+)\\.(\[0-9\]+)$ws*" \
                $filestr dummy major minor]} {
            return -code error \
                    "Input file \"$filename\" not in any MIF format"
        }
        if {$major!=2 || $minor<1} {
            # MIF 1.x or 2.0 format.  Try to convert with mifconvert.
            if {[catch {
                Oc_Application CommandLine mifconvert \
                        --format 2.1 --quiet - -
            } cmdLine]} {
                return -code error "Can't find mifconvert program to\
                        convert input file $filename to MIF 2.1 format."
            }
            set cmdLine [linsert $cmdLine 0 |]
            if {[string match unsafe $safety_level]} {
                lappend cmdLine "--unsafe"
            }
            lappend cmdLine "<<" $filestr
            set chan [open $cmdLine r]
            set filestr [read $chan]
            if {[catch {close $chan} errmsg]} {
                return -code error "Error reading input file $filename\
                        ---\n$errmsg"
            }

            # Safety check
            if {![regexp -- "^#$ws*MIF$ws*(\[0-9\]+)\\.(\[0-9\]+)$ws*" \
                    $filestr dummy major minor] \
                    || $major!=2 || $minor<1} {
                return -code error "Input file \"$filename\"\
                        not in or convertible to MIF 2.1 format"
            }
        }

        # Store effective file contents
        set mif_file_contents $filestr
        set mif_major_version $major
        set mif_minor_version $minor
        set version_2_1 0
        if {$mif_major_version==2 && $mif_minor_version==1} {
           set version_2_1 1
           # Remove MIF commands not in version 2.1
           interp alias $mif_interp SetOptions {} $this NotHere SetOptions
           interp alias $mif_interp GetOptions {} $this NotHere GetOptions
           interp alias $mif_interp EvalScalarField {} \
              $this NotHere EvalScalarField
           interp alias $mif_interp EvalVectorField {} \
              $this NotHere EvalVectorField
           interp alias $mif_interp GetAtlasRegions {} \
              $this NotHere GetAtlasRegions
           interp alias $mif_interp GetAtlasRegionByPosition {} \
              $this NotHere GetAtlasRegionByPosition
           interp alias $mif_interp GetMifFilename {} \
              $this NotHere GetMifFilename
           interp alias $mif_interp GetMifParameters {} \
              $this NotHere GetMifParameters
        }

        # Compute the CRC
        set mif_crc [Nb_ComputeCRCBuffer filestr]

        # Source filestr
        set mif_interp_nodelete 1
        set mif_interp_local_copy $mif_interp
        $mif_interp_local_copy eval {proc {} script {
            global errorInfo errorCode
            rename {} {}
            set code [catch {uplevel 1 $script} msg]
            if {$code == 2} {
                return -code error {"return" from outside of proc}
            }
            if {$code == 1} {
		set key {    ("uplevel" body line }
		set last [string last $key $errorInfo]
		set fixed [string range $errorInfo 0 $last]
		incr last [string length $key]
		set scanme [string range $errorInfo $last end]
		scan $scanme %d line
		append fixed [subst {   (file "%FILE%" line $line)}]
                return -code $code -errorinfo $fixed \
                        -errorcode $errorCode $msg
            } else {
                return -code $code $msg
            }
        }}
        set errcode [catch [list $mif_interp_local_copy \
                               eval [list {} $filestr]] errmsg]
        if {[info exists mif_interp_nodelete]} {
           set mif_interp_nodelete 0
        } else {
           # Presumable "this" has been deleted.
           catch {interp delete $mif_interp_local_copy}
        }

        if {$errcode} {
            # Error occurred sourcing $filename
            global errorInfo errorCode
            foreach {ei ec} [list $errorInfo $errorCode] {break}
            catch {$this Clear}  ;# Clear out partial problem spec
	    set ei [string map [list %FILE% $filename] $ei]
            return -code error -errorinfo $ei -errorcode $ec $errmsg
        }

        if {[llength [array names tmp_params]]>0} {
            $this Clear
            return -code error "Unused parameters: [array names tmp_params]"
        }

        if {$version_2_1} {
         # MIF version 2.1.  In this version, the MIF Specify command
         # only stores the Spec keys.  Oxs_ExtCreateAndRegister must be
         # run separately on each key after the entire MIF file has been
         # sourced.  Cf. notes in the CreateExtObject class.
         foreach key [$this GetSpecKeys] {
            set init_str [$this GetSpecValue $key]
            Oxs_ExtCreateAndRegister $key $init_str
         }
      }

      if {$regression_test_level>0} {
         $this RegressionTestSetup
      }
    }

    method SetOptions { args } {
       # For convenience, SetOptions can either be called with
       # an even number of separate arguments, or they can all
       # be wrapped up into a single list (with an even number
       # of arguments).
       set ac [llength $args]
       if {$ac == 1} {
          # One list
          set initstr [lindex $args 0]
       } else {
          # Separate args
          set initstr $args
       }
       if {[llength $initstr]%2!=0} {
          return -code error "SetOptions arglist should have an\
               even number of elements\
               ([llength $initstr] elements received)."
       }
       foreach {label value} $initstr {
          set options($label) $value
       }
    }

    method GetOption { label } {
        if {![info exists options($label)]} {
            return -code error "Option \"$label\" not set."
        }
        return $options($label)
    }

    method GetAllOptions { args } {
       if {[llength $args]>0} {
          return -code error "wrong # args: should be\
                        \"GetOptions\""
       }
       return [array get options]
    }

    method Dump {} {
        # Return specify block data.
        # NB: There may be additional data stored away
        #  in $mif_interp, for example, proc definitions.
        set specstr ""
        foreach k $speckeys {
            append specstr "[list Specify $k $spec($k)]\n"
        }
        return $specstr
    }

    # To set up a regression test run, call the SetRegressionTestLevel
    # proc to initialize the testing level and base output names before
    # creating the Oxs_Mif object. This will adjust random number
    # generation and run parameters in accordance with the specified
    # level. After the Oxs_Ext objects are created the
    # Oxs_DriverLoadTestSetup command can be called by the controlling
    # script (e.g., boxsi.tcl) to modify driver behavior.
    proc SetRegressionTestLevel { level testname } {
       set regression_test_level $level
       set regression_test_basename $testname
    }

    private method RegressionTestSetup {} {
       # This is not a standard run, but rather part of a regression
       # test.  If level == 1, then make the following changes to the
       # problem specification:
       #
       #   1) Clear any mif-specified output destination and schedules
       #   2) Make an archive destination
       #   3) Introduce a per-step DataTable output.
       #   4) Initialize random number stream to a fixed value.
       #
       # The last won't work as intended if the random number stream is
       # used during the parsing of the MIF file.  The only workaround
       # would appear to be to initialize RandomSeed and then disable
       # the interface before the MIF file is loaded.
       #
       # For all non-zero level, then run basename (as used for
       #  mmArchive output) is set to
       #
       #      "$regressiontest_basename"
       #
       # IMPORTANT NOTE: This routine only affects variables local to
       # Oxs_Mif, and so must be called *BEFORE* SetupInitialSchedule.
       #
       if {$regression_test_level < 1 } { return }  ;# Not a regression test
       if {$regression_test_level < 2} {
          catch {unset schedule}
          set destkeys {}
          catch {unset dest}
          catch {unset extra}
          $this Destination archive mmArchive
          $this Schedule DataTable archive Step 1
       }
       $this SetOptions [list basename $regression_test_basename]
    }

    Destructor {
       Oc_EventHandler Generate $this Delete
       Oc_EventHandler DeleteGroup $this
       if {!$mif_interp_nodelete} {
          catch {interp delete $mif_interp}
       }
    }
}
