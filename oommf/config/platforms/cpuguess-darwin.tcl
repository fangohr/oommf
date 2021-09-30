# FILE: cpuguess-darwin.tcl
#
# Tcl proc to guess the CPU model, on Apple OS X systems.
# Sourced by the darwin.tcl and cpuguess.tcl scripts.
#
#
########################################################################

# Routines to guess the processor type
#
# Sample output from various system introspection facilities, on a
# PowerBook G4:
#   hwprefs cpu_type           --> 7455 v2.1
#   machine                    --> ppc7450
#   system_profiler SPHardwareDataType | grep 'Machine Name'
#                              --> Machine Name: PowerBook G4
#   system_profiler SPHardwareDataType | grep 'CPU Type'
#                              --> CPU Type: PowerPC G4  (2.1)
#   sysctl hw.model            --> hw.model: PowerBook3,4
#   sysctl hw.cputype          --> hw.cputype: 18
#   sysctl hw.cpusubtype       --> hw.cpusubtype: 11
#   sysctl hw.optional.altivec --> hw.optional.altivec: 1
#
# From a eMac:
#   hwprefs cpu_type           --> <hwprefs not installed>
#   machine                    --> ppc7450
#   system_profiler SPHardwareDataType | grep 'Machine Name'
#                              --> <empty string>
#   system_profiler SPHardwareDataType | grep 'Machine Model'
#                              --> Machine Model: dMac
#   system_profiler SPHardwareDataType | grep 'CPU Type'
#                              --> CPU Type: PowerPC G4  (2.1)
#   sysctl hw.model            --> hw.model: PowerMac4,4
#   sysctl hw.cputype          --> hw.cputype: 18
#   sysctl hw.cpusubtype       --> hw.cpusubtype: 11
#   uname -p                   --> powerpc
#   echo $HOSTTYPE             --> powermac
#   echo $MACHTYPE             --> powerpc
#
# From a x86 MacBook Pro
#   hwprefs cpu_type           --> Intel Core Duo
#   machine                    --> i486
#   system_profiler SPHardwareDataType -->
#      Machine Name: MacBook Pro 15"
#      Machine Model: MacBookPro1,1
#      Processor Name: Intel Core Duo
#      Processor Speed: 2.16 GHz
#      Number Of Processors: 1
#      Total Number Of Cores: 2
#      L2 Cache (per processor): 2 MB
#      Memory: 2 GB
#      Bus Speed: 667 MHz
#      Boot ROM Version: MBP11.0055.B08
#      SMC Version: 1.2f10
#      Serial Number: W86290YNVWZ
#      Sudden Motion Sensor:
#          State: Enabled
#   sysctl hw.model            --> hw.model: MacBookPro1,1
#   sysctl hw.cputype          --> hw.cputype: 7
#   sysctl hw.cpusubtype       --> hw.cpusubtype: 4
#   uname -p                   --> i386
#   echo $HOSTTYPE             --> powerpc
#   echo $MACHTYPE             --> powerpc-apple-darwin8.0
#   sysctl machdep -->
#        machdep.cpu.vendor: GenuineIntel
#        machdep.cpu.brand_string: Genuine Intel(R) CPU T2600  @ 2.16GHz
#        machdep.cpu.model_string: Unknown Intel P6 Family
#        machdep.cpu.family: 6
#        machdep.cpu.model: 14
#        machdep.cpu.extmodel: 0
#        machdep.cpu.extfamily: 0
#        machdep.cpu.feature_bits: -1075184641 49577
#        machdep.cpu.extfeature_bits: 1048576 0
#        machdep.cpu.stepping: 8
#        machdep.cpu.signature: 1768
#        machdep.cpu.brand: 0
#        machdep.cpu.features: FPU VME DE PSE TSC MSR PAE MCE CX8 APIC \
#            SEP MTRR PGE MCA CMOV PAT CLFSH DS ACPI MMX FXSR SSE SSE2 \
#            SS HTT TM SSE3 MON VMX EST TM2 TPR
#        machdep.cpu.extfeatures: XD
#        machdep.cpu.logical_per_package: 2
#        machdep.cpu.cores_per_package: 2
#
#
# From a x86 Mac Pro
#   hwprefs cpu_type           --> Intel Xeon 53XX v7.0
#   machine                    --> i486
#   system_profiler SPHardwareDataType -->
#              Model Name: Mac Pro
#              Model Identifier: MacPro2,1
#              Processor Name: Quad-Core Intel Xeon
#              Processor Speed: 3 GHz
#              Number Of Processors: 2
#              Total Number Of Cores: 8
#              L2 Cache (per processor): 8 MB
#              Memory: 16 GB
#              Bus Speed: 1.33 GHz
#              ...
#   sysctl hw.model            --> hw.model: MacPro2,1
#   sysctl hw.cputype          --> hw.cputype: 7
#   sysctl hw.cpusubtype       --> hw.cpusubtype: 4
#   uname -p                   --> i386
#   echo $HOSTTYPE             --> i386
#   echo $MACHTYPE             --> i386-apple-darwin9.0
#   sysctl machdep -->
#        machdep.pmap.hashmax: 11
#        machdep.pmap.hashcnts: 247752
#        machdep.pmap.hashwalks: 215547
#        machdep.cpu.address_bits.virtual: 48
#        machdep.cpu.address_bits.physical: 36
#        machdep.cpu.cache.size: 4096
#        machdep.cpu.cache.L2_associativity: 8
#        machdep.cpu.cache.linesize: 64
#        machdep.cpu.arch_perf.fixed_width: 0
#        machdep.cpu.arch_perf.fixed_number: 0
#        machdep.cpu.arch_perf.events: 0
#        machdep.cpu.arch_perf.events_number: 7
#        machdep.cpu.arch_perf.width: 40
#        machdep.cpu.arch_perf.number: 2
#        machdep.cpu.arch_perf.version: 2
#        machdep.cpu.thermal.ACNT_MCNT: 1
#        machdep.cpu.thermal.thresholds: 2
#        machdep.cpu.thermal.dynamic_acceleration: 0
#        machdep.cpu.thermal.sensor: 1
#        machdep.cpu.mwait.sub_Cstates: 32
#        machdep.cpu.mwait.extensions: 3
#        machdep.cpu.mwait.linesize_max: 64
#        machdep.cpu.mwait.linesize_min: 64
#        machdep.cpu.cores_per_package: 4
#        machdep.cpu.logical_per_package: 4
#        machdep.cpu.extfeatures: XD EM64T
#        machdep.cpu.features: FPU VME DE PSE TSC MSR PAE MCE CX8 APIC   \
#          SEP MTRR PGE MCA CMOV PAT PSE36 CLFSH DS ACPI MMX FXSR SSE    \
#          SSE2 SS HTT TM SSE3 MON DSCPL VMX EST TM2 SSSE3 CX16 TPR PDCM
#        machdep.cpu.brand: 0
#        machdep.cpu.signature: 1783
#        machdep.cpu.extfeature_bits: 537919488 1
#        machdep.cpu.feature_bits: -1075053569 320445
#        machdep.cpu.stepping: 7
#        machdep.cpu.extfamily: 0
#        machdep.cpu.extmodel: 0
#        machdep.cpu.model: 15
#        machdep.cpu.family: 6
#        machdep.cpu.brand_string: Intel(R) Xeon(R) CPU X5365  @ 3.00GHz
#        machdep.cpu.vendor: GenuineIntel
#
# See also cpu-guess code in oommf/config/platforms/lintel.tcl.
#
proc GuessCpu_ppc {} {
   # PPC-flavor Mac
   set cpuinfo {}
   catch {
      # Try, in order, machine, hwprefs, system_profiler, sysctl
      if {![catch {exec machine} cpustr]} {
         regexp {ppc([0-9]+)} $cpustr dummy cpuinfo
      }
      if {[string match {} $cpuinfo] && \
             ![catch {exec hwprefs cpu_type} cpustr]} {
         set cpuinfo [lindex $cpustr 0]
      }
      if {[string match {} $cpuinfo] && \
             ![catch {exec system_profiler SPHardwareDataType} cpustr]} {
         set cpustr [split $cpustr "\n"]
         set cpurec [lsearch -glob $cpustr "*CPU Type*"]
         if {$cpurec>=0} {
            regexp {(G[0-9]+)} [lindex $cpustr $cpurec] dummy cpuinfo
         }
      }
      if {[string match {} $cpuinfo] && \
             ![catch {exec sysctl hw.model} cpustr]} {
         # sysctl hw.model returns strings of the form
         #       hw.model: PowerBook3,4
         # Unfortunately, AFAICT there is no simple mapping from
         # these model numbers to processors.  Below is a good(?)
         # guess, as of Jan-2006.
         set cpustr [string trim $cpustr]
         regexp {^hw.model: *(.*)$} $cpustr dummy cpustr
         regexp {^([^0-9]*)([0-9]+),?([^,]*)} $cpustr dummy type verA verB
         if {[string match PowerMac $type]} {
            if {$verA==4 && $verB==1} {
               set cpuinfo G3
            } elseif {$verA==10} {
               set cpuinfo G4
            } elseif {$verA>=7} {
               set cpuinfo G5
            } elseif {$verA>=3} {
               set cpuinfo G4
            } else {
               set cpuinfo G3
            }
         } elseif {[string match PowerBook $type]} {
            if {$verA==4 || ($verA==3 && $verB==1)} {
               set cpuinfo G3
            } elseif {$verA>=3} {
               set cpuinfo G4
            } else {
               set cpuinfo G3
            }
         } elseif {[string match RackMac $type]} {
            if {$verA<3} {
               set cpuinfo G4
            } else {
               set cpuinfo G5
            }
         } elseif {[string match iMac $type]} {
            # Note: Supposedly only the early iMac's returned "iMac"
            # as their machine id; later models return "PowerMac"
            set cpuinfo G3
         }
      }
      if {[string match G* $cpuinfo]} {
         # Map G# info a 3 or 4 digit model number
         switch -exact $cpuinfo {
            G3 { set cpuinfo 750 }
            G4 { set cpuinfo 7400 }
            G5 { set cpuinfo 970 }
            default { set cpuinfo {} }
         }
      }
   }
   if {![string match {} $cpuinfo]} {
      set cpuinfo [list PPC $cpuinfo]
   }
   return $cpuinfo
}

