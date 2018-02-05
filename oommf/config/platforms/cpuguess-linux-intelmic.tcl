# FILE: cpuguess-linux-intelmic.tcl
#
# Support procs for linux-intelmic (Intel Phi) platform.


# Routine to guess the Intel C++ version.  The import, icpc, is used
# via "exec $icpc --version" (or, rather, the "open" analogue) to
# determine the icpc version (since the flags accepted by icpc vary by
# version).  Return value is the icpc version string as a list of
# numbers, for example, {10 1} for version 10.1
proc GuessIcpcVersion { icpc } {
    set guess {}
    catch {
	set fptr [open "|$icpc --version" r]
	set verstr [read $fptr]
	close $fptr
        set digstr {[0-9]+\.[0-9.]+}
        set ws "\[ \t\n\r\]"
	regexp -- "(^|$ws)($digstr)($ws|$)" $verstr dummy dum0 guess dum1
    }
    return [split $guess "."]
}

proc GetIcpcBannerVersion { icpc } {
   set banner {}
   catch {
      set fptr [open "|$icpc --version" r]
      set banner [gets $fptr]
      close $fptr
   }
   return $banner
}


# Routines that report optimization flags for icpc.  Right now these
# are mostly placeholders, but they may be expanded and refined in the
# future.
proc GetIcpcGeneralOptFlags { icpc_version } {
   if {0} {
      set opts [list -O2 -no-prec-div -ansi_alias \
                   -fp-model fast=2 -fp-speculation fast \
                   -std=c++11]
   } else {
      set opts [list -std=c++11]
   }
   return $opts
}

# Routine to guess the gcc version.  The import, gcc, is used via "exec
# $gcc --version" (or, rather, the "open" analogue) to determine the
# gcc version (since the flags accepted by gcc vary by version).
# Return value is the gcc version string as a list of numbers,
# for example, {4 1 1} for version 4.1.1.
proc GuessGccVersion { gcc } {
   set guess {}
   if {[catch {
          set fptr [open "|$gcc --version" r]
          set verstr [read $fptr]
      close $fptr}]} {
      return $guess
   }
   set digstr {[0-9]+\.[0-9.]+}
   set datestr {[0-9]+}
   set ws "\[ \t\n\r\]"
   set revision {}
   set date {}
   if {![regexp -- "(^|$ws)($digstr)(${ws}($datestr|)|$)" \
            $verstr dummy dum0 revision dum1 date]} {
      return $guess
   }
   set verno [lrange [split $revision "."] 0 2]
   while {[llength $verno]<3} {
      lappend verno 0   ;# Zero or empty string?
   }
   if {[regexp {(....)(..)(..)} $date dummy year month day]} {
      set verdate [list $year $month $day]
   } else {
      set verdate [list {} {} {}]
   }
   return [concat $verno $verdate]
}
