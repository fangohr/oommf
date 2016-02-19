# FILE: charinfo.tcl
#
# Information about characters
#
# Last modified on: $Date: 2007/03/21 01:17:07 $
# Last modified by: $Author: donahue $

# NOTE: It appears that the SELECT callback must return an empty string
# to indicate success.

Oc_Class Ow_CharInfo {
    private array common charinfo   ;# Character info, indexed
    ## by "fontname,attribute"
    Constructor { args } {
        return -code error "Instances of $class are not allowed"
    }

    proc GetAveWidth { {font {}} } {
	# Returns the average width in pixels of a character
	# in "font", where the empty string uses the default
	# font for label widgets.

	# Check cache
	if {[info exists charinfo($font,avewidth)]} {
	    return $charinfo($font,avewidth)
	}

	# Otherwise, construct label widget 1 character wide in
	# the specified font, and use 'winfo reqwidth' to find
	# the corresponding width in pixels.
	set winname ".[$class WinBasename]-temp"
	label $winname -width 1 -borderwidth 0 -padx 0 -text ""
	if {![string match {} $font]} {
	    catch {$winname configure -font $font}
	    ## At least some versions of Tk don't raise an error
	    ## if you specify an unknown font, but instead silently
	    ## fall back to the default.
	}
	set avewidth [winfo reqwidth $winname]
	destroy $winname ;# We're done with label widget, so kill it.
	## We might want to wrap some of the above in a catch, to
	## ensure that $winname always gets destroyed.

	# Store result in cache and return
	set charinfo($font,avewidth) $avewidth
	return $avewidth
    }
}