proc GuessCpu_x86_old {} {
   # x86-flavor Mac
   set cpuinfo {}
   catch {
      if {![catch {exec sysctl machdep} machstr]} {
         # sysctl machdep returns strings of the form
         #       machdep.cpu.vendor: GenuineIntel
         set tmplst [split $machstr "\n"]
         catch {unset macharr}
         foreach elt $tmplst {
            if {[regexp {^machdep\.cpu\.([^:]+):(.*)$} $elt dummy lab val]} {
               set lab [string tolower $lab]
               set val [string tolower $val]
               set macharr($lab) [string trim $val]
            }
         }
         if {[info exists macharr(vendor)] && \
                [string match "*intel*" $macharr(vendor)]} {
            # Genuine Intel CPU.  Match using family/model
            set cpu_family $macharr(family)
            set cpu_model  $macharr(model)
            if {$cpu_family >= 15} {
               set cpuinfo pentium4
               if {$cpu_model == 3} {
                  set cpuinfo prescott
               } elseif {$cpu_model >= 4} {
                  set cpuinfo nocona
               }
            } elseif {$cpu_family == 6} {
               if {$cpu_model < 3} {
                  set cpuinfo pentiumpro
               } elseif {$cpu_model <7 } {
                  set cpuinfo pentium2
               } elseif {$cpu_model == 7 || $cpu_model == 8 \
                            || $cpu_model == 10 || $cpu_model == 11 } {
                  set cpuinfo pentium3
               } elseif {$cpu_model == 9 \
                            || $cpu_model == 13 || $cpu_model == 14 } {
                  set cpuinfo pentium-m
               } elseif {$cpu_model >= 15} {
                  set cpuinfo core2
               }
            }
         }
      }
   }
   return $cpuinfo
}

