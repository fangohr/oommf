# FILE: font.tcl
#
# Maps OOMMF-specified symbolic names to system-specific fonts, using
# Tk "font" command (introduced in Tcl 8.0) and the Tk "Standard Fonts"
# (introduced in Tcl 8.5).
#
# Symbolic font names:
#        default       --- TkDefaultFont
#        text          --- TkTextFont
#        small_caption --- TkSmallCaptionFont
#        caption       --- TkCaptionFont
#        fixed         --- TkFixedFont
#
#        standard      --- non-bold version of default
#        bold          --- bold version of standard
#        small         --- non-bold version of small_caption*
#        small_bold    --- bold version of small
#        large         --- non-bold version of caption*
#        large_bold    --- bold version of large
#
# If small_caption is not smaller than default, then small is
# instead made a copy of default resized by a factor of 10/12.
# If caption is not larger than default, then large is
# instead made a copy of default resized by a factor of 14/12.
#
# Last modified on: $Date: 2007/03/21 01:02:40 $
# Last modified by: $Author: donahue $
#
Oc_Class Oc_Font {

   private array common font

   public common oommf_font_names {
      default text small_caption caption fixed \
         standard bold small small_bold large large_bold
   }

   ClassConstructor {
      if {![Oc_Main HasTk]} {
         error "$class requires Tk"
      }
      # Tk fonts
      set font(default)       TkDefaultFont
      set font(text)          TkTextFont
      set font(small_caption) TkSmallCaptionFont
      set font(caption)       TkCaptionFont
      set font(fixed)         TkFixedFont

      # OOMMF fonts
      set font(standard) [font create {*}[font configure $font(default)]]
      font configure $font(standard) -weight normal
      set font(bold) [font create {*}[font configure $font(standard)]]
      font configure $font(bold) -weight bold
      set stdsize [Oc_Font GetSize standard]

      set font(small) [font create {*}[font configure $font(small_caption)]]
      font configure $font(small) -weight normal
      set smallsize [Oc_Font GetSize small]
      if {$smallsize >= $stdsize} {
         set smallsize [expr {int(round($stdsize*10./12.))}]
         font configure $font(small) -size $smallsize
      }
      set font(small_bold) [font create {*}[font configure $font(small)]]
      font configure $font(small_bold) -weight bold
      
      set font(large) [font create {*}[font configure $font(caption)]]
      font configure $font(large) -weight normal
      set largesize [Oc_Font GetSize large]
      if {$largesize <= $stdsize} {
         set largesize [expr {int(round($stdsize*14./12.))}]
         font configure $font(large) -size $largesize
      }
      set font(large_bold) [font create {*}[font configure $font(large)]]
      font configure $font(large_bold) -weight bold
   }

   Constructor {args} {
      error "Instances of $class are not allowed"
   }

   proc GetSize { symname } {
      set fontname [Oc_Font Get $symname]
      set size [font configure $fontname -size]
      if {$size==0} {
         set size -12  ;# Punt.  This could probably
         ## be made more accurate if necessary.
      }
      if {$size<0} {
         set size [expr {-1.0*$size/[tk scaling]}]
      }
      return $size
   }

   proc Get { symname } {
      if {[catch {set font($symname)} result]} {
         error "Unknown symbolic fontname: $symname"
      }
      return $result
   }

   proc DumpFontInfo {} {
      # For debugging.
      set msg "OOMMF fonts:\n"
      foreach k $oommf_font_names {
         set i $font($k)
         lappend oommf_fonts $i
         append msg [format "%20s:" $k]
         foreach j [font configure $i] {
            append msg [format " %10s" $j]
         }
         append msg "\n"
      }
      append msg "\nSystem fonts:\n"
      foreach i [lsort [font names]] {
         if {[string match Tk* $i] \
                || [lsearch -exact $oommf_fonts $i]<0} {
            append msg [format "%20s:" $i]
            foreach j [font configure $i] {
               append msg [format " %10s" $j]
            }
            append msg "\n"
         }
      }
      return $msg
   }

   proc SampleText { win {txt {}}} {
      # For debugging: Creates toplevel window with path $win and fills
      # it with some sample text for each oommf_font_name
      if {[catch {toplevel $win} msg]} {
         return -code error "Unable to create topleve window $win: $msg"
      }
      pack [label ${win}.title -text "--- OOMMF Fonts ---" \
               -font TkHeadingFont] -side top
      if {[string match {} $txt]} {
         set txt "The quick brown dog jumps over the lazy fox."
      }
      foreach k $oommf_font_names {
         set f [frame ${win}.${k}]
         label ${f}.name -text [format "Font %13s: " $k] \
            -font TkFixedFont -anchor e
         label ${f}.val -text $txt -font [Oc_Font Get $k] -anchor w
         pack ${f}.name ${f}.val -side left
         pack $f -side top -fill x
      }
   }

}
