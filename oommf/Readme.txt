README: OOMMF
Object Oriented MicroMagnetic computing Framework
Release 1.2a4

	This file last modified on: $Date: 2006-06-29 00:27:45 $

OOMMF is a project in the Mathematical and Computational Sciences 
Division (MCSD) of ITL/NIST aimed at developing portable, extensible 
public domain micromagnetic programs and tools. The end product will be 
a completely functional, user-friendly micromagnetic code, with a well 
documented, flexible programmer's interface to allow developers outside 
the OOMMF project to swap their own code in and out as desired. The guts 
of the code are being written in C++ with a Tcl/Tk (and in the future, 
possibly OpenGL) interface. Target systems include a wide range of Unix
and Windows platforms, and Mac OS X 10.3 (Panther).

The main contributors to OOMMF are Mike Donahue and Don Porter.

Before you can do anything with OOMMF, you must have Tcl/Tk installed
on your computer.  The Tcl/Tk Core is available for free download from 
the Tcl Developer eXchange at <URL:http://www.tcl.tk/>.  We recommend
the latest Tcl/Tk releases (currently 8.4.7) and OOMMF requires at
least Tcl version 7.5 and Tk version 4.1 on Unix platforms, at least
Tcl version 7.6 and Tk version 4.2 on Windows platforms, and at
least Tcl/Tk 8.4.0 on Mac OS X.  OOMMF software does not support any
alpha or beta versions of Tcl/Tk.

-----------------------------------------------------------------------
QUICK INSTALL:
Most OOMMF users on MS Windows platforms will download a release
with pre-compiled executables compatible with Tcl/Tk 8.4.x.  Type
the following commands at an MS-DOS Prompt to start using OOMMF:

	unzip oommf12a4_20040908.zip
	cd oommf
	tclsh84 oommf.tcl pimake upgrade
	tclsh84 oommf.tcl

For most OOMMF users on Unix, the following sequence of commands
should upgrade, build, and run your OOMMF installation:

	gunzip -c oommf12a4_20040908.tar.gz | tar xvf -
	cd oommf
	oommf.tcl pimake distclean
	oommf.tcl pimake upgrade
	oommf.tcl pimake
	oommf.tcl

OOMMF users on Mac OS X 10.3 (Panther) should first be sure that they
have installed the optional X11 component.  Then use the Finder to
find and start the X11 program found in the Applications folder.
A terminal program called xterm will start, and in that terminal,
you should build/install both Tcl and Tk 8.4, using the Unix instructions
instead of the Mac OS X instructions.  (This will build an X11
version of Tk rather than an Aqua version).  Then the OOMMF build
and install instructions for Unix above should work.

If that does not work, read the more detailed instructions in the
_OOMMF User's Guide_.
-----------------------------------------------------------------------
IMPORTANT NOTE ABOUT UPGRADING TO OOMMF 1.2:

In OOMMF 1.2 releases, the directory formerly known as "ext" has been
renamed "pkg".  If you unpack OOMMF 1.2 over a previous of OOMMF, the
old ext directory will not be overwritten, and the

	oommf.tcl pimake upgrade

will not delete it.  If you made any modifications to files in the
directory "ext", be sure to copy them over to the new directory "pkg",
and then we advise you delete the directory "ext".

Of course, this can all be avoided by installing OOMMF 1.2 in a
fresh location, not over the top of a previous OOMMF installation.
-----------------------------------------------------------------------

The _OOMMF User's Guide_ is available as the PDF file
oommf/doc/userguide.pdf, PostScript file oommf/doc/userguide.ps, or as a
tree of hyperlinked HTML files rooted in the file
oommf/doc/userguide/userguide.html .  You may view the HTML version with
any web browser.  In case one is not available, a simple HTML browser
application, mmHelp, is distributed with OOMMF software (in the
directory app/mmhelp).  It will display the _OOMMF User's Guide_ by
default.  The Tcl/Tk Windowing Shell program 'wish' is needed to run
mmHelp with the following command line:

wish app/mmhelp/mmhelp.tcl

where you may need to replace the general name 'wish' with the
specific name of the Tk Windowing Shell program on your platform
(say, 'wish4.2' or 'wish80.exe').  Consult your Tcl/Tk documentation.

For instructions on downloading, installing and using OOMMF software,
see the 'Installation' section of the _OOMMF User's Guide_.

For the latest information about OOMMF software, see 
<URL:http://math.nist.gov/oommf/>.

To contact the OOMMF developers, send e-mail to 
<michael.donahue@nist.gov>.

Major updates to the OOMMF project are announced on the "muMag 
Announcement" mailing list.  To join the list, see 
<URL:http://www.ctcms.nist.gov/~rdm/email-list.html>
