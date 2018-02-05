README: OOMMF
Object Oriented MicroMagnetic computing Framework
Release 2.0a0

OOMMF is a project in the Mathematical and Computational Sciences 
Division (MCSD) of ITL/NIST aimed at developing portable, extensible 
public domain micromagnetic programs and tools. The released code forms
a completely functional, micromagnetics pakage, with the additional
capability to be extended by other programmers so that developers
outisde the OOMMF project can build on the OOMMF foundation.

OOMMF is written in C++ with a Tcl/Tk interface. Target systems
include a wide range of Unix, Windows, and Mac OS X platforms.

The main contributors to OOMMF are Mike Donahue and Don Porter.

Before you can do anything with OOMMF, you must have Tcl/Tk installed
on your computer.  The Tcl/Tk Core is available for free download from 
the Tcl Developer eXchange at <URL:http://www.tcl.tk/>.  We recommend
the latest Tcl/Tk releases, currently 8.6.7. The oldest versions
of Tcl/Tk compatible with OOMMF varies with the computing platform,
but any installation likely to be found should be acceptable.
Please note as exceptions that releases 8.6.2, 8.6.3, 8.5.16, and
8.5.17 of Tcl should be avoided as they include I/O bugs that interfere
with OOMMF's ability to reliably record output in the file system.
OOMMF software does not support any alpha or beta versions of Tcl/Tk.

-----------------------------------------------------------------------
QUICK INSTALL:
Most OOMMF users on MS Windows platforms will download a release
with pre-compiled executables compatible with Tcl/Tk 8.6.x.  Type
the following commands at an MS-DOS Prompt to start using OOMMF:

	unzip oommf20a0.zip
	cd oommf
	tclsh86 oommf.tcl pimake upgrade
	tclsh86 oommf.tcl

For most OOMMF users on Unix and Mac OS X, the following sequence of
commands should upgrade, build, and run your OOMMF installation:

	gunzip -c oommf20a0.tar.gz | tar xvf -
	cd oommf
	oommf.tcl pimake distclean
	oommf.tcl pimake upgrade
	oommf.tcl pimake
	oommf.tcl

If that does not work, read the more detailed instructions in the
_OOMMF User's Guide_.
-----------------------------------------------------------------------

The _OOMMF User's Guide_ is available as the PDF file
oommf/doc/userguide.pdf, PostScript file oommf/doc/userguide.ps, or as a
tree of hyperlinked HTML files rooted in the file
oommf/doc/userguide/userguide.html .  

For instructions on downloading, installing and using OOMMF software,
see the 'Installation' section of the _OOMMF User's Guide_.

For the latest information about OOMMF software, see 
<URL:http://math.nist.gov/oommf/>.

To contact the OOMMF developers, send e-mail to 
<michael.donahue@nist.gov>.

Major updates to the OOMMF project are announced on the "muMag 
Announcement" mailing list.  To join the list, see 
<URL:http://www.ctcms.nist.gov/~rdm/email-list.html>
