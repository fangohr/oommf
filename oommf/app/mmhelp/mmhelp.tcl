#!/bin/sh
# FILE: mmhelp.tcl
#
# This Tcl script is the HTML browser of the OOMMF project.
# It must be evaluted by the Tcl/Tk interpreter wish
# (or whatever other name it is known by on your platform).
#
# On Windows platforms, when Tcl/Tk was installed, an association was
# placed in the registry to recognize the file extension .tcl of this
# file and use the installed wish program to evaluate it.
#
# On Unix platforms, it may be necessary to edit the "exec wish" line
# below to replace "wish" with the name of a Tcl/Tk interpreter shell
# installed on your computer.  Make the edit in place without changing
# the lines just before or after, and without changing the number of
# lines in this file.  See the documentation for a full explanation.
#
#    v--Edit here if necessary \
exec wish "$0" ${1+"$@"}
#
# Last modified on: $Date: 2015/03/25 16:43:31 $
# Last modified by: $Author: dgp $
#
# Usage: omfsh mmhelp.tcl <HomeURL>
#
# mmHelp permits browsing of pages containing help documentation.  Those
# pages are to be written with a subset of Strict HTML Level 1 markup --
# the tags <DIR> and <MENU> are not recognized -- plus selected other
# HTML elements.
# By default, the browser is forgiving in its display, and makes an attempt
# to display something reasonable even when unsupported elements are
# encountered.  If the option "Strict" is selected, however, then any
# unsupported elements cause an error to be reported.
# Pages written for this browser should try to stick to supported elements.
#
# In the current version, pages can only be retrieved via
# "file://localhost/..." URLs.
#
# TODO:
#       An OO redesign
#       Add ability to handle binary files
#       More and better error messages
#       Export many of these functions into suitable classes in an extension.
#       Help files
#       Print option
#       History list navigation control
#       split out retrieval and rendering libraries
#       Module/package management
#       Configuration via Xresources, etc.
#       improve performance
#       Internal link to top of page, not with "see"
#       Add more image types
#       Carry over format tags with closers inferred
#       Generalize by merging similar procs, perhaps toward SGML renderer?
#
########################################################################

########################################################################
# This script is meant to be portable to many platforms and many
# versions of Tcl/Tk.  Reporting of errors in startup scripts wasn't
# reliable on all platforms until Tcl version 8.0, so this script
# catches it's own errors and reports them to support older Tcl
# releases.
#
# The following [catch] command catches any errors which arise in the
# main body of this script.
#
set code [catch {

if {[catch {package require Tcl 8.5-}]} {
	return -code error "OOMMF software requires Tcl 8.5 or\
		higher.\n\tYour version: Tcl [info patchlevel]\nPlease\
		upgrade."
}

# When this script is evaluated by a shell which doesn't include the
# Oc extension, we must find and load the Oc extension on our own.
if {[string match "" [package provide Oc]]} {

# Robust determination of the absolute, direct pathname of this script:
set cwd [pwd]
set myAbsoluteName [file join $cwd [info script]]
if {[catch {cd [file dirname $myAbsoluteName]} msg]} {
    # This is probably impossible.  Can we read a script if we can't
    # cd to the directory which contains it?
    return -code error "Can't determine direct pathname for\
	    $myAbsoluteName:\n\t$msg"
}
set myAbsoluteName [file join [pwd] [file tail $myAbsoluteName]]
cd $cwd

# Create search path for OOMMF runtime library
set oommf(libPath) {}
if {[info exists env(OOMMF_ROOT)] \
        && [string match absolute [file pathtype $env(OOMMF_ROOT)]] \
        && [file isdirectory [file join $env(OOMMF_ROOT) app]]} {
    lappend oommf(libPath) $env(OOMMF_ROOT)
}
set _ [file join [file dirname [file dirname $myAbsoluteName]] lib oommf]
if {[file isdirectory [file join $_ app]]} {
    lappend oommf(libPath) $_
}
set _ [file dirname [file dirname [file dirname $myAbsoluteName]]]
if {[file isdirectory [file join $_ app]]} {
    lappend oommf(libPath) $_
}
if {![llength $oommf(libPath)]} {
    return -code error "Unable to find your OOMMF installation.\n\nSet the\
            environment variable OOMMF_ROOT to its root directory."
}

# Construct search path for extensions
foreach lib $oommf(libPath) {
    set dir [file join $lib pkg]
    if {[file isdirectory $dir] && [file readable $dir] \
            && [string match absolute [file pathtype $dir]]} {
        if {[lsearch -exact $auto_path $dir] < 0} {
            lappend auto_path $dir
        }
    }
}

# Locate a version of the Oc (OOMMF core) extension and set it up for loading.
if {[catch {package require Oc} msg]} {
    # When can 'package require Oc' raise an error?
    #	1. There's no Oc extension installed.
    #	2. An error occurred in the initialization of the Oc extension.
    #	3. Some other extension has a broken pkgIndex.tcl file, and
    #	   we are running a pre-8.0 version of Tcl, which didn't
    #	   control such problems.
    #
    # In case 3 only, it makes sense to try again.  Why?  Because the
    # pkgIndex.tcl file of the Oc extension may have been sourced before
    # the error.  Then on the second try, the [package unknown] step of
    # [package require] will be bypassed, so the error won't occur again.
    #
    # Otherwise, we should just report the error.
    if {[llength [package versions Oc]]
	    && [string match "" [package provide Oc]]} {
	package require Oc
    } else {
	error $msg $errorInfo $errorCode
    }
}
# Cleanup globals created by boilerplate code
foreach varName {cwd myAbsoluteName msg _ lib dir} {
    catch {unset $varName}
}
unset varName

# End of code for finding/loading the Oc extension
}

########################################################################
########################################################################
#
# Here's where the non-boilerplate code starts
#
########################################################################
########################################################################
package require Tk
catch {package require Ow}
catch {package require Img}	;# Can support more image formats with this
wm withdraw .

# Define standard global variables
proc OMFHB_PatchLevel {} {return 1.0a}
set omfhb_library [file dirname [Oc_DirectPathname \
	[file dirname [info script]] ]]
set omfhb_debug 0

# Define the Counter class 
source [file join [file dirname [info script]] counter.tcl]

Oc_Main SetAppName mmHelp
Oc_Main SetVersion 2.0b0
regexp \\\044Date:(.*)\\\044 {$Date: 2015/03/25 16:43:31 $} _ date
Oc_Main SetDate [string trim $date]
# regexp \\\044Author:(.*)\\\044 {$Author: dgp $} _ author
# Oc_Main SetAuthor [Oc_Person Lookup [string trim $author]]
Oc_Main SetAuthor [Oc_Person Lookup dgp]

Oc_CommandLine Option [Oc_CommandLine Switch] {
	{{URL optional} {} {Home page URL}}
    } {
	global url; set url $URL
} {End of options; next argument is URL}
Oc_CommandLine Parse $argv

###  Initialize the globals  ###
#
# array _omfhb_g contains variables which describe and configure 
# the browser software:
#	title	- program title for window title bars
#	help	- URL for Hb's own help pages
#	warn	- Set to true (1) to enable warning messages 
#	id	- ID number of next Browser to create
#	ict	- ID number of next canvac to create to hold an image
#	tct	- ID number of next frame to create to hold a table
#	period	- How many text insertions between calls to update
#		  Configures the interactivity of the display.
#	count	- A count of how many text insertions have been done
#	tabstop	- Distance between tabs in cm -- configures indentation
#	maxlistnest - How many levels can we nest HTML lists?
#	maxtabs	- How many tabs can we have on a line?
#	bullets - list of characters to use as "bullets" in UL elements
#		  (Yes, only three, that's what HTML 3.2 recommends.)
#       basefontsize - Default font size, in pixels.  This is intended
#                 as a constant.  Use "fontscale" to change size after
#                 initialization.
#       fontscale - Scaling for fonts, relative to basefontsize.

set _omfhb_g(title) 	"[Oc_Main GetAppName] Version [Oc_Main GetVersion]"

if {1} {
    # Point "Help|On mmHelp" directly into OOMMF documentation
    set _omfhb_g(help) [Oc_Url FromFilename [file join [file dirname \
	    [file dirname [file dirname [Oc_DirectPathname [info \
	    script]]]]] doc userguide userguide \
	    Documentation_Viewer_mmHelp.html]]
} else {
    # Point "Help|On mmHelp" to oommf/app/mmhelp/doc index file.
    set _omfhb_g(help)	[Oc_Url FromFilename [file join $omfhb_library \
	    mmhelp doc index.html]]
}

set _omfhb_g(warn) 		0
set _omfhb_g(id) 		0
set _omfhb_g(ict)		0
set _omfhb_g(tct)		0
set _omfhb_g(period)		10
set _omfhb_g(count) 		0
set _omfhb_g(tabstop)		0.2
set _omfhb_g(maxlistnest)	20
set _omfhb_g(maxtabs)		60
set _omfhb_g(bullets)		"O>\xb0"
for 	{set i [string length $_omfhb_g(bullets)]} \
        {$i < $_omfhb_g(maxlistnest)} {incr i} {
  append _omfhb_g(bullets) [string index $_omfhb_g(bullets) \
			[expr {[string length $_omfhb_g(bullets)] - 1}]]
}
if {[string match windows $tcl_platform(platform)]} {
    set _omfhb_g(basefontsize) 16  ;# Use 16 pixel font on Windows
} else {
    set _omfhb_g(basefontsize) 14  ;# Use 14 pixel font on Unix
}
# Let Oc_Option override default font sizes.
Oc_Option Get Font baseSize _omfhb_g(basefontsize)
if {[catch {incr _omfhb_g(basefontsize) 0}]} {
  Oc_Log Log "Bad value for Oc_Option 'Font baseSize':\
          $_omfhb_g(basefontsize)\n    (should be an integer font size)" error
}
set _omfhb_g(fontscale) 1.0

Oc_Main SetHelpURL $_omfhb_g(help)

### Data maps ###

# Map from file extensions to content types
# 	A sampling of common extensions.  Not complete.
array set hb_ExtensionMap {
	""	text/plain	.txt	text/plain	.text	text/plain
	.c	text/plain	.cc	text/plain	.c++	text/plain
	.h	text/plain	.hh	text/plain	.java	text/plain
	.l	text/plain	.pl	text/plain	.cpp	text/plain
	.sty	text/plain	.y	text/plain
	.html	text/html	.htm	text/html	.htl	text/html
	.gif	image/gif
	.jpg	image/jpeg	.jpeg	image/jpeg	.jpe	image/jpeg
	.tif	image/tiff	.tiff	image/tiff
	.png	image/png
	.pnm	image/x-portable-anymap	.pbm	image/x-portable-bitmap
	.pgm	image/x-portable-graymap	.ppm	image/x-portable-pixmap
	.xbm	image/x-xbitmap	.xpm	image/x-xpixmap
	.o	application/octet-stream	.a	application/octet-stream
	.z	application/octet-stream	.gz	application/octet-stream
	.bin	application/octet-stream	.exe	application/octet-stream
	.ps	application/postscript	.eps	application/postscript
	.dvi	application/x-dvi	.latex	application/x-latex
	.hdf	application/x-hdf	.tex	application/x-tex
	.texinfo	application/x-texinfo	.texi	application/x-texinfo
	.tar	application/x-tar	.zip	application/zip
}

# HTML entities (escape sequences) see HTML DTD
# Table includes both those sequences imported from the Added Latin 1
# entity set and the proposed entities for use with HTML 2.0
array set hb_EscapeTable {
	lt	<	gt	>	amp	&	quot	\"
	nbsp	\xa0	iexcl	\xa1	cent	\xa2	pound	\xa3
	curren 	\xa4	yen	\xa5	brvbar	\xa6	sect	\xa7
	uml	\xa8	copy	\xa9	ordf	\xaa	laquo	\xab
	not	\xac	shy	\xad	reg	\xae	macr	\xaf 
	deg	\xb0	plusmn	\xb1	sup2	\xb2	sup3	\xb3
	acute	\xb4	micro	\xb5	para	\xb6	middot	\xb7
	cedil	\xb8	sup1	\xb9	ordm	\xba	raquo	\xbb
	frac14	\xbc	frac12	\xbd	frac34	\xbe	iquest	\xbf
	times	\xd7	divide	\xf7	AElig	\xc6	Aacute	\xc1
 	Acirc	\xc2	Agrave	\xc0	Aring	\xc5	Atilde	\xc3
	Auml	\xc4	Ccedil	\xc7	ETH	\xd0	Eacute	\xc9
	Ecirc	\xca	Egrave	\xc8	Euml	\xcb	Iacute	\xcd
	Icirc	\xce	Igrave	\xcc	Iuml	\xcf	Ntilde	\xd1
	Oacute	\xd3	Ocirc	\xd4	Ograve	\xd2	Oslash	\xd8
	Otilde	\xd5	Ouml	\xd6	THORN	\xde 	acute	\xda
	Ucirc	\xdb	Ugrave	\xd9	Uuml	\xdc	Yacute	\xdd
	aacute	\xe1	acirc	\xe2	aelig	\xe6	agrave	\xe0
	aring	\xe5	atilde	\xe3	auml	\xe4	ccedil	\xe7
	eacute	\xe9	ecirc	\xea	egrave	\xe8	eth	\xf0
	euml	\xeb	iacute	\xed	icirc	\xee	igrave	\xec
	iuml	\xef	ntilde	\xf1	oacute	\xf3	ocirc	\xf4
	ograve	\xf2	oslash	\xf8	otilde	\xf5	ouml	\xf6
	szlig	\xdf	thorn	\xfe	uacute	\xfa	ucirc	\xfb
	ugrave	\xf9	uuml	\xfc	yacute	\xfd	yuml	\xff
}

###  Retrieving  ###

# proc ParseContentTypeString
#	Parses the string ctstring as a Content-type identifier according 
#	to RFC1521.  Returns an array containing the subparts by name.
proc _OMFHB_ParseContentTypeString {ctstring} {
  # First remove any whitespace and remove case
  regsub -all "\[ \t\r\n\]" $ctstring {} ctstring
  set ctstring [string tolower $ctstring]
  set elements [split $ctstring "/;"]
  set contentinfo(type) [lindex $elements 0]
  set contentinfo(subtype) [lindex $elements 1]
  set contentinfo(parameterlist) [lrange $elements 2 end]
  return [array get contentinfo]
}

# proc RetrievalError url message contentsref
#	Places in variable named by contentsref an appropriate error
#	message, and returns content-type text/html so that it will
#	be rendered for the user.
proc _OMFHB_RetrievalError {url message contentsref} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts retrievalError}
  upvar $contentsref contents
  set contents "<TITLE>$_omfhb_g(title) Retrieval Error</TITLE>\
<H1>$_omfhb_g(title) Retrieval Error</H1>\
<P>Can't fetch &lt;URL:[$url ToString]&gt;.$message"
  return [_OMFHB_ParseContentTypeString text/html]
}

