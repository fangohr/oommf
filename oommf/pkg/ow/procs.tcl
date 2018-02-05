# FILE: procs.tcl
#
# A collection of Tcl procedures (not Oc_Classes) which are part of the
# Ow extension
#
# Last modified on: $Date: 2016/01/04 22:45:24 $
# Last modified by: $Author: donahue $

########################################################################
# Routine that imports a color in any valid Tcl color format, and
# returns a string of the form #RRRRGGGGBBB representing the actual
# values that color will produce in the import window (which defaults
# to '.').
########################################################################
proc Ow_GetRGB { color {win .}} {
    foreach {r g b} [winfo rgb $win $color] {}
    return [format "#%04X%04X%04X" $r $g $b]
}

########################################################################
# Routines to use with entry widgets to verify input.  All these
# routines return 0 if the value is valid, 1 if the value is incomplete,
# but otherwise valid (e.g., a value "-" is not a legal number, but
# would be allowed for editting purposes if further additions made it,
# say, -3.14).  A value of 2 means the input includes characters illegal
# for the specified type, or in some cases illegal ordering of
# legal characters (e.g., 2.3.5 is not legal).  The latter might not be
# stringently checked in all cases.  A value of 3 indicates an error
# detected by 'expr $x', most likely a range error.  (NB: Due to some
# weirdness in the 8.x expr command, 'expr {$x}' doesn't catch range
# errors, so we stick to 'expr $x'.)

# Integer check routine
proc Ow_IsInteger { x } {
    if {[regexp -- {^-?[0-9]+$} $x]} {
        if {[catch {expr round($x)}]} {return 3}
        return 0
    }
    if {[regexp -- {^-?[0-9]*$} $x]} { return 1 }
    return 2
}

# Nonnegative integer check routine
proc Ow_IsPosInteger { x } {
    if {[regexp -- {^[0-9]+$} $x]} {
        if {[catch {expr round($x)}]} {return 3}
        return 0
    }
    if {[regexp -- {^[0-9]*$} $x]} { return 1 }
    return 2
}

# Fixed point (no exponential) check routine
proc Ow_IsFixedPoint { x } {
    if {[regexp -- {^-?[0-9]+\.?[0-9]*$} $x]} {
        if {[catch {expr $x}]} {return 3}
        return 0
    }
    if {[regexp -- {^-?[0-9]*\.?[0-9]+$} $x]} { return 0 }
    if {[regexp -- {^-?[0-9]*\.?[0-9]*$} $x]} { return 1 }
    return 2
}

# Nonnegative fixed point (no exponential) check routine
proc Ow_IsPosFixedPoint { x } {
    if {[regexp -- {^[0-9]+\.?[0-9]*$} $x] || \
            [regexp -- {^[0-9]*\.?[0-9]+$} $x]} {
        if {[catch {expr $x}]} {return 3}
        return 0
    }
    if {[regexp -- {^[0-9]*\.?[0-9]*$} $x]} { return 1 }
    return 2
}

# Floating point check routine (exponentials allowed)
proc Ow_IsFloat { x } {
    if {[regexp -- {^-?[0-9]+\.?[0-9]*((e|E)(-|\+)?[0-9]+)?$} $x] || \
            [regexp -- {^-?[0-9]*\.?[0-9]+((e|E)(-|\+)?[0-9]+)?$} $x]} {
        if {[catch {expr $x}]} {return 3}
        return 0
    }
    if {[regexp -- {^-?[0-9]*\.?[0-9]*((e|E)(-|\+)?)?[0-9]*$} $x]} {return 1}
    return 2
}

# Nonnegative floating point check routine (exponentials allowed)
proc Ow_IsPosFloat { x } {
    if {[regexp -- {^[0-9]+\.?[0-9]*((e|E)(-|\+)?[0-9]+)?$} $x] || \
            [regexp -- {^[0-9]*\.?[0-9]+((e|E)(-|\+)?[0-9]+)?$} $x]} {
        if {[catch {expr $x}]} {return 3}
        return 0
    }
    if {[regexp -- {^[0-9]*\.?[0-9]*((e|E)(-|\+)?)?[0-9]*$} $x]} {return 1}
    return 2
}

# Text check routine.  All input allowed
proc Ow_IsText { x } {
    return 0
}

# Routine to associate check routine with type name
proc Ow_GetEntryCheckRoutine { type } {
    set type [string tolower $type]
    switch -- $type {
        text     { set routine Ow_IsText }
        int      { set routine Ow_IsInteger }
        posint   { set routine Ow_IsPosInteger }
        fixed    { set routine Ow_IsFixedPoint }
        posfixed { set routine Ow_IsPosFixedPoint }
        float    { set routine Ow_IsFloat }
        posfloat { set routine Ow_IsPosFloat }
        default {
            Oc_Log Log "WARNING from Ow_GetEntryCheckRoutine:\
                    Unknown type $type; using type text instead." warning
            set routine Ow_IsText
        }
    }
    return $routine
}

