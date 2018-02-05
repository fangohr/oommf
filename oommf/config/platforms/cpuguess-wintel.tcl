# FILE: cpuguess-wintel.tcl
#
# Three Tcl procs for use on 32-bit Windows x86 systems:
#    GuessCpu
# to guess the CPU model,
#    GuessClVersion, GetClBannerVersion
# to guess the version of Microsoft Visual C++ being used,
#    GetClFlags
# which returns a list of processor-specific optimization flags that
# can be passed to cl,
#    GetBcc32BannerVersion
# to get the banner version information for bcc32.
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
# can be passed to gcc.
#
#  The GuessCpu proc is a wrapper around GuessCpuArch_VendorFamilyModel.
# This wrapper pulls the necessary Vendor, Family, Model information
# from the "PROCESSOR_IDENTIFIER" environment.
#  The Get*Version procs exec 'gcc --version' and 'cl', respectively,
# to deduce the compiler versions.
#  The Get*Flags procs take as imports the output from the corresponding
# Guess*Version; the Get*CpuOptFlags takes additionally the output from
# from GuessCpu.  Note that the options accepted by the compilers can
# vary between compiler versions.
#
# Sourced by the wintel.tcl and cpuguess.tcl scripts.
#
########################################################################

# Support procs
source [file join [file dirname [info script]] x86-support.tcl]

# Routine to guess the CPU using the "PROCESSOR_IDENTIFIER" environment
# variable. Output is a three element list.
# The first element is the vendor, which will be one of:
#     unknown, intel, or amd.
# The second element is the cpu type, which will be one of:
#     unknown,
#     i386, i486, pentium, pentium-mmx, pentiumpro, pentium2,
#     pentium3, pentium-m, core, core2
#     pentium4, prescott, nocona, pentium-d,
#     k5, k6, k6-2, k6-3, athlon, athlon-4, k8
# The third element is the supported SSE level, which will be one of:
#     0, 1, 2, 3
#
# This is a wrappter around the GuessCpuArch_VendorFamilyModel proc
# in x86-support.tcl.

proc GuessCpu {} {
   set vendor     unknown
   set cpu_family {}
   set cpu_model  {}

   global env

   if {[info exists env(PROCESSOR_IDENTIFIER)]} {
      set cpustr [string tolower $env(PROCESSOR_IDENTIFIER)]
      regexp -- {family ([0-9]+)} $cpustr dummy cpu_family
      regexp -- {model ([0-9]+)} $cpustr dummy cpu_model
      if {[string match "*intel*" $cpustr]} {
         set vendor intel
      } elseif {[string match "*amd*" $cpustr]} {
         set vender amd
      }
   }
   if {[string match {} $cpu_family] && [info exists env(PROCESSOR_LEVEL)]} {
      set cpu_family $env(PROCESSOR_LEVEL)
   }
   if {[string match {} $cpu_model] && [info exists env(PROCESSOR_REVISION)]} {
      if {[scan $env(PROCESSOR_REVISION) "%x" cpu_revision]==1} {
         set cpu_model [expr {($cpu_revision >> 8) & 0xFF}]
      }
   }

   return [GuessCpuArch_VendorFamilyModel $vendor $cpu_family $cpu_model]
}

# Routine to guess the cl version
proc GuessClVersion { cl } {
    set guess {}
    catch {exec $cl} usage_str
    if {[regexp -- {Version ([0-9]+)[.][0-9]+[.][0-9]+} \
             $usage_str dummy version]} {
        set guess [expr {$version - 6}]
    }
    return [split $guess "."]
}
proc GuessClVersion { cl } {
    set guess {}
    catch {exec $cl} usage_str
    if {[regexp -- {Version ([0-9]+)[.][0-9]+[.][0-9]+} \
             $usage_str dummy version]} {
       if {$version<19} {
          set guess [expr {$version - 6}]
       } else {
          set guess [expr {$version - 5}]
       }
       # NB: There is no version 13
    }
    return [split $guess "."]
}
proc GetClBannerVersion { cl } {
   set banner {}
   catch {exec $cl} usage_str
   regexp -- "\[^\n]*Version \[0-9.]+ \[^\n]*" $usage_str banner
   return $banner
}
proc GetBcc32BannerVersion { bcc32 } {
   set banner {}
   catch {exec $bcc32 -h} msg
   regexp -- "(Borland C\\+\\+ \[0-9.]+ \[^\n]*) Copyright " $msg dummy banner
   return $banner
}

