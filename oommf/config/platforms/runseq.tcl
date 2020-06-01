# File: runseq.tcl
# Purpose: Automates build and run regression tests.
#
# To use, create a oommf/config/platform/local/buildtest-details.tcl file
# with "MACHINE SPECIFIC DETAILS" as described below.  Then run "tclsh
# runseq.tcl".
#
# Each stanza below should set the following variables to a list of
# values to test:
#
#   initscript   Script to run before build, to initialize environment
#   compiler     compiler strings, as used in platform files
#   cpuarch      usually two values, generic and host
#   real8m       double or long double
#   realwide     double or long double
#   dothreads    0 or 1
#   donuma       0 or 1
#   sselevel     {} or level
#   ddbasetype   {} or setting for SetValue xp_doubledouble_basetype
#   ddaltsingle  {} or setting for SetValue xp_doubledouble_altsingle
#   numanodes    Node list, none or auto
#   threadcount  List of thread counts
#
# The platforms file for the platform should have the following lines at
# near the top of the file, after the platforms/local/ sourcing:
#
#   if {[info exists env(OOMMF_BUILDTEST)] && $env(OOMMF_BUILDTEST)} {
#      source [file join [file dirname [info script]] buildtest.tcl]
#      return
#   }
#
# This will redirect the build process to load the buildtest.tcl file
# when the build is run from inside this script, since this script sets
# the OOMMF_BUILDTEST environment variable to 1.  The buildtest.tcl file
# touches the file buildtest.tcl-touch to signal that it was properly
# sourced.

set runseq_start_time [clock seconds]

# Robust puts that doesn't squawk if stdout is broken
rename puts stock_puts
proc puts { args } {
   if {[catch {eval stock_puts $args} errmsg]} {
      if {[regexp {^error writing [^:]+: broken pipe} $errmsg]} {
         # Ignore broken pipe and exit
         exit
      } else {
         error $errmsg
      }
   }
}

set VERBOSE 0  ;# For debugging

proc Usage {} {
   puts stderr {Usage: tclsh runseq.tcl [-h] [-l]\
                [-buildonly] [-showhosts] [testno ...]}
   puts stderr { where  -h  prints this help message}
   puts stderr {        -l  lists test details, w/o running tests}
   puts stderr {        -buildonly skips the regression run tests}
   puts stderr {        -showhosts lists known machines}
   puts stderr {        testno are individual test numbers\
                            (default is all tests)}
   exit 1
}

if {[lsearch -regexp $argv {^-+(h|help)$}]>=0} { Usage }

set show_hosts_flag 0
set show_hosts_index [lsearch -regexp $argv {^-+showhosts$}]
if {$show_hosts_index>=0} {
   set show_hosts_flag 1
   set argv [lreplace $argv $show_hosts_index $show_hosts_index]
}