########################################################################
# Menu creation helper proc, Tk version independent.
########################################################################
proc Ow_MakeMenubar { rootwin menuwin args } {
    #    $args should be a list of triples.  Each triple is the name
    # (window path) of a submenu to make, the label to associate
    # with that submenu, and the index in the label of the letter
    # to underline for keyboard access.  If only one value is given,
    # it is assumed to be the label; the submenu path will be
    # constructed by appending the lowercase form of the label to
    # "${menuwin}.", and underline will be set to 0.  If only 2 values
    # are given, then if the first begins with a '.' it is assumed to
    # be the submenu path, label will be set from the second value,
    # underline will be set to 0.  If 2 values are given but the first
    # doesn't begin with a '.', then label and underline are set from
    # the input and submenu is constructed from the label as before.
    #    Submenus are created by this proc with '-tearoff 0'.  If you
    # want tearoff enabled, either create the submenu with '-tearoff 1'
    # before calling this proc, or (preferably) use 'configure -tearoff 1'
    # on the submenu after this proc returns.
    #
    # The return value is a list of the submenu pathnames, in order.
    #
    # Examples:
    #           Ow_MakeMenu . .menu File Options Help
    #           Ow_MakeMenu . .m {"Menu A" 5} {"Menu B" 5"} {"Menu C" 5}
    #           Ow_MakeMenu . .m {"A Menu"} {"B Menu"} {"C Menu"}
    #
    # Note that the last example requires 2 grouping sets, because a bare
    # "A Menu" or {A Menu} will be parsed as a two element list, and the
    # above rules will set underline to "Menu".
    #
    # Tk VERSION NOTE: The ".toplevel configure -menu" command first appears
    #   in Tcl/Tk version 8.0.  Prior to this menus had to be configured using
    #   a complicated arrangement of menus and menubuttons.  The Ow_MakeMenu
    #   proc detects which version of Tk is in use, and configures the menubar
    #   as appropriate.  Unfortunately, the older method requires an additional
    #   level of windows.  In this setting the import 'menuwin' is actually
    #   a frame, and the first argument in each of the $args triples is the
    #   winpath of a menubutton.  The actual submenus are children of the
    #   menubuttons, with extension .menu.  In all cases the return value is
    #   a list of _menus_ (never menubuttons).  Since the winpaths of the
    #   submenus will depend on the Tk version, it is *required* to setup the
    #   submenus based on the return value from this routine.  For example,
    #
    #        foreach {filemenu optionmenu helpmenu} \
    #                {[Ow_MakeMenu . .menu File Options Help]} {}
    #        $filemenu add command -label "Exit" -command { exit } -underline 1
    #        ...
    #
    #   will set filemenu to .menu.file under Tk 8.0, but .menu.file.menu under
    #   Tk 7.6.  Furthermore, .menu is a toplevel menu object in Tk 8.0, but a
    #   frame in Tk 7.6.  One may use the expression
    #
    #        if {[string match .menu [winfo toplevel .menu]]==0} { ... }
    #
    #   to distinguish the two cases.
    #

    set new_style 1
    if {[package vsatisfies [package provide Tk] 4]} { set new_style 0}

    if {$new_style} {
        if {![winfo exists $menuwin]} { menu $menuwin }
        $rootwin configure -menu $menuwin
    } else {
        if {![winfo exists $menuwin]} {
            frame $menuwin -borderwidth 2 -relief raised
            pack $menuwin -side top -fill x -expand 0
        }
    }

    set submenu_list {}

    foreach i $args {
        set submenu {} ; set underline 0
        if {[llength $i]<2} {
            # Only label provided
            set label [lindex $i 0]
        } elseif {[llength $i]==2} {
            if {[string match ".*" [lindex $i 0]]} {
                # submenu and label provided
                set submenu [lindex $i 0]
                set label [lindex $i 1]
            } else {
                # label and underline provided
                set label [lindex $i 0]
                set underline [lindex $i 1]
            }
        } else {
            # All three fields provided
            set submenu [lindex $i 0]
            set label  [lindex $i 1]
            set underline [lindex $i 2]
        }
        if {[string match {} $submenu]} {
            set submenu "${menuwin}.[string tolower $label]"
        }
        if {$new_style} {
            if {![winfo exists $submenu]} { menu $submenu -tearoff 0}
            $menuwin add cascade -menu $submenu -label $label \
                    -underline $underline
            lappend submenu_list $submenu
        } else {
            set realmenu ${submenu}.menu
            if {![winfo exists $submenu]} {
                menubutton $submenu -text $label -menu $realmenu \
                        -underline $underline
                if {[string compare $submenu ${menuwin}.help]==0} {
                    pack $submenu -side right ;# Pack "help" menu on the right.
                } else {
                    pack $submenu -side left
                }
            }
            if {![winfo exists $realmenu]} {
                menu $realmenu -tearoff 0
            }
            lappend submenu_list $realmenu
        }
        # Workaround for Tk/Aqua bug: active item on tearoff menus
        # displays (actually, not) as white-on-white. -mjd, 20091029.
        $submenu configure -activebackground \
           [Ow_GetRGB [$submenu cget -activebackground]]
        $submenu configure -activeforeground \
           [Ow_GetRGB [$submenu cget -activeforeground]]
    }
    return $submenu_list
}

