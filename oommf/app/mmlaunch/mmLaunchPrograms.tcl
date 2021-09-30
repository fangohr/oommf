# This file is sourced by mmLaunch to determine what applications to
# present to the user to launch.
#
# The entries in this list are stored as a Tcl array which maps an alias
# by which a program is known to information needed to launch it.

set mmLaunchPrograms [list \
    mmArchive	[list Oc_Application Exec mmArchive] \
    mmDataTable	[list Oc_Application Exec mmDataTable] \
    mmDisp	[list Oc_Application Exec mmDisp] \
    mmGraph	[list Oc_Application Exec mmGraph] \
    mmProbEd	[list Oc_Application Exec mmProbEd] \
    mmSolve2D	[list Oc_Application Exec mmSolve2D] \
    Oxsii	[list Oc_Application Exec Oxsii] \
]
