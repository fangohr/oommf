#!/bin/sh
# FILE: simplepatch.tcl
#
# Simple implementation of unix 'patch' utility in Tcl.
# Only recognizes unified diffs, missing lots of options.
#
#    v--Edit here if necessary \
exec tclsh "$0" ${1+"$@"}
########################################################################

set slip_lines 50 ;# Number of slippage lines to allow when finding match

proc Usage { {chan stdout} {exitcode 999} } {
   puts $chan \
      {Usage: tclsh simplepatch.tcl [-p #] [-d dir] [--dry-run] < patchfile}
   puts $chan { The following options are ignored:}
   puts $chan { -f --binary --no-backup-if-mismatch}
   puts $chan "Note: For use with unified diffs only."
   exit $exitcode
}


proc CheckMatch { data search_line match_text } {
   # Returns true if data starting at search_line exactly matches
   # match_text.
   set i $search_line
   foreach line $match_text {
      if {[string compare $line [lindex $data $i]]!=0} {
         return 0 ;# Match fail
      }
      incr i
   }
   return 1 ;# Hunks match
}

proc FindMatch { data search_line match_text } {
   # Returns the index number of the hunk that exactly matches
   # match_text which is closest to search_line but no more that +/-
   # slip+lines away. Returns -1 if no match found.
   # NOTE: search_line should be the original offset specified in the
   # patch @@ line, plus any adjustment arising from preceding
   # replacements.
   global slip_lines

   # First see if data starting at search_line matches.
   if {[CheckMatch $data $search_line $match_text]} {
      return $search_line
   }

   # Otherwise check search window for match. We use lsearch to find all
   # lines the match the first line of match_text, and then call
   # CheckMatch to see if the entire hunk matches. Note that lsearch may
   # return a lot of bad candidates if the first line of match_text is
   # something wimpy like a blank line. We might want to tweak the
   # search test to use a non-trivial line for the initial screening.
   set winstart [expr {$search_line - $slip_lines}]
   if {$winstart<0} { set winstart 0}
   set winstop [expr {$search_line + $slip_lines + 1}]
   if {$winstop>[llength $data]} { set winstop [llength $data] }
   set check_lines [lsearch -exact [lrange $data $winstart $winstop] \
                       [lindex $match_text 0]]
   set best_offset [expr {$slip_lines + 1}]
   foreach candidate $check_lines {
      set test_line [expr {$candidate+$winstart}]
      if {[CheckMatch $data $test_line $match_text]} {
         set offset [expr {$test_line - $search_line}]
         if {abs($offset) <= abs($best_offset)} {
            set best_offset $offset
         }
      }
   }
   if {abs($best_offset)>$slip_lines} {
      return -1
   }
   return [expr {$search_line + $best_offset}]
}

proc ApplyPatches { filename patches } {
   # Return is a two-element list:
   #  { Total number of hunks, Number of hunks applied}

   # Read file into memory
   if {[file exists $filename]} {
      set chan [open $filename r]
      set data [split [read -nonewline $chan] "\n"]
      close $chan
   } else {
      # Assume this is to be a new file.
      set data {}
   }

   # Apply patches to memory image
   set hunk_count 0
   set hunk_good_count 0
   set offset -1  ;# Patch line numbers start with 1
   set start_lines [lsearch -all -regexp $patches {^@@[\s0-9,+-]+@@}]
   # Note: In normal diffs the "start lines" start and end with "@@". In
   #       git diffs the second "@@" pair is followed by the enclosing
   #       function or proc name.
   if {[llength $start_lines]==0} {
      puts stderr "ERROR: No hunks in patch set for file $filename"
      exit 102
   }
   lappend start_lines [llength $patches]
   set start [lindex $start_lines 0]
   foreach stop [lrange $start_lines 1 end] {
      # Note: There may be trailing garbage (e.g., "Only in ..." lines)
      #       at the tail of the patch, so use the hunk line ranges
      #       to determine where the hunk ends.
      incr hunk_count
      set match_text {}
      set replacement_text {}

      # Extract hunk size info from hunk header
      if {![regexp \
       {^@@\s+[-]([0-9,]+)\s+[+]([0-9,]+)\s+@@} \
               [lindex $patches $start] dummy arange brange]} {
         puts stderr "ERROR: Can't parse hunk start line -->"
         puts stderr [lindex $patches $start]
         puts stderr "<-------------------------------------"
         exit 104
      }
      if {![regexp {^([0-9]+)} $arange dummy search_line]} {
         puts stderr "ERROR: Can't parse hunk start line -->"
         puts stderr [lindex $patches $start]
         puts stderr "<-------------------------------------"
         exit 106
      }
      if {![regexp {,([0-9]+)$} $arange dummy alength]} {
         # Default text length is 1
         set alength 1
      }
      if {![regexp {,([0-9]+)$} $brange dummy blength]} {
         # Default text length is 1
         set blength 1
      }

      # Split hunk into before and after versions
      foreach line [lrange $patches [expr {$start+1}] [expr {$stop-1}]] {
         switch -exact -- [string index $line 0] {
            {-} {
               lappend match_text [string range $line 1 end]
            }
            {+} {
               lappend replacement_text [string range $line 1 end]
            }
            default {
               lappend match_text [string range $line 1 end]
               lappend replacement_text [string range $line 1 end]
            }
         }
         if {[llength $match_text] == $alength \
                && [llength $replacement_text] == $blength} {
            break  ;# Hunk complete
         }
      }
      if {[llength $match_text] != $alength \
             || [llength $replacement_text] != $blength} {
         puts stderr "ERROR: Can't processing hunk starting with -->"
         puts stderr [lindex $patches $start]
         puts stderr "<-------------------------------------"
         exit 108
      }
      if {$search_line == 0 && [llength $match_text]==0} {
         # Empty original file
         set match_line 0
      } else {
         incr search_line $offset
         set match_line [FindMatch $data $search_line $match_text]
      }
      if {$match_line<0} {
         puts stderr "ERROR: Can't find match for hunk -->"
         puts stderr [lindex $patches $start]
         puts stderr $match_text
         puts stderr "<-------------------------------------"
      } else {
         set match_end [expr {$match_line + [llength $match_text] -1}]
         set data [lreplace $data $match_line $match_end \
                      {*}$replacement_text]
         set offset [expr {$offset + [llength $replacement_text] \
                              - [llength $match_text]}]
         incr hunk_good_count
      }
      set start $stop
   }
   # Write out modifications
   global dry_run
   if {!$dry_run && $hunk_good_count>0} {
      set chan [open $filename w]
      puts $chan [join $data "\n"]
      close $chan
   }
   return [list $hunk_count $hunk_good_count]
}

set path_strip 0
set dry_run 0
set change_directory {}
set i 0
while {$i<[llength $argv]} {
   set option [lindex $argv $i]
   if {[string compare {-p} $option]==0 && \
          $i+1<[llength $argv] } {
      set ni [expr {$i+1}]
      set path_strip [lindex $argv $ni]
      if {![string is integer $path_strip] || $path_strip<0} {
         puts stderr "Invalid path strip request: $path_strip"
         exit 10
      }
      set argv [lreplace $argv $i $ni]
   } elseif {[regexp {^-p([0-9]+)$} $option dummy cnt]} {
      set path_strip $cnt
      set argv [lreplace $argv $i $i]
   } elseif {[string compare {-d} $option]==0 && \
                $i+1<[llength $argv] } {
      set ni [expr {$i+1}]
      set change_directory [lindex $argv $ni]
      if {![file isdirectory $change_directory]} {
         puts stderr "Invalid directory request: $change_directory"
         exit 15
      }
      set argv [lreplace $argv $i $ni]
   } elseif {[string compare {--dry-run} $option]==0} {
      set dry_run 1
      set argv [lreplace $argv $i $i]
   } elseif {[string compare {-h} $option]==0 \
             || [string compare {--help} $option]==0 } {
      Usage
   } elseif {[string compare -f $option] == 0 \
                || [string compare --binary $option] == 0 \
                || [string compare --no-backup-if-mismatch $option] == 0} {
      # Ignored option
      set argv [lreplace $argv $i $i]
   } else {
      puts stderr "Unrecognized option: $option"
      Usage stderr 19
   }
}

if {![string match {} $change_directory]} {
   if {[catch {cd $change_directory} errmsg]} {
      puts stderr "ERROR: Unable to cd to \"$change_directory\""
      puts stderr " ERRMSG: $errmsg"
      exit 20
   }
}

set patch [split [read -nonewline stdin] "\n"]

# Remove comment and "Only in" lines at top of patch
set start_index 0
while {[string match \#* [lindex $patch $start_index]] \
          || [string match {Only in *} [lindex $patch $start_index]]} {
   incr start_index
}
set patch [lreplace $patch 0 $start_index-1]

# Check that input looks like a patch file
if {![string match {diff *} [lindex $patch 0]]} {
   puts stderr "First line of patch file does not look like diff output:"
   puts stderr [lindex $patch 0]
   exit 30
}

# git diffs slip an "index..." line between each "diff <file1> <file2>"
# and "--- <file1>" line, e.g.,
#
#   diff --git a/fiz/foo.tcl b/fiz/foo.tcl
#   index cf2fc6f71..51e065448 100644
#   --- a/fiz/foo.tcl
#   +++ b/fiz/foo.tcl
#   @@ -276,6 +275,7 @@ proc ReadFile { filename } {
#        # Mesh loads in with +z forward, so transform
#
# Check and remove the "index" lines
foreach i [lreverse [lsearch -regexp -all $patch {^diff --git}]] {
   if {[regexp {^index } [lindex $patch $i+1]]} {
      set patch [lreplace $patch $i+1 $i+1]
   }
}

# Collect patchsets
set check_patchsets [lsearch -all -regexp $patch {^---}]
set patchsets {}
foreach line $check_patchsets {
   if {[string match {diff *} [lindex $patch $line-1]] && \
          [string match {+++ *} [lindex $patch $line+1]] && \
          [string match {@@ *} [lindex $patch $line+2]]} {
      lappend patchsets [expr {$line-1}]
   }
}
if {[llength $patchsets]==0} {
   puts stderr "No patch sets found in patch file"
   exit 40
}

# To simplify processing, add a dummy marker at patchsets end
lappend patchsets [llength $patch]

# Group patch sets by file
set file_patches [dict create]
set patch_start 0
set errcount 0
foreach patch_end [lrange $patchsets 1 end] {
   set onepatch [lrange $patch $patch_start $patch_end-1]
   # The source "---" line in normal (non-git) diffs have the format
   #    --- <filename> 2022-01-22 02:39:23.919149032 -0500
   # OTOH, git diffs have just
   #    --- <filename>
   # w/o the timestamp. git diffs can be identified by the preceding
   # "diff" line, that looks like
   #    diff --git <file1> <file2>
   # Handle the two casses
   set origline [lindex $onepatch 1]
   if {[regexp {^diff --git } [lindex $onepatch 0]]} {
      # git diff
      # Note: git diffs have an "index ..." line following the
      # "diff --git ..." line, but in this script we've removed
      # the index line before this point to make the diff and
      # git-diff processing more similar.
      if {![regexp {^---\s+(.+)$} $origline _ filename]} {
         puts stderr "ERROR: Bad git patch? (skipping) ---"
         puts stderr [join $onepatch "\n"]
         puts stderr "--------------------------------"
         incr errcount
         set patch_start $patch_end
         continue
      }
   } else {
      # non-git diff
      if {![regexp {^---\s+(.*\S)\s+[0-9-]+\s+[0-9:.]+\s+[0-9+-]+$} \
               $origline _ filename]} {
         puts stderr "ERROR: Bad patch? (skipping) ---"
         puts stderr [join $onepatch "\n"]
         puts stderr "--------------------------------"
         incr errcount
         set patch_start $patch_end
         continue
      }
   }
   set filename [string trim $filename]

   # Remove leading path components as requested
   set workname [file join {*}[lrange [file split $filename] $path_strip end]]

   # Check that the file is either readable, or else that the patch
   # is creating a new file.
   if {[file readable $workname] || \
          (![file exists $workname] && \
              [regexp {^@@[[:space:]]+-0,0[[:space:]]+1,[0-9]+[[:space:]]+@@$} \
                  [lindex $onepatch 0]] && \
              [lsearch -glob -not {+*} [lrange $onepatch 1 end]] < 0)} {
      dict lappend file_patches $workname [lrange $onepatch 3 end]
   } else {
      puts stderr "ERROR: File \"$workname\" is not readable. (skipping)"
      incr errcount
   }
   set patch_start $patch_end
}

if {$errcount} {
   puts stderr "ERROR: $errcount patch processing errors"
   exit 42
}

# Apply patches
set namelength 0
set reject_count 0
foreach fn [dict keys $file_patches] {
   if {[string length $fn]>$namelength} {
      set namelength [string length $fn]
   }
}
dict for {filename patches} $file_patches {
   puts -nonewline [format "FILE %${namelength}s : " $filename]
   flush stdout
   lassign [ApplyPatches $filename [join $patches]] hunks good_hunks
   puts [format "applied %2d hunks out of %2d" $good_hunks $hunks]
   incr reject_count [expr {$hunks - $good_hunks}]
}

if {$reject_count>0} {
   exit 1
}
exit 0   ;# Success
