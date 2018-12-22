# Utility procs
proc TweakSpace { tststr aindex bindex } {
   # Expand aindex bindex block so that removing the block
   # retains the text layout.
   set leadchar 0 ;# Not whitespace
   while {$aindex>0} {
      set i [expr {$aindex-1}]
      set ch [string index $tststr $i]
      if {[string compare $ch " "]!=0 && [string compare $ch "\t"]!=0} {
         break
      }
      set aindex $i
      set leadchar 1 ;# Space or tab
   }
   if {$i>0} {
      set i [expr {$aindex-1}]
      if {[string compare [string index $tststr $i] "\n"]==0} {
         set aindex $i
         set leadchar 2 ;# Newline
      }
   }
   set length [string length $tststr]
   if {$bindex < 0} {
      # No trailing match
      if {$leadchar>0} { incr aindex }
      return [list $aindex -1]
   } else {
      incr bindex
      set tailchar 0 ;# Not whitespace
      while {$bindex<$length} {
         set ch [string index $tststr $bindex]
         if {[string compare $ch " "]!=0 && [string compare $ch "\t"]!=0} {
            break
         }
         incr bindex
         set tailchar 1 ;# Space or tab
      }
      if {$bindex<$length} {
         if {[string compare [string index $tststr $bindex] "\n"]==0} {
            incr bindex
            set tailchar 2 ;# Newline
         } elseif {[string compare \
                       [string range $tststr $bindex [expr {$bindex+1}]] \
                       "%\n"]==0} {
            # Treat "%\n" same as "\n"
            incr bindex 2
            set tailchar 2
         }
      }
   }
   if {$tailchar == 2} {
      incr bindex -1
   } elseif {$leadchar>0} {
      incr aindex
   } elseif {$tailchar>0} {
      incr bindex -1
   }
   # Whitespace handling cases:
   # IF Trailing newline: move outside match
   # ELSE Leading whitespace: move one character outside match
   # ELSE Trailing whitespace: move one character outside match
   return [list $aindex $bindex]
}

proc MatchBracketPair { tststr start_index lead open_bracket close_bracket } {
   # Returns the first index at or after start_index of a substring
   # exactly matching "lead" followed immediately by open_bracket,
   # and the index of the matching close_bracket.  (Return value is
   # a two item list.)  Returns {-1 -1} if not opening match is found,
   # or {n -1} with n>=0 if the opening match is found but no closing
   # match.
   # Note 1: Doesn't distinguish between bare and escaped brackets
   # Note 2: Code assumes open and close brackets are single characters
   set index_a [string first "${lead}${open_bracket}" $tststr $start_index]
   if {$index_a<0} { return [list -1 -1] }
   set level 1
   set index_b [string length $tststr]
   set i [expr {$index_a + [string length "${lead}${open_bracket}"]}]
   while {$i<$index_b} {
      set ch [string index $tststr $i]
      # For extension to multi-character brackets, change "string index"
      # to "string range" with the length of the range matching the
      # length of the close bracket.  If the open and close brackets
      # have different length, then a second string range operation
      # would then be needed for matching against open_bracket.
      if {[string compare $close_bracket $ch]==0} {
         incr level -1
         if {$level == 0} {
            return [list $index_a $i]
         }
      }
      if {[string compare $open_bracket $ch]==0} {
         incr level
      }
      incr i
   }
   return [list $index_a -1]
}

proc MatchBrackets { tststr start_index lead bracket_pair_list } {
   # Same as MatchBracketPair, but supports matching of multiple bracket
   # sets.  The last argument is a list of two-element lists, with each
   # pair being an open/close set.  For example
   #
   #    MatchBrackets $foo 0 {\broccoli} {{\{ \}}}
   #
   # is the same as calling the prior MatchBracketPair like so
   #
   #    MatchBracketPair $foo 0 {\broccoli} \{ \}
   #
   # To match a command like
   #
   #    \cabbage[red]{steamed}{boiled}
   # use
   #
   #    MatchBrackets $foo 0 {\cabbage} {{[ ]} {\{ \}} {\{ \}}}
   #
   # The return value is a list of length n+1, where n is the number of
   # bracket sets.  The first value on the return list is the index to
   # the first matching character of the lead string, the second is
   # the index of the first closing bracket, the third is the index
   # of the second closing bracket, and so on.
   #   If a match is not fully successful, then the last element
   # of the return list will be -1.  In this case the return list
   # may be short (i.e., less than n+1).
   foreach {lb rb} [lindex $bracket_pair_list 0] break
   set indices [MatchBracketPair $tststr $start_index $lead $lb $rb]
   if {[lindex $indices end] < 0} {
      return $indices
   }
   foreach pair [lrange $bracket_pair_list 1 end] {
      set offset [lindex $indices end]
      if {$offset<0} { break }
      incr offset
      foreach {lb rb} $pair break
      set nextpair [MatchBracketPair $tststr $offset {} $lb $rb]
      lappend indices [lindex $nextpair 1]
   }   
   return $indices
}

