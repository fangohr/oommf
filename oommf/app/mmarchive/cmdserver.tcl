# FILE: cmdserver.tcl
#
# Command line interface for scalar and vector field servers
#
########################### PROGRAM DOCUMENTATION ###################
# This program reads commands from stdin and writes responses
# and data to stdout.  The commands are:
#
#    configure  --- Configure interface and servers.  None, one, or more
#                   options may be set within a call.  The response
#                   displays the new configuration settings.
#    connect    --- Advertise servers via account server.
#    disconnect --- Remove servers from account server advertisements.
#    peek       --- Print data in data buffer.
#    eat        --- Print data in data buffer and empty buffer.
#    exit       --- Terminate cmdserver.
#
# The configure options are:
#
#  -alias   --- Name reported (advertised) by account server.
#  -mode    --- Either "poll" or "event".  Influences data handling in
#               proc ProcessImportData.
#  -structure --- Either "white" meaning white space delimited fields
#              (default), "tcl" meaning use the Tcl list structure, or
#              "csv" indicating "Character Separated Value", with the
#              separating character given by the "csvchar" option.
#  -csvchar --- Separating value in CSV format; defaults to ",".  An
#               interesting variant is the ASCII "unit separator", aka
#               US, which has hex value 1F.  One could implement an
#               ASCII-based -structure that uses \x1F for field
#               separation and \x1E for record separation (\x1E is the
#               ASCII code for RS, "record separator).  The pro is that
#               no quoting would be necessary for text data.  The con is
#               that one couldn't use standard library calls like gets
#               to process the stdout stream, because each record would
#               be terminated by an RS character instead of a new line.
#  -datatable, -scalarfield, -vectorfield --- Which servers to activate.
#               Set to 1 to enable, 0 (default) to disable.
#
# Typical usage pattern is:
#
#   configure -scalarfield 1 -vectorfield 1 -mode poll -alias AppName
#   connect
#   eat
#  <do other stuff>
#   eat
#  <do other stuff>
#   ...
#   eat
#   disconnect
#   eat
#   exit
#
# Here "AppName" in the configure command is the advertised name for the
# enveloping application.  If -mode is set to "event" then data is
# automatically written to stdout as it arrives (from a micromagnetic
# solver).  The enveloping application should be set up to process the
# data when a readable event is raised on stdout.  In that case the
# usage pattern is:
# 
#   configure -scalarfield 1 -vectorfield 1 -mode event -alias AppName
#   connect
#   <process data as it becomes available>
#   ...
#   disconnect
#   eat
#   exit
#
# In both cases the final "eat" after the disconnect insures that any
# data arriving between the disconnect request and actual connection
# closure gets processed.
#
# If "-mode poll" is used, then the enveloping application will probably
# want to access the data in non-blocking mode, since otherwise the
# "eat" calls will hang unless and until data is sent from the
# micromagnetic solver.
#
# There are three types of records written to stdout by this cmdserver:
#
#   Command response  (R)
#   Errors            (E)
#   Data              (D)
#
# The first character in each record is one of R, E, or D as indicated
# above.  Each record ends with a new line.  There are no embedded
# new lines in any R or E records, so the first new line in those records
# indicate the end of the record.
#
# Each command written to stdin will result in exactly one R or E record
# being written to stdout.  The reply to "peek" and "eat" commands
# consists of first a record of the form
#
#    R #
#
# where "#" is an integer value indicating the number of data records;
# after the "R #" record will follow "#" data records.
#
# Each R and E record contains two fields, the first field being either
# R or E, and the second field being the response or error string.
# These fields are demarcated in the same manner as data records,
# explained below, which depends on the -structure configuration
# setting.  For example, if "-structure csv" is used with "-csvchar ,",
# then a peek command with 3 records in the buffer would have the response
#
#   R,3
#
# followed by 3 data records.
# 
# Data records, whether in response to peek/eat commands or writen
# automatically in "event" mode, have the form
#
#   D <data type> <connection id> <data fields>
#
# The method of demarcation between the various fields depends on the
# configure -structure setting.  If -structure is "white", then the
# fields are whitespace delimited.  In this case if a field contains
# embedded whitespace, then the whole field will be enclosed with double
# quotes, i.e. ", with double quotes, backslashes and new lines escaped
# with backslashes as \", \\, and \n respectively.  If the first
# character of a field is not a double quote, then the field is rendered
# literally, with no backslash escapes.
#
# If -structure is "tcl", then the record is formatted as a proper Tcl
# list, which is primarily whitespace delimited with precise rules for
# escaping special characters and sublists---see Tcl documentation for
# details.  In addition to the normal Tcl list rules, backslashes and
# new lines are represented by the two character sequences \\ and \n,
# respectively.  The latter allows for new line separated records, and
# the former is necessary to distinguish between a new line and a
# backslash followed by the letter n, as can happen in path names under
# Windows.  This escape handling is applied to all fields.
#
# If -structure is "csv", # then the fields are separated by the
# configure -csvchar character, which is typically a comma, ",".  CSV
# format does not support sublists.  If a field contains the -csvchar
# character, new lines, or double quotes, then the field will be
# surrounded with double quotes, and any double quotes inside the field
# will be replaced with a pair of double quotes, and backslashes and new
# lines are represented by the two character sequences \\ and \n,
# respectively.  If the first character of a field is not a double
# quote, then the field is rendered literally, with no backslash
# escapes.
#
# See the ListToWhite, ListToTcl, and ListToCSV procs for exact details
# on the quoting rules.
#
# Examples of data records using the various -structure options are
# provided below.  Note that internally all records are handled as Tcl
# lists, with conversion to the various output structures done on
# output.
#
# The <data type> field is one of the two character sequences "DT",
# "SF", or "VF", indicating data table, scalar field, or vector field
# data, respectively.  The <connection id> field is a unique
# alphanumeric string identifying the source of the data.  This is
# useful in the situation where multiple micromagnetic solvers are
# sending output to one instance of cmdserver.
#
# The format of the data fields depends on the data type.  For
# scalar and vector field servers, the data portion of the 
# record consists of two fields:
#
#      <temp_filename>  <final_filename>
#
# The <temp_filename> field contains the absolute pathname of a file
# that holds the scalar or vector field data, in OVF format.  The
# <final_filename> field holds the intended name for that file.  The
# mmArchive application moves (renames) the OVF file from the first name
# to the second.  The mmDisp application reads the OVF data using the
# first filename, deletes that file, and then bases the file display
# name on the second filename.  The enveloping application using
# cmdserver can make use of this information as it sees fit, but
# regardless it is the responsibility of the enveloping application to
# insure that <temp_filename> is eventually deleted or moved.
#
# As an example, a vector field data record may look like
#
#   D VF _Net_Connection5 /tmp/caesar-28289-0012.ohf /home/julius/Oxs_Demag-Field-00-0000024.ohf
#
# if the configure -structure option is "white" or "tcl".  Alternately,
# if the configure -structure option is "csv" with -csvchar option set
# to ",", then the data record would be
#
#   D,VF,_Net_Connection5,/tmp/caesar-28289-0012.ohf,/home/julius/Oxs_Demag-Field-00-0000024.ohf
#
# Here is a more complicated example, with embedded white space and
# Windows-style (backslashes) directory separators:
#
#   D VF _Net_Connection5 \tmp\caesar-28289-0012.ohf "\\home\\julius caesar\\Oxs_Demag-Field-00-0000024.ohf"
#   D VF _Net_Connection5 {\\tmp\\caesar-28289-0012.ohf} {\\home\\julius caesar\\Oxs_Demag-Field-00-0000024.ohf}
#   D,VF,_Net_Connection5,\tmp\caesar-28289-0012.ohf,\home\julius caesar\Oxs_Demag-Field-00-0000024.ohf
#
# Using "white", "tcl" and "csv" settings for -structure, respectively.
# Note the single backslashes in the fourth field but "\\" in the last
# field of the -structure white line.
#
# The format of the data fields for the data table server consists of
# a list of triples.  The first of these triples has the form
#
#   @Filename:<filename.odt> {} 0
#
# where "<filename.odt>" will be the suggested filename for the output
# file (given as an absolute path).  mmArchive appends the data to this
# file.  The second element here is an empty string, and the third
# element is an unused dummy value.  The rest of the data record
# consists of triples that look like
#
#   "Oxs_RungeKuttaEvolve:evolve:Total energy" J 1.2390938239358198e-15
#
# where the first subfield is the name of the output, the second
# subfield tells the units (here "J" for joules), and the last subfield
# is the numeric value.  The first or second subfields may include
# spaces, in which case quoting is applied as determined by the
# -structure setting.
#
# The data fields are represented as a flat list where the total number
# of data fields is divisible by three.  This means that a data table
# record looks like
#
#   D DT _Net_Connection2 @Filename:/home/julius/oommf/app/oxs/examples/antidots-filled.odt "" 0 "Oxs_RungeKuttaEvolve:evolve:Total energy" J 1.2390938239358198e-15 "Oxs_RungeKuttaEvolve:evolve:Energy calc count" "" 133 "Oxs_RungeKuttaEvolve:evolve:Max dm/dt" deg/ns 2644.8593423271327 Oxs_RungeKuttaEvolve:evolve:dE/dt J/s -4.5119068749840701e-07
#
# for white space delimited records, or
#
#   D DT _Net_Connection2 @Filename:/home/julius/oommf/app/oxs/examples/antidots-filled.odt {} 0 {Oxs_RungeKuttaEvolve:evolve:Total energy} J 1.2390938239358198e-15 {Oxs_RungeKuttaEvolve:evolve:Energy calc count} {} 133 {Oxs_RungeKuttaEvolve:evolve:Max dm/dt} deg/ns 2644.8593423271327 Oxs_RungeKuttaEvolve:evolve:dE/dt J/s -4.5119068749840701e-07
#
# for Tcl-style records.  If the configure -structure option is "csv"
# with -csvchar set to ",", the above data record would be rendered as
#
#   D,DT,_Net_Connection2,@Filename:/home/julius/oommf/app/oxs/examples/antidots-filled.odt,,0,Oxs_RungeKuttaEvolve:evolve:Total energy,J,1.2390938239358198e-15,Oxs_RungeKuttaEvolve:evolve:Energy calc count,,133,Oxs_RungeKuttaEvolve:evolve:Max dm/dt,deg/ns,2644.8593423271327,Oxs_RungeKuttaEvolve:evolve:dE/dt,J/s,-4.5119068749840701e-07
#
# Note that empty fields are denoted by "", {}, and ,, in the three
# cases, respectively.
#

