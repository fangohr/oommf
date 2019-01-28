#!/bin/sh
# FILE: perftest.tcl
#
# Harness for performance testing
#
#    v--Edit here if necessary \
exec tclsh "$0" ${1+"$@"}

########################################################################
# Support code

if {[string match "windows" $tcl_platform(platform)]} {
   set nuldevice "nul:"
} else {
   set nuldevice "/dev/null"
}

##########
# In Tcl 8.5 and later, stderr from exec can be redirected to
# the result with the notation "2>@1".  The portability of this
# is better than "2>@stdout", so use the former when allowed.
set ERR_REDIRECT "2>@stdout"
foreach {TCL_MAJOR TCL_MINOR} [split [info tclversion] .] { break }
if {$TCL_MAJOR>8 || ($TCL_MAJOR==8 && $TCL_MINOR>=5)} {
   set ERR_REDIRECT "2>@1"
}

# The exec command in Tcl versions 8.5 and later supports an
# -ignorestderr option; without this output to stderr is interpreted as
# being an error.  The -ignorestderr option is a useful for debugging
# when you don't want debug output on stderr to register as an error.
# Introduce a proc that uses this option, if available, or else swallows
# all stderr output.
if {[regexp {^([0-9]+)\.([0-9]+)\.} [info patchlevel] dummy vmaj vmin] \
       && ($vmaj>8 || ($vmaj==8 && $vmin>=5))} {
   proc exec_ignore_stderr { args } {
      eval exec -ignorestderr $args
   }
} else {
   proc exec_ignore_stderr { args } {
      global ERR_REDIRECT
      if {[string compare & [lindex $args end]]==0} {
         set args [linsert $args [expr {[llength $args]-1}] $ERR_REDIRECT]
      } else {
         lappend args $ERR_REDIRECT
      }
      eval exec $args
   }
}

########################################################################
# Script control parameters
set tcl_precision 0

set EXEC_STAGE_TEST_TIMEOUT 6000 ;# Max time to wait for any single test
## stage to run, in seconds.

set HOME  [file dirname [info script]]
if {![catch {file normalize $HOME} _]} {
   # 'file normalize' new in Tcl 8.4
   set HOME $_
}

set OOMMF [file join $HOME .. .. .. oommf.tcl]

set TCLSH [info nameofexecutable]
if {![catch {file normalize $TCLSH} _]} {
   # 'file normalize' new in Tcl 8.4
   set TCLSH $_
}

set NUMASUPPORT 0
set boxsi_help [exec $TCLSH $OOMMF boxsi -help $ERR_REDIRECT]
if {[regexp -- {-numanodes} $boxsi_help]} {
   set NUMASUPPORT 1
}

set THREADSUPPORT 0
if {[regexp -- {-threads} $boxsi_help]} {
   set THREADSUPPORT 1
}

# Base name for output .odt files.  Decorate with machine name and pid,
# so concurrent runs can be made off one file system.
set BASENAME "perftest-datatable-[info hostname]-[pid]"
set ARCHIVE_NICKNAME ":perftest-[info hostname]-[pid]"

set ODTFILE "[file join $HOME $BASENAME].odt"

set LOGFILE [file join $HOME perftest.log]

set OUTLIER_BAR  1.05  ;# Stage times longer than $OUTLIER_BAR * average
## are assumed to be spurious and therefore dropped.

########################################################################
# Patches for older versions of Tcl (borrowed from oommf/pkg/oc/bug*tcl)
foreach {maj min} [split [info tclversion] "."] break

# The -unique option was added to [lsort] in 8.3
if {$maj < 8 || ($maj == 8 && $min < 3)} {
   rename lsort Tcl8.2_lsort
   ;proc lsort {args} {
      if {[llength $args] < 2} {
         return [uplevel 1 Tcl8.2_lsort $args]
      }
      set argcount [llength $args]
      set list [lindex $args [expr {$argcount-1}]]
      set options [lrange $args 0 [expr {$argcount-2}]]

      set uniq_flag 0
      set sort_index -1
      set sort_method ascii
      for {set i 0} {$i<$argcount-1} {incr i} {
         set o [lindex $options $i]
         switch -exact -- $o {
            -ascii -
            -dictionary -
            -increasing -
            -decreasing {}
            -integer { set sort_method integer }
            -real    { set sort_method real }
            -command {
               set sort_method command
               set sort_command [lindex $options [incr i]]
            }
            -index {
               set sort_index [lindex $options [incr i]]
            }
            -unique {
               set uniq_flag 1
               set options [lreplace $options $i $i]
               incr i -1
               incr argcount -1
            }
            default {
               return -code error "\[lsort] option \"$o\" not supported."
            }
         }
      }

      set result [uplevel 1 Tcl8.2_lsort $options [list $list]]
      if {!$uniq_flag || [llength $result]<2} {
         return $result
      }

      set new_result {}
      set oldcheck [set oldelt [lindex $result 0]]
      if {$sort_index>=0} { set oldcheck [lindex $oldcheck $sort_index] }
      foreach elt $result {
         set check $elt
         if {$sort_index>=0} { set check [lindex $check $sort_index] }
         switch -exact -- $sort_method {
            ascii {
               if {[string compare $oldcheck $check]!=0} {
                  lappend new_result $oldelt
               }
            }
            integer {
               if {int($oldcheck) != int($check)} {
                  lappend new_result $oldelt
               }
            }
            real {
               if {double($oldcheck) != double($check)} {
                  lappend new_result $oldelt
               }
            }
            command {
               if {[eval $sort_command [list $oldcheck $check]]!=0} {
                  lappend new_result $oldelt
               }
            }
            default {
               return -code error "Bad bug82.tcl \[lsort]\
                                 sort method: $sort_method"
            }
         }
         set oldelt $elt
         set oldcheck $check
      }
      lappend new_result $elt

      return $new_result
   }
}  ;# Tcl 8.3 patch

if {$maj < 7 || ($maj == 7 && $min < 6)} {
   if {[string match unix $tcl_platform(platform)]} {
      # The subcommands copy, delete, rename, and mkdir were added to
      # the Tcl command 'file' in Tcl version 7.6.  The following command
      # approximates them on Unix platforms.  It may not agree with
      # the Tcl 7.6+ command 'file' in all of its functionality (notably
      # the way it reports errors).  Further refinements should be made as
      # needed.
      rename file Tcl7.5_file
      proc file {option args} {
         switch -glob -- $option {
            c* {
               if {[string first $option copy] != 0} {
                  return [uplevel [list Tcl7.5_file $option] $args]
               }
               # Translate -force into -f
               if {[string match -force [lindex $args 0]]} {
                  set args [lreplace $args 0 0 -f]
               }
               uplevel exec cp $args
            }
            de* {
               if {[string first $option delete] != 0} {
                  return [uplevel [list Tcl7.5_file $option] $args]
               }
               if {[string match -force [lindex $args 0]]} {
                  set args [lreplace $args 0 0 -f]
               }
               catch {uplevel exec rm $args}
            }
            mk* {
               if {[string first $option mkdir] != 0} {
                  return [uplevel [list Tcl7.5_file $option] $args]
               }
               uplevel exec mkdir $args
            }
            ren* {
               if {[string first $option rename] != 0} {
                  return [uplevel [list Tcl7.5_file $option] $args]
               }
               if {[string match -force [lindex $args 0]]} {
                  set args [lreplace $args 0 0 -f]
               }
               uplevel exec mv $args
            }
            default {
               uplevel [list Tcl7.5_file $option] $args
            }
         }
      }
   }  ;# unix
} ;# Tcl 7.5 patch

