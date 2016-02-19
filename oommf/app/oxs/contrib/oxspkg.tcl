# File: oxspkg.tcl
#
# This Tcl script provides support for installing, uninstalling,
# and copying contrib extensions into and out of the oxs/local
# directory

# Install directory, relative to script directory
set INSTALLDIR [file join .. local]

# Directory names to exclude from package search
set EXCLUDEPKG [list CVS newcontrib tmp]

# Files to exclude from package list (glob-style match)
set EXCLUDEFILES [list versdate.txt readme.txt copying.txt \
		      README* *README.*]

# Patch program
set PATCH patch

# Env overrides of the above
global env
foreach elt [list INSTALLDIR EXCLUDEPKG PATCH] {
   if {[info exists env(OOMMF_$elt)]} {
      set $elt $env(OOMMF_$elt)
   }
}

# USAGE
proc Usage {} {
   puts stderr "Usage: oxspkg list"
   puts stderr "  or   oxspkg listfiles pkg \[pkg ...\]"
   puts stderr "  or   oxspkg install \[-v\] \[-nopatch\] pkg \[pkg ...\]"
   puts stderr "  or   oxspkg uninstall pkg \[pkg ...\]"
   puts stderr "  or   oxspkg copyout   pkg \[pkg ...\] destination"
   puts stderr "Wildcards (or keyword \"all\") accepted in pkg specifications."
   exit 255
}

global verbose
set verbose [lsearch -regexp $argv {^-+v(|erbose)$}]
if {$verbose<0} {
   set verbose 0
} else {
   set argv [lreplace $argv $verbose $verbose]
   set verbose 1
}

set argc [llength $argv]
if {$argc<1 ||
    [lsearch -regexp $argv {^-+h(|elp)$}]>=0} {
   Usage
}

# Change keyword "all", if any, to wildcard "*"
for {set i 1} {$i<[llength $argv]} {incr i} {
   if {[string compare all [lindex $argv $i]]==0} {
      set argv [lreplace $argv $i $i "*"]
   }
}

proc GetMaxStringLength { inlist } {
   set maxnamlen 0
   foreach elt $inlist {
      if {[string length $elt]>$maxnamlen} {
         set maxnamlen [string length $elt]
      }
   }
   return $maxnamlen
}

proc MatchPackages { pkguniverse patlist } {
   set matchlist {}
   foreach pat $patlist {
      # Note: The -all and -inline options to lsearch appear in Tcl 8.4.x
      set matchlist [concat $matchlist \
                        [lsearch -glob -all -inline $pkguniverse $pat]]
   }
   return $matchlist
}

proc FindFiles { dir } {
   global EXCLUDEFILES
   set files {}
   foreach testfile [lsort [glob -nocomplain -directory $dir -types f *]] {
      set patmatch 0
      foreach pat $EXCLUDEFILES {
         if {[string match $pat [file tail $testfile]]} {
            set patmatch 1
            break
         }
      }
      if {!$patmatch} {
         lappend files $testfile
      }
   }

   set subdirs [lsort [glob -nocomplain -directory $dir -types d *]]
   set index [lsearch -glob $subdirs [file join * CVS]]
   if {$index >= 0} {
      set subdirs [lreplace $subdirs $index $index]
   }
   foreach elt $subdirs {
      set files [concat $files [FindFiles $elt]]
   }
   return $files
}

proc ListFiles { pkg } {
   puts "Files contained in package \"$pkg\" ---"
   puts [join [FindFiles $pkg] "\n"]
   puts "---"
}

proc MakeInstallName { pkgname filename } {
   global INSTALLDIR
   set installname [file tail $filename]
   if [regexp -nocase {^readme} $installname] {
      set installname "${pkgname}-${installname}"
   }
   set installname [file join $INSTALLDIR $installname]
   return $installname
}