# Library support (let them eat options)
package require Oc 2   ;# Error & usage handling below requires Oc
package require Net 2

Oc_Main SetAppName CmdServer
Oc_Main SetVersion 2.0b0
regexp \\\044Date:(.*)\\\044 {$Date: 2015/09/10 22:13:14 $} _ date
Oc_Main SetDate [string trim $date]
# regexp \\\044Author:(.*)\\\044 {$Author: donahue $} _ author
# Oc_Main SetAuthor [Oc_Person Lookup [string trim $author]]
Oc_Main SetAuthor [Oc_Person Lookup donahue]
Oc_Main SetExtraInfo "Built against Tcl\
        [[Oc_Config RunPlatform] GetValue compiletime_tcl_patch_level]\n\
        Running on Tcl [info patchlevel]"
Oc_Main SetHelpURL [Oc_Url FromFilename [file join [file dirname \
        [file dirname [file dirname [file dirname [Oc_DirectPathname [info \
        script]]]]]] doc userguide userguide \
        Vector_Field_Display_mmDisp.html]]

Oc_CommandLine Parse $argv

array set config_options [subst {
   -alias [Oc_Main GetAppName]
   -mode  poll
   -structure white
   -csvchar ,
   -datatable   0
   -scalarfield 0
   -vectorfield 0
   -nickname {}
}]
proc VerifyOptions {} {
   global config_options
   set badopts {}
   if {![regexp {^poll|event$} $config_options(-mode)]} {
      lappend badopts -mode
   }
   if {![regexp {^white|tcl|csv$} $config_options(-structure)]} {
      lappend badopts -structure
   }
   foreach item {-datatable -scalarfield -vectorfield} {
      if {![regexp {^0|1$} $config_options($item)]} {
	 lappend badopts $item
      }
   }
   foreach tryname $config_options(-nickname) {
      if {[regexp {^[0-9]+$} $tryname]} {
         lappend badopts [list -nickname $tryname]
      }
   }
   return $badopts
}
proc Configure {args} {
   global config_options
   set unrecog {}
   foreach {label value} $args {
      if {[info exists config_options($label)]} {
         if {[string compare -nickname $label]==0} {
            if {[string match {} $value]} {
               # Use empty string to clear current nickname list
               set config_options(-nickname) {}
            } else {
               # Otherwise, -nickname requests stack
               lappend config_options(-nickname) $value
            }
         } else {
            set config_options($label) $value
         }
      } else {
	 lappend unrecog $label
      }
   }
   set badopts [VerifyOptions]
   if {[llength $unrecog]>0 || [llength $badopts]>0} {
      set errmsg {}
      if {[llength $unrecog]>0} {
	 append errmsg " Unrecognized configure value for option(s): $unrecog"
      }
      if {[llength $badopts]>0} {
	 append errmsg " Invalid value for option(s): $badopts"
      }
      WriteRecord [list E $errmsg]
   } else {
      foreach label [lsort [array names config_options]] {
	 lappend labvals $label $config_options($label)
      }
      WriteRecord [list R $labvals]
   }
}
set cmds(configure) Configure