# Optimization flags in play for the following two procs
# (GetClGeneralOptFlags and GetClCpuOptFlags):
# VC++ 6 (1998) and earlier are not supported.
#
# Options for VC++ 7.0 (2002), 7.1 (2003):
#            Disable optimizations: /Od
#             Maximum optimization: /Ox
#      Enable runtime debug checks: /GZ
#   Optimize for Pentium processor: /G5
#         Optimize for Pentium Pro: /G6
#
# Options for VC++ 8.0 (2005), 9.0 (2008):
#                  Disable optimizations: /Od
#                   Maximum optimization: /Ox
#                    Enable stack checks: /GZ
#                    Require SSE support: /arch:SSE
#                   Require SSE2 support: /arch:SSE2
# Fast (less predictable) floating point: /fp:fast
#     Use portable but insecure lib fcns: /D_CRT_SECURE_NO_DEPRECATE
#
# Options for VC++ 10 (2010):
#                  Disable optimizations: /Od
#                   Maximum optimization: /Ox
#                    Enable stack checks: /GZ
#       Select processor to optimize for: /favor:<blend|AMD64|INTEL64> (64-bit only)
#                     Enable SSE support: /arch:SSE   (32-bit only)
#                    Enable SSE2 support: /arch:SSE2  (32-bit only)
#                     Enable AVX support: /arch:AVX
# Fast (less predictable) floating point: /fp:fast
#     Use portable but insecure lib fcns: /D_CRT_SECURE_NO_DEPRECATE
#

# Routine that returns aggressive, processor agnostic, optimization
# flags for the Microsoft Visual C++ compiler cl.  The import is the
# cl version, as returned by the preceding GuessClVersion proc.
#
proc GetClGeneralOptFlags { cl_version platform } {
   # Import "platform" not presently used, but should
   # be one of "x86" or "x86_64".
   set cl_version [lindex $cl_version 0] ;# Safety

   # Max optimization
   set opts "/Ox"
   if {$cl_version>7} {
      lappend opts /fp:fast
   }

   return $opts
}

# Routine that determines processor specific optimization flags for
# Microsoft Visual C++ compiler cl.  The first import is the compiler
# version, as returned by the preceding GuessClVersion proc.  The
# second import, cpu_arch, should match output from the GuessCpu proc
# above.  Return value is a list of cl flags.
#
proc GetClCpuOptFlags { cl_version cpu_arch platform } {
   # Import "platform" should be one of "x86" or "x86_64".
   set opts {}

   set mvcpp_version [lindex $cl_version 0]
   if {![regexp {[1-9][0-9]*} $mvcpp_version] || \
       $mvcpp_version<7} {
      # Unknown version or unsupported version
      return $opts
   }

   # Extract cpu information from cpu_arch import
   set cpu_vendor "unknown"
   set cpu_type   "unknown"
   set cpu_sse    0
   foreach {cpu_vendor cpu_type cpu_sse cpu_avx} $cpu_arch { break }

   if {$mvcpp_version<8} {
      switch -glob -- $cpu_type {
         pentium     -
         pentium-mmx { lappend opts "/G5"}
         pentium*    -
         prescott    -
         nocona      -
         core*       { lappend opts "/G6" }
         k5          { lappend opts "/G5"}
         k6*         -
         athlon*     -
         opteron     -
         k8          { lappend opts "/G6" }
      }
   } elseif {$mvcpp_version<10} {
      # Microsoft Visual C++ version 8 (2005) or 9 (2008)
      if {$cpu_sse==1} {
         lappend opts /arch:SSE
      } elseif {$cpu_sse>=2} {
         lappend opts /arch:SSE2
      }
   } else {
      # Microsoft Visual C++ version 10 (2010) or later
      if {[string compare "x86" $platform]==0} {
         # 32-bit
         if {$cpu_avx>0} {
            # CPU supports AVX, but OOMMF doesn't use AVX at this time,
            # and the context switch between AVX and SSE can be
            # expensive, so keep arch at SSE2.
            lappend opts /arch:SSE2   ;# /arch:AVX
         } elseif {$cpu_sse>=2} {
            lappend opts /arch:SSE2
         } elseif {$cpu_sse==1} {
            lappend opts /arch:SSE
         }
      } elseif {[string compare "x86_64" $platform]==0} {
         # On x86_64, SSE2 is assumed and there is no /arch:SSE2 switch.
         # The /arch:AVX switch is supported, but OOMMF doesn't use AVX
         # at this time, and the context switch between AVX and SSE can
         # be expensive.  So for now leave off /arch:AVX
         # if {$cpu_avx>0} { lappend opts /arch:AVX }
         if {[string match "intel*" $cpu_vendor]} {
            lappend opts /favor:INTEL64
         } elseif {[string match "amd*" $cpu_vendor]} {
            lappend opts /favor:AMD64
         }
      } else {
         error "Unrecognized platform name \"$platform\";\
                should be either \"x86\" or \"x86_64\""
      }
   }

   return $opts
}
