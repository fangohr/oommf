# Make user requested tweaks to compile line options
proc LocalTweakOptFlags { config opts } {
   if {![catch {$config GetValue program_compiler_c++_remove_flags} noflags]} {
      foreach ropt $noflags {
         set rlen [llength $ropt] ;# Allows matching against multi-part options
         set keepopts {}
         set index 0
         # Match each option one-by-one against remove regexp
         while {$index < [llength $opts]} {
            if {[regexp -- $ropt \
                 [lrange $opts $index [expr {$index+$rlen-1}]]] == 0} {
               # No match
               lappend keepopts [lindex $opts $index]
               incr index
            } else {
               # Match (so remove)
               incr index $rlen
            }
         }
         set opts $keepopts
      }
      regsub -all -- {\s+-} $opts { -} opts  ;# Compress spaces
      regsub -- {\s*$} $opts {} opts
   }
   if {![catch {$config GetValue program_compiler_c++_add_flags} extraflags]} {
      foreach elt $extraflags {
         if {[lsearch -exact $opts $elt]<0} {
            lappend opts $elt
         }
      }
   }
   return $opts
}