proc StripBracket { data lead open_bracket close_bracket } {
   # Removes all occurrences of lead+open/close block in data string.
   # Whitespace is tweaked to retain text layout.
   set index 0
   set outstr {}
   while {$index>=0} {
      foreach {aindex bindex} \
         [MatchBracketPair $data $index $lead \
             $open_bracket $close_bracket] break
      if {$aindex<0} {
         append outstr [string range $data $index end]
         break
      }
      foreach {aindex bindex} [TweakSpace $data $aindex $bindex] break
      append outstr [string range $data $index [expr {$aindex - 1}]]
      set index $bindex
   }
   return $outstr
}

proc StripBracketWrapper { data lead open_bracket close_bracket } {
   # Replaces all occurrences of lead+open/close block in data string
   # with contents of the brackets.
   #
   # WARNING: This will break constructs that rely on the surrounding
   # brackets to delineate elements.  For example
   #
   #     \CmdA\latex{B}foo
   #
   # transforms to
   #
   #     \CmdABfoo
   #
   # which might not be what you want.
   set index 0
   set outstr {}
   while {$index>=0} {
      foreach {aindex bindex} \
         [MatchBracketPair $data $index $lead \
             $open_bracket $close_bracket] break
      if {$aindex<0} {
         append outstr [string range $data $index end]
         break
      }
      append outstr [string range $data $index [expr {$aindex - 1}]]
      incr aindex [string length "$lead$open_bracket"]
      if {$bindex>0} {
         # $bindex should never be 0
         append outstr [string range $data $aindex [expr {$bindex - 1}]]
         incr bindex
      } else {
         append outstr [string range $data $aindex end]
      }
      set index $bindex
   }
   return $outstr
}

proc MatchBlock { tststr start_index openstr closestr } {
   # Searches for first occurrence after start_index of openstr (exact
   # match).  Then finds the first occurrence after that point of the
   # closestr.  This code assumes no nesting of the open/close strings.
   # The return value is a two element list consisting of the open and
   # closed index point, expanded slightly to incorporate spaces and
   # tabs up to one newline about the matched region, tweaked slightly
   # so that removing all characters between the first returned list
   # element and up to and including the second element retains the text
   # layout.
   set aindex [string first $openstr $tststr $start_index]
   if {$aindex<0} { return [list -1 -1] }
   set bindex [string first $closestr $tststr $aindex]
   if {$bindex>=0} {
      incr bindex [expr {[string length $closestr]-1}]
   }
   return [TweakSpace $tststr $aindex $bindex]
}

proc StripBlock { data openstr closestr } {
   # Removes all occurrences of open/close block in data string
   set index 0
   set outstr {}
   while {$index>=0} {
      foreach {aindex bindex} \
         [MatchBlock $data $index $openstr $closestr] break
      if {$aindex<0} {
         append outstr [string range $data $index end]
         break
      }
      append outstr [string range $data $index [expr {$aindex - 1}]]
      set index $bindex
   }
   return $outstr
}

