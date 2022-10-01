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
# Notes on LaTeX .aux files:
#
# Both latex and pdflatex create and use .aux files when run.  Both will
# fail if run with an existing userguide.aux from the other, so it is
# safest to delete the .aux files before starting a build with either
# latex or pdflatex.
#
# The build process consists of three runs of latex or pdflatex, with
# bibtex run between the first and second, and makeindex between the
# second and third. The userguide.aux file changes with each run of
# latex. Interestingly, the userguide.aux file created by the first run
# of pdflatex has its timestamp updated by each subsequent run of
# pdflatex, but the contents don't change.  I don't know if this is true
# for any .tex input, or if the current state of userguide is special.
#
# LaTeX2HTML reads the userguide.aux file, and if not found complains with:
#
#  The userguide.aux file was not found, so sections will not be numbered
#  and cross-references will be shown as icons.
#
# The sections aren't numbered in our LaTeX2HTML output, but figures are
# numbered and references to the figures are unnumbered if the build is
# missing any of the relevant *.aux files. Also, latex2html has some
# difficulty parsing the pdflatex *.aux files, so it is probably best to
# use *.aux built by latex. In practice, the latex2html rule below tries
# to check that userguide.aux was built by latex, and if so assumes that
# all the other *.aux files exist and were built by latex.
#
# As a side note, latex2html also uses the bibliography file
# userguide.bbl, which is created by bibtex via userguide.aux.
#
# One other issue concerning latex2html. It complains a little if given
# a userguide.aux from pdflatex, but it complains a lot if given a
# userguide.aux file built with \usepackage{hyperref}, from either latex
# or pdflatex. Incidentally, some notes online recommend making
# \usepackage{hyperref} the last or nearly last \usepackage, presumably
# because of command collisions.
#

########################################################################
#
# Phony targets: doc maintainer-clean distclean clean upgrade
#                 :pdf :ps :latex2html :latexml

# Make all
MakeRule Define {
   -targets            doc
   -dependencies       [list :pdf :ps :latex2html :latexml]
}
## Note: :latex2html requires .aux from the latex run, so it is best if
##       :latex2html immediately follows :ps on the above dependencies
##       list.

# PDF target. Note: Two passes of pdflatex may be needed after makeindex
# to get Index included in the table of contents.
MakeRule Define {
   -targets            userguide.pdf
   -dependencies       [glob -nocomplain *.tex *.bib \
                           [file join .. common *.tex]]
   -script              {
      DeleteFiles {*}[glob -nocomplain *.aux]
      DeleteFiles {*}[glob -nocomplain *.bbl]
      DeleteFiles {*}[glob -nocomplain *.ind]
      DeleteFiles {*}[glob -nocomplain *.toc]
      Oc_Exec Foreground pdflatex userguide
      Oc_Exec Foreground bibtex userguide
      Oc_Exec Foreground pdflatex userguide
      Oc_Exec Foreground makeindex userguide
      Oc_Exec Foreground pdflatex userguide
      Oc_Exec Foreground pdflatex userguide
   }
}

MakeRule Define {
   -targets         :pdf
   -dependencies    userguide.pdf
}


# Postscript target. Note: Two passes of latex may be needed after
# makeindex to get Index included in the table of contents.
MakeRule Define {
   -targets            userguide.ps
   -dependencies       userguide.dvi
   -script      {
      DeleteFiles userguide.ps
      Oc_Exec Foreground dvips -Ppdf -f userguide.dvi > userguide.ps
   }
}

MakeRule Define {
   -targets            userguide.dvi
   -dependencies       [glob -nocomplain *.tex *.bib \
                           [file join .. common *.tex]]
   -script              {
      DeleteFiles {*}[glob -nocomplain *.aux]
      DeleteFiles {*}[glob -nocomplain *.bbl]
      DeleteFiles {*}[glob -nocomplain *.ind]
      DeleteFiles {*}[glob -nocomplain *.toc]
      Oc_Exec Foreground latex userguide
      Oc_Exec Foreground bibtex userguide
      Oc_Exec Foreground latex userguide
      Oc_Exec Foreground makeindex userguide
      Oc_Exec Foreground latex userguide
      Oc_Exec Foreground latex userguide
   }
}

MakeRule Define {
   -targets         :ps
   -dependencies    userguide.ps
}