########################################################################
# Support procs
proc PowerOfTwoCeil { n } {
   set pot 1
   while {$pot<$n} {
      set pot [expr {$pot*2}]
   }
   return $pot
}


########################################################################
# System data and task report header
proc SplitTopologyString { str } {
   # Covert strings like "1-3,7" to lists like "1 2 3 7".   Used for
   # parsing /sys/devices/system/cpu/cpu#/topology/*list data.
   set result {}
   foreach elt [split $str ","] {
      if {[regexp {^([0-9]+)-([0-9]+)$} $elt dummy a b] && $a<=$b} {
         for {set i $a} {$i<=$b} {incr i} {
            lappend result $i
         }
      } else {
         lappend result $elt
      }
   }
   return [lsort -integer $result]
}

proc CollectSystemInfo {} {
   global system_info tcl_platform
   set system_info(date) [clock format [clock seconds]]
   set system_info(os) "$tcl_platform(os) $tcl_platform(osVersion)"
   regsub {\..*$} [info hostname] {} system_info(machine_name)

   if {[string match "windows" $tcl_platform(platform)]} {
      if {![catch {exec wmic cpu get Name} cputype]} {
         regsub {^Name} $cputype {} cputype
         set system_info(cputype) [string trim $cputype]
      }
      if {![catch {exec wmic cpu get NumberOfCores} corecount]} {
         regsub {^NumberOfCores} $corecount {} corecount
         set system_info(core_count) [string trim $corecount]
      }
      if {![catch {exec wmic cpu get NumberOfLogicalProcessors} \
                threadcount]} {
         regsub {^NumberOfLogicalProcessors} $threadcount {} threadcount
         set system_info(thread_count) [string trim $threadcount]
      }
      if {![catch {exec wmic ComputerSystem get TotalPhysicalMemory} \
                memsize]} {
         regsub {^TotalPhysicalMemory} $memsize {} memsize
         set memsize [string trim $memsize]
         set memsize [expr {double($memsize)/1024./1024./1024.}]
         set system_info(memsize) [format "%.1f GB" $memsize]
      }
      if {![catch {exec wmic OS get FreePhysicalMemory} \
                freemem]} {
         # Reports free memory, in KB
         regsub {^FreePhysicalMemory} $freemem {} freemem
         set freemem [string trim $freemem]
         set freemem [expr {double($freemem)/1024./1024.}]
         set system_info(freemem) [format "%.1f GB" $freemem]
      }
      if {![catch {exec wmic ComputerSystem get NumberOfProcessors} \
                pcount]} {
         regsub {^NumberOfProcessors} $pcount {} pcount
         set system_info(processor_count) [string trim $pcount]
      }
      if {![catch {exec wmic OS get Caption} winver]} {
         regsub {^Caption} $winver {} winver
         append system_info(os) " / [string trim $winver]"
         if {![catch {exec wmic OS get CSDversion} winpatch]} {
            regsub {^CSDVersion} $winpatch {} winpatch
            regsub {Service Pack} $winpatch {SP} winpatch
            append system_info(os) " [string trim $winpatch]"
         }
      }
   } elseif {[string match "Linux" $tcl_platform(os)]} {
      if {![catch {open "/proc/cpuinfo"} chan]} {
         set data [split [string trim [read $chan]] "\n"]
         close $chan

         set modelindex  [lsearch -regexp $data "^model name\[ \t\]*:.*$"]
         if {$modelindex>=0} {
            set cputype [lindex $data $modelindex]
            regsub "^model name\[ \t\]*:" $cputype {} cputype
            set cputype [string trim $cputype]
            set system_info(cputype) $cputype
         }

         catch {unset physid}
         set threadcount 0
         while {[set index [lsearch -regexp $data "^physical id\[ \t\]*:"]] >= 0} {
            set elt [lindex $data $index]
            if {![regexp {: ([0-9]+)$} $elt dummy id]} {
               puts stderr "Bad /proc/cpuinfo record: $elt"
            } else {
               if {![info exists physid($id)]} {
                  set physid($id) 1
               } else {
                  incr physid($id)
               }
               incr threadcount
            }
            set data [lrange $data [expr {$index+1}] end]
         }
         if {[info exists physid]} {
            set system_info(processor_count) [llength [array names physid]]
            set system_info(thread_count) $threadcount
         }
         # Check for hyperthreading, and derive physical core count from
         # threadcount accordingly.
         if {![catch {
              set chana [open \
               /sys/devices/system/cpu/cpu0/topology/thread_siblings_list r]
              set siblings [read -nonewline $chana]
              close $chana
              set chanb [open /sys/devices/system/cpu/cpu0/topology/core_id r]
              set coreid [read -nonewline $chanb]
            close $chanb}]} {
            set siblings [SplitTopologyString $siblings]
            set sibcount 0
            foreach i $siblings {
               if {![catch {
                  set chanX [open \
                        /sys/devices/system/cpu/cpu${i}/topology/core_id r]
                  set checkid [read -nonewline $chanX]
                  close $chanX}]} {
                  if {$checkid == $coreid} {
                     incr sibcount
                  }
               } else {
                  set sibcount -1
                  break
               }
            }
            # Hyperthreading is enabled iff sibcount>1
            if {$sibcount>0} {
               set corecount [expr {$threadcount/$sibcount}]
               if {$corecount * $sibcount == $threadcount} {
                  set system_info(core_count) $corecount
               }
            }
         }
      }
      if {![catch {exec free} _]} {
         set memdata [split $_ "\n"]
         set total_index [lsearch -exact [lindex $memdata 0] total]
         set free_index [lsearch -exact [lindex $memdata 0] free]
         set avail_index [lsearch -exact [lindex $memdata 0] available]
         if {[set index [lsearch -regexp $memdata {^Mem:}]] >= 0} {
            set line [lindex $memdata $index]
            if {$total_index >= 0} {
               set total [lindex $line [expr {$total_index + 1}]]
               # "total" result is memory in KB
               set memsize [expr {double($total)/1024./1024.}]
               set system_info(memsize) [format "%.1f GB" $memsize]
            }
            if {$avail_index >= 0} {
               # 'free' output includes an 'available' column
               set avail [lindex $line [expr {$avail_index + 1}]]
               set free [expr {double($avail)/1024./1024.}]
               set system_info(freemem) [format "%.1f GB" $free]
            } elseif {$free_index >= 0} {
               # No 'available' column, so use 'free' column, preferably
               # from the -/+ buffers/cache line.
               set cache_index [lsearch -regexp $memdata {^-/\+ buffers/cache:}]
               if { $cache_index >= 0} {
                  set cache_line [lindex $memdata $cache_index]
                  if {[regexp "^-/\\+ buffers/cache:\[ \t\]*(.*)$" \
                       $cache_line dummy cache_line]} {
                     set free [lindex $cache_line [expr {$cache_index-1}]]
                     set free [expr {double($free)/1024./1024.}]
                     set system_info(freemem) [format "%.1f GB" $free]
                  }
               } else {
                  set free [lindex $line [expr {$free_index + 1}]]
                  set free [expr {double($free)/1024./1024.}]
                  set system_info(freemem) [format "%.1f GB" $free]
               }
            }
         }
      }
      if {![catch {exec lsb_release -d} _]} {
         if {[regexp {^Description:(.*)$} $_ dummy osname]} {
            set osname [string trim $osname]
            append system_info(os) " / $osname"
         }
      } elseif {[file readable /etc/redhat-release]} {
         if {![catch {
            set chan [open /etc/redhat-release r]
            set osname [string trim [read $chan]]
            close $chan}]} {
         }
         append system_info(os) " / $osname"
      } elseif {[file readable /etc/os-release]} {
         if {![catch {
            set chan [open /etc/os-release r]
            set osname [string trim [read $chan]]
            close $chan}]} {
         }
         append system_info(os) " / $osname"
      } elseif {[file readable /etc/debian_version]} {
         if {![catch {
            set chan [open /etc/debian_version r]
            set osver [string trim [read $chan]]
            close $chan}]} {
         }
         append system_info(os) " / Debian $osver"
      }
      # Check huge page support
      set HUGEPAGEFILE /sys/kernel/mm/transparent_hugepage/enabled
      if {![catch {open $HUGEPAGEFILE r} chan]} {
         set hpstat [read -nonewline $chan]
         close $chan
         if {[regexp {\[([^]]+)\]} $hpstat dummy mode]} {
            set system_info(hugepage) $mode
         }
      }
      # Check for turbo mode
      set TURBOFILE /sys/devices/system/cpu/cpufreq/boost
      set TURBOALTFILE /sys/devices/system/cpu/intel_pstate/no_turbo
      if {![catch {open $TURBOFILE r} chan]} {
         set turbo_state [read -nonewline $chan]
         close $chan
      } elseif {![catch {open $TURBOALTFILE r} chan]} {
         set noturbo_state [read -nonewline $chan]
         close $chan
         if {$noturbo_state} {
            set turbo_state 0
         } else {
            set turbo_state 1
         }
      }
      if {[info exists turbo_state]} {
         if {$turbo_state} {
            set system_info(turbo) enabled
         } else {
            set system_info(turbo) disabled
         }
      }

   } elseif {[string match "Darwin" $tcl_platform(os)]} { ;# Mac
       if {![catch {exec sw_vers} _]} {
	   set _ [split $_ "\n"]
	   set name [lsearch -regexp $_ {^ProductName:}]
	   set version [lsearch -regexp $_ {^ProductVersion:}]
	   if {$name>=0 && $version>=0} {
	       set name [lindex $_ $name]
	       regexp {^ProductName:(.*)$} $name dummy name
	       set name [string trim $name]
	       set version [lindex $_ $version]
	       regexp {^ProductVersion:(.*)$} $version dummy version
	       set version [string trim $version]
	       append system_info(os) " / $name $version"
	       foreach {major minor} [split $version "."] break
	       catch {unset codename}
	       if {$major == 10} {
		   switch $minor {
		       0 { set codename Cheetah }
		       1 { set codename Puma }
		       2 { set codename Jaguar }
		       3 { set codename Panther }
		       4 { set codename Tiger }
		       5 { set codename Leopard }
		       6 { set codename "Snow Leopard" }
		       7 { set codename Lion }
		       8 { set codename "Mountain Lion" }
		       9 { set codename Mavericks }
		      10 { set codename Yosemite }
		      11 { set codename "El Capitan" }
		   }
	       }
	       if {[info exists codename]} {
		   append system_info(os) " ($codename)"
	       }
	   }
       }
       if {![catch {exec sysctl -n machdep.cpu.brand_string} _]} {
	   set system_info(cputype) $_
       }
       if {![catch {exec sysctl -n hw.packages} _]} {
	   set system_info(processor_count) $_
       }
       if {![catch {exec sysctl -n hw.physicalcpu} _]} {
	   set system_info(core_count) $_
       }
       if {![catch {exec sysctl -n hw.logicalcpu} _]} {
	   set system_info(thread_count) $_
       }
       if {![catch {exec sysctl -n hw.memsize} _]} {
	   set memsize [expr {double($_)/1024./1024./1024.}]
	   set system_info(memsize) [format "%.1f GB" $memsize]
       }
       if {![catch {exec vm_stat} _] && \
	       [regexp "\nPages free:\[ \t\]*(\[0-9\]+)" $_ dummy freemem]} {
	   # freemem is free memory pages, where 1 page = 4096 bytes
	   set system_info(freemem) \
	       [format "%.1f GB" [expr {double($freemem)*4/1024./1024.}]]
       }
   }
}
   