proc ListToWhite { data } {
   # Converts import "data", treated as a Tcl list, into a white space
   # delimited record as per the rules detailed above in the file
   # header.  Any field containing whitespace or double quotes is quoted
   # and escaped as necessary.
   set record {}
   foreach elt $data {
      if {[string match "*\[\r\n\t\"\]*" $elt]} {
         # Quote field
         set mapping "\\\" \\\\\" \\\\ \\\\\\\\ \\n \\\\n"
         set elt \"[string map $mapping $elt]\"
      }
      append record " " $elt
   }
   return [string range $record 1 end]
}

proc ListToTcl { data } {
   # Escape backslashes and new lines.  This should allow proper
   # distinction between a new line and a backslash followed by an n, as
   # can happen for example in Window path names.
   return [string map {\\ \\\\ \n \\n} $data]
}

proc ListToCSV { data } {
   # Converts import "data", treated as a Tcl list, into
   # a single CVS record, with fields separated by the character
   # "ch" (defaults to comma).  Any field containing \n, \r, double
   # quotes or the separation character will be quoted with double
   # quotes.  Double quotes within a quoted field are indicated with
   # two double quotes.
   global config_options
   set ch $config_options(-csvchar)
   set record {}
   foreach elt $data {
      if {[string first $ch $elt]>=0 ||
          [string match "*\[\r\n\"\]*" $elt]} {
         # Quote string
         set elt \"[string map {\" \"\" \\ \\\\ \n \\n} $elt]\"
      }
      append record "," $elt
   }
   return [string range $record 1 end]
}

