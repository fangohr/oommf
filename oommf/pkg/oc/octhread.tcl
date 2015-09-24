# FILE: octhread.tcl                   -*-Mode: tcl-*-
# Tcl script portion of Oc class multi-threaded (parallel) code support.
#
proc Oc_GetDefaultThreadCount {} {
   # Default thread request count.  This may be overridden by
   # application (for example, using a value from the command line).
   # The default may be set in any of the following four ways, listed
   # in order of increasing priority:
   # 
   #   Global default value is 2 threads.
   # 
   #   In the oommf/config/platform/"platform.tcl" file:
   #      [Oc_Config RunPlatform] SetValue thread_count <value>
   # 
   #   In the oommf/config/options.tcl file:
   #      Oc_Option Add * Threads oommf_thread_count <value>
   # 
   #   From the shell environment variable,
   #      OOMMF_THREADS
   global env
   set otc -1
   if {[info exists env(OOMMF_THREADS)]} {
      # Set from environment
      set otc $env(OOMMF_THREADS)
   } elseif {![Oc_Option Get Threads oommf_thread_count val]} {
      # Set from Oc_Option database
      set otc $val
   } elseif {![catch {[Oc_Config RunPlatform] GetValue thread_count} val]} {
      # Set from RunPlatform value
      set otc $val
   } else {
      # Last change default
      set otc 2
   }
   if {$otc<1} {
      set otc 1 ;# Safety
   }
   return $otc
}

