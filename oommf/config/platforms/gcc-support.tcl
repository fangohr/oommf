# FILE: gcc-support.tcl
#
# A couple of Tcl procs for use determining compiler options for gcc:
#    GuessGccVersion
# which runs gcc to guess the version of GCC being used.
#    GetGccGeneralOptFlags
# which returns a list of aggressive, non-processor-specific
# optimization flags that can be passed to gcc, and
#    GetGccCpuOptFlags
# which returns a list of aggressive, processor-specific
# optimization flags for gcc.

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

proc GetGccBannerVersion { gcc } {
   set banner {}
   catch {
      set fptr [open "|$gcc --version" r]
      set banner [gets $fptr]
      close $fptr
      set fptr [open "|$gcc -dumpmachine" r]
      set target [gets $fptr]
      close $fptr
      if {![string match {} $target]} {
         append banner " / $target"
      }
   }
   return $banner
}

# Routine that returns aggressive, processor agnostic, optimization
# flags for gcc.  The import is the gcc version, as returned by the
# preceding GuessGccVersion proc.  Note that the flags accepted by gcc
# vary by version.
#
proc GetGccGeneralOptFlags { gcc_version } {

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

   # Optimization options
   set opts [list -O2]
   if {$verA>4 || ($verA==4 && $verB>=4)} {
      # -ffast-math breaks compensated summation in Nb_Xpfloat class.
      # GCC 4.4 and later support function attribute
      #          __attribute__((optimize("no-fast-math")))
      # which disables fast-math on a function-by-function basis,
      # thereby providing a work around.
      lappend opts -ffast-math
   }
   if {$verA>=3} {
      lappend opts -frename-registers

      # Docs say -fstrict-aliasing is in 2.95.  I can't find docs
      # for 2.8, but one extant installation I have (on SGI) doesn't
      # recognize -fscrict-aliasing.  To be safe, just require 3.x.
      # OTOH, i686-apple-darwin9-gcc-4.0.1 (OS X Leopard) (others?)
      # has an optimization bug yielding a "Bus error" for some
      # code if -fstrict-aliasing is enabled.
      if {$verA!=4 || $verB>0} {
         lappend opts -fstrict-aliasing
      }

      if {$verA>=4 || $verB>=4} {
         lappend opts -fweb
      }
   }

   # Frame pointer: Some versions of gcc don't handle exceptions
   # properly w/o frame-pointers.  This typically manifests as
   # Oxs dying without an error message while loading a MIF file.
   # On some x86 systems, including in addition the flag
   # -momit-leaf-frame-pointer makes the problem go away.
   # (See proc GetGccCpuOptFlags in x86-support.tcl).
   # Comment this out if the aforementioned problem occurs.
   lappend opts -fomit-frame-pointer

   return $opts
}

# Routine that determines processor specific optimization flags for
# gcc.  The first import is the gcc version, as returned by the
# GuessGccVersion proc (see above).  Note that the flags accepted by
# gcc vary by version.  The second import, cpu_arch, should match
# output from the GuessCpu proc in x86-support.tcl.  Return value is a
# list of gcc flags.
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
		|| [string compare "Darwin" $tcl_platform(os)]==0} {
      # On Mac Os X Lion (others?), despite what 'man gcc' reports,
      # gcc doesn't support "-march=native", although it does support
      # "mtune=native".

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

   # Frame pointer: Some versions of gcc don't handle exceptions
   # properly w/o frame-pointers.  This typically manifests as
   # Oxs dying without an error message while loading a MIF file.
   # Interestingly, including -momit-leaf-frame-pointer appears
   # to work around this problem, at least on some systems.  YMMV;
   # Comment this out if the aforementioned problem occurs.
   lappend cpuopts -momit-leaf-frame-pointer

   return $cpuopts
}