#####################################################################
### Crude effort to guess the working menu width, under Tk 8.0+.  ###
### Under older versions of Tk, this routine returns 0.           ###
#####################################################################
proc Ow_GuessMenuWidth { menu } {
    if {[package vsatisfies [package provide Tk] 4]} { return 0 }

    set menufont [$menu cget -font]
    set childcount [llength [winfo children $menu]]
    set menuwidth 0
    for { set index 1 } { $index <= $childcount } { incr index } {
        incr menuwidth 20  ;# Just a guess at padding
        if {![catch {$menu entrycget $index -label} indexlabel]} {
            incr menuwidth [font measure $menufont $indexlabel]
        }
    }
    return $menuwidth
}


#####################################################################
### Proc to fill in the menu items on a standard help menu for an ###
### application.  The argument is the window pathname of a menu   ###
### widget -- probably returned from Ow_MakeMenubar above.        ###
#####################################################################
proc Ow_StdHelpMenu {w} {
    if {![winfo exists $w] || ![string match Menu [winfo class $w]]} {
        return -code error "argument must be a menu widget"
    }
    $w add command -label About -underline 0 \
            -command [list Ow_AboutBox $w About]
    $w add command -label License -underline 0 \
            -command [list Ow_LicenseBox $w License]
    $w add command -label "On [Oc_Main GetAppName]..." -underline 0 \
            -command [list Ow_LaunchHelp]
}

proc Ow_AboutBox {menu item} {
   set txt "[Oc_Main GetAppName] version [Oc_Main GetVersion]\n"
   append txt "  Last Modified: [Oc_Main GetDate]\n"
   append txt "Primary Author: [[Oc_Main GetAuthor] Cget -name]\n"
   append txt "Bug reports to: [[Oc_Main GetAuthor] Cget -email]\n"
   global tcl_platform
   append txt "\nRunning on: [info hostname]\n"
   append txt   "OS/machine: $tcl_platform(os)/$tcl_platform(machine)\n"
   if {[info exists tcl_platform(user)]} {
      append txt "User: $tcl_platform(user)\t"
   }
   append txt "PID: [Oc_Main GetPid]"
   if {![catch {Net_GetNicknames} nicknames] \
          && [llength $nicknames]>0} {
      # Pull out default name, and trim off default prefix
      set appname [string tolower [Oc_Main GetAppName]]
      set oid [Oc_Main GetOid]
      set shortnames {}
      foreach n $nicknames {
         if {[regexp "^${appname}:(.*)\$" $n dummy suffix]} {
            if {[string compare $oid $suffix]!=0} {
               lappend shortnames $suffix
            }
         } else {
            # Bad form; indicate by adding a "{}" prefix
            lappend shortnames "{}:$n"
         }
      }
      if {[llength $shortnames]>0} {
         append txt "\nInstance nicknames: $shortnames"
      }
   }
   if {[string length [Oc_Main GetExtraInfo]]} {
      append txt "\n[Oc_Main GetExtraInfo]"
   }
   set title "About [Oc_Main GetTitle]"
   set w [Ow_Dialog 0 $title info $txt]
   bind $w <Destroy> "+
        # Ignore child window <Destroy> events
        if \[string compare $w %W\] continue
        # catch activation of menu item in case menu was destroyed
        catch {$menu entryconfigure $item -state normal}
    "
   $menu entryconfigure $item -state disabled
}

proc Ow_LicenseBox {menu item} {
    set fn [Oc_Main GetLicenseFile]
    if {![file readable $fn]} {
        error "license file '$fn' not readable"
    }
    set f [open $fn r]
    set txt [read $f [file size $fn]]
    close $f
    # Parse paragraph breaks
    set ptxt ""
    foreach line [split $txt \n] {
        set line [string trim $line]
        if {[string length $line]} {
            append ptxt "$line "
        } else {
            append ptxt \n\n
        }
    }
    set w [Ow_Dialog 0 "[Oc_Main GetAppName] terms of use" info $ptxt]
    bind $w <Destroy> "+
        # Ignore child window <Destroy> events
        if \[string compare $w %W\] continue
        # catch activation of menu item in case menu was destroyed
        catch {$menu entryconfigure $item -state normal}
    "
    $menu entryconfigure $item -state disabled
}

proc Ow_LaunchHelp {} {
    set url [Oc_Main GetHelpURL]
    if {[Oc_Option Get {} htmlBrowserCmd browsercmd]} {
        eval {Oc_Application Exec mmhelp} [$url ToString] &
    } else {
        eval $browsercmd [$url ToString] &
    }
}

#################################################
### Create a list of descendents of a window, ###
### to a specified level.                     ###
#################################################
proc Ow_Descendents { win {level 0} } {
    # Set level to 0 to get all descendents.
    # level = 1 is equivalent to [winfo children $win]
    lappend childlist [winfo children $win]
    if {$level == 1} {
        return [winfo children $win]
    }
    incr level -1
    set childlist {}
    foreach child [winfo children $win] {
        lappend childlist $child
        set childlist [concat $childlist [Ow_Descendents $child $level]]
    }
    return $childlist
}


