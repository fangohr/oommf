# FILE: ctype.tcl
#
# This script makes use of Tcl's http package to retrieve a URL and
# print a summary of its properties.  Notably, one can discover the
# Content-Type of the resources identified by that URL.  This can
# be useful in testing whether web servers are properly exporting
# special Content-Types associated with OOMMF's file formats.
#
package require http
set token [http::geturl [lindex $argv 0]]
http::wait $token
array set summary [array get $token]
set summary(body) [string range $summary(body) 0 100]...
parray summary