proc Terminate {} {
   DisconnectServers
   exit
}
set cmds(exit) Terminate


proc WriteRecord { record } {
   global config_options
   switch -exact $config_options(-structure) {
      white {
         set record [ListToWhite $record]
      }
      tcl   {
         set record [ListToTcl $record]
      }
      csv   {
         set record [ListToCSV $record]
      }
      default {
         error "Invalid -structure configure option: \
               \"$$config_options(-structure)\""
      }
   }
   puts $record
}

set datastack {}  ;# Stack for storing all server import data
proc Peek {} {
   global datastack
   WriteRecord [list R [llength $datastack]]
   foreach record $datastack {
      WriteRecord [linsert $record 0 D]
   }
}
set cmds(peek) Peek

proc Eat {} {
   global datastack
   Peek
   set datastack {}
}
set cmds(eat) Eat



proc ProcessImportData { datatype connection data } {
   global config_options datastack
   lappend datastack [linsert $data 0 $datatype $connection]
   if {[string compare poll $config_options(-mode)]!=0} {
      # Write each record to stdout, and empty list
      foreach record $datastack {
	 WriteRecord $record
      }
      set datastack {}
   }
}

proc ProcessFieldData { datatype connection fnlist } {
   # data consists of two fields: temp name and final name.  Convert
   # these to native filenames before calling ProcessImportData
   set data [list [file nativename [lindex $fnlist 0]] \
                [file nativename [lindex $fnlist 1]]]
   ProcessImportData $datatype $connection $data
}

