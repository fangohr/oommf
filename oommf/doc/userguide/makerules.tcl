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
    -dependencies       [list userguide.pdf userguide.ps \
			        [file join userguide userguide.html]]
}

MakeRule Define {
    -targets            [file join userguide userguide.html]
    -dependencies       userguide.dvi
    -script	{
	eval DeleteFiles [glob -nocomplain [file join userguide img*]]
	Oc_Exec Foreground latex2html \
               -init_file latex2html-init userguide.tex
	file copy -force ../common/contents.gif userguide
	file copy -force ../common/oommficon.gif userguide
    }
}

MakeRule Define {
    -targets            userguide.ps
    -dependencies       userguide.dvi
    -script	{
		DeleteFiles userguide.ps
		Oc_Exec Foreground dvips -Ppdf -f userguide.dvi > userguide.ps
		}
}

MakeRule Define {
    -targets            userguide.dvi
    -dependencies       [glob -nocomplain *.tex *.bib \
	                             [file join .. common *.tex]]
    -script		{
			eval DeleteFiles [glob -nocomplain *.aux]
			eval DeleteFiles [glob -nocomplain *.bbl]
			eval DeleteFiles [glob -nocomplain *.ind]
			eval DeleteFiles [glob -nocomplain *.toc]
			Oc_Exec Foreground latex userguide
			Oc_Exec Foreground bibtex userguide
			Oc_Exec Foreground latex userguide
                        Oc_Exec Foreground makeindex userguide.idx
			Oc_Exec Foreground latex userguide
			}
}

MakeRule Define {
    -targets            userguide.pdf
    -dependencies       [glob -nocomplain *.tex [file join .. common *.tex]]
    -script		{
			eval DeleteFiles [glob -nocomplain *.aux]
			eval DeleteFiles [glob -nocomplain *.bbl]
			eval DeleteFiles [glob -nocomplain *.ind]
			eval DeleteFiles [glob -nocomplain *.toc]
			Oc_Exec Foreground pdflatex userguide
			Oc_Exec Foreground bibtex userguide
			Oc_Exec Foreground pdflatex userguide
                        Oc_Exec Foreground makeindex userguide.idx
			Oc_Exec Foreground pdflatex userguide
			}
}

MakeRule Define {
    -targets            maintainer-clean
    -dependencies       distclean
    -script		{
			DeleteFiles userguide userguide.ps userguide.pdf
			}
}
 
MakeRule Define {
    -targets		distclean
    -dependencies	{}
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
			eval DeleteFiles [glob -nocomplain \
				[file join userguide images.*]]
			}
}

MakeRule Define {
    -targets		upgrade
    -script		{eval DeleteFiles \
			    [glob -nocomplain *.html *.css *.gif *.pl]
			 DeleteFiles images.tex
			 DeleteFiles batchsys-body.tex
			 DeleteFiles batchsys.tex
			 DeleteFiles mmsolve2d-body.tex
			 DeleteFiles mmsolve2d.tex
			 DeleteFiles oxsii-body.tex
			 DeleteFiles oxsii.tex
			 DeleteFiles images.tex
			 DeleteFiles [file join userguide 2D_Micromagnetic_Solver_mmS.html]
			 DeleteFiles [file join userguide Batch_Scheduling_System.html]
			 DeleteFiles [file join userguide Data_table_format_ODT.html]
			 DeleteFiles [file join userguide OOMMF_Batch_System.html]
			 DeleteFiles [file join userguide Problem_specification_forma.html]
			 DeleteFiles [file join userguide Solver_Batch_Interface_batc.html]
			 DeleteFiles [file join userguide fileformats.html]
			 DeleteFiles [file join userguide images.pl]
			 DeleteFiles [file join userguide vectorfieldformat.html]
			}
}