proc WriteJobHeader {} {
   global system_info
   if {![info exists system_info(date)]} {
      CollectSystemInfo
   }
   lappend rh Date $system_info(date)
   lappend rh Machine $system_info(machine_name)
   lappend rh OS $system_info(os)
   if {[info exists system_info(memsize)]} {
      if {[info exists system_info(freemem)]} {
         lappend rh "Total memory" \
             "$system_info(memsize) ($system_info(freemem) free)"
      } else {
         lappend rh "Total memory" $system_info(memsize)
      }
   }
   if {[info exists system_info(hugepage)]} {
      lappend rh "Huge pages" $system_info(hugepage)
   }
   if {[info exists system_info(cputype)]} {
      lappend rh "Cpu type" $system_info(cputype)
   }
   if {[info exists system_info(turbo)]} {
      lappend rh "Turbo mode" $system_info(turbo)
   }
   if {[info exists system_info(processor_count)]} {
      lappend rh "Processors" $system_info(processor_count)
   }
   if {[info exists system_info(core_count)]} {
      lappend rh "Cores" $system_info(core_count)
   }
   if {[info exists system_info(thread_count)]} {
      lappend rh "Threads" $system_info(thread_count)
   }

   set label_width 0
   foreach {label value} $rh {
      if {[string length $label]>$label_width} {
         set label_width [string length $label]
      }
   }
   foreach {label value} $rh {
      puts [format "%*s: %s" $label_width $label $value]
   }
   flush stdout
}

########################################################################
# Defaults for user options
CollectSystemInfo
set sil 10 ;# Number is iterations per stage (not including init stage)
set nos  5 ;# Number of stages after the first, initialization stage
set threads 1
if {$THREADSUPPORT && [info exists system_info(thread_count)]} {
   set threads $system_info(thread_count)
}
set default_cellsize(1) [list 10 20/3. 5 4 20/6.]
set default_cellsize(1b) [list 16 32/3. 8 32/6. 32/7. 4]
set default_cellsize(3) [list 32 16 8 4]
set cellsize {}
set numa none
set tick_digits 1