proc StripBlockWrapper { data openstr closestr } {
   # Removes wrapper code surrounding each occurrence of open/close
   # block in data string
   set index 0
   set outstr {}
   while {$index>=0} {
      foreach {aindex bindex} \
         [MatchBlock $data $index $openstr $closestr] break
      if {$aindex<0} {
         append outstr [string range $data $index end]
         break
      }
      # Copy data up to outstr
      if {[string index $data $aindex] eq "\n"} {
         append outstr [string range $data $index $aindex]
      } else {
         append outstr [string range $data $index [expr {$aindex - 1}]]
      }
      
      # Determine boundaries of interior, and copy interior to outstr
      set aindex [string first $openstr $data $aindex] ;# Skip leading ws
      incr aindex [expr {[string length $openstr]+1}]
      if {$bindex>0} {
         set mindex [string last $closestr $data $bindex] ;# Skip trailing ws
         incr mindex -1
         set ch [string index $data $mindex]
         while {$ch eq " " || $ch eq "\t"} {
            # Skip over space/tab leader in front of closestr
            incr mindex -1
            set ch [string index $data $mindex]
         }
         if {[string index $data $mindex] == "\n" \
                && [string index $data $bindex] == "\n"} {
            incr $bindex -1  ;# Collapse trailing double newlines
                             ## into a single newline.
         }
      } else {
         set mindex [string length $data]
      }
      append outstr [string range $data $aindex [expr {$mindex-1}]]
      
      # Reset for next pass
      set index $bindex
   }
   return $outstr
}

   
# Step 0: Read data
set data [read stdin]

# Affected constructs:
# \begin{rawhtml}..\end{rawhtml}
# \begin{htmlonly}..\end{htmlonly}
# \htmlimage{..}
# \html{..}
#
# \begin{latexonly}..\end{latexonly}
# %begin{latexonly}..%end{latexonly}
# \latexonly{..}
# \latex{..}
#
# \latexhtml{..tex..}{..html..}
#
# \hyperrefhtml{html}{pre-tex}{post-tex}{label}
# \htmlref{html}{label}
# \htmlonlyref{html}{label}
#
# ref notes:
# We probably want something like
#
#   \usepackage[pdftex, colorlinks=true, citecolor=blue]{hyperref}
#
# in oommfhead.tex.  This package redefines a bunch of stuff, so
# it should probably be one of the last usepackage commands.
# With this package the standard LaTeX \ref command will generate
# a link on the number \ref produces, e.g.,
#   
#    See Section~\ref{sec:foo} for details
#
# will put a colored box around the \ref{sec:foo} value and set
# a link to the boxed area.  If you use \autoref instead then
# text like "Section~4" is generated and the link set to that,
# so sample text would be like
#   
#    See \autoref{sec:foo} for details
#
# However, this is gives less flexibility to the author, and in
# some circumstances the auto-generated context text (Section,
# Figure, Theorem, etc.) may be wrong.  So it is probably better
# to convert
#
#   \hyperrefhtml{html}{pre-tex}{post-tex}{label}
# 
# to
#
#   \hyperref[label]{pre-tex\ref*{label}post-tex}
#
# with the understanding that the link will cover the full pre-tex +
# number + post-tex text.  If necessary, trim in a post-processing
# stage.
#
# Note that the \hyperref command has args
#
#  \hyperref[label]{text}
#
# which is the reverse order from that used by \htmlref and
# \htmlonlyref.  I suggest we cross fingers and convert both
# of these to \hyperref with a simple arg order swap.

set data [StripBlock $data {\begin{rawhtml}} {\end{rawhtml}}]
set data [StripBlock $data {\begin{htmlonly}} {\end{htmlonly}}]
set data [StripBracket $data {\htmlimage} \{ \}]
set data [StripBracket $data {\html} \{ \}]

set data [StripBlockWrapper $data {\begin{latexonly}} {\end{latexonly}}]
set data [StripBlockWrapper $data {%begin{latexonly}} {%end{latexonly}}]
set data [regsub -all {\\latexonly} $data {\latex}]
set data [StripBracketWrapper $data {\latex} \{ \}]

# Convert \latexhtml{..tex..}{..html..} to ..tex..
set index 0
set outstr {}
set leader {\latexhtml}
set bpl {{\{ \}} {\{ \}}}  ;# Two sets of brace pairs
while {$index>=0} {
   set ipts [MatchBrackets $data $index $leader $bpl]
   if {[lindex $ipts 0]<0} {
      append outstr [string range $data $index end]
      break ;# No more matches
   }
   if {[lindex $ipts end]<0} {
      error "Invalid $leader construct"
   }
   foreach {ia ib ic} $ipts break
   set text [string range $data \
                [expr {$ia+[string length $leader]+1}] [expr {$ib-1}]]
   append outstr [string range $data $index [expr {$ia-1}]]
   append outstr $text
   set index [expr {$ic+1}]
}
set data $outstr

