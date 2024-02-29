# FILE: computefield.tcl
#
# PURPOSE: Compute and write out demag and other fields from a specified
# magnetization file.

set TCLSH [info nameofexecutable]
set OOMMF_ROOTDIR [file dirname [file dirname [file dirname \
                                       [file normalize [info script]]]]]
set OOMMF [file join $OOMMF_ROOTDIR oommf.tcl]

########################################################################

proc Usage { {errcode 0} } {
   if {$errcode==0} {
      set chan stdout
   } else {
      set chan stderr
   }
   puts $chan {Usage: computefield [options] <m0> [outdemagfile]}
   puts $chan {where}
   puts $chan {  <m0> is input magnetization (.omf) file (required)}
   puts $chan {  [outdemagfile] is output demag field (.ohf) file (optional)}
   puts $chan {and options include:}
   puts $chan {  -cwd <directory>}
   puts $chan {  -Msfile <Msfile>}
   puts $chan {  -Msspec {scalar field Specify block}}
   puts $chan {  -Msmask {mask expr-expression}}
   puts $chan {  -simulationbox <xmin> <ymin> <zmin> <xmax> <ymax> <zmax>}
   puts $chan {  -outdemagenergy <demag_energy_file>}
   puts $chan {  -outmif <generated_MIF_file.mif>}
   puts $chan {  -outM <output_magnetization_file>}
   puts $chan {  -pbc <axes>}
   puts $chan {  -userscriptfile <user_energies.tcl>}
   puts $chan {  -useroutputs {outname1 outfile1 ...}}
   puts $chan {  -runboxsi <0|1> (defaults to 1 (true))}
   puts $chan {  -runopts <boxsi run options>}
   exit $errcode
}

# TODO:
# 1a) Create a new Oxs_Ext class, Oxs_FileScalarField.  This class takes
#     a field file as import and produces a scalar field. By default,
#     the scalar value at each point will be the magnitude of the field
#     there.  Optionally, for mult-component fields the user can specify
#     a single component to use instead of the vector magnitude. In this
#     case we will probably want to allow negative values; should there
#     be an additional option to control sign?
# 1b) Replace the Oxs_VecMagScalarField kludge implementation of
#     -Msinfile with Oxs_FileScalarField.
#  2) Should there be an -Msmaskargs option?

if {[lsearch -regexp $argv {-+h(|elp)$}]>=0} {
   Usage
}

set m0_infile {}
set Ms_infile {}
set Ms_spec {}
set Ms_mask {}
set simulation_box {}
set demag_outfile {}
set demagenergy_outfile {}
set magnetization_outfile {}
set pbc_axes {}
set user_scriptfile {}
set user_outputs {}
set mif_outfile {}
set run_boxsi 1
set run_boxsi_options {}

set i 0
while {$i+1<[llength $argv]} {
   set elt [lindex $argv $i]
   set nextelt [lindex $argv $i+1]
   set eat_elt 1  ;# If true remove index $i from argv
   set eat_cnt 1  ;# Number of elts following index $i to also remove
   switch -regexp $elt {
      {^-+cwd$}             { set work_dir  $nextelt }
      {^-+Msfile$}          { set Ms_infile $nextelt }
      {^-+Msspec$}          { set Ms_spec   $nextelt }
      {^-+Msmask$}          { set Ms_mask   $nextelt }
      {^-+simulationbox}    {
         if {[llength $nextelt]==6} {
            # Simulation box spec entered as a list
            set simulation_box $nextelt
            set eat_cnt 1
         } elseif {$i+6<[llength $argv]} {
            # Simulation box spec entered as 6 separate elements
            set eat_cnt 6
            set simulation_box [lrange $argv $i+1 $i+$eat_cnt]
         } else { ;# argv too short
            set eat_elt 0
         }
      }
      {^-+outdemagenergy$}  { set demagenergy_outfile   $nextelt }
      {^-+outmif$}          { set mif_outfile           $nextelt }
      {^-+outM$}            { set magnetization_outfile $nextelt }
      {^-+pbc$}             { set pbc_axes              $nextelt }
      {^-+userscriptfile$}  { set user_scriptfile       $nextelt }
      {^-+useroutputs$}     { set user_outputs          $nextelt }
      {^-+runboxsi}         { set run_boxsi             $nextelt }
      {^-+runopts}          { set run_boxsi_options     $nextelt }
      default {  set eat_elt 0 }
   }
   if { $eat_elt } {
      set argv [lreplace $argv $i $i+$eat_cnt]
   } else {
      incr i
   }
}
if {[llength $argv]<1} {
   puts stderr "ERROR: Input magnetization file required."
   Usage 9
}

