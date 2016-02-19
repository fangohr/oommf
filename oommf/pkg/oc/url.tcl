# FILE: url.tcl
#
# Each instance of the Oc_Class Oc_Url is a Uniform Resource Locator (URL).
# URLs are best known by their representation as strings, such as
#
#	http://math.nist.gov/oommf/	
#
# but here they are stored as objects.  The string representation is
# constructed in (almost[*]) strict compliance with RFC 1738 and RFC 1808, 
# but strings are parsed less strictly due to the large number of 
# non-compliant URLs found in documents available on the Internet.
#
# URLs play the role of addresses in the location of objects (resources)
# on the Internet.  At any given time, a URL locates at most one resource.
#
# We may want to move this class into the net extension.
#
#------------------------------------------------------------------------
# [*] The exception to the following of RFC 1738 and RFC 1808 is in
# the string representation of URLs with the file: scheme.  RFC 1738
# spells out the syntax for these URLs as
#
#     file://<host>/<path>
#
# but older versions of Internet Explorer (< 4.0) will not accept file:
# URLs in that form.  To support exchange of file: URLs with these
# buggy old releases of Internet Explorer, the Oc_Class Oc_Url supports
# a configuration option through its public common 'msieCompatible'.
# By default, msieCompatible has the value 0, and file: URLs strictly
# follow the RFCs.  If the Oc_Option command is used like so:
#
#	Oc_Option Add * Oc_Url msieCompatible 1
#
# to set the msieComptible option of the Oc_Url class to 1, then
# the Oc_Url class will export its file: URLs in the form:
#
#     file:/<path>
#
# which is acceptable to Internet Explorer, as well as other browsers we
# tried.  The Oc_Option command is usually placed in the file
# .../oommf/config/local/options.tcl .
#------------------------------------------------------------------------
# DOME:
#	Fill in empty methods
#	Add support for non-generic URLs
#	Change code so escaping is done only on the string representation,
#		not also on the stored components -- should improve efficiency
#
# Last modified on: $Date: 2007/03/21 01:02:42 $
# Last modified by: $Author: donahue $

