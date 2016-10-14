#!/bin/sh
# FILE: runtests.tcl
#
# Top-level regression tests control script.
#
#    v--Edit here if necessary \
exec tclsh "$0" ${1+"$@"}
########################################################################

set tcl_precision 17

####
# Some patches for pre-8.4 versions of Tcl.  These are taken from the
# oommf/pkg/oc/bug files.

# Before Tcl 8.4, [expr rand()] can return an out of range result
# the first time it is called on a 64-bit platform.  Workaround that
# problem by always calling it and discarding the possibly bad result
# here before good results are expected.  Use [catch] to cover Tcl 7
# where rand() did not yet exist.
catch {expr {rand()}}

# Some new options were added to [lsearch] in 8.4 that are handy to
# make use of.
set vers [split [info patchlevel] "."]
if {[lindex $vers 0]<8 || ([lindex $vers 0]==8 && [lindex $vers 1]<4)} {
   rename lsearch Tcl8.3_lsearch
   ;proc lsearch {args} {
      if {[llength $args] < 2} {
         return [uplevel 1 Tcl8.3_lsearch $args]
      }
      set argcount [llength $args]
      set pattern [lindex $args [expr {$argcount-1}]]
      set list [lindex $args [expr {$argcount-2}]]
      set options [lrange $args 0 [expr {$argcount-3}]]

      set useAll 0
      set legalOptions {}
      foreach o $options {
         switch -exact -- $o {
	    -ascii	-
	    -decreasing	-
	    -dictionary	-
	    -increasing	-
	    -inline	-
	    -integer	-
	    -not	-
	    -real	-
	    -sorted 	-
	    -start	{
               return -code error "\[lsearch] option \"$o\" not supported;\
			Upgrade to Tcl 8.4 or higher."
	    }
	    -all {
               set useAll 1
	    }
	    default {
               lappend legalOptions $o
	    }
         }
      }
      if {!$useAll} {
         return [uplevel 1 Tcl8.3_lsearch $options [list $list $pattern]]
      }
      set result {}
      set idx [uplevel 1 Tcl8.3_lsearch $legalOptions [list $list $pattern]]
      set offset 0
      while {$idx >= 0} {
         incr offset $idx
         lappend result $offset
         incr offset
         set list [lrange $list [incr idx] end]
         set idx [uplevel 1 Tcl8.3_lsearch $legalOptions [list $list $pattern]]
      }
      return $result
   }
}

# Backport 'string replace'
if {[lindex $vers 0]<8 || ([lindex $vers 0]==8 && [lindex $vers 1]<1)} {
   rename string Tcl8.0_string
   ;proc string {args} {
      set argcount [llength $args]
      if {$argcount < 4 || $argcount > 5 || \
             [string compare replace [lindex $args 0]]!=0 } {
         return [uplevel 1 Tcl8.0_string $args]
      }
      set basestr [lindex $args 1]
      set basesize [string length $basestr]
      set index1 [lindex $args 2]
      if {[string compare end $index1]==0} {
         set index1 [expr {$basesize-1}]
      }
      set index2 [lindex $args 3]
      if {[string compare end $index2]==0} {
         set index2 [expr {$basesize-1}]
      }
      if {$index1>=$basesize || $index2<0 || $index1>$index2} {
         return $basestr
      }
      set newstr [string range $basestr 0 [expr {$index1 - 1}]]
      if {$argcount == 5} {
         append newstr [lindex $args 4]
      }
      append newstr [string range $basestr [expr {$index2 + 1}] end]
      return $newstr
   }
}

