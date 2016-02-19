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

# Routine to guess the Clang version.  The import, clang, is used via "exec
# $clangc --version" (or, rather, the "open" analogue) to determine the
# clang version (useful for whene the flags accepted by clang vary by version).
# Return value is the clang version string as a list of numbers,
# for example, {3 1} for version 3.1.
proc GuessClangVersion { clang } {
   set guess {}
   if {[catch {
          set fptr [open "|$clang --version" r]
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
   catch {
      set fptr [open "|$clang --version" r]
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
# flags for clang.  The import is the clang version, as returned by
# the preceding GuessClangVersion proc.  Note that the flags accepted
# by clang may vary by version.
#
proc GetClangGeneralOptFlags { clang_version } {
   # At present state of development, this proc ignores the
   # clang_version import.  Option set is taken from the Clang
   # 3.1 man page.
   # NB: In at least some versions of clang (such as the one for the
   # original Mavericks release) the apparently undocumented option
   # -fomit-frame-pointer causes problems with exception handling
   # through class constructors, and apparently some problems with the
   # map STL container.  So just leave it out, for now.

   lappend opts -O3

   return $opts
}

# Routine that determines processor specific optimization flags for
# Clang.  The first import is the clang version, as returned by the
# GuessClangVersion proc (see above).  Note that the flags accepted by
# clang may vary by version.  The second import, cpu_arch, should
# match output from the GuessCpu proc in x86-support.tcl.  Return
# value is a list of clang flags.
proc GetClangCpuOptFlags { clang_version cpu_arch } {
   # None at this time, but track "-march=native".
    return {}
}
