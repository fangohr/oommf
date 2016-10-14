# Make user requested tweaks to compile line options
proc LocalTweakOptFlags { config opts } {
   if {![catch {$config GetValue program_compiler_c++_remove_flags} noflags]} {
      foreach ropt $noflags {
         set keepopts {}
         foreach copt $opts {
            # Match each option one-by-one against remove regexp
            if {[regexp -- $ropt $copt] == 0} {
               lappend keepopts $copt  ;# No match
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