proc CheckInstall { pkg } {
   # Returns a two element list.
   # The first element is a list of all installed files.
   # The second element is a list of all files not installed.
   # NOTE: The installed file list gives the file names as they appear
   #       in the installed area.  The not installed list gives the
   #       names as they appear in the contrib package area.
   set installed_list {}
   set notinstalled_list {}
   foreach nfile [FindFiles $pkg] {
      set ifile [MakeInstallName $pkg $nfile]
      if {[file exists $ifile]} {
         lappend installed_list $ifile
      } else {
         lappend notinstalled_list $nfile
      }
   }
   return [list $installed_list $notinstalled_list]
}

proc ListPackages { pkgs } {
   set maxnamlen [GetMaxStringLength $pkgs]
   puts "Available contrib packages ---"
   foreach elt $pkgs {
      foreach {ilist nlist} [CheckInstall $elt] break
      set fcount [expr {[llength $ilist] + [llength $nlist]}]
      if {$fcount == 0} {
         set status {}
      } elseif {[llength $ilist]==0} {
         set status "Not installed"
      } elseif {[llength $nlist]==0} {
         set status "Installed"
      } else {
         set status "Partial install ([llength $ilist]/$fcount)"
      }
      puts [format "%*s %4d files   %s" $maxnamlen $elt $fcount $status]
   }
   puts "---"
}

proc Install { dopatch pkg } {
   global verbose

   # Copy files
   set filelist [FindFiles $pkg]
   foreach elt $filelist {
      file copy -force -- $elt [MakeInstallName $pkg $elt]
   }
   if {$verbose} {
      puts [format "%2d files installed: %s" [llength $filelist] $filelist]
   } else {
      puts -nonewline [format "%2d files installed." [llength $filelist]]
   }
   if {!$dopatch} {
      puts {}
      return
   }

   # Search for patches
   set patchfile [lsort [glob -nocomplain -types f -path ${pkg}- *.patch]]
   if {[llength $patchfile]>1} {
      # Multiple patches; pick biggest!?
      puts "\n*** Multiple patch files found ***"
      puts [join $patchfile "\n"]
      puts "******"
      set bigpatch {}
      set bigsize -1
      foreach elt $patchfile {
         if {[file size $elt] > $bigsize} {
            set bigpatch $elt
            set bigsize [file size $elt]
         }
      }
      set patchfile $bigpatch
   }

   # Apply patches, if any
   if {[llength $patchfile]==0} {
      puts "   No patches found."
   } else {
      puts -nonewline "   Patching with \"$patchfile\" ..."
      global PATCH INSTALLDIR
      set cmd [linsert $PATCH 0 exec]
      set cmdargs [list -f -p1 --no-backup-if-mismatch \
                      -d $INSTALLDIR "<" $patchfile]
      # Test with --dry-run first, so if any errors then we can kick out
      # without leaving behind .rej files.  Also, on Windows, test both
      # with and w/o --binary option.
      set drycmd [concat $cmd --binary --dry-run $cmdargs]
      set cmdB [concat $cmd --binary $cmdargs]
      if {[catch {eval $drycmd} errmsgB] || [catch {eval $cmdB} errmsgB]} {
         global tcl_platform
         set drycmd [concat $cmd --dry-run $cmdargs]
         set cmdT [concat $cmd $cmdargs]
         if {![string match windows $tcl_platform(platform)] \
                || [catch {eval $drycmd} errmsgT] \
                || [catch {eval $cmdT} errmsgT]} {
            puts " ERROR"
            if {$verbose} {
               # Don't know which error message to report, but
               # --binary result is probably best guess.
               puts "---\n$errmsgB\n---"
            }
         } else {
            puts " OK"
         }
      } else {
         puts " OK"
      }
   }
}

proc Uninstall { pkg } {
   set count 0
   set filelist [FindFiles $pkg]
   foreach elt $filelist {
      set installfile [MakeInstallName $pkg $elt]
      if {[file exists $installfile]} {
         file delete $installfile
         incr count
      }
   }
   if {$count == [llength $filelist]} {
      puts [format "%2d files deleted." $count]
   } else {
      puts [format "%2d files deleted\
       (package contains [llength $filelist] files)." $count]
   }
}

