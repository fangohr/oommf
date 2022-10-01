# FILE: x86-support.tcl
#
# Several Tcl procs for use on x86-based systems:
#    GuessCpuArch_VendorFamilyModel
# which returns a cpu-arch list, {vendor type sse-level} given the
# vendor, processor family and model id numbers.
#    GuessCpuArch_NameStr
# which produces a cpu-arch list like the previous proc, but using
# the "model name" string.  (Avoid using this proc.)
#    GetGccCpuOptFlags
# which returns a list of processor-specific optimization flags that
# can be passed to gcc.
#
# Additionally, this file sources gcc-support.tcl, which provides
#    GuessGccVersion
# which runs gcc to guess the version of GCC being used.
#    GetGccGeneralOptFlags
# which returns a list of aggressive, non-processor-specific
# optimization flags that can be passed to gcc.
#
# NOTE: I don't know any definitive way to ascertain sse level for
#  the AMD k8 chips (i.e., sse2 versus sse3) based on just the
#  family and model numbers.  On linux-based systems, it is safer
#  and easier to get this information from the "flags" output of
#  /proc/cpuinfo ("pni" (Prescott New Instructions) means sse3) than
#  the cpu-arch result from either GuessCpuArch routine.
#
########################################################################

########################################################################
### Generic gcc support
source [file join [file dirname [info script]] gcc-support.tcl]

### Generic Clang support
source [file join [file dirname [info script]] clang-support.tcl]

########################################################################
# Routines to guess the CPU architecture. Output is a four element list.
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
# The fourth element is the supported AVX level, which will be one of:
#     0, 1
#

proc GuessCpuArch_VendorFamilyModel { vendor cpu_family cpu_model } {
   set vendor  [string tolower $vendor]
   if {[string match "*intel*" $vendor]} {
      set vendor intel
   } elseif {[string match "*amd*" $vendor]} {
      set vendor amd
   } else {
      set vendor unknown
   }

   set cputype unknown
   set sselevel 0
   set avxlevel 0

   if {[string match intel $vendor]} {
      if {$cpu_family < 4} {
         set cputype i386
      } elseif {$cpu_family == 4} {
         set cputype i486
      } elseif {$cpu_family == 5} {
         set cputype pentium
         if {$cpu_model>3} { set cputype pentium-mmx }
      } elseif {$cpu_family == 6} {
         if {$cpu_model < 3} {
            set cputype pentiumpro
         } elseif {$cpu_model < 7} {
            set cputype pentium2
         } elseif {$cpu_model <9 \
                      || $cpu_model == 10 || $cpu_model == 11 } {
            set cputype pentium3
            set sselevel 1
         } elseif {$cpu_model == 9 || $cpu_model == 13} {
            set cputype pentium-m
            set sselevel 2
         } elseif {$cpu_model == 14} {
            set cputype core
            set sselevel 3
         } elseif {$cpu_model >= 15 && $cpu_model<26} {
            set cputype core2
            set sselevel 3
         } elseif {$cpu_model >= 26 && $cpu_model<45} {
            set cputype nehalem
            set sselevel 3
         } elseif {$cpu_model>=45 && $cpu_model<60} {
            set cputype sandy_bridge
            set sselevel 3
            set avxlevel 1
         } elseif {$cpu_model>=60} {
            set cputype haswell
            set sselevel 3
            set avxlevel 1
         }
      } elseif {$cpu_family >= 15} {
         set cputype pentium4
         set sselevel 2
         if {$cpu_model >=3} {
            set sselevel 3
            set cputype prescott
            if {$cpu_model >= 4} { set cputype nocona    }
            if {$cpu_model >= 6} { set cputype pentium-d }
         }
      }
   } elseif {[string match amd $vendor]} {
      set vendor "amd"
      if {$cpu_family == 4} {
         if {$cpu_model < 14} {
            set cputype i486
         } else {
            set cputype pentium
         }
      } elseif {$cpu_family == 5} {
         if {$cpu_model < 6} {
            set cputype k5
         } elseif {$cpu_model < 8} {
            set cputype k6
         } elseif {$cpu_model == 8} {
            set cputype k6-2
         } else {
            set cputype k6-3
         }
      } elseif {$cpu_family == 6} {
         if {$cpu_model<6} {
            set cputype athlon
         } else {
            set cputype athlon-4
            set sselevel 1
         }
      } elseif {$cpu_family == 15} {
         set cputype k8
         set sselevel 2
         if {$cpu_model > 32 } {  ;# Best guess?
            set sselevel 3
         }
      } elseif {$cpu_family < 18} {
         set cputype k10
         set sselevel 3
      } elseif {$cpu_family < 20} {
         set cputype llano
         set sselevel 3
      } elseif {$cpu_family < 21} {
         set cputype bobcat
         set sselevel 3
      } else {
         set cputype bulldozer
         set sselevel 3
         set avxlevel 1
      }
   }
   return [list $vendor $cputype $sselevel $avxlevel]
}