########################################################################
# Usage
proc Usage {} {
   flush stdout
   global NUMASUPPORT THREADSUPPORT EXEC_STAGE_TEST_TIMEOUT
   global test_number default_cellsize
   global sil nos threads cellsize numa
   global tick_digits default_evolver
   puts -nonewline stderr "Usage: tclsh perftest.tcl \<testnum\>"
   if {$NUMASUPPORT} {
      puts -nonewline stderr " \[-numa \<none\|auto\|\#\[,\#...\]\>\]"
   }
   if {$THREADSUPPORT} {
      puts -nonewline stderr " \[-threads \<\# \[\# ...\]\>\]"
   }
   puts stderr \
       " \[-sil \<\#\>\] \\\n  \
         \[-number_of_stages \<\#\>\]\
         \[-cellsize \<\# \[\# ...\]\>\] \\\n  \
         \[-digits \<\#\>\]\
         \[-evolver \
          \<cg\|rk\|rk2\|rk2heun\|rk4\|rkf54\|rkf54m\|rkf54s\|euler\>\] \\\n  \
         \[-timeout \<\#\>\] \[-showoutput\]\
         \[-v\] \[-info\] \[-efficiency\] \[-speedup\]"
   puts stderr " Where:"
   puts stderr "  testnum is the test number (1, 1b or 3)"
   if {$NUMASUPPORT} {
      puts stderr "  -numa specifies NUMA loading (default $numa)"
   }
   if {$THREADSUPPORT} {
      puts stderr "  -threads is thread count list (default $threads)"
   }
   puts stderr "  -sil is stage iteration limit (default $sil)"
   puts stderr "  -number_of_stages is number of stages (default $nos)"
   if {![info exists test_number] || \
          ![info exists default_cellsize($test_number)]} {
      puts stderr "  -cellsize is cellsize (in nm) list\
                   (default depends on testnum)"
   } else {
      puts stderr "  -cellsize is cellsize (in nm) list\
                   (default $default_cellsize($test_number))"
   }
   puts stderr "  -digits is precision for time output (default $tick_digits)"
   if {[string match {} $default_evolver]} {
      puts stderr "  -evolver selects evolver (options may vary by test)"
   } else {
      puts stderr "  -evolver selects evolver (default $default_evolver)"
   }
   puts stderr "  -timeout sets stage timeout, in seconds;\
                0 to disable, default=$EXEC_STAGE_TEST_TIMEOUT"
   puts stderr "  -showoutput prints run stdout and stderr output"
   puts stderr "  -v for verbose output."
   puts stderr "  -info shows system info and exits"
   puts stderr "  -efficiency to print efficiency table"
   puts stderr "  -speedup to print speedup table"
   exit 1
}

set default_evolver {}
set evolver {}

set help_flag 0
set help_index [lsearch -regexp $argv {^-+(h|help)$}]
if {$help_index>=0} {
   # Process remaining command args and then call Usage
   set help_flag 1
   set argv [lreplace $argv $help_index $help_index]
}

set sil_index [lsearch -regexp $argv {^-+sil$}]
if {$sil_index >= 0 && $sil_index+1 < [llength $argv]} {
   set ul [expr {$sil_index + 1}]
   set val [lindex $argv $ul]
   set argv [lreplace $argv $sil_index $ul]
   if {![regexp {^[1-9][0-9]*$} $val]} {
      puts stderr "ERROR: Option sil must be a positive integer"
      Usage
   }
   set sil $val
}

set nos_index [lsearch -regexp $argv {^-+number_of_stages$}]
if {$nos_index >= 0 && $nos_index+1 < [llength $argv]} {
   set ul [expr {$nos_index + 1}]
   set val [lindex $argv $ul]
   set argv [lreplace $argv $nos_index $ul]
   if {![regexp {^[1-9][0-9]*$} $val]} {
      puts stderr "ERROR: Option number_of_stages must be a positive integer"
      Usage
   }
   set nos $val
}

set EXEC_TEST_TIMEOUT $EXEC_STAGE_TEST_TIMEOUT
## Total allowed test run time
set timeout_index [lsearch -regexp $argv {^-+timeout$}]
if {$timeout_index >= 0 && $timeout_index+1 < [llength $argv]} {
   set ul [expr {$timeout_index + 1}]
   set val [lindex $argv $ul]
   set argv [lreplace $argv $timeout_index $ul]
   if {![regexp {^(-|)[0-9]+$} $val]} {
      puts stderr "ERROR: Option timeout must be an integer"
      Usage
   }
   set EXEC_TEST_TIMEOUT $val
}
set EXEC_TEST_TIMEOUT [expr {($nos+1)*$EXEC_TEST_TIMEOUT}]

if {$NUMASUPPORT} {
   set numa_nodes {}
   set numa_index [lsearch -regexp $argv {^-+numa}]
   if {$numa_index >= 0} {
      set index [expr {$numa_index + 1}]
      while {$index < [llength $argv]} {
         set elt [lindex $argv $index]
         if {[string match -* $elt]} { break }
         set elt [split $elt ","]
         set numa_nodes [concat $numa_nodes $elt]
         incr index
      }
      if {[llength $numa_nodes]<1} {
         puts stderr "ERROR: Empty numa nodes list"
         Usage
      }
      set argv [lreplace $argv $numa_index [expr {$index - 1}]]
      set numa_nodes [join $numa_nodes ","]  ;# Comma separate elements
      ## to match form expected by boxsi -numanodes option.
   }
}


if {$THREADSUPPORT} {
   set threads_index [lsearch -regexp $argv {^-+threads$}]
   if {$threads_index >= 0} {
      set index [expr {$threads_index + 1}]
      set threads {}
      while {$index < [llength $argv]} {
         set elt [lindex $argv $index]
         if {[string match -* $elt]} { break }
         if {![regexp {^[1-9][0-9]*$} $elt]} {
            puts stderr "ERROR: Invalid thread count \"$elt\""
            Usage
         }
         lappend threads $elt
         incr index
      }
      if {[llength $threads]<1} {
         puts stderr "ERROR: Empty thread count list"
         Usage
      }
      set argv [lreplace $argv $threads_index [expr {$index - 1}]]
   }
}

set cellsize_index [lsearch -regexp $argv {^-+cellsize$}]
if {$cellsize_index >= 0} {
   set index [expr {$cellsize_index + 1}]
   set cellsize {}
   while {$index < [llength $argv]} {
      set elt [lindex $argv $index]
      if {[string match -* $elt]} { break }
      if {[catch {eval expr {$elt}} computed_elt]} {
         puts stderr "ERROR: Invalid cellsize \"$elt\""
         Usage
      }
      lappend cellsize $computed_elt
      incr index
   }
   if {[llength $cellsize]<1} {
      puts stderr "ERROR: Empty cellsize list"
      Usage
   }
   set argv [lreplace $argv $cellsize_index [expr {$index - 1}]]
}

