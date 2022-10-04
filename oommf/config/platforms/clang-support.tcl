# FILE: clang-support.tcl
#
# A couple of Tcl procs for use determining compiler options for clang:
#    GuessClangVersion
# which runs clang++ to guess the version of Clang being used.
#    GetClangGeneralOptFlags
# which returns a list of aggressive, non-processor-specific
# optimization flags that can be passed to clang, and
#    GetClangCpuOptFlags
# which returns a list of aggressive, processor-specific
# optimization flags for clang.
#

# Routine to guess the Clang version.  The import, clang, should be
# an executable command list, which is used via
#   exec [concat $clang --version]
# or, rather, the "open" analogue to determine the clang version (useful
# for when the flags accepted by clang vary by version).  In particular,
# this means that spaces in the clang cmd need to be protected
# appropriately.  For example, if there is a space in the clang command
# path, then clang should be set like so:
#  set clang [list {/path with spaces/clang++}]
# For multi-element commands, for example using the xcrun shim from Xcode,
# this would look like
#  set clang [list xcrun {/path with spaces/clang++}]
#
# The return value is the clang version string as a list of numbers,
# for example, {3 1} for version 3.1.
proc GuessClangVersion { clang } {
   set guess {}
   set testcmd [concat | $clang --version]
   if {[catch {
          set fptr [open $testcmd r]
          set verstr [read $fptr]
      close $fptr}]} {
      return $guess
   }
   set digstr {[0-9]+\.[0-9.]+}
   set ws "\[ \t\n\r\]"
   set revision {}
   if {![regexp -- "(^|$ws)($digstr)(${ws}|$)" \
            $verstr dummy dum0 revision dum1]} {
      return $guess
   }
   set verno [lrange [split $revision "."] 0 1]
   while {[llength $verno]<2} {
      lappend verno 0   ;# Zero or empty string?
   }
   return [concat $verno]
}

proc GetClangBannerVersion { clang } {
   set banner {}
   set testcmd [concat | $clang --version]
   catch {
      set fptr [open $testcmd r]
      set banner [gets $fptr]
      while {[gets $fptr line]>=0} {
	 if {[string length $line]} {
	    append banner " / $line"
	 }
      }
      close $fptr
   }
   return [string trim $banner]
}

# Routine that returns aggressive, processor agnostic, optimization
# flags for clang.  The import is the clang version, as returned by the
# preceding GuessClangVersion proc.  Note that the flags accepted by
# clang may vary by version.  This proc is called by
# GetClangValueSafeFlags; changes here may require adjustments there.
#
# Use a command like
#   echo 'int;' | clang -xc -Ofast - -o /dev/null -\#\#\#
# to see what flags combination options like -Ofast enable.
#
proc GetClangGeneralOptFlags { clang_version optlevel } {
   # At present state of development, this proc ignores the
   # clang_version import.
   # NB: In at least some versions of clang (such as the one for the
   # original Mavericks release) the apparently undocumented option
   # -fomit-frame-pointer causes problems with exception handling
   # through class constructors, and apparently some problems with the
   # map STL container.  So just leave it out, for now.

   # Note: -Ofast enables -ffast-math, which includes non-value-safe
   # optimizations.  If this causes problems replace -Ofast with -O3.
   set opts -std=c++11
   if {$optlevel <= 0} {
      lappend opts -O0
   } elseif {$optlevel == 1} {
      lappend opts -O1
   } elseif {$optlevel == 2} {
      lappend opts -O2
   } else {
      lappend opts -Ofast
   }
   return $opts
}

# Routine that returns optimization flags for gcc which are floating
# point value-safe.  The import is the gcc version, as returned by the
# preceding GuessGccVersion proc.  Note that the flags accepted by gcc
# vary by version.
#
proc GetClangValueSafeOptFlags { clang_version optlevel } {
   set opts [GetClangGeneralOptFlags $clang_version $optlevel]
   if {[set index [lsearch -exact $opts -Ofast]]>=0} {
         set opts [lreplace $opts $index $index -O3]
   }
   return $opts
}

# Routine that determines processor specific optimization flags for
# Clang.  The first import is the clang version, as returned by the
# GuessClangVersion proc (see above).  Note that the flags accepted by
# clang may vary by version.  The second import, cpu_arch, should
# match output from the GuessCpu proc in x86-support.tcl.  Return
# value is a list of clang flags.
proc GetClangCpuOptFlags { clang_version cpu_arch } {
   lappend opts -march=native
   return $opts
}
