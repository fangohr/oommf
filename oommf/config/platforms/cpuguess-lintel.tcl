# FILE: cpuguess-lintel.tcl
#
# This file defines a Tcl proc for use on Linux x86 systems,
#    GuessCpu
# to guess the CPU model.
#   Additionally, this script sources x86-support.tcl, which defines
# the following Tcl procs:
#    GuessCpuArch_VendorFamilyModel
# which returns a cpu-arch list, {vendor type sse-level} given the
# vendor, processor family and model id numbers.
#    GuessCpuArch_NameStr
# which produces a cpu-arch list like the previous proc, but using
# the "model name" string.
#    GuessGccVersion
# which runs gcc to guess the version of GCC being used.
#    GetGccGeneralOptFlags
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


