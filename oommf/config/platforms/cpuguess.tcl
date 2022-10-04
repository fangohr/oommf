#!/bin/sh
# FILE: cpuguess-lintel.tcl
#
# Wrapper Tcl script to guess the CPU model.
#
#    v--Edit here if necessary \
exec tclsh "$0" ${1+"$@"}
########################################################################

proc Usage {} {
   puts stderr "Usage: tclsh cpuguess.tcl <platform> ?extras?"
   puts stderr " where <platform> is, for example, \"lintel\","
   puts stderr " and optional parameter extras is 0 or 1."
   exit 1
}

if {[llength $argv] <1 || [llength $argv]>2} { Usage }
if {[lsearch -regexp $argv {^-*[h|H]}]>=0} { Usage }

set platform [lindex $argv 0]
set platform_file "cpuguess-"
append platform_file [file rootname [file tail $platform]]
append platform_file ".tcl"
set platform_file [file join [file dirname [info script]]  $platform_file]

set extras 0
if {[llength $argv]>1} { set extras [lindex $argv 1] }

if {![file exists $platform_file]} {
   # puts stderr "Platform cpu file \"$platform_file\" does not exist"
   # exit 2
   puts "CPU guess: unknown"
   exit
}

if {![file readable $platform_file]} {
   puts stderr \
      "Platform cpu file \"$platform_file\" exists but is not readable"
   exit 2
}

source $platform_file

set cpu_arch [GuessCpu]
puts "CPU guess: $cpu_arch"

if {$extras} {
   set optlevel 2 ;# Default optimization level
   foreach elt [info proc Guess*Version] {
      if {[regexp -- {^Guess(.*)Version$} $elt dum compiler_str]} {
         set compiler [string tolower $compiler_str]
         if {[string match gcc $compiler]} {
            set compiler g++
         }
         if {![catch {$elt $compiler} compiler_version] \
                && ![string match {} $compiler_version]} {
            puts "$compiler version: $compiler_version"
            if {[string compare "Cl" $compiler_str]==0} {
               # Microsoft Cl compiler takes different switches depending on
               # whether the mode is 32- or 64-bit.
               if {[string match "*x86_64*" $platform]} {
                  set clmode x86_64
               } else {
                  set clmode x86
               }
               set genopts \
                  [GetClGeneralOptFlags $compiler_version $clmode $optlevel]
               set cpuopts \
                  [GetClCpuOptFlags $compiler_version $cpu_arch $clmode]
            } else {
               set genopts [Get${compiler_str}GeneralOptFlags \
                               $compiler_version $optlevel]
               set cpuopts [Get${compiler_str}CpuOptFlags \
                               $compiler_version $cpu_arch]
            }
            puts "  $compiler flags: [concat $genopts $cpuopts]"
         }
      }
   }
}


