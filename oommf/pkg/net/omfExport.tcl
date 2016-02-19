# This file is sourced by the OOMMF account server to determine what
# processes it is permitted to "export".  That is, it defines a list of
# programs on the local host which it will launch in response to a request
# received over a network socket. 
#
# The entries in this list are stored as a Tcl array which maps an alias
# by which a program is known remotely to information needed to launch
# it locally.
#
# Last modified on: $Date: 2007/03/21 01:17:04 $
# Last modified by: $Author: donahue $

set Omf_export_list [list \
    mmArchive	[list Oc_Application Exec mmArchive] \
    mmDataTable	[list Oc_Application Exec mmDataTable] \
    mmDisp	[list Oc_Application Exec mmDisp] \
    mmGraph	[list Oc_Application Exec mmGraph] \
    mmProbEd	[list Oc_Application Exec mmProbEd] \
    mmSolve2D	[list Oc_Application Exec mmSolve2D] \
    Oxsii	[list Oc_Application Exec Oxsii] \
]

