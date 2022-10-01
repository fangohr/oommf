# FILE: mmlaunch.tcl
#
# An application which presents an interface for launching and
# interconnecting other OOMMF applications.
#
# Note: This application supports a single account, which is
#       the effective uid. In principle multiple accounts could
#       be managed by embedding the acct name into the runapps
#       dict keys or by having separate runapps dicts for each
#       acccount. But there does not appear to be a strong use
#       case for this feature.

#############################################################
#
# ***** Global variables *****
#
#  dict runapps:
#    Each entry has up to seven fields:
#
#      oid : acct appname sid port protocol thread
#
#    The oid is stored as the dict key, and the remaining fields form
#    the dict value. If the oid is not associated with a server, then
#    only the acct and appname fields are present.  Note: sid has the
#    form oid:servernumber, e.g., 5:0
#
#  dict runwidgets:
#    Created in parallel with dict app, appwidget holds the up to three
#    Tk windows associated with each oid, the app name label, the sid
#    label, and the GUI checkbutton, if any. The oid is the dict key:
#
#      oid : label_appname label_sid checkbutton_GUI
#
#    The UpdateRunAppDisplay app is called to control the layout of
#    these buttons in the $appbox.frame widget embedded inside the
#    appbox canvas.
#
# ***** Primary Tk widgets *****
#
#    Variable :          Path         :  Type       : Notes
#  brace      : .brace                : canvas      : Min outer window width
#  outerframe : .of                   : frame       : Client area wrapper
#    acctlabel  : .of.acct            : label       : user@host
#
#    progframe  : .of.progframe       : frame       : Frame launch program btns
#      proglabel  : $progframe.lbl    : label       : "Programs"
#      <btn>      : $progframe.w$pgm  : button      : Launch $pgm
#
#    appframe   : .of.appframe        : frame       : Frame running apps
#      applabel   : $appframe.lbl     : label       : "Running Applications"
#      appbox     : $appframe.box     : canvas      : Scrollable container
#        apps       : $appbox.frame   : frame       : Embedded in $appbox
#          <lblname>  : $apps.ln$oid  : label       : App name for $oid
#          <lbloid>   : $apps.lo$oid  : label       : "<$oid>"
#          <chkbox>   : $apps.cb$oid  : checkbutton : GUI toggle (if any)
#      appscroll  : $appframe.yscroll : scrollbar   : Scrolls $appbox
#
#
#
# ***** Program flow for running application Tk widgets *****
#
# When mmLaunch starts it connects to the default account server and
# sends 'trackoids' and 'services' requests. The trackoids reply
# contains a list of all currently assigned OIDs, in increasing order
# by OID. In addition, the account server will also send 'newoid'
# and 'deleteoid' messages to mmLaunch going forward whenever a new
# application requests an OID or exits.
#
# In similar fashion the 'services' reply is a list of all OOMMF apps
# with a server socket offering GUI support, along with updates on all
# future 'newservice' and 'deleteservice' events.
#
# When each newoid event is processed, whether from the 'trackoids'
# response or a later 'newoid' message, the 'runapps' dict is checked
# and if the oid is not listed then a runapps entry is created for the
# oid storing the appname associated with the oid. The oid is also
# placed onto the global widgetadd list. Likewise, 'deleteoid'
# messages result in the oid being removed from runapps and added to
# the widgetdel list.
#
# When each newservice event is processed, whether from the 'services'
# response or a later 'newservice' message, the runapps dict is
# checked for the oid. Usually it will already be present, in which
# case the existing entry is augmented with the sid, port, and
# protocol information. If the oid is also present in the 'runwidgets'
# dict, then UpdateGuiButtonDisplay is called to add a checkbutton to
# the application display line. If oid is present in runapps but not
# runwidgets, then it should be already on the widgetadd list, in
# which case the GUI checkbutton will be added when the running apps
# frame is updated.
#
# Due to the FIFO nature of socket handling, it is probably not
# possible for a newservice event to occur before the corresponding
# newoid event. But if necessary the newservice code can create
# a full runapps entry and place the oid on the widgetadd list.
#
# Connections to GUI interface sockets are not established until
# the user selects the corresponding checkbutton for the first
# time. When that occurs the SetupGuiToggle proc is called to
# create a Net_Thread connection to the application GUI socket,
# and the interface is brought up.
#
# Events deleteoid and deleteservice remove the oid from runapps
# and place the oid on the widgetdel list. The deleteservice event
# additional shuts down the connection to the GUI interface socket
# if it has been established.
#
# Additions of an oid to the widgetadd or widgetdel lists are tied
# to addition or deletion of the oid from the runapps dict. This way
# an oid is added to widgetadd and widgetdel at most once.
#
# ******************************