proc CopyOut { pkg dest } {
   if {![file exists $dest]} {
      file mkdir $dest
   } elseif {![file isdirectory $dest]} {
      puts stderr "\nERROR: Destination \"$dest\" is not a directory"
      exit 2
   }

   set count 0
   set filelist [FindFiles $pkg]
   foreach elt $filelist {
      set installfile [MakeInstallName $pkg $elt]
      if {[file exists $installfile]} {
         file copy -force $installfile $dest
         incr count
      }
   }
   if {$count == [llength $filelist]} {
      puts [format "%2d files copied." $count]
   } else {
      puts [format "%2d files copied\
       (package contains [llength $filelist] files)." $count]
   }
}

set cmd [lindex $argv 0]

set scriptdir [file dirname $argv0]
cd $scriptdir

set contrib_packages [lsort [glob -types d *]]
foreach elt $EXCLUDEPKG {
   set index [lsearch -exact $contrib_packages $elt]
   if {$index >= 0} {
      set contrib_packages [lreplace $contrib_packages $index $index]
   }
}

switch -exact $cmd {
   list {
      if {$argc!=1} {
         puts stderr "ERROR: Too many arguments"
         Usage
      }
      ListPackages $contrib_packages
   }
   listfiles {
      if {$argc<2} {
         puts stderr "ERROR: Too few arguments"
         Usage
      }
      set pkglist [MatchPackages $contrib_packages [lrange $argv 1 end]]
      if {[llength $pkglist]<1} {
         puts stderr "ERROR: No matching packages"
         Usage
      }
      foreach pkg $pkglist  {
         ListFiles $pkg
      }
   }
   install {
      if {$argc<2} {
         puts stderr "ERROR: Too few arguments"
         Usage
      }
      set dopatch [lsearch -regexp $argv {^-+nopat}]
      if {$dopatch < 0} {
         set dopatch 1
      } else {
         set argv [lreplace $argv $dopatch $dopatch]
         set dopatch 0
      }
      set pkglist [MatchPackages $contrib_packages [lrange $argv 1 end]]
      if {[llength $pkglist]<1} {
         puts stderr "ERROR: No matching packages"
         Usage
      }
      set maxnamlen [GetMaxStringLength $pkglist]
      foreach pkg $pkglist  {
         puts -nonewline [format "%*s: " $maxnamlen $pkg]
         Install $dopatch $pkg
      }
   }
   uninstall {
      if {$argc<2} {
         puts stderr "ERROR: Too few arguments"
         Usage
      }
      set pkglist [MatchPackages $contrib_packages [lrange $argv 1 end]]
      if {[llength $pkglist]<1} {
         puts stderr "ERROR: No matching packages"
         Usage
      }
      set maxnamlen [GetMaxStringLength $pkglist]
      foreach pkg $pkglist  {
         puts -nonewline [format "%*s: " $maxnamlen $pkg]
         Uninstall $pkg
      }
   }
   copyout {
      if {[llength $argv]<3} {
         puts stderr "ERROR: Too few arguments"
         Usage
      }
      set dest [lindex $argv end]
      set pkgend [expr {[llength $argv]-2}]
      set pkglist [MatchPackages $contrib_packages [lrange $argv 1 $pkgend]]
      if {[llength $pkglist]<1} {
         puts stderr "ERROR: No matching packages"
         Usage
      }
      set maxnamlen [GetMaxStringLength $pkglist]
      if {![file exists $dest]} {
         file mkdir $dest
      } elseif {![file isdirectory $dest]} {
         puts stderr "\nERROR: Destination \"$dest\" is not a directory"
         exit 2
      }
      foreach pkg $pkglist  {
         puts -nonewline [format "%*s: " $maxnamlen $pkg]
         CopyOut $pkg [file join $dest $pkg]
      }
   }
   default {
      puts stderr "ERROR: Unknown command \"$cmd\""
      Usage
   }
}