Oc_Class Oc_Url {

    public common msieCompatible 0

    array common cache 
    common input
    common baseURL

    const public variable scheme = http
    const public variable netloc
    const public variable abspath = ""
    const public variable params = ""
    const public variable query = ""

    const private variable stringRep
    const public variable isGeneric

    Constructor {args} {
        if {![info exists input]} {
            error "Do not call '$class New' directly.  Use '$class FromString'."
        }
        eval $this Configure $args
        # Construct canonical form of the URL string representation
        # Remove "//", "/." and "/foo/.." from innards of paths
        while {[regsub // $abspath / abspath]} {}
        while {[regsub {(/\./)|(/\.$)} $abspath / abspath]} {}
        while {[regsub [join { 
			(/[^/]/\.\./)|
			(/[^/][^/][^/]+/\.\./)|
			(/[^/][^/.]/\.\./)|
			(/[^/.][^/]/\.\./)|
			(/[^/]/\.\.$)|
			(/[^/][^/][^/]+/\.\.$)|
			(/[^/][^/.]/\.\.$)|
			(/[^/.][^/]/\.\.$)
		       } ""] $abspath / abspath]} {}
        if {$isGeneric} {
            if {[string match file $scheme] && $msieCompatible} {
                # Use the non-standard string representation for file:
                # URLs required by Internet Explorer
                set stringRep $scheme:$abspath$params$query
            } else {
                set stringRep $scheme://$netloc$abspath$params$query
            }
        } else {
            set stringRep [$this SR[string toupper $scheme]]
        }
        if {![catch {set cache(,$stringRep)} existingURL]} {
            $this Delete
            array set cache [list $baseURL,$input $existingURL]
            lappend cache(:$existingURL) $baseURL,$input
            unset input
            unset baseURL
            return $existingURL
        }
        array set cache [list $baseURL,$input $this]
        array set cache [list :$this $baseURL,$input]
        unset input
        unset baseURL
    }

    method Contents {} {}

    method Fetch {} {}

    method IsAccessible {} {scheme} {
        return [string match file $scheme]
    }

    private method SRMAILTO {} {netloc} {
        return mailto:$netloc
    }

    method ToFilename {} {
        # Determine and return a Tcl-useable filename from a file: URL
        if {![string match file $scheme]} {
            error "Can't determine local filename from URL 
	<$stringRep>"
        }
        set epcomps [split [string range $abspath 1 end] /]
        if {[string match localhost $netloc]} {
            set froot [$class Unescape [lindex $epcomps 0]]
        } else {
            set froot  //[$class Unescape $netloc]
            append froot /[$class Unescape [lindex $epcomps 0]]
        }
        if {![string match absolute [file pathtype $froot]]} {
            append froot /
        }
        set efn [eval file join [list $froot] [lrange $epcomps 1 end]]
        set fn [$class Unescape $efn]
        if {![string match absolute [file pathtype $fn]]} {
            set fn /$fn
        }
        if [file isdirectory $fn] {
            return [file join $fn index.html]
        }
        return $fn
    }

    method ToString {} { stringRep } {
        return $stringRep
    }

    Destructor {
        if {![catch {set cache(:$this)} idxlist]} {
            foreach idx $idxlist {
                unset cache($idx)
            }
            unset cache(:$this)
        }
    }

    proc Escape {string} {
        # Escapes any characters in $string which aren't legal in a URL.
        # The set of characters which are legal in the string representation
        # of a URL is very small.  All other ASCII characters may only be
        # legally included within a URL by the use of an escape sequence.
        # The escape sequence is the '%' character followed by two hexadecimal
        # digits (hexadigits?) which represent the byte value of the
        # escaped ASCII character.
        # First escape all % signs, so we know from now on any %'s we see came
        # from escaping.
        regsub -all % $string %25 string
        # Escape all '[' characters, which would otherwise break the call to
        # 'subst' below.
        regsub -all {\[} $string %5b string
        # Escape all other characters except the legal character set for URLs.
        regsub -all {[^a-zA-Z0-9$_+.!*'(),;/?:@&=%-]} $string \
                {%[scan {&} %c v; format %02x $v]} string
        set string [subst -nobackslashes -novariables $string]
        return $string
    }

    proc FromFilename {filename} {
        # Reject non-absolute and tilde-substituted filenames
        if {![string match absolute [file pathtype $filename]] \
                || [string match ~* $filename]} {
            error "Can't construct URL from filename <$filename>:
	Filename must be absolute."
        }
        # Split the filename into its components
        # The operation of 'file split' is platform dependent.
        # It not only splits the filename, but it cleans up the
        # components in the list it returns into just a few forms.
        # For example, extra separators are removed, backslashes are
        # converted to forward slashes under Windows, and the root
        # component of a UNC-style filename under Windows is put
        # in the form "//host/export" no matter what type or how
        # many slashes appear in the original filename string.
        # Thus, all possible absolute filenames on all platforms are
        # reduced to a small number of cases.
        set fncomps [file split $filename]
        set froot [lindex $fncomps 0]
        if {[regexp {^//([^/]+)/([^/]+)} $froot m netloc export]} {
            # Case 1 (Windows): //host/mnt/foo/bar		- filename
            #                    --> {//host/mnt foo bar}	- fncomps
            #                    --> file://host/mnt/foo/bar	- URL
            set pcomps [list $export]
        } else {
            # Case 2 (Unix):    /foo/bar			- filename
            #                    --> {/ foo bar}		- fncomps
            #                    --> file://localhost/foo/bar	- URL
            # Case 3 (Windows): C:/foo/bar			- filename
            #                    --> {C:/ foo bar}		- fncomps
            #                    --> file://localhost/C%3A/foo/bar	- URL
            # Case 4 (Mac):     Folder:File:Foo			- filename
            #                    --> {Folder: File Foo}		- fncomps
            #                    --> file://localhost/Folder%3A/File/Foo - URL
            # Case 4 is based on the Tcl man pages and source code,
            # not actual testing on a Mac platform.  Also, the URL results
            # in Case 4 may not be in agreement with WWW browsers for
            # Mac platforms.  If not, another case may need to be added.
            set netloc localhost
            set pcomps [string trimright $froot /]
            if {[llength $pcomps] > 1} {
                set pcomps [list $pcomps]
            }
        }
        eval lappend pcomps [lrange $fncomps 1 end]
        # Now pcomps is a list of components of the path section of
        # the URL.  Those components have special escaping requirements.
        set epcomps {}
        foreach pcomp $pcomps {
            set string [$class Escape $pcomp]
	    #
	    # It appears the IE 6 cannot load URLs like
	    #	file://localhost/C%3a/foo/bar.txt
	    # Reviewing RFCs 1738 and 1808, I don't see
	    # a requirement to escape the : character.
	    # I can imagine in a relative URL, it might
	    # cause confusion with the portion before the
	    # first unescaped : being mistaken for a scheme,
	    # but that's not relevant here because we're
	    # constructing an absolute URL.  Removing this
	    # extra escape.
            #regsub -all : $string %3a string
            regsub -all / $string %2f string
            regsub -all {;} $string %3b string
            regsub -all {[?]} $string %3f string
            lappend epcomps $string
        }
        # Append a trailing '/' on directories.
        if {[file isdirectory $filename]} {
            set abspath /[join $epcomps /]/
        } else {
            set abspath /[join $epcomps /]
        }
        set input file://$netloc$abspath
        set baseURL ""
        return [$class New _\
                -scheme file \
                -netloc $netloc \
                -abspath $abspath \
                -params "" \
                -query "" \
                -isGeneric 1]
    }

    # Create a new URL object from a string representation.
    # Raises an error if the string cannot be (forgivingly) parsed as a 
    # legal URL + fragment.
    # Returns a two-element list.  The first element is the name of
    # the new Oc_Url object holding the URL.  The second element is
    # the fragment part of the string input.
    proc FromString {string {base {}}} {
        # First determine the fragment part.
        if {![regexp {^(.*)#([^#]*)$} $string m string fragment]} {
            set fragment ""
        }
        set input $string
        set baseURL $base
        # URLs should contain no white space.  Forgive that non-compliance
        # by removing any white space.
        regsub -all "\[ \t\r\n\]" $string {} string
        # First, try looking up in the cache
        if {![catch {set cache($base,$string)} retval]} {
            return [list $retval $fragment]
        }
        # If there could be escape sequences in the URL, unescape it,
        # so we don't end up double escaping anything.
        if {[regexp % $string]} {
            set string [$class Unescape $string]
        }
        # Escape the entire string so that it contains no characters which
        # cannot appear anywhere in a URL.
        set string [$class Escape $string]
        # Try interpreting string as an absolute URL first.
        # Determine the scheme, and based on it parse the rest of the string
        if [regexp {^([a-zA-Z0-9+.-]+):(.*)$} $string m scheme rest] {
            # $string looks like an absolute URL.  
            # Ignore $base in further processing
            set baseURL {}
            # '//' indicates a URL using the common URL syntax for
            # IP-based protocol schemes
            if {[string match //* $rest]} {
                return [list [$class ParseGeneric $scheme $rest] $fragment]
            } elseif {[catch \
                    {$class Parse[string toupper $scheme] $rest} retval]} {
                error "Can't construct URL from string 
	<$input>:
	Handler for non-generic scheme '$scheme' not implemented."
            } else {
                return [list $retval $fragment]
            }
        } else {
            # $string isn't absolute URL.  Try to resolve relative to base.
            if {![catch {$base Class} bclass] \
                    && [string match $class $bclass]} {
                return [list [$class Resolve $string $base] $fragment]
            }
            if {[string length $base]} {
                error "Illegal argument 'base':
	$base is not the name of a $class."
            } else {
                error "Can't construct URL from string <$input>:
	Unable to determine scheme (protocol)."
            }
        }
    }

    # Resolve the string as a relative URL, relative to $base.
    private proc Resolve {string base} {
        if {![$base Cget -isGeneric]} {
            error "Can't construct URL from string <$input>:
	Unable to resolve non-generic relative URLs."
        }
        if {[string match //* $string]} {
            return [$class ParseGeneric [$base Cget -scheme] $string]
        }
        if {[string match /* $string]} {
            return [$class ParseGenericRelPath [$base Cget -scheme] \
                [$base Cget -netloc] [string range $string 1 end]]
        }
        regexp {^([^;?]*)?(;[^?]*)?(\?.*)?$} $string m path params query
        if {[string match {} $path]} {
            set abspath [$base Cget -abspath]
            if {[string match {} $params]} {
                set params [$base Cget -params]
                if {[string match {} $query]} {
                    set query [$base Cget -query]
                }
            }
        } else {
            regsub {/[^/]*$} [$base Cget -abspath] / abspath
            append abspath $path
        }
        return [$class New _\
                -scheme [$base Cget -scheme] \
                -netloc [$base Cget -netloc] \
                -abspath $abspath \
                -params $params \
                -query $query \
                -isGeneric 1]
    }

    # Parses the schemepart of the so-called "generic-RLs".
    # Generic-RLs are those URLs whose scheme makes use of the
    # IP schemepart syntax (see RFC 1738 and RFC 1808).
    # Raises error if schemepart cannot be parsed according to that syntax.
    # Returns Oc_Url object if it can.
    private proc ParseGeneric {scheme schemepart} {
        if {[regexp {^//([^/]*)(/.*)?$} $schemepart m netloc abspath]} {
            set relpath $abspath
            while {[regsub ^/ $relpath {} relpath]} {}
            return [$class ParseGenericRelPath $scheme $netloc $relpath]
        } else {
            error "Can't construct URL from string <$input>:
	Unable to determine host."
        }
    }

    private proc ParseGenericRelPath {scheme netloc relpath} {
        regexp {^([^;?]*)?(;[^?]*)?(\?.*)?$} $relpath m path params query
        return [$class New _\
                -scheme $scheme \
                -netloc $netloc \
                -abspath /$path \
                -params $params \
                -query $query \
                -isGeneric 1]
    }

    private proc ParseMAILTO {schemepart} {
        return [$class New _ -scheme mailto -netloc $schemepart -isGeneric 0]
    }

    # This handles the non-standard file: URLs favored by Internet Explorer
    # by supplying the missing "//localhost"
    private proc ParseFILE {schemepart} {
        set relpath $schemepart
        while {[regsub ^/ $relpath {} relpath]} {}
        return [$class ParseGenericRelPath file localhost $relpath]
    }

    # Inverse of Escape
    #   
    #	NOTE: This routine does not detect and report illegal escape
    #   sequences.  It just passes them through.  Because of this, it
    #   is possible to construct a string for which [Oc_Url Unescape $string]
    #   is not the same as [Oc_Url Unescape [Oc_Url Unescape $string]].
    #   (Example: a%A%65).  Checking for this error in the argument would be
    #   expensive, and very rarely useful.
    proc Unescape {string} {
        # For safety, escape any non-escaped '[' characters.
        # (A compliant URL will not have any.)  If any are present,
        # they will break the invocation of 'subst' below.
        regsub -all {\[} $string %5b string
        # Convert all %HH sequences to the corresponding character
        regsub -all {%([0-9a-fA-F][0-9a-fA-F])} $string \
	        {[scan \1 %x v; format %c $v]} string
        set string [subst -nobackslashes -novariables $string]
        return $string
    }
}