#####################################################################
### Proc to position a child window relative to a parent.  The    ###
### imports xrat & yrat are the position of the upper lefthand    ###
### corner of the child relative to the ulh corner of the parent. ###
### Bug: if the parent geometry has negative position parameters, ###
### then the aforementioned corners will not be ulh.  Also, this  ###
### may be wm-specific.                                           ###
#####################################################################
proc Ow_PositionChild { child {parent .} {xrat .25} {yrat .25} } {
    set parentpos [winfo geometry [winfo toplevel $parent]]
    set junk ""
    regexp {^([0-9]*)x?([-+]?[0-9]*)([-+][0-9]+)([-+][0-9]+)} \
            "$parentpos" junk parwidth parheight parposx parposy
    if { "$junk" != "" } then {
        if { "$parposx" >= 0 } then {
            set childposx [expr {round($parposx + $xrat*$parwidth)}]
        } else {
            set childposx [expr {round($parposx - $xrat*$parwidth)}]
        }
        # Note: Negative entries measure from lower & right window corner
        if { "$parposy" >= 0 } then {
            set childposy [expr {round($parposy + $yrat*$parheight)}]
        } else {
            set childposy [expr {round($parposy - $yrat*$parheight)}]
        }
        # We can write some fancier code (like centering) at a later date
        # -mjd 19-Jan-1997
        if { $childposx >= 0 } then { set childposx "+$childposx" }
        if { $childposy >= 0 } then { set childposy "+$childposy" }
        wm geometry $child "$childposx$childposy"
    }
}

######################################################################
# Routine to associate icon with a window.
# Type (color, b/w, or none) and size can be set via the
# owWindowIconType and owWindowIconSize options in
#
#    oommf/config/options.tcl
# or
#    oommf/config/local/options.tcl
#
proc Ow_SetIcon { win {iconfile {}} {iconmask {}} } {
   global ow

   # Icon type
   if {[Oc_Option Get {} owWindowIconType icontype]} {
      set icontype color ;# Valid values are color (the default),
      ## b/w (black and white), and none.
   }
   set icontype [string tolower $icontype]
   if {[string match none $icontype]} { return }

   # Icon size
   if {[Oc_Option Get {} owWindowIconSize iconsize]} {
      set iconsize 16 ;# Valid values are 16 and 32.
   }
   if {$iconsize!=16 && $iconsize!=32} {
      return -code error "Invalid owWindowIconSize ($iconsize). \
         See oommf/config/options.tcl for more information."
   }

   # Bug hacks
   if {[Oc_Option Get {} owWindowIconSwapRB swaprb]} {
      set swaprb 0 ;# Default is no swap.
   }

   # Color icon
   set iconset 0
   if {[string match color $icontype]} {
      # Try to set color icons.
      #    Note 1: The iconphoto subcommand to wm is available in Tcl/Tk
      # 8.4.5 and later.
      #    Note 2: The iconphoto subcommand is suppose to accept
      # multiple icons of different sizes, and choose the most
      # appropriate.  However, at least in Tcl/Tk 8.4.12 on Windows, if
      # more than one icon is specified then the larger one is always
      # used, downsampled as necessary.  This is rather poor, so instead
      # just use the size as requested in the oommf/config/options.tcl
      # file.
    if {![package vsatisfies [package provide Tcl] 8.4]} {
      set code 1
    } else {
      set iconname $iconfile
      if {[string match {} $iconname]} {
         if {$swaprb} {
            set iconname [file join $ow(library) omficon${iconsize}rb.gif]
         } else {
            set iconname [file join $ow(library) omficon${iconsize}.gif]
         }
      }
      set icon [image create photo -file $iconname]
      set code [catch {wm iconphoto $win $icon}]
      image delete $icon
    }
      if {!$code} {
         # Success
         set iconset 1
      } else {
         # Error; problem is probably that "wm iconphoto" isn't
         # supported.  If this is Windows, try to load .ico file
         # using "wm iconbitmap" hack
         global tcl_platform
         if {[string match windows $tcl_platform(platform)]} {
            set iconname $iconfile
            if {[string match {} $iconname]} {
               set iconname [file join $ow(library) omficon.ico]
               # The omficon.ico file has both 16x16 and 32x32 pixel
               # icons inside it.
            }
            if {![catch {wm iconbitmap $win $iconname}]} {
               set iconset 1
            }
         }
      }
   }

   # Black and white icon
   if {!$iconset} {
      if {[string match {} $iconfile]} {
         set iconfile [file join $ow(library) omficon${iconsize}.xbm]
         if {[string match {} $iconmask]} {
            set iconmask [file join $ow(library) omfmask${iconsize}.xbm]
         }
      }
      wm iconbitmap $win @$iconfile
      if {![string match {} $iconmask]} {
         wm iconmask $win @$iconmask
      }
   }
   
}


