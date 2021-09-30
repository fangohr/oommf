# FILE: tempfile.tcl
#
# Instances of this class represent temporary files -- files created by
# an application for some purpose, but which should not remain after the
# application terminates.
#
# Last modified on: $Date: 2014/01/15 05:36:55 $
# Last modified by: $Author: donahue $

Oc_Class Oc_TempFile {

    common unique 0

    # Proc to test if directory dirname is a suitable candidate
    # directory for temporary files.
    private proc CheckDir { dirname } {
       if {![file isdirectory $dirname]} { return 0 }
       if {![file writable $dirname]} { return 0 }
       # To be certain that we can actually create a file in the
       # directory, test it.
       set base [file join $dirname test-[info hostname]-[pid]]
       if {[catch {Oc_OpenUniqueFile -pfx $base} result]} {
          return 0  ;# No joy
       }
       close [lindex $result 0]
       file delete [lindex $result 1]
       return 1  ;# Directory looks good
    }

    # If user hasn't explicitly set a directory for temporary files,
    # then try to find one.
    private proc DefaultDirectory {} {
       global tcl_platform env
       set result "."       ;# In case all else fails
       if {[string match windows $tcl_platform(platform)]} {
          if {[info exists env(TEMP)] && [$class CheckDir $env(TEMP)]} {
             return $env(TEMP)
          }
          if {[info exists env(TMP)] && [$class CheckDir $env(TMP)]} {
             return $env(TMP)
          } 
          if {[info exists env(TMPDIR)] && [$class CheckDir $env(TMPDIR)]} {
             return $env(TMPDIR)
          }
          if {[info exists env(HOMEDRIVE)] && [info exists env(HOMEPATH)]} {
             set bdir "$env(HOMEDRIVE)$env(HOMEPATH)"
          } elseif {[info exists env(USERPROFILE)]} {
             set bdir "$env(USERPROFILE)"
          } else {
             catch {unset bdir}  ;# Safety
          }
          if {[info exists bdir]} {
             foreach tdir [list \
              [file join $bdir AppData Local Temp] \
              [file join $bdir "Local Settings" TEMP] \
              [file join $bdir TEMP] \
              [file join $bdir TMP] \
              $bdir] {
                if {[$class CheckDir $tdir]} {
                   return $tdir
                }
             }
          }
          if {[$class CheckDir C:/TEMP]} {
             return C:/TEMP
          }
          if {[$class CheckDir C:/TMP]} {
             return C:/TMP
          }
          if {[info exists env(PUBLIC)]} {  ;# Windows Vista
             set tdir $env(PUBLIC)
          } else {
             set tdir "C:/Users/Public"
          }
          set tdir [file join $tdir Documents]
          if {[$class CheckDir $tdir]} {
             return $tdir
          }
          if {[$class CheckDir C:/]} {
             return C:/
          }
       } else { ;# Assume Unix
          foreach test [list TMP TMPDIR TEMP] {
             if {[info exists env($test)] && [$class CheckDir $env($test)]} {
                return $env($test)
             }
          }
          if {[$class CheckDir /tmp]} {
             return /tmp
          }
       }
       if {[$class CheckDir .]} {
          return .
       }
       error "Can't find a directory with permission\
		to write a temporary file"
    }

    const public variable stem = _
    const public variable extension = {}
    const public variable directory

    private variable absoluteName
    private variable fileHandle
    private variable cleanup

    Constructor {args} {
       eval $this Configure $args
       if {![info exists directory]} {
          global env
          if {[info exists env(OOMMF_TEMP)]} {
             set directory $env(OOMMF_TEMP)
             set dirsrc "environment variable OOMMF_TEMP"
          } elseif {![catch {[Oc_Config RunPlatform] GetValue \
                                path_directory_temporary} directory]} {
             set dirsrc "platform config value path_directory_temporary"
          } else {
             set directory [$class DefaultDirectory]
             set dirsrc "OOMMF default temp directory search"
          }
       }
       if {[string match volumerelative [file pathtype $directory]]} {
          set drive [lindex [file split [pwd]] 0]
          set absoluteDir [eval file join \
               [lreplace [file split $directory] 0 0 $drive]]
       } else {
          set absoluteDir [file join [pwd] $directory]
       }
       # absoluteDir should be in absolute form.  Cheap check for now.
       set direrr 0
       if {![string match absolute [file pathtype $absoluteDir]]} {
          # The Cygwin environment can show symptoms of multiple
          # personality disorder.
          global tcl_platform
          if {![string match windows $tcl_platform(platform)] ||
              ![string match absolute [file pathtype \
                 [file nativename $absoluteDir]]]} {
             set msg "Programming error:\
                        Temp filename $absoluteDir not absolute."
             set direrr 1
          }
       }
       if {!$direrr && ![file isdirectory $absoluteDir]} {
          set msg "Temporary directory '$absoluteDir' does not exist"
          set direrr 1
       }
       if {!$direrr && ![$class CheckDir $absoluteDir]} {
          set msg "Temporary directory '$absoluteDir' is not writable"
          set direrr 1
       }
       if {$direrr} {
          if {[info exists dirsrc]} {
             # Caller did not name directory.  Our default is bad.
             append msg "\nDirectory setting from $dirsrc is bad."
             append msg "\nEither edit the path_directory_temporary\
                         setting in your platform config"
             append msg "\nfile or set the OOMMF_TEMP environment\
                         variable to a directory where"
             append msg "\nyou have write access."
          }
          error $msg $msg
       }

       set base [file join $absoluteDir $stem-[info hostname]-[pid]]
       foreach {fileHandle absoluteName unique} \
          [Oc_OpenUniqueFile -sfx $extension -start $unique \
              -pfx $base] \
          break

       # Set up so that temp files get cleaned up on exit, unless
       # claimed by some caller.
       Oc_EventHandler New cleanup Oc_Main LibShutdown [list $this Delete]
    }

    Destructor {
        if {[info exists fileHandle]} {
            catch {close $fileHandle}
        }
       if {[info exists cleanup] && [string compare Oc_Nop $cleanup]} {
	    $cleanup Delete
            if {[info exists absoluteName]} {
        	file delete $absoluteName
            }
	}
    }

    method Claim {} {
	$cleanup Delete
	set cleanup Oc_Nop
    }

    method AbsoluteName {} {
        return $absoluteName
    }

    method Handle {} {
        return $fileHandle
    }
}