source [file join [file dirname [info script]] x86-support.tcl]
catch {rename GetGccCpuOptFlagsX86 {}} ;# In case this file is
## sourced more than once.
rename GetGccCpuOptFlags GetGccCpuOptFlagsX86

proc GetGccCpuOptFlags { gcc_version cpu_arch } {
   if {![string match "ppc" [string tolower [lindex $cpu_arch 0]]]} {
      set opts [GetGccCpuOptFlagsX86 $gcc_version $cpu_arch]
      # Some old Apple versions of gcc had a -fast option,
      # but that has been superseded by gcc-standard -Ofast.
   } else {
      # Otherwise, PowerPC CPU
      set cputype [lindex $cpu_arch 1]
      set verA [lindex $gcc_version 0]
      set verB [lindex $gcc_version 1]
      if {$verA>3 || ($verA==3 && $verB>=3)} {
         # Processor specific optimization
         if {[string match 7?? $cputype]} {
            # G3; use default opts string
         } elseif {[string match 9?? $cputype]} {
            # G5
            if {[string match {} $opts]} {
               set opts -fast
               ## -fast is an Apple specific option
            }
         } elseif {[string match 74?? $cputype]} {
            # G4
            if {$cputype>=7450} {
               set opts -mcpu=7450
            } else {
               set opts -mcpu=7400
            }
            if {[string match {} $opts]} {
               set opts -fast
               ## -fast is an Apple specific option
            }
         }
      }
   }
   return $opts
}


