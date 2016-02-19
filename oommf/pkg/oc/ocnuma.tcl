# FILE: ocnuma.tcl                   -*-Mode: tcl-*-
# Tcl script portion of Oc class NUMA (multi-threaded Non-Uniform
# Memory Access) code support.
#
proc Oc_GetDefaultNumaNodes {} {
   # Default NUMA nodes request list.  This may be overridden by
   # application --- for example, by using a value from the command line.
   # The default may be set in any of the following four ways, listed
   # in order of increasing priority:
   # 
   #   Global default value is none
   # 
   #   In the oommf/config/platform/"platform.tcl" file:
   #      [Oc_Config RunPlatform] SetValue numanodes <value>
   # 
   #   In the oommf/config/options.tcl file:
   #      Oc_Option Add * Numa numanodes <value>
   # 
   #   From the shell environment variable,
   #      OOMMF_NUMANODES
   global env
   set nodes none  ;# Global default
   if {[info exists env(OOMMF_NUMANODES)]} {
      # Set from environment
      set nodes $env(OOMMF_NUMANODES)
   } elseif {![Oc_Option Get Numa numanodes val]} {
      # Set from Oc_Option database
      set nodes $val
   } elseif {![catch {[Oc_Config RunPlatform] GetValue numanodes} val]} {
      # Set from RunPlatform value
      set nodes $val
   }
   if {![regexp {^([0-9 ,]*|auto|none)$} $nodes]} {
      puts stderr "\n************************************************"
      puts stderr "ERROR: Bad setting for numanodes: \"$nodes\""
      puts stderr "   Overriding to \"none\""
      puts stderr "************************************************"
      set nodes none
   }
   return $nodes
}
