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
# Routine that imports a color in any valid Tcl color format, and
# returns a value between 0 and 255 indicating the gray-level shade of
# that color, with 0 being black and 255 white. (This is computed as a
# simple equally weighted average of the red, green, and blue
# components.
########################################################################
proc Ow_GetShade { color {win .}} {
   foreach {r g b} [winfo rgb $win $color] {}
   return [expr {($r+$g+$b)/(3*256)}]
}

########################################################################
# Change OOMMF color scheme
########################################################################
proc Ow_ChangeColorScheme { scheme } {
   # At present only support color schemes are Tk default and light
   if {[string match default $scheme]} { return }
   switch -exact -- $scheme {
      light { _Ow_ChangeColorScheme_light }
      dark  { _Ow_ChangeColorScheme_dark }
      default {
         return -code error "unsupported scheme request: \"$scheme\".\
                             Should be one of \"default\" or \"light\"."
      }
   }
}
;proc _Ow_ChangeColorScheme_light {} {
   # Approximate macOS "aqua" (light) mode
   # NOTE: Selected radiobuttons and checkboxes should show white on
   # blue background, but the standard (non-ttk) Tk widgets on macOS
   # don't allow control of the background color on these widgets, which
   # in macOS dark mode are fixed to gray. This could be fixed by moving
   # to the ttk widgets with "default" theme (which is not the default
   # on macOS).
   tk_setPalette \
      activeBackground    #ECECEC \
      activeForeground    #FFFFFF \
      background          #ECECEC \
      disabledForeground  #A3A3A3 \
      foreground          #000000 \
      highlightBackground #ECECEC \
      highlightColor      #828282 \
      insertBackground    #000000 \
      selectColor         #B03060 \
      selectBackground    #B3D7FF \
      selectForeground    #000000 \
      troughColor         #C3C3C3 \
      disabledBackground  #ECECEC \
      readonlyBackground  #ECECEC
   # tk_setPalette changes colors in all existing widgets, and also sets
   # up values in the option database that control colors in new
   # (future) widgets. You can also manually add settings to the option
   # database to set colors in all or some new widgets. For example, to control
   # the text foreground color in new menu widgets, you can do
   #    option add *Menu.foreground cyan
   # if .mb.file is any menu widget then
   #    option get .mb.file foreground *
   # will return "cyan", regardless of what the foreground color for
   # that widget actually is, i.e., ".mb.file cget -foreground" may be
   # different. Likewise, to set the default checkbutton box color, use
   #    option add *Checkbutton.selectColor pink
   # This can be read by creating a checkbutton (say .foo.cb) widget
   # and checking
   #    option get .foo.cb selectColor *
   # The above Palette list was obtained from Tk 8.6.10 on macOS
   # 10.14.6 (Mojave). Note that the last two items, disabledBackground
   # and readonlyBackground, are optionally used by the Tk entry widget,
   # but are not listed on the Tk palette man page. BTW, tk_setPalette
   # allows you to set "unknown" keys, for example, "tk_setPalette
   # background black foo white", so it is safe to set keys not known on
   # all platforms.

   # BTW, IMO the above, when run under dark mode in macOS, leaves the
   # select marker in checkbutton widgets a little too light, but I
   # haven't been able to find any way to change that. The
   # ttk::checkbutton widget appears more flexible, so we might want to
   # switch over to the ttk widget set. (This report is for a Tcl/Tk
   # 8.6.10 aqua build of Tk (check "tk windowingsystem"). In that
   # setting the color of the checkbutton box and select indicator are
   # controlled by the checkbutton -background option. The -selectcolor
   # and -foreground options, which usually control the checkbutton box
   # and select indicator colors, are ignored.)

   # The tk_setPalette code is a Tcl proc defined, at least for Tcl/Tk
   # 8.6.10, in the Tk source file tk/library/palette.tcl. The values
   # are stored in the global array ::tk::Palette, so, if tk_setPalette
   # has been called, then the palette can be viewed via
   #   parray ::tk::Palette
   #
   # The only database names explicitly set in tk_setPalette are
   #    activeBackground
   #    activeForeground
   #    background
   #    disabledForeground
   #    foreground
   #    highlightBackground
   #    highlightColor
   #    insertBackground
   #    selectForeground
   #    selectBackground
   #    troughColor
   # which omits selectColor mentioned on the Tk palette man page, as
   # well as disabledBackground and readonlyBackground keys used by the
   # Tk entry widget. BTW, the 11 keys listed above are exactly the set
   # set by the tk_bisque proc.
   #
   # For light mode on macOS ::tk::Palette is:
   #       activeBackground    #FFFFFF
   #       activeForeground    #000000
   #       background          #ECECEC  (systemWindowBackgroundColor)
   #       disabledForeground  #B1B1B1
   #       foreground          #000000
   #       highlightBackground #ECECEC  (systemWindowBackgroundColor)
   #       highlightColor      #000000
   #       insertBackground    #000000
   #       selectBackground    #D5D5D5
   #       selectForeground    #000000
   #       troughColor         #D5D5D5
   #
   # For dark mode on macOS ::tk::Palette is:
   #       activeBackground     #767676
   #       activeForeground     #FFFFFF
   #       background           #323232
   #       disabledForeground   #656565
   #       foreground           #FFFFFF
   #       highlightBackground  #323232
   #       highlightColor       #FFFFFF
   #       insertBackground     #FFFFFF
   #       selectBackground     #2D2D2D
   #       selectForeground     #FFFFFF
   #       troughColor          #2D2D2D
   #
   # The widget set recognized by tk_setPalette is:
   #
   #  button canvas checkbutton entry frame label labelframe
   #  listbox menubutton menu message radiobutton scale scrollbar
   #  spinbox text
   #
   # Here is a table of all color-related widget configure options and
   # the widgets which support each:
   #
   #         -activebackground : button checkbutton label menubutton menu \
   #                              radiobutton scale scrollbar spinbox
   #         -activeforeground : button checkbutton label menubutton menu \
   #                              radiobutton
   #               -background : button canvas checkbutton entry frame label \
   #                              labelframe listbox menubutton menu message \
   #                              radiobutton scale scrollbar spinbox text
   #       -disabledforeground : button checkbutton entry label listbox \
   #                              menubutton menu radiobutton spinbox
   #               -foreground : button checkbutton entry label labelframe \
   #                              listbox menubutton menu message radiobutton \
   #                              scale spinbox text
   #      -highlightbackground : button canvas checkbutton entry frame label \
   #                              labelframe listbox menubutton message \
   #                              radiobutton scale scrollbar spinbox text
   #           -highlightcolor : button canvas checkbutton entry frame label \
   #                              labelframe listbox menubutton message \
   #                              radiobutton scale scrollbar spinbox text
   #         -insertbackground : canvas entry spinbox text
   #              -selectcolor : checkbutton menu radiobutton
   #         -selectforeground : canvas entry listbox spinbox text
   #         -selectbackground : canvas entry listbox spinbox text
   #              -troughcolor : scale scrollbar
   #
   # -inactiveselectbackground : text
   #       -readonlybackground : entry spinbox
   #       -disabledbackground : entry spinbox
   #         -buttonbackground : spinbox
   #                 -colormap : frame labelframe
   #
   # There are 17 entries here, which include the 12 from the tk_setPalette
   # man page, plus the bottom 5.
   #
   #
   # On my Tk 8.6.10/macOS Mojave build,
   #    winfo rgb . systemWindowBackgroundColor
   # returns
   #   60652 60652 60652
   # (=hex color ECECEC) regardless of whether light or dark mode are
   # selected. This appears to be correct for light mode, but in dark I
   # think this should be
   #    12850 12850 12850
   # (=hex color 323232).
   #
   # BIG NOTE: It appears that 'winfo rgb . <system_symbolic_colorname>'
   # returns the rgb values for that color at process start time, that
   # is, calling '::tk::unsupported::MacWindowStyle appearance' to
   # change the light/dark display mode doesn't have any effect on the
   # 'winfo rgb' return value. So, to get the proper color values you
   # need to change 'System Preference|General|Appearance' to the
   # desired mode before launching tclsh/wish and running 'winfo rgb
   # ...'. Use the boolean return from
   #
   #    tk::unsupported::MacWindowStyle isdark <win>
   #
   # to determine whether or not <win> is in dark mode.
   #
   # I ran this test on BigSur with Tcl/Tk 8.6.12 for all the "semantic
   # colors" I could find listed in the Tk docs.  The results
   # follow. Many of the colors were the same for both light and dark
   # mode, so I broke the results into three lists, first the non-common
   # colors for light mode, non-common for dark mode, and then the
   # common color list:
   #
   # Light unique ---
   #                    systemControlTextColor: #000000
   #            systemDisabledControlTextColor: #000000
   #                          systemLabelColor: #000000
   #                           systemLinkColor: #0068DA
   #                      systemMenuActiveText: #000000
   #                        systemMenuDisabled: #000000
   #                            systemMenuText: #000000
   #                systemPlaceholderTextColor: #000000
   #                systemSelectedTabTextColor: #000000
   #         systemSelectedTextBackgroundColor: #B3D7FF
   #                   systemSelectedTextColor: #000000
   #                      systemSeparatorColor: #000000
   #                 systemTextBackgroundColor: #FFFFFF
   #                           systemTextColor: #000000
   #               systemWindowBackgroundColor: #ECECEC
   #
   # Dark unique ---
   #                    systemControlTextColor: #FFFFFF
   #            systemDisabledControlTextColor: #FFFFFF
   #                          systemLabelColor: #FFFFFF
   #                           systemLinkColor: #419CFF
   #                      systemMenuActiveText: #FFFFFF
   #                        systemMenuDisabled: #FFFFFF
   #                            systemMenuText: #FFFFFF
   #                systemPlaceholderTextColor: #FFFFFF
   #                systemSelectedTabTextColor: #FFFFFF
   #         systemSelectedTextBackgroundColor: #3F638B
   #                   systemSelectedTextColor: #FFFFFF
   #                      systemSeparatorColor: #FFFFFF
   #                 systemTextBackgroundColor: #1E1E1E
   #                           systemTextColor: #FFFFFF
   #               systemWindowBackgroundColor: #323232
   #
   # Common colors ---
   #                      systemActiveAreaFill: #FFFFFF
   #               systemAlertBackgroundActive: #ECECEC
   #             systemAlertBackgroundInactive: #ECECEC
   #      systemAlternatePrimaryHighlightColor: #0850D0
   #                 systemAppleGuideCoachmark: #FFFFFF
   #                     systemBevelActiveDark: #A3A3A3
   #                    systemBevelActiveLight: #A3A3A3
   #                   systemBevelInactiveDark: #D1D1D1
   #                  systemBevelInactiveLight: #D1D1D1
   #                               systemBlack: #000000
   #           systemButtonActiveDarkHighlight: #E1E1E1
   #              systemButtonActiveDarkShadow: #DCDCDC
   #          systemButtonActiveLightHighlight: #FDFDFD
   #             systemButtonActiveLightShadow: #E1E1E1
   #                          systemButtonFace: #F0F0F0
   #                    systemButtonFaceActive: #F0F0F0
   #                  systemButtonFaceInactive: #FAFAFA
   #                   systemButtonFacePressed: #ABABAB
   #                         systemButtonFrame: #828282
   #                   systemButtonFrameActive: #828282
   #                 systemButtonFrameInactive: #C2C2C2
   #         systemButtonInactiveDarkHighlight: #EEEEEE
   #            systemButtonInactiveDarkShadow: #E6E6E6
   #        systemButtonInactiveLightHighlight: #FDFDFD
   #           systemButtonInactiveLightShadow: #EEEEEE
   #          systemButtonPressedDarkHighlight: #979797
   #             systemButtonPressedDarkShadow: #8A8A8A
   #         systemButtonPressedLightHighlight: #BABABA
   #            systemButtonPressedLightShadow: #979797
   #                       systemChasingArrows: #000000
   #                  systemControlAccentColor: #007AFF
   #              systemDialogBackgroundActive: #ECECEC
   #            systemDialogBackgroundInactive: #ECECEC
   #            systemDocumentWindowBackground: #FFFFFF
   #                          systemDragHilite: #B2D7FF
   #                    systemDrawerBackground: #E8E8E8
   #              systemFinderWindowBackground: #FFFFFF
   #                      systemFocusHighlight: #7EADD9
   #                           systemHighlight: #B2D7FF
   #                  systemHighlightAlternate: #0850D0
   #                  systemHighlightSecondary: #D0D0D0
   #                 systemIconLabelBackground: #FAFDFD
   #         systemIconLabelBackgroundSelected: #999999
   #                  systemListViewBackground: #FFFFFF
   #               systemListViewColumnDivider: #000000
   #           systemListViewEvenRowBackground: #F5F5F5
   #            systemListViewOddRowBackground: #FFFFFF
   #                   systemListViewSeparator: #FFFFFF
   #        systemListViewSortColumnBackground: #FFFFFF
   #                                systemMenu: #FFFFFF
   #                          systemMenuActive: #3571CD
   #                      systemMenuBackground: #FFFFFF
   #              systemMenuBackgroundSelected: #3571CD
   #      systemModelessDialogBackgroundActive: #ECECEC
   #    systemModelessDialogBackgroundInactive: #ECECEC
   #              systemMovableModalBackground: #ECECEC
   #        systemNotificationWindowBackground: #ECECEC
   #                    systemPopupArrowActive: #000000
   #                  systemPopupArrowInactive: #808080
   #                   systemPopupArrowPressed: #000000
   #              systemPressedButtonTextColor: #FFFFFF
   #               systemPrimaryHighlightColor: #B2D7FF
   #            systemScrollBarDelimiterActive: #FFFFFF
   #          systemScrollBarDelimiterInactive: #FFFFFF
   #             systemSecondaryHighlightColor: #D0D0D0
   #                     systemSheetBackground: #ECECEC
   #               systemSheetBackgroundOpaque: #ECECEC
   #          systemSheetBackgroundTransparent: #E8E8E8
   #                      systemStaticAreaFill: #FFFFFF
   #                   systemToolbarBackground: #ECECEC
   #                         systemTransparent: #FFFFFF
   #       systemUtilityWindowBackgroundActive: #ECECEC
   #     systemUtilityWindowBackgroundInactive: #ECECEC
   #                               systemWhite: #FFFFFF
   #                          systemWindowBody: #FFFFFF
   #
   # YMMV.

   # Adjust colors in ttk "themed" widgets. Note that "ttk::style theme
   # use" resets the colors, so "ttk::style configure" should come
   # afterwards. Note: The "default" theme is not the default on macOS,
   # but does allow setting of the background.
   catch {ttk::style theme use default}
   ttk::style configure TButton -foreground #000000
   ttk::style configure TButton -background #FFFFFF

   # On macOS, menu background is not changed by tk_setPalette, so
   # override changes to menus. This only affects new menus, not
   # existing ones, which should be OK provided this routine is called
   # before any menus are created.
   if {![catch {tk windowingsystem} _] && [string match aqua $_]} {
      set menu_colors {
              activebackground systemMenuActive
              activeforeground systemMenuActiveText
                    background systemMenu
            disabledforeground systemMenuDisabled
                    foreground systemMenuText
                   selectColor systemMenuActive
       }
      foreach {name value} $menu_colors {
         option add *Menu.$name $value
      }
   }
   # Another way to affect menu colors is to add a code block like the
   # following into proc Ow_MakeMenubar in oommf/pkg/ow/procs.tcl:
   #    if {[llength [option get . foreground *]]} {
   #       # Assume tk_setPalette was used to change default foreground and
   #       # possibly other colors. Revert to defaults for menubar items.
   #       foreach w $submenu_list {
   #          foreach optset [$w configure] {
   #             if {[llength $opt]==2} {
   #                continue  ;# Alias, e.g., -fg for -foreground
   #             }
   #             set opt [lindex $optset 0]
   #             if {[string match -nocase -*foreground $opt] \
   #                    || [string match -nocase -*background $opt] \
   #                    || [string match -nocase -*color $opt]} {
   #                $w configure $opt [lindex $optset 3]
   #             }
   #          }
   #       }
   #    }

   # The standard Tk widget tk_chooseColor can be used as a tool to get
   # RGB values for displayed colors. (Annoyingly, this tool is
   # implemented as a modal dialog box, so you can't capture color
   # changes that require window focus.)
}
;proc _Ow_ChangeColorScheme_dark {} {
   # Approximate macOS "darkaqua" (dark) mode

   # Note 1: In Tk 8.6.10, most widgets use default -background of
   # systemWindowBackgroundColor, but some, notably listbox, use instead
   # systemTextBackgroundColor. In macOS dark mode, the former is
   # #323232, and the latter is #1E1E1E. Take your pick, or use the
   # average value of #282828.

   # Note 2: In Tk 8.6.10 on macOS, when the system appearance is
   # "dark", the standard (non-ttk) buttons have background colored by
   # their -highlightbackground, with -background ignored. If the macOS
   # appearance setting is "light", then the button background is
   # -highlightbackground if the window is not the active window (where
   # active means the window holding the focus), but is forced white
   # when the windows has focus (go figure), with -background again
   # ignored. The ttk:button class doesn't have this focus/non-focus
   # behavior, and it is arguably a bug in the Tk button, but regardless
   # this is a problem for implementing the OOMMF dark mode on top of
   # macOS light appearance. One fix would be to replace all the
   # standard button widgets in OOMMF with ttk:buttons (there is a
   # similar issue with radiobuttons and checkbuttons in light mode, see
   # above), but the easier fix is to push the app into macOS dark
   # mode. The latter relies on a tk::unsupported command, which is
   # moreover broken on some anaconda builds of Tk 8.6.10, but this
   # OOMMF dark mode is not critical functionality for OOMMF, so we'll
   # go that route for now. (Don't be surprised if this breaks in the
   # future.)
   if {![catch {tk windowingsystem} _] && [string match aqua $_]} {
      update idletasks ;# Needed for tk::unsupported::MacWindowStyle call
      catch {::tk::unsupported::MacWindowStyle appearance . darkaqua}
      # Ignore on failure
   }

   set bgcolor #282828
   tk_setPalette \
      activeBackground     #767676 \
      activeForeground     #FFFFFF \
      background           $bgcolor \
      disabledForeground   #656565 \
      foreground           #FFFFFF \
      highlightBackground  $bgcolor \
      highlightColor       #FFFFFF \
      insertBackground     #FFFFFF \
      selectBackground     #3F638B \
      selectForeground     #FFFFFF \
      troughColor          #C3C3C3 \
      selectColor          #3F638B \
      disabledBackground   $bgcolor \
      readonlyBackground   $bgcolor

   # Adjust colors in ttk "themed" widgets. Note that "ttk::style theme
   # use" resets the colors, so "ttk::style configure" should come
   # afterwards.
   catch {ttk::style theme use default}
   ttk::style configure TButton -foreground #FFFFFF
   ttk::style configure TButton -background #707070

   # On macOS, menu background is not changed by tk_setPalette, so
   # override changes to menus. This only affects new menus, not
   # existing ones, which should be OK provided this routine is called
   # before any menus are created.
   if {![catch {tk windowingsystem} _] && [string match aqua $_]} {
      set menu_colors {
              activebackground systemMenuActive
              activeforeground systemMenuActiveText
                    background systemMenu
            disabledforeground systemMenuDisabled
                    foreground systemMenuText
                   selectColor systemMenuActive
       }
      foreach {name value} $menu_colors {
         option add *Menu.$name $value
      }
   }
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
### Under older versions of Tk, this routine returns 0. This      ###
### routine is typically used to set a brace frame to maintain    ###
### a minimum primary window size to insure menu visibility.      ###
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
### Set title for toplevel window win, and return the recommended ###
### minimum window width.  The client can use the return value to ###
### size a supporting brace that protects title visibility. (If   ###
### the window is too narrow the title can be obscured by window  ###
### decorations.)                                                 ###
#####################################################################
proc Ow_GetWindowTitleSize { title } {
   if {![Oc_Option ClassConstructorDone]} {
      set maxwidth 25  ;# Default values in case Oc_Option
      set tscale 1.0   ;# is not initialized.
      set decwidth 100
   } else {
      set maxwidth 0 ;# Default values if Oc_Option
      set tscale 1.0 ;# is initialized
      set decwidth 0
      # 'Oc_Option Get' uses 'catch' to handle the case where the
      # requested option does not exist. However, this muddies the
      # errorInfo stack trace. So, as a courtesy for error handlers,
      # save and restore errorInfo about the 'Oc_Option Get' calls.
      set save_errorInfo $::errorInfo
      Oc_Option Get {} owMaxTitleWidth maxwidth
      Oc_Option Get {} owTitleScaling tscale
      Oc_Option Get {} owDecorationWidth decwidth
      set ::errorInfo $save_errorInfo ;# Clear false error stack
   }
   if {[lsearch -exact [font names] owTitleFont]<0} {
      set titlefont TkCaptionFont  ;# Safety
   } else {
      set titlefont owTitleFont
   }
   set width [font measure $titlefont $title]
   set maxwidth [font measure $titlefont [string repeat M $maxwidth]]
   if {$width>$maxwidth} {
      set width $maxwidth
   }
   set width [expr {int(ceil($width*$tscale))}]
   set width [expr {$width + $decwidth}]
   return $width
}
proc Ow_SetWindowTitle { win title } {
   wm title $win $title
   return [Ow_GetWindowTitleSize $title]
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
    raise $child
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
   if {![Oc_Option ClassConstructorDone]} {
      # NOP if Oc_Option class constructor hasn't run to completion.
      return
   }

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

########################################################################
# Tcl/Tk 8.6.10 (at least) does not recognize screen size changes,
# e.g., when running on an X11 server in a VNC (Virtual Network
# Computing) desktop. (Recent (2022) vncserver/viewer software allows
# the user to interactively change the apparent screen size.) The
# following proc runs a fresh Tcl/Tk shell to interrogate the screen
# dimensions. The return is a three item list of couples:
#
#   { [winfo screenwidth .] [winfo screenheight .] }
#   { [winfo vrootwidth .]  [winfo vrootheight .] }
#   { [wm maxsize .] }
#
# An error is thrown if the Tcl/Tk shell launch fails.
proc Ow_CheckScreenDimensions {} {
   if {[catch {
      set runplatform [Oc_Config RunPlatform]
      set usetcl [$runplatform Tclsh]
      set screenscript {
         package require Tk
         set sdim [list [winfo screenwidth .] [winfo screenheight .] ]
         set vdim [list [winfo vrootwidth .]  [winfo vrootheight .] ]
         set mdim [wm maxsize .]
         puts [list $sdim $vdim $mdim]
         exit
      }
      set dimensions [exec $usetcl << $screenscript]
   } errmsg]} {
      return -code error $errmsg
   }
   return $dimensions
}

########################################################################
# Code to force toplevel window resize to size requested by children.
# Typically this occurs automatically through propagate option of
# pack.  What follows is primarily a hack to workaround buggy window
# managers that don't automatically propagate geometry changes.  The
# proc Ow_PropagateGeometry is a a NOP unless the configuration
# variable bad_geom_propagate is set true in the platform config file.
# To force geometry propagation at a code point, use
# Ow_ForcePropateGeometry, which just calls wm geometry.  (Note: Do
# default definitions first with "proc" in first column so the procs
# get indexed.)
proc Ow_ForcePropagateGeometry { win } {
   wm geometry [winfo toplevel $win] {}
}
proc Ow_PropagateGeometry { w {total_time 0}} {}   ;# NOP
if {![catch {[Oc_Config RunPlatform] GetValue bad_geom_propagate} _] \
        && $_} {
   proc Ow_PropagateGeometry { win } {
      Ow_ForcePropagateGeometry $win
   }
   proc Ow_ForcePropagateGeometry { w {total_time 0}} {
      global ow_propagategeometry_aid
      global ow_propagategeometry_dims ow_propagategeometry_reqdims
      if {[winfo exists $w]} {
         set w [winfo toplevel $w] ;# Work with top level
      }
      if {[info exists ow_propagategeometry_aid($w)]} {
         ;# Insure at most one stream at a time
         after cancel $ow_propagategeometry_aid($w)
      }
      if {![winfo exists $w]} {
         return ;# Kill call cycle
      }
      set setup_time 1000 ;# Time to allow for window setup (milliseconds)
      set setup_incr  200 ;# Time increment during setup period
      set wait_incr 1000  ;# Time between checks after setup period
      set max_time 10000  ;# Max check time
      set dims "[winfo width $w]x[winfo height $w]"
      set reqdims "[winfo reqwidth $w]x[winfo reqheight $w]"
      if {$total_time <= $setup_time \
          || [string compare $dims $ow_propagategeometry_dims($w)]!=0 \
          || [string compare $reqdims $ow_propagategeometry_reqdims($w)]!=0 } {
         # Force update on first pass, or subsequent passes
         # if window geometry changes.
         set ow_propagategeometry_dims($w) $dims
         set ow_propagategeometry_reqdims($w) $reqdims
         wm geometry $w {}
         if {$total_time<$setup_time} {
            set timestep $setup_incr
         } else {
            set timestep $wait_incr
         }
         if {[incr total_time $timestep]<=$max_time} {
            # If geometry update was made and max check time not
            # reached, then schedule later recheck.
            set ow_propagategeometry_aid($w) \
             [after $timestep [list Ow_ForcePropagateGeometry $w $total_time]]
         }
      }
   }
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
# Note 1-Sep-2021: The tkwait commands in this routine allow event
#    loop servicing while waiting, in particular during the command
#       tkwait variable ow(dialog,btncode$instance)
#    which may hang out for a good stretch waiting for human response.
#    If this routine is being used for error processing (e.g., inside
#    Oc_Log), then additional errors can cause additional Ow_Dialog
#    widgets to be displayed. Code which detects and reports errors
#    should therefore be hardened against possible reentrancy.
set ow(dialog,width) [winfo pixels . 12c]
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
    Ow_PositionChild $window $parent
    set dialogwidth [Ow_SetWindowTitle $window $title]
    set brace [canvas ${window}.brace -width $dialogwidth -height 0 \
                 -borderwidth 0 -highlightthickness 0]
    pack $brace -side top
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
    # to enable selections. Match the canvas text color to that used
    # in the (themed) buttons.
    set width [winfo pixels . $width]
    set height [expr {int(ceil(0.5*$width))}]
    set textcolor [ttk::style lookup TButton -foreground]
    set msg [canvas $window.top.msg -confine 1 \
             -borderwidth 2 -relief flat \
             -highlightthickness 0 -selectborderwidth 0]
    if {[lsearch -exact [font names] _ow_dialog_boldfont]<0} {
       # Call Tk 'font' directly, rather than Oc_Font, in case Oc_Font
       # is not yet initialized.
       font create _ow_dialog_boldfont {*}[font configure TkDefaultFont]
       font configure _ow_dialog_boldfont -weight bold
    }
    $msg create text 2 2 -anchor nw -font _ow_dialog_boldfont \
       -text $message -width $width -fill $textcolor -tags text

    update idletasks
    foreach {xmin ymin xmax ymax} [$msg bbox all] {}
    if {$ymax<$height} {
        # No scrollbar
        $msg configure -width $xmax -height $ymax
        $window.top configure -width $xmax -height $height
        pack $msg -side left -fill both -expand 1
        pack $window.top -side top -fill both -expand 1
        global ${msg}_text
        set ${msg}_text $message
        bind $msg <Configure> "+
           $msg delete text
           $msg create text 4 4 -anchor nw -font _ow_dialog_boldfont \
                   -text \$\{${msg}_text\} -width \[expr %w-10\] \
                   -fill $textcolor -tags text"
        bind $msg <Destroy> "+ unset ${msg}_text"
     } else {
        # Crop height and add vertical scrollbar
        if {![regexp -- "\n$" $message]} {
            append message "\n"    ;# Add trailing newline; this ensures
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
           $msg create text 4 4 -anchor nw -font _ow_dialog_boldfont \
                   -text \$\{${msg}_text\} -width \[expr %w-10\] \
                   -fill $textcolor -tags text
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
        if {[catch {Oc_Main GetInstanceName} instancename]} {
           # Protect against initialization errors
           set instancename INIT
        }
        set msg "$instancename Stack:\n"
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
    Ow_PositionChild $window $parent
    set dialogwidth [Ow_SetWindowTitle $window $title]
    set brace [canvas ${window}.brace -width $dialogwidth -height 0 \
                 -borderwidth 0 -highlightthickness 0]
    pack $brace -side top
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
        Oc_DefaultLogMessage $msg $type $src
        return
    }
    if {[catch {clock format [clock seconds]  -format "%T"} tock]} {
       set tock "Unknown time"
    }
    set tock [format {[%s]} $tock]
    if {[catch {Oc_Main GetInstanceName} instancename]} {
       # Protect against initialization errors
       set instancename INIT
    }
    switch $type {
        panic {
            Ow_NonFatalError "$instancename $src panic:\n$msg" \
                    "$instancename $src Panic  $tock" 1 error 1
        }
        error {
            Ow_NonFatalError "$instancename $src error:\n$msg" \
                    "$instancename $src Error  $tock" 1 error
        }
        warning {
            Ow_NonFatalError "$instancename $src warning:\n$msg" \
                    "$instancename $src Warning  $tock" 1 warning
        }
        info {
            Ow_Dialog 1 "$instancename $src Info  $tock" info \
                    "$instancename $src info:\n$msg" {} 0
        }
        default {
            Ow_Dialog 0 "$instancename $src Info  $tock" info \
                    "$instancename $src info:\n$msg" {} 0
        }
    }
}
