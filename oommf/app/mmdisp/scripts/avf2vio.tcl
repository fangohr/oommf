# FILE: avf2vio.tcl
#
# This file must be evaluated by mmDispSh

package require Oc 1.1.1.0
package require Mmdispcmds 1.1.1.0
Oc_ForceStderrDefaultMessage
catch {wm withdraw .}

Oc_Main SetAppName avf2vio
Oc_Main SetVersion 1.2.1.0

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
