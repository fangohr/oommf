# FILE: inputfilter.tcl
#
# A collection of Tcl procedures (not Oc_Classes) which are part of the
# Nb extension
#
# Last modified on: $Date: 2007/03/21 01:17:02 $
# Last modified by: $Author: donahue $
#

Oc_Class Nb_InputFilter {
   # Each filter type variable (e.g., decompress) holds a list with an
   # even number of elements.  The first element of each pair is a list
   # of one or more file extensions, and the second element is a filter
   # command to run if the end of the input filename matches one of the
   # file extensions.  File extension matching is performed in order,
   # with the first match taking precedence.  After a filter is
   # applied, further matching is done against a reduced name formed by
   # removing the matched extension string, so for example, after
   # "foo.ovf.gz" is matched against ".gz", subsequent filtering will
   # use the string "foo.ovf" for filter extension matching.  For this
   # reason one should generally include the leading period ('.') in
   # the extension name.  However, if one wants to match say "foo.tgz"
   # against "gz", then both ".gz" and "gz" can be included in the file
   # extensions list.  But in this case the more restrictive match,
   # i.e., ".gz" should be earlier in the list.
   public common known_types {decompress ovf odt}
   public common decompress {{.gz .z .zip} {gzip -dc}}
   public common ovf {}
   public common odt {}

   ClassConstructor {
      # Lowercase all extensions, because filename
      # matching is done case insensitive.
      foreach type $known_types {
         if {[llength [set $type]]%2!=0} {
            Oc_Log Log "Illegal extension/cmd list for filter type\
			    $type.  Element count should be even, not\
                            [llength [set $type]]" \
               panic $class
         }
         set lowered_list {}
         foreach {exts cmd} [set $type] {
            lappend lowered_list [string tolower $exts] $cmd
         }
         set $type $lowered_list
      }
   }

   proc GetExtensionList { type } {
      # Import 'type' should be one of $known_types
      set extlist {}
      foreach {exts cmd} [set $type] {
         set extlist [concat $extlist $exts]
      }
      return $extlist
   }

   private proc FindFilter { filename filtertype } {
      upvar $filtertype filterlist
      if {![info exists filterlist]} {
         Oc_Log Log "Unknown filter request \"$filtertype\"\
		    for file \"$filename\"" panic $class
      }
      set lowfile [string tolower $filename]
      set usefilter {}
      set basename {}
      foreach {exts cmd} $filterlist {
         foreach i $exts {
            # Loop through extensions
            if {[regexp "^(.*)$i\$" $lowfile dum0 dum1]} {
               set usefilter $cmd
               set basename $dum1
               break
            }
         }
         if {![string match {} $usefilter]} break
      }
      return [list $usefilter $basename]
   }

   private proc ApplyFilter { infile filterprog } {
      # Pipes $infile through $filterprog, writing result into
      # a temporary file.  The name of the temporary file is
      # returned.  An empty string is returned on error.
      #   Note: It is the responsibility of the calling code
      # to remove the temporary file.
      if {[string match {} $filterprog]} { return {} } ;# No filterprog

      # Make up temporary file name.
      if {[catch {Oc_TempName nbinfil} tempname]} {
         Oc_Log Log $tempname error $class
         return {}
      }

      # Try to run filter command.  Note that we only want one
      # substitution pass on $infile and $tempname, lest they have
      # spaces or backslash sequences in their values.
      if {[catch {eval exec $filterprog {<$infile >$tempname}} errmsg]} {
         Oc_Log Log "Error while trying to execute \
		    $filterprog: $errmsg" error $class
         catch {file delete $tempname}
      }

      if {[file exists $tempname]} { return $tempname } ;# Apparent success
      return {}  ;# Failure
   }

   proc FilterFile { filename args } {
      #   The args import is a list of filter types to try to apply,
      # in sequence, to the filename import.  Filter type matching
      # is done solely on filename matching against extensions
      # stored in the class file common area, i.e., there is no
      # attempt made to identify file type info from the contents
      # of the file.
      #   If one or more filters are successfully applied, then the
      # name of the final temporary file is returned.  It is the
      # responsibility of the calling code to delete this file when
      # finished.
      #   If no filters are applied, then an empty string is returned.

      set workname $filename  ;# Name to match filters against
      set infile $filename    ;# File to apply filter to
      foreach type $args {
         foreach {fprog basename} \
            [Nb_InputFilter FindFilter $workname $type] break
         if {![string match {} $fprog]} {
            # Match found
            set tempfile [Nb_InputFilter ApplyFilter $infile $fprog]
            if {![string match {} $tempfile]} {
               # Filter successful
               if {[string compare $infile $filename]!=0} {
                  file delete $infile  ;# Delete old tempfile
               }
               set infile $tempfile
               set workname $basename
            }
         }
      }
      if {[string compare $infile $filename]==0} {
         return {} ; # No filter applied
      }
      return $infile ;# Temporary file holding filtered output
   }
}