##########
# This is the tclsh used to launch child OOMMF processes.  More
# generally, we may want to vary this to test builds with different
# Tcl's.
set DEFAULT_TCLSH [info nameofexecutable]
if {![catch {file normalize $TCLSH} _]} {
   # 'file normalize' new in Tcl 8.4
   set DEFAULT_TCLSH $_
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

##########
#
# tclplatform.tcl script; should be in same directory as current script.
set TCLPLATFORM_SCRIPT [file join [file dirname [file join \
                                  [pwd] [info script]]] tclplatform.tcl]

##########
#
# OOMMF script; take path to current script and back up until
# something named oommf* is hit.
#
set pathlist [file split [file dirname [file join [pwd] [info script]]]]
set rootindex [lindex [lsearch -all -glob $pathlist "oommf*"] end]
if {[llength $rootindex]==1 && $rootindex<0} {
   puts stderr "ERROR: Unable to extract OOMMF root directory"
   exit 1
}
incr rootindex
set pathlist [lreplace $pathlist $rootindex end]
set OOMMF [eval file join $pathlist oommf.tcl]
if {![file exists $OOMMF]} {
   puts stderr "ERROR: Unable to local oommf.tcl"
   exit 1
}
#
##########

#####################################################################
###
### MACHINE SPECIFIC DETAILS
###
#####################################################################
set env(OOMMF_BUILDTEST) 1  ;# For platform files
set hostname [info hostname]
set detailsfile [file join [file dirname $OOMMF] \
                     config platforms local buildtest-details.tcl]
if {![file exists $detailsfile]} {
      puts stderr "ERROR: Build test details file \"$detailsfile\" \
                   does not exist."
      exit 1
}
if {[catch {source $detailsfile} errmsg]} {
   puts stderr "ERROR SOURCING DETAILS FILE: $errmsg"
}
if {![info exists buildtest_labels] && !$show_hosts_flag} {
   puts stderr \
      "Variable buildtest_labels not set by build test details file"
   puts stderr "\"$detailsfile\""
   puts stderr "Is machine $hostname known to details file?"
   exit 1
}
if {![info exists buildtest_values] && !$show_hosts_flag} {
   puts stderr \
      "Variable buildtest_values not set by build test details file"
   puts stderr "\"$detailsfile\""
   puts stderr "Edit details file to fix."
   exit 1
}

#
##########

##########
#
# Command line options parsing
#
set list_tests_flag 0
set list_tests_index [lsearch -regexp $argv {^-+(l|list)$}]
if {$list_tests_index>=0} {
   set list_tests_flag 1
   set argv [lreplace $argv $list_tests_index $list_tests_index]
}

set build_only_flag 0
set build_only_index [lsearch -regexp $argv {^-+buildonly$}]
if {$build_only_index>=0} {
   set build_only_flag 1
   set argv [lreplace $argv $build_only_index $build_only_index]
}

set test_requests {}
foreach elt $argv {
   if {[regexp -- {^([0-9]*)-([0-9]*)$} $elt dummy start end]} {
      if {[string match {} $start]} { set start 0 }
      if {[string match {} $end]} { set end [llength $buildtest_values] }
      if {$start <= $end} {
         # Range request
         for {set i $start} {$i<=$end} {incr i} {
            lappend test_requests $i
         }
      } else {
         lappend test_requests $elt
      }
   } else {
      lappend test_requests $elt
   }
}


if {![regexp -- {^[0-9]*$} [join $test_requests {}]]} {
   puts stderr "Invalid test request."
   Usage
}

if {!$list_tests_flag && !$show_hosts_flag} {
   puts "runseq start time: [clock format $runseq_start_time]"
}
#
##########

#####################################################################
###
### MACHINE SPECIFIC OPTIONS
###
#####################################################################
if {$show_hosts_flag} {
   if {[llength [info command ListHosts]]!=1} {
      puts stderr "ERROR: Build test details file \"$detailsfile\" \
                   does not define a ListHosts command."
      exit 1
   }
   set index 0
   set match_index -1
   foreach elt [ListHosts] {
      puts [format "%2d) $elt" [incr index]]
      if {[string compare $hostname $elt]==0} {
         set match_index $index
      }
   }
   if {$match_index<0} {
      puts stderr "Current host $hostname not on list"
   } else {
      puts stderr "Current host is entry $match_index"
   }
   exit
}

# Check that test labels match known labels
set known_labels [list tclsh initscript compiler cpuarch \
                     real8m realwide dothreads donuma numanodes \
                     threadcount sselevel ddbasetype ddaltsingle]
foreach label $buildtest_labels {
   if {[lsearch -exact $known_labels $label]<0} {
      puts stderr "ERROR: Unknown test label: \"$label\""
      puts stderr " Known labels: $known_labels"
      exit 1
   }
}
#
##########

proc CleanErrorMessage { msg } {
   regsub -all "child process exited abnormally\[\n\]*" $msg {} msg
   regsub -all "oommf.tcl *\[.0-9\]+ *panic:\n" $msg {} msg
   set msg [string trimright [string trimleft $msg "\n"]]
   return $msg
}

proc DoInit { initscript } {
   global ERR_REDIRECT tcl_platform env

   if {[string compare windows $tcl_platform(platform)]==0} {
      # Windows environment
      set cmd [list $env(ComSpec) /c [list $initscript >nul: 2>nul: && set]]

      if {[catch {eval exec $cmd $ERR_REDIRECT} result]} {
         puts stderr "Error running environment initialization script\
                \"$initscript\""
         exit 2
      }

      set arraylist {}
      foreach line [split $result "\n"] {
         if {![regexp -- {^([^=]+)=(.*)$} $line dummy name value]} {
            puts stderr "Bad env line: $line"
            exit 3
         }
         lappend arraylist $name $value
      }
      return $arraylist
   }
   puts stderr "ERROR: No initscript support for platform\
                       $tcl_platform(platform)"
   exit 1
}

proc ConstructExePath { exename } {
   # On Windows, the Tcl exec command throws the directory containing
   # the running executable onto the front of the search path.  This is
   # not what we want for this test script, so instead we explicitly
   # construct the path from the env variable.
   global tcl_platform env
   if {[info exists tcl_platform(pathSeparator)]} {
      set sc $tcl_platform(pathSeparator)
   } elseif {[string compare windows $tcl_platform(platform)]==0} {
      set sc {;}
   } else {
      set sc {:}
   }
   auto_reset
   foreach dir [string trim [split $env(PATH) $sc]] {
      if {[string match {} $dir]} { continue }
      set testname [auto_execok [file join $dir $exename]]
      if {![string match {} $testname]} {
         set fullname $testname
         break
      }      
   }
   if {![info exists fullname]} {
      puts stderr "ERROR: buildtest.tcl can't find\
                   executable \"$exename\" in path $env(PATH)"
      exit 1
   }
   return $fullname
}

proc DoBuild {} {
   global TCLSH OOMMF VERBOSE ERR_REDIRECT
   set touchtime [clock seconds]
   set oommfroot [file dir $OOMMF]
   set touchfile [file join $oommfroot config platforms buildtest.tcl-touch]

   for {set attempt 0} {$attempt<5} {incr attempt} {
      set catchcode [catch {exec $TCLSH $OOMMF pimake -cwd $oommfroot \
                               distclean $ERR_REDIRECT} result]
      if {$VERBOSE} { puts $result ; flush stdout }
      if {$catchcode} {
         after 5000  ;# Distclean failed.  On Windows this probably means
         ## application shutdown from previous regression test hasn't
         ## completed.  Wait 5 seconds and try again.
      } else {
         break  ;# Distclean succeeded.
      }
   }
   if {$attempt >= 5} {
      return -code error "PIMAKE DISTCLEAN FAILED: $result"
   }
   if {![file exists $touchfile] || [file mtime $touchfile]<$touchtime} {
      puts stderr "ERROR: buildtest.tcl not sourced by pimake distclean"
      puts "Touchtime 1: [clock format $touchtime -format %H:%M:%S]"
      puts "Touchtime 2: [clock format [file mtime $touchfile] \
                          -format %H:%M:%S]"
      puts "Check that the platform file sources buildtest.tcl"
      exit 1
   }
   set touchtime [clock seconds]

   set result [exec $TCLSH $OOMMF pimake -cwd $oommfroot $ERR_REDIRECT]
   if {$VERBOSE} { puts $result ; flush stdout }
   if {![file exists $touchfile] || [file mtime $touchfile]<$touchtime} {
      puts stderr "ERROR: buildtest.tcl not sourced by pimake"
      exit 1
   }
}

proc RunRegressionSuite {} {
   global TCLSH OOMMF ERR_REDIRECT VERBOSE
   set oommfroot [file dir $OOMMF]
   set regress [file join $oommfroot app oxs regression_tests runtests.tcl]
   if {$VERBOSE} {
      puts "Running cmd: $TCLSH $regress"
   }
   set results {}
   if {[catch {exec $TCLSH $regress $ERR_REDIRECT} results]} {
      # At least some errors
      # Following code requires recent Tcl
      set errdata {}
      catch {
         if {[regexp -line \
                 {^Comparison failures: (.*)$} $results dummy badblocks]} {
            set errblocks [regexp -inline -all ".*?\n---\n" $results]
            foreach elt $badblocks {
               set index [lsearch -regexp $errblocks "^TEST \[^:\]*: $elt ---.*"]
               if {$index<0} {
                  append errdata "*** Can't match error block $elt ***\n"
               } else {
                  append errdata [lindex $errblocks $index]
               }
            }
         }
      }
      set rlist [split $results "\n"]
      set sumindex \
         [lindex [lsearch -all -regexp $rlist {^-* *SUMMARY *-*$}] end]
      if {[llength $sumindex]==1 && $sumindex >= 0} {
         set results [join [lrange $rlist [incr sumindex] end] "\n   "]
         set results "   $results"
      }
      if {![string match {} $errdata]} {
         regsub -all "\n" $errdata "\n   " errdata
         append results "*** Error details *******************\n   "
         append results $errdata
         append results "*************************************"
      }
      return -code error $results
   }
   if {$VERBOSE} { puts $results ; flush stdout }
   # Otherwise, return the number of tests passed
   set rlist [split $results "\n"]
   set sumindex \
      [lindex [lsearch -all -regexp $rlist {^-* *SUMMARY *-*$}] end]
   if {[llength $sumindex]!=1 || $sumindex < 0} {
      return -code error "Regression tests results parse error."
   }
   set rlist [lrange $rlist [incr sumindex] end]
   set passed_index [lsearch -regexp $rlist {^Tests passed:}]
   if {$passed_index<0 || ![regexp {^Tests passed: *([0-9]+) *$} \
                               [lindex $rlist $passed_index] \
                               dummy passno]} {
      return -code error "Regression tests summary parse error."
   }
   return $passno
}

# Save initial environment
set env_orig [array get env]

# Default test control values:
set tcv(tclsh)        {}      ;# default is DEFAULT_TCLSH
set tcv(initscript)   {}
set tcv(compiler)     "g++ -c"
set tcv(cpuarch)      "generic"
set tcv(real8m)       "double"
set tcv(realwide)     "long double"
set tcv(dothreads)    0
set tcv(donuma)       0
set tcv(numanodes)    {}      ;# default depending on tcv(donuma)
set tcv(threadcount)  1

set testno 0
set badtests {}

if {$list_tests_flag} {
   puts "Test details listing:"
}
foreach test $buildtest_values {
   incr testno
   if {[llength $test_requests]>0} {
      if {[lsearch -exact $test_requests $testno]<0} {
         # Skip test
         continue
      }
   }

   lappend badtests $testno  ;# testno is removed from badtest iff
   ## test runs to completion without errors.

   # Make sure environment is the same at the start of each test.
   # NOTE: 'unset env' breaks the magic connection between the Tcl array
   # "env" and the shell environment.  So, instead, unset each element
   # of env one-by-one.
   foreach elt [array names env] { unset env($elt) }
   array set env $env_orig

   puts "------------------------------------------------"
   puts -nonewline \
      [format "Test %2d/%2d:" $testno [llength $buildtest_values]]
   if {$list_tests_flag} {
      puts {}
   } else {
      puts " [clock format [clock seconds] -format %H:%M:%S]"
   }
   if {[llength $test] != [llength $buildtest_labels]} {
      puts stderr "-----------\
         \nERROR BAD TEST: Number of items differs from number of labels."
      puts "Labels: $buildtest_labels"
      puts "  Test: $test"
      exit 1
   }

   foreach name $buildtest_labels value $test {
      set tcv($name) $value
      puts [format "--- %13s: %s" $name $value]
   }
   flush stdout

   # Make thread count request a proper Tcl list
   if {[info exists tcv(threadcount)]} {
      set tcv(threadcount) [join [regsub -all {,} $tcv(threadcount) " "]]
      set newcount {}
      foreach elt $tcv(threadcount) {
         if {[regexp -- {^([0-9]*)-([0-9]*)$} $elt dummy start end]} {
            for {set i $start} {$i<=$end} {incr i} {
               lappend newcount $i
            }
         } else {
            lappend newcount $elt
         }
      }
      set tcv(threadcount) $newcount
   }

   # Setup environment variables used for communication with children
   set env(OOMMF_BUILDTEST_COMPILER) $tcv(compiler)
   set env(OOMMF_BUILDTEST_CPUARCH)  $tcv(cpuarch)
   set env(OOMMF_BUILDTEST_REAL8m)   $tcv(real8m)
   set env(OOMMF_BUILDTEST_REALWIDE) $tcv(realwide)
   set env(OOMMF_BUILDTEST_THREADS)  $tcv(dothreads)
   set env(OOMMF_BUILDTEST_NUMA)     $tcv(donuma)
   if {![string match {} $tcv(numanodes)]} {
      set env(OOMMF_NUMANODES)       $tcv(numanodes)
   } elseif $tcv(donuma) {
      set env(OOMMF_NUMANODES)       auto
   } else {
      if {[info exists env(OOMMF_NUMANODES)]} {
         unset env(OOMMF_NUMANODES)
      }
   }
   if {[info exists tcv(sselevel)] && \
          ![string match {} $tcv(sselevel)]} {
      set env(OOMMF_BUILDTEST_SSELEVEL)     $tcv(sselevel)
   }
   if {[info exists tcv(ddbasetype)] && \
          ![string match {} $tcv(ddbasetype)]} {
      set env(OOMMF_BUILDTEST_DOUBLEDOUBLE_BASETYPE) $tcv(ddbasetype)
   }
   if {[info exists tcv(ddaltsingle)] && \
          ![string match {} $tcv(ddaltsingle)]} {
      set env(OOMMF_BUILDTEST_DOUBLEDOUBLE_ALTSINGLE) $tcv(ddaltsingle)
   }

   if {$list_tests_flag} { continue }

   if {[llength $tcv(initscript)]>0} {
      puts -nonewline "Initializing environment..." ; flush stdout
      if {[catch {DoInit $tcv(initscript)} result]} {
         puts "ERROR"; flush stdout
         set errmsg [CleanErrorMessage $result]
         puts stderr "ERROR INIT: $errmsg"
         flush stderr
         continue
      }
      if {[catch {array set env $result} msg]} {
         puts "ERROR"; flush stdout
         puts stderr "ERROR INIT: $msg"
         flush stderr
         continue
      }
      puts "OK" ; flush stdout
   }

   global TCLSH
   if {[string match {} $tcv(tclsh)]} {
      set tcv(tclsh) $DEFAULT_TCLSH
   }
   set TCLSH [ConstructExePath $tcv(tclsh)]

   # Check compatibility of TCLSH with dothreads request
   if {$tcv(dothreads)} {
      if {[catch {exec $TCLSH $TCLPLATFORM_SCRIPT $ERR_REDIRECT} result]} {
         puts stderr "ERROR running $TCLPLATFORM_SCRIPT:\n$result"
         continue
      }
      catch {unset platform_check}
      array set platform_check $result
      if {![info exists platform_check(threaded)] \
             || !$platform_check(threaded)} {
         puts stderr "ERROR: Threaded build request with non-threaded Tcl"
         continue
      }
      unset platform_check
   }

   puts -nonewline "Building..." ; flush stdout
   set build_start_time [clock seconds]
   if {[catch {DoBuild} errmsg]} {
      puts "ERROR" ; flush stdout
      set errmsg [CleanErrorMessage $errmsg]
      puts stderr "ERROR BUILD: $errmsg"
      flush stderr
      continue
   }
   set build_stop_time [clock seconds]
   set build_time [expr { $build_stop_time - $build_start_time}]
   set build_secs [expr {$build_time % 60}]
   set build_mins [expr {($build_time-$build_secs)/60}]
   puts [format "OK (time %2d:%02d)" $build_mins $build_secs]
   flush stdout

   if {$build_only_flag} {
      set badtests [lreplace $badtests end end]
      continue
   }

   set test_ok 1
   foreach threadcount $tcv(threadcount) {
      set env(OOMMF_THREADS) $threadcount
      if {$threadcount == 1} {
         puts -nonewline "Running regression tests (1 thread)...."
      } else {
         puts -nonewline "Running regression tests ($threadcount threads)..."
      }
      flush stdout
      set regression_start_time [clock seconds]
      if {$VERBOSE} {
         foreach t [array names env OOMMF*] { puts "env($t): \"$env($t)\"" }
      }
      if {[catch {RunRegressionSuite} results]} {
         set test_ok 0
         set errmsg [CleanErrorMessage $results]
         set stop_time [clock seconds]
         set regtime [expr {$stop_time - $regression_start_time}]
         set regsecs [expr {$regtime % 60}]
         set regmins [expr {($regtime-$regsecs)/60}]
         puts [format "ERROR (time %2d:%02d)" $regmins $regsecs]
         flush stdout
         puts stderr "ERROR REGRESSION:\n$errmsg"
         flush stderr
      } else {
         set stop_time [clock seconds]
         set regtime [expr {$stop_time - $regression_start_time}]
         set regsecs [expr {$regtime % 60}]
         set regmins [expr {($regtime-$regsecs)/60}]
         puts [format "OK, $results tests passed (time %2d:%02d)" \
                  $regmins $regsecs]
         flush stdout
      }
      # Make sure everything gets killed, so succeeding pass starts clean
      if {[catch {exec $TCLSH $OOMMF killoommf all $ERR_REDIRECT} errmsg]} {
         puts stderr "ERROR KILLOOMMF:\n$errmsg"
      }
   }
   if {$test_ok} {
      set badtests [lreplace $badtests end end]
   }
}

# Reset environment, just in case this script is ever sourced
# from inside another script
array set env $env_orig


puts "------------------------------------------------"

if {$list_tests_flag} { exit }

set runseq_stop_time [clock seconds]
puts "runseq stop time: [clock format $runseq_stop_time]"
set runseq_time [expr {$runseq_stop_time - $runseq_start_time}]
set run_secs [expr {$runseq_time % 60}]
set runseq_time [expr {($runseq_time - $run_secs)/60}]
set run_mins [expr {$runseq_time % 60}]
set runseq_time [expr {($runseq_time - $run_mins)/60}]
puts [format "Total runseq time: %d:%02d:%02d" $runseq_time $run_mins $run_secs]

set total_test_count [llength $test_requests]
if {$total_test_count == 0} {
   set total_test_count [llength $buildtest_values]
}
if {[llength $badtests] == 0} {
   puts "All $total_test_count tests passed."
} else {
   set bad_test_count [llength $badtests]
   set good_test_count [expr {$total_test_count - $bad_test_count}]
   if {$good_test_count==0} {
      puts "No tests passed."
   } elseif {$good_test_count==1} {
      puts "1 test passed."
   } else {
      puts "$good_test_count tests passed."
   }
   if {$bad_test_count==1} {
      puts "1 test failure: $badtests"
   } else {
      puts "$bad_test_count test failures: $badtests"
   }
}

