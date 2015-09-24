# FILE: font.tcl
#
# Maps OOMMF-specified symbolic names to system-specific fonts
#
# Symbolic font names:
#        standard    --- Non-bold font, used by default in Tk entry widgets.
#        bold        --- Bold font, used by default in Tk labels and buttons.
#        compressed_bold --- Compressed/reduced bold font.
#        large       --- Larger than standard, non-bold.
#        large_bold  --- Large bold font.
#
# Last modified on: $Date: 2007-03-21 01:02:40 $
# Last modified by: $Author: donahue $
#
Oc_Class Oc_Font {

    private array common font

    ClassConstructor {
	if {![Oc_Main HasTk]} {
	    error "$class requires Tk"
	}
	# Set standard, non-bold font, using default from Tk's entry widget.
	set x [entry ".[$class WinBasename]"]
	set font(standard) [$x cget -font]
	destroy $x

	# Set bold font, using default from Tk's label widget.
	set x [label ".[$class WinBasename]" -text "Testing"]
	set font(bold) [$x cget -font]

	# Try to get a condensed-bold font
	global tcl_platform
	if {[string match windows $tcl_platform(platform)]} {
	    # Windows
	    if {[llength [info commands font]]} {
		# Tcl8.0 or later
		set font(compressed_bold) \
			[font create -family Arial -size 7 -weight bold]
		#	[font create -family systemfixed -weight bold]
	    } else {
		# Pre-Tcl8.0
		set bc "-Adobe-Helvetica-Bold-R-Normal--*-120-*-*-*-*-*-*"
		if {![catch {$x configure -font $bc}]} {
		    set font(compressed_bold) $bc
		} else {
		    set font(compressed_bold) $font(bold)
		}
	    }
	} else {
	    # Unix
	    set bc "-misc-fixed-bold-r-semicondensed-*-*-120-*-*-*-*-*-*"
	    set bca "-*-fixed-bold-*-*-*-*-*-*-*-*-*-*-*"
	    if {![catch {$x configure -font $bc}]} {
		# Note: This test should always succeed if Tcl is 8.0
		# or later, because of font-substitution.
		set font(compressed_bold) $bc
	    } elseif {![catch {$x configure -font $bca}]} {
		set font(compressed_bold) $bca
	    } else {
		set font(compressed_bold) $font(bold)
	    }
	}

        # Setup "large" fonts
        if {[llength [info commands font]]} {
            # Tcl8.0 or later
            array set myfont [font actual $font(standard)]
            if {![catch {set myfont(-size)} size]} {
                set myfont(-size) [expr round($size*14./12.)]
            }
            set font(large) [eval font create [array get myfont]]
            array set myfont [font actual $font(bold)]
            if {![catch {set myfont(-size)} size]} {
                set myfont(-size) [expr round($size*14./12.)]
            }
            set font(large_bold) [eval font create [array get myfont]]
        } else {
            # Pre-Tcl8.0
            set lsf "-Adobe-Helvetica-Medium-R-Normal--*-140-*-*-*-*-*-*"
            set lbf "-Adobe-Helvetica-Bold-R-Normal--*-140-*-*-*-*-*-*"
	    if {![catch {$x configure -font $lsf}]} {
                set font(large) $lsf
            } else {
                set font(large) $font(standard)
            }
	    if {![catch {$x configure -font $lbf}]} {
                set font(large_bold) $lbf
            } else {
                set font(large_bold) $font(bold)
            }
        }
	destroy $x
    }

    Constructor {args} {
        error "Instances of $class are not allowed"
    }

    proc Get { symname } {
	if {[catch {set font($symname)} result]} {
	    error "Unknown symbolic fontname: $symname"
	}
	return $result
    }
}

