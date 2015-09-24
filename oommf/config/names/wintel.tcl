# wintel.tcl
#
# Defines the Oc_Config name 'wintel' to indicate the Windows 95/NT operating
# system running on an Intel architecture.

Oc_Config New _ [string tolower [file rootname [file tail [info script]]]] {
   global tcl_platform
   if {![regexp -nocase -- windows $tcl_platform(platform)]} {
      return 0   ;# Not windows
   }
   if {![string match intel $tcl_platform(machine)] &&
       ![string match i?86  $tcl_platform(machine)]} {
      return 0   ;# Not x86
   }
   return [expr {![Oc_IsCygwinPlatform]}]
}
