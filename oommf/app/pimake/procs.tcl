# FILE: procs.tcl
#
# A collection of Tcl procedures used by pimake application

set oommfExtDir [file join [file dirname [file dirname [file dirname \
                [Oc_DirectPathname [info script]]]]] pkg]

proc DeleteFiles {args} {
    if {![llength $args]} {return}
    puts "Deleting $args in [pwd] ..."
    file delete -force -- {*}$args
}

proc MakeDirectory {dir} {
    puts "Making directory '[file join [pwd] $dir]' ..."
    file mkdir $dir
}

proc VerifyExecutable {stemlist} {
    foreach fn [Platform Executables $stemlist] {
        if {![file executable $fn]} {
            puts "$fn not executable.  Deleting ..."
            file delete $fn
        } else {
# puts "Hooray! $fn is executable!"
        }
    }
}

interp alias {} MakeTclIndex {} Oc_MakeTclIndex

proc Recursive {target} {
    foreach mr [glob -nocomplain [file join * makerules.tcl]] {
        if {[catch {
            if {[MakeRule SourceRulesFile $mr]} {
                MakeRule Build [file join [file dirname \
                        [Oc_DirectPathname $mr]] $target]
            }
        } msg]} {
            global errorInfo errorCode
            foreach {ei ec} [list $errorInfo $errorCode] {}
            set emsg $msg
            break
        }
    }
    if {![info exists emsg]} {
        return
    }
    set index [string last "\n    ('MakeRule' proc"  $ei]
    set ei [string trimright [string range $ei 0 $index]]
    error $emsg $ei $ec
}

# Routine to create Oxs_Exs extension initialization (*.cc) file
# The import outfile is the name of the output file; args is a
# list of files to scan for OXS_EXT_CHILD_INTERFACE_SUPPORT macros.
proc MakeOxsExtInit { outfile args } {
    puts "Creating [file join [pwd] $outfile] ..."
    set cnamelist {}
    # Scan files
    foreach file $args {
	if {[catch {open $file "r"} chan]} {
	    continue              ;# Bad filename; skip
	}
	# Read file up to '/* End includes */' or eof.
	set buf {}
	while {[gets $chan line]>=0} {
	    if {[regexp -nocase \
                    "/\\\*\[ \t\]*end\[ \t\]+includes?\[ \t\]*\\\*/" \
                    $line]} {
		# Line matched '/* End includes */'
		break
	    }
	    append buf $line
	    append buf "\n"
	}
	catch {close $chan}

	# Remove backslash+newline combinations
	regsub -all -- "\\\\\n" $buf {} buf

	# Search for OECIS macro
	set ws "\[ \t\n\]*"
	set bs "\[^ \t\b\]*"
	set key "OXS_EXT_CHILD_INTERFACE_SUPPORT"
	if {[regexp \
		"$key$ws\\($ws\($bs\)$ws\\)" \
		$buf dummy cname]} {
	    lappend cnamelist $cname
	}
    }

    # Write out extension initialization file
    if {![catch {open $outfile "w"} outchan]} {
	set declfmt {void OxsExtGet%s}
	append declfmt {RegistrationInfo(Oxs_ExtRegistrationBlock&);}
	set callfmt {  OxsExtGet%sRegistrationInfo(regblk);}
	puts $outchan {#include "ext.h"}
	puts $outchan "\n/* End includes */\n"
	foreach cname $cnamelist {
	    puts $outchan [format $declfmt $cname]
	}
	puts $outchan "\nvoid OxsInitializeExt()\n{"
	puts $outchan {  Oxs_ExtRegistrationBlock regblk;}
	foreach cname $cnamelist {
	    puts $outchan [format $callfmt $cname]
	    puts $outchan {  Oxs_Ext::Register(regblk);}
	}
	puts $outchan "}"
	catch {close $outchan}
    }
}

