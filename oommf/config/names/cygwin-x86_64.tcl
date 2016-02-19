# cygwin-x86_64.tcl
#
# Defines the Oc_Config name 'cygwin-x86_64' to indicate the Windows
# operating system running on an Intel architecture using 64-bit
# Cygwin Tcl/Tk.
#
# NB: The Oc_IsCygwin64Platform proc is defined in oommf/pkg/oc/procs.tcl.
Oc_Config New _ [string tolower [file rootname [file tail [info script]]]] {
   return [Oc_IsCygwin64Platform]
}
