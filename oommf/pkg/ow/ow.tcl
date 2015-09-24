# FILE: ow.tcl
#
#	OOMMF Widgets
#
# Last modified on: $Date: 2012-09-25 17:12:02 $
# Last modified by: $Author: dgp $
#
# When this version of the ow extension is selected by the 'package require'
# command, this file is sourced.

# Other package requirements
if {[catch {package require Tcl 8}]} {
    package require Tcl 7.5
}
package require Oc 1.1.0.2			;# [Oc_ResolveLink]
if {[catch {package require Tk 8}]} {
    package require Tk 4.1
}
package require Nb 1.1.1.0			;# [Nb_RatApprox]

Oc_CheckTclIndex Ow

# CVS 
package provide Ow 1.2.0.5

# Provide Oc_Log dialogs for message reporting 
Oc_Log SetLogHandler Ow_Message panic Oc_Log
Oc_Log SetLogHandler Ow_Message error Oc_Log
Oc_Log SetLogHandler Ow_Message warning Oc_Log
Oc_Log SetLogHandler Ow_Message info Oc_Log
Oc_Log SetLogHandler Ow_Message panic
Oc_Log SetLogHandler Ow_Message error
Oc_Log SetLogHandler Ow_Message warning
Oc_Log SetLogHandler Ow_Message info

# Running under X11?
proc Ow_IsX11 {} {
   if {[catch {tk windowingsystem} ws]} {
      # tk windowingsystem command introduced in Tk 8.4
      global tcl_platform
      if {[string match windows $tcl_platform(platform)]} {
         return 0
      } else {
         # Otherwise, presumably X11
         return 1
      }
   }
   if {[string match x11 $ws]} {
      return 1
   }
   return 0
}

# Running under Mac OS X Aqua?
proc Ow_IsAqua {} {
   # tk windowingsystem command introduced in Tk 8.4
   if {![catch {tk windowingsystem} ws] && [string match aqua $ws]} {
      return 1
   }
   return 0
}

# Menu bind hack to work around mousewheel problems on X11
if {[Ow_IsX11] \
       && [string match {} [bind Menu <ButtonPress-1>]] \
       && [string match {} [bind Menu <ButtonRelease-1>]]} {
   set bp [bind Menu <ButtonPress>]
   set br [bind Menu <ButtonRelease>]
   if {![string match {} $bp] && ![string match {} $br]} {
      # If code flows to here, then we are on X11, with no bindings on
      # Menu to button 1, but with bindings on Menu to general button.
      # All the restrictions are an attempt to future-proof this hack
      # against changes in the default Tk bindings.
      bind Menu <ButtonPress-1> $bp
      bind Menu <ButtonRelease-1> $br
      bind Menu <ButtonPress> {}
      bind Menu <ButtonRelease> {}
   }
}


# Set up for autoloading of em extension commands
set ow(library) [file dirname [info script]]
if { [lsearch -exact $auto_path $ow(library)] == -1 } {
    lappend auto_path $ow(library)
}

# Load in any local modifications
set local [file join [file dirname [info script]] local ow.tcl]
if {[file isfile $local] && [file readable $local]} {
    uplevel #0 [list source $local]
}


### MouseWheel support   ###############################################
### Based on coded posted to http://paste.tclers.tk/tcl by "de" on
### Wed Mar 19 00:37:18 GMT 2008.  The <Shift-MouseWheel> binding to
### horizontal scroll reportedly mimics standard mousewheel bindings
### on Mac OS X.
global Ow_mw_classes
set Ow_mw_classes [list Text Canvas Listbox Table TreeCtrl]

proc MouseWheel {wFired D X Y horizontal} {
   # do not double-fire in case the class already has a binding
   if {![string match {} [bind [winfo class $wFired] <MouseWheel>]]} { return }
   # obtain the window the mouse is over
   set w [winfo containing $X $Y]
   # if we are outside the app, try and scroll the focus widget
   if {![winfo exists $w]} { catch {set w [focus]} }
   if {[winfo exists $w]} {
      # scrollbars have different call conventions
      if {[string match "Scrollbar" [winfo class $w]]} {
         # Ignore "horizontal" import; scrollbars presumably know
         # which way to scroll.
         switch -glob -- [$w cget -orient] {
            h* { eval [$w cget -command] scroll [expr {-($D/30)}] units }
            v* { eval [$w cget -command] scroll [expr {-($D/30)}] units }
         }
      } else {
         if {$horizontal} {
            set dir "x"
         } else {
            set dir "y"
         }
         while {![string match {} $w]} {
            global Ow_mw_classes
            if {[lsearch $Ow_mw_classes [winfo class $w]] > -1} {
               if {[string match {} [$w cget -${dir}scrollcommand]]} {
                  set w [winfo parent $w]
                  continue
               }
               catch {$w ${dir}view scroll [expr {- ($D / 120) * 4}] units}
               break
            }
            set w [winfo parent $w]
         }
      }
   }
}

if {[Oc_Application CompareVersions [info tclversion] 8.1]>=0} {
   # MouseWheel event introduced in Tk 8.1
   foreach class $Ow_mw_classes { bind $class <MouseWheel> {} }
   if {[Ow_IsX11]} {
      foreach class $Ow_mw_classes {
         bind $class <4> {}
         bind $class <5> {}
      }
      # Support for mousewheels on Linux/Unix commonly comes through
      # mapping the wheel to the extended buttons.
      bind all <4> [list MouseWheel %W 120 %X %Y 0]
      bind all <5> [list MouseWheel %W -120 %X %Y 0]
      bind all <Shift-4> [list MouseWheel %W 120 %X %Y 1]
      bind all <Shift-5> [list MouseWheel %W -120 %X %Y 1]
   }
   bind all <MouseWheel> [list MouseWheel %W %D %X %Y 0]
   bind all <Shift-MouseWheel> [list MouseWheel %W %D %X %Y 1]
}
