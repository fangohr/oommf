# FILE: cpuguess-linux-ppc64.tcl
#
# This file defines a Tcl proc for use on Linux PowerPC 64 systems:
#    GuessCpu
# to guess the CPU model, and also the proc
#    GetGccCpuOptFlags
# which returns a list of processor-specific optimization flags that
# can be passed to gcc.
#
# Additionally, this script sources gcc-support.tcl that provides
# procs for determining compiler versions and non-processor-specific
# compiler optimization flags.
#
# This file is sourced by the linux-ppc64le.tcl script.
#
########################################################################

########################################################################
### Generic gcc support
source [file join [file dirname [info script]] gcc-support.tcl]

########################################################################
# Routine to guess the CPU using /proc/cpuinfo provided by Linux kernel,
# which looks like:
#  processor       : 0
#  cpu             : POWER9, altivec supported
#  clock           : 3783.000000MHz
#  revision        : 2.2 (pvr 004e 1202)
proc GuessCpu {} {
   set cpu_type unknown
   if {[catch {
      if {[file readable /proc/cpuinfo]} {
         set fptr [open /proc/cpuinfo r]
         set cpustr [read $fptr]
         close $fptr
      }
      # Look at first processor info only
      set tmplst [split $cpustr "\n"]
      set cpu_record [lsearch -regexp $tmplst "^cpu\[ \t:\]"]
      if {$cpu_record>=0} {
         set cpu_record [lindex $tmplst $cpu_record]
         regexp {^cpu.*: *([^,]*)} $cpu_record dummy cpu_type
      }
   } err]} {
      puts stderr "CODING ERROR: $err"
   }
   return [list $cpu_type]
}

########################################################################
# Routine that determines processor specific optimization flags for gcc
# for x86 processors.  The first import is the gcc version, as returned
# by the GuessGccVersion proc.  Note that the flags accepted by gcc vary
# by version.  The second import, cputype, should match output from the
# GuessCpu proc above.  Return value is a list of gcc flags.
proc GetGccCpuOptFlags { gcc_version cpu_type } {
   global tcl_platform

   set cpuopts {}

   if {[llength $gcc_version]<2} {
      # Unable to determine gcc version.  Return an empty string.
      return {}
   }
   set verA [lindex $gcc_version 0]
   set verB [lindex $gcc_version 1]

   if {![regexp -- {[0-9]+} $verA] || ![regexp -- {[0-9]+} $verB]} {
      return -code error "Invalid input:\
         gcc_version should be a list of numbers, not\
         \"$gcc_version\""
   }

   set power_level 0
   set power_suffix {}
   regexp -nocase {^power([0-9]+)([^,]*)} $cpu_type \
      dummy power_level power_suffix
   set power_suffix [string tolower $power_suffix]
   if {$verA>=8} {
      lappend cpuopts [list -mcpu=native]
   } elseif {$power_level>=3} {
      # GCC 7.4.0 supports power3 through power9.  Some of the older
      # versions of GCC support power and power2, but those levels are
      # ignored here.
      set flag ${power_level}${power_suffix}
      if {$verA>=6 && $power_level>9} {
         set flag 9
      } elseif {($verA>=5 || ($verA==4 && $verB>=8)) && $power_level>8} {
         set flag 8
      } elseif {$verA==4 && $verB>=4 && $power_level>7} {
         set flag 7
      } elseif {$verA==4 && $verB>=1 && $power_level>=6} {
         set flag 6  ;# 4.3.6 supports 6x, but 4.2.4 and 4.1.2 only 6
      } elseif {($verA>=4 || ($verA>=3 && $verB>=4)) && $power_level>5} {
         set flag 5
      } elseif {$verA<3 || ($verA==3 && $verB<4)} {
         set flag {}
      }
      if {![string match {} $flag]} {
         lappend cpuopts [list -mcpu=power${flag}]
      }
   }

   return $cpuopts
}

proc GetGccLinkOptFlags { gcc_version } {
   set opts "-lpthread"
   return $opts
}


########################################################################
########################################################################

# Routines to obtain version from Portland Group pgc++ compiler.
proc GuessPgccVersion { pgcc } {
   set guess {}
   catch {
      set fptr [open "|$pgcc --version" r]
      set verstr [string trim [read $fptr]]
      close $fptr
      set verstr [lindex [split $verstr "\n"] 0]
      set digstr {[0-9]+\.[0-9.-]+}
      set ws "\[ \t\n\r\]"
      regexp -- "(^|$ws)($digstr)($ws|$)" $verstr dummy dum0 guess dum1
   }
   return [split $guess ".-"]
}

proc GetPgccBannerVersion { pgcc } {
   set banner {}
   catch {
      set fptr [open "|$pgcc --version" r]
      set verstr [string trim [read $fptr]]
      close $fptr
      set banner [lindex [split $verstr "\n"] 0]
   }
   return $banner
}

proc GetPgccGeneralOptFlags { pgcc_version } {
   # Note: If flags -mp and --exceptions are both used, the latter must
   # come later in the option sequence.
   set opts [list -std=c++11]
   if {[lindex $pgcc_version 0]<=16} {
       lappend opts --exceptions
   }
   return $opts
}
proc GetPgccValueSafeOptFlags { pgcc_version cpu_arch} {
   set opts  [list -O2 -Kieee -Mnofprelaxed -Mnofpapprox -Mvect=noassoc]
   return $opts
}
proc GetPgccFastOptFlags { pgcc_version cpu_arch } {
   # CPU model architecture specific options.
   set opts [list -fast -Knoieee]
   return $opts
}

proc GetPgccLinkOptFlags { pgcc_version } {
   # Linker flags
   set opts {-Mnoeh_frame}
   ## Without -Mnoen_frame, threads in Oxs abort during
   ## Tcl_ExitThread() processing with the error "terminate called
   ## without an active exception" if the thread code contains a
   ## try/catch block.  (At least for pgc++ version 16.10-0.)
   if {[lindex $pgcc_version 0]<=16} {
       lappend opts -latomic
   }
   return $opts
}