set ow(icon) [file join $ow(library) omficon.xbm]
set ow(iconmask) [file join $ow(library) omfmask.xbm]
proc Ow_SetIconX { win {iconname {}} {iconmaskname {}} } {
   if {[Oc_Option Get {} owDisableWindowIcons disableicons]} {
      set disableicons 0 ;# Default is to enable
   }
   if {$disableicons || ![winfo exists $win]} {
      return
   }

   global ow
   set win [winfo toplevel $win]
   if { "$win" == "." } {
      set iconwin ".root:iconwindow"
   } else {
      set iconwin "$win:iconwindow"
   }

   if {[string match {} $iconname]} {
      set iconname $ow(icon)
   }
   if {[string match {} $iconmaskname]} {
      set iconmaskname $ow(iconmask)
   }

   if {[string match {} $iconname] || [string match {} $iconmaskname]} {
      return         ;# Give up
   }

   # Setup plain icon
   wm iconbitmap $win @$iconname
   wm iconmask $win @$iconmaskname

   # Setup fancier icon (not all wm's support this)
   if {[winfo exists $iconwin]} {
      destroy $iconwin
      update idletasks
   }
   if {![winfo exists $iconwin]} {
      toplevel $iconwin
      label $iconwin.label
      pack $iconwin.label
      $iconwin.label configure -bitmap @$iconname -bg yellow -fg black
      if {[catch {wm iconwindow $win $iconwin}] || \
             [string compare $iconwin [wm iconwindow $win]]!=0 || \
             [update idletasks ; \
                 expr {[winfo viewable $win] && [winfo viewable $iconwin]}] || \
             [string compare "icon" [wm state $iconwin]]!=0 } {
         # wm iconwindow didn't take
         catch {wm iconwindow $win {}}
         destroy $iconwin
      } else {
         # wm iconwindow *did* take.  Setup auto-destruct, taking care
         # to ignore child window <Destroy> events
         bind $win <Destroy> "+
            if \[string compare $win %W\] continue
            # Added catch below to stop errors under RedHat Linux fvwm
            catch {destroy $iconwin}
            "
      }
   }
   # Here's an alternate way, by loading the image from a file:
   #    set imagetoken [image create photo -file oommficon.gif]
   #    toplevel $iconwin
   #    label $iconwin.label -image $imagetoken
   #    pack $iconwin.label
   #    wm iconwindow $win $iconwin
   #    unset imagetoken
}

######################################################
# Routines for  raising a toplevel window and transfering
# keyboard focus to it.
proc Ow_MoveFocus { win } {
    if {[string match {} [focus -lastfor $win]]} {
        focus $win
    } else {
        focus [focus -lastfor $win]
    }
}

proc Ow_RaiseWindow { win } {
    # Note: $win must be a toplevel window
    if {[string compare iconic [wm state $win]]==0} {
        wm state $win normal
    }
    Ow_MoveFocus $win
    raise $win
}

######################################################
# Routines to push/pop watch cursor in requested
# windows.
# Note 1: If multiple Ow_PushWatchCursor calls are made
#  without intervening calls to Ow_PopWatchCursor,
#  then a count of the extra calls is saved, and the
#  watch cursor is not actually removed until the
#  same number of Ow_PopWatchCursor calls have been
#  made.  The "windows" parameter is only
#  interpreted on the first call in such a sequence.
#  This works fine if all calls to this routine from
#  a program use the same "windows" list, but
#  otherwise more sophisticated Push/Pot routines
#  would be needed.
# Note 2: Swapping cursors produces a serious memory
#  leak on some Tk+X platforms.  The unmapped window
#  .owhiddencursorframe is designed to work around
#  this leak, by maintaining a valid reference so Tk
#  doesn't make repeated calls into X freeing and
#  releasing the cursor.  Another workaround is to
#  disable the cursor swapping altogether, by setting
#  the owDisableWatchCursor option in
#      oommf/config/local/options.tcl
#  See also Note 4 below.
# Note 3: On X systems, changing the cursor in a
#  top-level window is sufficient to for the cursor
#  to change in all children packed inside that
#  window.  On Windows, the propagation of the
#  cursor change is lazy, so if you really want the
#  cursor change to be visible, the cursor should
#  be changed in all visible windows.  However, on
#  at least some platforms, boundaries of frame
#  widgets sometimes flash when the cursor is
#  changed.
# Note 4: Changing the cursor on a canvas widget can
#  be relatively expensive, with respect to cpu-time.
#  Tests show the penalty to be a factor of 25 as
#  compared to the root window, or a factor of 6 as
#  compared to a frame.  FWIW, changing cursors on
#  buttons is even slower.  Currently we don't have
#  a good explanation for this behavior, but if the
#  speed issue is ever trouble, a workaround is to
#  set the owDisableWatchCursor option in
#      oommf/config/local/options.tcl
# Note 5: Setting the cursor on menu widgets on at
#  least the winalp (Windows on Dec/Digital/Compaq/HP
#  Alpha) causes the enclosing widget to flash to the
#  last size set by 'wm geometry.'  For this reason,
#  windows of this type are detected and ignored from
#  the import windows list.
if {[Oc_Option Get {} owDisableWatchCursor disablewatch]} {
    set disablewatch 0 ;# Default is to enable
}
if {$disablewatch} {
    proc Ow_PushWatchCursor { {windows {}} } {}
    proc Ow_PopWatchCursor { {force 0} } {}
} else {
    frame .owhiddencursorframe -cursor watch ;# Leave this unmapped;
    ## the purpose is just to hold a reference to the watch cursor
    ## as a workaround to an apparent X memory leak.
    set _watch_cursor_count 0
    set _watch_cursor_safetyid {}
    set _watch_cursor_windows {}
    proc Ow_PushWatchCursor { {windows {}} } {
        global _watch_cursor_count _watch_cursor_safetyid
        global _watch_cursor_windows ;# Only set if _watch_cursor_count<1
        if {$_watch_cursor_count<1} {
            # First watch cursor set event.
            set _watch_cursor_windows {}
            foreach w $windows {
                if {[winfo exists $w] && \
                        [string compare Menu [winfo class $w]]!=0} {
                    ## See Note 5 above.
                    $w config -cursor watch
                    lappend _watch_cursor_windows $w
                }
            }
            set _watch_cursor_count 1
            # Force display of watch cursor
            update idletasks
        } else {
            incr _watch_cursor_count
        }
        # Make sure watch cursor is removed no later than first idle
        # point after 30 seconds.
        after cancel $_watch_cursor_safetyid
        set _watch_cursor_safetyid [after 30000 Ow_PopWatchCursor 1]
    }
    proc Ow_PopWatchCursor { {force 0} } {
        global _watch_cursor_count _watch_cursor_safetyid
        global _watch_cursor_windows
        if {$force || $_watch_cursor_count<=1} {
            # Last watch cursor set event.
            foreach w $_watch_cursor_windows {
                catch {$w config -cursor {}}
                # The catch is to protect against windows that
                # have been destroyed between the cursor push
                # and now.
            }
            after cancel $_watch_cursor_safetyid
            set _watch_cursor_count 0
            set _watch_cursor_windows {}
        } else {
            incr _watch_cursor_count -1
        }
    }
}