# Support libraries
if {[catch {package require Tk 8}]} {
    package require Tk 4.1
}
package require Oc 2
package require Net
package require Ow
wm withdraw .

Oc_Main SetAppName mmLaunch
Oc_Main SetVersion 3.0b0
regexp \\\044Date:(.*)\\\044 {$Date: 2015/03/25 16:43:38 $} _ date
Oc_Main SetDate [string trim $date]
Oc_Main SetAuthor [Oc_Person Lookup dgp]
Oc_Main SetHelpURL [Oc_Url FromFilename [file join [file dirname \
        [file dirname [file dirname [Oc_DirectPathname [info \
        script]]]]] doc userguide userguide \
	OOMMF_Launcher_Control_Inte.html]]

Oc_CommandLine ActivateOptionSet Net

set bufferinfo [dict create mintime 150 maxtime 1000 requestcnt 0]
Oc_CommandLine Option buffertime {
   {timeout {expr {[regexp {^[0-9]+$} $timeout] && $timeout>=0}}}
} {
   dict set ::bufferinfo mintime $timeout
} [subst {Buffer timeout in ms\
             (default is [dict get $::bufferinfo mintime])}]

Oc_CommandLine Option buffertimemax {
   {timeout {expr {[regexp {^[0-9]+$} $timeout] && $timeout>=0}}}
} {
   dict set ::bufferinfo maxtime $timeout
} [subst {Max concatenated buffer timeout in ms\
             (default is [dict get $::bufferinfo maxtime])}]

Oc_CommandLine Parse $argv

# Top-level widget layout and control
set title [Oc_Main GetInstanceName]
set bracewidth [Ow_SetWindowTitle . $title]
wm iconname . $title
Ow_SetIcon .
wm deiconify .

set menubar .mb
foreach {fmenu hmenu} [Ow_MakeMenubar . $menubar File Help] {}
$fmenu add command -label "Exit All OOMMF" -command {ConfirmExitAll}
$fmenu add command -label "Exit" -underline 1 -command exit
proc Die { win } {
    if {![string match . $win]} {return}
    exit
}
bind $menubar <Destroy> {+Die %W}
wm protocol . WM_DELETE_WINDOW { exit }
Oc_EventHandler New _ Oc_Main Exit {proc Die win {}}
Ow_StdHelpMenu $hmenu

set menuwidth [Ow_GuessMenuWidth $menubar]
if {$menuwidth>$bracewidth} {
   set bracewidth $menuwidth
}
set brace [canvas .brace -width $bracewidth -height 0 -borderwidth 0 \
        -highlightthickness 0]
pack $brace -side top
# Resize root window when OID is assigned:
Oc_EventHandler New _ Net_Account NewTitle [subst -nocommands {
  $brace configure -width \
    [expr {%winwidth<$menuwidth ? $menuwidth : %winwidth}]
}]

set outerframe [frame .of -borderwidth 4 -relief ridge]
pack .of -expand 1 -fill both

proc AccountLabel { {acct {}} } {
   if {![regexp {^[^.]+} [info hostname] host]} {
      set host localhost
   }
   if {[string match {} $acct]} {
      if {[info exists ::tcl_platform(user)]} {
         set user $::tcl_platform(user)
      } else {
         set user "oommf"
      }
   } else {
      set user [$acct Cget -accountname]
   }
   return "${user}@${host}"
}

set acctlabel [label .of.acct -text [AccountLabel] \
                  -borderwidth 3 -relief ridge]
set progframe [frame .of.progframe -padx 2]
set appframe [frame .of.appframe -padx 2]

grid $acctlabel - - -sticky ew
grid $progframe x -sticky new
grid $appframe  -sticky news -row 1 -column 2
grid rowconfigure $outerframe 0 -weight 0
grid rowconfigure $outerframe 1 -weight 1
grid columnconfigure $outerframe 0 -weight 2
grid columnconfigure $outerframe 1 -weight 1
grid columnconfigure $outerframe 2 -weight 10

set labelopts [list -borderwidth 0 -relief flat -pady 2]
set proglabel [label $progframe.lbl -text "Programs" {*}$labelopts]
pack $proglabel -side top -fill x

set applabel [label $appframe.lbl -text "Running Applications" \
                 {*}$labelopts]
set appbox [canvas $appframe.box -cursor {} -width 0 -height 0 \
               -borderwidth 0 -highlightthickness 0 \
               -yscrollcommand [list $appframe.yscroll set]]