# LaTeX2HTML target
MakeRule Define {
   -targets         [file join userguide userguide.html]
   -dependencies    [glob -nocomplain *.tex [file join .. common *.tex]]
   -script      {
      # latex2html needs the *.aux files, and prefers the version
      # created by latex, not pdflatex. Rebuild userguide.dvi if
      # necessary to get good builds of *.aux. (Check userguide.aux,
      # and assume all other *.aux files are the same vintage.)
      set f userguide
      set auxtime [set dvitime [set pdftime 0]]
      if [file exists $f.aux] { set auxtime [file mtime $f.aux] }
      if [file exists $f.dvi] { set dvitime [file mtime $f.dvi] }
      if [file exists $f.pdf] { set pdftime [file mtime $f.pdf] }
      if { $dvitime < $auxtime || $auxtime <= $pdftime } {
         # Rebuild .aux files
         # Note: Rebuild cases include when .aux exists but
         #       neither .dvi nor .pdf do.
         # Note: The rebuilding process relies on the FindRule proc
         #       and Invoke method of MakeRule, which are perhaps not
         #       expected to be used outside of the pimake
         #       application.  Changes to the behavior of those
         #       routines could break this code.
         puts "*** Rebuilding $f.aux ***"
         DeleteFiles $f.dvi
         set targetpath [Oc_DirectPathname $f.dvi]
         if {![catch {MakeRule FindRule $targetpath} rule] \
                && ![string match {} $rule]} {
            if {![catch {$rule Invoke $targetpath}]} {
               # .ps will now test as out-of-date, so rebuild it too.
               DeleteFiles $f.ps ;# Invoke compares current time of
               ## target to cached times of dependencies. If we
               ## don't delete .ps then Invoke .ps may not run.
               set targetpath [Oc_DirectPathname $f.ps]
               if {![catch {MakeRule FindRule $targetpath} rule] \
                      && ![string match {} $rule]} {
                  catch {$rule Invoke $targetpath} ;# Rebuild .ps
               }
            }
         }
      }
      # latex2html also needs the userguide.bbl bibliography file.
      # This can be constructed by bibtex provided userguide.aux is
      # available.
      if {![file exists $f.bbl]} {
         Oc_Exec Foreground bibtex userguide
      }

      DeleteFiles {*}[glob -nocomplain [file join userguide img*]]
      Oc_Exec Foreground latex2html \
         -init_file latex2html-init userguide.tex
      file copy -force ../common/contents.gif userguide
      file copy -force ../common/oommficon.gif userguide
    }
}

# Shortcut target for latex2html build. We might want to include a version
# test to short-circuit builds with known bad versions of latex2html.
if {[llength [auto_execok latex2html]]} {
   MakeRule Define {
      -targets         :latex2html
      -dependencies    [file join userguide userguide.html]
   }
} else {
   MakeRule Define {
      -targets         :latex2html
      -dependencies    {}
      -script {
         puts stderr \
            "\n*** Skipping target :latex2html (missing executable) ***\n"
      }
   }
}


# LaTeXML target
# Rules for latexml .tex -> .xml -> .html processing.
# Unlike latex2html, which has side dependencies on the .bbl and .aux
# files from the latex build, the only dependencies for latexml are the
# *.tex files.
MakeRule Define {
   -targets            userguide.xml
   -dependencies       [glob -nocomplain *.tex [file join .. common *.tex] \
                           [file join .. common xmlextras *.ltxml]]
   -script      {
       Oc_Exec Foreground latexml \
          --path=../common/xmlextras --dest=userguide.xml userguide
   }
}

MakeRule Define {
   -targets            oommf.bib.xml
   -dependencies       oommf.bib
   -script      {
      Oc_Exec Foreground latexml --dest=oommf.bib.xml oommf.bib
   }
}

MakeRule Define {
   -targets        [file join userguidexml userguide.html]
   -dependencies   [concat  userguide.xml oommf.bib.xml \
                       [glob -nocomplain \
                           [file join .. common xmlextras css *.css] \
                           [file join .. common xmlextras js *.js] \
                           [file join .. common xmlextras xslt *.xsl]]]
   -script     {
       Oc_Exec Foreground latexmlpost \
         --split --splitat=subsection --splitnaming=label\
         --urlstyle=file \
         --nodefaultresources \
         --css=../common/xmlextras/css/oommf.css \
         --css=../common/xmlextras/css/oommf-report.css \
         --css=../common/xmlextras/css/oommf-tweakfonts.css \
         --navigationtoc=context \
         --css=../common/xmlextras/css/oommf-navbar.css \
         --javascript=../common/xmlextras/js/LaTeXML-maybeMathjax.js \
         --timestamp=0 \
         --stylesheet=../common/xmlextras/xslt/oommf-html5.xsl \
         --dest=userguidexml/userguide.html userguide.xml
   }
}