################################
# Message dialog box
#
#   If the import modal is 1, then a modal dialog box is created,
# and control is passed to the event loop until the user selects a
# button.  The return value is the number of the selected button.
#   If the import modal is 0, then a non-modal dialog box is created,
# and the proc returns 0 and does not enter the event loop.  Currently
# it does not make sense in this case to display more than one button,
# because the only effect of the button click is to destroy the window,
# i.e., no information is passed back to the calling routine.
#
# Note: Tk built-in bitmaps include error, info, question,
#  questhead and warning.
#
# Note 9-Feb-2007: Under Windows XP, with ActiveTcl 8.4.14,
#  this routine can cause a fatal error if called early in
#  program startup.  This occurs with the current repository
#  version of mmDisp if mmDisp is started with a non-existent
#  file to open on the command line.  The fatal error occurs
#  at the "tkwait variable" command in the "modal" block below.
#  In that case the problem can be fixed by calling update
#  before beginning the file open procedure.  I don't know what
#  causes this.  -mjd
#
set ow(dialog,width) 12c
set ow(dialog,instance) 0
proc Ow_Dialog { modal title bitmap message {width ""} \
        {defaultbtn ""} args } {
    global ow tcl_platform
    set parent [focus]    ;# Position dialog over toplevel with focus.
    if {[string match {} $parent]} {
        set parent "."
    } else {
        set parent [winfo toplevel $parent]
    }
    set instance $ow(dialog,instance)
    incr ow(dialog,instance)
    if {"$width"==""} {set width $ow(dialog,width)}
    set window [toplevel .owDialog$instance]
    wm group $window $parent
    wm title $window "$title"
    Ow_PositionChild $window $parent
 
    frame $window.top    -bd 5
    frame $window.bottom -bd 5
    if {![string match {} $bitmap]} {
        label $window.top.bitmap -bitmap $bitmap
        pack $window.top.bitmap -side left -anchor w -padx 2m
    }

    # Setup text display.  Use a canvas widget instead of a text
    # widget so we can determine the size of the message without
    # actually bringing it up on the display (via update idletasks).
    # The downside is that the canvas doesn't supply default bindings
    # to enable selections.
    set width [winfo pixels . $width]
    set height [expr {int(ceil(0.5*$width))}]
    set msg [canvas $window.top.msg -confine 1 \
            -borderwidth 2 -relief flat \
            -highlightthickness 0 -selectborderwidth 0]
    $msg create text 2 2 -anchor nw -font [Oc_Font Get bold] \
            -text $message -width $width -tags text
    foreach {xmin ymin xmax ymax} [$msg bbox all] {}
    if {$ymax<$height} {
        # No scrollbar
        $msg configure -width $xmax -height $ymax
        pack $msg -side left -fill none -expand 1
        pack $window.top -side top -fill both -expand 1
    } else {
        # Crop height and add vertical scrollbar
        if {![regexp -- "\n$" $message]} {
            append message "\n"    ;# Add trailing newline; this insures
            ## that last line of the message is completely visible.
            ## Note that we can count on at least one <Configure>
            ## event after this setup, and $message will be reloaded
            ## at that time.
        }
        $window.top configure -width $xmax -height $height
        $msg configure -width 0 -height 0
        $msg create rectangle 0 0 1 1 -outline {} ;# Space holder
        ## just to make sure 0 0 is in the bbox.
        set yscroll [scrollbar $window.top.yscroll -orient vertical \
            -command [list $msg yview]]
        $msg configure -yscrollcommand [list $yscroll set] \
                -relief ridge -scrollregion [$msg bbox all]
        pack $msg -side left -fill both -expand 1
        pack $yscroll -side left -fill y
        pack $window.top -side top -fill both -expand 1
        pack propagate $window.top 0
        global ${msg}_text
        set ${msg}_text $message
        bind $msg <Configure> "+
           $msg delete text
           $msg create text 4 4 -anchor nw -font \{[Oc_Font Get bold]\} \
                   -text \$\{${msg}_text\} -width \[expr %w-10\] -tags text
           $msg configure -scrollregion \[$msg bbox all\]"
        # Add "power" scrolling.  This will be enabled after the first
        # <Configure> event, which will occur at display time.
        bind $msg <Configure> {+
            bind %W <B1-Leave><B1-Motion> {
                if {%%y<0} {
                    %%W yview scroll -1 units
                } elseif {%%y>%h} {
                    %%W yview scroll 1 units
                }
            }
        }
        bind $msg <Destroy> "+ unset ${msg}_text"
    }
    # Add selection bindings to text in msg canvas.  These use the
    # mouse to set the PRIMARY selection
    $msg bind text <Button-1> {
        %W select clear
        %W select from current @[%W canvasx %x],[%W canvasy %y]
    }
    $msg bind text <B1-Motion> {
        %W select to current @[%W canvasx %x],[%W canvasy %y]
    }
    # Keystrokes to copy the PRIMARY selection into the clipboard.
    # Needed primarily for windows.
    bind $window <Control-c> _Ow_CopyPrimarySelectionToClipboard
    bind $window <Control-Insert> _Ow_CopyPrimarySelectionToClipboard

    # Add control buttons
    set btncount 0
    if {[info exists ow(dialog,btncode$instance)]} {
        unset ow(dialog,btncode$instance)  ;# Safety
    }
    if {$modal} {
        set btncmd {set ow(dialog,btncode$instance) $btncount}
    } else {
        set btncmd {destroy $window}
    }
    if {[string compare Darwin $tcl_platform(os)]==0 \
           && [llength [info commands ttk::button]]==1} {
       # The "button" widget on the Mac are limited to a height of one
       # line, so, if available, use ttk:button instead.
       set textjust [ttk::style lookup Ow_DialogButtons -justify]
       if {[string compare center $textjust]!=0} {
          # Define style with center text justification.  TButton is the
          # default style for ttk:button (with left justification).
          ttk::style layout Ow_DialogButtons [ttk::style layout TButton]
          eval ttk::style configure Ow_DialogButtons \
             [ttk::style configure TButton]
          ttk::style configure Ow_DialogButtons -justify center
       }
       foreach btnlabel $args {
          set btn($btncount) [ttk::button $window.bottom.btn$btncount \
                              -text $btnlabel -command [subst $btncmd] \
                              -style Ow_DialogButtons]
          bind $btn($btncount) <Key-Return> "$btn($btncount) invoke"
          pack $btn($btncount) -side left -expand 1 -padx 5
          incr btncount 1
       }
       if {$btncount==0} {
          set btn($btncount) [ttk::button $window.bottom.btn$btncount \
                                 -text "OK" -command [subst $btncmd]  \
                                 -style Ow_DialogButtons]
          pack $btn($btncount) -side left -expand 1 -padx 5
          incr btncount 1
       }
    } else {
       foreach btnlabel $args {
          set btn($btncount) [button $window.bottom.btn$btncount \
                              -text $btnlabel -command [subst $btncmd] ]
          bind $btn($btncount) <Key-Return> "$btn($btncount) invoke"
          pack $btn($btncount) -side left -expand 1 -padx 5
          incr btncount 1
       }
       if {$btncount==0} {
          set btn($btncount) [button $window.bottom.btn$btncount \
                                -text "OK" -command [subst $btncmd] ]
          pack $btn($btncount) -side left -expand 1 -padx 5
          incr btncount 1
       }
    }
    pack $window.bottom -side bottom -fill x -expand 0 -before $window.top
 
    if {"$defaultbtn"!="" && $defaultbtn>=0 && $defaultbtn<$btncount} {
        # Code to make a "default" frame around default button.
        set fout [frame $window.bottom.defaultouter -bd 2 -relief sunken]
        lower $fout
        pack $fout -before $btn($defaultbtn) \
                -side left -expand 1 -padx 5
        pack forget $btn($defaultbtn)
        pack $btn($defaultbtn) -in $fout -padx 3 -pady 3
        # Give this button the keyboard focus
        focus $btn($defaultbtn)
        wm protocol $window WM_DELETE_WINDOW "$btn($defaultbtn) invoke"
    } elseif {$modal} {
        wm protocol $window WM_DELETE_WINDOW "$btn(0) invoke"
    }

    Ow_SetIcon $window

    if {$modal} {
        update idletasks
        focus $window
        catch {tkwait visibility $window; grab $window }
        # Note: It appears dangerous to put a "grab" on a window
        #  before it is visible.  (Errant mouse clicks???)
        # Also, the sequence (including order) of
        #  'update idletasks ; focus $window ; catch ...'
        # seems to protect against binding reentrancy from the
        # user clicking too many times, or hitting keys too fast.
        catch {tkwait variable ow(dialog,btncode$instance) }
        # Note: See 9-Feb-2007 note at top of proc about this
        #  tkwait command.
        set returncode $ow(dialog,btncode$instance)
        unset ow(dialog,btncode$instance)
        grab release $window
        after 10 "destroy $window" ;# "after" is a workaround for Tk 8.5.x bug
        return $returncode
    }

    return $window
}

