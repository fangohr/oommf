# FILE: makerules.tcl
#
# This file controls the application 'pimake' by defining rules which
# describe how to build the applications and/or extensions produced by
# the source code in this directory.
#
# Verify that this script is being sourced by pimake
if {[llength [info commands MakeRule]] == 0} {
    error "'[info script]' must be evaluated by pimake"
}
#
########################################################################

MakeRule Define {
    -targets            doc
    -dependencies       [list progman.pdf progman.ps \
			        [file join progman progman.html]]
}

MakeRule Define {
    -targets            [file join progman progman.html]
    -dependencies       progman.dvi
    -script	{
	Oc_Exec Foreground latex2html \
               -init_file latex2html-init progman.tex
	file copy -force ../common/contents.gif progman
	file copy -force ../common/oommficon.gif progman
    }
}

MakeRule Define {
    -targets            progman.ps
    -dependencies       progman.dvi
    -script	{
		DeleteFiles progman.ps
		Oc_Exec Foreground dvips -Ppdf -f progman.dvi > progman.ps
		}
}

MakeRule Define {
    -targets            progman.dvi
    -dependencies       [glob -nocomplain *.tex [file join .. common *.tex]]
    -script		{
			eval DeleteFiles [glob -nocomplain *.aux]
			eval DeleteFiles [glob -nocomplain *.bbl]
			eval DeleteFiles [glob -nocomplain *.ind]
			eval DeleteFiles [glob -nocomplain *.toc]
			Oc_Exec Foreground latex progman
			Oc_Exec Foreground bibtex progman
			Oc_Exec Foreground latex progman
                        Oc_Exec Foreground makeindex progman.idx
			Oc_Exec Foreground latex progman
			}
}

MakeRule Define {
    -targets            progman.pdf
    -dependencies       [glob -nocomplain *.tex [file join .. common *.tex]]
    -script		{
			eval DeleteFiles [glob -nocomplain *.aux]
			eval DeleteFiles [glob -nocomplain *.bbl]
			eval DeleteFiles [glob -nocomplain *.ind]
			eval DeleteFiles [glob -nocomplain *.toc]
			Oc_Exec Foreground pdflatex progman
			Oc_Exec Foreground bibtex progman
			Oc_Exec Foreground pdflatex progman
                        Oc_Exec Foreground makeindex progman.idx
			Oc_Exec Foreground pdflatex progman
			}
}

MakeRule Define {
    -targets            maintainer-clean
    -dependencies       distclean
    -script		{
			DeleteFiles progman progman.ps progman.pdf
			}
}
 
MakeRule Define {
    -targets            distclean
    -dependencies       {}
    -script		{
			eval DeleteFiles [glob -nocomplain *.aux]
			eval DeleteFiles [glob -nocomplain *.bbl]
			eval DeleteFiles [glob -nocomplain *.blg]
			eval DeleteFiles [glob -nocomplain *.dvi]
			eval DeleteFiles [glob -nocomplain *.idx]
			eval DeleteFiles [glob -nocomplain *.ind]
			eval DeleteFiles [glob -nocomplain *.ilg]
			eval DeleteFiles [glob -nocomplain *.log]
			eval DeleteFiles [glob -nocomplain *.out]
			eval DeleteFiles [glob -nocomplain *.toc]
			}
}

MakeRule Define {
    -targets		upgrade
    -script		{DeleteFiles biblio.tex}
}

