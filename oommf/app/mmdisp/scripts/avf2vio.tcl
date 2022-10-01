# FILE: avf2vio.tcl
#
# This file must be evaluated by mmDispSh

package require Oc 2
package require Mmdispcmds 2
Oc_ForceStderrDefaultMessage
catch {wm withdraw .}

Oc_Main SetAppName avf2vio
Oc_Main SetVersion 2.0b0

Oc_CommandLine Option console {} {}
Oc_CommandLine Option [Oc_CommandLine Switch] {
	{file {} {Input vector field file}}
    } {
	global infile; set infile $file
} {End of options; next argument is file}
Oc_CommandLine Parse $argv
 
ChangeMesh [Oc_DirectPathname $infile] 0 0 0 -1
# $filename == "" --> write to stdout
WriteMeshUsingDeprecatedVIOFormat ""
exit 0