# Convert \hyperrefhtml statements to \hyperref
set index 0
set outstr {}
set leader {\hyperrefhtml}
set bpl {{\{ \}} {\{ \}} {\{ \}} {\{ \}}}  ;# Four sets of brace pairs
while {$index>=0} {
   set ipts [MatchBrackets $data $index $leader $bpl]
   if {[lindex $ipts 0]<0} {
      append outstr [string range $data $index end]
      break ;# No more matches
   }
   if {[lindex $ipts end]<0} {
      error "Invalid $leader construct"
   }
   foreach {ia ib ic id ie} $ipts break
   set pretex [string range $data [expr {$ib+2}] [expr {$ic-1}]]
   set posttex [string range $data [expr {$ic+2}] [expr {$id-1}]]
   set label [string range $data [expr {$id+2}] [expr {$ie-1}]]
   append outstr [string range $data $index [expr {$ia-1}]]
   append outstr "\\hyperref\[$label]{$pretex\\ref*{$label}$posttex}"
   set index [expr {$ie+1}]
}
set data $outstr

# Convert \htmlref and \htmlonlyref to \hyperref
set data [regsub -all {\\htmlonlyref} $data {\htmlref}]
set index 0
set outstr {}
set leader {\htmlref}
set bpl {{\{ \}} {\{ \}}}  ;# Two sets of brace pairs
while {$index>=0} {
   set ipts [MatchBrackets $data $index $leader $bpl]
   if {[lindex $ipts 0]<0} {
      append outstr [string range $data $index end]
      break ;# No more matches
   }
   if {[lindex $ipts end]<0} {
      error "Invalid $leader construct"
   }
   foreach {ia ib ic} $ipts break
   set text [string range $data \
                [expr {$ia+[string length $leader]+1}] [expr {$ib-1}]]
   set label [string range $data [expr {$ib+2}] [expr {$ic-1}]]
   append outstr [string range $data $index [expr {$ia-1}]]
   append outstr "\\hyperref\[$label]{$text}"
   set index [expr {$ic+1}]
}
set data $outstr

# Use listings package instead of verbatim + upquotes
# Replace \begin{verbatim}...\end{verbatim}
#    with \begin{lstlisting}...\end{lstlisting}
#      or \begin{lstlisting}[language=Tcl]...\end{lstlisting}
# and
# Replace \verb+...+
#    with \lstinline+...+
#      or \lstinline[language=Tcl]+...+
# where '+' is an an arbitrary character not appearing in ...
#
# Language choices include: C, C++, Tcl, sh, bash, csh, make, HTML, gnuplot
# Default options can be set via the \lstset command.
#
# NOTE: To get \verbatim type output, include these settings:
#    \lstset{
#      basicstyle=\ttfamily,
#      columns=fixed,
#      fontadjust=true,
#      basewidth=0.5em
#    }
# For some unknown to me reason, the default basewidth in the listings
# package is 0.6em, whereas 0.5em matches what verbatim does.
#
# An alternative, which is perhaps better, is
#    \lstset{
#      basicstyle=\ttfamily,
#      columns=fullflexible,
#      keepspaces=true
#    }
# See section "2.10 Fixed and flexible columns" of the listings package
# documentation.  I think columns=fullflexible honors the typeface
# design, which will be less ugly if you switch to a non-monospace font.
#
# To get typewriter-style single quotes, include also the upquote=true
# option (which requires \usepackage{textcomp}).
#
# The listings package also supports user-defined styles, for example
#
#  \lstdefinestyle{MIF}{
#    basicstyle=,
#    language=Tcl,
#    columns=fixed,
#    basewidth=0.55em,
#    morekeywords={Specify},
#    deletekeywords={script}
#  }
#  \begin{lstlisting}[style=MIF]
#   Specify Oxs_Demag {}
#  \end{lstlisting}
#
# Note that the standard \ttfamily does not have bold, so you probably
# don't want to use that with keyword highlighting.  Use "\basicstyle=,"
# to reset to the default font.  BTW, to select both the font family and
# the size, do something like "\basicstyle=\ttfamily\small".
#
# After some exploration, here are some options for code listings:
#