# proc FetchUrl url contentsref
#	Places in variable named by contentsref as one huge string the 
#	contents of the "file" located by the URL.  Returns the MIME 
#	Content-type of those contents.
#
#	First pass: only "file:" URLs.
#
#	Migrate this functionality into the URL class
proc _OMFHB_FetchUrl {url contentsref} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts fetchURL}
  upvar $contentsref contents
# Perhaps table lookup or proc construction is better than switch down the road
  switch [$url Cget -scheme] {
    file {
      if {[catch {$url ToFilename} filename]} {
        return [_OMFHB_RetrievalError $url "\
<p>Can't determine local filename. $filename" contents]
      }
      if {[file exists $filename]} {
        if {[file readable $filename]} {
          global hb_ExtensionMap
          set fileID [open $filename RDONLY]
          fconfigure $fileID -translation binary -eofchar {}
          set contents [read -nonewline $fileID]
          close $fileID
          set ext [string tolower [file extension $filename]]
          if {[catch {set ct $hb_ExtensionMap($ext)}]} {
            return [_OMFHB_ParseContentTypeString content/unknown]
          } else {
            return [_OMFHB_ParseContentTypeString $ct]
          }
        } else {
          return [_OMFHB_RetrievalError $url "\
<p>Read permission denied for &quot;$filename&quot; . " contents]
        }
      } else {
        return [_OMFHB_RetrievalError $url "\
<p>File not found: &quot;$filename&quot; . " contents]
      }
     }
    default {
        return [_OMFHB_RetrievalError $url "\
<p>Protocol &quot;[$url Cget -scheme]&quot; not implemented.\
<p>Try a real WWW browser." contents]
     }
  }
}

###  Rendering  ###

# proc MakeFontTag textwidget
#	Create a font tag in the textwidget from current style settings
proc _OMFHB_MakeFontTag {t} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts makeFontTag}
  set fontspec $_omfhb_g(family$t).$_omfhb_g(weight$t).$_omfhb_g(slant$t).$_omfhb_g(pixels$t)
  set _omfhb_g(fonttag$t) $fontspec.$_omfhb_g(offset$t)
  set taglist [$t tag names]
  set idx [lsearch -glob $taglist $fontspec.*]
  set fontsize [expr {round($_omfhb_g(fontscale) * $_omfhb_g(pixels$t))}]
  if {$idx < 0} {
    if {[string length [info commands font]]} {
      set fontname [font create -family $_omfhb_g(family$t) \
          -weight $_omfhb_g(weight$t) -slant $_omfhb_g(slant$t) \
          -size [expr {-1 * $fontsize}]]
    } else {
      set fontname -*-$_omfhb_g(family$t)-$_omfhb_g(weight$t)-$_omfhb_g(slant$t)-*-*-$fontsize-*-*-*-*-*-*-*
    }
    if {$_omfhb_g(warn)} {
      puts "Adding font: $fontname"
    }
  } else {
      set tag [lindex $taglist $idx]
      set fontname [lindex [$t tag configure $tag -font] 4]
  }
  if {[lsearch -glob [$t tag names] $_omfhb_g(fonttag$t)] < 0} {
    switch -exact -- $_omfhb_g(offset$t) {
        sup {set offset [expr {$_omfhb_g(pixels$t) / 2}]}
        sub {set offset [expr {- $_omfhb_g(pixels$t) / 2}]}
        baseline {set offset 0}
    }
    $t tag configure $_omfhb_g(fonttag$t) -font $fontname -offset $offset
  }
}

# proc RenderFontStyle textref textwidget 
#	DOME: Consider adding color
proc _OMFHB_RenderFontStyle {textref textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderFontStyle}
  upvar $textref text
  regexp -nocase "^<font(\[^>\]*)>(.*)$" \
         $text match attrlist text
  set size [_OMFHB_GetAttribute size $attrlist]
  set oldpixels $_omfhb_g(pixels$textwidget)
  if {[regexp {^\+([0-9]+)$} $size _ val]} {
      set _omfhb_g(pixels$textwidget) [expr {$oldpixels + 2*$val}]
  } elseif {[regexp {^-([0-9]+)$} $size _ val]} {
      set _omfhb_g(pixels$textwidget) [expr {$oldpixels - 2*$val}]
  } elseif {[regexp {^([0-9]+)$} $size _ val]} {
      set _omfhb_g(pixels$textwidget) [expr {8 + 2*$val}]
  } elseif {[string length $size]} {
      _OMFHB_StrictWarn "Illegal value in SIZE attribute of FONT tag: $size" \
          $textwidget text
  }
  _OMFHB_MakeFontTag $textwidget
  _OMFHB_RenderMaxTextSequence text $textwidget
  set _omfhb_g(pixels$textwidget) $oldpixels
  _OMFHB_MakeFontTag $textwidget
  if {[regexp -nocase "^</font\[^a-z0-9_.\]" $text]} {
    regexp -nocase "^</font\[^>\]*>(.*)$" $text match text
  } else {
    _OMFHB_InferClose font $textwidget text
  }
}

# proc ItalicizeFont tw
#	Set the current font to be in italic slant.
proc _OMFHB_ItalicizeFont {tw} {
  global _omfhb_g
  set tmp $_omfhb_g(slant$tw)
  if {[string length [info commands font]]} {
    set _omfhb_g(slant$tw) italic
  } else {
    set _omfhb_g(slant$tw) i
  }
  _OMFHB_MakeFontTag $tw
  return $tmp
}

# proc FixedFont tw
#	Set the current font to be in a fixed width family.
proc _OMFHB_FixedFont {tw} {
  global _omfhb_g
  set tmp $_omfhb_g(family$tw)
  set _omfhb_g(family$tw) courier
  _OMFHB_MakeFontTag $tw
  return $tmp
}

# proc EmboldenFont tw
#	Set the current font to be of bold weight
proc _OMFHB_EmboldenFont {tw} {
  global _omfhb_g
  set tmp $_omfhb_g(weight$tw)
  set _omfhb_g(weight$tw) bold
  _OMFHB_MakeFontTag $tw
  return $tmp
}

proc _OMFHB_SuperFont {tw} {
  global _omfhb_g
  set tmp $_omfhb_g(offset$tw)
  set _omfhb_g(offset$tw) sup
  _OMFHB_MakeFontTag $tw
  return $tmp
}

proc _OMFHB_SubFont {tw} {
  global _omfhb_g
  set tmp $_omfhb_g(offset$tw)
  set _omfhb_g(offset$tw) sub
  _OMFHB_MakeFontTag $tw
  return $tmp
}

# proc DisplayRenderErrorMessage message textwidget
#	display an error message in textwidget
proc _OMFHB_DisplayRenderErrorMessage {message textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts displayRenderErrorMessage}
  set wholemessage "<TITLE>$_omfhb_g(title) Rendering Error</TITLE>\
                    <H1>$_omfhb_g(title) Rendering Error</H1>$message"
  # Error browser displays HTML, but doesn't activate links.
  # Can't have navigation because the "homepage" is not a file,
  # so it has no URL, and can't be navigated to and from.
  if {[catch {toplevel .hb_$_omfhb_g(id)} result]} {
    # Apparently trying to open a second help browser with same ID
    # Change this warning to a dialog box or something
    return -code error -errorinfo "Can't do that!"
  } 
  incr _omfhb_g(id)
  set browser $result
  set currentstate [$textwidget cget -state]
  $textwidget configure -state disabled
  set tw [text $browser.text -background white -insertwidth 0]
  set buttons [frame $browser.buttons]
  set _omfhb_g(continue$tw) [button $buttons.continue -text Continue]
  $_omfhb_g(continue$tw) configure \
      -command "set _omfhb_g(continue$textwidget) 1; destroy $browser"
  set _omfhb_g(abort$tw) [button $buttons.abort -text Abort]
  $_omfhb_g(abort$tw) configure \
      -command "set _omfhb_g(continue$textwidget) 0; destroy $browser"
  set yscroll [scrollbar $browser.yscroll \
                                     -command [list $tw yview] \
                                     -orient vertical]
  set xscroll [scrollbar $browser.xscroll \
                                     -command [list $tw xview] \
                                     -orient horizontal]
  $tw configure -yscrollcommand [list $yscroll set] \
                -xscrollcommand [list $xscroll set]

  pack $_omfhb_g(continue$tw) -padx 25 -pady 10 -side left
  pack $_omfhb_g(abort$tw) -padx 25 -pady 10 -side right
  pack $buttons -side bottom -fill both
  pack $yscroll -side right -fill y
  pack $xscroll -side bottom -fill x
  pack $tw -side left -fill both -expand 1

  # Windows seems to need this help to enable selections
  focus $tw

  # Tag configurations
  $tw tag configure justifyleft -justify left
  $tw tag configure justifycenter -justify center
  $tw tag configure justifyright -justify right
  $tw tag configure wordwrap -wrap word
  $tw tag configure nowrap -wrap none
  for {set i 0} {$i < $_omfhb_g(maxtabs)} {incr i} {
    set lm [expr {(6 * $i + 4) * $_omfhb_g(tabstop)}]
    set tab1 [expr {$lm + 5 * $_omfhb_g(tabstop)}]
    set tab2 [expr {$lm + 6 * $_omfhb_g(tabstop)}]
    $tw tag configure indent$i -lmargin1 ${lm}c -lmargin2 ${lm}c
    $tw tag configure rindent$i -rmargin ${lm}c
    $tw tag configure bullet$i -lmargin1 0c -lmargin2 0c \
	-tabs [list ${tab1}c right ${tab2}c left]
  }
  set lm [expr {4 * $_omfhb_g(tabstop)}]
  $tw tag configure rule -borderwidth 2 -relief groove \
        -tabs [winfo width $tw]
  if {[catch {$tw tag configure rule \
	-font -*-courier-medium-r-*-*-2-*-*-*-*-*-*-*}]} {
    $tw tag configure rule -font -*-courier-medium-r-*-*-10-*-*-*-*-*-*-*
  }
  bind $tw <Configure> {%W tag configure rule -tabs [expr {%w - 10}]}

  _OMFHB_Display_text_html wholemessage "" $tw
  $tw configure -state disabled
  catch {  Ow_SetIcon $browser}
  tkwait window $browser
  $textwidget configure -state $currentstate
}