set digits_index [lsearch -regexp $argv {^-+digits$}]
if {$digits_index >= 0} {
   set index [expr {$digits_index + 1}]
   set tick_digits [lindex $argv $index]
   set argv [lreplace $argv $digits_index $index]
}
set tickfmt [format "%%7.%df" $tick_digits]

set evolver_index [lsearch -regexp $argv {^-+evolver$}]
if {$evolver_index >= 0} {
   set index [expr {$evolver_index + 1}]
   set evolver [string tolower [lindex $argv $index]]
   set argv [lreplace $argv $evolver_index $index]
}

set efficiency_flag 0
set efficiency_index [lsearch -regexp $argv {^-+efficiency$}]
if {$efficiency_index >= 0} {
   set efficiency_flag 1
   set argv [lreplace $argv $efficiency_index $efficiency_index]
}

set speedup_flag 0
set speedup_index [lsearch -regexp $argv {^-+speedup$}]
if {$speedup_index >= 0} {
   set speedup_flag 1
   set argv [lreplace $argv $speedup_index $speedup_index]
}

set showoutput 0
set showoutput_index [lsearch -regexp $argv {^-+showoutput$}]
if {$showoutput_index >= 0} {
   set showoutput 1
   set argv [lreplace $argv $showoutput_index $showoutput_index]
}

set verbose 0
set verbose_index [lsearch -regexp $argv {^-+v$}]
if {$verbose_index >= 0} {
   set verbose 1
   set argv [lreplace $argv $verbose_index $verbose_index]
}

set info_only 0
set info_index [lsearch -regexp $argv {^-+info$}]
if {$info_index >= 0} {
   set info_only 1
   set argv [lreplace $argv $info_index $info_index]
}

set test_number {}
if {[lsearch -regexp $argv {^-}]<0} {
   # No more option flags
   set test_number [lindex $argv 0]
   set argv [lreplace $argv 0 0]
}

if {[llength $argv]>0} {
   puts stderr "Unrecognized command line arguments: \"$argv\""
   Usage
}

if {[string match {} $test_number]} {
   if {!$info_only} {
      puts stderr "No test specified (should be 1, 1b or 3)"
      Usage
   }
} else {
   switch -exact $test_number {
      1  {
         set MIFFILE [file join $HOME bench1.mif]
         set DIMX 2000   ;# NB: These values must match part dimensions in
         set DIMY 1000   ;#     MIFFILE.
         set DIMZ   20
         set default_evolver cg
      }
      1b {
         set MIFFILE [file join $HOME bench1b.mif]
         set DIMX 2048   ;# NB: These values must match part dimensions in
         set DIMY 1024   ;#     MIFFILE.
         set DIMZ   32
         set default_evolver cg
      }
      3 {
         set MIFFILE [file join $HOME bench3.mif]
         set DIMX 512   ;# NB: These values must match part dimensions in
         set DIMY 512   ;#     MIFFILE.
         set DIMZ 512
         set default_evolver cg
      }
      default {
         puts stderr "Unrecognized test number $test_number; should be 1, 1b or 3"
         Usage
      }
   }
   if {[string match {} $cellsize]} {
      foreach elt $default_cellsize($test_number) {
         if {[catch {eval expr {$elt}} computed_elt]} {
            puts stderr "ERROR: Invalid default cellsize \"$elt\""
            exit 86
         }
         lappend cellsize $computed_elt
      }
   }
   if {[string match {} $evolver]} {
      set evolver $default_evolver
   }
}

if {$help_flag} {
   Usage
}

set check_cellsize {}
foreach cs $cellsize {
   set xcount [expr {int(round($DIMX/$cs))}]
   set ycount [expr {int(round($DIMY/$cs))}]
   set zcount [expr {int(round($DIMZ/$cs))}]
   set xslip [expr {abs($DIMX-$xcount*$cs)/$DIMX}]
   set yslip [expr {abs($DIMY-$ycount*$cs)/$DIMY}]
   set zslip [expr {abs($DIMZ-$zcount*$cs)/$DIMZ}]
   set EPS 1e-13
   if {$xslip>$EPS || $yslip>$EPS || $zslip>$EPS} {
      puts stderr "ERROR: Requested cellsize $cs doesn't divide integrally\
                   into part dimensions $DIMX x $DIMY x $DIMZ"
      exit 999
   }
   set cs [expr {double($DIMZ)/double($zcount)}]
   lappend check_cellsize $cs
   set cc {}
   set ccc [expr {$xcount*$ycount*$zcount}]  ;# Cell count
   set zccc [expr {[PowerOfTwoCeil $xcount]*[PowerOfTwoCeil $ycount] \
                      *[PowerOfTwoCeil $zcount]}]
   set pad_ratio($cs) [format "%.2f" [expr {double($zccc)/$ccc}]]
   while {[string compare $cc $ccc] != 0} {
      # Comma separate cell count string
      set cc $ccc
      regsub {([0-9])([0-9][0-9][0-9])(,|$)} $cc {\1,\2\3} ccc
   }
   set cell_count($cs) $cc
}
set cellsize $check_cellsize
unset check_cellsize

if {$info_only} {
   WriteJobHeader
   exit 0
}



########################################################################
# Clean-up code

if {[string compare "windows" $tcl_platform(platform)]==0} {
   # Kill command on Windows
   set pgmlist {}
   set pgm [auto_execok taskkill]
   if {![string match {} $pgm]} {
      lappend pgm /f /t /PID
      lappend pgmlist $pgm
   }
   set pgm [auto_execok tskill]
   if {![string match {} $pgm]} {
      # tskill will terminate some processes that taskkill claims
      # don't exist.  However, it appears that tskill is only
      # accessible by 64-bit programs.
      lappend pgmlist $pgm
   }
   if {[llength $pgmlist]<2} {
      set pgm [auto_execok pskill]
      if {![string match {} $pgm]} {
         # pskill is part of Windows Sysinternals suite
         lappend pgmlist $pgm
      }
   }
   set KILL_COMMAND [lindex $pgmlist 0]
   set KILL_COMMAND_B [lindex $pgmlist 1]
   # The /t option to taskkill kills child processes
   set KILL_PGREP {}
} else {
   # Kill command on unix. If pgrep is available, use that
   # to get list of all child processes (with -P option).
   # Use signal '-9' to make the kill as forceful as possible.
   set KILL_COMMAND [set KILL_COMMAND_B [auto_execok kill]]
   if {![string match {} $KILL_COMMAND]} {
      lappend KILL_COMMAND -15   ;# SIGTERM
      lappend KILL_COMMAND_B -9  ;# SIGKILL
   }
   set KILL_PGREP [auto_execok pgrep]
   if {![string match {} $KILL_PGREP]} {
      lappend KILL_PGREP -P
   }
}