# The subcommands copy, delete, rename, and mkdir were added to
# the Tcl command 'file' in Tcl version 7.6.  The following command
# approximates them on Unix platforms.  It may not agree with
# the Tcl 7.6+ command 'file' in all of its functionality (notably
# the way it reports errors).
if {[lindex $vers 0]<8 && [lindex $vers 1]<6 && \
       [string match unix $tcl_platform(platform)]} {
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

########################################################################
# TEST FILES AND LOCATIONS:

#   MIF files are stored in the examples, bug and local directories, as
# identified by the examples_dir, bug_dir and local_load_dir variables.
#   Check result files are stored in the load, bug and local result
# directories, respectively, for the examples, bug and local tests.
# These are ODT and OVF files.
#   Parameter spec files are stored in each of the result directories,
# with the ".subtests" extensions.  There is one such file for
# each test MIF file.  Each line in a subtest file lists the parameters
# for a separate boxsi run.  An empty .subtest file indicates a
# single boxsi run with default parameter settings.
# EXTENSION: If a line in a .subtest file has an odd number of
# arguments, and one of the arguments at an even-indexed location is
# "REFERRS", then the parameters list stops with the item preceding
# REFERRS.  The items following REFERRS are output names and allowed
# error from reference values.  See proc ParseSubtestLine for details.
#   Tests from the examples and local directories are run through
# boxsi with the "-regression_test 1" option.  This option disables
# default output and stopping criteria.  Instead, an ODT file is
# generated and filled with output from each step, and the stage and
# total iteration limits are set to small values.  The main purpose of
# these tests is to see that the MIF file can be loaded and run.
#   More extensive testing can be performed out of the bug directory.
# MIF files there are run as-is, so these MIF files should be
# carefully written for bug testing.  In particular, generate only
# the output you need, and try to limit the size and length of the
# run to an extent suitable for regression testing.
#   All tests are checked to see that boxsi exits without error.  In
# addition, checks are made against output files.  For "load" tests
# (i.e., those from MIF files from the examples and local
# directories), an ODT output file is assumed, and comparison is made
# against it.  For "bug" tests, the results directory is checked for
# *.odt and *.o?f files, and comparisons are made only against such
# existing results files.
#
########################################################################
# ADDING NEW TESTS
#
#   For new tests in the examples directory, drop the MIF file
# in the examples directory, and run this script with the
# -autoadd flag.  An associated empty .subtests file will be
# made in the load directory.  If desired, you can add lines
# to the .subtest file to specify multiple runs or particular
# parameter values.  Each run will generate a .odt file; if
# a .odt file does not exist before the run, the generated .odt
# file will automatically become the new check results file.
#   For new tests in the local directory, manually place empty
# .subtest and an appropriate .odt file.  At this time there
# is no support for automated population of this directory.
#   For new tests in the bug directory, just add the MIF file
# to the bug directory.  It will be automatically picked up
# and an empty .subtests file will be made.  Again, you can
# edit this as desired.  Note that unlike with the load tests,
# where a .odt file (and only a .odt file) is assumed, there
# are no automatic result files checks for bug tests.  Instead,
# you should run boxsi separately by hand for each desired
# parameter set, and save the check files to the bug directory
# with the appropriate name.  The "appropriate name" is constructed
# as follows:  The "basename" specified in the MIF file should
# agree with the filename (or else omitted, which results in the
# same), the output name, stage and iteration count, and file
# extension should be as computed by boxsi.  If there is only
# one subtest, then that is it.  If there are more than one subtest,
# then each output file should have an "_#" appended immediately
# after the basename (before the output name and counts, if any,
# and the .extension), where "#" refers to the 0-based subtest
# number.  For example,
#
#   ovfout_2-Oxs_TimeDriver-Magnetization-00-0000005.omf
#
# is the Oxs_TimeDriver Magnetization output for stage 0, iteration
# 5, for subtest 2 (i.e., the third subtest) of test ovfout.  The
# file renaming can be accomplished with a regex match like
#
#     s/^ovfout(([^_].*|)\.o..)/ovfout_2\1/
#
# where the two "ovfout"'s and the "2" are adjusted as appropriate.
# After creating the result check files, run the regression test
# with the "-v" option to see that all the intended file comparisons
# are being made.
#
#   Another way to fill odt files in the bug directory is to create
# an empty .odt file with the correct name (e.g., unifexch_29.odt),
# and then run runtest.tcl with the --updaterefdata switch.
#
#   NOTE: Test "names" are based on the MIF filenames.  Be
# sure that the MIF filenames in the examples directory are
# all distinct from the MIF filenames in the bug directory.
#
########################################################################

########################################################################
# Skip labels: These are intended for development work only.  The global
# variables oldskiplabels and newskiplabels each contain a list of
# regular expressions used to match against column headers in the
# reference ("old") and test ("new") ODT files.  Columns matching these
# regexp's are removed before comparison testing.
global oldskiplabels newskiplabels
set oldskiplabels {}
set newskiplabels {}
set swaplabels {}
if {0} {
   set oldskiplabels {
      {^Oxs_CGEvolve:[^:]*:Cycle.* count}
      {^Oxs_CGEvolve:[^:]*:Energy calc count}
      {^Oxs_RungeKuttaEvolve:[^:]*:Delta E}
   }
   set newskiplabels {
      {^Oxs_CGEvolve:[^:]*:Cycle.* count}
      {^Oxs_CGEvolve:[^:]*:Energy calc count}
      {^Oxs_RungeKuttaEvolve:[^:]*:Delta E}
   }
   set swaplabels {
      {{.*:Max Spin Ang} {.*:Max Spin Ang}}
      {{.*:Stage Max Spin Ang} {.*:Stage Max Spin Ang}}
      {{.*:Run Max Spin Ang} {.*:Run Max Spin Ang}}
   }
}
########################################################################

if {[string match "windows" $tcl_platform(platform)]} {
   set nuldevice "nul:"
} else {
   set nuldevice "/dev/null"
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

set PID_INFO_TIMEOUT 60   ;# Max time to wait for mmArchive to start,
                          ## in seconds.

set EXEC_TEST_TIMEOUT 150 ;# Max time to wait for any single test to
## run, in seconds.

set HOME  [file dirname [info script]]
if {![catch {file normalize $HOME} _]} {
   # 'file normalize' new in Tcl 8.4
   set HOME $_
}
set TCLSH [info nameofexecutable]
if {![catch {file normalize $TCLSH} _]} {
   # 'file normalize' new in Tcl 8.4
   set TCLSH $_
}

set OOMMF [file join $HOME .. .. .. oommf.tcl]

set SIGFIGS 8   ;# Default
set loglevel 0  ;# Default
set logfile [file join $HOME runtests.log]

set load_dir [file join $HOME load_tests]
set examples_dir [file join $HOME .. examples]

set local_load_dir [file join $HOME local_tests]
set local_examples_dir [file join $HOME .. local]

set bug_dir [file join $HOME bug_tests]

set results_basename_template "regression-test-output-%d"
set results_basename [format $results_basename_template [pid]]


########################################################################
# CHECK DISPLAY:
#    See if any2ppm can be run.  If not, then tests which rely on this
# will fail; set the global variable no_display appropriately so
# tests that rely on Tk can be skipped.
set no_display 0
set any2ppm_command [exec_ignore_stderr $TCLSH $OOMMF +command any2ppm -h]
set any2ppm_command [linsert $any2ppm_command 0 exec]
if {[catch {eval $any2ppm_command 2>$nuldevice} _]} {
   set no_display 1
}

proc Usage {} {
   global loglevel results_basename_template SIGFIGS EXEC_TEST_TIMEOUT
   puts stderr "Usage: tclsh runtests.tcl\
                  \[-autoadd\]\
                  \[-alttestdir \<dirname\>\]\
                  \[-ignoreextra\]\
                  \[-keepfail\]\
                  \[-listtests\]\n      \
                  \[-loglevel \<\#\>\]\
                  \[-noexcludes\]\
                  \[-resultsfile \<filename\>\]\
                  \[-showoutput\]\n      \
                  \[-sigfigs \<\#\>\]\
                  \[-threads \<\#\>\]\
                  \[-timeout \<\#\>\]\
                  \[-updaterefdata\]\
                  \[-v\]\n      \
                  \[testa testb ...\]"
   puts stderr " Where:"
   puts stderr "  -autoadd automatically adds new tests from\
                   examples directory"
   puts stderr "  -alttestdir specifies alternative test directory"
   puts stderr "  -ignoreextra ignore extra columns in new data"
   puts stderr "  -keepfail saves results from failed tests"
   puts stderr "  -listtests shows selected tests and exits"
   puts stderr "  -loglevel controls output to boxsi.errors\
                   (default $loglevel)"
   puts stderr "  -noexcludes ignore exclude files"
   puts stderr "  -resultsfile sets temp results filename (default\
                   \"$results_basename_template\", with <pid> filling %d)"
   puts stderr "  -showoutput dumps test stdout and stderr output"
   puts stderr "  -sigfigs is number of significant figures (default $SIGFIGS)"
   puts stderr "  -threads is number of threads to run (threaded builds only)"
   puts stderr "  -timeout is max seconds to wait for one test\
                   (default 150; 0 == no timeout)"
   puts stderr "  -updaterefdata to overwrite reference data\
                   with new results"
   puts stderr "  -v enable verbose output"
   puts stderr "If no tests are specified, then all tests are run."
   exit 1
}

proc GetOid { app_name app_pid } {
   # OID is "OOMMF ID"
   global TCLSH OOMMF PID_INFO_TIMEOUT

   if {[catch {exec_ignore_stderr $TCLSH $OOMMF pidinfo -noheader \
                  -wait $PID_INFO_TIMEOUT -pid $app_pid} pidinfo]} {
      puts stderr "---\n$pidinfo\n---"
      puts stderr "ERROR: Unable to determine OID for application\
                      $app_name with pid $app_pid"
      exit 1
   }

   # On success, pidinfo string should be "OID PID AppName"
   if {[llength $pidinfo]!=3 \
          || ![regexp {^[0-9]+$} [lindex $pidinfo 0]] \
          || ![regexp {^[0-9]+$} [lindex $pidinfo 1]] } {
      puts stderr "ERROR: Malformed response from OOMMF\
                   pidinfo application: $pidinfo"
      exit 1
   }

   return [lindex $pidinfo 0]
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

proc Cleanup {} {
   # Send die request to mmArchive
   global TCLSH OOMMF mmArchive_pid mmArchive_oid nuldevice
   set errcode 0
   if {[info exists mmArchive_oid]} {
      set errcode [catch {exec $TCLSH $OOMMF killoommf \
                             $mmArchive_oid 2>$nuldevice}]
   } elseif {[info exists mmArchive_pid]} {
      set errcode [catch {exec $TCLSH $OOMMF killoommf \
                             -pid $mmArchive_pid 2>$nuldevice}]
   }
   if {$errcode && [info exists mmArchive_pid]} {
      SystemKill $mmArchive_pid
   }
   # By unsetting mmArchive_oid and mmArchive_pid, we insure
   # that multiple calls to Cleanup during shutdown don't cause
   # any problems.
   catch {unset mmArchive_oid}
   catch {unset mmArchive_pid}
}

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

# Set up exit handler to try our best to kill mmArchive
rename exit orig_exit
proc exit { args } {
   catch { Cleanup }
   eval orig_exit $args
}


if {[lsearch -regexp $argv {^-+(h|help)$}]>=0} { Usage }

set autoadd 0
set autoadd_index [lsearch -regexp $argv {^-+autoadd$}]
if {$autoadd_index >= 0} {
   set autoadd 1
   set argv [lreplace $argv $autoadd_index $autoadd_index]
}

set alt_test_dir {}
set alttestdir_index [lsearch -regexp $argv {^-+alttestdir$}]
if {$alttestdir_index >= 0 && $alttestdir_index+1 < [llength $argv]} {
   set ul [expr {$alttestdir_index + 1}]
   set alt_test_dir [file join [pwd] [lindex $argv $ul]]
   set argv [lreplace $argv $alttestdir_index $ul]
}


set ignoreextra 0
set ignoreextra_index [lsearch -regexp $argv {^-+ignoreextra$}]
if {$ignoreextra_index >= 0} {
   set ignoreextra 1
   set argv [lreplace $argv $ignoreextra_index $ignoreextra_index]
}

set keepfail 0
set keepfail_index [lsearch -regexp $argv {^-+keepfail(|ed)$}]
if {$keepfail_index >= 0} {
   set keepfail 1
   set argv [lreplace $argv $keepfail_index $keepfail_index]
}

set leak 0
set leak_index [lsearch -regexp $argv {^-+leak$}]
if {$leak_index >= 0} {
   set leak 1
   set argv [lreplace $argv $leak_index $leak_index]

   if {[string match {} [auto_execok valgrind]]} {
      puts stderr "ERROR: No -leak test without valgrind"
      exit 2
   }
}

set listtests 0
set listtests_index [lsearch -regexp $argv {^-+listtests$}]
if {$listtests_index >= 0} {
   set listtests 1
   set argv [lreplace $argv $listtests_index $listtests_index]
}

set loglevel_index [lsearch -regexp $argv {^-+loglevel$}]
if {$loglevel_index >= 0 && $loglevel_index+1 < [llength $argv]} {
   set ul [expr {$loglevel_index + 1}]
   set loglevel [lindex $argv $ul]
   set argv [lreplace $argv $loglevel_index $ul]
   if {![regexp {^[0-9]+$} $loglevel]} {
      puts stderr "ERROR: Option loglevel must be a non-negative integer"
      exit 2
   }
}

set noexcludes 0
set noexcludes_index [lsearch -regexp $argv {^-+noexcludes$}]
if {$noexcludes_index >= 0} {
   set noexcludes 1
   set argv [lreplace $argv $noexcludes_index $noexcludes_index]
}

set resultsfile_index [lsearch -regexp $argv {^-+resultsfile$}]
if {$resultsfile_index >= 0 && $resultsfile_index+1 < [llength $argv]} {
   set ul [expr {$resultsfile_index + 1}]
   set results_basename [lindex $argv $ul]
   set argv [lreplace $argv $resultsfile_index $ul]
   if {[string match {} $results_basename]} {
      puts stderr "ERROR: resultsfile filename must be an non-empty string"
      exit 3
   }
}

set showoutput 0
set showoutput_index [lsearch -regexp $argv {^-+showoutput$}]
if {$showoutput_index >= 0} {
   set showoutput 1
   set argv [lreplace $argv $showoutput_index $showoutput_index]
}

set sigfig_index [lsearch -regexp $argv {^-+sigfigs$}]
if {$sigfig_index >= 0 && $sigfig_index+1 < [llength $argv]} {
   set ul [expr {$sigfig_index + 1}]
   set SIGFIGS [lindex $argv $ul]
   set argv [lreplace $argv $sigfig_index $ul]
   if {![regexp {^[0-9]+$} $SIGFIGS]} {
      puts stderr "ERROR: Option sigfigs must be a positive integer"
      exit 2
   }
}

set threads_index [lsearch -regexp $argv {^-+threads$}]
if {$threads_index >= 0 && $threads_index+1 < [llength $argv]} {
   set ul [expr {$threads_index + 1}]
   set val [lindex $argv $ul]
   set argv [lreplace $argv $threads_index $ul]
   if {![regexp {^[1-9][0-9]*$} $val]} {
      puts stderr "ERROR: Option threads must be a positive integer"
      exit 2
   }
   set thread_count $val
}

set timeout_index [lsearch -regexp $argv {^-+timeout$}]
if {$timeout_index >= 0 && $timeout_index+1 < [llength $argv]} {
   set ul [expr {$timeout_index + 1}]
   set val [lindex $argv $ul]
   set argv [lreplace $argv $timeout_index $ul]
   if {![regexp {^[0-9]*$} $val]} {
      puts stderr "ERROR: Option timeout must be a non-negative integer"
      exit 2
   }
   set EXEC_TEST_TIMEOUT $val
}

set updaterefdata 0
set updaterefdata_index [lsearch -regexp $argv {^-+updaterefdata$}]
if {$updaterefdata_index >= 0} {
   set updaterefdata 1
   set argv [lreplace $argv $updaterefdata_index $updaterefdata_index]
}

set verbose 0
set verbose_index [lsearch -regexp $argv {^-+v$}]
if {$verbose_index >= 0} {
   set verbose 1
   set argv [lreplace $argv $verbose_index $verbose_index]
}

# Note: If alt_test_dir is non-empty, then default load, local, and bug
# tests are disabled.
if {![string match {} $alt_test_dir]} {
   set loadtests {}
   set localtests {}
   set new_tests {}
   set bug_dir $alt_test_dir ;# If set, alt_test_dir replaces bug_dir
} else {
   # Check for new or missing tests
   set load_list [glob -nocomplain [file join $load_dir *.subtests]]
   set examples_list [glob -nocomplain [file join $examples_dir *.mif]]
   set new_tests {}
   set missing_tests {}

   if {$autoadd} {
      foreach test $examples_list {
         set root [file rootname [file tail $test]]
         set check [file join $load_dir ${root}.subtests]
         if {[lsearch -exact $load_list $check]<0} {
            lappend new_tests $root
         }
      }
   }

   foreach test $load_list {
      set root [file rootname [file tail $test]]
      set check [file join $examples_dir ${root}.mif]
      if {[lsearch -exact $examples_list $check]<0} {
         lappend missing_tests $root
      }
   }

   # Report new and missing tests
   if {[llength $new_tests]>0} {
      puts "New tests---"
      foreach test $new_tests { puts $test }
      puts "------------"
   }

   if {[llength $missing_tests]>0} {
      puts "Missing tests---"
      foreach test $missing_tests { puts $test }
      puts "----------------"
   }

   # Build full load test list
   set loadtests $new_tests
   foreach test $load_list {
      set root [file rootname [file tail $test]]
      if {[lsearch -exact $missing_tests $root]<0} {
         lappend loadtests $root
      }
   }
   set loadtests [lsort $loadtests]

   # Silently omit any missing local MIF files, so that
   # extensions can be removed from local w/o breaking
   # regression test suite.
   set local_load_list \
      [glob -nocomplain [file join $local_load_dir *.subtests]]
   set local_examples_list \
      [glob -nocomplain [file join $local_examples_dir *.mif] \
          [file join $local_examples_dir *.mif2]]
   set localtests {}
   foreach test $local_load_list {
      set root [file rootname [file tail $test]]
      set check [file join $local_examples_dir ${root}.mif]
      if {[lsearch -exact $local_examples_list $check]>=0} {
         lappend localtests $root
      } else {
         set check [file join $local_examples_dir ${root}.mif2]
         if {[lsearch -exact $local_examples_list $check]>=0} {
            lappend localtests $root
         }
      }
   }
}

# Build full bug test list
set bug_list [glob -nocomplain [file join $bug_dir *.mif]]
set bugtests {}
foreach test $bug_list {
   set root [file rootname [file tail $test]]
   set check [file join $bug_dir ${root}.subtests]
   if {![file exists $check]} {
      # Create empty subtests file
      set chan [open $check w]
      close $chan
   }
   lappend bugtests $root
}
set bugtests [lsort $bugtests]


# Compare loadtests, localtests and bugtests to requested tests in
# argv, and construct a list of tests to do.  Each element of dotests
# is a list of the form
#
#           basetest testlevel mifdir resultdir [subtest# subtest# ...]
#
# Where each "subtest#" is a 0-based index into the subtests described
# in the basetest.subtest files.  If no subtest# are given, then all
# subtests are run.
#   The "testlevel" is an integer (1 or 2) that is passed to boxsi as a
# parameter to the -regression_test switch.  "1" is used for load tests,
# and limits the number of iterations performed by boxsi.  Also, the
# output schedule (if any) is deleted, and a default DataTable output
# is introduced with a "Step 1" schedule.  "2" is used for bug tests,
# and causes boxsi to respect the run limits and output schedules as
# specified in the mif file.  In both cases, the base output name
# is set to $results_basename.

set dotests {}
if {[llength $argv]==0} {
   foreach test $loadtests {
      lappend dotests [list $test 1 $examples_dir $load_dir]
   }
   foreach test $localtests {
      lappend dotests [list $test 1 $local_examples_dir $local_load_dir]
   }
   foreach test $bugtests {
      lappend dotests [list $test 2 $bug_dir $bug_dir]
   }
} else {
   foreach test $argv {
      set basetest [lindex $test 0]
      set loadmatchlist [lsearch -all -glob $loadtests $basetest]
      set localmatchlist [lsearch -all -glob $localtests $basetest]
      set bugmatchlist [lsearch -all -glob $bugtests $basetest]
      if {[llength $loadmatchlist] == 0 \
          && [llength $localmatchlist] == 0 \
          && [llength $bugmatchlist] == 0} {
         puts stderr "Skipping unrecognized command line test \"$test\""
      }
      set test [lreplace $test 0 0]
      regsub -all {,} $test { } test  ;# Replace commas with spaces
      if {[llength $loadmatchlist]>0} {
         foreach match $loadmatchlist {
            lappend dotests [linsert $test 0 \
                     [lindex $loadtests $match] 1 $examples_dir $load_dir]
         }
      }
      if {[llength $localmatchlist]>0} {
         foreach match $localmatchlist {
            lappend dotests [linsert $test 0 \
             [lindex $localtests $match] 1 $local_examples_dir $local_load_dir]
         }
      }
      if {[llength $bugmatchlist]>0} {
         foreach match $bugmatchlist {
            lappend dotests [linsert $test 0 \
                     [lindex $bugtests $match] 2 $bug_dir $bug_dir]
         }
      }
   }
}
if {[llength $dotests]==0} {
    puts stderr "*** NO TESTS ***"
    exit 1
}

# Hack for Tcl 7.x support.  (The -index option to lsort
# appears in Tcl 8.0.)
proc SortCompareIndex { index elta eltb } {
   set checka [expr {int([lindex $elta $index])}]
   set checkb [expr {int([lindex $eltb $index])}]
   if {$checka < $checkb} { return -1 }
   if {$checka > $checkb} { return  1 }
   return 0
}
proc SortUpIndex { index list } {
   if {[lindex [split [info tclversion] "."] 0]>7} {
      return [lsort -integer -index $index $list]
   }
   return [lsort -command "SortCompareIndex $index" $list]
}
proc SortDownIndex { index list } {
   if {[lindex [split [info tclversion] "."] 0]>7} {
      return [lsort -integer -decreasing -index $index $list]
   }
   return [lsort -decreasing -command "SortCompareIndex $index" $list]
}

# Utility code to compare .odt table output
proc SwapColumns { swapcols row } {
   # Import swapcols is a list of column pairs, e.g.,
   #     { {3 2} {12 4} {5 5} {1 7} }
   # The first number in each pair is the source column index,
   # the second is the destination (target) column index.  This
   # code assumes swapcols is sorted on the second index.
   #  Return value is re-ordered row.

   if {[llength $swapcols]==0} { return $row }

   # Make copy of swapping entries
   set pieces {}
   foreach elt $swapcols {
      lappend pieces [lindex $row [lindex $elt 0]]
   }

   # Delete swapping entries
   foreach elt [SortDownIndex 0 $swapcols] {
      set i [lindex $elt 0]
      set row [lreplace $row $i $i]
   }

   # Replace swapping entries into new location.  This
   # step requires increasing order on the target indices.
   foreach elt $swapcols p $pieces {
      set row [linsert $row [lindex $elt 1] $p]
   }

   return $row
}

proc ParseSubtestLine { line } {
   # Code to parse individual lines from .subtest files.
   #   In the simplest form, these lines have an even number of
   # elements, where the even indexed elements are parameter names,
   # and the odd indexed elements are parameter values.
   #   In the extended form, the lines have an odd number of elements,
   # and one of the even indexed elements is the literal string
   # "REFERRS".  In this case, all the elements before the REFERRS
   # marker are parameter name+value pairs, as in the simple form, but
   # the element pairs following the REFERRS marker are simulation
   # output names + error values.  The reference values are used in the
   # comparison check routines to determine the range of deviation from
   # the reference value that is allowed w/o raising an error.  For
   # convenience, the glob-style wildcards are allowed in the referrs
   # output names.  The matching is done left to right, with the last
   # match taking precedence, so more general glob patterns can be
   # placed first, followed by more specific glob patterns that
   # override earlier values.
   #   The return value is a two item list; the first item is the
   # parameter name+value list.  The second item is the referrs output
   # name + error values.

   set parlist {}
   set errlist {}
   if {[llength $line] % 2 == 0} {
      # No referrs
      set parlist $line
   } else {
      set refindex [lsearch -exact $line REFERRS]
      if { $refindex < 0 || $refindex % 2 != 0 } {
         # Malformed line
         error "Malformed subtest line: $line"
      }
      set parlist [lrange $line 0 [expr {$refindex - 1}]]
      set errlist [lrange $line [expr {$refindex + 1}] end]
   }

   return [list $parlist $errlist]
}


proc TestCompareODT { oldfile newfile suberrors } {
   global verbose
   if {$verbose} {
      puts "Comparing \"$newfile\" to \"$oldfile\""
   }

   global oldskiplabels newskiplabels swaplabels ignoreextra
   if {![info exists oldskiplabels]}  { set oldskiplabels {} }
   if {![info exists newskiplabels]}  { set newskiplabels {} }
   if {![info exists swapskiplabels]} { set swapskiplabels {} }

   set chan [open $oldfile r]
   set oldtable [read -nonewline $chan]
   close $chan

   set MAXTRYCOUNT 10
   set file_opened 0
   for {set trycount 0} {$trycount<$MAXTRYCOUNT} {incr trycount} {
      if {![catch {open $newfile r} chan]} {
         set file_opened 1
         set newtable [read -nonewline $chan]
         close $chan
         if {[regexp "# *Table *End\[ \t\n\]*\$" $newtable]} {
            break
         }
      }
      # Either can't open file or file trailer is missing, presumably
      # because mmArchive isn't finished writing it.  Wait and retry.
      after 1000
   }
   if {$trycount >= $MAXTRYCOUNT} {
      if {!$file_opened} {
         puts "ERROR: Unable to open test ODT file \"$newfile\""
      } else {
         puts "ERROR: Test ODT file \"$newfile\" incomplete."
      }
      return 1
   }


   # Remove Title line; it embeds a timestamp, which will change from
   # run to run.
   regsub -all "\n# Title:\[^\n]*\n" $oldtable \
      "\n# Title: (removed)\n" oldtable
   regsub -all "\n# Title:\[^\n]*\n" $newtable \
      "\n# Title: (removed)\n" newtable

   if {[string compare $oldtable $newtable]==0} {
      return 0
   }

   # We have non-matches at the string comparison level.  Try comparing
   # data on an entry-by-entry basis as floating point values.
   global SIGFIGS
   set sigfmt "%#.${SIGFIGS}g"

   # Join split lines, and split tables by lines
   regsub -all " *\\\\\n# *" $oldtable { } oldtable
   set oldtable [split $oldtable "\n"]
   regsub -all " *\\\\\n# *" $newtable { } newtable
   set newtable [split $newtable "\n"]
   if {[llength $oldtable] != [llength $newtable]} {
         puts "ERROR: New and reference output\
               have different number of lines\
               ([llength $newtable] != [llength $oldtable])"
         return 1
   }

   # Collect column by column error tolerances
   catch {unset colmax}
   set columns {}
   set oldskipcols {} ;# List of column indices to ignore,
                      ## sorted from biggest to smallest.
   foreach line $oldtable {
      if {[string match "\#*" $line]} {
         # Comment line
         if {[llength $columns]==0 && \
                [regexp "^\# Columns: (.*)$" $line dummy foo]} {
            set columns $foo
            set oldskipcols {}
            for {set i [expr {[llength $columns]-1}]} {$i>=0} {incr i -1} {
               set elt [lindex $columns $i]
               foreach pat $oldskiplabels {
                  if {[regexp $pat $elt]} {
                     lappend oldskipcols $i
                     break
                  }
               }
            }
            foreach i $oldskipcols {
               set columns [lreplace $columns $i $i]
            }
            set swapcols {}
            foreach patpair $swaplabels {
               set oldpat [lindex $patpair 0]
               lappend swapcols [lsearch -regexp $columns $oldpat]
            }
            # At this point swapcols contains the destination address
            # for each swap pair in the post-skip column list.
         }
         continue
      }
      foreach i $oldskipcols {
         set line [lreplace $line $i $i] ;# Removed skipped columns
      }
      if {![info exists colmax]} {
         # First line
         foreach elt $line { lappend colmax [expr {abs($elt)}] }
      } else {
         # Otherwise, see if any data in new line is new biggest
         if {[llength $colmax] != [llength $line]} {
            error "Data lines in $oldfile have varying number of columns"
         }
         for {set i 0} {$i<[llength $colmax]} {incr i} {
            set testelt [expr {abs([lindex $line $i])}]
            if {$testelt>[lindex $colmax $i]} {
               set colmax [lreplace $colmax $i $i $testelt]
            }
         }
      }
   }
   # Special case handling for particular columns:
   if {[llength $columns]>0} {
      # Normalized m output
      foreach i [lsearch -regexp -all $columns {^.*::m[xyz]$}] {
            set colmax [lreplace $colmax $i $i 1.0]
      }

      # Non-normalized M output
      set Mmax 0.0
      set Mset [lsearch -regexp -all $columns {^.*::M[xyz]$}]
      foreach i $Mset {
         if {[lindex $colmax $i]>$Mmax} {
            set Mmax [lindex $colmax $i]
         }
      }
      foreach i $Mset {
         set colmax [lreplace $colmax $i $i $Mmax]
      }

      # Spin data
      foreach i [lsearch -regexp -all $columns {^.*Max Spin Ang$}] {
            set colmax [lreplace $colmax $i $i 180.]
      }
   }
   set colerr {}  ;# Allowed error, column-by-column
   set errmult [expr {5.0*pow(10,-1*$SIGFIGS)}]
   ## The "5" factor is a fudge, put in because "significant figures"
   ## don't map exactly to relative error.  For example, 11.4 and 10.6
   ## agree to two sig figs, but the relative error is 0.07, which is
   ## significantly larger than 10^-2 = 0.01.
   foreach elt $colmax {
      if {$elt == 0.0} {
         lappend colerr 1e-300  ;# Dummy value
      } else {
         lappend colerr [expr {$elt*$errmult}]
      }
   }

   # Adjust allowed errors as request by subtest
   foreach {outname outerr} $suberrors {
      foreach i [lsearch -glob -all $columns $outname] {
         set colerr [lreplace $colerr $i $i $outerr]
      }
   }

   # Compute skip and swap columns for new data
   set newskipcols {}
   if {$ignoreextra || [llength $newskiplabels] || [llength $swaplabels]} {
      set colrow [lsearch -regexp $newtable "^\# Columns: "]
      if {$colrow<0} {
         puts "ERROR: New (test) ODT file is missing column header line"
         return 1
      }
      set line [lindex $newtable $colrow]
      if {[regexp "^\# Columns: (.*)$" $line dummy cols]} {
         for {set i [expr {[llength $cols]-1}]} {$i>=0} {incr i -1} {
            set elt [lindex $cols $i]
            if {$ignoreextra && [lsearch -exact $columns $elt]<0} {
                  # $elt doesn't match any column in reference data
                  lappend newskipcols $i
                  puts "Ignoring new data column \"$elt\""
                  continue  ;# next i
            }
            foreach pat $newskiplabels {
               if {[regexp $pat $elt]} {
                  lappend newskipcols $i
                  break
               }
            }
         }
         foreach i $newskipcols {
            # Remove skipped columns from column template
            set cols [lreplace $cols $i $i]
         }
         set swap_dest $swapcols
         set swapcols {}
         foreach patpair $swaplabels i $swap_dest {
            set newpat [lindex $patpair 1]
            set j [lsearch -regexp $cols $newpat]
            if {$i>=0 && $j>=0} {
               # What to do if either or both of swap patterns find no
               # match?  First guess is to just throw out that pair.
               lappend swapcols [list $j $i]
            }
         }
         set swapcols [SortUpIndex 1 $swapcols]
         # swapcols now contains columns pairs, {src_col dest_col}
         # for each swap pair in the post-skip column list, sorted
         # to be increasing on the dest_col.
      }
   }

   # Detail error check;  Report worst mismatch
   set linecount 0
   set maxrat 1.0
   set badreport {}
   foreach oldline $oldtable newline $newtable {
      incr linecount

      if {[llength $oldskipcols]} {
         if {![string match "\#*" $oldline]} {
            foreach i $oldskipcols {
               # Removed skipped columns
               set oldline [lreplace $oldline $i $i]
            }
         } elseif {[regexp "^\# Columns: (.*)$" $oldline dummy foo]} {
            foreach i $oldskipcols {
               set foo [lreplace $foo $i $i] ;# Removed skipped columns
            }
            set oldline "\# Columns: $foo"
         } elseif {[regexp "^\# Units: (.*)$" $oldline dummy foo]} {
            foreach i $oldskipcols {
               set foo [lreplace $foo $i $i] ;# Removed skipped columns
            }
            set oldline "\# Units: $foo"
         }
      }

      if {[llength $newskipcols] || [llength $swapcols]} {
         if {![string match "\#*" $newline]} {
            foreach i $newskipcols {
               # Removed skipped columns
               set newline [lreplace $newline $i $i]
            }
            set newline [SwapColumns $swapcols $newline]
         } elseif {[regexp "^\# Columns: (.*)$" $newline dummy foo]} {
            foreach i $newskipcols {
               set foo [lreplace $foo $i $i] ;# Removed skipped columns
            }
            set foo [SwapColumns $swapcols $foo]
            foreach elt $swapcols {
               # Rewrite swapped column headers
               set j [lindex $elt 1]
               set foo [lreplace $foo $j $j [lindex $columns $j]]
            }
            set newline "\# Columns: $foo"
         } elseif {[regexp "^\# Units: (.*)$" $newline dummy foo]} {
            foreach i $newskipcols {
               set foo [lreplace $foo $i $i] ;# Removed skipped columns
            }
            set foo [SwapColumns $swapcols $foo]
            set newline "\# Units: $foo"
         }
      }


      if {[string compare $oldline $newline]==0} { continue }

      if {[regexp "^\# Columns: (.*)$" $oldline dummy oldfoo] && \
             [regexp "^\# Columns: (.*)$" $newline dummy newfoo]} {
         set oldfoo [eval list $oldfoo]  ;# Remove extraneous spaces
         set newfoo [eval list $newfoo]
         if {[string compare $oldfoo $newfoo]==0} { continue }
         puts "ERROR: New and reference output have\
                      column header line mismatch"
         if {$verbose} {
            puts "Ref line---\n$oldline"
            puts "New line---\n$newline"
            set mismatch_count 0
            foreach oelt $oldline nelt $newline {
               if {[string compare $oelt $nelt]!=0} {
                  puts "Column difference: ->$oelt<-"
                  puts " ------------------->$nelt<-"
                  incr mismatch_count
               }
            }
            puts "Number of mismatched columns: $mismatch_count"
         }
         return 1
      }

      if {[regexp "^\# Units: (.*)$" $oldline dummy oldfoo] && \
             [regexp "^\# Units: (.*)$" $newline dummy newfoo]} {
         set oldfoo [eval list $oldfoo]  ;# Remove extraneous spaces
         set newfoo [eval list $newfoo]
         if {[string compare $oldfoo $newfoo]==0} { continue }
         puts "ERROR: New and reference output have\
                      units header line mismatch"
         if {$verbose} {
            puts "Ref line---\n$oldline"
            puts "New line---\n$newline"
         }
         return 1
      }

      if {[string match "\#*" $oldline] || \
             [string match "\#*" $newline]} {
         puts "ERROR: New and reference output have\
                      header or comment line mismatch"
         if {$verbose} {
            puts "Ref line---\n$oldline"
            puts "New line---\n$newline"
         }
         return 1
      }

      if {[llength $oldline] != [llength $newline]} {
         puts "ERROR: New and reference output have\
                different number of columns"
         return 1
      }

      if {[llength $oldline] != [llength $colerr]} {
         puts "ERROR: Reference output format error;\
               column header/data mismatch"
         return 1
      }

      set eltcount 0
      foreach oldelt $oldline newelt $newline allowed_err $colerr {
         set errcode [catch {expr {abs(double($oldelt-$newelt))}} observed_err]
         if {$errcode || $observed_err>$allowed_err*$maxrat} {
            if {$errcode} {
               puts "ERROR ref data: $oldelt"
               puts "      bad data: $newelt"
               set maxrat 1e100
            } elseif {[catch {expr {$observed_err/$allowed_err}} maxrat]} {
               puts "ERROR bad errors: observed_err = $observed_err"
               puts "                   allowed_err = $allowed_err"
               puts $maxrat
               set maxrat 1e100
            }
            set itval {}
            set itcol [lsearch -regexp $columns {^.*::Iteration$}]
            if {$itcol>=0} {
               set itval " (Iteration [lindex $oldline $itcol])"
            }
            set eltname " ([lindex $columns $eltcount])"
            set badreport "ODT compare error;\
                  Worst mismatch: line $linecount$itval, \
                  element $eltcount$eltname:\n"
            if {$errcode} {
               append badreport [format " REF VALUE: $sigfmt\n" $oldelt]
               append badreport [format " NEW VALUE: %s\n" $newelt]
               append badreport [format " INVALID DATA"]
            } else {
               append badreport [format " REF VALUE: $sigfmt\n" $oldelt]
               append badreport [format " NEW VALUE: $sigfmt\n" $newelt]
               append badreport [format " ERROR observed/allowed: %.2e/%.2e" \
                                     $observed_err $allowed_err]
            }
         }
         incr eltcount
      }
   }

   if {[string match {} $badreport]} {
      return 0
   }

   puts $badreport
   return 1
}

proc TestCompareOVF { oldfile newfile } {
   global TCLSH OOMMF verbose
   global avfdiff_basecmd nuldevice
   if {$verbose} {
      puts "Comparing \"$newfile\" to \"$oldfile\""
   }
   if {![info exists avfdiff_basecmd]} {
      set avfdiff_basecmd [exec_ignore_stderr \
                              $TCLSH $OOMMF avfdiff +command]
      global tcl_platform
      if {[string compare "Darwin" $tcl_platform(os)] != 0} {
         # On (at least some) Mac OS X, this redirect can cause
         # spurious EOFs on unrelated channel reads.  OTOH, this
         # redirect may protect against console problems on Windows.
         lappend avfdiff_basecmd "<" $nuldevice
      }
   }

   # Test that newfile is ready to be opened and read
   for {set trycount 0} {$trycount<5} {incr trycount} {
      if {![catch {open $newfile r} chan]} { break }
      if {$trycount+1 >= 5} {
         puts "ERROR: Unable to open test ODT file \"$newfile\""
         return 1
      }
      after 1000  ;# Can't open file; wait a little and try again
   }
   close $chan
   set cmd [concat $avfdiff_basecmd -info \
                [list $oldfile] [list $newfile]]
   if {[catch {
         set results [eval exec_ignore_stderr $cmd]
      } errmsg]} {
      puts "Run compare error \"$oldfile\" \"$newfile\" -->"
      puts "$errmsg\n<-- Run compare error"
      return 1
   }
   set listresults [split $results "\n"]
   if {[llength $listresults]!=3} {
      puts "Run compare error (a) \"$oldfile\" \"$newfile\";\
            Bad avfdiff -info output -->"
      puts "$results\n<-- Run compare error"
      return 1
   }
   set line0 [string trim [lindex $listresults 0]]
   set line1 [string trim [lindex $listresults 1]]
   set line2 [string trim [lindex $listresults 2]]
   if {![string match "${oldfile} (in *" $line0] || \
         ![string match "${newfile} (in *" $line2]} {
      puts "Run compare error (b) \"$oldfile\" \"$newfile\";\
            Bad avfdiff -info output -->"
      puts "$results\n<-- Run compare error"
      return 1
   }
   if {![regexp {^.*Max *mag = *([0-9.e+-]+),} \
            $line1 dummy maxmag]} {
      puts "Run compare error (c) \"$oldfile\" \"$newfile\";\
            Bad avfdiff -info output -->"
      puts "$results\n<-- Run compare error"
      return 1
   }
   if {![regexp {^.*Max *diff = *([0-9.e+-]+),} \
            $line2 dummy maxdiff]} {
      puts "Run compare error (d) \"$oldfile\" \"$newfile\";\
            Bad avfdiff -info output -->"
      puts "$results\n<-- Run compare error"
      return 1
   }

   global SIGFIGS
   set allowed_err [expr {5.0*pow(10,-1*$SIGFIGS)*$maxmag}]
   ## See notes on "errmult" in proc TestCompareODT
   if {$maxdiff > $allowed_err} {
      # Error too big
      puts "OVF compare error on \"$oldfile\":"
      puts [format " ERROR observed/allowed: %.2e/%.2e" \
                     $maxdiff $allowed_err]
      return 1
   }

   return 0
}

proc TestCompare { oldfile newfile suberrors } {
   set file_ext [string tolower [file extension $oldfile]]
   if {[string compare ".odt" $file_ext]==0} {
      return [TestCompareODT $oldfile $newfile $suberrors]
   }
   if {[string match ".o?f" $file_ext]} {
      return [TestCompareOVF $oldfile $newfile]
   }
   puts "UNKNOWN FILE TYPE; NO COMPARISON: \"$oldfile\" \"$newfile\""
   return 0
}

proc ExcludeTests { testlist } {
   # Each testlist element has the form
   #    basetest testlevel mifdir resultdir [subtest# subtest# ...]
   # If no subtests specified, then all subtests are included.  Subtests
   # may include ranges.

   # Each excludelist element has the form
   #    basetest {subtest# subtest# ...} "Reason for exclusion"
   # If no subtests specified, then all subtests are included.  Subtests
   # may include ranges.

   global noexcludes examples_dir load_dir bug_dir local_load_dir skipped_tests
   if {$noexcludes} { return $testlist }
   set excludelist {}
   catch {source [file join $load_dir exclude.tcl]}
   set loadexcludes [lsort $excludelist]
   set excludelist {}
   catch {source [file join $bug_dir exclude.tcl]}
   set bugexcludes [lsort $excludelist]

   set excludelist {}
   catch {source [file join $local_load_dir exclude.tcl]}
   set localexcludes [lsort $excludelist]

   set excludelist [concat $loadexcludes $bugexcludes $localexcludes]

   set skipped_tests {}
   foreach excludetest $excludelist {
      set matchlist {}
      set index 0
      set check [lindex $excludetest 0]
      foreach elt $testlist {
         if {[string compare $check [lindex $elt 0]]==0} {
            lappend matchlist $index
         }
         incr index
      }
      if {[llength $matchlist]>0} {
         lappend skipped_tests $excludetest
      }
      foreach index $matchlist {
         if {[llength [lindex $excludetest 1]]==0} {
            # No subtests specified in exclude list, so remove all
            # tests of this basetest; indicate this by replacing
            # entry in testlist with an empty list.  We'll come
            # back at the end and remove all empty entries.
            set testlist [lreplace $testlist $index $index {}]
         } else {
            # Exclude list specifies a sub-range.
            set test [lindex $testlist $index]
            # Open subtest file to determine subtest count.
            set basetest   [lindex $test 0]
            set resultsdir [lindex $test 3]
            set subfile [file join $resultsdir ${basetest}.subtests]
            set lines [ReadSubtestFile $subfile]
            set subcount [llength $lines]
            if {0 == $subcount} { set subcount 1 }

            # Make explicit subtest list with no embedded ranges.
            set newsub {}
            if {[llength $test] == 4} {
               # testlist specifies run all subtests.
               for {set i 0} {$i<$subcount} {incr i} {
                  lappend newsub $i
               }
            } else {
               # Include only explicitly requested subtests
               set subtests [lrange $test 4 end]
               foreach i $subtests {
                  if {[regexp {^([0-9]*)-([0-9]*)$} $i dummy elta eltb]} {
                     # Range request
                     if {[string match {} $elta]} { set elta 0 }
                     if {[string match {} $eltb] || $eltb>=$subcount} {
                        set eltb [expr {$subcount - 1}]
                     }
                     for {set i $elta} {$i<=$eltb} {incr i} {
                        lappend newsub $i
                     }
                  } elseif {0<=$i && $i<$subcount} {
                     # Individual test
                     lappend newsub $i
                  }
               }
            }

            # Remove elements from newsub that are in exclude list
            foreach i [lindex $excludetest 1] {
               if {[regexp {^([0-9]*)-([0-9]*)$} $i dummy elta eltb]} {
                  # Range request
                  if {[string match {} $elta]} { set elta 0 }
                  if {[string match {} $eltb] || $eltb>=$subcount} {
                     set eltb [expr {$subcount - 1}]
                  }
                  for {set i $elta} {$i<=$eltb} {incr i} {
                     foreach match [lsearch -exact -all $newsub $i] {
                        set newsub [lreplace $newsub $match $match {}]
                     }
                  }
               } elseif {0<=$i && $i<$subcount} {
                  # Individual test
                  foreach match [lsearch -exact -all $newsub $i] {
                     set newsub [lreplace $newsub $match $match {}]
                  }
               }
            }
            set tmp {}
            foreach elt $newsub {
               if {[llength $elt]>0} {
                  lappend tmp $elt
               }
            }
            set newsub $tmp
            
            # Update testlist
            if {[llength $newsub] ==0 } {
               set test {}
            } else {
               set test [concat [lrange $test 0 3] $newsub]
            }
            set testlist [lreplace $testlist $index $index $test]
         }
      }
   }

   # Remove empty members from testlist
   set newtestlist {}
   foreach elt $testlist {
      if {[llength $elt] != 0} {
         lappend newtestlist $elt
      }
   }
   return $newtestlist
}

proc ReadSubtestFile { subfile } {
   set chan [open $subfile r]
   set lines [read -nonewline $chan]
   close $chan
   # Remove extraneous whitespace
   set lines [string trim $lines]
   regsub -all "\[ \t]*\n\[ \t]*" $lines "\n" lines
   regsub -all "\n\n+" $lines "\n" lines
   # Break lines into records
   set lines [split $lines "\n"]
   if {[llength $lines]==0} { set lines [list {}] }
   return $lines
}

proc TestCount { testlist } {
   # Here "testlist" should be a list having the structure
   # specified for global "dotests" list.
   set count 0
   foreach test $testlist {
      # Open subtests file and count lines.
      set basetest   [lindex $test 0]
      set resultsdir [lindex $test 3]
      set subfile [file join $resultsdir ${basetest}.subtests]
      if {[catch {ReadSubtestFile $subfile} lines]} {
         # Assume that file dne, but will be constructed
         # later as an empty file
         set lines {}
      }
      set subcount [llength $lines]
      if {0 == $subcount} { set subcount 1 }

      set reqlist [lrange $test 4 end]
      ## reqlist are explicit subtest requests

      if {[llength $reqlist] == 0} {
         # Include all subtests
         incr count $subcount
      } else {
         # Include only explicitly requested subtests
         foreach i $reqlist {
            # Replace commas with spaces:
            if {[regexp {^([0-9]*)-([0-9]*)$} $i dummy elta eltb]} {
               # Range request
               if {[string match {} $elta]} { set elta 0 }
               if {[string match {} $eltb] || $eltb>=$subcount} {
                  set eltb [expr {$subcount - 1}]
               }
               if {$eltb >= $elta} {
                  incr count [expr {$eltb - $elta + 1}]
               }
            } elseif {0<=$i && $i<$subcount} {
               # Individual test request
               incr count
            }
         }
      }
   }
   return $count
}

set dotests [ExcludeTests $dotests]

if {$listtests} {
   set dumplist {}
   foreach elt $dotests {
      lappend dumplist [lreplace $elt 1 3]
   }
   puts $dumplist
   puts "Total test count: [TestCount $dotests]"
   exit
}

if {$updaterefdata} {
   puts -nonewline \
      "Overwrite reference results for [TestCount $dotests]\
       tests with new results? \[N/y\] -->"
   flush stdout
   gets stdin answer
   set answer [string tolower $answer]
   if {![string match "y" $answer] && ![string match "yes" $answer]} {
      exit 4
   }
}

# Build new tests with empty parameter list
foreach root $new_tests {
   set newfile [file join $load_dir ${root}.subtests]
   set chan [open $newfile w]
   close $chan
}

# Launch an instance of mmArchive for use by tests.  (If there are other
# instances of mmArchive running, then the tests may end up using one or
# more of those instead, but the distinguishing feature of the one
# launched here is that we send it a terminate request when we're done.
set mmArchive_command [exec_ignore_stderr $TCLSH $OOMMF +command mmArchive +bg]
set mmArchive_command [linsert $mmArchive_command 0 exec_ignore_stderr]
set mmArchive_pid [eval $mmArchive_command]
after 1000

# Make sure the instance of mmArchive launched above is
# registered with the account server, and save OID info.
# This may take a few seconds.
set mmArchive_oid [GetOid mmArchive $mmArchive_pid]

# Run tests
set runerrors {}
set timeouterrors {}
set comperrors {}
set testcount 0
set passedcount 0
set runerrcount 0
set timeouterrcount 0
set comperrcount 0
set newcount 0
set boxsi_base_command [exec_ignore_stderr $TCLSH $OOMMF +command boxsi]
if {[info exists thread_count] && $thread_count>0} {
   lappend boxsi_base_command -threads $thread_count
}
# To stress-test the host+account server launching+shutdown code,
# uncomment the next line to cause a full restart with each test.
# lappend boxsi_base_command -kill all
if {[string compare "Darwin" $tcl_platform(os)] != 0} {
   # On (at least some) Mac OS X, this redirect causes spurious EOFs
   # on unrelated channel reads.  OTOH, this redirect may protect
   # against console problems on Windows.
   lappend boxsi_base_command "<" $nuldevice
}
set teststart_time [clock seconds]
set number_of_tests [TestCount $dotests]
if {$number_of_tests<1} {
   puts stderr "*** NO TESTS ***"
   exit 1
}
set tstfmt [format "%%%dd" [expr {int(floor(log10($number_of_tests)))+1}]]
foreach test $dotests {
   set basetest   [lindex $test 0]
   set reglevel   [lindex $test 1]
   set mifdir     [lindex $test 2]
   set resultsdir [lindex $test 3]
   set miffile [file join $mifdir ${basetest}.mif]
   if {![file exists $miffile]} {
      set miffile [file join $mifdir ${basetest}.mif2]
      if {![file exists $miffile]} {
         puts stderr "Unable to find MIF file for test $basetest; Skipping"
         continue
      }
   }
   set subfile [file join $resultsdir ${basetest}.subtests]
   set lines [ReadSubtestFile $subfile]

   set testlist {}
   if {[llength $test]>4} {
      # Do particular subtests
      foreach i [lrange $test 4 end] {
         if {[regexp {^([0-9]*)-([0-9]*)$} $i dummy elta eltb]} {
            # Range request
            if {[string match {} $elta]} { set elta 0 }
            if {[string match {} $eltb] || $eltb>=[llength $lines]} {
               set eltb [expr {[llength $lines]-1}]
            }
            for {set j $elta} {$j<=$eltb} {incr j} {
               lappend testlist [list $basetest $j [lindex $lines $j]]
            }
         } elseif {$i<0 || $i>=[llength $lines]} {
            puts stderr \
               "Skipping unrecognized command line test \"$basetest $i\""
         } else {
            # Individual test request
            lappend testlist [list $basetest $i [lindex $lines $i]]
         }
      }
   } else {
      # Do all subtests
      set index 0
      foreach params $lines {
         lappend testlist [list $basetest $index $params]
         incr index
      }
   }

   foreach subdesc $testlist {
      incr testcount

      set subtest [lindex $subdesc 0]

      if {[llength $lines]>1} {
         set subtestname "$subtest [lindex $subdesc 1]"
         set subcomp_base "${subtest}_[lindex $subdesc 1]"
      } else {
         set subtestname $subtest
         set subcomp_base ${subtest}
      }

      puts [format "TEST $tstfmt/$tstfmt: $subtestname ---------------" \
               $testcount $number_of_tests ]
      flush stdout

      if {$reglevel == 1} {
         set subcomp [list [file join $resultsdir ${subcomp_base}.odt]]
      } else {
         set subcomp [glob -nocomplain \
                         [file join $resultsdir ${subcomp_base}.odt]]
         set subcomp [concat $subcomp \
                      [lsort [glob -nocomplain \
                       [file join $resultsdir ${subcomp_base}-*.o?f]]]]
      }
      if {[catch {ParseSubtestLine [lindex $subdesc 2]} lineparts]} {
         puts "ERROR: $lineparts"
         lappend runerrors $subtestname
         incr runerrcount
         continue
      } 
      set subparams [lindex $lineparts 0]
      set suberrors [lindex $lineparts 1]

      set results_file_odt [file join $mifdir ${results_basename}.odt]
      if {[file exists $results_file_odt]} {
         # Make sure we start on a clean slate
         file delete $results_file_odt
      }

      # Build boxsi command line
      set boxsi_command [linsert $boxsi_base_command end \
                            -loglevel $loglevel \
                            -logfile  $logfile \
                            -regression_testname $results_basename \
                            -regression_test $reglevel]
      if {[llength $subparams]>0} {
         lappend boxsi_command -parameters $subparams
      }
      lappend boxsi_command $miffile $ERR_REDIRECT

      # Run boxsi
      if {$verbose} {
         puts "CMD: $boxsi_command"
      }
      set start_time [clock seconds]
      puts "start time: [clock format $start_time]"
      flush stdout
      #set code [catch {eval $boxsi_command} errmsg]
      foreach {code errmsg} \
         [TimeoutExec $boxsi_command $EXEC_TEST_TIMEOUT] break
      set stop_time [clock seconds]
      set elapsed_time [expr {$stop_time-$start_time}]
      puts [format "stop  time: %s (%2d second%s)" \
               [clock format $stop_time] $elapsed_time \
               [expr {$elapsed_time == 1 ? "" : "s"}]]
      flush stdout

      if {$code > 0} {
         lappend runerrors $subtestname
         puts "RUN ERROR-->\n[string trim $errmsg]\n<--RUN ERROR"
         incr runerrcount
      } elseif {$code < 0} {
         lappend timeouterrors $subtestname
         puts "TIMEOUT-->\n[string trim $errmsg]\n<--TIMEOUT"
         incr timeouterrcount
      } else {
         if {$leak} {
            regexp {definitely lost: (\d+) bytes} $errmsg -> count
            if {$count} {
               puts "LEAKED $count bytes"
            }
         }
         if {$showoutput} {
            flush stderr
            puts "TEST OUTPUT >>>>"
            puts [string trim $errmsg]
            puts "<<<< TEST OUTPUT"
            flush stdout
         }
         if {$reglevel==1 && ![file exists [lindex $subcomp 0]]} {
            # New test
            puts "New test; no comparison checks"
            incr newcount
            file rename $results_file_odt [lindex $subcomp 0]
         } elseif {$updaterefdata} {
            set blen [expr {[string length $subcomp_base]-1}]
            foreach check_file $subcomp {
               set results_file [file tail $check_file]
               set results_file [string replace $results_file 0 $blen \
                                    $results_basename]
               set results_file [file join $mifdir $results_file]
               if {![file exists $results_file]} {
                  set code 1
                  puts "ERROR: Missing output file $results_file"
               } else {
                  puts "Overwriting \"$check_file\""
                  file rename -force $results_file $check_file
               }
            }
         } else {
            # Otherwise, do comparison
            set blen [expr {[string length $subcomp_base]-1}]
            if {$verbose && [llength $subcomp]==0} {
               puts "No check file comparisons"
            }
            foreach check_file $subcomp {
               set results_file [file tail $check_file]
               set results_file [string replace $results_file 0 $blen \
                                    $results_basename]
               set results_file [file join $mifdir $results_file]
               if {![file exists $results_file]} {
                  set code 1
                  puts "ERROR: Missing output file $results_file"
               } else {
                  set testresults \
                     [TestCompare $check_file $results_file $suberrors]
                  if {$testresults != 0} {
                     set code 1
                     if {$keepfail} {
                        file rename -force $results_file \
                           "${check_file}_failed"
                     }
                  }
               }
            }
            if {$code} {
               lappend comperrors $subtestname
               incr comperrcount
               puts "Test failed"
            } else {
               incr passedcount
               puts "Test passed"
            }
         }
      }
      # Delete any "non-saved" results files
      foreach results_file [glob -nocomplain \
                               [file join $mifdir ${results_basename}*.o??]] {
         set time0 [clock seconds]
         for {set idelete 0} {$idelete<20} {incr idelete} {
            if {![catch {file delete $results_file}]} {
               break
            }
            after 500
         }
         set deletetime [expr {[clock seconds]-$time0}]
         if {[file exists $results_file]} {
            error "Unable to delete results file \"$results_file\""
         }
         if {$deletetime>10} {
            global max_deletetime
            if {![info exists max_deletetime]} {
               set max_deletetime $deletetime
            }
            if {$deletetime>=$max_deletetime} {
               puts stderr "HEY! It took $deletetime seconds to\
                            delete $results_file"
               global max_deletetime_file
               set max_deletetime $deletetime
               set max_deletetime_file $results_file
            }
         }
      }
      set testtime [expr {[clock seconds] - $teststart_time}]
      set testtime_minutes [expr {int(floor($testtime/60.))}]
      set testtime_remainder [expr {$testtime - 60*$testtime_minutes}]
      puts [format "Running total: $tstfmt passed, $tstfmt failed,\
                    elapsed time: %d:%02d" \
               $passedcount \
               [expr {$runerrcount + $timeouterrcount + $comperrcount}] \
               $testtime_minutes $testtime_remainder]
      puts "---"
      flush stdout
   }
}
set teststop_time [clock seconds]

puts "\n--- SUMMARY ---"
puts "Total  tests: $testcount"
puts "Tests passed: $passedcount"
puts "Tests failed: [expr {$runerrcount + $timeouterrcount + $comperrcount}]"
if {$newcount>0} {
   puts "New    tests: $newcount"
}
if {[llength $runerrors]>0} {
   puts "Run failures: $runerrors"
}
if {[llength $timeouterrors]>0} {
   puts "Timeout failures: $timeouterrors"
}
if {[llength $comperrors]>0} {
   puts "Comparison failures: $comperrors"
}
if {[info exists skipped_tests] && [llength $skipped_tests]>0} {
   puts "Skipped tests:"
   set colwidthA -1
   set colwidthB -1
   foreach elt $skipped_tests {
      set cwA [string length [lindex $elt 0]]
      if {$cwA>$colwidthA} {
         set colwidthA $cwA
      }
      set cwB [string length [lindex $elt 1]]
      if {$cwB>$colwidthB} {
         set colwidthB $cwB
      }
   }
   foreach elt $skipped_tests {
      puts [format " %${colwidthA}s  %-${colwidthB}s  Reason: %s" \
               [lindex $elt 0] [lindex $elt 1] [lindex $elt 2]]
   }
}
set testtime [expr {$teststop_time - $teststart_time}]
set testtime_minutes [expr {int(floor($testtime/60.))}]
set testtime_remainder [expr {$testtime - 60*$testtime_minutes}]
puts [format "Total test time: %3d:%02d" \
         $testtime_minutes $testtime_remainder]

global max_deletetime max_deletetime_file
if {[info exists max_deletetime]} {
   puts stderr "Maximum file delete time: $max_deletetime\
                for $max_deletetime_file"
}

# Send die request to mmArchive
Cleanup

# Report errors, if any
if {$runerrcount + $timeouterrcount + $comperrcount > 0} {
   exit 10
}