proc GuessCpuArch_NameStr { namestr } {
   # Seriously, don't use this routine.  It is inferior in every way
   # to GuessCpuArch_VendorFamilyModel.  Some common names like "Xeon"
   # and "Sempron" are entirely missing, because on their own "Xeon"
   # and "Sempron" carry very little meaning.
   #   This proc is hanging around on the off chance that someone
   # might find some use for it someday.

   set namestr [string tolower $namestr]
   if {[string match "*intel*" $namestr]} {
      set vendor intel
   } elseif {[string match "*amd*" $namestr]} {
      set vendor amd
   } else {
      set vendor unknown
   }

   set sselevel 0
   set cputype unknown
   switch -regexp -- $namestr {
      {^386}                    { set cputype i386 }
      {^486}                    { set cputype i486 }
      {intel(\(r\)|) *core(\(tm\)|) *2}  {
         set cputype core2
         set sselevel 3
      }
      {intel(\(r\)|) *core}     {
         set cputype core
         set sselevel 3
      }
      {pentium(\(r\)|) *d( |$)}  {
         set cputype pentium-d
         set sselevel 3
      }
      {pentium(\(r\)|) *(iv|4)}  {
         set cputype pentium4
         set sselevel 2
      }
      {pentium(\(r\)|) *m( |$)}  {
         set cputype pentium-m
         set sselevel 2
      }
      {pentium(\(r\)|) *(iii|3)} {
         set cputype pentium3
         set sselevel 1
      }
      {pentium(\(r\)|) *(ii|2)} { set cputype pentium2 }
      {pentium(\(r\)|) *pro}    { set cputype pentiumpro }
      {pentium}                 { set cputype pentium }
      {overdrive podp5v83}      { set cputype pentium }
      {opteron}                 -
      {athlon(\(tm\)|) 64}      -
      {athlon(\(tm\)|) fx}      {
         set cputype k8
         set sselevel 2
      }
      {athlon(\(tm\)|) mp}      -
      {athlon(\(tm\)|) xp}      -
      {athlon(\(tm\)|) (iv|4)}  {
         set cputype athlon-4
         set sselevel 1
      }
      {athlon(\(tm\)|) (tbird)} -
      {athlon}                  { set cputype athlon }
      {^k8}                     {
         set cputype k8
         set sselevel 2
      }
      {^k6-3}                   { set cputype k6-3 }
      {^k6-2}                   { set cputype k6-2 }
      {^k6}                     { set cputype k6 }
      {^k5}                     { set cputype k5 }
   }

   return [list $vendor $cputype $sselevel]
}


