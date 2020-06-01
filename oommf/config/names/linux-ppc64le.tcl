# linux-ppc64le.tcl
#
# Defines the Oc_Config name 'linux-ppc64le' to indicate the Linux
# operating system running on the PowerPC 64-bit little endian
# architecture.

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
   if {![string match ppc64le $tcl_platform(machine)]} {
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
   if {[info exists tcl_platform(byteOrder)] &&
       ![string match -nocase littleendian $tcl_platform(byteOrder)]} {
      return 0
   }

   return 1
}