# proc DisplayUnknownContentType URL type subtype textwidget
#	display error message in textwidget
proc _OMFHB_DisplayUnknownContentType {URL type subtype textwidget} {
  _OMFHB_DisplayRenderErrorMessage "\
<p>Can't display contents of URL \"[$URL ToString]\" . \
<p>Content type \"$type/$subtype\" not implemented.\
<p>Try a World Wide Web browser." $textwidget
}

# proc DisplayInvalidHtml message textwidget
#	display error message in textwidget
proc _OMFHB_DisplayInvalidHtml {message textwidget} {
  _OMFHB_DisplayRenderErrorMessage "\
<p>Page contains invalid HTML markup.<p>\
$message" $textwidget
}

# proc EscapeLookup key
#	Look up the key in the escape table.  If not found,
#	translate into a character which indicates the error.
proc _OMFHB_EscapeLookup {key} {
  global hb_EscapeTable
  set result "\xbf"
  catch {set result $hb_EscapeTable($key)}
  return $result
}

# proc TranslateEscapes text
#	Translate the HTML entities (escape sequences) into the
#	appropriate characters
proc _OMFHB_TranslateEscapes {text} {
  if {![regexp & $text]} {return $text}
  regsub -all {([]\\$[])} $text {\\\1} text
  regsub -all {&#([0-9][0-9]?[0-9]?);?} $text \
              {[format %c [scan \1 %d tmp; set tmp]]} text
  regsub -all {&([a-zA-Z]+);?} $text {[_OMFHB_EscapeLookup \1]} text
  return [subst $text]
}

# proc GetAttribute attr attrlist
#	Extract from the attribute list attrlist the value of the
#	attribute attr. (If duplicates, take first)
#
#	The syntax assumed by this parser isn't the fully general
#	syntax of SGML attribute lists, but should handle most
#	structures that appear in HTML files.  Attribute values
#	which contain whitespace must be quoted with "" or ''.
#	Quoted values cannot contain their quote symbol.  Use the
#	other quotes, or use an SGML entity if that's a problem.
#	One level of surrounding quotes are stripped from the value,
#	so if they are required in the final value, quote or escape them.
proc _OMFHB_GetAttribute {attr attrlist} {
  if {[regexp -nocase [join [list \
				"\[ \t\r\n\]+" \
				$attr \
				"\[ \t\r\n\]*" \
				= \
				"\[ \t\r\n\]*" \
				( \
				{("[^"]*")|} \
				{('[^']*')|} \
				"(\[^'\" \t\r\n\]\[^ \t\r\n\]*)|" \
				$) ] "" ] \
           $attrlist match attrvalue]} {
    if {[regexp {"([^"]*)"} $attrvalue match unquoted]} {
      return $unquoted
    } elseif {[regexp {'([^']*)'} $attrvalue match unquoted]} {
      return $unquoted
    } 
    return $attrvalue
  } else {
    return {}
  }
}

# proc RenderHR textref textwidget
#	text begins with an HR element.  Process it and return
#	the rest of the string.
proc _OMFHB_RenderHR {textref textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderHR}
  upvar $textref text
  regexp -nocase {<hr[^>]*>(</hr[^>]*>)?(.*)} \
         $text match optendtag text
#  $textwidget insert end "    ----------------------------------------------------------------    "
  $textwidget insert end "\t" [list rule $_omfhb_g(indenttag$textwidget)]
#  _OMFHB_RequireLineBreak $textwidget
  $textwidget insert end "\n" rule
}

# proc RenderAltText attrlist textwidget
#	Get the alt text and render it.
proc _OMFHB_RenderAltText {attrlist textwidget} {
  set alttext [_OMFHB_GetAttribute alt $attrlist]
  if {[regexp . $alttext]} {
    _OMFHB_RenderPcdata $alttext $textwidget
  } else {
    _OMFHB_RenderPcdata {[Image]} $textwidget
  }
}

# proc RenderIMG textref textwidget
#	text begins with an IMG element.  Process it and return
#	the rest of the string.
proc _OMFHB_RenderIMG {textref textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderIMG}
  upvar $textref text
  regexp -nocase {<img([^>]*)>(</img[^>]*>)?(.*)} $text match attrlist \
      optendtag text
  set src [_OMFHB_GetAttribute src $attrlist]
  if {![regexp . $src]} {
      _OMFHB_RenderAltText $attrlist $textwidget
      return
  }
  if {[catch {foreach {imgURL foo} \
          [Oc_Url FromString $src $_omfhb_g(URLin$textwidget)] {}}]} {
      _OMFHB_StrictWarn "Malformed URL in SRC attribute of IMG tag" \
          $textwidget text
      _OMFHB_RenderAltText $attrlist $textwidget
      return
  }
  array set contentinfo [_OMFHB_FetchUrl $imgURL data]
  if {[string compare $contentinfo(type) image]} {
      _OMFHB_StrictWarn "URL in SRC attribute of IMG tag locates non-image." \
          $textwidget text
      _OMFHB_RenderAltText $attrlist $textwidget
      return
  }
  if {[catch {image create photo -format $contentinfo(subtype) -data $data} \
          img]} {
      # Can't create image from -data, try file.
      if {[string compare [$imgURL Cget -scheme] file] == 0 && \
              ![catch {$imgURL ToFilename} imgfile]} {
          if {[catch {image create photo -format $contentinfo(subtype) \
              -file $imgfile} img]} {
              _OMFHB_StrictWarn "Can't render image subtype:
	$contentinfo(subtype)" $textwidget text
              _OMFHB_RenderAltText $attrlist $textwidget
              return
          }
      } else {
        # The -data option fails, and the source file is not local,
        # so write a temp file.  Implement this when non file:* URLs
        # are possible
          _OMFHB_StrictWarn \
                  "Image rendering from non-local file not implemented!" \
                  $textwidget text
          _OMFHB_RenderAltText $attrlist $textwidget
          return
      }
  }
  # Image created successfully
  set border [_OMFHB_GetAttribute border $attrlist]
  if {![regexp {^[0-9]+$} $border]} {
      set border 2
  }

  # Get align attribute and translate to nearest Tcl option
  # Values "left" and "right" ignored
  set align [_OMFHB_GetAttribute align $attrlist]
  if {[regexp -nocase {^(top|middle|bottom)$} $align]} {
      switch [string tolower $align] {
          top		{set align top}
          middle	{set align center}
          bottom	{set align baseline}
      }
  } else {
      set align center
  }

  # Display the image
  # Create a canvas to hold it
  canvas $textwidget.img$_omfhb_g(ict) \
        -width [expr {[image width $img] + 2 * $border}] \
  	-height [expr {[image height $img] + 2 * $border}] \
  	-background [lindex [$textwidget configure -background] 4] \
          -highlightthickness 0

  # If image is part of hyperlink, color its border and set bindings
  if {[regexp . $_omfhb_g(linktag$textwidget)]} {
      regexp {URL:(.*)} $_omfhb_g(linkdest$textwidget) match URL
      bind $textwidget.img$_omfhb_g(ict) <Enter> [concat \
              [list _OMFHB_DisplayLink $textwidget] [split $URL #]]
      bind $textwidget.img$_omfhb_g(ict) <Leave> \
              [list _OMFHB_EraseLink $textwidget]
      $textwidget.img$_omfhb_g(ict) create rectangle $border $border \
              [expr {$border + [image width $img]}] \
              [expr {$border + [image height $img]}] \
              -fill [lindex [$textwidget configure -background] 4] \
              -outline {}
      if {[[lindex [split $URL #] 0] IsAccessible]} {
          $textwidget.img$_omfhb_g(ict) configure -background blue
      } else {
          $textwidget.img$_omfhb_g(ict) configure -background red
      }
      bind $textwidget.img$_omfhb_g(ict) <1> \
                  [eval list _OMFHB_GoToUrl [split $URL #] $textwidget]
  } 

  # Place the image on the canvas
  $textwidget.img$_omfhb_g(ict) create image $border $border -image $img \
          -anchor nw

  # Insert the canvas into textwidget according to alignment
  $textwidget window create end -window $textwidget.img$_omfhb_g(ict) \
          -align $align

  # Extend any active tags to cover image
  $textwidget tag add $_omfhb_g(anchorname$textwidget) end-2c
  $textwidget tag add $_omfhb_g(indenttag$textwidget) end-2c
  $textwidget tag add $_omfhb_g(rindenttag$textwidget) end-2c
  $textwidget tag add $_omfhb_g(wraptag$textwidget) end-2c
  $textwidget tag add $_omfhb_g(justtag$textwidget) end-2c

  # Increment the image count
  incr _omfhb_g(ict)
}

# proc renderPCDATA text textwidget
#	text contains no HTML tags. Render it into the textwidget
proc _OMFHB_RenderPcdata {text textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts "renderPCDATA $text"}
  if {[regexp {^$} $text]} {return}
  $textwidget insert end [_OMFHB_TranslateEscapes $text] \
          [list $_omfhb_g(fonttag$textwidget) $_omfhb_g(linktag$textwidget) \
          $_omfhb_g(linkdest$textwidget) $_omfhb_g(anchorname$textwidget) \
          $_omfhb_g(indenttag$textwidget) $_omfhb_g(rindenttag$textwidget) \
          $_omfhb_g(wraptag$textwidget) $_omfhb_g(justtag$textwidget)]
  incr _omfhb_g(count)
  if {$_omfhb_g(count) % $_omfhb_g(period) == 0} {
    $textwidget configure -state disabled
    update
    $textwidget configure -state normal
  }
}

# proc RequireLineBreak textwidget
#	Call this routine when there needs to be a line break
#	in the output.
proc _OMFHB_RequireLineBreak {textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts requireLineBreak}
  while {[string match " " [$textwidget get {end -2 char}]]} {
    $textwidget delete {end -2 char}
  }
  switch -exact -- [$textwidget get {end -2 char}] {
      \n {}
      default {$textwidget insert end "\n"}
  }
}

# proc RequireEmptyLine textwidget
#	Call this routine when there needs to be an empty line
#	in the output.
proc _OMFHB_RequireEmptyLine {textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts requireEmptyLine}
  _OMFHB_RequireLineBreak $textwidget
  switch -exact -- [$textwidget get {end -3 char}] {
      \n {}
      default {$textwidget insert end "\n"}
  }
}

# proc RenderBR textref textwidget
#	text begins with a BR element.  Process it and return
#	the rest of the string.
proc _OMFHB_RenderBR {textref textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderBR}
  upvar $textref text
  regexp -nocase "<br\[^>\]*>(</br\[^>\]*>)?\[ \t\r\n\]*(.*)" \
         $text match optendtag text
  $textwidget insert end "\n"
}

# proc StrictWarn msg tw textref
#	A general warning that the Strict HTML rules aren't being followed
proc _OMFHB_StrictWarn {msg tw textref} {
  global _omfhb_g
  if {$_omfhb_g(warn)} {
    puts "Unsupported element encountered"
    puts $msg
    puts "Trying to recover..."
    puts ""
  }
  if {$_omfhb_g(strict$tw) && $_omfhb_g(continue$tw)} {
    _OMFHB_DisplayInvalidHtml $msg $tw
    if {!$_omfhb_g(continue$tw)} {
      upvar $textref text
      set text ""
    }
  }
}

# proc InferClose tag tw textref
#	Report the inference of a missing end tag.
proc _OMFHB_InferClose {tag tw textref} {
  upvar $textref text
  _OMFHB_StrictWarn "Missing &lt;/$tag&gt; tag." $tw text
}

# proc RenderFontFormat textref textwidget tag fontattribute routine
#	text begins with an element indicating the formatting of the font
#	of the text it contains by setting the fontattribute with the routine.
#	Process this element and return the rest of the string.
proc _OMFHB_RenderFontFormat {textref textwidget tag fontattribute routine} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderFontFormat}
  upvar $textref text
  regexp -nocase "^<$tag\[^>\]*>(.*)$" \
         $text match text
# Here need to set tags to change appearance of text font
  set old [$routine $textwidget]
  _OMFHB_RenderMaxTextSequence text $textwidget
# Undo whatever we did
  set _omfhb_g($fontattribute$textwidget) $old
  _OMFHB_MakeFontTag $textwidget
  if {[regexp -nocase "^</$tag\[^a-z0-9_.\]" $text]} {
    regexp -nocase "^</$tag\[^>\]*>(.*)$" $text match text
  } else {
    _OMFHB_InferClose $tag $textwidget text
  }
}

# proc RenderUnknown textref textwidget tag
#	Element is not supported
#	Check if a corresponding end tag exists in the remaining text.
#	If so, assume this is formatting-type tag containing a text
#	sequence, and try to process based on that assumption.  
#	Otherwise, assume the tag's contents must be empty, and just
#	strip it away and ignore it.
#	This should allow browser to be flexible when encountering higher
#	level HTML or proprietary extensions.  It's a shot anyway.
proc _OMFHB_RenderUnknown {textref textwidget tag} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderUnknown}
  upvar $textref text
  regexp -nocase "^<$tag\[^>\]*>(.*)$" \
         $text match text
  _OMFHB_StrictWarn "The tag &lt;[string toupper $tag]&gt; 
is unknown and not supported." $textwidget text
  if {[regexp -nocase "</$tag\[^a-z0-9_.\]" $text]} {
    _OMFHB_RenderMaxTextSequence text $textwidget
    # An 'Abort' may have cleared the text
    if {[string length $text]} {
      if {[regexp -nocase "^</$tag\[^a-z0-9_.\]" $text]} {
        regexp -nocase "^</$tag\[^>\]*>(.*)$" $text match text 
      } else {
        _OMFHB_DisplayInvalidHtml "The tag &lt;[string toupper $tag]&gt; 
is unknown and not supported." $textwidget
        set text ""
      }
    }
  }
}

# proc RenderMaxTextSequence textref textwidget
#	text starts with a text sequence.  Render it into the textwidget.
#	Stop on first thing which isn't part of a text sequence and return it
proc _OMFHB_RenderMaxTextSequence {textref textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderMaxTextSequence}
  upvar $textref text
  regsub "^\[ \t\r\n\]+" $text {} text
  while {[regexp . $text]} {
    if {[regexp {<[/a-zA-Z]} $text]} {
      regexp {^([^<]*<([^/a-zA-z][^<]*<)*)((/?[a-zA-Z][a-zA-Z0-9._]*).*)$} \
             $text match beforetag foo text tag
      # Take the < off the end of beforetag
      regsub {<$} $beforetag {} beforetag
      # Collapse the whitespace
      regsub -all "\[ \t\r\n\]+" $beforetag " " beforetag
      _OMFHB_RenderPcdata $beforetag $textwidget
      regsub {^(.*)$} $text {<\1} text
      set tag [string tolower $tag]
  # proc construction might be better than switch
      switch -glob -- $tag {
        a	-
	img	-
	br      {_OMFHB_Render[string toupper $tag] text $textwidget}
  	tt	-
	code	-
	samp	-
	kbd	-
	var	{_OMFHB_RenderFontFormat text $textwidget $tag family \
                 _OMFHB_FixedFont}
  	b	-
	strong 	{_OMFHB_RenderFontFormat text $textwidget $tag weight \
		 _OMFHB_EmboldenFont}
  	i	-
	em	-
	cite	{_OMFHB_RenderFontFormat text $textwidget $tag slant \
		 _OMFHB_ItalicizeFont}
	sup	{_OMFHB_RenderFontFormat text $textwidget $tag offset \
		 _OMFHB_SuperFont}
	sub	{_OMFHB_RenderFontFormat text $textwidget $tag offset \
		 _OMFHB_SubFont}
	font	{_OMFHB_RenderFontStyle text $textwidget}
  	/*	-
	address	-
	hr	-
	h[1-6]	-
	p	-
	dl	-
	blockquote	-
	pre	-
	ul	-
	ol	-
	dt	-
	dd	-
	li	-
        table   -
	div	{# All the tags which can legally appear next, but which
  		 # do not continue a Text Sequence
  		 break}
  	default	{# Try to handle unknown tags (eg. Netscape extensions)
  		 _OMFHB_RenderUnknown	text $textwidget $tag}
      }
    } else {
      # Collapse the whitespace
      regsub -all "\[ \t\r\n\]+" $text " " text
      _OMFHB_RenderPcdata $text $textwidget
      set text ""
    }
  }
}

# proc RenderMaxBlockSequence textref textwidget
#	text starts with a block sequence.  Render it into the textwidget.
#	Stop on first thing which isn't part of a block sequence and return it
proc _OMFHB_RenderMaxBlockSequence {textref textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderMaxBlockSequence}
  upvar $textref text
  while {[regexp . $text]} {
    if {[catch {_OMFHB_RenderBlock text $textwidget}]} {
      break
    }
  }
}

# proc RenderFlow textref textwidget
#	text starts with a flow.  Render it into the textwidget.
#	Stop on first thing which isn't part of a flow and return it.
proc _OMFHB_RenderFlow {textref textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderFlow}
  upvar $textref text
  set oldlength 0
  set newlength [string length $text]
  while {$newlength != $oldlength} {
    _OMFHB_RenderMaxBlockSequence text $textwidget
    _OMFHB_RenderMaxTextSequence text $textwidget
    set oldlength $newlength
    set newlength [string length $text]
  }
}

# proc RenderHeading textref textwidget
#	textref begins with a heading element.  Process it and return
#	the rest of the string.
proc _OMFHB_RenderHeading {textref textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderHeading}
  upvar $textref text
  regexp -nocase {^<h([1-6])[^>]*>(.*)$} $text match level text
# Here need to set tags to change text font, based on level
  set oldpixels $_omfhb_g(pixels$textwidget)
  set _omfhb_g(pixels$textwidget) \
	  [expr {$_omfhb_g(basefontsize) + 2*(4 - $level)}]
  set oldweight [_OMFHB_EmboldenFont $textwidget]
  _OMFHB_RenderMaxTextSequence text $textwidget
# Undo whatever we did
  set _omfhb_g(pixels$textwidget) $oldpixels
  set _omfhb_g(weight$textwidget) $oldweight
  _OMFHB_MakeFontTag $textwidget
  if {[regexp -nocase "^</h$level" $text]} {
    _OMFHB_RequireLineBreak $textwidget
    regexp -nocase "^</h$level\[^>\]*>(.*)$" $text match text
  } else {
    _OMFHB_InferClose H$level $textwidget text
  }
}

# proc RenderADDRESS textref textwidget
#	text begins with an ADDRESS element.  Process it and return
#	the rest of the string.
proc _OMFHB_RenderADDRESS {textref textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderADDRESS}
  upvar $textref text
  regexp -nocase {^<address[^>]*>(.*)$} $text match text 
  set old [_OMFHB_ItalicizeFont $textwidget]
  set oldlength 0
  set newlength [string length $text]
  while {$oldlength != $newlength} {
    _OMFHB_RenderMaxTextSequence text $textwidget
    if {[regexp -nocase {^<p[^a-z0-9_.]} $text]} {
      _OMFHB_RenderP text $textwidget
    }
    set oldlength $newlength
    set newlength [string length $text]
  }
# Undo whatever we did
  set _omfhb_g(slant$textwidget) $old
  _OMFHB_MakeFontTag $textwidget
  if {[regexp -nocase "^</address" $text]} {
    _OMFHB_RequireLineBreak $textwidget
    regexp -nocase "^</address\[^>\]*>(.*)$" $text match text
  } else {
    _OMFHB_InferClose ADDRESS $textwidget text
  }
}

# proc RenderBLOCKQUOTE textref textwidget
#	text begins with a BLOCKQUOTE element.  Process it and return
#	the rest of the string.
proc _OMFHB_RenderBLOCKQUOTE {textref textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderBLOCKQUOTE}
  upvar $textref text
  regexp -nocase {^<blockquote[^>]*>(.*)$} $text match text
  # Require a line break before any BLOCKQUOTE
  _OMFHB_RequireLineBreak $textwidget
# Here need to set tags to change text font, based on level
  incr _omfhb_g(indentlevel$textwidget)
  set _omfhb_g(indenttag$textwidget) indent$_omfhb_g(indentlevel$textwidget)
  incr _omfhb_g(rindentlevel$textwidget)
  set _omfhb_g(rindenttag$textwidget) rindent$_omfhb_g(rindentlevel$textwidget)
  _OMFHB_RenderBodyContent text $textwidget
# Undo whatever we did
  incr _omfhb_g(indentlevel$textwidget) -1
  set _omfhb_g(indenttag$textwidget) indent$_omfhb_g(indentlevel$textwidget)
  incr _omfhb_g(rindentlevel$textwidget) -1
  set _omfhb_g(rindenttag$textwidget) rindent$_omfhb_g(rindentlevel$textwidget)
  # Require a blank line following any BLOCKQUOTE
  _OMFHB_RequireEmptyLine $textwidget
  if {[regexp -nocase "^</blockquote" $text]} {
    regexp -nocase "^</blockquote\[^>\]*>(.*)$" $text match text
  } else {
    _OMFHB_InferClose BLOCKQUOTE $textwidget text
  }
}

# proc RenderA textref textwidget
#	text begins with an A element.  Process it and return
#	the rest of the string.
proc _OMFHB_RenderA {textref textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderA}
  upvar $textref text
  # Check for nested A tags -- illegal 
  if {$_omfhb_g(inanchor$textwidget)} {
    _OMFHB_DisplayInvalidHtml \
"One &lt;A&gt; element is nested inside another one." $textwidget
    set text ""
  }
  set _omfhb_g(inanchor$textwidget) 1
  regexp -nocase {^<a([^>]*)>(.*)$} $text match attrlist text
  if {[regexp . [set href [_OMFHB_GetAttribute href $attrlist]]]} {
    if {![catch {foreach {url fragment} [Oc_Url FromString $href \
            $_omfhb_g(URLin$textwidget)] {}}]} {
        if {[$url IsAccessible]} {
            # Add testing for cached links?
            set _omfhb_g(linktag$textwidget) link
            set _omfhb_g(linkdest$textwidget) URL:$url#$fragment
        } else {
            set _omfhb_g(linktag$textwidget) deadlink
            set _omfhb_g(linkdest$textwidget) URL:$url#$fragment
        }
    } else {
        set _omfhb_g(linktag$textwidget) deadlink
        set _omfhb_g(linkdest$textwidget) URL:$href
    }
  }
  if {[regexp . [set name [_OMFHB_GetAttribute name $attrlist]]]} {
    set _omfhb_g(anchorname$textwidget) AN:$name
  }
  set beforelength [string length $text]
  _OMFHB_RenderMaxTextSequence text $textwidget
  # Be sure no anchor is empty
  if {[string length $text] == $beforelength} {
    _OMFHB_RenderPcdata "&nbsp;" $textwidget
  }
# Undo whatever we did
  set _omfhb_g(linktag$textwidget) {}
  set _omfhb_g(linkdest$textwidget) {}
  set _omfhb_g(anchorname$textwidget) {}
  set _omfhb_g(inanchor$textwidget) 0
  if {[regexp -nocase {^</a[^a-z0-9_.]} $text]} {
    regexp -nocase "^</a\[^>\]*>(.*)$" $text match text
  } else {
    if {[regexp "^<(/?\[a-zA-Z\]\[a-zA-Z0-9._\]*)" $text match tag]} {
      _OMFHB_DisplayInvalidHtml \
"Tag &lt;[string toupper $tag]&gt; is not permitted 
between &lt;A&gt; and &lt;/A&gt;." $textwidget
    } else {
      _OMFHB_DisplayInvalidHtml \
"Illegal contents between &lt;A&gt; and &lt;/A&gt;." $textwidget
    }
    set text ""
  }
}


proc _OMFHB_GetJustification {tw} {
  global _omfhb_g
  return $_omfhb_g(justtag$tw)
}

proc _OMFHB_SetJustification {tw align} {
  global _omfhb_g
  set align [string tolower $align]
  switch -exact -- $align {
      left	-
      center	-
      right	{set _omfhb_g(justtag$tw) justify$align}
      default	{set _omfhb_g(justtag$tw) justifyleft}
  }
}

# proc RenderP textref textwidget
#	text begins with a P element.  Process it and return
#	the rest of the string.
proc _OMFHB_RenderP {textref textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderP}
  upvar $textref text
  regexp -nocase {^<p([^>]*)>(.*)$} $text match attrlist text
  _OMFHB_RequireEmptyLine $textwidget
  set origjust [_OMFHB_GetJustification $textwidget]
  _OMFHB_SetJustification $textwidget [_OMFHB_GetAttribute align $attrlist]
  _OMFHB_RenderMaxTextSequence text $textwidget
  # Strip away the closing </P> tag, if present
  regsub -nocase {^</p[^>]*>} $text {} text
  _OMFHB_RequireLineBreak $textwidget
  _OMFHB_SetJustification $textwidget $origjust
}

# proc RenderDIV textref textwidget
#	text begins with a DIV element.  Process it and return
#	the rest of the string.
proc _OMFHB_RenderDIV {textref textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderDIV}
  upvar $textref text
  regexp -nocase {^<div([^>]*)>(.*)$} $text match attrlist text
  _OMFHB_RequireLineBreak $textwidget
  set origjust [_OMFHB_GetJustification $textwidget]
  _OMFHB_SetJustification $textwidget [_OMFHB_GetAttribute align $attrlist]
  _OMFHB_RenderFlow text $textwidget
  # Strip away the closing </DIV> tag, if present
  regsub -nocase {^</div[^>]*>} $text {} text
  _OMFHB_RequireLineBreak $textwidget
  _OMFHB_SetJustification $textwidget $origjust
}

proc _OMFHB_RenderTABLE {textref textwidget} {
#	text begins with a TABLE element.  For now, just
#       process alignment attribute.
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderTABLE}
  upvar $textref text
  regexp -nocase {^<table([^>]*)>(.*)$} $text match attrlist text
  _OMFHB_RequireLineBreak $textwidget
  set origjust [_OMFHB_GetJustification $textwidget]
  _OMFHB_SetJustification $textwidget [_OMFHB_GetAttribute align $attrlist]
  # create frame
  set frame $textwidget.table$_omfhb_g(tct)
  frame $frame
  $frame configure -background [lindex [$textwidget configure -background] 4]
  # Place the table frame in the master text widget
  $textwidget window create end -window $frame
  set bw [_OMFHB_GetAttribute border $attrlist]
  if {[string length $bw] == 0} {
    set bw 0
  }
  set row 0
  regsub "^\[ \t\r\n\]+" $text {} text
  while {[regexp -nocase {^<tr[^a-z0-9._]} $text match ]} {
    regexp -nocase {^<tr([^>]*)>(.*)$} $text match attrlist text
    set col 0
    set maxheight 1
    regsub "^\[ \t\r\n\]+" $text {} text
    while {[regexp -nocase {^<td[^a-z0-9._]} $text match ]} {
      regexp -nocase {^<td([^>]*)>(.*)$} $text match attrlist text
      if {![info exists maxwidth($col)]} {
        set maxwidth($col) 1
      }
      # create text widget for the table cell
      set cell $frame.r${row}c$col
      text $cell -height $maxheight -width $maxwidth($col) 
      $cell configure -background [lindex \
		[$textwidget configure -background] 4]
      $cell configure -highlightbackground [lindex \
		[$textwidget configure -background] 4]
      $cell configure -borderwidth $bw
      foreach {n v} [array get _omfhb_g *$textwidget] {
	set _omfhb_g($n.table$_omfhb_g(tct).r${row}c$col) $v
      }
      $cell configure -wrap none
      _OMFHB_RenderMaxTextSequence text $cell
      $cell configure -state disabled
      grid $cell -row $row -column $col -sticky nsew
      $textwidget see $frame
      update
      set wfrac [lindex [$cell xview] 1]
      set hfrac [lindex [$cell yview] 1]
      if {$hfrac == 0.0} {
        set hfrac 0.2
      }
      set width [expr {int(ceil($maxwidth($col)/$wfrac))}]
      set height [expr {int(ceil($maxheight/$hfrac))}]
      $cell configure -width $width -height $height
      if {$width > $maxwidth($col)} {
	set maxwidth($col) $width
      }
      if {$height > $maxheight} {
	set maxheight $height 
      }
      regsub -nocase {^</td[^>]*>} $text {} text
      regsub "^\[ \t\r\n\]+" $text {} text
      incr col
    }
    regsub -nocase {^</tr[^>]*>} $text {} text
    regsub "^\[ \t\r\n\]+" $text {} text
    incr row
  }
  incr _omfhb_g(tct)
  # Extend any active tags to cover table 
  $textwidget tag add $_omfhb_g(anchorname$textwidget) end-2c
  $textwidget tag add $_omfhb_g(indenttag$textwidget) end-2c
  $textwidget tag add $_omfhb_g(rindenttag$textwidget) end-2c
  $textwidget tag add $_omfhb_g(wraptag$textwidget) end-2c
  $textwidget tag add $_omfhb_g(justtag$textwidget) end-2c
  # Strip away the closing </TABLE> tag, if present
  regsub -nocase {^</table[^>]*>} $text {} text
  $textwidget insert end "\n"
  _OMFHB_SetJustification $textwidget $origjust
}

# proc RenderDT textref textwidget
#	text begins with a DT element.  Process it and return
#	the rest of the string.
#
#	Strictly, DT elements should contain only text.  However,
#	a common abuse accepted by most browsers is to place 
#	headings in DT elements to get font scaling.  Thus, such
#	markup is accepted here with a warning.  However, the
#	appearance probably won't be what the author is expecting.
proc _OMFHB_RenderDT {textref textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderDT}
  upvar $textref text
  regexp -nocase {^<dt[^>]*>(.*)$} $text match text
  regsub "^\[ \t\r\n\]+" $text {} text
  _OMFHB_RequireLineBreak $textwidget
  if {[regexp -nocase {^<(h[1-6])[^a-z0-9_.]} $text foo htag]} {
    _OMFHB_StrictWarn "Illegal use of heading &lt;[string toupper $htag]&gt; 
in DT element" $textwidget text
    # An 'Abort' could clear the string
    if {[string length $text]} {
      _OMFHB_RenderHeading text $textwidget
    }
  } elseif {[regexp -nocase {^<hr[^a-z0-9_.]} $text]} {
    _OMFHB_StrictWarn "Illegal use of &lt;HR&gt; in DT element" $textwidget text
    # An 'Abort' could clear the string
    if {[string length $text]} {
      _OMFHB_RenderHR text $textwidget
    }
  } else {
    _OMFHB_RenderMaxTextSequence text $textwidget
  }
  # Strip away the closing </DT> tag, if present
  regsub -nocase {^</dt[^>]*>} $text {} text
  _OMFHB_RequireLineBreak $textwidget
}

# proc RenderDD textref textwidget
#	text begins with a DD element.  Process it and return
#	the rest of the string.
proc _OMFHB_RenderDD {textref textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderDD}
  upvar $textref text
  regexp -nocase {^<dd[^>]*>(.*)$} $text match text
  _OMFHB_RequireLineBreak $textwidget
  incr _omfhb_g(indentlevel$textwidget)
  set _omfhb_g(indenttag$textwidget) indent$_omfhb_g(indentlevel$textwidget)
  _OMFHB_RenderFlow text $textwidget
  incr _omfhb_g(indentlevel$textwidget) -1
  set _omfhb_g(indenttag$textwidget) indent$_omfhb_g(indentlevel$textwidget)
  # Strip away the closing </DD> tag, if present
  regsub -nocase {^</dd[^>]*>} $text {} text
  _OMFHB_RequireLineBreak $textwidget
}

# proc RenderLI textref textwidget bulletgen
#	text begins with a LI element.  Process it and return
#	the rest of the string.
proc _OMFHB_RenderLI {textref textwidget bulletgen} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderLI}
  upvar $textref text
  regexp -nocase {^<li[^>]*>(.*)$} $text match text
  _OMFHB_RequireLineBreak $textwidget
  set bullettext [$bulletgen Next]
  if {[string compare list [$bulletgen Cget -type]]} {
    append bullettext .
  }
  set oldfam $_omfhb_g(family$textwidget)
  set _omfhb_g(family$textwidget) courier
  set oldwt $_omfhb_g(weight$textwidget)
  set oldsl $_omfhb_g(slant$textwidget)
  if {[string length [info commands font]]} {
      set _omfhb_g(weight$textwidget) normal
      set _omfhb_g(slane$textwidget) roman
  } else {
      set _omfhb_g(weight$textwidget) medium
      set _omfhb_g(slane$textwidget) r
  }
  _OMFHB_MakeFontTag $textwidget
  $textwidget insert end "\t$bullettext\t" \
        [list $_omfhb_g(fonttag$textwidget) \
        bullet$_omfhb_g(indentlevel$textwidget)]
  set _omfhb_g(family$textwidget) $oldfam
  set _omfhb_g(weight$textwidget) $oldwt
  set _omfhb_g(slant$textwidget) $oldsl
  _OMFHB_MakeFontTag $textwidget
  incr _omfhb_g(indentlevel$textwidget) 1
  set _omfhb_g(indenttag$textwidget) indent$_omfhb_g(indentlevel$textwidget)
  _OMFHB_RenderFlow text $textwidget
  incr _omfhb_g(indentlevel$textwidget) -1
  set _omfhb_g(indenttag$textwidget) indent$_omfhb_g(indentlevel$textwidget)
  # Strip away the closing </LI> tag, if present
  regsub -nocase {^</li[^>]*>} $text {} text
  _OMFHB_RequireLineBreak $textwidget
}

# proc RenderDLContents textref textwidget
#	text begins with the contents of a DL element.  This should
#	be a sequence of DT and DD elements.  Process it and return
#	the rest of the string.
proc _OMFHB_RenderDLContents {textref textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderDLContents}
  upvar $textref text
  regsub "^\[ \t\r\n\]+" $text {} text
  while {[regexp -nocase {^<d([td])[^a-z0-9._]} $text match type]} {
    switch -exact -- [string tolower $type] {
      t	{_OMFHB_RenderDT text $textwidget}
      d	{_OMFHB_RenderDD text $textwidget}
    }
    regsub "^\[ \t\r\n\]+" $text {} text
  }
  if {$omfhb_debug} {puts renderDLContentsdone}
}

# proc RenderListItemSequence textref textwidget bulletgen
#	text begins with the contents of an OL or UL element.  
#	This should be a sequence of LI elements.  Process it 
#	and return the rest of the string.
proc _OMFHB_RenderListItemSequence {textref textwidget bulletgen} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderListItemSequence}
  upvar $textref text
  regsub "^\[ \t\r\n\]+" $text {} text
  while {[regexp -nocase {^<li[^a-z0-9._]} $text match ]} {
    _OMFHB_RenderLI text $textwidget $bulletgen
    regsub "^\[ \t\r\n\]+" $text {} text
  }
  if {$omfhb_debug} {puts renderListItemSequencedone}
}

# proc RenderDL textref textwidget
#	text begins with a DL element.  Process it and return
#	the rest of the string.
proc _OMFHB_RenderDL {textref textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderDL}
  upvar $textref text
  regexp -nocase {^<dl[^>]*>(.*)$} $text match text
  incr _omfhb_g(listlevel$textwidget)
  while {[regexp . $text]} {
    _OMFHB_RenderDLContents text $textwidget
    if {[regexp -nocase "^</dl" $text]} {break}
    # DL Contents should be processed now, but check for illegal HTML
    if {[regexp "^<(/?\[a-zA-Z\]\[a-zA-Z0-9._\]*)" $text match tag]} {
      set errtext "Tag &lt;[string toupper $tag]&gt;"
    } else {
      set errtext "No tag"
    }
    _OMFHB_StrictWarn \
"$errtext where one of the following is required:
<UL><LI>&lt;DT&gt;<LI>&lt;DD&gt;</UL>
<P>Between &lt;DL&gt; and &lt/DL&gt, only &lt;DT&gt; and &lt;DD&gt; elements
(and their contents) are permitted." $textwidget text
    set charstogo [string length $text]
    # Text could have been cleared by an 'Abort'
    if {$charstogo} {
      # First try inserting a <DD> or <DT> tag
      if {[regexp -nocase {^<h[r1-6][^a-z0-9_.]} $text]} {
        regsub {^(.*)$} $text {<DT>\1} text
      } else {
        regsub {^(.*)$} $text {<DD>\1} text
      }
      _OMFHB_RenderDLContents text $textwidget
      if {$charstogo == [string length $text] } {
        _OMFHB_DisplayInvalidHtml \
"$errtext where one of the following is required:
<UL><LI>&lt;DT&gt;<LI>&lt;DD&gt;</UL>
<P>Between &lt;DL&gt; and &lt/DL&gt, only &lt;DT&gt; and &lt;DD&gt; elements
(and their contents) are permitted." $textwidget 
        set text ""
      }
    }
  }
  incr _omfhb_g(listlevel$textwidget) -1
  if {[regexp . $text]} {
    regexp -nocase "^</dl\[^>\]*>(.*)$" $text match text
  } else {
    _OMFHB_InferClose DL $textwidget text
  }
}

# proc RenderUL textref textwidget
#	text begins with a UL element.  Process it and return
#	the rest of the string.
proc _OMFHB_RenderUL {textref textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderUL}
  upvar $textref text
  regexp -nocase {^<ul[^>]*>(.*)$} $text match text
  incr _omfhb_g(listlevel$textwidget)
  Counter New ctr -type list -list [list [string index \
      $_omfhb_g(bullets) $_omfhb_g(listlevel$textwidget)]]
  while {[regexp . $text]} {
    _OMFHB_RenderListItemSequence text $textwidget $ctr
    if {[regexp -nocase "^</ul" $text]} {break}
  # UL Contents should be processed now, but check for illegal HTML
    if {[regexp "^<(/?\[a-zA-Z\]\[a-zA-Z0-9._\]*)" $text match tag]} {
      set errtext "Tag &lt;[string toupper $tag]&gt;"
    } else {
      set errtext "No tag"
    }
    _OMFHB_StrictWarn \
"$errtext where &lt;LI&gt; is required.
<P>Between &lt;UL&gt; and &lt/UL&gt, only &lt;LI&gt; elements
(and their contents) are permitted." $textwidget text
    set charstogo [string length $text]
    # Text could have been cleared by an 'Abort'
    if {$charstogo} {
      # First try inserting a <LI> tag
      regsub {^(.*)$} $text {<LI>\1} text
      _OMFHB_RenderListItemSequence text $textwidget $ctr
      if {$charstogo == [string length $text] } {
        _OMFHB_DisplayInvalidHtml \
"$errtext where &lt;LI&gt; is required.
<P>Between &lt;UL&gt; and &lt/UL&gt, only &lt;LI&gt; elements
(and their contents) are permitted." $textwidget
        set text ""
      }
    }
  }
  if {$omfhb_debug} {puts renderULdone}
  $ctr Delete
  incr _omfhb_g(listlevel$textwidget) -1
  if {[regexp . $text]} {
    regexp -nocase "^</ul\[^>\]*>(.*)$" $text match text
  } else {
    _OMFHB_InferClose UL $textwidget text
  }
}

# proc RenderOL textref textwidget
#	text begins with a OL element.  Process it and return
#	the rest of the string.
proc _OMFHB_RenderOL {textref textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderOL}
  upvar $textref text
  regexp -nocase {^<ol([^>]*)>(.*)$} $text match attrlist text
  incr _omfhb_g(listlevel$textwidget)
  set type [_OMFHB_GetAttribute type $attrlist]
  if {[regexp {^$} $type]} {
    set type 1
  } 
  switch $type {
    1 {
      Counter New ctr -type integer
    }
    a {
      Counter New ctr -type alphabet -case lower
    }
    A {
      Counter New ctr -type alphabet -case upper
    }
    i {
      Counter New ctr -type roman -case lower
    }
    I {
      Counter New ctr -type roman -case upper
    }
    default {
      Counter New ctr -type integer
    }
  }
  while {[regexp . $text]} {
    _OMFHB_RenderListItemSequence text $textwidget $ctr
    if {[regexp -nocase "^</ol" $text]} {break}
  # OL Contents should be processed now, but check for illegal HTML
    if {[regexp "^<(/?\[a-zA-Z\]\[a-zA-Z0-9._\]*)" $text match tag]} {
      set errtext "Tag &lt;[string toupper $tag]&gt;"
    } else {
      set errtext "No tag"
    }
    _OMFHB_StrictWarn \
"$errtext where &lt;LI&gt; is required.
<P>Between &lt;OL&gt; and &lt/OL&gt, only &lt;LI&gt; elements
(and their contents) are permitted." $textwidget text
    set charstogo [string length $text]
    # Text could have been cleared by an 'Abort'
    if {$charstogo} {
      # First try inserting a <LI> tag
      regsub {^(.*)$} $text {<LI>\1} text
      _OMFHB_RenderListItemSequence text $textwidget $ctr
      if {$charstogo == [string length $text] } {
        _OMFHB_DisplayInvalidHtml \
"$errtext where &lt;LI&gt; is required.
<P>Between &lt;OL&gt; and &lt/OL&gt, only &lt;LI&gt; elements
(and their contents) are permitted." $textwidget
        set text ""
      }
    }
  }
  if {$omfhb_debug} {puts renderOLdone}
  $ctr Delete
  incr _omfhb_g(listlevel$textwidget) -1
  if {[regexp . $text]} {
    regexp -nocase "^</ol\[^>\]*>(.*)$" $text match text
  } else {
    _OMFHB_InferClose OL $textwidget text
  }
}

# proc RenderPRE textref textwidget
#	text begins with a PRE element.  Process it and return
#	the rest of the string.
proc _OMFHB_RenderPRE {textref textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderPRE}
  upvar $textref text
  regexp -nocase {^<pre[^>]*>(.*)$} $text match text
# Here need to set tags to change appearance of text font
  set old [_OMFHB_FixedFont $textwidget]
  set oldw $_omfhb_g(wraptag$textwidget)
  set _omfhb_g(wraptag$textwidget) nowrap
  while {[regexp . $text]} {
    if {[regexp {<[/a-zA-Z]} $text]} {
      regexp {^([^<]*<([^/a-zA-z][^<]*<)*)((/?[a-zA-Z][a-zA-Z0-9._]*).*)$} \
             $text match beforetag foo text tag
      # Take the < off the end of beforetag
      regsub {<$} $beforetag {} beforetag
      _OMFHB_RenderPcdata $beforetag $textwidget
      regsub {^(.*)$} $text {<\1} text
  # proc construction might be better than switch
      switch -exact -- [string tolower $tag] {
  	a	-
	hr	-
	br	{_OMFHB_Render[string toupper $tag]	text $textwidget}
  	/pre	{# All the tags which can legally appear next, but which
  		 # do not continue a PRE element
  		 break
		}
  	default	{# Other tags passed through as literals
                 _OMFHB_RenderPcdata "<$tag" $textwidget
                 regsub -nocase "^<$tag" $text {} text
		}
      }
    } else {
      _OMFHB_RenderPcdata $text $textwidget
      set text ""
    }
  }
# Undo whatever we did
  set _omfhb_g(family$textwidget) $old
  set _omfhb_g(wraptag$textwidget) $oldw
  _OMFHB_MakeFontTag $textwidget
  _OMFHB_RequireEmptyLine $textwidget
  if {[regexp -nocase "^</pre" $text]} {
    regexp -nocase "^</pre\[^>\]*>(.*)$" $text match text
  } else {
    _OMFHB_InferClose PRE $textwidget text
  }
}
  
# proc RenderBlock textref textwidget
#	text begins with a block element.  Process it and return
#	the rest of the string.
#
#	This routine may not be necessary
proc _OMFHB_RenderBlock {textref textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderBlock}
  upvar $textref text
  if {[regexp {^<([a-zA-Z][a-zA-Z0-9._]*)[^>]*>} $text match tag]} {
    switch -exact -- [string tolower $tag] {
	p	-
	dl	-
	ul	-
	ol	-
	blockquote	-
	pre	-
        table   -
	div	{_OMFHB_Render[string toupper $tag]	text $textwidget}
    	default	{return -code break}
    }
  } else {
    return -code break
  }
}

# proc RenderParagraphRecover textref textwidget message
#	When illegal HTML Level 1 is encountered at a high level
#	of structure, an easy fix is often to supply the missing 
#	structure with a <DIV> tag.
proc _OMFHB_RenderParagraphRecover {textref textwidget message} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderParagraphRecover}
  upvar $textref text
  if {[string length $message]} {
      _OMFHB_StrictWarn $message $textwidget text
  }
  set charstogo [string length $text]
  # An 'Abort' after a warning could leave text empty
  # If so, do nothing.
  if {$charstogo} {
    # First try inserting a <DIV> tag
    regsub {^(.*)$} $text {<DIV>\1} text
    # RenderBlock returns a break when done.
    catch {_OMFHB_RenderBlock text $textwidget}
    if {$charstogo == [string length $text] } {
      _OMFHB_DisplayInvalidHtml "\
    <p>Unrecoverable error: $message" $textwidget
      set text ""
    }
  }
}

# proc RenderBodyContent textref textwidget
#	Render the Body Content in text into the textwidget
proc _OMFHB_RenderBodyContent {textref textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts renderBodyContent}
  upvar $textref text
#	An HTML body is a sequence of elements.  Repeatedly process
#	the first element and discard it until string is consumed.
  while {[regexp . $text]} {
    if {$omfhb_debug} {puts BodyContentLoop}
    # First strip away any leading (separating) whitespace.
    regsub "^\[ \t\r\n\]+" $text {} text
    # Each element in the sequence starts with a start tag.
    # Determine which and pass control to a handler routine.
    if {[regexp "^<(/?\[a-zA-Z\]\[a-zA-Z0-9._\]*)" $text match tag]} {
      switch -glob -- [string tolower $tag] {
	/blockquote	-
	/body	{break}
	address	-
	hr	{_OMFHB_RequireEmptyLine $textwidget
		 _OMFHB_Render[string toupper $tag] text $textwidget
		}
	h[1-6]	{_OMFHB_RequireEmptyLine $textwidget
		 _OMFHB_RenderHeading text $textwidget}
	p	-
	blockquote	-
	pre	-
        table   -
	div	{_OMFHB_RenderBlock text $textwidget}
	dl	-
	ul	-
	ol	{_OMFHB_RequireEmptyLine $textwidget
		 _OMFHB_RenderBlock	text $textwidget}
	img	{_OMFHB_RenderParagraphRecover text $textwidget ""}
	default	{_OMFHB_RenderParagraphRecover text \
$textwidget "Tag &lt;[string toupper $tag]&gt; 
where one of the following is required:
<UL><LI>&lt;ADDRESS&gt;<LI>&lt;BLOCKQUOTE&gt;<LI>&lt;DL&gt;<LI>&lt;H1&gt;
<LI>&lt;H2&gt;<LI>&lt;H3&gt;<LI>&lt;H4&gt;<LI>&lt;H5&gt;<LI>&lt;H6&gt;
<LI>&lt;HR&gt;<LI>&lt;IMG&gt;<LI>&lt;OL&gt;<LI>&lt;P&gt;<LI>&lt;PRE&gt;
<LI>&lt;UL&gt;<LI>&lt;TABLE&gt;<LI>&lt;DIV&gt;</UL>"}
      }
    } else {
      _OMFHB_RenderParagraphRecover text $textwidget \
"No tag where one of the following is required:
<UL><LI>&lt;ADDRESS&gt;<LI>&lt;BLOCKQUOTE&gt;<LI>&lt;DL&gt;<LI>&lt;H1&gt;
<LI>&lt;H2&gt;<LI>&lt;H3&gt;<LI>&lt;H4&gt;<LI>&lt;H5&gt;<LI>&lt;H6&gt;
<LI>&lt;HR&gt;<LI>&lt;IMG&gt;<LI>&lt;OL&gt;<LI>&lt;P&gt;<LI>&lt;PRE&gt;
<LI>&lt;UL&gt;<LI>&lt;TABLE&gt;<LI>&lt;DIV&gt;</UL>"
    }
  }
}

# proc Display_text_html contentsref myURL textwidget
#	Displays in the text widget textwidget the contents of contentsref
#	translating HTML into a suitable appearance along the way
proc _OMFHB_Display_text_html {contentsref myURL textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts Display_text_html}
  set _omfhb_g(continue$textwidget) 1
  set _omfhb_g(URLin$textwidget) $myURL
  upvar $contentsref contents
  # Clear away all link and name tags
  set tags [$textwidget tag names]
  foreach tag $tags {
    if {[regexp {^URL:.*$} $tag]} {
      $textwidget tag delete $tag
    }
    if {[regexp {^AN:.*$} $tag]} {
      $textwidget tag delete $tag
    }
  }
  set _omfhb_g(family$textwidget) times
  if {[string length [info commands font]]} {
    set _omfhb_g(weight$textwidget) normal
    set _omfhb_g(slant$textwidget) roman
  } else {
    set _omfhb_g(weight$textwidget) medium
    set _omfhb_g(slant$textwidget) r
  }
  set _omfhb_g(pixels$textwidget) $_omfhb_g(basefontsize)
  set _omfhb_g(offset$textwidget) baseline
  _OMFHB_MakeFontTag $textwidget
  # Configure html appearance
  # The next line shouldn't be needed, but it apparently is:
  $textwidget configure -wrap word
  set _omfhb_g(justtag$textwidget) justifyleft
  set _omfhb_g(wraptag$textwidget) wordwrap
  set _omfhb_g(linktag$textwidget) {}
  set _omfhb_g(linkdest$textwidget) {}
  set _omfhb_g(anchorname$textwidget) {}
  set _omfhb_g(listlevel$textwidget) -1
  set _omfhb_g(indentlevel$textwidget) 0
  set _omfhb_g(indenttag$textwidget) indent$_omfhb_g(indentlevel$textwidget)
  set _omfhb_g(rindentlevel$textwidget) 0
  set _omfhb_g(rindenttag$textwidget) rindent$_omfhb_g(rindentlevel$textwidget)
  set _omfhb_g(inanchor$textwidget) 0
  # Clear the text window
  $textwidget delete 1.0 end
  # Get rid of any SGML declarations (should be only comments and DOCTYPE)
  regsub -all {<!((----)|(--([^-]+-)+-)|(---([^-]+-)+-)|([^>-])|(-[^>-]))*-?>} \
              $contents {} contents

  #Strip leading and trailing whitespace and HTML tags if any
  regsub -nocase "^\[ \t\r\n\]*(<html\[^>\]*>)?\[ \t\r\n\]*" \
                 $contents {} contents
  regsub -nocase "\[ \t\r\n\]*(</html\[ \t\r\n\]*>)?\[ \t\r\n\]*$" \
                 $contents {} contents

  # Strip leading HEAD tag and whitespace, if any
  regsub -nocase "^(<head\[^>\]*>)?\[ \t\r\n\]*" $contents {} contents

  # Now loop looking for elements which are part of the HEAD element
  # If we find the closing HEAD tag, or any tag which doesn't belong
  # to a HEAD element, or any raw text not enclosed in an element,
  # that ends our HEAD processing.
  set title ""
  set body ""
  while {[regexp . $contents]} {
    if {[regexp {^<(/?[a-zA-Z][a-zA-Z0-9._]*)} $contents match tag]} {
      switch -exact -- [string tolower $tag] {
        title {
          if {![regexp -nocase {^<title[^>]*>([^<]*)</title[^>]*>(.*)$} \
              $contents m title contents]} {
            _OMFHB_DisplayInvalidHtml "Broken tag $tag in HEAD element" \
                $textwidget
            set body $contents
            break
          }
        }
        base -
        isindex -
        link -
        meta {
          if {![regexp -nocase "^<$tag\[^>\]*>(</$tag\[^>\]*>)?(.*)$" \
              $contents m foo contents]} {
            _OMFHB_DisplayInvalidHtml "Broken tag $tag in HEAD element" \
                $textwidget
            set body $contents
            break
          }
        }
        /head {
          # closing HEAD tag -- stop HEAD processing
          if {![regexp -nocase "^<$tag\[^>\]*>(.*)$" $contents m body]} {
            _OMFHB_DisplayInvalidHtml "Broken tag $tag in HEAD element" \
                $textwidget
            set body $contents
          }
          break
        }
        default {
          # Tag which doesn't belong in HEAD element
          set body $contents
          break
        }
      }
    } else {
      # Raw text not enclosed in an element
      set body $contents
      break
    }

    # strip away any separating whitespace.
    regsub "^\[ \t\r\n\]+" $contents {} contents
  }

  # Display the page title on window title bar
  wm title [winfo toplevel $textwidget] \
      [join [list $_omfhb_g(title) : " " [_OMFHB_TranslateEscapes $title]] ""]
  # If there is still a </HEAD> tag left, then we stopped before we reached
  # it.  That would only happen if the <HEAD> had illegal content.
#  if {[regexp -nocase </head\[^>\]*> $body]} {
#    _OMFHB_DisplayInvalidHtml "Illegal contents in &lt;HEAD&gt;" $textwidget
#  }
  # If there is a <BODY> tag, skip anything before it (like frame support)
  if {[regexp -nocase -indices <body\[^>\]*> $body idx]} {
    set body [string range $body [lindex $idx 0] end]
  }
  #Strip leading and trailing whitespace and BODY tags if any
  regsub -nocase "^\[ \t\r\n\]*(<body\[^>\]*>)?" \
                 $body {} body
  regsub -nocase "\[ \t\r\n\]*(</body\[ \t\r\n\]*>)?\[ \t\r\n\]*$" \
                 $body {} body 
  # Now need to render the body according to the HTML markup
  _OMFHB_RenderBodyContent body $textwidget
  if {[regexp . $body]} {
    _OMFHB_StrictWarn "Text after &lt;/BODY&gt; tag: $body" $textwidget text
  }
}

# proc Display_text_plain contentsref textwidget
#	Displays in the text widget textwidget as plain text the
#	contents of contentsref
proc _OMFHB_Display_text_plain {contentsref myURL textwidget} {
  global _omfhb_g
  upvar $contentsref contents
  $textwidget configure -wrap none
  $textwidget delete 1.0 end
  wm title [winfo toplevel $textwidget] $_omfhb_g(title)
  $textwidget insert 1.0 $contents
}

# proc GoHistory index textwidget
#	Go to the page at index in the history list.
#	Reset current pointer and enable / disable appropriate buttons.
proc _OMFHB_GoHistory {index textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts goHistory}
  $textwidget configure -cursor watch
  set _omfhb_g(cursor$textwidget) watch
  set cUaS [lindex $_omfhb_g(history$textwidget) $_omfhb_g(current$textwidget)]
  if {[string match {} $cUaS]} {
      foreach {currentURL currentFragment} {{} {}} {}
  } else {
      foreach {currentURL currentFragment} [Oc_Url FromString $cUaS] {}
  }
  # No catch here -- we know data from the history list is good.
  foreach {targetURL targetFragment} [Oc_Url FromString \
      [lindex $_omfhb_g(history$textwidget) $index]] {}
  if {$_omfhb_g(warn)} {
    puts "Loading [$targetURL ToString]#$targetFragment ..."
  }
  set _omfhb_g(current$textwidget) $index
  $textwidget configure -state normal
  # Disable navigation while rendering
  $_omfhb_g(quit$textwidget) configure -state disabled
  $_omfhb_g(contents$textwidget) configure -state disabled
  $_omfhb_g(back$textwidget) configure -state disabled
  $_omfhb_g(forward$textwidget) configure -state disabled
  $_omfhb_g(refresh$textwidget) configure -state disabled
  foreach label {Home Back Forward} {
    set idx [$_omfhb_g(nmenu$textwidget) index $label]
    $_omfhb_g(nmenu$textwidget) entryconfigure $idx -state disabled
  }
  set idx [$_omfhb_g(fmenu$textwidget) index Refresh]
  $_omfhb_g(fmenu$textwidget) entryconfigure $idx -state disabled
  $textwidget tag configure link -foreground black -underline 0
  if {[string compare $targetURL $currentURL]
	|| [string match _OMFHB_Refresh [lindex [info level -1] 0] ] } {
    array set contentinfo [_OMFHB_FetchUrl $targetURL contents] 
    if {[string length [info commands \
        _OMFHB_Display_$contentinfo(type)_$contentinfo(subtype)]]} {
      _OMFHB_Display_$contentinfo(type)_$contentinfo(subtype) contents \
          $targetURL $textwidget
    } else {
      _OMFHB_DisplayUnknownContentType $targetURL $contentinfo(type) \
                                $contentinfo(subtype) $textwidget
    }
  }
  if {[regexp . $targetFragment]} {
    # Use catch in case the named anchor doesn't exist
    if {[catch {$textwidget see [lindex [$textwidget tag ranges \
            AN:$targetFragment] 0]}]} {
        $textwidget see 1.0
    }
  } else {
      $textwidget see 1.0
  }
  $textwidget configure -state disabled -cursor top_left_arrow
  set _omfhb_g(cursor$textwidget) top_left_arrow
  # Enable navigation
  $textwidget tag configure link -foreground blue -underline 1
  $_omfhb_g(quit$textwidget) configure -state normal
  $_omfhb_g(contents$textwidget) configure -state normal
  $_omfhb_g(refresh$textwidget) configure -state normal
  set idx [$_omfhb_g(nmenu$textwidget) index Home]
  $_omfhb_g(nmenu$textwidget) entryconfigure $idx -state normal
  set idx [$_omfhb_g(fmenu$textwidget) index Refresh]
  $_omfhb_g(fmenu$textwidget) entryconfigure $idx -state normal
  if {!$_omfhb_g(current$textwidget)} {
    $_omfhb_g(back$textwidget) configure -state disabled
    set idx [$_omfhb_g(nmenu$textwidget) index Back]
    $_omfhb_g(nmenu$textwidget) entryconfigure $idx -state disabled
  } else {
    $_omfhb_g(back$textwidget) configure -state normal
    set idx [$_omfhb_g(nmenu$textwidget) index Back]
    $_omfhb_g(nmenu$textwidget) entryconfigure $idx -state normal
  }
  if {$_omfhb_g(current$textwidget) == [llength $_omfhb_g(history$textwidget)] - 1} {
    $_omfhb_g(forward$textwidget) configure -state disabled
    set idx [$_omfhb_g(nmenu$textwidget) index Forward]
    $_omfhb_g(nmenu$textwidget) entryconfigure $idx -state disabled
  } else {
    $_omfhb_g(forward$textwidget) configure -state normal
    set idx [$_omfhb_g(nmenu$textwidget) index Forward]
    $_omfhb_g(nmenu$textwidget) entryconfigure $idx -state normal
  }
  if {$_omfhb_g(quitrequest$_omfhb_g(quit$textwidget))} {
    set _omfhb_g(quitrequest$_omfhb_g(quit$textwidget)) 0
    $_omfhb_g(quit$textwidget) invoke
  }
}

# proc GoBack textwidget
#	Go back one page in the history list
#	Be sure to disable button when this makes no sense
proc _OMFHB_GoBack {textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts goBack}
  _OMFHB_GoHistory [expr {$_omfhb_g(current$textwidget) - 1}] $textwidget
}

# proc GoForward textwidget
#	Go forward one page in the history list
#	Be sure to disable button when this makes no sense
proc _OMFHB_GoForward {textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts goForward}
  _OMFHB_GoHistory [expr {$_omfhb_g(current$textwidget) + 1}] $textwidget
}

# proc GoHome textwidget
#	Go to the Home page.
proc _OMFHB_GoHome {textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts goHome}
  set URL [lindex $_omfhb_g(history$textwidget) 0]
  set _omfhb_g(history$textwidget) [lrange [ \
      linsert $_omfhb_g(history$textwidget) \
      [expr {$_omfhb_g(current$textwidget) + 1}] $URL] \
      0 [expr {$_omfhb_g(current$textwidget) + 1}]]
  _OMFHB_GoHistory [expr {$_omfhb_g(current$textwidget) + 1}] $textwidget
}

# proc Refresh textwidget
#	Re-render the current page
proc _OMFHB_Refresh {textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts Refresh}
  _OMFHB_GoHistory $_omfhb_g(current$textwidget) $textwidget
}

# proc Open textwidget
#	Open a file.
proc _OMFHB_Open {textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts Open}
  set f [tk_getOpenFile -filetypes {{HTML {.html .htm}} {{All files} {*}}} \
    -parent $textwidget]
  if {[string length $f]} {
    _OMFHB_GoToUrl [Oc_Url FromFilename $f] "" $textwidget
  }
}

# proc GoToURL url fragment textwidget
#       Go to a URL, managing browser history as we go.
proc _OMFHB_GoToUrl {url fragment textwidget} {
    global _omfhb_g omfhb_debug
    if {$omfhb_debug} {puts gotoURL}
    set linkcolor [$textwidget tag cget link -foreground]
    if {[string compare blue $linkcolor]} {return}
    set _omfhb_g(history$textwidget) \
            [lrange [linsert $_omfhb_g(history$textwidget) \
            [expr {$_omfhb_g(current$textwidget) + 1}] \
            [$url ToString]#$fragment] 0 \
            [expr {$_omfhb_g(current$textwidget) + 1}]]      
    _OMFHB_GoHistory [expr {$_omfhb_g(current$textwidget) + 1}] $textwidget
}

# proc FollowLink textwidget x y
#	follow the hyperlink found at coords x,y in textwidget
proc _OMFHB_FollowLink {textwidget x y} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts followLink}
  set tags [$textwidget tag names @$x,$y]
  set URL [lindex $tags [lsearch -glob $tags URL:*]]
  regsub ^URL: $URL {} URL
  eval _OMFHB_GoToUrl [split $URL #] $textwidget
}

# proc DisplayLink textwidget x y
#	Display the destination URL of the hyperlink at coords x,y
proc _OMFHB_DisplayLink {textwidget url frag} {
  global _omfhb_g 
  set _omfhb_g(cursor$textwidget) [$textwidget cget -cursor]
  $textwidget config -cursor hand2
  $_omfhb_g(link$textwidget) configure -state normal
  $_omfhb_g(link$textwidget) delete 0 end
  switch $frag {
      {} {
          $_omfhb_g(link$textwidget) insert 0 "Link: [$url ToString]"
      }
      default {
          $_omfhb_g(link$textwidget) insert 0 "Link: [$url ToString]#$frag"
      }
  }
  $_omfhb_g(link$textwidget) configure -state disabled
}

proc _OMFHB_DisplayLinkXY {textwidget x y} {
  set tags [$textwidget tag names @$x,$y]
  set URL [lindex $tags [lsearch -glob $tags URL:*]]
  regsub ^URL: $URL {} URL
  foreach {url frag} [split $URL #] {}
  _OMFHB_DisplayLink $textwidget $url $frag
}

# proc EraseLink textwidget
#	Erase the destination URL from status bar
proc _OMFHB_EraseLink {textwidget} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts EraseLink}
  $textwidget config -cursor $_omfhb_g(cursor$textwidget)
  if {0} {
    # Turn this block on to erase URL from Link: line.
    $_omfhb_g(link$textwidget) configure -state normal
    $_omfhb_g(link$textwidget) delete 0 end
    $_omfhb_g(link$textwidget) insert 0 "Link:"
    $_omfhb_g(link$textwidget) configure -state disabled
  }
}

# proc Quit win btn
#	Quit this browser only if the button btn is active.
#	Wait for any rendering to complete first.
proc _OMFHB_Quit {win btn} {
  global _omfhb_g
  set state [$btn cget -state]
  if {[string compare $state disabled] == 0} {
    set _omfhb_g(quitrequest$btn) 1
  } else {
    exit
  }
}

# Proc FontScale textwidget
#     Allow the user to interactive change the overall font scaling.
proc _OMFHB_FontScale { textwidget } {
    global _omfhb_g
    set result [Ow_PromptDialog  "[Oc_Main GetAppName]: Font Scale" \
            "Enter relative font scaling:" $_omfhb_g(fontscale)]
    if {[lindex $result 1]==0} {
	# Remove all current font tags, as identified by
	# .baseline, .sup or .sub suffixes.
	foreach tag [$textwidget tag names] {
	    if {[regexp -- {\.baseline$|\.sup$|\.sub$} $tag]} {
		$textwidget tag delete $tag
	    }
	}

	# Change scaling
	set _omfhb_g(fontscale) [lindex $result 0]

	# Redisplay
	_OMFHB_Refresh $textwidget
    }
}

# proc Browser tableofcontentsURL
# 	The "main" procedure for starting up the on-line help window.
proc OMFHB_Browser {tableofcontentsURL {fragment {}}} {
  global _omfhb_g omfhb_debug
  if {$omfhb_debug} {puts HelpBrowser:Browser}
  if {[catch {toplevel .hb_$_omfhb_g(id)} result]} {
    # Apparently trying to open a second help browser with same ID
    # Change this warning to a dialog box or something
    return -code error -errorinfo "Can't do that!"
  } 
  incr _omfhb_g(id)
  set browser $result
  set tw [text $browser.text -background white -insertwidth 0]
  set _omfhb_g(link$tw) [entry $browser.link -relief flat \
	-font [Oc_Font Get bold]]
  $_omfhb_g(link$tw) insert 0 "Link:"
  $_omfhb_g(link$tw) configure -state disabled
  set buttons [frame $browser.buttons]
  set _omfhb_g(quit$tw) [button $buttons.quit -text Exit]
  $_omfhb_g(quit$tw) configure -command [list _OMFHB_Quit $browser $_omfhb_g(quit$tw)]
  wm protocol [winfo toplevel $browser] WM_DELETE_WINDOW \
          [list _OMFHB_Quit $browser $_omfhb_g(quit$tw)]
  set _omfhb_g(contents$tw) [button $buttons.contents -text Home\
				-command [list _OMFHB_GoHome $tw] \
				-state disabled]
  set _omfhb_g(back$tw) [button $buttons.back -text Back \
				-command [list _OMFHB_GoBack $tw] \
				-state disabled]
  set _omfhb_g(forward$tw) [button $buttons.forward -text Forward \
				-command [list _OMFHB_GoForward $tw] \
				-state disabled]
  set _omfhb_g(refresh$tw) [button $buttons.refresh -text Refresh \
				-command [list _OMFHB_Refresh $tw] \
				-state disabled]
  set yscroll [scrollbar $browser.yscroll \
                                     -command [list $tw yview] \
                                     -orient vertical]
  set xscroll [scrollbar $browser.xscroll \
                                     -command [list $tw xview] \
                                     -orient horizontal]
  $tw configure -yscrollcommand [list $yscroll set] \
                -xscrollcommand [list $xscroll set]

if {[catch {
  set mb $browser.menubar
  foreach [list _omfhb_g(fmenu$tw) _omfhb_g(nmenu$tw) _omfhb_g(omenu$tw) \
          _omfhb_g(hmenu$tw)] [Ow_MakeMenubar $browser $mb File Navigate \
          Options Help] {}
  Ow_StdHelpMenu $_omfhb_g(hmenu$tw)
}]} {
  set menubar [frame $browser.menubar -borderwidth 2 -relief raised]
  set fmb [menubutton $browser.file -text "File" -menu $browser.file.menu \
                                    -underline 0]
  set _omfhb_g(fmenu$tw) [menu $browser.file.menu -tearoff 0]
  set nmb [menubutton $browser.navigate -text "Navigate" -underline 0 \
                                        -menu $browser.navigate.menu]
  set _omfhb_g(nmenu$tw) [menu $browser.navigate.menu -tearoff 0]
  set omb [menubutton $browser.options -text "Options" -underline 0 \
                                        -menu $browser.options.menu]
  set _omfhb_g(omenu$tw) [menu $browser.options.menu -tearoff 0]
  pack $fmb $nmb $omb -side left -in $menubar
  pack $menubar -side top -fill x
}
  $_omfhb_g(fmenu$tw) add command -label "Open..." \
      -command [list _OMFHB_Open $tw] -underline 0
  $_omfhb_g(fmenu$tw) add command -label "Refresh" \
      -command [list _OMFHB_Refresh $tw] -underline 0
  $_omfhb_g(fmenu$tw) add separator
  $_omfhb_g(fmenu$tw) add command -label "Exit" -underline 1 \
                     -command [list _OMFHB_Quit $browser $_omfhb_g(quit$tw)]
  $_omfhb_g(nmenu$tw) add command -label "Home" \
      -command [list _OMFHB_GoHome $tw] -underline 0
  $_omfhb_g(nmenu$tw) add command -label "Back" \
      -command [list _OMFHB_GoBack $tw] -underline 0 -state disabled
  $_omfhb_g(nmenu$tw) add command -label "Forward" \
      -command [list _OMFHB_GoForward $tw] -underline 0 -state disabled
if {![catch {Ow_IsInteger 0}]} {
  $_omfhb_g(omenu$tw) add command -label "Font scale..." \
      -command [list _OMFHB_FontScale $tw] -underline 0
}
# HTML generated by latex2html does not meet our "Strict" requirements,
# so disable this option for the time being.
  set offerStrict 0
  Oc_Option Get Menu offerStrict offerStrict
  if {[catch {expr {$offerStrict && $offerStrict}}]} {
      Oc_Log Log "Bad value for Oc_Option 'Menu offerStrict':\
              $offerStrict\n    (should be a boolean)" error
  } elseif {$offerStrict} {
      $_omfhb_g(omenu$tw) add check -label "Strict" \
              -variable _omfhb_g(strict$tw) -underline 0
  }

  pack $_omfhb_g(quit$tw) -padx 25 -pady 10 -side right
  pack $_omfhb_g(contents$tw) -padx 2 -pady 10 -side left
  pack $_omfhb_g(back$tw) -padx 2 -pady 10 -side left
  pack $_omfhb_g(forward$tw) -padx 2 -pady 10 -side left
  pack $_omfhb_g(refresh$tw) -padx 2 -pady 10 -side left
  pack $buttons -side top -fill x
  pack $_omfhb_g(link$tw) -side top -fill x
  pack $yscroll -side right -fill y
  pack $xscroll -side bottom -fill x
  pack $tw -side left -fill both -expand 1
#  pack $browser -side top -fill both -expand 1

  # Windows seems to need this help to enable selections
  focus $tw

  set _omfhb_g(quitrequest$_omfhb_g(quit$tw)) 0
  set _omfhb_g(history$tw) [list]  ;# Work around for Tcl 8.0p0 bug
  set _omfhb_g(current$tw) -1
  set _omfhb_g(strict$tw) 0

  # Tag configurations
  $tw tag configure link -foreground blue -underline 1
  $tw tag bind link <Enter> {_OMFHB_DisplayLinkXY %W %x %y}
  $tw tag bind link <Leave> {_OMFHB_EraseLink %W}
  $tw tag bind link <1> {after 0 {_OMFHB_FollowLink %W %x %y}}
  $tw tag configure deadlink -foreground red -underline 1
  $tw tag bind deadlink <Enter> {_OMFHB_DisplayLinkXY %W %x %y}
  $tw tag bind deadlink <Leave> {_OMFHB_EraseLink %W}
  $tw tag bind deadlink <1> {after 0 {_OMFHB_FollowLink %W %x %y}}
  $tw tag configure justifyleft -justify left
  $tw tag configure justifycenter -justify center
  $tw tag configure justifyright -justify right
  $tw tag configure wordwrap -wrap word
  $tw tag configure nowrap -wrap none
  for {set i 0} {$i < $_omfhb_g(maxtabs)} {incr i} {
    set lm [expr {(6 * $i + 4) * $_omfhb_g(tabstop)}]
    set tab1 [expr {$lm + 5 * $_omfhb_g(tabstop)}]
    set tab2 [expr {$lm + 6 * $_omfhb_g(tabstop)}]
    $tw tag configure indent$i -lmargin1 ${lm}c -lmargin2 ${lm}c
    $tw tag configure rindent$i -rmargin ${lm}c
    $tw tag configure bullet$i -lmargin1 0c -lmargin2 0c \
	-tabs [list ${tab1}c right ${tab2}c left]
  }
  set lm [expr {4 * $_omfhb_g(tabstop)}]
  $tw tag configure rule -borderwidth 2 -relief groove \
        -tabs [winfo width $tw]
  if {[catch {$tw tag configure rule \
	-font -*-courier-medium-r-*-*-2-*-*-*-*-*-*-*}]} {
    $tw tag configure rule -font -*-courier-medium-r-*-*-10-*-*-*-*-*-*-*
  }
  _OMFHB_GoToUrl $tableofcontentsURL $fragment $tw
  bind $tw <Configure> {%W tag configure rule -tabs [expr {%w - 10}]}
    catch {Ow_SetIcon $browser}
  return $browser
}

if {[string length $url]} {
    foreach {url fragment} [Oc_Url FromString $url] {}
} else {
    # Default page = OOMMF Userguide TOC
    set url [Oc_Url FromFilename \
	    [file join $omfhb_library .. doc userguide userguide \
	    userguide.html]]
    set fragment ""
}

OMFHB_Browser $url $fragment
vwait forever

########################################################################
########################################################################
#
# End of the non-boilerplate code.
#
########################################################################
########################################################################

# The following line is the end of the [catch] command which encloses the
# entire main body of this script.
} msg]
#
########################################################################

########################################################################
# Check the result of the enclosing [catch] and construct any error
# message to be reported.
#
if {$code == 0} {
    # No error; we're done
    exit 0
} elseif {$code == 1} {
    # Unexpected error; Report whole stack trace plus errorCode
    set rei $errorInfo
    set rec $errorCode
} elseif {$code != 2} {
    # Weirdness -- just pass it on
    return -code $code $msg
} else {
    # Expected error; just report the message
    set rei $msg
    set rec $errorCode
}
#
########################################################################

########################################################################
# Try various error reporting methods
#
# First, use Oc_Log if it is available.
set src ""
if {[string match OC [lindex $rec 0]]} {
    set src [lindex $rec 1]
}
set errorInfo $rei
set errorCode $rec
if {![catch {Oc_Log Log $msg panic $src}]} {
    # Oc_Log reported the error; now die
    exit 1
}

# If we're on Windows using pre-8.0 Tk, we have to report the message
# ourselves.  Use [tk_dialog].
if { [array exists tcl_platform]
	&& [string match windows $tcl_platform(platform)]
	&& [string length [package provide Tk]]
	&& [package vsatisfies [package provide Tk] 4]} {
    wm withdraw .
    option add *Dialog.msg.wrapLength 0
    tk_dialog .error "<[pid]> [info script] error:" $rei error 0 Die
    exit 1
}

# Fall back on Tcl/Tk's own reporting of startup script errors
error $msg "\n<[pid]> [info script] error:\n$rei" $rec
#
########################################################################
