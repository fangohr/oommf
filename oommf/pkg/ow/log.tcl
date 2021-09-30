# FILE: log.tcl
#
# Last modified on: $Date: 2016/01/22 23:57:33 $
# Last modified by: $Author: donahue $

Oc_Class Ow_Log {
    public variable memory = 1000
    public variable memory_shrink = 100
    public variable basewinpath = .
    public variable font = {}
    public variable fontsize = {}
    public variable wrap = 0
    const private variable winpath
    const private variable tw
    const private variable xscroll
    const private variable yscroll
    private variable lineslogged
    private variable linesinwidget


    Constructor {args} {
       eval $this Configure $args
       # Don't add '.' suffix if there already is one.
       # (This happens chiefly when basewinpath is exactly '.')
       if {![string match {.} \
                [string index $basewinpath \
                    [expr [string length $basewinpath]-1]]]} {
          append basewinpath {.}
       }

       set winpath ${basewinpath}w$this
       frame $winpath -bd 4 -relief ridge

       set tw [text $winpath.text -height 5 \
                  -exportselection 1 -highlightthickness 0 \
                  -insertwidth 0 -insertofftime 0 -wrap none]
       set xscroll [scrollbar $winpath.xscroll -orient horizontal \
                       -command [list $tw xview]]
       set yscroll [scrollbar $winpath.yscroll -orient vertical \
                       -command [list $tw yview]]
       $tw configure -yscrollcommand [list $yscroll set] \
          -xscrollcommand [list $xscroll set]

       # --- Auto text color selection ---
       set oommf_background [option get . background *]
       if {[llength $oommf_background]>0 \
              && [Ow_GetShade $oommf_background]<128} {
          # User has requested dark colors
          $tw configure -background black
          $tw tag configure ERROR -foreground red -background white
          $tw tag configure WARNING -foreground yellow -background black
          $tw tag configure STATUS -foreground white -background black
          $tw configure -foreground cyan -background black ;# Unrecognized \
                                                            ## message type
       } else {
          # Use default color set
          $tw configure -background white
          $tw tag configure ERROR -foreground yellow -background black
          $tw tag configure WARNING -foreground red -background white
          $tw tag configure STATUS -foreground black -background white
          $tw configure -foreground green -background white ;# Unrecognized \
                                                            ## message type
       }
       $tw tag raise sel ;# Insure selection coloring is visible

       # On Windows, the text widget settings hide the selected text
       # when the window loses focus.  Other apps on Windows don't do
       # this, so change inactive select background to match the active
       # select background.  We make this configuration change
       # Windows-specific; the Mac, for example, changes the select
       # background color depending on whether or not a window has
       # focus, but either way the focus remains visible.
       global tcl_platform
       if {[string compare windows $tcl_platform(platform)]==0} {
          catch {
             $tw configure -inactiveselectbackground [$tw cget -selectbackground]
          }
          # Use catch because option -inactiveselectbackground first
          # appears in Tk 8.5 docs.
       }

       # On entry, font is intended to be a font family; however,
       # after this stanza font holds a font name created by the Tk
       # font command.
       if {[string match {} $font]} {
          set font [font actual [$tw cget -font] -displayof $tw -family]
       }
       if {![regexp {^[1-9][0-9]*$} $fontsize]} {
          set fontsize [font actual [$tw cget -font] -displayof $tw -size]
       }
       set font [font create -family $font -size $fontsize]
       $tw configure -font $font

       set lineslogged 0
       set linesinwidget 0
       pack $yscroll -side right -fill y
       pack $xscroll -side bottom -fill x
       pack $tw -side left -fill both -expand 1
       bind $winpath <Destroy> [list $this WinDestroy %W]
       pack $winpath -side bottom -fill both -expand 1

       $this Wrap $wrap  ;# Don't call Wrap until $yscroll is packed

       # Disable insertions and deletion bindings.  This is based on
       # ideas from wiki.tcl.tk on a read-only text widget.
       rename $tw ${tw}.hidden
       proc $tw {args} [subst -nocommands {
          switch -exact -- [lindex \$args 0] {
             insert  { return }
             delete  { return }
             replace { return }
             ins { set args [lreplace \$args 0 0 insert] }
             del { set args [lreplace \$args 0 0 delete] }
             rep { set args [lreplace \$args 0 0 replace] }
          }
          set cmd [linsert \$args 0 ${tw}.hidden]
          return [eval \$cmd]
       }]
    }

   method AdjustFontSize { delta } { tw font fontsize wrap winpath } {
      if {[llength [$tw dlineinfo {end - 2 chars}]]>0} {
         set follow 1 ;# End is currenty visible
      } else {
         set follow 0 ;# End is not currenty visible
      }
      set signfont 1
      if {$fontsize<0} {
         set fontsize [expr {abs($fontsize)}]
         set signfont -1
      }
      set fontsize [expr {$fontsize + $delta}]
      if {$fontsize < 1}   { set fontsize 1 }
      if {$fontsize > 128 } {set fontsize 128 }
      set fontsize [expr {$signfont * $fontsize}]
      font configure $font -size $fontsize
      $tw configure -font $font
      if {$follow} {
         if {$wrap} {
            $tw see {end - 2 chars lineend}
         } else {
            $tw see {end - 2 chars linestart}
         }
      }
      Ow_PropagateGeometry $winpath
   }

   method EntryPrefix { seconds type } {} {
      if {[catch {clock format $seconds -format "%Y/%m/%d %T"} timestamp]} {
         set timestamp "Unknown time"
      }
      set prefix [format "\[$timestamp\] %-7s " $type]
      return $prefix
   }

   method Wrap { state } { tw xscroll yscroll wrap } {
      if {[llength [$tw dlineinfo {end - 2 chars}]]>0} {
         set follow 1 ;# End is currenty visible
      } else {
         set follow 0 ;# End is not currenty visible
      }
      if {$state} {
         set wrap 1
         set strut {0000}
         if {[catch {font measure [$tw cget -font] \
                         -displayof $tw $strut} ipix]} {
            set ipix [expr {6*[string length $strut]}]
         }
         $tw tag configure all -lmargin2 $ipix
         $tw configure -wrap char
         pack forget $xscroll
         if {$follow} { $tw see {end - 2 chars lineend} }
      } else {
         set wrap 0
         $tw configure -wrap none
         pack $xscroll -side bottom -fill x -after $yscroll
         if {$follow} { $tw see {end - 2 chars linestart} }
      }
   }

    method Append { report } \
        { tw lineslogged linesinwidget memory memory_shrink wrap } {
        # Cleanup routines might try to log messages after the window
        # is destroyed.  Prevent that error.
	if {![winfo exists $tw]} {
            return
        }
        foreach {seconds type msg} $report break
        set type [string toupper $type]
        set prefix [$this EntryPrefix $seconds $type]
        incr lineslogged
        incr linesinwidget
        if {[llength [$tw dlineinfo {end - 2 chars}]]>0} {
           set follow 1 ;# End is currenty visible
        } else {
           set follow 0 ;# End is not currenty visible
        }
        if {$linesinwidget>$memory} {
           $this Purge [expr {$linesinwidget - $memory + $memory_shrink}]
        }
        $tw ins end "$prefix$msg\n" [list all $type]
        if {$follow} {
           if {$wrap} {
              $tw see {end - 2 chars lineend}
           } else {
              $tw see {end - 2 chars linestart}
           }
        }
	### update idletasks   ;# Only uncomment this if *really* needed,
	### because sometimes we want to call this method from inside an
	### event handler.
    }

    private method Purge { numlines } { tw linesinwidget } {
       if {$numlines < 1} { return }
       if {$numlines > $linesinwidget} {
          set numlines $linesinwidget
       }
       $tw del 1.0 "1.0 + $numlines lines"
       set linesinwidget [expr {$linesinwidget - $numlines}]
    }

    method Clear {} { tw linesinwidget } {
       $tw del 1.0 end
       set linesinwidget 0
    }

    method CopySelected {} { tw } {
       # The "-exportselection 1" option in the $tw text widget
       # construction (which is actually the default) automatically sets
       # the "primary" selection, in X11-style, at least on *nix.  This
       # routine can be tied to a key binding (say Ctrl-C) and/or a menu
       # item to force the selected text to be copied to the clipboard,
       # which follows the traditional Windows-style copy and paste
       # sematics --- and increasing expected by *nix users as well.

       # Get selected text
       set selected_txt [$tw tag ranges sel]
       if {[llength $selected_txt]} {
          set txt [eval [linsert $selected_txt 0 $tw get]]
       } else {
          set txt {}
       }

       # Copy to clipboard
       clipboard clear
       clipboard append -- $txt

       # As mentioned above, the primary selection is handled
       # automatically by the text widget, but if you want to override
       # that behavior and keep the primary selection in sync with the
       # clipboard, use "-exportselection 0" in the $tw text widget
       # construction and include here something like
       #    selection clear
       #    selection handle . [list OwLogSelectionHandler $txt]
       #    selection own .
       # where OwLogSelectionHandler looks like
       #    proc OwLogSelectionHandler { str offset maxbytes } {
       #      return [string range $str $offset [expr {$offset+$maxbytes}]]
       #    }
       # Note that you have to do a bit more if you want highlighted
       # text in $tw to automatically de-select when the selection is
       # grabbed elsewhere.
    }

    method WinDestroy { w } { winpath } {
        if {[string compare $winpath $w]} {
            return	;# Child destroy event
        }
        $this Delete
    }

    # Append a line to this log
    Destructor {
        Oc_EventHandler Generate $this Delete
	if {[info exists winpath] && [winfo exists $winpath]} {
            bind $winpath <Destroy> {}
            destroy $winpath
        }
    }

}