if {[llength $argv]>2} {
   puts stderr "Unrecognized command line: $argv"
   Usage 9
}

set m0_infile [lindex $argv 0]
set demag_outfile [lindex $argv 1]

# Sanity checks
if { [string match {} $demag_outfile]
     && [string match {} $demagenergy_outfile]
     && [string match {} $magnetization_outfile]
     && [string match {} $user_scriptfile]
     && [string match {} $user_outputs]
     && [string match {} $mif_outfile] } {
   puts stderr "WARNING: No output selected."
}
if {![string match {} $Ms_infile] && ![string match {} $Ms_spec]} {
   puts stderr "ERROR: Both Ms_infile and Ms_spec selected; pick at most one."
   exit 11
}
if {![string match {} $pbc_axes]} {
   set pbc_axes [regsub -expanded -all {[{}(),[:space:]]} $pbc_axes {}]
   set pbc_axes [string tolower $pbc_axes]
   set pbc_axes [join [lsort -unique [split $pbc_axes {}]] {}]
   if {![regexp {^[xyz]+$} $pbc_axes]} {
      puts stderr "ERROR: -pbc axes must be a subset of \{x,y,z\}"
      exit 11
   }
}
foreach elt $simulation_box {
   if {$elt ne "-" && ![string is double $elt]} {
      puts stderr "ERROR: simulationbox should be 6 numbers"
      exit 11
   }
}

if {[info exists work_dir]} {
   if {[catch {cd $work_dir} err]} {
      puts stderr "Change directory error: $err"
      exit 13
   }
}

set user_script {}
if {![string match {} $user_scriptfile]} {
   if {[catch {open $user_scriptfile r} userchan]} {
      puts stderr "ERROR: Unable to open userscriptfile\
         \"$user_scriptfile\" --- $userchan"
      exit 11
   }
   set user_script [string trim [read $userchan]]
   close $userchan
}
if {[llength $user_outputs]%2 != 0} {
   puts stderr "ERROR: useroutputs should have an even number of components"
   exit 11
}


set output_file_assignment [dict create]
if { ![string match {} $demag_outfile] } {
   dict set output_file_assignment Oxs_Demag::Field \
      $demag_outfile
}
if { ![string match {} $demagenergy_outfile] } {
   dict set output_file_assignment [list Oxs_Demag::Energy density] \
      $demagenergy_outfile
}
if { ![string match {} $magnetization_outfile] } {
   dict set output_file_assignment [list Oxs_MinDriver::Magnetization] \
      $magnetization_outfile
}