# Routine that determines processor specific optimization flags for gcc
# for x86 processors.  The first import is the gcc version, as returned
# by the GuessGccVersion proc.  Note that the flags accepted by gcc vary
# by version.  The second import, cpu_arch, should match output from the
# GuessCpu proc above.  Return value is a list of gcc flags.
proc GetGccCpuOptFlags { gcc_version cpu_arch } {
   global tcl_platform

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

   # Extract cpu information from cpu_arch import
   set cpu_vendor "unknown"
   set cpu_type   "unknown"
   set cpu_sse    0
   foreach {cpu_vendor cpu_type cpu_sse} $cpu_arch { break }

   # Construct optimization flags
   # Note: -fprefetch-loop-arrays is available in gcc 3.1
   # and later, for Intel processors pentium3 or better,
   # and for AMD processors k6-2 or better.
   set cpuopts {}
   if {$verA==2 && $verB>=95} {
      # Don't bother setting -march in case of
      # i386, i486, or k5
      switch -glob -- $cpu_type {
         k6*         { set cpuopts [list -march=k6] }
         pentium  -
         pentium-mmx { set cpuopts [list -march=pentium] }
         pentium* -
         prescott -
         nocona   -
         core*    -
         athlon*  -
         opteron  -
         k8          { set cpuopts [list -march=pentiumpro] }
      }
      set cpu_sse 0
   } elseif {$verA==3 && $verB<1} {
      # Don't bother setting -march in case of
      # i386, i486, or k5
      switch -glob -- $cpu_type {
         pentium  -
         pentium-mmx { set cpuopts [list -march=pentium] }
         pentium* -
         prescott -
         nocona   -
         core*       { set cpuopts [list -march=pentiumpro] }
         k6*         { set cpuopts [list -march=k6] }
         athlon*  -
         opteron  -
         k8          { set cpuopts [list -march=athlon] }
      }
      set cpu_sse 0
   } elseif {$verA==3  && $verB<3} {
      # Don't bother setting -march in case of
      # i386, i486, or k5
      switch -glob -- $cpu_type {
         pentium    -
         pentium-mmx { set cpuopts [list -march=pentium] }
         pentiumpro { set cpuopts [list -march=pentiumpro] }
         pentium2   { set cpuopts [list -march=pentium2] }
         pentium3   -
         pentium-m  { set cpuopts [list -march=pentium3 \
                                      -fprefetch-loop-arrays] }
         pentium*   -
         prescott   -
         nocona     -
         core*      { set cpuopts [list -march=pentium4 \
                                      -fprefetch-loop-arrays] }
         k6         { set cpuopts [list -march=k6] }
         k6-2       { set cpuopts [list -march=k6-2 -fprefetch-loop-arrays] }
         k6-3       { set cpuopts [list -march=k6-3 -fprefetch-loop-arrays] }
         athlon     { set cpuopts [list -march=athlon -fprefetch-loop-arrays] }
         athlon-tbird { set cpuopts [list -march=athlon-tbird \
                                        -fprefetch-loop-arrays] }
         athlon*    -
         opteron    -
         k8         { set cpuopts [list -march=athlon-4 \
                                      -fprefetch-loop-arrays] }
      }
      if {$cpu_sse>=2} { set cpu_sse 2 }
   } elseif {$verA==3 && $verB==3} {
      # Don't bother setting -march in case of
      # i386, i486, or k5
      switch -glob -- $cpu_type {
         pentium     -
         pentium-mmx { set cpuopts [list -march=pentium] }
         pentiumpro  { set cpuopts [list -march=pentiumpro] }
         pentium2    { set cpuopts [list -march=pentium2] }
         pentium3    -
         pentium-m   { set cpuopts [list -march=pentium3 \
                                       -fprefetch-loop-arrays] }
         pentium4    { set cpuopts [list -march=pentium4 \
                                       -fprefetch-loop-arrays] }
         prescott    { set cpuopts [list -march=prescott \
                                       -fprefetch-loop-arrays] }
         nocona      -
         pentium*    -
         core*       { set cpuopts [list -march=nocona \
                                       -fprefetch-loop-arrays] }
         k6          { set cpuopts [list -march=k6] }
         k6-2        { set cpuopts [list -march=k6-2 -fprefetch-loop-arrays] }
         k6-3        { set cpuopts [list -march=k6-3 -fprefetch-loop-arrays] }
         athlon      { set cpuopts [list -march=athlon \
                                       -fprefetch-loop-arrays] }
         athlon-tbird { set cpuopts [list -march=athlon-tbird \
                                        -fprefetch-loop-arrays] }
         athlon*     -
         opteron     -
         k8          { set cpuopts [list -march=athlon-4 \
                                       -fprefetch-loop-arrays] }
      }
      if {$cpu_sse>=3} { set cpu_sse 3 }  ;# Safety
  } elseif {($verA==3 && $verB>=4) || ($verA==4 && $verB<=1) \
      || ($verA<5 && [string compare "Darwin" $tcl_platform(os)]==0)} {
      # On Mac Os X Lion (others?), despite what 'man gcc' reports,
      # gcc doesn't support "-march=native", although it does support
      # "mtune=native". (This is fixed in more recent gcc.)

      # Don't bother setting -march in case of i386, i486, or k5
      switch -glob -- $cpu_type {
         pentium     -
         pentium-mmx { set cpuopts [list -march=pentium] }
         pentiumpro  { set cpuopts [list -march=pentiumpro] }
         pentium2    { set cpuopts [list -march=pentium2] }
         pentium3    { set cpuopts [list -march=pentium3 \
                                       -fprefetch-loop-arrays] }
         pentium-m   { set cpuopts [list -march=pentium-m \
                                       -fprefetch-loop-arrays] }
         pentium4    { set cpuopts [list -march=pentium4 \
                                       -fprefetch-loop-arrays] }
         prescott    { set cpuopts [list -march=prescott \
                                       -fprefetch-loop-arrays] }
         nocona      -
         pentium*    -
         core*       { set cpuopts [list -march=nocona \
                                       -fprefetch-loop-arrays] }
         k6          { set cpuopts [list -march=k6 -fprefetch-loop-arrays] }
         k6-2        { set cpuopts [list -march=k6-2 -fprefetch-loop-arrays] }
         k6-3        { set cpuopts [list -march=k6-3 -fprefetch-loop-arrays] }
         athlon      { set cpuopts [list -march=athlon \
                                       -fprefetch-loop-arrays] }
         athlon-4    { set cpuopts [list -march=athlon-4 \
                                       -fprefetch-loop-arrays] }
         opteron     -
         k8          { set cpuopts [list -march=k8 -fprefetch-loop-arrays] }
         default {
	     # Assume cputype is newer than any on above list, and
	     # so use flags for core on intel and k8 on amd
	     if {[string match intel $cpu_vendor]} {
		 set cpuopts [list -march=nocona -fprefetch-loop-arrays]
	     } elseif {[string match amd $cpu_vendor]} {
		 set cpuopts [list -march=k8 -fprefetch-loop-arrays]
	     }
         }
      }
      if {($verA==4 && $verB>=2)} {
         lappend cpuopts "-mtune=native"
      }
      if {$cpu_sse>=3} { set cpu_sse 3 }  ;# Safety
   } elseif {$verA>4 || ($verA==4 && $verB>=2)} {
      set cpuopts [list -march=native]
      # -march/-mtune=native setting introduced with gcc 4.2
      switch -glob -- $cpu_type {
         pentium     -
         pentium-mmx -
         pentiumpro  -
         pentium2    {}
         pentium*    -
         prescott    -
         nocona      -
         pentium*    -
         core*       { lappend cpuopts -fprefetch-loop-arrays }
         k6          {}
         k6-*        -
         athlon*     -
         opteron     -
         k8          { lappend cpuopts -fprefetch-loop-arrays }
      }
      if {$cpu_sse>=3} { set cpu_sse 3 }  ;# Safety
   }
   if {$cpu_sse>0} {
      lappend cpuopts -mfpmath=sse -msse
      for {set sl 2} {$sl<=$cpu_sse} {incr sl} {
         lappend cpuopts -msse$sl
      }
   }

   return $cpuopts
}