# Utility proc to copy primary selection to clipboard.
;proc _Ow_CopyPrimarySelectionToClipboard {} {
    # Wrap up in catch in case selection not set
    catch {
        clipboard clear
        clipboard append -- [selection get]
    }
}


###################################################
### Nonfatal warning message dialog box (modal) ###
###################################################
proc Ow_NonFatalError { msg {title {}} {exitcode 1} \
        {bitmap warning} {default_die 0} } {
    if {"$title"==""} {
        global ProgName
        if {[info exists ProgName]} { set title "$ProgName: " }
        append title "Nonfatal Error"
    }
    global errorCode errorInfo
    foreach {ei ec} [list $errorInfo $errorCode] {}
    if {$default_die} {
        set default_action 2
    } else {
        set default_action 0
    }
    set usercode [Ow_Dialog 1 $title $bitmap $msg {} $default_action \
            "Continue" "Stack" "Die"]
    if {$usercode==2} { exit $exitcode }
    if {$usercode==1} {
        set msg "[Oc_Main GetInstanceName] Stack:\n"
        append msg "$ei\n-----------\nAdditional info: $ec"
        if {$default_die} {
            set default_action 1
        } else {
            set default_action 0
        }
        set usercode [Ow_Dialog 1 "${title}:Stack" info $msg {} \
                $default_action "Continue" "Die"]
        if {$usercode==1} { exit $exitcode }
    }
    return 0
}

