# File: specblocks.tcl
# This Tcl script takes a list of MIF files on the command line,
# and counts the number of files with an explicit Specify block
# for each Oxs_Ext object. Note that implicit, inline Oxs_Ext
# objects are not included in the count, and the count is the
# number of matching files, not instances. The first limitation
# is because this simple code just looks text following a Specify
# command, and the latter because the intention of this script
# is to discover commonalities between MIF files for debugging
# errors flagged by oxsregression testing.

proc Usage {} {
   puts "Usage: tclsh specblocks.tcl \[-sorttypecount\]\
                file1.mif \[file2.mif ...\]"
   exit
}

set sortflag [lsearch -regexp $argv {^-+sorttypecount$}]
if {$sortflag<0} {
   set sortflag 0
} else {
   set argv [lreplace $argv $sortflag $sortflag]
   set sortflag 1
}

if {[llength $argv]<1 || [lsearch -regexp $argv {^-+(h|help)$}]>=0} {
   Usage
}

proc FindMifFiles { filepat } {
   # If filepat ends with .mif, then does a glob match for filepat in
   # the current directory. If one or more files are found, then returns
   # that list.
   #   Otherwise, if the current working directory lies under the
   # oxs directory, then as an aid to processing oxsregression errors,
   # .mif is appended to filepat and a search is made in each of the
   # standard oxsregression directories: bug_tests, load_tests,
   # local_tests, and ../examples.
   if {[string match *.mif $filepat]} {
      # Direct search
      return [glob -nocomplain -type {f r} -- $filepat]
   }

   # Check if under oxs directory
   set dirparts [file split [pwd]]
   set oxsindex [lsearch -exact $dirparts oxs]
   if {$oxsindex<0} { return {} }

   # Looks like an oxsregression test list. Try expanding with that
   # assumption.
   set updirs [expr {[llength $dirparts] - $oxsindex - 1}]
   if {$updirs==0} {
      set oxsdir {}
   } else {
      set oxsdir [file join {*}[lrepeat $updirs ..]]
   }
   if {[string compare examples \
           [lindex $dirparts [expr {$oxsindex+1}]]]==0} {
      # cwd is in examples or a subdirectory of it
      if {$updirs==1} {
         set examplesdir {}
      } else {
         set examplesdir [file join {*}[lrepeat [expr {$updirs-1}] ..]]
      }
   } else {
      set examplesdir [file join $oxsdir examples]
   }
   if {[string compare regression_tests \
           [lindex $dirparts [expr {$oxsindex+1}]]]==0} {
      # cwd is regression_tests or a subdirectory of it
      if {$updirs==1} {
         set regtstdir {}
      } else {
         set regtstdir [file join {*}[lrepeat [expr {$updirs-1}] ..]]
      }
   } else {
      set regtstdir [file join $oxsdir regression_tests]
   }


   # Tests in oxs/examples/
   append filepat .mif
   lappend filelist {*}[glob -nocomplain -directory $examplesdir \
                              -type {f r} -- $filepat]
   # Tests under oxs/regression_tests/
   foreach subdir [list bug_tests load_tests local_tests] {
      set testdir [file join $regtstdir $subdir]
      lappend filelist {*}[glob -nocomplain -directory $testdir \
                              -type {f r} -- $filepat]
   }
   return $filelist
}

# Identify file requests
set filelist {}
foreach filepat $argv {
   set matches [FindMifFiles $filepat]
   if {[llength $matches] == 0} {
      puts stderr "ERROR: Can't find any MIF files matching pattern $filepat"
   } else {
      lappend filelist {*}$matches
   }
}

set filenamelen 4
foreach file $filelist {
   set len [string length $file]
   if {$len>$filenamelen} { set filenamelen $len }
}

puts [format "%*s  %-8s  %-8s  %s" $filenamelen  {}  Specify Distinct Ungrokked]
puts [format "%*s  %-8s  %-8s  %s" $filenamelen File Blocks " Types" Lines]
puts "[string repeat - $filenamelen]  [string repeat - 8] \
      [string repeat - 8]  [string repeat - 12]"

set typecounts [dict create]
set blocksum 0
set typesum 0
set ungrokkedsum 0
foreach file $filelist {
   if {[catch {open $file r} chan]} {
      puts stderr "ERROR : Unable to open file $file : $chan"
      continue
   }
   set text [split [read -nonewline $chan] "\n"]
   close $chan
   set matches [lsearch -regexp -all $text {^\s*Specify(\s+|$)}]
   set typelist {}
   set missing {}
   foreach line $matches {
      if {[regexp {^\s*Specify\s+([^:\s]+)} [lindex $text $line] \
              dummy exttype]} {
         lappend typelist $exttype
      } else {
         lappend missing [expr {$line+1}]
         puts stderr "NOMATCH: [lindex $text $line]"
      }
   }
   set typelist [lsort -unique $typelist] ;# Remove dups
   puts [format "%*s  %4d  %8d      %s" $filenamelen $file [llength $matches] \
            [llength $typelist] $missing]
   incr blocksum [llength $matches]
   incr typesum [llength $typelist]
   incr ungrokkedsum [llength $missing]
   foreach type $typelist {
      dict incr typecounts $type
   }
}
puts "[string repeat - $filenamelen]  [string repeat - 8] \
      [string repeat - 8]  [string repeat - 12]"
puts [format "Totals: %*s  %4d  %8d      %s lines" [expr {$filenamelen-8}] \
         "[llength $filelist] files" $blocksum $typesum $ungrokkedsum]



set typenamelen 4
foreach type [dict keys $typecounts] {
   set len [string length $type]
   if {$len>$typenamelen} { set typenamelen $len }
}

# Convert typecounts dict to list. (Is there an easier way to do this?)
set typelist {}
dict for {key value} $typecounts {
   lappend typelist [list $key $value]
}

if {$sortflag} {
   set typelist [lsort -integer -decreasing -index 1 $typelist]
} else {
   set typelist [lsort $typelist]
}

puts [format "\n\n%*s  Count" $typenamelen Type]
puts [format "%*s  -----" $typenamelen [string repeat - $typenamelen]]
foreach pair $typelist {
   puts [format "%*s %4d" $typenamelen {*}$pair]
}