# Shortcut target for latexml-based html build
if {[llength [auto_execok latexml]]} {
   MakeRule Define {
      -targets            :latexml
      -dependencies       [file join userguidexml userguide.html]
   }
} else {
   MakeRule Define {
      -targets            :latexml
      -dependencies       {}
      -script {
         puts stderr "\n*** Skipping target :latexml (missing executable) ***\n"
      }
   }
}


# Maintainer-clean target deletes all files except the base set which
# can be used to reconstruct everything. This should leave only files
# that are in the repo.
MakeRule Define {
   -targets            maintainer-clean
   -dependencies       distclean
   -script              {
      DeleteFiles userguide userguide.ps userguide.pdf
      DeleteFiles userguidexml userguide.xml oommf.bib.xml
   }
}

# Distclean target removes all files not needed by end user.
MakeRule Define {
   -targets             distclean
   -dependencies        clean
   -script              {
      DeleteFiles {*}[glob -nocomplain *.aux *.bbl *.dvi *.xml]
   }
}

# Clean target removes all files not needed by top-level
# targets (make all).
MakeRule Define {
   -targets            clean
   -dependencies       {}
   -script             {
      # Keep *.{aux,bbl,dvi} for latex2html
      DeleteFiles {*}[glob -nocomplain *.blg]
      DeleteFiles {*}[glob -nocomplain *.idx]
      DeleteFiles {*}[glob -nocomplain *.ilg]
      DeleteFiles {*}[glob -nocomplain *.ind]
      DeleteFiles {*}[glob -nocomplain *.log]
      DeleteFiles {*}[glob -nocomplain *.out]
      DeleteFiles {*}[glob -nocomplain *.toc]
      DeleteFiles {*}[glob -nocomplain [file join userguide images.*]]
   }
}

MakeRule Define {
   -targets            upgrade
   -script             {
      DeleteFiles {*}[glob -nocomplain *.html *.css *.gif *.pl]
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

# NOTES:
# latexmlpost format options:
#
#   --destination=destination ... Specifies the destination file and
#      directory. The directory is needed for mathimages, mathsvg and
#      graphics processing.
#
#   --format=(html|html5|html4|xhtml|xml) ... Specifies the output
#      format for post processing. By default, it will be guessed from
#      the file extension of the destination (if given), with html
#      implying html5, xhtml implying xhtml and the default being xml,
#      which you probably don’t want.
#
#   --omitdoctype, --noomitdoctype ... Omits (or includes) the document
#      type declaration. The default is to include it if the document
#      model was based on a DTD.
#
#   --numbersections, --nonumbersections ... Includes (default), or
#      disables the inclusion of section, equation, etc, numbers in
#      the formatted document and crossreference links.
#
#   --css=cssfile ............ Adds cssfile as a css stylesheet to be
#       used in the transformed html/html5/xhtml. Multiple stylesheets
#       can be used; they are included in the html in the order given,
#       following the default ltx-LaTeXML.css. The stylesheet is copied
#       to the destination directory, unless it is an absolute url.
#
#       Some stylesheets included in the distribution are
#
#          –css=navbar-left Puts a navigation bar on the left. (default
#               omits navbar) –css=navbar-right Puts a navigation bar on
#               the left.
#          –css=theme-blue A blue coloring theme for headings.
#          –css=amsart A style suitable for journal articles.
#
#   --stylesheet=xslfile ..... Requests the XSL transformation of the
#      document using the given xslfile as stylesheet. If the stylesheet
#      is omitted, a ‘standard’ one appropriate for the format (html4,
#      html5 or xhtml) will be used.
#
#   --xsltparameter=name:value ... Passes parameters to the XSLT
#      stylesheet. See the manual or the stylesheet itself for available
#      parameters.
#
#   --timestamp=timestamp .... Provides a timestamp (typically a time
#      and date) to be embedded in the comments by the stock XSLT
#      stylesheets. If you don’t supply a timestamp, the current time
#      and date will be used. (You can use --timestamp=0 to omit the
#      timestamp).
#
#   --icon=iconfile .......... Copies iconfile to the destination
#      directory and sets up the linkage in the transformed
#      html/html5/xhtml to use that as the ”favicon”.
#
#   --javascript=jsfile ...... Includes a link to the javascript file
#      jsfile, to be used in the transformed html/html5/xhtml. Multiple
#      javascript files can be included; they are linked in the html in
#      the order given. The javascript file is copied to the destination
#      directory, unless it is an absolute url.
#
#   --nodefaultresources ..... Disables the copying and inclusion of
#      resources added by the binding files; This includes CSS,
#      javascript or other files. This does not affect resources
#      explicitly requested by the --css or --javascript options.