################################
# Modal prompt dialog box
#   This routine allows the user to input an arbitrary string.
# The return is a 2 element list: a return string, and either
# 0 or 1; 0 indicates an "OK" return, 1 implies a "Cancel" return.
# If 0, then the return string is the user input string. If 0, then
# "$defaultvalue" is returned.
#
proc Ow_PromptDialog { title message {defaultvalue {}} \
        {width {}} {checkscript {}} } {
    # "checkscript" not yet implemented
    global ow
    set parent [focus]    ;# Position dialog over toplevel with focus.
    if {[string match {} $parent]} {
        set parent "."
    } else {
        set parent [winfo toplevel $parent]
    }

    if {[string match {} $width]} {set width $ow(dialog,width)}
    set window [toplevel .owPromptDialog]
    wm group $window .
    wm title $window "$title"
    Ow_PositionChild $window $parent

    frame $window.top    -bd 10
    frame $window.bottom -bd 10
    set msg [message $window.top.msg -text $message -width $width]
    set value [entry $window.top.entry]
    $value insert 0 $defaultvalue

    pack $msg -side top -fill both -expand 1
    pack $value -side top -fill x -expand 1

    set btnOK [button $window.bottom.btnOK -text "OK" \
            -command "set ow(prompt_dialog,btn) 0"]
    set btnCancel [button $window.bottom.btnCancel -text "Cancel" \
            -command "set ow(prompt_dialog,btn) 1"]
    pack $btnOK $btnCancel -side left -expand 1 -padx 5

    wm protocol $window WM_DELETE_WINDOW \
            "set ow(prompt_dialog,btn) 0"

    pack $window.top $window.bottom -side top -fill both -expand 1

    # Extra bindings
    bind $value <Key-Return> "$btnOK invoke"
    bind $value <Key-Escape> \
            "$value delete 0 end ; $value insert 0 \{$defaultvalue\}"
    bind $btnOK <Key-Return> "$btnOK invoke"
    bind $btnCancel <Key-Return> "$btnCancel invoke"
 
    $value selection range 0 end
    focus $value
 
    update idletasks
    focus $window
    catch {tkwait visibility $window; grab $window }
    # Note: It appears dangerous to put a "grab" on a window
    #  before it is visible.  (Errant mouse clicks???)
    # Also, the sequence (including order) of
    #  'update idletasks ; focus $window ; catch ...'
    # seems to protect against binding reentrancy from the
    # user clicking too many times, or hitting keys too fast.
 
    Ow_SetIcon $window
    catch {tkwait variable ow(prompt_dialog,btn) }
    if {$ow(prompt_dialog,btn)==0} {
        # OK invoked
        set result [list [$value get] 0]
    } else {
        # Cancel invoked
        set result [list $defaultvalue 1]
    }    
    unset ow(prompt_dialog,btn)
 
    grab release $window
    after 10 "destroy $window" ;# "after" is a workaround for Tk 8.5.x bug
    return $result
}

proc Ow_Message {msg type src} {
    global errorInfo errorCode
    foreach {ei ec} [list $errorInfo $errorCode] {break}
    if {[catch {winfo exists .} result] || !$result} {
        foreach {errorInfo errorCode} [list $ei $ec] {break}
        Oc_DefaultMessage $msg $type $src
        return
    }
    set tock [format {[%s]} [clock format [clock seconds] -format "%T"]]

    switch $type {
        panic {
            Ow_NonFatalError "[Oc_Main GetInstanceName] $src panic:\n$msg" \
                    "[Oc_Main GetInstanceName] $src Panic  $tock" 1 error 1
        }
        error {
            Ow_NonFatalError "[Oc_Main GetInstanceName] $src error:\n$msg" \
                    "[Oc_Main GetInstanceName] $src Error  $tock" 1 error
        }
        warning {
            Ow_NonFatalError "[Oc_Main GetInstanceName] $src warning:\n$msg" \
                    "[Oc_Main GetInstanceName] $src Warning  $tock" 1 warning
        }
        info {
            Ow_Dialog 1 "[Oc_Main GetInstanceName] $src Info  $tock" info \
                    "[Oc_Main GetInstanceName] $src info:\n$msg" {} 0
        }
        default {
            Ow_Dialog 0 "[Oc_Main GetInstanceName] $src Info  $tock" info \
                    "[Oc_Main GetInstanceName] $src info:\n$msg" {} 0
        }
    }
}