# 1) Use verbatim.  Cons: Single quotes are bad and no boldface
#
# 2) \usepackage{upquote} + verbatim: Cons: no boldface and no upquote
#    support in LaTeXML
#
# 3) \usepackage{listings} + \usepackage{textcomp} +
#      \lstset{basicstyle=\ttfamily,
#              columns=fixed,
#              basewidth=0.5em,
#              upquote=true,
#              showstringspaces=false}
#    with \begin{lstlisting}...\end{lstlisting}
#  Pros: Supported by LaTeXML, good quotes.  Cons: No boldface
#
# 4) \usepackage{listings} + \usepackage{textcomp} +
#      \lstset{
#         basicstyle=,
#         columns=fixed,
#         basewidth=0.525em,
#         upquote=true,
#         showstringspaces=false
#      }
#    with \begin{lstlisting}...\end{lstlisting}
#  Pros: Supported by LaTeXML, boldface.
#  Cons: Bad double quotes, ugly character kerning
#
# 5) Same as 4 but with additionally \usepackage[T1]{fontenc}
#  Pros: Supported by LaTeXML, boldface, good quotes.
#  Cons: Ugly character kerning
#
# 6) \usepackage{lmodern}
#    \usepackage[T1]{fontenc}
#    \usepackage{listings}
#    \usepackage{textcomp} % Needed for upquote=true option in listings
#    \lstset{
#       basicstyle=\ttfamily,
#       columns=fixed,
#       basewidth=0.5em,
#       upquote=true,
#       showstringspaces=false
#    }
#    with \begin{lstlisting}...\end{lstlisting}
#  Pros: Supported by LaTeXML, good quotes, proper monospaced font
#  Cons: Bold font only slightly darker than non-bold
#
# 7) Same as 6 but inside lstset use
#       basicstyle=\ttfamily\fontseries{l}\selectfont,
#  Pros: Same as 6, plus bold font is obvious
#  Cons: Regular font is a little light.
#
# 8) Yet another tt font: inconsolata, which is apparently part of
# TeXLive.  It can be used with or without \usepackage[T1]{fontenc}.  By
# default the zero is slashed; use the var0 to get unslashed variant.
# There is a varl option to get a variant lowercase l, and a scaled=x
# option if you want to tweak the size.  Also, by default this package
# allow spacing to stretch and shrink.  You can use the option mono to
# get a true monospace font, although that is irrelevant inside the
# lstlisting environment if the lstlisting columns setting is "fixed".
# See inconsolata-doc for more information and options.
#    \usepackage[varqu]{inconsolata}
#    \usepackage{listings}
#    \usepackage{textcomp} % Needed for upquote=true option in listings
#    \lstset{
#       basicstyle=\ttfamily,
#       columns=fixed,
#       basewidth=0.5em,
#       upquote=true,
#       showstringspaces=false
#    }
#    with \begin{lstlisting}...\end{lstlisting}
#  Pros: Good quotes, proper monospaced font, good bolding.
#  Cons: Monospaced font?
#  Also inconsolata is sans-serif.  Is that good or bad?
#
# WRT PDF output, DGP prefers 5 and 6 over 1 and 7.  He prefers 5 over 6
# except that the double quotes in 5 are too close together to be
# clearly legible when lmodern is used.  Option 5 without lmodern is
# probably OK, although with or without lmodern MJD finds that in some
# places 5 runs characters together (e.g. "+0" and "DOG"); this could be
# tweaked by increasing basewidth, but then it most text becomes rather
# airy.
#
# All considered, MJD likes 8 best.
#
## NOTE: Instead of \lstset one can wrap settings inside a
#       \lstdefinestyle definition.
#
set data [regsub -all {\\begin{verbatim}} $data {\begin{lstlisting}}]
set data [regsub -all {\\end{verbatim}} $data {\end{lstlisting}}]
set data [regsub -all {\\verb} $data {\lstinline}]

puts -nonewline $data 

