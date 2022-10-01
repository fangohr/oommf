# FILE: gcc-support.tcl
#
# A couple of Tcl procs for use determining compiler options for gcc:
#    GuessGccVersion
# which runs gcc to guess the version of GCC being used.
#    GetGccGeneralOptFlags
# which returns a list of aggressive, non-processor-specific
# optimization flags that can be passed to gcc.

# Routine to guess the gcc version.  The import, gcc, should be
# an executable command list, which is used via
#   exec [concat $gcc --version]
# or, rather, the "open" analogue to determine the gcc version (useful
# for when the flags accepted by gcc vary by version).  In particular,
# this means that spaces in the gcc cmd need to be protected
# appropriately.  For example, if there is a space in the gcc command
# path, then gcc should be set like so:
#  set gcc [list {/path with spaces/g++}]
# For multi-element commands, for example using the xcrun shim from Xcode,
# this would look like
#  set gcc [list xcrun {/path with spaces/g++}]
#
# The return value is the gcc version string as a list of numbers,
# for example, {4 1 1} for version 4.1.1.
proc GuessGccVersion { gcc } {
   set guess {}
   set testcmd [concat | $gcc --version]
   if {[catch {
          set fptr [open $testcmd r]
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


# Return leading element of gcc target triplet
proc GetGccDefaultTarget { gcc } {
   set testcmd [concat | $gcc -dumpmachine]
   if {[catch {
        set fptr [open $testcmd r]
        set targetstr [read $fptr]
        close $fptr}]} {
      return {}
   }
   return [lindex [split $targetstr -] 0]
}


proc GetGccBannerVersion { gcc } {
   set banner {}
   set testcmd [concat | $gcc --version]
   catch {
      set fptr [open $testcmd r]
      set banner [gets $fptr]
      close $fptr
      set testcmd [concat | $gcc -dumpmachine]
      set fptr [open $testcmd r]
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
# vary by version.  This proc is called by GetGccValueSafeFlags; changes
# here may require adjustments there.
proc GetGccGeneralOptFlags { gcc_version optlevel} {
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

   # Include flag for C++ 11 support, as available.  (Official gcc docs
   # say -std=c++0x introduced in gcc 4.3, and -std=c++11 in gcc 4.7).
   set opts {}
   if {$verA>4 || ($verA==4 && $verB>=3)} {
      if {$verA==4 && $verB<7} {
         lappend opts -std=c++0x
      } else {
         lappend opts -std=c++11
      }
   }

   # Lower level optimizations
   if {$optlevel<3} {
      if {$optlevel==0} {
         lappend opts -O0
      } elseif {$optlevel==1} {
         lappend opts -O1
      } else {
         lappend opts -O2
      }
      return $opts
   }

   # Higher level optimizations; some are gcc version specific
   if {$verA>4 || ($verA==4 && $verB>=6)} {
      lappend opts -Ofast   ;# Note: -Ofast is NOT standards conforming
   } else {
      lappend opts -O3
   }
   if {$verA>4 || ($verA==4 && $verB>=4)} {
      # -ffast-math breaks compensated summation in Nb_Xpfloat class.
      # GCC 4.4 and later support function attribute
      #          __attribute__((optimize("no-fast-math")))
      # which disables fast-math on a function-by-function basis,
      # thereby providing a workaround.
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

   return $opts
}

# Routine that returns optimization flags for gcc which are floating
# point value-safe.  The import is the gcc version, as returned by the
# preceding GuessGccVersion proc.  Note that the flags accepted by gcc
# vary by version.
#
proc GetGccValueSafeOptFlags { gcc_version optlevel } {
   set opts [GetGccGeneralOptFlags $gcc_version $optlevel]
   foreach check {-ffast-math -funsafe-math-optimizations -ffinite-math-only} {
      while {[set index [lsearch -exact $opts $check]]>=0} {
         set opts [lreplace $opts $index $index]
      }
   }
   if {[set index [lsearch -exact $opts -Ofast]]>=0} {
         set opts [lreplace $opts $index $index -O3]
   }

   if {[llength $gcc_version]>=2} {
      set verA [lindex $gcc_version 0]
      set verB [lindex $gcc_version 1]
      if {$verA>4 || ($verA==4 && $verB>=6)} {
         # The -ffp-contract=off control, which disables contraction of
         # floating point operations (such as changing a*b+c into an fma
         # instruction) appears in gcc 4.6.
         set check -ffp-contract=off
         if {[lsearch -exact $opts $check]<0} {
            lappend opts $check
         }
      }
   }

   return $opts
}

proc GetGccNativeFlags { gcc_version } {
   # Returns -march=native if gcc_version supports it, otherwise
   # returns empty string.
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

   # -march/-mtune=native setting introduced with gcc 4.2
   set march {}
   if {$verA>4 || ($verA==4 && 2<=$verB)} {
      set march [list -march=native]
   }
   return $march
}

proc GetGccx86ExtFlags { gcc_version cpu_flags } {
   # Returns a list of gcc flags based on gcc_version and cpu_flags,
   # where cpu_flags is a list of supported instruction extensions,
   # modeled on the "flags" line in /proc/cpuinfo on Linux systems.
   # For example,
   #   mmx sse sse2 ssse3 sse4_1 sse4_2 avx
   #
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

   # Internally defined cpuvect levels:
   # 0 - no vect      5 - avx
   # 1 - sse          6 - avx2
   # 2 - sse2         7 - avx512f, pf, er, cd
   # 3 - sse3         8 - avx512vl, bw, dq, ifma, vbmi
   # 4 - ssse3,sse4*
   #
   # Note: Three argument fma support via -mfma (as opposed to -mfma4)
   #  was introduced to gcc along with avx2 support.

   # Determine highest vectorization level supported by compiler.
   if {$verA<3} {
      set gccvect 0
   } elseif {$verA==3 && $verB<3} {
      set gccvect 2
   } elseif {$verA==3 || ($verA==4 && $verB<3)} {
      set gccvect 3
   } elseif {$verA==4 && $verB<4} {
      set gccvect 4
   } elseif {$verA==4 && $verB<7} {
      set gccvect 5
   } elseif {$verA==4 && $verB<9} {
      set gccvect 6
   } elseif {$verA<6} {
      set gccvect 7
   } else {
      set gccvect 8
   }

   # Combine gccvect with cpu_flags to set options
   set cpuopts {}
   if {$gccvect<5} {
      # SSE instructions
      if {$gccvect>=1 && [lsearch -exact $cpu_flags sse]>=0} {
         lappend cpuopts -msse
      }
      if {$gccvect>=2 && [lsearch -exact $cpu_flags sse2]>=0} {
         lappend cpuopts -msse2
      }
      if {$cpuvect>=3 && [lsearch -glob $cpu_flags *sse3]>=0} {
         lappend cpuopts -msse3
      }
      if {$cpuvect>=4} {
         foreach lvl [list ssse3 sse4 sse4_1 sse4_2] {
            if {[lsearch -exact $cpu_flags $lvl]>=0} {
               lappend cpuopts -m[regsub _ $lvl .]
            }
         }
      }
   } else {
      # AVX instructions
      if {$cpuvect>=5 && [lsearch -exact $cpu_flags avx]>=0} {
         lappend cpuopts -mavx
      }
      if {$cpuvect>=6 && [lsearch -exact $cpu_flags avx2]>=0} {
         lappend cpuopts -mavx2
         if {[lsearch -exact $cpu_flags fma]>=0} {
            lappend cpuopts -mfma
         }
      }
      if {$cpuvect>=7} {
         foreach sfx {f pf ef cd} {
            if {[lsearch -exact $cpu_flags avx512$sfx]>=0} {
               lappend cpuopts -mavx512$sfx
            }
         }
      }
      if {$cpuvect>=8} {
         foreach sfx {vl bw dq ifma vbmi} {
            if {[lsearch -exact $cpu_flags avx512$sfx]>=0} {
               lappend cpuopts -mavx512$sfx
            }
         }
      }
   }

   return $cpuopts
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
   set cpu_flags   {}
   foreach {cpu_vendor cpu_type cpu_flags} $cpu_arch { break }

   # Internally defined cpuvect levels:
   # 0 - no vect      5 - avx
   # 1 - sse          6 - avx2
   # 2 - sse2         7 - avx512f, pf, er, cd
   # 3 - sse3         8 - avx512vl, bw, dq, ifma, vbmi
   # 4 - sse4
   # These are set based on compiler support.  Comparison is
   # made to the cpu_arch flags to set cpu_flags
   set cpuvect 0

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
      set cpuvect 2
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
      set cpuvect 3
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
      set cpuvect 3
   } elseif {$verA==4 && 2<=$verB && $verB<4} {
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
      if {$verB>=3} {
         set cpuvect 4
      } else {
         set cpuvect 3
      }
   } elseif {$verA==4 && 4<=$verB && $verB<7} {
      set cpuvect 5
   } elseif {$verA==4 && 7<=$verB && $verB<9} {
      set cpuvect 6
   } elseif {($verA==4 && 9<=$verB) || $verA==5} {
      set cpuvect 7
   } elseif {$verA>=6} {
      set cpuvect 8
   }

   if {$cpuvect<5} {
      # SSE instructions
      if {$cpuvect>=1 && [lsearch -exact $cpu_flags sse]>=0} {
         lappend cpuopts -msse
      }
      if {$cpuvect>=2 && [lsearch -exact $cpu_flags sse2]>=0} {
         lappend cpuopts -msse2
      }
      if {$cpuvect>=3 && [lsearch -glob $cpu_flags *sse3]>=0} {
         lappend cpuopts -msse3
      }
      if {$cpuvect>=4 && [lsearch -glob $cpu_flags sse4*]>=0} {
         lappend cpuopts -msse4
      }
   } else {
      # AVX instructions
      if {$cpuvect>=5 && [lsearch -exact $cpu_flags avx]>=0} {
         lappend cpuopts -mavx
      }
      if {$cpuvect>=6 && [lsearch -exact $cpu_flags avx2]>=0} {
         lappend cpuopts -mavx2
      }
      if {$cpuvect>=7} {
         foreach sfx {f pf ef cd} {
            if {[lsearch -exact $cpu_flags avx512$sfx]>=0} {
               lappend cpuopts -mavx512$sfx
            }
         }
         if {[lsearch -exact $cpu_flags fma]>=0} {
            lappend cpuopts -mfma
         }
      }
      if {$cpuvect>=8} {
         foreach sfx {vl bw dq ifma vbmi} {
            if {[lsearch -exact $cpu_flags avx512$sfx]>=0} {
               lappend cpuopts -mavx512$sfx
            }
         }
      }
   }

   return $cpuopts
}
