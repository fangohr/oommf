# FILE: cpuguess-lintel.tcl
#
# One Tcl proc for use on Linux x86 systems:
#    GuessCpu
# to guess the CPU model.
#   Additionally, this script sources x86-support.tcl, which defines
# these five Tcl procs:
#    GuessCpuArch_VendorFamilyModel
# which returns a cpu-arch list, {vendor type sse-level} given the
# vendor, processor family and model id numbers.
#    GuessCpuArch_NameStr
# which produces a cpu-arch list like the previous proc, but using
# the "model name" string.
#    GuessGccVersion
# which runs gcc to guess the version of GCC being used.
#    GetGccGeneralOptlags
# which returns a list of aggressive, non-processor-specific
# optimization flags that can be passed to gcc.
#    GetGccCpuOptFlags
# which returns a list of processor-specific optimization flags that
# can be passed to gcc
#    Find_SSE_Level
# which determines the SSE type level /proc/cpuinfo
#    Find_FMA_type
# which determines the FMA type from /proc/cpuinfo
#
# The GuessCpu proc is mostly a wrapper around the
# GuessCpuArch_VendorFamilyModel proc in x86-support.tcl, except that
# the SSE level is deduced directly from the /proc/cpuinfo "flags"
# line.
#   See the x86-support.tcl file for information about the other procs.
#
# This file is sourced by the lintel.tcl, linux-x86_64.tcl, cygtel.tcl,
# cpuguess.tcl, cpuguess-linux-x86_64.tcl, cpuguess-cygtel.tcl scripts.
#
########################################################################

# Support procs
source [file join [file dirname [info script]] x86-support.tcl]

proc Find_SSE_Level { {flags {}} } {
   if {[string match {} $flags]} {
      # Read and organize cpu info from /proc/cpuinfo
      set chan [open "/proc/cpuinfo" r]
      set data [read $chan]
      close $chan
      regsub "\n\n.*" $data {} cpu0
      regsub -all "\[ \t]+" $cpu0 { } cpu0
      regsub -all " *: *" $cpu0 {:} cpu0
      regsub -all " *\n *" $cpu0 "\n" cpu0
      array set cpuarr [split $cpu0 ":\n"]
      set flags $cpuarr(flags)
   }

   # Determine SSE level from settings in $flags.  For now,
   # don't try to distinguish between the various sub-levels,
   # e.g., map ssse3 -> sse3; sse4_1, sse4_2, sse4a -> sse4
   if {[lsearch $flags sse4*]>=0 || [lsearch $flags nni]} {
      set sse_level 4
   } elseif {[lsearch $flags  sse3]>=0 ||
             [lsearch $flags   pni]>=0 ||
             [lsearch $flags ssse3]>=0 ||
             [lsearch $flags   mni]>=0 ||
             [lsearch $flags   tni]>=0} {
      set sse_level 3
   } elseif {[lsearch $flags sse2]>=0} {
      set sse_level 2
   } elseif {[lsearch $flags sse]>=0} {
      set sse_level 1
   } else {
      set sse_level 0
   }
   return $sse_level
}

proc Find_FMA_Type { {flags {}} } {
   if {[string match {} $flags]} {
      # Read and organize cpu info from /proc/cpuinfo
      set chan [open "/proc/cpuinfo" r]
      set data [read $chan]
      close $chan
      regsub "\n\n.*" $data {} cpu0
      regsub -all "\[ \t]+" $cpu0 { } cpu0
      regsub -all " *: *" $cpu0 {:} cpu0
      regsub -all " *\n *" $cpu0 "\n" cpu0
      array set cpuarr [split $cpu0 ":\n"]
      set flags $cpuarr(flags)
   }
   if {[lsearch $flags fma]>=0} {
      set fma_type 3
   } elseif {[lsearch $flags fma4]>=0} {
      set fma_type 4
   } else {
      set fma_type 0
   }
}

# Routine to guess the CPU using /proc/cpuinfo provided by Linux kernel
# Output is a three element list.
# The first element is the vendor, which will be one of:
#     unknown, intel, or amd.
# The second element is the cpu type, which will be one of:
#     unknown,
#     i386, i486, pentium, pentium-mmx, pentiumpro, pentium2,
#     pentium3, pentium-m, core, core2
#     pentium4, prescott, nocona, pentium-d,
#     k5, k6, k6-2, k6-3, athlon, athlon-4, k8
# The third element is the supported SSE level, which will be one of:
#     0, 1, 2, 3, 4
#
# This proc is mostly a wrapper around the GuessCpuArch_VendorFamilyModel
# proc in x86-support.tcl, except that the SSE level is deduced directly
# from the /proc/cpuinfo "flags" line.

proc GuessCpu {} {
   set vendor  unknown
   set cputype unknown
   set sselevel 0

   if {[catch {
      if {[file readable /proc/cpuinfo]} {
         set fptr [open /proc/cpuinfo r]
         set cpustr [read $fptr]
         close $fptr
      } else {
         # If we are running under cygwin, then we might
         # not be able to see /proc/cpuinfo directly
         # but still be able to set cat on it.
         set cpustr [exec cat /proc/cpuinfo]
      }
      # Look at first processor info only
      set tmplst [split $cpustr "\n"]
      set record_end [lsearch $tmplst {}]
      if {$record_end>0} {
         set tmplst [lreplace $tmplst $record_end end]
      }
      set cpustr [join $tmplst ":"]
      foreach {x y} [split $cpustr ":"] {
         set cpuinfo([string trim $x]) [string tolower [string trim $y]]
      }

      if {[info exists cpuinfo(vendor_id)]} {
         set vendor $cpuinfo(vendor_id)
      } elseif {[info exists cpuinfo(model\ name)]} {
         set vendor $cpuinfo(model\ name)
      }

      set cpu_family {}
      if {[info exists cpuinfo(cpu\ family)]} {
         set cpu_family $cpuinfo(cpu\ family)
      }

      set cpu_model {}
      if {[info exists cpuinfo(model)]} {
         set cpu_model $cpuinfo(model)
      }

      foreach {vendor cputype sselevel} \
         [GuessCpuArch_VendorFamilyModel $vendor $cpu_family $cpu_model] {
            break
         }

      if {[string match "unknown" $cputype]} {
         # Otherwise, try to match against "model name"
         foreach {vendor cputype sselevel} \
            [GuessCpuArch_NameStr $cpuinfo(model\ name)] { break }
      }

      if {[info exists cpuinfo(flags)]} {
         set sselevel [Find_SSE_Level $cpuinfo(flags)]
      }
   } err]} {
      puts stderr "CODING ERROR: $err"
   }
   return [list $vendor $cputype $sselevel]
}