########################################################################
# Routines to determine processor capabilites from the features flags:
proc GetFlags {} {
   set flags {}
   global tcl_platform
   if {[regexp -nocase -- linux $tcl_platform(os)]} {
      # Linux; Read and organize cpu info from /proc/cpuinfo
      set chan [open "/proc/cpuinfo" r]
      set data [read $chan]
      close $chan
      regsub "\n\n.*" $data {} cpu0
      regsub -all "\[ \t]+" $cpu0 { } cpu0
      regsub -all " *: *" $cpu0 {:} cpu0
      regsub -all " *\n *" $cpu0 "\n" cpu0
      array set cpuarr [split $cpu0 ":\n"]
      set flags $cpuarr(flags)
   } elseif {[regexp -nocase -- darwin $tcl_platform(os)]} {
      # Mac OS X
      if {![catch {exec sysctl machdep.cpu.features} feats]} {
         # sysctl returns strings of the form
         #       machdep.cpu.features: FPU VME DE PSE
         set flags [regsub {^[^:]+: *} $feats {}]
      }
   }
   return [string tolower $flags]
}

proc Find_ISE_Level { {flags {}} } {
   # Returns Instruction Set Extensions, one of
   #    {}  (= none)
   #   sse
   #   sse2
   #   pni    (aka "Prescott New Instructions" or SSE3)
   #   ssse3
   #   sse4_1
   #   sse4_2
   #   sse4a
   #   avx
   #   avx2
   # The last matching value in the above list is returned.
   if {[string match {} $flags]} {
      set flags [GetFlags]
   }
   foreach elt [list avx2 avx sse4a sse4_2 sse4_1 ssse3 pni sse2 sse] {
      if {[lsearch -exact $flags $elt]>=0} {
         return $elt
      }
   }
   return {}
}

