# Make user requested tweaks to compile line options
;proc RemoveFlags { opts noflags } {
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
   return $opts
}
;proc AddFlags { opts extraflags } {
   foreach elt $extraflags {
      if {[lsearch -exact $opts $elt]<0} {
         lappend opts $elt
      }
   }
   return $opts
}

proc LocalTweakOptFlags {config opts} {
   if {![catch {$config GetValue program_compiler_c++_remove_flags} noflags]} {
      set opts [RemoveFlags $opts $noflags]
   }
   if {![catch {$config GetValue program_compiler_c++_add_flags} extraflags]} {
      set opts [AddFlags $opts $extraflags]
   }
   return $opts
}

proc LocalTweakValueSafeOptFlags {config valuesafeopts} {
   if {![catch {$config GetValue program_compiler_c++_remove_valuesafeflags} \
            noflags]} {
      set valuesafeopts [RemoveFlags $valuesafeopts $noflags]
   }
   if {![catch {$config GetValue program_compiler_c++_add_valuesafeflags} \
             extraflags]} {
      set valuesafeopts [AddFlags $valuesafeopts $extraflags]
   }
   return $valuesafeopts
}