proc SystemKill { runpid } {
   # Returns a three item list.  The first item is a number;
   # 0 indicates success, anything else is failure.  The
   # second and third items in the list are the eval outputs
   # from the kill command(s).
   global KILL_COMMAND KILL_COMMAND_B KILL_PGREP
   if {[string match {} $KILL_COMMAND]} {
      return [list 1 "No kill commands registered" {}]
   }
   if {![string match {} $KILL_PGREP]} {
      # Note: pgrep returns an error if there are no child processes.
      if {![catch {eval exec $KILL_PGREP $runpid} children]} {
         set runpid [concat $runpid $children]
      }
   }
   set msgkilla [set msgkillb "This space unintentionally blank"]
   if {[set errcode [catch {eval exec $KILL_COMMAND $runpid} msgkilla]]} {
      if {![string match {} $KILL_COMMAND_B]} {
         set errcode [catch {eval exec $KILL_COMMAND_B $runpid} msgkillb]
      } else {
         set msgkillb "No secondary kill command registered"
      }
   } else {
      set msgkillb {}
   }
   ## NB: kill command can fail if runpid exits before kill can get to it.
   return [list $errcode $msgkilla $msgkillb]
}


########################################################################
# Exec command with timeout (timeout in seconds)
proc TimeoutExecReadHandler { chan } {
   global TE_runcode TE_results
   if {![eof $chan]} {
      append TE_results [set data [read $chan]]
      if {[string match {*Boxsi run end.*} $data]} {
         # Handle case where exit blocked by still-running
         # child process.
         set TE_runcode 0
      }
   } else {
      set TE_runcode 0
   }
}
proc TimeoutExec { cmd timeout } {
   # Return codes:
   #  0 => Success
   #  1 => Error
   # -1 => Timeout
   set timeout [expr {$timeout*1000}]  ;# Convert to ms
   global TE_runcode TE_results
   set TE_results {}
   set TE_runcode 0
   set cmd [linsert $cmd 0 {|}]
   set chan [open $cmd r]
   fconfigure $chan -blocking 0 -buffering none
   fileevent $chan readable [list TimeoutExecReadHandler $chan]
   if {$timeout>0} {
      set timeoutid [after $timeout [list set TE_runcode -1]]
   }
   vwait TE_runcode
   if {$timeout>0} { after cancel $timeoutid }
   if {$TE_runcode >= 0} {
      # cmd ran to completion; reset chan to blocking mode
      # so close can return error info.  Don't do this if
      # cmd timed out, because in that case close may block
      # indefinitely.
      fconfigure $chan -blocking 1
   } else {
      # Kill process
      SystemKill [pid $chan]
   }
   if {[catch {close $chan} errmsg]} {
      append TE_results "\nERROR: $errmsg"
      if {$TE_runcode == 0} {
         set TE_runcode 1
      }
   }
   return [list $TE_runcode $TE_results]
}

########################################################################
# mmArchive

# Launch an instance of mmArchive for use by tests.  (If there are other
# instances of mmArchive running, then the tests may end up using one or
# more of those instead.)
#set mmArchive_command [exec_ignore_stderr $TCLSH $OOMMF +command mmArchive +bg]
#set mmArchive_command [linsert $mmArchive_command 0 exec_ignore_stderr]
#set mmArchive_pid [eval $mmArchive_command]
#after 1000

########################################################################
# odtcols
set odtcols_command [exec_ignore_stderr $TCLSH $OOMMF +command odtcols]
lappend odtcols_command -type bare "*Energy calc count" "*Wall time"
lappend odtcols_command "<" $ODTFILE

########################################################################
# Boxsi

set boxsi_base_command [exec_ignore_stderr $TCLSH $OOMMF +command boxsi]
if {$NUMASUPPORT && [llength $numa_nodes]>0} {
   lappend boxsi_base_command -numanodes $numa_nodes
}
lappend boxsi_base_command -logfile $LOGFILE
set parameter_list \
   [list basename $BASENAME \
       archive_nickname $ARCHIVE_NICKNAME \
       evolver $evolver]
if {[info exists sil] && $sil>0} {
   lappend parameter_list sil $sil
}
if {[info exists nos] && $nos>0} {
   lappend parameter_list number_of_stages $nos
}


if {[string compare "Darwin" $tcl_platform(os)] != 0} {
   # On (at least some) Mac OS X, this redirect causes spurious EOFs
   # on unrelated channel reads.  OTOH, this redirect may protect
   # against console problems on Windows.
   lappend boxsi_base_command "<" $nuldevice
}
lappend boxsi_base_command $MIFFILE


########################################################################
# Do runs

catch {file delete $ODTFILE}
if {!$verbose} { puts stderr {} }
flush stderr ; flush stdout
foreach cs $cellsize {
   set xcount [expr {int(round($DIMX/$cs))}]
   set ycount [expr {int(round($DIMY/$cs))}]
   set zcount [expr {int(round($DIMZ/$cs))}]
   set xslip [expr {abs($DIMX-$xcount*$cs)/$DIMX}]
   set yslip [expr {abs($DIMY-$ycount*$cs)/$DIMY}]
   set zslip [expr {abs($DIMZ-$zcount*$cs)/$DIMZ}]
   set EPS 1e-13
   if {$xslip>$EPS || $yslip>$EPS || $zslip>$EPS} {
      puts stderr "ERROR: Requested cellsize $cs doesn't divide integrally\
                   into part dimensions $DIMX x $DIMY x $DIMZ"
      continue
   }
   set cc [expr {$xcount*$ycount*$zcount}]  ;# Cell count

   foreach tc $threads {
      if {!$verbose} {
         puts -nonewline stderr \
            [format "\rRunning cellsize%8.3f, thread count %3d    " $cs $tc]
         flush stderr
      }
      set boxsi_command $boxsi_base_command
      if {$THREADSUPPORT} {
         lappend boxsi_command -threads $tc
      }
      lappend boxsi_command -parameters [concat $parameter_list cellsize $cs]
      set boxsi_command [concat $boxsi_command $ERR_REDIRECT]

      set start_time [clock seconds]
      set run_result [TimeoutExec $boxsi_command $EXEC_TEST_TIMEOUT]
      set run_time [expr {[clock seconds]-$start_time}]
      if {$verbose} {
         puts [format "Results for cell size %2s nm; cellcount %10s;\
                 thread count %2d; runtime %3s seconds:" \
                   $cs $cc $tc $run_time]
      }
      if {[lindex $run_result 0] == -1} {
         puts stderr "Timeout; cell size=$cs, thread count=$tc\
                      (>$EXEC_TEST_TIMEOUT seconds)"
      } elseif {[lindex $run_result 0] != 0} {
         puts stderr "Run error; cell size=$cs, thread count=$tc"
      } else {
         if {$showoutput} {
            flush stderr
            puts "RUN OUTPUT >>>>"
            puts [string trim [lindex $run_result 1]]
            puts "<<<< RUN OUTPUT"
            flush stdout
         }
         set timing_data [split [eval exec $odtcols_command] "\n"]
         foreach {base_evals base_time} [lindex $timing_data 0] break
         set stage 1
         foreach line [lrange $timing_data 1 end] {
            foreach {evals time} $line break
            if {$evals > $base_evals} {
               set time_per_eval \
                   [expr {($time - $base_time)/($evals - $base_evals)}]
               if {$verbose} {
                  puts [format "Stage %2d: %5.2f seconds/eval" \
                            $stage $time_per_eval]
               }
               set base_evals $evals
               set base_time $time
               lappend results([list $cs $tc]) $time_per_eval
            }
            incr stage
         }
      }
      catch {file delete $ODTFILE}
   }
}
if {!$verbose} {
   puts -nonewline stderr "\r                                              \r"
   flush stderr
}

