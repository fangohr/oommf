# linux-x86_64.tcl
#
# Defines the Oc_Config name 'linux-x86_64' to indicate the Linux
# operating system running on the x86_64 architecture.

Oc_Config New _ [string tolower [file rootname [file tail [info script]]]] {
   global tcl_platform

   if {[llength [info commands LocalNameCheck]] == 0} {
      # If local/mynames.tcl exists and defines LocalNameCheck,
      # then use that.
      set fn [file join \
                 [file dirname [file dirname [file dirname [info script]]]] \
                 config names local mynames.tcl]
      if {[file readable $fn]} {
         catch {source $fn}
      }
   }
   if {[llength [info commands LocalNameCheck]] == 1} {
      set localname [LocalNameCheck]
      if {![string match {} $localname]} {
         set checkname [$this GetValue platform_name]
         return [expr {![string compare $checkname $localname]}]
      }
   }

   # Otherwise, fall back on default rules
   if {![regexp -nocase -- linux $tcl_platform(os)]} {
      return 0
   }
   if {![string match x86_64 $tcl_platform(machine)]} {
      return 0
   }
   if {[info exists tcl_platform(wordSize)] &&
       $tcl_platform(wordSize) != 8} {
      return 0
   }
   if {[info exists tcl_platform(pointerSize)] &&
       $tcl_platform(pointerSize) != 8} {
      return 0
   }
   return 1
}
