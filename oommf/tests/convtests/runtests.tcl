#!/bin/sh
# FILE: runtests.tcl
#
# The script runs the various onespin convergence tests
# and sends the output through gnuplot to plot the results.
#
#    v--Edit here if necessary \
exec tclsh "$0" ${1+"$@"}

set tcl_precision 17

set gnuplot gnuplot

set tclsh tclsh
set oommfroot ~/oommf
set oommf [file join $oommfroot oommf.tcl]
set null "/dev/null"

set outbase onespinconv
set convmif onespinconv.mif


set runtime 100e-12
set alpha 0.1

set tsteps [list 100e-12 80e-12 40e-12 20e-12 10e-12 8e-12 4e-12 2e-12 1e-12]
set solvers [list rkf54 rkf54s rkf54m rk4 rk2 rk2heun \
                srkf54 srkf54s srkf54m srk4 srk2 srk2heun \
                euler]

set outputfiles {}
foreach s $solvers {
   puts "Solver $s"

   # Clear away old data (if any)
   set solvfile "${outbase}-$s.odt"
   file delete $solvfile

   # Run boxsi for each timestep
   foreach t $tsteps {
#      eval exec [list $tclsh $oommf boxsi $convmif \
#                    -parameters "runtime $runtime alpha $alpha \
#                            solver $s timestep $t" 2> $null]
      set chan [open [list | $tclsh $oommf boxsi $convmif \
                    -parameters "runtime $runtime alpha $alpha \
                            solver $s timestep $t" 2>@ stdout] r]
      set routput [read $chan]
      close $chan
      if {![regexp \
           {Analytic solution, \(mx,my,mz\) = \(([^,]+),([^,]+),([^,]+)\)} \
              $routput dummy amx amy amz]} {
         puts stderr "Bad boxsi output: no analytic solution reported."
      }
   }

   # Extract mx, my, and mz data
   set chan [open [list | $tclsh $oommf odtcols \
                      < $solvfile *mx *my *mz -t bare] r]
   set soutput [read $chan]
   close $chan
   if {[llength $soutput] != 3*[llength $tsteps]} {
      puts stderr "Wrong odtcols output"
      exit 1
   }
   set outfile "${s}-results.dat"
   set ochan [open $outfile w]
   puts $ochan "# Results from solver $s on MIF file $solvfile"
   puts $ochan "# runtime=$runtime, alpha=$alpha"
   puts $ochan "# Analytic solution: ($amx,$amy,$amz)"
   puts $ochan \
      "#\n# Timestep (s)          mx                   my                  mz"
   foreach t $tsteps {mx my mz} $soutput {
      puts $ochan [format "%11s     %#20.16g %#20.16g %#20.16g" $t $mx $my $mz]
   }
   close $ochan
   lappend outputfiles $outfile
}

puts "Output files: $outputfiles"
puts "\nTo produce graphs of these results, modify onespin.gplt"
puts "to use analytic solution (runtime=$runtime, alpha=$alpha)"
puts " (mx,my,mz) = ($amx,$amy,$amz)"
puts "and then run gnuplot onespin.gplt."