set appscroll [scrollbar $appframe.yscroll -orient vertical \
               -command [list $appframe.box yview]]

grid $applabel x -sticky ew
grid $appbox $appscroll -sticky ns
grid columnconfigure $appframe 0 -weight 1
grid columnconfigure $appframe 1 -weight 0
grid rowconfigure $appframe 0 -weight 0
grid rowconfigure $appframe 1 -weight 1

proc ScrollbarOnOff {} {
   # Display or hide scrollbar as needed
   global appscroll
   update idletasks
   lassign [$appscroll get] f0 f1
   if {$f1-$f0 >= 1.0} {
      grid remove $appscroll
   } else {
      grid configure $appscroll
   }
}

bind $appframe <Configure> ScrollbarOnOff

if {[string compare Darwin $tcl_platform(os)]==0} {
   # This binding links the canvas and scrollbar on the Mac
   bind . <MouseWheel> [subst -nocommands {$appscroll set {*}[$appbox yview]}]
}

proc KillAll { acct } {
   $acct Send kill *
}

proc ExitAll {} {
   Oc_Main BlockShutdown
   foreach acct [Net_Account Instances] {
      $acct IgnoreDie  ;# Ignore self-die requests from account server
      if {[$acct Ready]} {
         KillAll $acct
      }
   }
   Oc_EventHandler New _ Net_Account Ready [list KillAll %object]

   # Keep event loop running while running applications shut down.
   set ::runapps_count [dict size $::runapps]
   if {$::runapps_count>0} {
      trace add variable ::runapps write \
         { set ::runapps_count [dict size $::runapps] ;# }
      set shutdown_timeout 30000 ;# Timeout in ms (safety)
      set shutdown_timeout_id [after $shutdown_timeout {
         set ::runapps_count 0
      }]
      while {$::runapps_count>0} {
         vwait ::runapps_count
         after cancel $shutdown_timeout_id
         set shutdown_timeout_id [after $shutdown_timeout {
            set ::runapps_count 0
         }]
      }
      after cancel $shutdown_timeout_id
      trace remove variable ::runapps write \
         { set ::runapps_count [dict size $::runapps] ;# }
   }
   Oc_Main AllowShutdown
   exit
}

proc ConfirmExitAll {} {
    set response [Ow_Dialog 1 "Exit all OOMMF processes?" \
	    warning "Are you *sure* you want to exit all running OOMMF\
	    programs (including all solvers)?" {} 1 Yes No]
    if {$response == 0} {
       ExitAll
    }
}

#############################################################
# Program launch buttons
set scriptdir [file dirname [info script]]
set launchFiles {}
if {[file readable [file join $scriptdir mmLaunchPrograms.tcl]]} {
   lappend launchFiles [file join $scriptdir mmLaunchPrograms.tcl]
}
if {[file readable [file join $scriptdir .mmLaunchPrograms.tcl]]} {
   lappend launchFiles [file join $scriptdir .mmLaunchPrograms.tcl]
}
foreach file $launchFiles {
   Oc_Log Log "Sourcing $file ..." status
   if {[catch {source $file} msg]} {
      Oc_Log Log "Error sourcing $file: $msg" warning
   }
}
if {![info exists mmLaunchPrograms]} {
   error "mmLaunchPrograms not set in [list $launchFiles]"
}
foreach {label cmd} $mmLaunchPrograms {
   lappend launchpgm $label
   set launchcmd($label) $cmd
}
foreach pgm $launchpgm {
   set btn [button $progframe.w$pgm -text $pgm]
   $btn configure -command [list LaunchProgram $pgm]
   lappend myProgramButtons $btn
   pack $btn -side top -fill x
}
proc LaunchProgram { pgm } {
   global launchcmd
   if {![info exists launchcmd($pgm)]} {
      error "Can't launch $program: no such program"
   }
   set code [catch {eval $launchcmd($pgm) &} threadid]
   if $code {
      error "Launch failed: $threadid"
   }
}


#############################################################
# Running app control
#
# Each app entry has up to six fields:
#
#   oid appname sid port protocol thread
#
# The oid is stored as the dict key, and the remaining
# fields the dict value.
# Note: sid has the form oid:servernumber, e.g., 5:0

# Track OIDs and also servers that provide a GUI
set gui_protocols {
   {OOMMF GeneralInterface protocol}
   {OOMMF solver protocol}
   {OOMMF batchsolver protocol}
}

set ::runapps [dict create] ;# Info on currently running applications
set ::runwidgets [dict create] ;# Running apps widgets

proc ResetRunApps {} {
   set keys [lsort -integer [dict keys $::runapps]]
   if {[llength $keys]==0} { return }
   set acct [lindex [dict get $::runapps [lindex $keys 0]] 0]
   dict for {oid data} $::runapps {
      if {[llength $data]>2} {
         # App has a GUI checkbutton
         set thread [lindex $data 5]
         if {![string match {} $thread]} {
            if {[catch {
               $thread Send bye ;# All threads created with -enable_cache 1
               $thread SafeClose
            } msg]} {
               # This branch probably means service crashed
               Oc_Log Log \
                  "mmLaunch ResetRunApps non-clean thread close: $msg" info
            }
         }
      }
   }
   set ::runapps [dict create]
   set ::widgetadd {}
   set ::widgetdel $keys
   UpdateRunAppDisplay $acct 0 1  ;# Delete all runwidgets
   if {[llength $::runwidgets]!=0} {
      error "Consistency error: runwidgets not cleared on reset"
   }
}

proc TrackAcct { acct } {
   # Primarily this routine is used at mmLaunch start to initialize
   # the "Running Applications" pane and to configure updates for
   # future application starts and deaths. However, if the account
   # server dies then this routine will also be called when a new
   # account server is automatically started. In this latter case it
   # is important that all preexisting runapps and runwidgets be
   # closed, and the Running Applications pane be cleared before
   # re-initializing runapps and runwidgets because
   # UpdateRunAppDisplay assumes widget add requests precede widget
   # delete requests, i.e., if an oid is simultaneously on both
   # widgetadd and widgetdel lists then it will be first added and
   # then deleted (in other words, it won't be added). Also of note in
   # the account server restart scenario is that new OID messages may
   # occur in arbitrary order, as preexisting applications initiate
   # connections with the new account server.

   $::acctlabel configure -text [AccountLabel $acct]

   # In case of account server reset, clear runapps and runwidgets
   ResetRunApps

   # Update "Running Applications" display
   set qid [$acct Send trackoids]
   Oc_EventHandler New _ $acct Reply$qid [list GetOidReply $acct $qid] \
      -groups [list $acct GetOid-$qid]
   Oc_EventHandler New _ $acct Timeout$qid \
      [list GetOidTimeout $acct $qid] \
      -groups [list $acct GetOid-$qid]

   global gui_protocols
   set qid [$acct Send services $gui_protocols]
   Oc_EventHandler New _ $acct Reply$qid [list GetServiceReply $acct $qid] \
      -groups [list $acct GetService-$qid]
   Oc_EventHandler New _ $acct Timeout$qid \
      [list GetServiceTimeout $acct $qid] \
      -groups [list $acct GetService-$qid]

   # Handle future "Tell" messages from account server
   Oc_EventHandler DeleteGroup GetOid-$acct
   Oc_EventHandler DeleteGroup GetService-$acct
   Oc_EventHandler New _ $acct Readable [list HandleAccountTell $acct] \
      -groups [list $acct GetOid-$acct GetService-$acct]
}

proc GetOidReply { acct qid } {
   Oc_EventHandler DeleteGroup GetOid-$qid

   # Process trackoids reply
   set myoid [Oc_Main GetOid]
   set oidreply [$acct Get]
   Oc_Log Log "Received OID list: $oidreply" status
   if {[lindex $oidreply 0]==0} {
      foreach pair [lrange $oidreply 1 end] {
         lassign $pair oid appname
         if {$oid != $myoid && ![dict exists $::runapps $oid]} {
            # The exists check protects against overwriting
            # extended GUI server information.
            dict set ::runapps $oid [list $acct $appname]
            lappend ::widgetadd $oid ;# Code that adds oid to runapps is
            ## solely responsible for adding oid to widgetadd and calling
            ## UpdateRunAppDisplay.
         }
      }
   }
   UpdateRunAppDisplay $acct
}
proc GetOidTimeout { acct qid } {
    Oc_EventHandler DeleteGroup GetOid-$qid
    set msg "Timed out waiting for OID list"
    error $msg $msg
}
proc NewOid { acct oid appname } {
   if {$oid != [Oc_Main GetOid] && ![dict exists $::runapps $oid]} {
      # The second check protects against overwriting
      # extended GUI server information.
      dict set ::runapps $oid [list $acct $appname]
      lappend ::widgetadd $oid ;# Code that adds oid to runapps is
      ## solely responsible for adding oid to widgetadd and calling
      ## UpdateRunAppDisplay.
      UpdateRunAppDisplay $acct $oid
   }
}
proc DeleteOid { acct oid } {
   if {[dict exists $::runapps $oid]} {
      set data [dict get $::runapps $oid]
      if {[llength $data]>2} {
         DeleteService $acct [lindex $data 2]
      } else {
         dict unset ::runapps $oid
         lappend ::widgetdel $oid ;# Code that removes oid from runapps
         ## is solely responsible for adding oid to widgetdel and calling
         ## UpdateRunAppDisplay.
         UpdateRunAppDisplay $acct $oid
      }
   }
}

proc GetServiceReply { acct qid } {
   Oc_EventHandler DeleteGroup GetService-$qid
   # Process service reply
   set servicereply [$acct Get]
   Oc_Log Log "Received service list: $servicereply" status
   if {![lindex $servicereply 0]} {
      # Reply is a list of quartets: appname, sid, port, and protocol
      foreach quartet [lrange $servicereply 1 end] {
         # Most likely the oid labels associated with each service are
         # either already mapped or scheduled for mapping, in which
         # case the following NewService call won't call
         # UpdateRunAppDisplay. (So there is no need to batch runwidgets
         # updates as is done in GetOidReply.)
         NewService $acct $quartet
      }
   }
   UpdateRunAppDisplay $acct 0 1  ;# Force immediate display update
}
proc GetServiceTimeout { acct qid } {
    Oc_EventHandler DeleteGroup GetService-$qid
    set msg "Timed out waiting for service list"
    error $msg $msg
}

proc NewService { acct quartet } {
   lassign $quartet appname sid port protocol
   set oid [lindex [split $sid :] 0]
   if {$oid != [Oc_Main GetOid]} {
      set quintet [concat $acct $quartet]
      if {[dict exists $::runwidgets $oid]} {
         # OID labels exist and should be mapped
         dict set ::runapps $oid $quintet
         UpdateGuiButtonDisplay $acct $oid
      } else {
         # OID not mapped. We expect OID to be recorded in runapps,
         # but handle either way.
         if {[dict exists $::runapps $oid]} {
            dict set ::runapps $oid $quintet
            # If oid is in runapps but not runwidgets then oid should
            # already be on widgetadd.
         } else {
            Oc_Log Log "Runapps oid for new service $quintet not set" warning
            dict set ::runapps $oid $quintet
            lappend ::widgetadd $oid
            UpdateRunAppDisplay $acct $oid
            ## Code that adds oid to runapps is solely responsible for
            ## appending to widgetadd and calling UpdateRunAppDisplay.
         }
      }
   }
}

proc ServiceCrash { acct sid } {
   set oid [lindex [split $sid :] 0]
   if {[dict exists $::runapps $oid]} {
      set data [dict get $::runapps $oid]
      set appname [lindex $data 1]
      lassign [dict get $::runapps $oid] \
         acct appname sidx port protocol thread
      dict set ::runapps $oid [lrange $data 0 4] ;# Strip dead thread
      after idle [list Oc_Log Log "mmLaunch ServiceCrash:\
       Unclean shutdown of connection to $appname <$sid>" info]
      DeleteService $acct $sid
   }
}

proc DeleteService { acct sid {safeclose 0} } {
   # If safeclose is true then this DeleteService call has been
   # triggered by a Net_Thread SafeClose event.  In that case the
   # Net_Thread is in the process of shutting down already, so we
   # don't need to do that.
   set oid [lindex [split $sid :] 0]
   if {[dict exists $::runapps $oid]} {
      lassign [dict get $::runapps $oid] \
         acct appname sidx port protocol thread
      dict unset ::runapps $oid ;# Ensure that any subsequent
      ## DeleteService calls on this sid are ignored. This can
      ## happen, for example, when HandleAccountTell receives both
      ## DeleteService and DeleteOid messages on the same sid.
      if {!$safeclose && ![string match {} $thread]} {
         if {[catch {
            Oc_EventHandler DeleteGroup mmLaunchCrash-$thread
            $thread Send bye ;# All threads created with -enable_cache 1
         } msg]} {
            # This branch probably means service crashed
            set msg "mmLaunch DeleteService non-clean close\
               (Send bye) on connection to $appname <$sid>: $msg"
            Oc_Log Log $msg info
         } elseif {[catch {
            if {[llength [info commands $thread]]>0} {
               # Only run $thread SafeClose if $thread wasn't
               # deleted by previous command
               $thread SafeClose
            }
         } msg]} {
            # This branch probably means service crashed
            set msg "mmLaunch DeleteService non-clean close\
               on connection to $appname <$sid>: $msg"
            Oc_Log Log $msg info
         }
      }
      lappend ::widgetdel $oid ;# Code that removes oid from runapps
      ## is solely responsible for adding oid to widgetdel.
      UpdateRunAppDisplay $acct $oid
   }
}

proc SetupGuiToggle { acct oid chkbox } {
   if {![dict exists $::runapps $oid]} {
      return   ;# Service deleted
   }
   lassign [dict get $::runapps $oid] \
      xacct appname sid port protocol thread
   if {[string compare $xacct $acct]!=0} {
      error "Account mismatch for oid $oid? $xacct != $acct"
   }
   if {[string match {} $thread]} {
      # First call; create and setup thread
      Net_Thread New thread \
         -hostname [$acct Cget -hostname] \
         -accountname [$acct Cget -accountname] \
         -pid $sid -enable_cache 1 -protocol $protocol \
         -alias $appname -port $port
      set quintet [list $acct $appname $sid $port $protocol $thread]
      dict set ::runapps $oid $quintet
      Oc_EventHandler New _ $thread SafeClose \
         [list DeleteService $acct $sid 1] \
         -groups mmLaunchCrash-$thread -oneshot 1
      Oc_EventHandler New _ $thread Delete \
         [list ServiceCrash $acct $sid] \
         -groups mmLaunchCrash-$thread -oneshot 1
   }
   upvar #0 [$thread GlobalName btnvar] thdbtn
   ## Note: btnvar is an array element
   if {![info exists thdbtn]} {
      set thdbtn [set [$chkbox cget -variable]]
   }

   # Reconfigure chkbox so subsequent access calls $thread ToggleGui
   # directly.
   $chkbox configure \
      -variable [$thread GlobalName btnvar] \
      -command [list $thread ToggleGui]

   # Activate gui
   $thread ToggleGui
}

# Detect and handle new/delete OID and server messages from account
# server.
proc HandleAccountTell { acct } {
   set message [$acct Get]
   switch -exact -- [lindex $message 0] {
      newoid {
         # message: newoid %oid %appname
         NewOid $acct {*}[lrange $message 1 2]
      }
      deleteoid {
         # message: deleteoid %oid
         DeleteOid $acct [lindex $message 1]
      }
      newservice {
         # message: newservice %appname %sid %port %proto
         NewService $acct [lrange $message 1 end]
      }
      deleteservice {
         # message: deleteservice %sid
         DeleteService $acct [lindex $message 1]
      }
      default {
         Oc_Log Log "Bad message from account $acct:\n\t$message" status
      }
   }
}

# Frames are not scrollable, but canvases are. So we embed the
# frame $apps into the canvas $appbox; the scrollbar $appscroll
# is tied to $appbox.
set apps [frame $appbox.frame]
set appid [$appbox create window 5 0 -window $apps -anchor nw]

proc SearchIncreasingList { lst ikey } {
   # Returns the insertion point for integer ikey, where lst is a list
   # of integers sorted in increasing order. The return value matches
   # that of the lsearch -bisect command, i.e., the largest index with
   # value <= ikey. In particular, if ikey is smaller than the first
   # element of lst then -1 is returned.
   if {[catch {lsearch -integer -bisect $lst $ikey} ipt]} {
      # Search error; most likely this is a Tcl version prior to 8.6,
      # which doesn't support the -bisect option. Fall back to a simple
      # linear search.
      for {set ipt 0} {$ipt<[llength $lst]} {incr ipt} {
         if {$ikey < [lindex $lst $ipt]} { break }
      }
      incr ipt -1
   }
   return $ipt
}

proc AdjustAppDisplaySize { appbox appid } {
   update idletasks  ;# Set appid size
   lassign [$appbox bbox $appid] x1 y1 x2 y2
   set lmargin 5
   set rmargin 1
   set x1 [expr {$x1-$lmargin}]
   set x2 [expr {$x2+$rmargin}]
   $appbox configure -scrollregion [list $x1 $y1 $x2 $y2] \
      -width [expr {$x2-$x1}] -confine 1
}

proc CloseShop {} {
   # Clean up on exit
   global buffer_afterid
   if {[info exists buffer_afterid]} {
      after cancel $buffer_afterid
      unset buffer_afterid
   }
   foreach t [Net_Thread Instances] {
      if {[catch {
         $t Send bye ;# All threads are created with -enable_cache 1
      } msg]} {
         # This branch probably means service crashed
         Oc_Log Log \
            "mmLaunch CloseShop non-clean thread close: $msg" info
      }
   }
}

Oc_EventHandler New _ Oc_Main Exit CloseShop

# Options for spacing around labels and checkbuttons
# in the "Running Applications" pane.
if {[string compare windows $tcl_platform(platform)]==0} {
   set lblopts [list -padx 1 -pady 0 -borderwidth 1]
   set chkopts [list -padx 0 -pady 0 -borderwidth 0]
} else {
   set lblopts [list -padx 1 -pady 1 -borderwidth 1]
   set chkopts [list -padx 0 -pady 0 -borderwidth 1]
}

proc CreateGuiCheckbutton { acct oid } {
   # The variable set here is a placeholder used only if and until the
   # button is tied to a GUI thread.
   set ::appchk${oid} 0  ;# Protect against a stale value being
   ## inherited across an account server restart.
   return [checkbutton $::apps.cb$oid {*}$::chkopts \
              -variable ::appchk${oid} \
              -command [list SetupGuiToggle $acct $oid $::apps.cb$oid]]
}

set widgetadd {}
set widgetdel {}

proc UpdateRunAppDisplay { acct {startoid 0} {force_display 0}} {
   # Updates display for oids in the range startoid and larger, i.e.,
   # the display rows from startoid and below. If startoid is equal to
   # or less than the lowest oid (e.g., startoid == 0), then the full
   # runapp display is reset.
   #
   # On entry, ::runapps holds the information for all running
   # applications, with the oids as keys. Aside from checkbox tweaks
   # by UpdateGuiButtonDisplay, ::runwidgets is only modified by this
   # routine. On entry ::runwidgets holds the widgets currently on
   # display in the "Running Applications" frame of mmLaunch. The
   # widgetadd and widgetdel lists are oids that need to be added or
   # deleted from ::runwidgets to bring it up to date with ::runapps.
   #
   # widgetdel is processed after widgetadd, so if an oid is on both
   # lists it will be briefly added to runwidgets and then immediately
   # removed before being displayed.
   global bufferinfo

   # Track smallest start oid request
   if {[dict exists $bufferinfo startoid] \
          && [dict get $bufferinfo startoid] <= $startoid} {
      set startoid [dict get $bufferinfo startoid]
   } else {
      dict set bufferinfo startoid $startoid
   }

   # Buffer control
   dict incr bufferinfo requestcnt   ;# Display update request count
   if {[dict get $bufferinfo mintime]>0 && !$force_display} {
      if {![dict exists $bufferinfo afteridmin]} {
         # No buffering in progress. Start fresh timers.
         dict set bufferinfo afteridmax \
            [after [dict get $bufferinfo maxtime] \
                [list UpdateRunAppDisplay $acct $startoid 1]]
         ## Long timeout forces display update
      } else {
         # Buffering in progress. Reset short timer.
         after cancel [dict get $bufferinfo afteridmin]
      }
      set cnt [dict get $bufferinfo requestcnt]
      dict set bufferinfo afteridmin \
         [after [dict get $bufferinfo mintime] [subst -nocommands {
            UpdateRunAppDisplay $acct $startoid \
               [expr {$cnt == [dict get \$bufferinfo requestcnt]}]
         }]]
      ## Short timer auto resets iff requestcnt changes during timeout
      return
   }
   if {[dict exists $bufferinfo afteridmin]} {
      # If afteridmin is set then afteridmax should be too.
      after cancel [dict get $bufferinfo afteridmin]
      after cancel [dict get $bufferinfo afteridmax]
      dict unset bufferinfo afteridmin
      dict unset bufferinfo afteridmax
   }
   dict unset bufferinfo startoid
   dict set bufferinfo requestcnt 0

   # Begin display update
   global runapps runwidgets appbox apps appid
   set keys [lsort -integer [dict keys $runwidgets]]
   set startindex [SearchIncreasingList $keys $startoid]
   if {$startindex>=0 && [lindex $keys $startindex]<$startoid} {
      incr startindex ;# startindex is index of first change
   }

   # Delete old rows
   if {$startindex <= 0} {
      # Full wipe
      catch {grid forget {*}[grid slaves $apps]}
      ## Catch in case [grid slaves $apps] is empty
      set startrow 0
      set startindex 0
      set keepoid -1
   } else {
      # The oid immediately preceding startindex in runwidgets
      # is suppose to be currently mapped by grid.
      set keepoid [lindex $keys $startindex-1]
      lassign [dict get $runwidgets $keepoid] lblname lbloid chkbox
      set keepinfo [grid info $lblname]
      if {[llength $keepinfo]==0} {
         error "OID $keepoid not mapped"
      }
      set startrow [expr [dict get $keepinfo -row] + 1]
      set endrow [expr {[lindex [grid size $apps] 1]-1}]
      while {$endrow>=$startrow} {
         set widgets [grid slaves $apps -row $endrow]
         if {[llength $widgets]>0} {
            # Calling code may have already destroyed the widgets in
            # some rows.
            grid forget {*}$widgets
         }
         incr endrow -1
      }
   }

   # Handle create and delete requests. widgetadd and widgetdel are
   # lists of oids.
   global widgetadd widgetdel
   foreach oid [lsort -integer $widgetadd] {
      if {[dict exists $runapps $oid]} {
         # If oid is not in runapps, then presumably it has been
         # deleted.
         if {$oid<=$keepoid} {
            error "OID order error: $oid<=$keepoid"
         }
         lassign [dict get $::runapps $oid] \
            xacct appname sid port protocol thread
         if {[string compare $xacct $acct]!=0} {
            error "Account mismatch for oid $oid? $xacct != $acct"
         }
         set wdglist [label $apps.ln$oid -text $appname {*}$::lblopts]
         lappend wdglist [label $apps.lo$oid -text "<$oid>" {*}$::lblopts]
         if {![string match {} $protocol]} {
            lappend wdglist [CreateGuiCheckbutton $acct $oid]
         }
         dict set runwidgets $oid $wdglist
      }
   }
   set widgetadd {}
   foreach oid $widgetdel {
      if {[dict exists $runwidgets $oid]} {
         foreach widget [dict get $runwidgets $oid] {
            destroy $widget
         }
         dict unset runwidgets $oid
      }
   }
   set widgetdel {}
   set widgetkeys [lsort -integer [dict keys $runwidgets]]
   # NB: If the Running Applications pane is being restored after an
   #     account server termination, then the oids might come in
   #     arbitrary order.

   # Consistency check
   if {[llength [dict keys $runapps]] != [llength $widgetkeys]} {
      error "Apps/widgets inconsistency detected."
   }

   # Update grid
   set row $startrow
   foreach oid [lrange $widgetkeys $startindex end] {
      lassign [dict get $runwidgets $oid] lblname lbloid chkbox
      if {[string match {} $chkbox]} {
         set chkbox x
      }
      grid $lblname          -row $row -sticky e
      grid x $lbloid $chkbox -row $row -sticky n
      incr row
   }
   grid columnconfigure $apps 0 -weight 0
   grid columnconfigure $apps 1 -weight 0
   grid columnconfigure $apps 2 -weight 0
   AdjustAppDisplaySize $appbox $appid
   ScrollbarOnOff
}

proc UpdateGuiButtonDisplay { acct oid } {
   # Create a checkbutton for a GUI and append it to an existing
   # display entry.
   global runapps runwidgets appbox appid
   if {![dict exists $runapps $oid]} {
      error "No runapps entry for oid $oid"
   }
   lassign [dict get $runapps $oid] \
      xacct appname sid port protocol thread
   if {[string compare $xacct $acct]!=0} {
      error "Account mismatch for oid $oid? $xacct != $acct"
   }
   if {[string match {} $protocol]} {
      error "App $oid doesn't support a GUI service socket."
   }
   if {![dict exists $runwidgets $oid]} {
      error "No existing display entry for oid $oid"
   }
   lassign [dict get $runwidgets $oid] lblname lbloid chkbox
   if {![string match {} $chkbox]} {
      error "Display entry for oid $oid already has a GUI checkbox"
   }
   set chkbox [CreateGuiCheckbutton $acct $oid]
   dict set runwidgets $oid [list $lblname $lbloid $chkbox]

   set rowdata [grid info $lblname]
   if {[llength $rowdata]>0} {
      # Row has been previously mapped
      set row [dict get $rowdata -row]
      grid $chkbox -row $row -column 2 -sticky n
      AdjustAppDisplaySize $appbox $appid
      # If this is the first oid with a checkbutton, then display
      # width will change. OTOH, the number of oid rows, and hence
      # display height, doesn't change, so neither will the (vertical)
      # scrollbar on/off state.
   } else {
      Oc_Log Log "No labels for oid $oid to append GUI checkbox to." warning
   }
}

# When any account becomes ready, get oids and general interface servers
Oc_EventHandler New _ Net_Account Ready [list TrackAcct %object]

# Open connection with default account server
Net_Account New account_server
UpdateRunAppDisplay $account_server 0 1  ;# Initialize display
