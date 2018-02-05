# FILE: octhread.tcl                   -*-Mode: tcl-*-
# Tcl script portion of Oc class multi-threaded (parallel) code support.
#
proc Oc_GetThreadLimit {} {
   # Returns an empty string if no hard thread limit is set.  Otherwise
   # returns the thread limit.  The thread limit can be set in any if
   # the following ways, listed in order of increasing priority:
   # 
   #   In the oommf/config/platform/"platform.tcl" file:
   #      [Oc_Config RunPlatform] SetValue thread_limit <value>
   # 
   #   In the oommf/config/options.tcl file:
   #      Oc_Option Add * Threads oommf_thread_limit <value>
   # 
   #   From the shell environment variable,
   #      OOMMF_THREADLIMIT
   #
   global env
   set hard_limit {}
   if {[info exists env(OOMMF_THREADLIMIT)]} {
      # Set from environment
      set hard_limit $env(OOMMF_THREADLIMIT)
   } elseif {![Oc_Option Get Threads oommf_thread_limit val]} {
      # Set from Oc_Option database
      set hard_limit $val
   } elseif {![catch {[Oc_Config RunPlatform] GetValue thread_limit} val]} {
      # Set from RunPlatform value
      set hard_limit $val
   }
   if {![string match {} $hard_limit]} {
      if {[catch {expr {int($hard_limit)}} hard_limit] \
             || [catch {expr {$hard_limit<1}} _] \
             || $_} {
         puts stderr "\n************************************************"
         puts stderr "ERROR: Bad setting for hard thread limit: $hard_limit"
         puts stderr "   Hard limit is disabled"
         puts stderr "************************************************"
         set hard_limit {}
      }
   }
   return $hard_limit
}

proc Oc_EnforceThreadLimit { thread_request } {
   # If the hard thread count limit is set, then the return value is the
   # smaller of $thread_request and the thread limit.  If the hard
   # thread limit is not set then the return is $thread_request.
   set hard_limit [Oc_GetThreadLimit]
   if {![string match {} $hard_limit] && $thread_request>$hard_limit} {
      set thread_request $hard_limit ;# Override
   }
   return $thread_request
}

proc Oc_GetDefaultThreadCount {} {
   # Default thread request count.  This may be overridden by
   # application --- for example, by using a value from the command
   # line.  The default may be set in any of the following four ways,
   # listed in order of increasing priority:
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
   set otc 2  ;# Global default
   if {[info exists env(OOMMF_THREADS)]} {
      # Set from environment
      set otc $env(OOMMF_THREADS)
   } elseif {![Oc_Option Get Threads oommf_thread_count val]} {
      # Set from Oc_Option database
      set otc $val
   } elseif {![catch {[Oc_Config RunPlatform] GetValue thread_count} val]} {
      # Set from RunPlatform value
      set otc $val
   }
   set otc [Oc_EnforceThreadLimit $otc]
   if {$otc<1} {
      puts stderr "\n************************************************"
      puts stderr "ERROR: Bad setting for thread_count: $otc"
      puts stderr "   Overriding to 1"
      puts stderr "************************************************"
      set otc 1
   }
   return $otc
}