proc ProcessDataTableData { connection triples } {
   # data consists of set of triples.  Flatten this list before calling
   # ProcessImportData.
   set flatdata {}
   foreach elt $triples {
      set flatdata [concat $flatdata $elt]
   }
   ProcessImportData DT $connection $flatdata
}

# Data table server
Net_Protocol New dt_protocol -name "OOMMF DataTable protocol 0.1"
$dt_protocol AddMessage start DataTable { triples } {
   ProcessDataTableData $connection $triples
   return [list start [list 0 0]]
}

# Scalar field server
Net_Protocol New sf_protocol -name [list OOMMF scalarField protocol 0.1]
$sf_protocol AddMessage start datafile { fnlist } {
   ProcessFieldData SF $connection $fnlist
   return [list start [list 0]]
}

# Vector field server
Net_Protocol New vf_protocol -name [list OOMMF vectorField protocol 0.1]
$vf_protocol AddMessage start datafile { fnlist } {
   ProcessFieldData VF $connection $fnlist
   return [list start [list 0]]
}

proc StartServers {} {
   global acct acct_ready config_options
   global dt_server sf_server vf_server
   global dt_protocol sf_protocol vf_protocol

   # Account server
   Oc_Main SetAppName $config_options(-alias)
   upvar #0 [Net_Account GlobalName nicknames] acct_nicknames
   set acct_nicknames $config_options(-nickname)
   ## Is there a cleaner way to set the Net_Account common variable
   ## "nicknames"?
   if {![info exists acct]} {
      Net_Account New acct
      if {![$acct Ready]} {
         set acct_ready 0
         Oc_EventHandler New acct_ready_handler $acct Ready \
            [list set acct_ready 1] -oneshot 1
         after 10000 [list set acct_ready -1]
         vwait acct_ready
         if {$acct_ready != 1} {
            error "Unable to open connection with account server"
         }
      }
   } else {
      # Already connected to account server.  Use Getoid call
      # to reset application name and nicknames
      $acct Getoid
   }

   # Data table
   if {$config_options(-datatable)} {
      if {![info exists dt_server]} {
         Net_Server New dt_server -protocol $dt_protocol -register 1
      }
      $dt_server Configure -alias $config_options(-alias)
      $dt_server Start 0
   }

   # Scalar field
   if {$config_options(-scalarfield)} {
      if {![info exists sf_server]} {
         Net_Server New sf_server -protocol $sf_protocol -register 1
      }
      $sf_server Configure -alias $config_options(-alias)
      $sf_server Start 0
   }

   # Vector field
   if {$config_options(-vectorfield)} {
      if {![info exists vf_server]} {
         Net_Server New vf_server -protocol $vf_protocol -register 1
      }
      $vf_server Configure -alias $config_options(-alias)
      $vf_server Start 0
   }
}

proc StopServers {} {
   global dt_server sf_server vf_server acct
   if {[info exists dt_server]} {
      if {[$dt_server IsListening]} { $dt_server Stop }
      $dt_server Delete
      unset dt_server
   }
   if {[info exists sf_server]} {
      if {[$sf_server IsListening]} { $sf_server Stop }
      $sf_server Delete
      unset sf_server
   }
   if {[info exists vf_server]} {
      if {[$vf_server IsListening]} { $vf_server Stop }
      $vf_server Delete
      unset vf_server
   }
   if {[info exists acct]} {
      $acct Delete
      unset acct
   }
}

proc DisconnectServers {} {
   StopServers
   WriteRecord [list R OK]
}

proc ConnectServers {} {
   StopServers
   StartServers
   WriteRecord [list R OK]
}

set cmds(connect) ConnectServers
set cmds(disconnect) DisconnectServers

proc ReadCommand {} {
   global cmds
   set cmdline [gets stdin]
   set cmdname [lindex $cmdline 0]
   set cmdargs [lrange $cmdline 1 end]
   if {[info exists cmds($cmdname)]} {
      eval {$cmds($cmdname)} $cmdargs
   } else {
      WriteRecord [list E "Unrecognized command request: \"$cmdname\"; \
            Valid commands: [array names cmds]"]
   }
}
fileevent stdin readable [list ReadCommand]

vwait forever