# Kill mmArchive
exec_ignore_stderr $TCLSH $OOMMF killoommf mmArchive$ARCHIVE_NICKNAME

########################################################################
# Output
WriteJobHeader

########################################################################
# Results table setup.  Efficiency and speedup tables use nearly
# identical layout.
set cellsize [lsort -real -unique -decreasing $cellsize]
set threads [lsort -integer -unique -increasing $threads]
set cs_width 0
set cc_width 0
set cr_width 0
foreach cs $cellsize {
   set cstmp [string trimright [string trimright [format "%6.3f" $cs] "0"] "."]
   set cellsize_formatted($cs) $cstmp
   if {[string length $cstmp]>$cs_width} {
      set cs_width [string length $cstmp]
   }
   set cc $cell_count($cs)
   if {[string length $cc]>$cc_width} {
      set cc_width [string length $cc]
   }
   set cr $pad_ratio($cs)
   if {[string length $cr]>$cr_width} {
      set cr_width [string length $cr]
   }
   foreach tc $threads {
      if {![info exists results([list $cs $tc])]} {
         # No results, probably due to run error
         continue
      }
      set tick_list [lsort -real $results([list $cs $tc])]
      if {[llength $tick_list]<1} { continue }
      if {$verbose} {
         set ta [expr {[lindex $tick_list 0]*1000.}]
         set tz [expr {[lindex $tick_list end]*1000.}]
         puts [format \
               "Cell size=$cs, thread count=$tc, time range: %.2f - %.2f (ms)" \
               $ta $tz]
      }
      set tick_sum 0.0
      foreach ticks $tick_list {
         set tick_sum [expr {$tick_sum + $ticks}]
      }
      set ave [expr {$tick_sum / double([llength $tick_list])}]
      # Check for outliers
      if {[lindex $tick_list end] > $OUTLIER_BAR*$ave} {
         set tick_count 0
         set tick_sum 0.0
         foreach ticks $tick_list {
            if {$ticks > $OUTLIER_BAR*$ave} { break }
            set tick_sum [expr {$tick_sum + $ticks}]
            incr tick_count
         }
         set ave [expr {$tick_sum / $tick_count}]
         if {$verbose || 2*$tick_count < [llength $tick_list]} {
            puts "\nWARNING: Dropping\
               [expr {[llength $tick_list] - $tick_count}]\
               outliers for cell size=$cs, thread count=$tc"
            puts " Stage eval times (seconds): $results([list $cs $tc])"
         }
      }
      set ave_tick([list $cs $tc]) $ave
   }
}
set tc_width [string length [lindex $threads end]]
set tc_label_width 7
if {$tc_label_width < $tc_width} {
   set tc_label_width $tc_width
}
set tce_label_width 5 ;# For efficiency table
if {$tce_label_width < $tc_width} {
   set tce_label_width $tc_width
}

set cs_label_width 14
if {$cs_label_width < $cs_width} {
   set cs_label_width $cs_width
}
set cs_lead [expr {($cs_label_width-$cs_width)/2}]
set cs_width [expr {$cs_label_width-$cs_lead}]

set cc_label_width 10
if {$cc_label_width < $cc_width} {
   set cc_label_width $cc_width
}
set cc_width [expr {$cc_width + ($cc_label_width-$cc_width+1)/2 + 2}]
set cc_tail [expr {$cc_label_width - ($cc_width - 2)}]

set cr_label "Pad ratio"
set cr_label_width [string length $cr_label]

if {$cc_label_width < $cr_width} {
   set cr_label_width $cr_width
}
set cr_width [expr {$cr_width + ($cr_label_width-$cr_width+1)/2 + 2}]
set cr_tail [expr {$cr_label_width - ($cr_width - 2)}]

set table_left_width [expr {$cs_label_width + $cc_label_width \
                               + $cr_label_width + 6}]
set table_right_width [expr {[llength $threads]*($tc_label_width+1)+1}]

set thread_header " --- Threads --- "
set thl [string length $thread_header]
set tc_lead 0
set tc_tail 0
if {$table_right_width < $thl} {
   set tc_lead [expr {($thl - $table_right_width)/2}]
   set tc_tail [expr {$thl - $table_right_width - $tc_lead}]
   set table_right_width $thl
}

set table_header \
   " *** Test $test_number-$evolver, time per field evaluation, in ms *** "
set th_width [string length $table_header]
set table_width [expr {$table_left_width+$table_right_width+1}]
if {$table_width < $th_width} {
   set padding [expr {$th_width - $table_width}]
   incr table_right_width $padding
   set table_width $th_width
   incr tc_lead [expr {$padding/2}]
   incr tc_tail [expr {$padding - $padding/2}]
}
set th_width [expr {($table_width+$th_width)/2}]

set left_fill {}
for {set i 0} {$i<$table_left_width} {incr i} { append left_fill "-" }
set right_fill {}
for {set i 0} {$i<$table_right_width} {incr i} { append right_fill "-" }

########################################################################
# Write timing table header
puts "\n+${left_fill}-${right_fill}+"
puts [format "|%*s%*s|" \
         $th_width $table_header [expr {$table_width-$th_width}] ""]
puts "+${left_fill}+${right_fill}+"

set thread_header "--- Threads ---"
set tt_width [expr {($table_right_width+[string length $thread_header])/2}]
puts [format "|%*s|%*s%*s|" $table_left_width "" $tt_width $thread_header \
         [expr {$table_right_width - $tt_width}] ""]

puts -nonewline [format "| %*s  %*s  %*s |" \
                     $cs_label_width "Cell size (nm)" \
                     $cc_label_width "Cell count" \
                     $cr_label_width "Pad ratio"]
puts -nonewline [format "%*s" [expr {$tc_lead + $tc_label_width - 1}] \
                    [lindex $threads 0]]
foreach tc [lrange $threads 1 end] {
   puts -nonewline [format " %*s" $tc_label_width $tc]
}
puts [format "%*s |\n+${left_fill}+${right_fill}+" [expr {$tc_tail+2}] ""]

########################################################################
# Write time table data
foreach cs $cellsize {
   puts -nonewline [format "| %*s%-*s%*s%*s%*s%*s |%*s" \
                       $cs_lead "" \
                       $cs_width $cellsize_formatted($cs) \
                       $cc_width $cell_count($cs) \
                       $cc_tail "" \
                       $cr_width $pad_ratio($cs) \
                       $cr_tail "" \
                       $tc_lead ""]
   foreach tc $threads {
      if {[info exists ave_tick([list $cs $tc])]} {
         puts -nonewline \
            [format " $tickfmt" [expr {1000.*$ave_tick([list $cs $tc])}]]
      } else {
         puts -nonewline [format " %7s" "   -   "]
      }
   }
   puts [format "%*s |" $tc_tail ""]
}
puts "+${left_fill}+${right_fill}+"

