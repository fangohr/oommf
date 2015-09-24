### Bug fixes for LaTeX2HTML Version 2002-2-1 (1.71) #####################

# An apparent bug in the definition of the following two variables in
# /usr/bin/latex2html results in the occasional dropping of
# "mathend000#" into the HTML output.  Ideally latex2html.pin should be
# patched to fix this, but inasmuch as LaTeX2HTML development appears
# dead patching it here may be the best we can do. (-mjd, 2007-03-08)
# USE: Place this l2hbugs.perl and an empty l2hbugs.sty in the same
#      directory as the main source file (e.g., userguide.tex), and
#      put "\usepackage{l2hbugs}" in the main source file, after the
#      \documentclass command.  This must be done for each main
#      source file.  It would be nice to stuff this into
#      doc/common/oommfhead.tex, but for complicated reasons (including
#      the l2h image generation pass) I haven't been able to get that
#      to work.  Anyway, because of this, be sure to duplicate any
#      changes to l2hbugs.perl in both copies of l2hbugs.perl, i.e.,
#      the copy in doc/userguide and doc/progman.
$math_verbatim_rx = "$verbatim_mark#?math(\\d+)#";
$mathend_verbatim_rx = "$verbatim_mark#?mathend([^#]*)#";

1;	# This must be the last line