proc Find_SSE_Level { {flags {}} } {
   if {[string match {} $flags]} {
      set flags [GetFlags]
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
      set flags [GetFlags]
   }
   if {[lsearch $flags fma]>=0} {
      set fma_type 3
   } elseif {[lsearch $flags fma4]>=0} {
      set fma_type 4
   } else {
      set fma_type 0
   }
}

########################################################################

########################################################################
########################################################################

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
   set opts [list -xHost -O3 -ipo -no-prec-div -ansi_alias \
                -fp-model fast=2 -fp-speculation fast \
                -std=c++11]
   return $opts
}
proc GetIcpcValueSafeOptFlags { icpc_version } {
   set opts [list -xHost -O3 -ipo -ansi_alias \
                -fp-model precise -no-ftz -fp-speculation fast \
                -std=c++11]
   # AFAICT, the -no-ftz (no flush to zero) flag doesn't hurt, but
   # is only operative when compiling main().  All other routines
   # inherit the FPU settings from the caller.
   return $opts
}
proc GetIcpcCpuOptFlags { icpc_version cpu_arch } {
   # CPU model architecture specific options.
   set cpuopts {}
   set cpu_vendor "unknown"
   set cpu_type   "unknown"
   set cpu_sse    0
   foreach {cpu_vendor cpu_type cpu_sse} $cpu_arch {
      set cpu_vendor [string tolower $cpu_vendor]
      set cpu_type   [string tolower $cpu_type]
      break
   }
   if {[lindex $icpc_version 0]<11} {
      # The following flags are based on the docs for icc version 10.0.
      # Allowed flags may differ for other versions of icc.  Note: -xT
      # and -xP imply SSE3.
      switch -exact -- $cpu_type {
         athlon-4  { lappend cpuopts -xK }
         opteron   -
         k8        { lappend cpuopts -xW}
         pentium3  { lappend cpuopts -xK }
         pentium-m { lappend cpuopts -xW}
         pentium4  { lappend cpuopts -xN}
         prescott  -
         nocona    -
         pentium-d -
         core      { lappend cpuopts -xP }
         core2     { lappend cpuopts -xT }
         default   {
            if {$cpu_sse>=3 && [string match intel $cpu_vendor]} {
               lappend cpuopts -xP
            } elseif {$cpu_sse>=2} {
               lappend cpuopts -xW
            } elseif {$cpu_sse>=1} {
               lappend cpuopts -xK
            }
         }
      }
   } else {
      # The following flags are based on the docs for icc version 11.
      switch -exact -- $cpu_type {
         athlon-4  { lappend cpuopts -xSSE }
         opteron   -
         k8        { lappend cpuopts -msse2}
         pentium3  { lappend cpuopts -xSSE }
         pentium-m { lappend cpuopts -msse2}
         pentium4  { lappend cpuopts -xSSE2}
         prescott  -
         nocona    -
         pentium-d -
         core      { lappend cpuopts -xSSE3}
         core2     { lappend cpuopts -xSSSE3}
         default   {
            if {$cpu_sse>=3 && [string match intel $cpu_vendor]} {
               lappend cpuopts -xSSE3
            } elseif {$cpu_sse>=2} {
               lappend cpuopts -msse2
            } elseif {$cpu_sse>=1} {
               lappend cpuopts -xSSE
            }
         }
      }
   }

   return $cpuopts
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
   set opts  [list -O2 -Kieee -Mvect=noassoc,fuse,prefetch \
                 -Mcache_align -Mprefetch -Msmartalloc -Mnoframe \
                 -Munroll -Mnoautoinline]
   return $opts
}
proc GetPgccFastOptFlags { pgcc_version cpu_arch } {
   # CPU model architecture specific options.
   set opts [list -fast -Knoieee -Mnoautoinline]
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