########################################################################
# Efficiency results
if {$efficiency_flag && [llength $threads]>1} {
   # Write efficiency table header
   set table_eff_right_width \
       [expr {[llength $threads]*($tce_label_width+1)+1}]
   set tce_lead 0
   set tce_tail 0
   if {$table_eff_right_width < $thl} {
      set tce_lead [expr {($thl - $table_eff_right_width)/2}]
      set tce_tail [expr {$thl - $table_eff_right_width - $tce_lead}]
      set table_eff_right_width $thl
   }


   set table_eff_header \
      " *** Test $test_number-$evolver, threading efficiency *** "
   set teh_width [string length $table_eff_header]
   set table_eff_width [expr {$table_left_width+$table_eff_right_width+1}]
   if {$table_eff_width < $teh_width} {
      set padding [expr {$teh_width - $table_eff_width}]
      incr table_eff_right_width $padding
      set table_eff_width $teh_width
      incr tce_lead [expr {$padding/2}]
      incr tce_tail [expr {$padding - $padding/2}]
   }
   set teh_width [expr {($table_eff_width+$teh_width)/2}]

   set right_eff_fill {}
   for {set i 0} {$i<$table_eff_right_width} {incr i} {
      append right_eff_fill "-"
   }

   puts "\n+${left_fill}-${right_eff_fill}+"
   puts [format "|%*s%*s|" $teh_width $table_eff_header \
             [expr {$table_eff_width-$teh_width}] ""]
   puts "+${left_fill}+${right_eff_fill}+"

   set tet_width [expr {($table_eff_right_width + \
                             [string length $thread_header])/2}]
   puts [format "|%*s|%*s%*s|" $table_left_width "" $tet_width $thread_header \
             [expr {$table_eff_right_width - $tet_width}] ""]

   puts -nonewline [format "| %*s  %*s  %*s |" \
                       $cs_label_width "Cell size (nm)" \
                       $cc_label_width "Cell count" \
                       $cr_label_width "Pad ratio"]
   puts -nonewline [format "%*s" [expr {$tce_lead + $tce_label_width - 1}] \
                        [lindex $threads 0]]
   foreach tc [lrange $threads 1 end] {
      puts -nonewline [format " %*s" $tce_label_width $tc]
   }
   puts [format "%*s |\n+${left_fill}+${right_eff_fill}+" \
             [expr {$tce_tail+2}] ""]

   # Write efficiency table data
   foreach cs $cellsize {
      puts -nonewline [format "| %*s%-*s%*s%*s%*s%*s |%*s" \
                          $cs_lead "" \
                          $cs_width $cellsize_formatted($cs) \
                          $cc_width $cell_count($cs) \
                          $cc_tail "" \
                          $cr_width $pad_ratio($cs) \
                          $cr_tail "" \
                          $tc_lead ""]
      set base_tc [lindex $threads 0]
      if {![info exists ave_tick([list $cs $base_tc])]} {
         foreach tc $threads {
            puts -nonewline [format " %5s" "  -  "]
         }
      } else {
         set base_time [expr {$ave_tick([list $cs $base_tc]) \
                                  *double($base_tc)}]
         foreach tc $threads {
            if {[info exists ave_tick([list $cs $tc])]} {
               set eff [expr {$base_time/$ave_tick([list $cs $tc]) \
                                  /double($tc)}]
               puts -nonewline [format " %5.2f" $eff]
            } else {
               puts -nonewline [format " %5s" "  -  "]
            }
         }
      }
      puts [format "%*s |" $tce_tail ""]
   }
   puts "+${left_fill}+${right_eff_fill}+"
}
########################################################################

########################################################################
# Speedup results
if {$speedup_flag && [llength $threads]>1} {
   # Write speedup table header
   set table_speed_right_width \
       [expr {[llength $threads]*($tce_label_width+1)}]
   set tce_lead 0
   set tce_tail 0
   if {$table_speed_right_width < $thl} {
      set tce_lead [expr {($thl - $table_speed_right_width)/2}]
      set tce_tail [expr {$thl - $table_speed_right_width - $tce_lead}]
      set table_speed_right_width $thl
   }


   set table_eff_header \
      " *** Test $test_number-$evolver, speed-ups *** "
   set teh_width [string length $table_eff_header]
   set table_eff_width [expr {$table_left_width+$table_speed_right_width+1}]
   if {$table_eff_width < $teh_width} {
      set padding [expr {$teh_width - $table_eff_width}]
      incr table_speed_right_width $padding
      set table_eff_width $teh_width
      incr tce_lead [expr {$padding/2}]
      incr tce_tail [expr {$padding - $padding/2}]
   }
   set teh_width [expr {($table_eff_width+$teh_width)/2}]

   set right_speed_fill {}
   for {set i 0} {$i<$table_speed_right_width} {incr i} {
      append right_speed_fill "-"
   }

   puts "\n+${left_fill}-${right_speed_fill}+"
   puts [format "|%*s%*s|" $teh_width $table_eff_header \
             [expr {$table_eff_width-$teh_width}] ""]
   puts "+${left_fill}+${right_speed_fill}+"

   set tet_width [expr {($table_speed_right_width + \
                             [string length $thread_header])/2}]
   puts [format "|%*s|%*s%*s|" $table_left_width "" $tet_width $thread_header \
             [expr {$table_speed_right_width - $tet_width}] ""]

   puts -nonewline [format "| %*s  %*s  %*s |" \
                       $cs_label_width "Cell size (nm)" \
                       $cc_label_width "Cell count" \
                       $cr_label_width "Pad ratio"]
   puts -nonewline [format "%*s" [expr {$tce_lead + $tce_label_width - 1}] \
                        [lindex $threads 0]]
   foreach tc [lrange $threads 1 end] {
      puts -nonewline [format " %*s" $tce_label_width $tc]
   }
   puts [format "%*s |\n+${left_fill}+${right_speed_fill}+" \
             [expr {$tce_tail+1}] ""]

   # Write efficiency table data
   foreach cs $cellsize {
      puts -nonewline [format "| %*s%-*s%*s%*s%*s%*s |%*s" \
                          $cs_lead "" \
                          $cs_width $cellsize_formatted($cs) \
                          $cc_width $cell_count($cs) \
                          $cc_tail "" \
                          $cr_width $pad_ratio($cs) \
                          $cr_tail "" \
                          $tc_lead ""]
      set base_tc [lindex $threads 0]
      if {![info exists ave_tick([list $cs $base_tc])]} {
         foreach tc $threads {
            puts -nonewline [format " %5s" "  -  "]
         }
      } else {
         set base_time [expr {$ave_tick([list $cs $base_tc]) \
                                  *double($base_tc)}]
         foreach tc $threads {
            if {[info exists ave_tick([list $cs $tc])]} {
               set speedup [expr {$base_time/$ave_tick([list $cs $tc])}]
               puts -nonewline [format "%5.1f " $speedup]
            } else {
               puts -nonewline [format "%5s " "  -  "]
            }
         }
      }
      puts [format "%*s|" $tce_tail ""]
   }
   puts "+${left_fill}+${right_speed_fill}+"
}
########################################################################
