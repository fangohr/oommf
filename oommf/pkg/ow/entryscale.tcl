# FILE: entryscale.tcl
#
# Entry box + scale dialog sub-widget
#
# Last modified on: $Date: 2010/08/23 23:36:08 $
# Last modified by: $Author: donahue $
#
#   Use the method call "ReadEntryString" to get the value
# currently displayed in the entry box.  Use "Cget -value" to
# get the last value "committed" with a <Key-Return> event.
#   The <Key-Return> event copies the value from the entry widget
# into -value.
#   Set -autoundo if you want automatic undo action (equivalent
# to the <Key-Escape> event) on <FocusOut> events.  This copies the
# last "committed" value (as stored in -value) into the entry widget
# display.
#   Use the method Set to set both -value and the displayed
# text.  Use SetEdit to set only the displayed text.
Oc_Class Ow_EntryScale {
    const public variable winpath
    const public variable outer_frame_options = "-bd 2 -relief ridge"
    private variable winsunken = {} ;# Frame holding entry and scale
    ## widgets, and also scale "marks".
    public variable winentry = {}
    public variable label = {} {
	if {[winfo exists $winpath.label]} {
            $winpath.label configure -text $label
        }
    }
    public variable labelwidth = 0
    const public variable valuewidth = 5
    public variable value = {}  ;# Last committed value
    const public variable variable = {} ;# Name of a global variable to
    ## tie to committed value.  Anytime this variable is written to,
    ## the entry and scale widgets are updated.  Conversely, if a commit
    ## occurs from the entry or scale widgets, then $variable gets
    ## updated, unless $command != {}.  Note: If one wishes to remove
    ## the 'const' attribute, then the verification script will need to
    ## to unset and reset traces on $variable.

    public variable command = {}  ;# Command prefix to run on
    ## commits.  One argument is sent: $value.  If $command == {},
    ## then no command is run, but instead $variable will be updated
    ## (if set).

    public variable valuecheck = Oc_Nop
    public variable valuetype = float {
	if {![string match custom $valuetype]} {
	    set valuecheck [Ow_GetEntryCheckRoutine $valuetype]
	    if {[string match $valuecheck Ow_IsText]} {
		Oc_Log Log "WARNING from Ow_EntryScale Configure:\
			Bad value type $valuetype;\
			using type float instead." warning
		set valuetype float
		set valuecheck [Ow_GetEntryCheckRoutine $valuetype]
	    }
        }
    }
    ## See oommf/ext/ow/procs.tcl for complete list of predefined
    ## valid types.  Currently these are text, int, posint, fixed,
    ## posfixed, float and posfloat.  Use "custom" and the valuecheck
    ## variable to register a custom routine.  See procs.tcl check
    ## routine structure.  Entryscale should be restricted to numeric
    ## types, so in particular "text" is disallowed.

    const public variable valuejustify = right

    private variable testvalue = {} ;# Used to enforce write restrictions
    private variable oldvalue = {}  ;# (see valuetype and valuecheck).

    private variable scalevalue = {} ;# "Discretized" value stored
    ## in scale widget.
    private variable scalevaluefmt = "%.8g"

    # Default colors are setup in constructor to take the
    # associated values from the standard Tk button widget.
    const private variable default_foreground
    const private variable default_disabledforeground

    const public variable foreground = {} {
	if {[info exists default_foreground]} {
	    # If default_foreground isn't setup, then we
	    # must be inside Constructor.
	    if {[string match {} $foreground]} {
		set foreground $default_foreground   ;# Use default
	    }
	    # Update foreground colors in "normal state" widgets
	    if {[string match normal $state]} {
		$this Configure -state normal
	    }
	}
    }

    const public variable disabledforeground = {} {
	if {[info exists default_disabledforeground]} {
	    # If default_disabledforeground isn't setup, then we
	    # must be inside Constructor.
	    if {[string match {} $disabledforeground]} {
		# Use default
		set disabledforeground $default_disabledforeground
	    }
	    # Update colors in "disabled state" widgets
	    if {[string match disabled $state]} {
		$this Configure -state disabled
	    }
	}
    }

    const public variable coloredits = 1
    const public variable activeeditcolor = red
    const public variable commiteditcolor
    const public variable commiteditsfcolor
    # Set coloredits 1 to color uncommitted edits

    public variable state = normal {
	# Adjust state of inner label widget
	if {[winfo exists $winpath.label]} {
	    # Starting with Tcl/Tk 8.3.2, the label
	    # widget supports the "-state" configuration
	    # option.  But for backwards compatibility
	    # we'll just manually adjust the text color.
            if {[string match disabled $state]} {
                $winpath.label configure -foreground $disabledforeground
            } else {
                $winpath.label configure -foreground $foreground
            }
	}
        # Adjust state of inner entry widget
        if {![string match {} $winentry]} {
            $winentry configure -state $state
            if {![string match disabled $state]} {
                $winentry configure -foreground $foreground
            } else {
                $winentry configure -foreground $disabledforeground
            }
        }
        # Adjust state of inner scale widget
        if {![string match {} $winscale]} {
            $winscale configure -state $state
            if {![string match disabled $state]} {
                $winscale configure -foreground $foreground
            } else {
                $winscale configure -foreground $disabledforeground
            }
        }
    }

    private variable scaleautocommitid = {}
    public variable scaleautocommitechotime = 1000
    public variable scaleautocommit = {} ;# If != {}, then between
    ## a button down and a button release event on the slider, the
    ## current slider value is automatically commited every
    ## $scaleautocommit milliseconds.  If $scaleautocommit is smaller
    ## than $scaleautocommitechotime, then scale edit traces are
    ## disabled.

    const public variable autoundo = 0 ;# If true, then widget performs
    ## an undo action on FocusOut events

    const public variable commitontab = 1 ;# If set, then
    ## tabbing out of the widget commits the value

    const public variable uselabelfont = 1 ;# If set, then the font used
    ## in the label widget is also used in the entry widget.  Otherwise,
    ## the default font is used in the entry widget.

    # Scale widget variables
    public variable winscale = {}

    # logscale==1  -->  logarithmic scaling in the scale widget
    const public variable logscale = 0 {
	if {$logscale!=0} {
	    set logscale 1
	    if {$rangemin<=0.0} {
		set rangemin [expr pow(10,$rangemin)]
		set rangemax [expr pow(10,$rangemax)]
	    }
	    set valuetype "posfloat"
	    $this AdjustScaleResolution $scalestep
	}
	## If logscale is toggled to 0, should valuetype be
	## reset?
	$this UpdateMarkList
    }

    # Working range.  See also the AdjustRange method.
    public variable rangemin = 0 {
	if {$logscale} {
	    if {[catch {expr log10($rangemin)} scalemin]} {
		set rangemin 0.1
		set scalemin [expr log10($rangemin)]
	    }
	} else {
	    set scalemin $rangemin
	}
        $this WidgetConfigure $winscale -from $scalemin
	if {$logscale} { $this AdjustScaleResolution $scalestep }
	$this UpdateMarkList
    }
    public variable rangemax = 1 {
	if {$logscale} {
	    if {[catch {expr log10($rangemax)} scalemax]} {
		set rangemax 10.0
		set scalemax [expr log10($rangemax)]
	    }
	} else {
	    set scalemax $rangemax
	}
        $this WidgetConfigure $winscale -to $scalemax
	if {$logscale} {$this AdjustScaleResolution $scalestep}
	$this UpdateMarkList
    }

    public variable autorange = 0
    ## If autorange is true, then whenever a value is committed
    ## that is outside (or nearly outside) the slider range, the
    ## slider range is reset.

    # Hard limits.  If not {}, then no value outside the
    # specified range will be accepted.
    public variable hardmax = {}
    public variable hardmin = {}

    # Scalestep is the slider stepsize, in "scale" units.
    # For linear scales, the number of steps is
    #           (rangemax-rangemin)/stepsize
    # For log scales, the number of steps is
    #           (log10(rangemax)-log10(rangemin))/stepsize
    public variable scalestep = 0.1 {
	$this AdjustScaleResolution $scalestep
    }

    public variable displayfmt = "%g"
    ## Used when reading values off the slider

    # If relsliderwidth is != {}, then it overrides sliderwidth
    # each time the widget is resized.
    public variable sliderwidth = {} {
	$this WidgetConfigure $winscale -sliderlength $sliderwidth
	$this UpdateMarkList
    }
    private variable old_relsliderwidth = {}
    public variable relsliderwidth = {} {
	if {$old_relsliderwidth != $relsliderwidth} {
	    $this SetRelativeSliderWidth
	    $this UpdateMarkList
	    set old_relsliderwidth $relsliderwidth
	}
    }

    public variable scalewidth = 75 {
        $this WidgetConfigure $winscale -length $scalewidth
    }
    public variable troughcolor = {} {
        if {![string match {} $troughcolor]} {
            $this WidgetConfigure $winscale -troughcolor $troughcolor
        }
    }

    # NOTE: The validation script for marklist disables
    #  action if there is no change in the marklist.  To
    #  force redraw of the marks, call $this UpdateMarkList
    #  directly.
    private variable old_marklist = {}
    public variable marklist = {} {
	set listchanged 1
	if {[llength $old_marklist] == [llength $marklist]} {
	    set listchanged 0
	    foreach x $old_marklist y $marklist {
		if {$x != $y} {
		    set listchanged 1
		    break
		}
	    }
	}
	if {$listchanged} {
	    $this UpdateMarkList
            set old_marklist $marklist
	}
    }
    public variable markcolor = "black" {
	$this UpdateMarkList
    }
    public variable markwidth = 1 {
	$this UpdateMarkList
    }

    Constructor {basewinpath args} {
	# If $basewinpath doesn't exist, then we use that name
	# for the outer frame.  Otherwise, we create a hopefully
	# unique name based on $this, as a child of $basewinpath.
	# Don't add '.' suffix if there already is one, which
	# happens when basewinpath is exactly '.'
	if {![winfo exists $basewinpath]} {
	    set winpath $basewinpath
	} else {
	    if {![string match {.} \
		    [string index $basewinpath \
		    [expr [string length $basewinpath]-1]]]} {
		append basewinpath {.}
	    }
	    set winpath ${basewinpath}[$this WinBasename]
	}

	# Initialize valuecheck
	set valuecheck [Ow_GetEntryCheckRoutine $valuetype]

	# Process user preferences
	eval $this Configure $args

        # Check for tied variable, and if it exists use it to
        # override any -value setting
        if {![string match {} variable]} {
            upvar #0 $variable tievariable
            if {[info exists tievariable]} {
                set value $tievariable
            } else {
		set tievariable $value  ;# Safety initialization
            }
        }

        # Validate input
        if {[Ow_IsFloat $value]!=0} {
	    set value 0     ;# Input not a number
        }
	if {$logscale && $value<=0.0} {
	    set value 1.0   ;# Must be strictly positive for logscale
	}
	if {![string match custom $valuetype]} {
	    set valuecheck [Ow_GetEntryCheckRoutine $valuetype]
	    if {[string match $valuecheck Ow_IsText]} {
		if {$logscale} {
		    set temptype "posfloat"
		} else {
		    set temptype "float"
		}
		Oc_Log Log "WARNING from Ow_EntryScale Configure:\
			Bad value type $valuetype;\
			using type $temptype instead." warning
		set valuetype $temptype
		set valuecheck [Ow_GetEntryCheckRoutine $valuetype]
	    }
        }

	# Pack entire subwidget into a subframe.  This makes
	# housekeeping easier, since the only bindings on
	# $winpath will be ones added from inside the class,
	# and also all subwindows can be destroyed by simply
	# destroying $winpath.
	eval frame $winpath $outer_frame_options

	# Setup default colors, using a temporary button widget
	set btn [button $winpath.throwawaybutton]
	set default_foreground [$btn cget -foreground]
	set default_disabledforeground [$btn cget -disabledforeground]
	destroy $btn

	# If foreground and disabledforeground weren't setup by
	# user, then use default colors.
	if {[string match {} $foreground]} {
	    set foreground $default_foreground
	}
	if {[string match {} $disabledforeground]} {
	    set disabledforeground $default_disabledforeground
	}

	# Create label subwidget
	label $winpath.label -text $label
	if {![string match {} $labelwidth]} {
	    if {$labelwidth>=0} {
		$winpath.label configure -width $labelwidth -anchor e
	    } else {
		# No label; remove extra space
		$winpath.label configure \
			-width 0 -padx 0 -pady 0 -borderwidth 0
	    }
	}
	if {[string match disabled $state]} {
	    $winpath.label configure -foreground $disabledforeground
	} else {
	    $winpath.label configure -foreground $foreground
	}
	pack $winpath.label -side left -anchor e

	# Put entry and scale widgets together inside a sunken frame
	set winsunken [frame $winpath.sunken -bd 2 -relief sunken]

        # Entry widget
	set winentry [entry $winsunken.value -relief sunken -bd 0 \
		-width $valuewidth -bg [$winpath.label cget -bg] \
                -state $state -justify $valuejustify]
	## The default color scheme under Tcl 8.0 + Windows makes
	## labels a light gray, and entry backgrounds bright white.
	## *I* think it looks pretty hideous...  -mjd 97/10/3
	set commiteditcolor [$winentry cget -foreground]
	set commiteditsfcolor [$winentry cget -selectforeground]
        if {[string match {} $commiteditsfcolor]} {
           set commiteditsfcolor $commiteditcolor
        }

	# Set up edit tracing
        $winentry configure -textvariable [$this GlobalName testvalue]
        trace variable testvalue w "$this TraceEdits"

        # Scale widget
	if {$rangemin>$rangemax} {
	    set temp $rangemin
	    set rangemin $rangemax
	    set rangemax $temp
	}
	if {$logscale} {
	    set tempmin [expr log10($rangemin)]
	    set tempmax [expr log10($rangemax)]
	} else {
	    set tempmin $rangemin
	    set tempmax $rangemax
	}
        set winscale [scale $winsunken.scale \
                -orient horizontal -showvalue 0 \
                -length $scalewidth -from $tempmin -to $tempmax \
                -bigincrement 0 -bd 0 -resolution 0 -digits 13 \
                -command "$this ScaleCommand"]
	$this AdjustScaleResolution $scalestep
        ## Note: Setting "-variable [$this GlobalName testvalue]" in
        ##   the scale widget appears to disable the TraceEdits binding
        ##   on testvalue.
	if {![string match {} $troughcolor]} {
	    $winscale configure -troughcolor $troughcolor
	} else {
	    # If troughcolor is not specified, then adjust
	    # defaults if necessary so that the slider trough
	    # color is different from entry background
	    set ebcolor [winfo rgb $winentry [$winentry cget -background]]
	    set stcolor [winfo rgb $winscale [$winscale cget -troughcolor]]
	    if {[lindex $ebcolor 0] == [lindex $stcolor 0] \
		    && [lindex $ebcolor 1] == [lindex $stcolor 1] \
		    && [lindex $ebcolor 2] == [lindex $stcolor 2] } {
		set r [expr {round([lindex $stcolor 0]*0.9)}]
		set g [expr {round([lindex $stcolor 1]*0.9)}]
		set b [expr {round([lindex $stcolor 2]*0.9)}]
		set newcolor [format "#%04X%04X%04X" $r $g $b]
		# If stcolor is close to black, then newcolor
		# may be the same as stcolor, but it's probably
		# just not worth the bother to handle that case.
		$winscale configure -troughcolor $newcolor
	    }
	}
	if {[string match {} $sliderwidth]} {
	    set sliderwidth [$winscale cget -sliderlength]
	} else {
	    $winscale configure -sliderlength $sliderwidth
	}
	$this UpdateMarkList

        if {![string match disabled $state]} {
            $winentry configure -foreground $foreground
        } else {
            $winentry configure -foreground $disabledforeground
        }

        # Pack
	pack $winentry -side left
        pack $winscale -side right -fill x -expand 1
	pack $winsunken -fill x
	$this Set $value

        # Set trace to tied variable, if any (upvar from $variable
        # to tievariable done above).
        if {![string match {} variable]} {
            if {![info exists tievariable] || $tievariable!=$value} {
                set tievariable $value
            }
            trace variable tievariable w "$this TraceValue $variable"
        }

	# Setup fonts
	if {$uselabelfont} {
	    $winentry configure -font [$winpath.label cget -font]
	}

	# Widget bindings
	bind $winentry <Key-Return> "$this CommitValue"
        bind $winscale <Key-Return> "$this CommitValue"
	if {$commitontab} {
	    bind $winentry <Key-Tab> "$this CommitValue"
	    bind $winscale <Key-Tab> "$this CommitValue"
	    Oc_SetShiftTabBinding $winentry "$this CommitValue"
	    Oc_SetShiftTabBinding $winscale "$this CommitValue"
	}
	bind $winentry <Key-Escape> "$this Undo"
	bind $winscale <Key-Escape> "$this Undo"
	if {$autoundo} {
	    bind $winentry <FocusOut> [bind $winentry <Key-Escape>]
	    bind $winscale <FocusOut> [bind $winentry <Key-Escape>]
	}
        bind $winscale <ButtonPress> \
		"$this ScaleForce edit ; $this ScaleAutoCommit start"
        bind $winscale <ButtonRelease> \
		"$this ScaleAutoCommit stop ; $this ScaleForce commit"
	bind $winscale <Configure> "$this WindowConfigureCallback"
	bind $winpath <Destroy> "$this WinDestroy %W"

	# Make precise scale range setting
	$this AdjustRange $rangemin $rangemax

	# Commit initial value
	$this CommitValue
    }
    private method WidgetConfigure { widget optname optvalue } {
        if {![string match {} $widget]} {
            $widget configure $optname $optvalue
        }
    }
    callback method WindowConfigureCallback {} {
	$this SetRelativeSliderWidth
	$this UpdateMarkList
    }
    private method UpdateMarkList {} {
	if {[string match {} $winscale]} { return }
	# Delete any excess mark windows
	set markwindows {}
        foreach w [winfo children $winsunken] {
            if {[string match $winsunken.mark* $w]} {
                lappend markwindows $w
            }
        }
	if {[llength $markwindows] > [llength $marklist]} {
	    # Remove extra windows from end.  We have to disable
	    # <Configure> bindings on $winscale because otherwise Tk
	    # jumps directly to them from inside the destroy call
	    # (and this routine gets recursively called with the
	    # "mark" child windows in a half-destroyed state).
	    set destroy_index [llength $marklist]
	    set confscript [bind $winscale <Configure>]
	    bind $winscale <Configure> {}
	    eval destroy [lrange $markwindows $destroy_index end]
	    bind $winscale <Configure> $confscript
	    set $markwindows [lreplace $markwindows $destroy_index end]
	}

	if {[string match {} $marklist]} { return }

	# Create new mark frames as needed so that number of mark
	# frames equals the number of marks.
	for {set id [llength $markwindows]} {$id<[llength $marklist]} \
            {incr id} {
                frame $winsunken.mark$id -width $markwidth \
                    -background $markcolor -borderwidth 0 -relief flat
                # Marks are created as children of $winsunken as opposed
                # to $winscale as a sop to mmDisp on Windows, which
                # sets the watch cursor on every leaf in the window
                # widget hierarchy.  This way, $winscale remains a leaf.
        }

	# Add one mark for each element of mark list
	if {$logscale} {
	    set scalemin [expr log10($rangemin)]
	    set scalemax [expr log10($rangemax)]
	    set slist {}
	    foreach mark $marklist {
		if {$mark>0.0} {
		    lappend slist [expr log10($mark)]
		}
	    }
	} else {
	    set scalemin $rangemin
	    set scalemax $rangemax
	    set slist $marklist
	}
	if {$markwidth==1} {
	    set anchor ne
	} else {
	    set anchor n
	}
	set id 0
	foreach mark $slist {
	    if {$mark<$scalemin || $mark>$scalemax} {
		place forget $winsunken.mark$id
	    } else {
		set xpos [lindex [$winscale coords $mark] 0]
                set xpos [expr {$xpos-1}] ;# Empirical adjustment
		place $winsunken.mark$id -in $winscale \
			-x $xpos -y 0 -relheight 1 -anchor $anchor
	    }
	    incr id
	}
    }
    callback method TraceEdits { args } {
	# Ignore imports
	if {[string match disabled $state]} {
	    return  ;# Edits disabled
	}
	set checkresult [eval $valuecheck {$testvalue}]
	if {$checkresult>1} {
	    # Bad value; change back to old value
	    set testvalue $oldvalue
	} else {
	    # Accept new value
	    set oldvalue $testvalue
	    if {$coloredits && ($testvalue != $value)} {
		$winentry configure -foreground $activeeditcolor \
			-selectforeground $activeeditcolor
	    }
            # If complete number, copy over to scale widget.
            if {$checkresult==0} {
		if {$logscale} {
		    if {$testvalue>0} {
			set scalevalue [expr {log10($testvalue)}]
		    }
		} else {
		    set scalevalue $testvalue
		}
                $winscale set $scalevalue
		set scalevalue [format $scalevaluefmt [$winscale get]]
            }
	}
    }
    private method GetGoodStep { min max step } {
        # Attempts to adjust step so that it evenly divides
        # both min and max.  Return value is a good guess.
        set step [expr double($step)]
        if { $step<=0. || ($min==0. && $max==0.)} { return 0. }
        set a [expr double(abs($min))]
        set b [expr double(abs($max))]
        if {$a>$b} { set t $a ; set a $b ; set b $t }
        if {$a==0.0} { set a $b }
	set result {}
        set x [expr $a/$b]
        for {set level 1} {$level<10} {incr level} {
            # Seems there should be a better way to do this...
            foreach { c d } [Ow_RatApprox $x $level] { break }
            if { $c>0 && $d>0 } {
                set temp [expr ($a/double($c)+$b/double($d))/2.0]
                if {$temp>=$step} {
		    if {![catch {expr $temp/round($temp/$step)} ans]} {
			## Skip on overflow
			set result $ans
		    }
                } else {
		    # temp too small, and will only get smaller
		    if {[string match {} $result] || \
			    abs($result-$step)>abs($step-$temp)} {
			set result $temp
		    }
		    break
		}
            }
        }
	if {[string match {} $result]} {
	    set result $step  ;# Punt
	}

        set result [format "%.8g" $result] ;# There is a bug in the
        ## Tk scale widget that causes "quivering" in slider positions
	## during mouse motion in some circumstances.  Rounding as
	## above helps reduce this effect.  A patch for this problem
	## has been submitted by DGP, to appear in Tcl 8.3.

        return $result
    }
    private method AdjustScaleResolution { newstep } {
	if {[string match {} $winscale]} { return }
	if {$logscale} {
            set scalemin [expr log10($rangemin)]
            set scalemax [expr log10($rangemax)]
            set tempstep [$this GetGoodStep $scalemin $scalemax $newstep]
	} else {
            set tempstep [$this GetGoodStep $rangemin $rangemax $newstep]
	}
        if {![string match {} $scalevalue] && $tempstep!=0} {
	    set scalevalue \
		    [expr round($scalevalue/double($tempstep))*$tempstep]
	}


	# Note: The scale -from and -to values are limited by the
	# resolution, so if we change the resolution we should
	# also reset -from and -to.  Ditto the scale value.
	if {$logscale} {
	    set scalemin   [expr log10($rangemin)]
	    set scalemax   [expr log10($rangemax)]
	    set scalevalue [expr log10($value)]
	} else {
	    set scalemin $rangemin
	    set scalemax $rangemax
	    set scalevalue $value
	}
        $winscale configure -resolution $tempstep \
		-from $scalemin -to $scalemax
	$winscale set $scalevalue
    }
    public method AdjustRange { newmin newmax } {
	# Sets rangemin and rangemax simultaneously, in a high
	# precision manner (i.e., works around scale resolution
	# difficulties).
	if {$newmin==$rangemin && $newmax==$rangemax} {
	    return   ;# No changes
	}
	if {$logscale} {
	    if {[catch {expr log10($newmin)} scalemin] || \
		    [catch {expr log10($newmax)} scalemax]} {
		set newmin 1
		set newmax 10
		set scalemin [expr log10($newmin)]
		set scalemax [expr log10($newmax)]
	    }
	    set scalevalue [expr log10($value)]
	} else {
	    set scalemin $newmin
	    set scalemax $newmax
	    set scalevalue $value
	}
	set rangemin $newmin
	set rangemax $newmax
	if {![string match {} $winscale]} {
	    $winscale configure -resolution 0
	    $winscale configure -from $scalemin -to $scalemax
	    $this AdjustScaleResolution $scalestep
	    $this UpdateMarkList
	    $winscale set $scalevalue
	    set scalevalue [format $scalevaluefmt [$winscale get]]
	}
    }
    private method SetRelativeSliderWidth {} {
	# Adjusts size of slider to fill up $proportion of
	# the active region of the scale.  Note that as the
	# slider grows, the active region shrinks.
	if {[string match {} $winscale]} { return }
	if {[string match {} $relsliderwidth]} {return}
	set totalsize [expr [winfo width $winscale] \
                         -2*[$winscale cget -borderwidth]]
	set size 2 ;# To make sure slider is visible, limit minimum
	if {$relsliderwidth>0} {              ;## sliderlength to 2.
	    set size [expr \
		    $relsliderwidth*$totalsize/(1+$relsliderwidth)]
	    if {$size<2} {set size 2}
	}
	$winscale configure -sliderlength $size
    }
    callback method TraceValue { globalname args } {
        # The trace is set to pass in the global variable name so that
        # even if the trace is triggered by a call to Tcl_SetVar
        # in C code called from a proc from which the traced variable
        # is not visible (due to no 'global' statement on that variable),
        # this trace can still function.
        upvar #0 $globalname shadow
        set keep_state $state
        $this Configure -state normal
        $this Set $shadow
        $this CommitValue 0
	$this Configure -state $keep_state
    }
    method ReadEntryString {} { winentry } {
	return [$winentry get]
    }
    method CommitValue { {do_callbacks 1} } {
	if {[string match disabled $state]} {
	    return  ;# Edits disabled
	}
	set text [$winentry get]
        if {[Ow_IsFloat $text]!=0 || [eval $valuecheck {$text}]!=0} {
            # Invalid value (not a number)
            $this Set $value
            return
        }
	# Check hard limits
	if {![string match {} $hardmin] && $text<$hardmin} {
	    set text $hardmin
	}
	if {![string match {} $hardmax] && $text>$hardmax} {
	    set text $hardmax
	}
        set value $text
	if {$coloredits} {
	    $winentry configure -foreground $commiteditcolor \
		    -selectforeground $commiteditsfcolor
	}
        if {$do_callbacks} {
            if {![string match {} $command]} {
                eval $command {$value}
		# Note 1: The preceding callback may delete $this,
		#  in which case "variable" will cease to exist.
		# Note 2: The preceding callback may cause this
		#  method to be recursively called.  Do we care?
		if {[info exists variable] && \
			![string match {} $variable]} {
		    upvar #0 $variable shadow
		    $this Set $shadow
		}
            } elseif { ![string match {} $variable] } {
                upvar #0 $variable shadow
                # Note: 'global $variable' won't work if $variable
                #   is an element in an array.
                set shadow $value
            }
        }
	# Reset slider range?
	if {$autorange} {
	    # Reset if slider is at scale extreme.
	    if {$logscale} {
		if {[catch {expr log10($value)} workvalue] || \
			[catch {expr {log10($rangemin)}} scalemin] || \
			[catch {expr {log10($rangemax)}} scalemax]} {
		    # Fallback
 		    set workvalue [format $scalevaluefmt [$winscale get]]
		    set scalemin [$winscale cget -from]
		    set scalemax [$winscale cget -to]
		}
	    } else {
		set workvalue $value
		set scalemin $rangemin
		set scalemax $rangemax
	    }
            set tempmin [expr $scalemin+[$winscale cget -resolution]*0.5]
            set tempmax [expr $scalemax-[$winscale cget -resolution]*0.5]
	    if { $workvalue<$tempmin || $workvalue>$tempmax } {
                # Allow for a little round off error
		set temp2min [expr  $scalemin*1.001-$scalemax*0.001]
		if {$temp2min<$workvalue && $workvalue<$tempmin} {
		    set workvalue $scalemin
		}
		set temp2max [expr -$scalemin*0.001+$scalemax*1.001]
		if {$tempmax<$workvalue && $workvalue<$temp2max} {
		    set workvalue $scalemax
		}
		set offset [expr {$workvalue-($scalemin+$scalemax)/2.0}]
		set scalediff [expr {$scalemax-$scalemin}]
		set scalemin [expr {$scalemin+$offset}]
		set scalemax [expr {$scalemax+$offset}]
		if {$logscale} {
		    if {![string match {} $hardmin]} {
			set logmin [expr {log10($hardmin)}]
			if {$scalemin<$logmin} {
			    set scalemin $logmin
			    set scalemax [expr {$scalemin+$scalediff}]
			}
		    }
		    if {![string match {} $hardmax]} {
			set logmax [expr {log10($hardmax)}]
			if {$scalemax>$logmax} {
			    set scalemax $logmax
			    set scalemin [expr {$scalemax-$scalediff}]
			}
		    }
                    set rvalue [expr pow(10,$workvalue)]
		    set rmin [expr {pow(10,$scalemin)}]
		    set rmax [expr {pow(10,$scalemax)}]
		} else {
		    # Try to respect scalestep
		    if {$scalestep>0} {
			set rmin [expr round($scalemin/$scalestep)*$scalestep]
			set rmax [expr $rmin + $scalemax - $scalemin]
			if {$workvalue<$rmin || $workvalue>$rmax} {
			    # No can do!
			    set rmin $scalemin
			    set rmax $scalemax
			}
		    } else {
			set rmin $scalemin
			set rmax $scalemax
		    }
                    set rvalue $workvalue
                }
		# Check that proposed limits are valid values
		if {!$logscale && $rmin<0 && [eval $valuecheck {$rmin}]!=0} {
		    # Scale is probably restricted to positive values
                    set rmax [expr $rmax - $rmin]
                    set rmin 0
		}
		if {$rvalue<$rmin} {
		    set rvalue $rmin
		}
		if {[eval $valuecheck $rmin]==0 && \
			[eval $valuecheck $rmax]==0 && \
			[eval $valuecheck $rvalue]==0} {
		    # Change limits
                    if {$logscale} {
                        set scalevalue [expr log10($rvalue)]
                    } else {
                        set scalevalue $rvalue
                    }
		    $this AdjustRange $rmin $rmax
		}
	    }
	}
    }
    callback method ScaleAutoCommit { cmd } {
	# Call with $cmd == start to start timer,
	#                == run   to commit current value & reschedule
	#                == stop  to stop
	if {![string match {} $scaleautocommitid]} {
	    after cancel $scaleautocommitid
	    set scaleautocommitid {}
	}
	if {[string match "run" $cmd]} {
	    $this ScaleForce commit
	}
	if {![string match "stop" $cmd] && \
		![string match {} $scaleautocommit]} {
	    set scaleautocommitid \
		    [after $scaleautocommit "$this ScaleAutoCommit run"]
	}
    }
    method ScaleForce { cmd } {
	# Force writing of scale widget value
	set scalevalue [format $scalevaluefmt [$winscale get]]
	if {$logscale} {
	    set testvalue \
		    [format $displayfmt [expr pow(10,$scalevalue)]]
	} else {
	    set testvalue [format $displayfmt $scalevalue]
	}
	if {[string match "commit" $cmd]} {
	    $this CommitValue
	}
    }
    method ScaleCommand { newvalue } {
	set newvalue [format $scalevaluefmt $newvalue]
	if {$newvalue == [format $scalevaluefmt $scalevalue]} {
	    return
	}
	## If the above is true, then presumably the scale update
	## was caused by the call to '$winscale set' inside
	## callback method TraceEdits (or some other internal method),
	## in which case we don't need (or want) the discretized scale
	## value written back into the entry box.

	if {![string match {} $scaleautocommitid] && \
		![string match {} $scaleautocommit] && \
		$scaleautocommit<$scaleautocommitechotime} {
	    # Don't echo scale value into entry box if a short
	    # scaleautocommit event is pending
	    return
	}

	if {$logscale} {
	    set newvalue [expr pow(10,$newvalue)]
	}
	set newvalue [format $displayfmt $newvalue]
	set workmin [expr $rangemin + abs($rangemin)*1e-5]
	set workmax [expr $rangemax - abs($rangemax)*1e-5]
	if {($newvalue < $workmin && $testvalue<$rangemin) || \
		($newvalue > $workmax && $testvalue>$rangemax)} {
	    return
	}
	## Don't inforce scale range limits on entry widget.

	set testvalue $newvalue
    }
    method SetEdit { newvalue } {
	if {[Ow_IsFloat $newvalue]!=0} {
	    return  ;# Input not a number
	    ## See also notes on corresponding stanza in
	    ## method Set.
	}
	if {[string compare $newvalue [$winentry get]]!=0} {
	    $winentry delete 0 end
	    $winentry insert 0 $newvalue
	}
        if {$testvalue!=$newvalue} { set testvalue $newvalue }
    }
    method Set { newvalue } {
	if {[string match disabled $state]} {
	    return  ;# Edits disabled
	}
	if {[Ow_IsFloat $newvalue]!=0} {
	    return  ;# Input not a number
	}
	if {[string compare $newvalue [$winentry get]]!=0} {
	    $winentry delete 0 end
	    $winentry insert 0 $newvalue
	}
        if {$value!=$newvalue} { set value $newvalue }
        if {$coloredits} {
            $winentry configure -foreground $commiteditcolor \
                    -selectforeground $commiteditsfcolor
        }
    }

    method Undo {} { value } {
	$this Set $value
    }
    method WinDestroy { w } { winpath } {
	if {[string compare $winpath $w]!=0} {
	    return    ;# Child destroy event
	}
	$this Delete
    }

    # Methods providing Tk-style naming convention interfaces
    # to Ow_EntryBox methods.
    method cget { option } {} { $this Cget $option }
    method configure { args } {} { eval $this Configure $args }

    Destructor {
        # Delete variable traces.
        if {[catch {
            if {![string match {} variable]} {
                upvar #0 $variable tievariable
                trace vdelete tievariable w "$this TraceValue $variable"
            }
        } errmsg]} {
            Oc_Log Log $errmsg error
        }
        if {[catch {
	    trace vdelete testvalue w "$this TraceEdits"
        } errmsg]} {
            Oc_Log Log $errmsg error
        }

        # Destroy windows
        if {[catch {
            if {[winfo exists $winpath]} {
                # Remove <Destroy> binding
                bind $winpath <Destroy> {}
                # Destroy instance frame, all children, and bindings
                destroy $winpath
            }
        } errmsg]} {
            Oc_Log Log $errmsg error
        }
    }
}