proc ReadHeader { ovffile } {
   set chan [open $ovffile r]
   if {[gets $chan firstline]<0} {
      catch {close $chan}
      error "Empty OVF file: \"$ovffile\""
   }
   if {![regexp -nocase {^# *OOMMF.*(mesh|OVF)} $firstline]} {
      catch {close $chan}
      error "Input file \"$ovffile\" is not an OVF file."
   }
   if {[regexp -nocase {irregular *mesh} $firstline]} {
      catch {close $chan}
      error "Input file OVF \"$ovffile\" is an\
             unsupported irregular mesh type."
   }
   # Skip to start of header
   while {[gets $chan line]>=0} {
      if {[regexp -nocase {^#[ \t]*Begin:[ \t]*Header} $line]} {
         break  ;# Start of header
      }
   }
   set header [dict create]
   while {[gets $chan line]>=0} {
      if {![regexp {^#[ \t]*([^:]+):[ \t]*(.*[^ \t]*)$} \
               $line _ label value]} {
         continue ;# Assumed comment line
      }
      if {[string compare -nocase end $label]==0 \
             && [string compare -nocase header $value]==0} {
         break  ;# End of header
      }
      dict set header [string tolower $label] $value
   }
   close $chan
   if {![dict exists $header meshtype]} {
      if {[regexp -nocase {^# *OOMMF: *rectangular *mesh} $firstline]} {
         dict set header meshtype rectangular
      } else {
         error "Input OVF file \"$ovffile\" missing\
                required meshtype header entry."
      }
   }
   set meshtype [dict get $header meshtype]
   if {[string compare -nocase $meshtype rectangular]!=0} {
      error "Input OVF file \"$ovffile\" has invalid mesh\
             type \"$meshtype\"; should be rectangular"
   }
   return $header
}

proc CheckHeader { header } {
   # Check that header isn't missing needed keys
   set keycheck {
      xmin ymin zmin xmax ymax zmax
      xstepsize ystepsize zstepsize
      xbase ybase zbase
      xnodes ynodes znodes
   }
   foreach key $keycheck {
      if {![dict exists $header $key]} {
         error "Unsupported OVF file; key $key missing"
      }
   }
   dict with header {
      set xmeshmin [expr {$xbase-0.5*$xstepsize}]
      set ymeshmin [expr {$ybase-0.5*$ystepsize}]
      set zmeshmin [expr {$zbase-0.5*$zstepsize}]
      set xmeshmax [expr {$xbase+($xnodes-0.5)*$xstepsize}]
      set ymeshmax [expr {$ybase+($ynodes-0.5)*$ystepsize}]
      set zmeshmax [expr {$zbase+($znodes-0.5)*$zstepsize}]
      set eps 1e-14
      if {abs($xmin-$xmeshmin)>$eps*abs($xstepsize)
          || abs($ymin-$ymeshmin)>$eps*abs($ystepsize)
          || abs($zmin-$zmeshmin)>$eps*abs($zstepsize)
          || abs($xmax-$xmeshmax)>$eps*abs($xstepsize)
          || abs($ymax-$ymeshmax)>$eps*abs($ystepsize)
          || abs($zmax-$zmeshmax)>$eps*abs($zstepsize)} {
         error "OVF file range mismatch with mesh"
      }
   }
}

# Read and check header from m0 OVF file.
if {[catch {
   set header [ReadHeader $m0_infile]
   CheckHeader $header
} errmsg]} {
   puts stderr $errmsg
   exit 19
}

# If simulation_box specified, use those extents instead of ones from
# m0_infile
if {[llength $simulation_box]==6} {
   set box_clipped 0
   set eps 4e-16
   foreach location $simulation_box elt [list xmin ymin zmin xmax ymax zmax] {
      set filevalue [dict get $header $elt]
      if {$location eq "-"} {
         set location $filevalue
      } else {
         # Hyphen is request to use m0_infile value
         if {[string match ?min $elt]} { ;# Min side
            if {$filevalue < $location} {
               if {$location*(1-$eps) <= $filevalue} {
                  set location $filevalue ;# Allow for rounding error
               } else {
                  incr box_clipped
               }
            }
         } else { ;# Max side
            if {$location < $filevalue} {
               if {$filevalue <= $location*(1+$eps)} {
                  set location $filevalue ;# Allow for rounding error
               } else {
                  incr box_clipped
               }
            }
         }
      }
      dict set header simbox_$elt $location
   }
   if {$box_clipped} {
      puts stderr "WARNING: Magnetization outside simulationbox is ignored"
   }
} else { ;# Otherwise copy values from m0_infile
   foreach elt [list xmin ymin zmin xmax ymax zmax] {
      dict set header simbox_$elt [dict get $header $elt]
   }
}

# Create MIF script
set mifscript "\# MIF 2.2"

# Exporting the output control block to the MIF file is a little tricky,
# because we need to interpolate "$output_file_assignment" from this
# script, but not interpolate other variables inside the OutputFilename
# proc.
append mifscript [subst {

# Output control
set output_file_assignment \[dict create [dict get $output_file_assignment]\]
}]

append mifscript {
set outfilenames {}
proc OutputFilename { params } {
   global output_file_assignment outfilenames
   set output_name [dict get $params name]
   if {[dict exists $output_file_assignment $output_name]} {
      set filename [dict get $output_file_assignment $output_name]
   } else {
      # Try glob match on output_name
      set matches {}
      foreach key [dict keys $output_file_assignment] {
         if {[string match -nocase $key $output_name]} {
            lappend matches $key
         }
      }
      if {[llength $matches]==1} {
         set key [lindex $matches 0]
         set filename [dict get $output_file_assignment $key]
      } else {
         set filename [DefaultFieldFilename $params]
      }
   }
   lappend outfilenames $filename
   return $filename
}
SetOptions {
   vector_field_output_filename_script OutputFilename
   scalar_field_output_filename_script OutputFilename
}
}

set xstepsize [dict get $header xstepsize]
set ystepsize [dict get $header ystepsize]
set zstepsize [dict get $header zstepsize]

append mifscript [subst {
# Simulation volume and mesh
Specify Oxs_MultiAtlas:atlas {
   atlas { Oxs_BoxAtlas:base {
      xrange {[dict get $header xmin] [dict get $header xmax]}
      yrange {[dict get $header ymin] [dict get $header ymax]}
      zrange {[dict get $header zmin] [dict get $header zmax]}
   }}
   xrange {[dict get $header simbox_xmin] [dict get $header simbox_xmax]}
   yrange {[dict get $header simbox_ymin] [dict get $header simbox_ymax]}
   zrange {[dict get $header simbox_zmin] [dict get $header simbox_zmax]}
}}]
if {[string match {} $pbc_axes]} {
   append mifscript [subst {
Specify Oxs_RectangularMesh:mesh {
  cellsize { $xstepsize $ystepsize $zstepsize }
  atlas :atlas
}
}]
} else {
   append mifscript [subst {
Specify Oxs_PeriodicRectangularMesh:mesh {
  cellsize { $xstepsize $ystepsize $zstepsize }
  atlas :atlas
  periodic $pbc_axes
}
}]
}

append mifscript [subst {
# Input data
Specify Oxs_FileVectorField:m0 {
   file [list $m0_infile]
   norm 1.0
   spatial_scaling {1 1 1}
   spatial_offset  {0 0 0}
   exterior {1 0 0}
   comment {Use size and scale from file}
}}]

# Specify Ms; if not given separately, use m0_infile
if {![string match {} $Ms_infile]} {
   # Kludge: Until an Oxs_FileScalarField Oxs_Ext object
   # is implemented, use Oxs_VecMagScalarField
   append mifscript [subst {
Specify Oxs_VecMagScalarField:Ms {
   field { Oxs_FileVectorField {
      file [list $Ms_infile]
      spatial_scaling {1 1 1}
      spatial_offset  {0 0 0}
      exterior {0 0 0}
      comment {Use size and scale from file}
   }}
}
}]
   set Msinit :Ms
} elseif {![string match {} $Ms_spec]} {
   if {[string is double $Ms_spec]} {
      # If Ms_spec is a single number, then create a specification
      # that restricts that value to the volume covered by m0_infile,
      # and is 0 outside.
      append mifscript [subst {
Specify Oxs_AtlasScalarField:Ms {
   atlas :atlas
   default_value 0.0
   values { base $Ms_spec }
}
}]
      set Msinit :Ms
   } else {
      # $Ms_spec should be an inline scalar field specification.
      # Change this to a standalone Specify block
      set tst [string trim $Ms_spec 0]
      if {[regsub -expanded {(:[^[:space:]]*|)[[:space:]]} $tst ":Ms " tst] \
          != 1} {
         error "Unable to parse Msspec"
      }
      set Ms_spec $tst
      append mifscript "\nSpecify $Ms_spec\n"
      set Msinit :Ms
   }
} else {
   append mifscript [subst {
Specify Oxs_VecMagScalarField:Ms {
   field { Oxs_FileVectorField {
      file [list $m0_infile]
      spatial_scaling {1 1 1}
      spatial_offset  {0 0 0}
      exterior {0 0 0}
      comment {Use size and scale from file}
   }}
}
}]
   set Msinit :Ms
}

# Ms_mask
if {![string match {} $Ms_mask]} {
   append mifscript [subst -nocommands {
proc Ms_mask { x y z rx ry rz Ms mx my mz } {
   set scale [expr {$Ms_mask}]
   return [expr {\$Ms*\$scale}]
}

Specify Oxs_ScriptScalarField:Ms_mask {
   script Ms_mask
   script_args { rawpt relpt scalars vectors }
   scalar_fields $Msinit
   vector_fields :m0
   atlas :atlas
}
}]
   set Msinit Oxs_ScriptScalarField:Ms_mask
}

# Standard outputs
append mifscript [subst -nocommands {
set output_file_assignment [dict create $output_file_assignment]
}]

append mifscript {
# Energy objects
Specify Oxs_Demag {}
}

# User script
if {![string match {} $user_script] || ![string match {} $user_outputs]} {
   append mifscript {
##################################################################
### User script
}
   if {![string match {} $user_script]} {
      append mifscript "\n$user_script\n"
   }
   if {![string match {} $user_outputs]} {
      # Output selection. These overwrite or extend settings from user_script
      append mifscript "\n# Outputs\nlappend user_outputs $user_outputs\n"
   }

append mifscript {
if {[llength $user_outputs]>0} {
   foreach {output_name output_file} $user_outputs {
      if {[string match {} $output_file]} { # Remove output
         dict unset output_file_assignment $output_name
      } else { # Adds or overwites existing output
         dict set output_file_assignment $output_name $output_file
      }
   }
}
}
   append mifscript {
### User script
##################################################################
}}

append mifscript [subst {
# Run control
Specify Oxs_CGEvolve:evolve {}
Specify Oxs_MinDriver {
   evolver :evolve
   total_iteration_limit 1
   mesh :mesh
   Ms $Msinit
   m0 :m0
   checkpoint_interval -1
}
}]

# Output schedule
append mifscript {
# Outputs
if {[dict size $output_file_assignment]>0} {
   Destination archive mmArchive
   foreach output [dict keys $output_file_assignment] {
      Schedule $output archive Step 0
   }
   ExitHandler {
      Report "Output files: [lsort $outfilenames]"
   }
}
}

# Save MIF file?
if {![string match {} $mif_outfile]} {
   if {[string compare - $mif_outfile]!=0} {
      set chan [open $mif_outfile w]
   } else {
      set chan stdout
   }
   puts $chan $mifscript
   if {[string compare - $mif_outfile]!=0} {
      close $chan
   }
}

if {$run_boxsi} {
   # Create temporary MIF file in working directory
   if {![file writable .]} {
      # Q: Does this check work properly on all platforms
      #    and filesystems?
      puts stderr "Unable to write temporary MIF file into directory [pwd]"
      exit 86
   }
   set worktemplate "computefield-workmif-%02d.mif"
   for {set i 0} {$i<100} {incr i} {
      set mif_workfile [format $worktemplate $i]
      if {![catch {open $mif_workfile {WRONLY CREAT EXCL}} chan]} {
         puts $chan $mifscript
         close $chan
         set tempmif 1
         break
      }
   }
   if {!$tempmif} {
      puts stderr "Unable to create a temporary MIF file in directory [pwd]"
      exit 86
   }

   if {[info exists env(OOMMF_HOSTPORT)]} {
      set save_OOMMF_HOSTPORT $env(OOMMF_HOSTPORT)
   }

   if {[catch {
      puts "---"
      set env(OOMMF_HOSTPORT) [exec $TCLSH $OOMMF launchhost 0]
      set runoutput [exec $TCLSH $OOMMF boxsi {*}$run_boxsi_options \
                        -- $mif_workfile 2>@1]
      if {[string length $runoutput]>0} {
         puts $runoutput
      }
      puts "---"
      exec $TCLSH $OOMMF killoommf all
   } errmsg]} {
      file delete $mif_workfile
      puts stderr "RUNERROR---\n$errmsg\n---RUNERROR"
      exit 99
   }
   file delete $mif_workfile
   if {[info exists save_OOMMF_HOSTPORT]} {
      set env(OOMMF_HOSTPORT) $save_OOMMF_HOSTPORT
   }
}