proc GuessCpu_x86 {} {
   # x86-flavor Mac
   set cpuinfo {}
   catch {
      if {![catch {exec sysctl machdep} machstr]} {
         # sysctl machdep returns strings of the form
         #       machdep.cpu.vendor: GenuineIntel
         set tmplst [split $machstr "\n"]
         catch {unset macharr}
         foreach elt $tmplst {
            if {[regexp {^machdep\.cpu\.([^:]+):(.*)$} $elt dummy lab val]} {
               set lab [string tolower $lab]
               set val [string tolower $val]
               set macharr($lab) [string trim $val]
            }
         }
         if {[info exists macharr(vendor)]} {
            set vendor $macharr(vendor)
            set cpu_family $macharr(family)
            set cpu_model  $macharr(model)
            foreach {vendor cputype sselevel} \
               [GuessCpuArch_VendorFamilyModel $vendor $cpu_family $cpu_model] {
                  break
               }
            # Note: One might also want to look at $macharr(brand_string)
            set cpuinfo [list $vendor $cputype $sselevel]
         }
      }
   }
   return $cpuinfo
}


proc GuessCpu {} {
   # Return value is of the form "Brand Model ?sselevel?",
   # for example, "intel core2 3" or "PPC 970"
   set guess unknown
   set cpuinfo {}
   # Is this an x86 or PPC machine?
   global tcl_platform
   if {[string match "i?86" $tcl_platform(machine)] \
          || [string match "x86*" $tcl_platform(machine)]} {
      set cpuinfo [GuessCpu_x86]
   } elseif {![catch {exec machine} cpustr]} {
      if {[string match "i?86" $cpustr] || [string match "x86*" $cpustr]} {
         set cpuinfo [GuessCpu_x86]
      } else {
         set cpuinfo [GuessCpu_ppc]
      }
   }
   if {![string match {} $cpuinfo]} {
      set guess $cpuinfo
   }
   return $guess
}
