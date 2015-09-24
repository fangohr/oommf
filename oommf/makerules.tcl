# FILE: makerules.tcl
#
# This file controls the application 'pimake' by defining rules which
# describe how to build the applications and/or extensions produced by
# the source code in this directory.
#
# Verify that this script is being sourced by pimake
if {[llength [info commands MakeRule]] == 0} {
    error "" "'[info script]' must be evaluated by pimake"
}
#
########################################################################

MakeRule Define {
    -targets            all
    -script		{Recursive all}
}

# Be sure a 'make distclean' is done in ext before 'make upgrade'
# tries to remove the files there, if that makes sense
set dep ""
if {[file readable [file join ext makerules.tcl]]} {
    set dep [file join ext distclean]
} 

# Set of files under ext subdirectory in old releases of OOMMF that
# 'make upgrade' should remove.
set toDelete {
    if  {if.cc if.h makerules.tcl version.h}
    nb  {array.cc array.h bucket.cc bucket.h dstring.cc dstring.h
          errhandlers.cc errhandlers.h evoc.cc evoc.h functions.cc
          functions.h list.cc list.h makerules.tcl nb.cc nb.h
          stopwatch.cc stopwatch.h vector.cc vector.h version.h
          xpfloat.cc xpfloat.h}
    net {account.tcl accountGui.tcl connection.tcl fewtest.tcl
          host.tcl hostGui.tcl link.tcl makerules.tcl net.tcl
          omfExport.tcl pkgIndex.tcl protocol.tcl server.tcl
          thread.tcl threadGui.tcl}
    oc  {application.tcl autobuf.cc autobuf.h bug.tcl bug75.tcl
          bug76.tcl bug80.tcl bug81.tcl byte.cc byte.h class.tcl
          commandline.tcl config.tcl console.tcl custom.tcl
          eventhandler.tcl exec.tcl font.tcl imports.cc imports.h
          log.tcl main.tcl makerules.tcl messages.cc messages.h
          nop.tcl nullchannel.cc nullchannel.h oc.cc oc.h oc.tcl
          option.tcl person.tcl pkgIndex.tcl procs.tcl tclIndex
          tempfile.tcl url.tcl varinfo.cc version.h}
    ow  {charinfo.tcl dirselect.tcl entrybox.tcl entryscale.tcl
	  filedlg.tcl fileselect.tcl graphwin.tcl listbox.tcl
          log.tcl makerules.tcl multibox.tcl omficon.xbm omfmask.xbm
          ow.tcl pkgIndex.tcl printdlg.tcl procs.tcl selectbox.tcl}
    vf  {fileio.cc fileio.h makerules.tcl mesh.cc mesh.h ptsearch.cc
          ptsearch.h vecfile.cc vecfile.h vecfile.tcl version.h
          vf.cc vf.h vf.tcl}
}

MakeRule Define [format {
    -targets		upgrade
    -dependencies	{%s}
    -script		{
			 Recursive upgrade
			 DeleteFiles [file join ext net threads host.tcl]
			 DeleteFiles [file join ext net threads account.tcl]
			 catch {file delete [file join ext net threads]}
			 foreach {d l} {%s} {
			     foreach f $l {
				 DeleteFiles [file join ext $d $f]
			     }
			     catch {file delete [file join ext $d]}
			 }
			 DeleteFiles [file join ext makerules.tcl]
			 DeleteFiles [file join ext pkgIndex.tcl]
			 catch {file delete ext}
			 unset d l f
			 DeleteFiles [file join config colors.def]
			}
} $dep $toDelete]

unset dep toDelete


