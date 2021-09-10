# FILE: ddtest.tcl
#
# Tcl + Mpexpr script for evaluating doubledouble.cc bulk test output.
# Input data can be generated with ddtest.cc:
#
#   ddtest Atan -2 2  1000 | tclsh ddtest.tcl -quiet
#
# The output can be used to create or augment test suites for
# Xp_DoubleDouble.
#
# NB: This script requires the Mpexpr multiple precision math Tcl
#     extension.

proc Usage {} {
   puts stderr "Usage: tclsh ddtest.tcl\
     \[-precision \#\] \[-quiet\] \[-formatsample\] <indata"
   exit 1 
}

proc GetClicks {} {
   # Returns reference time in microseconds
   if {[catch {clock microseconds} clicks]} {
      # Probably old version of Tcl that doesn't grok microseconds
      # option.  Use older "seconds" interface, converted to
      # microseconds.  Of course, this only provides second
      # granularity.
      set clicks [expr {1e6*[clock seconds]}]
   }
   return $clicks
}

set expr_pi [expr {4*atan(1.0)}]

package require Mpexpr
if {$mp_precision < 80} {
   set mp_precision 80
}

proc UpdateFloatPrecision { args } {
   global mp_precision old_float_precision float_precision
   if {$float_precision != $old_float_precision} {
      # Set rough approximates to extreme values.  These range limits
      # should really be tied to exponent size, not mantissa size,
      # but in practice this should work well enough.
      global MAXVAL MINVAL MINEXP
      if {$float_precision == 53} {
         set MAXVAL 1.7976931348623157e308
         set MINVAL 4.9406564584124654e-324  ;# Subnormal
         ## Smallest normal magnitude is 2.2250738585072014e-308
         set MINEXP -1074  ;# Subnormal
      } elseif {$float_precision == 64} {
         set MAXVAL 1.18973149535723176502e4932
         set MINVAL 3.6451995318824746e-4951  ;# Subnormal
         ## Smallest normal magnitude is 3.36210314311209350626e-4932
         set MINEXP -16445  ;# Subnormal
      } else {
         error "Unsupported float_precision: $float_precision"
      }
      set old_float_precision $float_precision
      
      # Update proc guards ##############################
      # Try to keep result magnitudes larger than PRECISION_GUARD_SIZE
      global PRECISION_GUARD_SIZE
      set PRECISION_GUARD_SIZE [mpexpr {pow(10,-$mp_precision/2)}]

      # Guards for Exp
      global EXP_CHECK_MAX EXP_CHECK_MIN EXP_TEST_MIN
      set EXP_CHECK_MAX [mpexpr {log($MAXVAL)}]
      set EXP_CHECK_MIN [mpexpr {log($MINVAL)}]
      set EXP_TEST_MIN  [mpexpr {(-$mp_precision/2)*2.3}]

      # Maximum number of decimal digits required to explicitly specify
      # a double-double binary value.
      global MAXPREC
      set MAXPREC [expr {2+int(floor((2*$float_precision+1)*log10(2)))}]
   }
}

# float_precision is the number of bits in double-precision floating
# point.  This is normally 53 for IEEE 8-byte double, or 64 for 10-byte
# "long double".
if {![info exists float_precision]} {
   set old_float_precision [set float_precision -1]
   trace add variable float_precision write UpdateFloatPrecision
   set float_precision 53
} else {
   trace remove variable float_precision write UpdateFloatPrecision
   trace add variable float_precision write UpdateFloatPrecision
   set old_float_precision -1
   set float_precision $float_precision
}

if {!$tcl_interactive} {
   if {[set index [lsearch -regexp $argv {-+(p|precision)}]]>=0} {
      set nindex [expr {$index+1}]
      set p [lindex $argv $nindex]
      if {![string is integer $p] || $p<1} { Usage }
      set mp_precision $p
      set argv [lreplace $argv $index $nindex]
   }

   set flag_quiet 0
   if {[set index [lsearch -regexp $argv {-+(q|quiet)}]]>=0} {
      set flag_quiet 1
      set argv [lreplace $argv $index $index]
   }

   set format_sample 0
   if {[set index [lsearch -regexp $argv {-+(f|formatsample)}]]>=0} {
      set format_sample 1
      set argv [lreplace $argv $index $index]
   }

   if {[llength $argv]>0} { Usage }
}

########################################################################
### Utility functions

# Write value in HexBin format
proc WriteHexBin { precision val } {

   # Inf/NaN check
   if {[regexp {inf|nan} [string tolower $val]]} {
      return $val
   }

   set precision [expr {int(round($precision))}]
   if {[mpexpr {$val == 0.0}]} {
      set exp 0
   } else {
      set exp [mpexpr {1+int(floor(log(abs($val))/log(2)))}]
   }
   set mantissa [mpexpr {$val*pow(2,-1*$exp)}]
   set mantbound [mpexpr {pow(2,$precision)}]
   set hi [mpexpr {int(round($mantissa*$mantbound))}]
   if {[mpexpr {abs($hi) >= $mantbound}]} {
      # Bad round at border
      incr exp
      set mantissa [mpexpr {$mantissa/2.0}]
      set hi [mpexpr {int(round($mantissa*$mantbound))}]
   }

   set valb [mpexpr {$mantissa*pow(2,$precision)-$hi}]
   if {[mpexpr {$valb == 0.0}]} {
      set expb 0
   } else {
      set expb [mpexpr {1+int(floor(log(abs($valb))/log(2)))}]
   }
   set mantissab [mpexpr {$valb*pow(2,-1*$expb)}]
   set lo [mpexpr {int(round($mantissab*$mantbound))}]
   if {[mpexpr {abs($lo) >= $mantbound}]} {
      # Bad round at border
      incr expb
      set mantissab [mpexpr {$mantissab/2.0}]
      set lo [mpexpr {int(round($mantissab*$mantbound))}]
   }

   if {$hi>=0} {
      set hi_sign " "
   } else {
      set hi_sign "-" ; set hi [expr {-1*$hi}]
   }
   if {$lo>=0} {
      set lo_sign " "
   } else {
      set lo_sign "-" ; set lo [expr {-1*$lo}]
   }
   set hi_exp [expr {$exp-$precision}]
   set lo_exp [expr {$hi_exp+$expb-$precision}]
   set dc [expr {($precision+3)/4}] ;# Number of hex digits
   return [format "\[%s0x%0${dc}llXxb%+03d,%s0x%0${dc}llXxb%+03d\]" \
              $hi_sign $hi $hi_exp $lo_sign $lo $lo_exp]
}

# Write value in HexBin triple-double format
proc WriteTripletHexBin { precision val } {
   set precision [expr {int(round($precision))}]
   if {[mpexpr {$val == 0.0}]} {
      set exp 0
   } else {
      set exp [mpexpr {1+int(floor(log(abs($val))/log(2)))}]
   }
   set mantissa [mpexpr {$val*pow(2,-1*$exp)}]
   set hi [mpexpr {int(round($mantissa*pow(2,$precision)))}]

   set valb [mpexpr {$mantissa*pow(2,$precision)-$hi}]
   if {[mpexpr {$valb == 0.0}]} {
      set expb 0
   } else {
      set expb [mpexpr {1+int(floor(log(abs($valb))/log(2)))}]
   }
   set mantissab [mpexpr {$valb*pow(2,-1*$expb)}]
   set mid [mpexpr {int(round($mantissab*pow(2,$precision)))}]

   set valc [mpexpr {$mantissab*pow(2,$precision)-$mid}]
   if {[mpexpr {$valc == 0.0}]} {
      set expc 0
   } else {
      set expc [mpexpr {1+int(floor(log(abs($valc))/log(2)))}]
   }
   set mantissac [mpexpr {$valc*pow(2,-1*$expc)}]
   set lo [mpexpr {int(round($mantissac*pow(2,$precision)))}]

   if {$hi>=0} {
      set hi_sign " "
   } else {
      set hi_sign "-" ; set hi [expr {-1*$hi}]
   }
   if {$mid>=0} {
      set mid_sign " "
   } else {
      set mid_sign "-" ; set mid [expr {-1*$mid}]
   }
   if {$lo>=0} {
      set lo_sign " "
   } else {
      set lo_sign "-" ; set lo [expr {-1*$lo}]
   }
   set hi_exp  [expr {$exp-$precision}]
   set mid_exp [expr {$hi_exp+$expb-$precision}]
   set lo_exp  [expr {$mid_exp+$expc-$precision}]
   set dc [expr {($precision+3)/4}] ;# Number of hex digits
   return [format \
    "\[%s0x%0${dc}llXxb%+03d,%s0x%0${dc}llXxb%+03d,%s0x%0${dc}llXxb%+03d\]" \
    $hi_sign $hi $hi_exp $mid_sign $mid $mid_exp $lo_sign $lo $lo_exp]
}

proc WriteSingleHexBin { precision val } {
   # Write value as a single hexbin value (as opposed to a double-double pair)

   # Inf/NaN check
   if {[regexp {inf|nan} [string tolower $val]]} {
      return $val
   }

   set precision [expr {int(round($precision))}]
   if {[mpexpr {$val == 0.0}]} {
      set exp 0
   } else {
      set exp [mpexpr {1+int(floor(log(abs($val))/log(2)))}]
   }
   set mantissa [mpexpr {$val*pow(2,-1*$exp)}]
   set mantbound [mpexpr {pow(2,$precision)}]
   set hi [mpexpr {int(round($mantissa*$mantbound))}]
   if {[mpexpr {abs($hi) >= $mantbound}]} {
      # Bad round at border
      incr exp
      set mantissa [mpexpr {$mantissa/2.0}]
      set hi [mpexpr {int(round($mantissa*$mantbound))}]
   }

   if {$hi>=0} {
      set hi_sign " "
   } else {
      set hi_sign "-" ; set hi [expr {-1*$hi}]
   }
   set hi_exp [expr {$exp-$precision}]
   set dc [expr {($precision+3)/4}] ;# Number of hex digits
   return [format "%s0x%0${dc}llXxb%+03d" $hi_sign $hi $hi_exp]
}

proc WriteHexFloat { val } {
   # Based on WriteHexBin, but using the C99 %a format

   global float_precision mp_precision
   set precision $float_precision

   # Inf/NaN check
   if {[regexp {inf|nan} [string tolower $val]]} {
      return $val
   }

   set save_precision $mp_precision
   set mp_precision [expr {3*$precision}]
   
   if {[mpexpr {$val == 0.0}]} {
      set exp 0
   } else {
      set exp [mpexpr {1+int(floor(log(abs($val))/log(2)))}]
   }
   set mantissa [mpexpr {$val*pow(2,-1*$exp)}]
   set mantbound [mpexpr {pow(2,$precision)}]
   set hi [mpexpr {int(round($mantissa*$mantbound))}]
   if {[mpexpr {abs($hi) >= $mantbound}]} {
      # Bad round at border
      incr exp
      set mantissa [mpexpr {$mantissa/2.0}]
      set hi [mpexpr {int(round($mantissa*$mantbound))}]
   }

   set valb [mpexpr {$mantissa*pow(2,$precision)-$hi}]
   if {[mpexpr {$valb == 0.0}]} {
      set expb 0
   } else {
      set expb [mpexpr {1+int(floor(log(abs($valb))/log(2)))}]
   }
   set mantissab [mpexpr {$valb*pow(2,-1*$expb)}]
   set lo [mpexpr {int(round($mantissab*$mantbound))}]
   if {[mpexpr {abs($lo) >= $mantbound}]} {
      # Bad round at border
      incr expb
      set mantissab [mpexpr {$mantissab/2.0}]
      set lo [mpexpr {int(round($mantissab*$mantbound))}]
   }

   if {$hi>=0} {
      set hi_sign " "
   } else {
      set hi_sign "-" ; set hi [expr {-1*$hi}]
   }
   if {$lo>=0} {
      set lo_sign " "
   } else {
      set lo_sign "-" ; set lo [expr {-1*$lo}]
   }
   set hi_exp [expr {$exp-$precision}]
   set lo_exp [expr {$hi_exp+$expb-$precision}]
   set dc [expr {($precision+3)/4}] ;# Number of hex digits

   # Add radix point and adjust exponent accordingly
   set hi_mant [format "%0${dc}llX" $hi]
   set hi_mant "[string range $hi_mant 0 0].[string range $hi_mant 1 end]"
   set hi_exp [expr {$hi_exp + 4*($dc-1)}]
   set lo_mant [format "%0${dc}llX" $lo]
   set lo_mant "[string range $lo_mant 0 0].[string range $lo_mant 1 end]"
   if {[mpexpr {$lo == 0.0}]} {
      set lo_exp 0
   } else {
      set lo_exp [expr {$lo_exp + 4*($dc-1)}]
   }
   set result [format "\[%s0x%sp%+05d,%s0x%sp%+05d\]" \
                  $hi_sign $hi_mant $hi_exp $lo_sign $lo_mant $lo_exp]

   set mp_precision $save_precision
   return $result
}

proc EstimateULP { precision val } {
   if {[mpexpr {$val == 0.0}]} { return 0.0 }
   global MINEXP
   set log2 \
    0.693147180559945309417232121458176568075500134360255254120680009493393622
   set exp [mpexpr {1+int(floor(log(abs($val))/$log2))-$precision}]
   if {$exp<$MINEXP} {
      set exp $MINEXP  ;# Underflow
   }

   # Assume that the bog-standard expr only supports exponent values
   # up to 1024.  If we are testing a precision beyond that, use
   # mpexpr to compute pow(2,$exp)
   if {abs($exp) < 1024} {
      return [expr {pow(2,$exp)}]
   }
   # Otherwise, expr doesn't have enough bits
   global mp_precision
   set log_10_2 0.3010299956639812
   set test [expr {($precision-$exp)*$log_10_2}]
   if {$test>$mp_precision} {
      set save_precision $mp_precision
      set mp_precision [expr {int(ceil($test))}]
      set result [mpexpr {pow(2,$exp)}]
      set mp_precision $save_precision
   } else {
      set result [mpexpr {pow(2,$exp)}]
   }
   return [mpformat %.16e $result]
}

# Read value in HexBin format
proc ReadHexBin { val mantissa_width_name work_precision_name } {
   # If optional import mantissa_width_name has string length greater
   # than zero, then it is interpreted as the name in the caller of a
   # variable to store the inferred width of the mantissa, in bits.  If
   # the mantissa width can not be determined from the input val, then
   # the mantissa width export is not set.
   if {[string length $mantissa_width_name]>0} {
      upvar $mantissa_width_name mantissa_width
   }
   if {[string length $work_precision_name]>0} {
      upvar $work_precision_name work_precision
   }

   if {[regexp \
           {\[ *([-+]?0x([0-9a-fA-F]+))xb([-+0-9]+),\
               *([-+]?0x([0-9a-fA-F]+))xb([-+0-9]+) *\]} \
           $val dummy a am b c cm d]} {
      # Determine mantissa width, in bits
      set maxsize 0
      foreach mv [list $am $cm] {
         set size [expr {4*[string length $mv]}]
         set prefix "0x[string index $mv 0]"
         if {$prefix==0} {
            set size 0 ;# Number is 0; can't tell precision
         } elseif {$prefix<2} {
            incr size -3
         } elseif {$prefix<4} {
            incr size -2
         } elseif {$prefix<8} {
            incr size -1
         }
         if {$size>$maxsize} { set maxsize $size }
      }
      if {$maxsize>0} {
         set mantissa_width $maxsize
      }
   } elseif {[regexp \
           {\[ *([-+]?[.#0-9a-fA-FxXiInNqQsS]+),\
               *([.#0-9a-fA-FxXiInNbqQsS+-]+) *\]} $val dummy a c]} {
      set b 0
      set d 0
      # Can't determine mantissa width from this format
   } else {
      puts "BADVAL: $val"
      error "Unrecognized value format"
   }

   # Inf/NaN check
   if {[regexp -nocase {inf} $a]} {
      if {[string match {-*} $a]} {
         return "-Inf"
      } else {
         return "Inf"
      }
   }
   if {[regexp -nocase {nan} $a]} {
      return "NaN"
   }

   # Strip leading zeros, if any
   regsub -- {^([+]|)(-?(0x|))(0*)(.+)$} $a {\2\5} a
   regsub -- {^([+]|)(-?(0x|))(0*)(.+)$} $b {\2\5} b
   regsub -- {^([+]|)(-?(0x|))(0*)(.+)$} $c {\2\5} c
   regsub -- {^([+]|)(-?(0x|))(0*)(.+)$} $d {\2\5} d

   # Estimate and adjust precision needed to capture value to requested
   # accuracy
   global mp_precision
   set save_precision $mp_precision
   set add_precision [expr {int(ceil((2-[string length $a])*1.20412
                                     -$b*0.30103))+12}]
   if {$add_precision>0} {
      set mp_precision [mpexpr {$mp_precision+$add_precision}]
   }
   set work_precision $mp_precision
   if {[catch {mpexpr {$a*pow(2,$b) + $c*pow(2,$d)}} result]} {
      puts stderr "ERROR processing value $val"
      puts stderr "a=$a";   puts stderr "b=$b"
      puts stderr "c=$c";   puts stderr "d=$d"
      puts stderr "ERRMSG: $result"
      set mp_precision $save_precision
      error "Try again"
   }
   set mp_precision $save_precision
   return $result
}

# Read three values in HexBin format
proc ReadTripletHexBin { val {mantissa_width_name {}}} {
   # If optional import mantissa_width_name has string length greater
   # than zero, then it is interpreted as the name in the caller of a
   # variable to store the inferred width of the mantissa, in bits.  If
   # the mantissa width can not be determined from the input val, then
   # the mantissa width export is not set.

   if {[string length $mantissa_width_name]>0} {
      upvar $mantissa_width_name mantissa_width
   }

   if {[regexp \
         {\[ *([-+]?0x([0-9a-fA-F]+))xb([-+0-9]+),\
             *([-+]?0x([0-9a-fA-F]+))xb([-+0-9]+),\
             *([-+]?0x([0-9a-fA-F]+))xb([-+0-9]+) *\]} \
           $val dummy a am ae b bm be c cm ce]} {
      # Determine mantissa width, in bits
      set maxsize 0
      foreach mv [list $am $bm $cm] {
         set size [expr {4*[string length $mv]}]
         set prefix "0x[string index $mv 0]"
         if {$prefix==0} {
            set size 0 ;# Number is 0; can't tell precision
         } elseif {$prefix<2} {
            incr size -3
         } elseif {$prefix<4} {
            incr size -2
         } elseif {$prefix<8} {
            incr size -1
         }
         if {$size>$maxsize} { set maxsize $size }
      }
      if {$maxsize>0} {
         set mantissa_width $maxsize
      }
   } elseif {[regexp \
         {\[ *([-+]?[0-9a-fA-FxXiInNqQsS]+),\
             *([.#0-9a-fA-FxXiInNbqQsS+-]+),\
             *([.#0-9a-fA-FxXiInNbqQsS+-]+) *\]} $val dummy a b c]} {
      set ae 0
      set be 0
      set ce 0
      # Can't determine mantissa width from this format
   } else {
      puts "BADVAL: $val"
      error "Unrecognized value format"
   }

   # Inf/NaN check
   if {[regexp -nocase {inf} $a]} {
      if {[string match {-*} $a]} {
         return "-Inf"
      } else {
         return "Inf"
      }
   }
   if {[regexp -nocase {nan} $a]} {
      return "NaN"
   }

   # Strip leading zeros, if any
   regsub -- {^([+]|)(-?(0x|))(0*)(.+)$} $a  {\2\5} a
   regsub -- {^([+]|)(-?(0x|))(0*)(.+)$} $ae {\2\5} ae
   regsub -- {^([+]|)(-?(0x|))(0*)(.+)$} $b  {\2\5} b
   regsub -- {^([+]|)(-?(0x|))(0*)(.+)$} $be {\2\5} be
   regsub -- {^([+]|)(-?(0x|))(0*)(.+)$} $c  {\2\5} c
   regsub -- {^([+]|)(-?(0x|))(0*)(.+)$} $ce {\2\5} ce

   # Estimate and adjust precision needed to capture value to requested
   # accuracy
   global mp_precision
   set save_precision $mp_precision
   set add_precision [expr {int(ceil((2-[string length $a])*1.20412
                                     -$ae*0.30103))+12}]
   if {$add_precision>0} {
      set mp_precision [mpexpr {$mp_precision+$add_precision}]
   }
   if {[catch {mpexpr {$a*pow(2,$ae)
                       + $b*pow(2,$be) + $c*pow(2,$ce)}} result]} {
      puts stderr "ERROR processing value $val"
      puts stderr "a=$a";   puts stderr "ae=$ae"
      puts stderr "b=$b";   puts stderr "be=$be"
      puts stderr "c=$c";   puts stderr "ce=$ce"
      puts stderr "ERRMSG: $result"
      set mp_precision $save_precision
      error "Try again"
   }
   set mp_precision $save_precision
   return $result
}

# Read a single, bare hexbin value (as opposed to a double-double pair)
proc ReadSingleHexBin { val {mantissa_width_name {}}} {
   # If optional import mantissa_width_name has string length greater
   # than zero, then it is interpreted as the name in the caller of a
   # variable to store the inferred width of the mantissa, in bits.  If
   # the mantissa width can not be determined from the input val, then
   # the mantissa width export is not set.
   if {[string length $mantissa_width_name]>0} {
      upvar $mantissa_width_name mantissa_width
   }

   if {[regexp \
           { *([-+]?0x([0-9a-fA-F]+))xb([-+0-9]+)} \
           $val dummy a am b]} {
      # Determine mantissa width, in bits
      set size [expr {4*[string length $am]}]
      set prefix "0x[string index $am 0]"
      if {$prefix==0} {
         set size 0 ;# Number is 0; can't tell precision
      } elseif {$prefix<2} {
         incr size -3
      } elseif {$prefix<4} {
         incr size -2
      } elseif {$prefix<8} {
         incr size -1
      }
      set maxsize $size
      if {$maxsize>0} {
         set mantissa_width $maxsize
      }
   } elseif {[regexp { *([-+]?[.#0-9a-fA-FxXiInNqQsS]+)} \
                 $val dummy a]} {
      set b 0
      # Can't determine mantissa width from this format
   } else {
      puts "BADVAL: $val"
      error "Unrecognized value format"
   }

   # Inf/NaN check
   if {[regexp -nocase {inf} $a]} {
      if {[string match {-*} $a]} {
         return "-Inf"
      } else {
         return "Inf"
      }
   }
   if {[regexp -nocase {nan} $a]} {
      return "NaN"
   }

   # Strip leading zeros, if any
   regsub -- {^([+]|)(-?(0x|))(0*)(.+)$} $a {\2\5} a
   regsub -- {^([+]|)(-?(0x|))(0*)(.+)$} $b {\2\5} b

   # Estimate and adjust precision needed to capture value to requested
   # accuracy
   global mp_precision
   set save_precision $mp_precision
   set add_precision [expr {int(ceil((2-[string length $a])*1.20412
                                     -$b*0.30103))+12}]
   if {$add_precision>0} {
      set mp_precision [mpexpr {$mp_precision+$add_precision}]
   }
   if {[catch {mpexpr {$a*pow(2,$b)}} result]} {
      puts stderr "ERROR processing value $val"
      puts stderr "a=$a";   puts stderr "b=$b"
      puts stderr "ERRMSG: $result"
      set mp_precision $save_precision
      error "Try again"
   }
   set mp_precision $save_precision
   return $result
}

# Decompose floating point number x into n pieces of m bits each.
# Return is a list of length n.  Each element of the list is two
# integers.  The first integer in each sublist is the mantissa, the
# second integer is the base 2 exponent.  For example, the return
# value {{a1 b1} {a2 b2} {a3 b3}} represents the number
#
#   a1 * 2^b1  +  a2 * 2^b2  +  a3 * 2^b3
#
# Each of the ai is m bits wide, with leading bit 1.  The exponents
# satisfy b_i - b_{i+1} >= m.  Each of the ai's may be positive or
# negative.
#
proc Decompose { x n m } {
   global mp_precision
   set keep_precision $mp_precision
   if {$mp_precision < ($n+1)*($m+1)*log(2)/log(10)} {
       set mp_precision [expr {int(ceil(($n+1)*($m+1)*log(2)/log(10)))}]
   }
   set origx $x
   set rlist {}
   while {[llength $rlist]<$n} {
      if {[mpexpr {$x==0}]} {
         while {[llength $rlist]<$n} {
            lappend rlist [list 0.0 0]
         }
         break
      }
      set ax [mpexpr {abs($x)}]
      set b [mpexpr {-1*int(floor(log($ax)/log(2)))}]
      while {[mpexpr {$ax >= pow(0.5,$b)}]} { incr b -1 }
      incr b $m
      while {[mpexpr {$ax < pow(2,$m-1-$b)}]} { incr b }
      set a [mpexpr {round($x*pow(2,$b))}]
      set b [expr {-1*$b}]
      lappend rlist [list $a $b]
      set x [mpexpr {$x - $a*pow(2,$b)}]
      # Check constraints on a
      if {[mpexpr {abs($a)<pow(2,$m-1)}] || [mpexpr {abs($a)>=pow(2,$m)}]} {
         set mp_precision $keep_precision
         return -code error "Algorithm error: a=$a"
      }
   }
   # Check constraints on b's
   foreach {a0 b0} [lindex $rlist 0] {}
   foreach elt [lrange $rlist 1 end] {
      foreach {a1 b1} $elt {}
      if {$a1!= 0.0 && $b0-$b1 < $m} {
         set mp_precision $keep_precision
         return -code error "Algoritm error: bi=$b0, bi+1=$b1"
      }
      set b0 $b1
   }

   # Check relative error
   set check 0.0
   foreach elt $rlist {
      foreach {a b} $elt {}
      set check [mpexpr {$check + $a*pow(2,$b)}]
   }
   set checkerr [mpexpr {($check - $origx)}]
   if {[mpexpr {abs($checkerr)
        > abs($origx)*pow(2,-1*($m+($n-1)*($m+1)+1))}]} {
      set mp_precision $keep_precision
      return -code error \
         "Algorithm error: check error = [mpformat %.3e $checkerr]"
   }

   set mp_precision $keep_precision
   return $rlist
}

# Split is very similar to Decompose, except that the return list
# is a list of single integers.  The first integer is the power of
# 2 to apply to the next item on the list.  Each element after that
# is multiplied by a factor of 1/2^m less than the one before it.
# That is, the return list {b a1 a2 a3} represents the number
#
#   a1 * 2^b  +  a2 * 2^(b-m)  +  a3 * 2^(b-2m)
#
# All of the integers after the first will have the same sign, positive
# or negative according to the sign of import x.
proc Split { x n m } {
   global mp_precision
   set keep_precision $mp_precision
   set precoff [mpexpr {ceil(3-log(abs($x))/log(10))}]
   if {$mp_precision < $n*$m*log(2)/log(10)+$precoff} {
      set mp_precision [expr {int(ceil($n*$m*log(2)/log(10)+$precoff))}]
   }
   set origx $x
   set signx [mpexpr {$x>=0 ? 1 : -1}]
   set x [mpexpr {abs($x)}]
   set b [mpexpr {-1*int(floor(log($x)/log(2)))}]
   while {[mpexpr {$x >= pow(0.5,$b)}]} { incr b -1 }
   incr b $m
   while {[mpexpr {$x < pow(2,$m-1-$b)}]} { incr b }
   set x [mpexpr {$x*pow(2,$b)}]
   set a [mpexpr {int(floor($x))}]
   set rlist [list [expr {-1*$b}] $a]
   set x [mpexpr {$x - $a}]
   while {[llength $rlist]<$n+1} {
      if {[mpexpr {$x==0}]} {
         while {[llength $rlist]<$n+1} {
            lappend rlist 0
         }
         break
      }
      set x [mpexpr {$x*pow(2,$m)}]
      set a [mpexpr {int(floor($x))}]
      lappend rlist $a
      set x [mpexpr {$x - $a}]
   }
   # Check constraints on a's
   set amax [mpexpr {pow(2,$m)}]
   foreach a [lrange $rlist 1 end] {
      if {[mpexpr {$a>=$amax}]} {
         set mp_precision $keep_precision
         return -code error "Algorithm error: a=$a"
      }
   }
   # Adjust signs on a's
   if {$signx < 0} {
      set tlist [lindex $rlist 0]
      foreach a [lrange $rlist 1 end] {
         lappend tlist [mpexpr {-1*$a}]
      }
      set rlist $tlist
   }
   # Check relative error
   set check 0.0
   set b [lindex $rlist 0]
   foreach a [lrange $rlist 1 end] {
      set check [mpexpr {$check + $a*pow(2,$b)}]
      incr b [expr {-1*$m}]
   }
   set checkerr [mpexpr {($check - $origx)}]
   if {[mpexpr {abs($checkerr)*pow(2,$m*$n-1) > abs($origx)}]} {
      set errmsg "Algorithm error: check error =\
          [mpformat %.3e [mpexpr {abs($checkerr)}]]"
      incr mp_precision 10
      set allowed_err [mpexpr {abs($origx)*pow(2,-1*$m*$n+1)}]
      append errmsg "\n              \
               Allowed error = [mpformat %.3e $allowed_err]"
      set mp_precision $keep_precision
      return -code error $errmsg
   } else {
      set rerr [mpformat "%.3e" [mpexpr {$checkerr/$origx}]]
      set aerr [mpformat "%.3e" [mpexpr {pow(2,-1*$m*$n+1)}]]
      puts [format "RELATIVE ERR: %s" $rerr]
      puts [format "ALLOWED  ERR: %*s" [string length $rerr] $aerr]
   }

   set mp_precision $keep_precision
   return $rlist
}

# Unsplit undoes Split.  Import splitval should be a list as returned
# from Split; in particular, the first element should be a power of
# two offset.  The second import, m, specifies the width in bits of
# each of the subsequent elements of splitval.  This should be the
# same value of m used to split the list in the call to Split.
# Returned value is the rejoined value.
proc Unsplit { splitval m } {
   set b [lindex $splitval 0]
   set twom [mpexpr {pow(2,$m)}]
   set sum [lindex $splitval 1]
   foreach elt [lrange $splitval 2 end] {
      set sum [mpexpr {$sum*$twom + $elt}] ;# Integer sums
   }
   return [mpexpr {$sum/pow(2,([llength $splitval]-2)*$m - $b)}]
}

proc XpBigFloatVecInitString { x n m } {
   # Uses Split to construct a string that can be directly copied
   # for use as an initializer string for the Xp_BigFloatVec struct
   # defined in xp_common.h.
   set sign 1
   if {$x<0} {
      set sign -1
      set x [mpexpr {-1*$x}]
   }
   set data [Split $x $n $m]
   set maxwidth 1
   foreach elt [lrange $data 1 end] {
      set width [string length $elt]
      if {$width>$maxwidth} { set maxwidth $width }
   }
   set numfmt "%${maxwidth}s"
   set colcnt [expr {(79-3)/($maxwidth+2)}]
   if {$colcnt<1} { set colcnt 1 }
   set initstring "{\n  $sign, [lindex $data 0], $m,\n  {"
   set i 0
   foreach elt [lrange $data 1 end-1] {
      append initstring [format " $numfmt," $elt]
      if {[incr i]==$colcnt} {
         append initstring "\n   "
         set i 0
      }
   }
   append initstring [format " $numfmt}\n}" [lindex $data end]]
   return $initstring
}

proc ReadXpBigFloatVecInitString { initstring } {
   # Converts return value from XpBigFloatVecInitString
   # back into a mpexpr number
   set initlist [string map { \{ "" \} "" , " "} $initstring]
   set sign [lindex $initlist 0]
   if {$sign<0} {
      set sign "-"
   } else {
      set sign ""
   }
   set offset [lindex $initlist 1]
   set width [lindex $initlist 2]
   set splitval [concat $offset [lrange $initlist 3 end]]
   return "${sign}[Unsplit $splitval $width]"
}

# Exceptional values
proc IsNan { x } {
   if {[regexp {^[.0-9eE+-]+$} $x]} { return 0 }
   if {[regexp -nocase {nan} $x]}   { return 1 }
   return 0
}
proc IsInf { x } {
   if {[regexp {^[.0-9eE+-]+$} $x]} { return 0 }
   if {[regexp -nocase {inf} $x]}   { return 1 }
   return 0
}

proc FormatMpNum { width precreq val } {
   # Formats an Mpexpr val in proper exponential (scientific) notation.
   # The string output is flush right in the a field of the specified
   # width with precision digits to the right of the decimal place.  The
   # exponent field has the form "esddd" where s is + or -, and ddd are
   # three decimal digits.
   global float_precision MAXPREC
   
   set ddbits [expr {2*$float_precision + 1}]

   if {$precreq<$MAXPREC} {
      set precision $precreq
   } else {
      set precision $MAXPREC
   }
   set val [mpformat "%.${precision}e" $val]
   if {[regexp {^([^e]*)(|e(.*))} $val dum0 mant dum1 power]} {
      if {[string match {} $power]} { set power 0 }
      # mpformat %e always prints 0 as 0.0; pad to requested precision
      if {[string match 0.0 $mant]} {
         set mant 0.
         for {set i 0} {$i<$precision} {incr i} {
            append mant 0
         }
      }
      set newval [format "%se%+04d" $mant $power]
   } else {
      puts stderr "Unsupported mpformat output"
      exit 99
   }
   if {$precreq>$precision} {
      # Add trailing zeros
      set eindex [string first e $newval]
      set newval [string replace $newval $eindex $eindex \
                     "[string repeat 0 [expr {$precreq-$precision}]]e"]
   }
   if {[string index $newval 0] != "-"} {
      # Add leading pad space for non-negative values
      set newval " $newval"
   }
   return [format "%${width}s" $newval]
}

proc RoundToDouble { x } {
   global float_precision MINEXP
   if {$x==0} { return 0 }
   if {$x<0} {
      set sign -1
      set x [mpexpr {-1*$x}]
   } else {
      set sign 1
   }
   set lx [mpexpr {log($x)/log(2)}]
   set pot [mpexpr {floor($lx)}]
   set powadj [expr {$float_precision-1-$pot}]
   if {$powadj>1-$MINEXP} {
      set powadj [expr {1-$MINEXP}]
   }
   set y [mpexpr {$x*pow(2,$powadj)}]
   set iy [mpexpr {round($y)}]
   set z [mpexpr {$sign*$iy*pow(2,-$powadj)}]
   return $z
}

proc RoundToDoubleDouble { x {y {}}} {
   if {![string match {} $y]} {
      set x [RoundToDouble $x]
      set y [RoundToDouble $y]
      return [mpexpr {$x + $y}]
   }
   
   global float_precision MINEXP
   set ddbits [expr {2*$float_precision + 1}]
   
   if {$x==0} { return 0 }
   if {$x<0} {
      set sign -1
      set x [mpexpr {-1*$x}]
   } else {
      set sign 1
   }
   set lx [mpexpr {log($x)/log(2)}]
   set pot [mpexpr {floor($lx)}]
   set powadj [expr {$ddbits-1-$pot}]
   if {$powadj>1-$MINEXP} {
      set powadj [expr {1-$MINEXP}]
   }
   set y [mpexpr {$x*pow(2,$powadj)}]
   set iy [mpexpr {round($y)}]
   set z [mpexpr {$sign*$iy*pow(2,-$powadj)}]
   return $z
}

########################################################################

### Supported test functions

# Compute sum
proc Add { a b } {
   return [mpexpr {$a+$b}]
}
proc ExprAdd { a b } {
   return [expr {$a+$b}]
}


# Compute difference
proc Subtract { a b } {
   return [mpexpr {$a-$b}]
}
proc ExprSubtract { a b } {
   return [expr {$a-$b}]
}

# Compute multiply
proc Multiply { a b } {
   if {[mpexpr {$a == 0.0 || $b == 0.0}]} {
      return 0.0
   }
   global PRECISION_GUARD_SIZE
   set result [mpexpr {$a*$b}]
   if {[mpexpr {abs($result)>$PRECISION_GUARD_SIZE}]} {
      return $result
   }
   # Precision loss.  Try increasing precision for this one operation.
   # Note that subsequent operations at lower precision may result in
   # data loss.
   global mp_precision
   set save_precision $mp_precision
   set mp_precision [mpexpr {($mp_precision+1)/2 + 2 \
                             - int(log10(abs($a))) - int(log10(abs($b)))}]
   set result [mpexpr {$a*$b}]
   set mp_precision $save_precision
   return $result
}
proc Multiply_sd_dd { a b } {
   return [Multiply $a $b]
}
proc Multiply_dd_sd { a b } {
   return [Multiply $a $b]
}
proc ExprMultiply { a b } {
   return [expr {$a*$b}]
}
proc ExprMultiply_sd_dd { a b } {
   return [expr {$a*$b}]
}
proc ExprMultiply_dd_sd { a b } {
   return [expr {$a*$b}]
}


# Compute divison
proc Divide { a b } {
   if {[mpexpr {$b == 0.0}]} {
      if {$a>0.0} {
         return Inf
      } elseif {$a<0.0} {
         return -Inf
      }
      return NaN
   }
   if {[mpexpr {$a == 0.0}]} {
      return 0.0
   }
   global PRECISION_GUARD_SIZE
   set result [mpexpr {$a/$b}]
   if {[mpexpr {abs($result)>$PRECISION_GUARD_SIZE}]} {
      return $result
   }

   # Precision loss.  Try increasing precision for this one operation.
   # Note that subsequent operations at lower precision may result in
   # data loss.
   global mp_precision
   set save_precision $mp_precision
   set mp_precision [mpexpr {($mp_precision+1)/2 + 2 \
                             + int(log10(abs($b))) - int(log10(abs($a)))}]
   set result [mpexpr {$a/$b}]
   set mp_precision $save_precision
   return $result
}
proc DDDivide { a b } { return [Divide $a $b] }

proc ExprDivide { a b } {
   if {$a == 0.0 && $b == 0.0} {
      return "NaN"
   }
   return [expr {$a/$b}]
}
proc ExprDDDivide { a b } { return [ExprDivide $a $b] }

# Compute reciprocal
proc Recip { x } {
   if {[mpexpr {$x == 0.0}]} {
      return Inf
   }
   global PRECISION_GUARD_SIZE
   set result [mpexpr {1.0/$x}]
   if {[mpexpr {abs($result)>$PRECISION_GUARD_SIZE}]} {
      return $result
   }
   # Precision loss.  Try increasing precision for this one operation.
   # Note that subsequent operations at lower precision may result in
   # data loss.
   global mp_precision
   set save_precision $mp_precision
   set mp_precision [mpexpr {($mp_precision+1)/2 + 1 + int(log10(abs($x)))}]
   set result [mpexpr {1.0/$x}]
   set mp_precision $save_precision
   return $result
}
proc DivideRecip { x } { return [Recip $x] }

proc ExprRecip { x } {
   return [expr {1.0/$x}]
}
proc ExprDivideRecip { x } { return [ExprRecip $x] }

# Squares and roots
proc Square { x } {
   return [mpexpr {$x*$x}]
}
proc SquareM1 { x } {
   # Computes (1+x)^2 - 1 = x^2 + 2.x
   return [mpexpr {$x*($x + 2)}]
}
proc Sqrt { x } {
   return [mpexpr {sqrt($x)}]
}
proc RecipSqrt { x } {
   if {[mpexpr {$x == 0.0}]} {
      return Inf
   }
   global PRECISION_GUARD_SIZE
   set result [mpexpr {1.0/sqrt($x)}]
   if {[mpexpr {abs($result)>$PRECISION_GUARD_SIZE}]} {
      return $result
   }
   # Precision loss.  Try increasing precision for this one operation.
   # Note that subsequent operations at lower precision may result in
   # data loss.
   global mp_precision
   set save_precision $mp_precision
   set mp_precision [mpexpr {($mp_precision+1)/2 + 1 + int(log10(0.5*$x))}]
   set result [mpexpr {1.0/sqrt($x)}]
   set mp_precision $save_precision
   return $result
}

proc ExprSquare { x } {
   return [expr {$x*$x}]
}
proc ExprSquareM1 { x } {
   # Computes (1+x)^2 - 1 = x^2 + 2.x
   return [expr {$x*($x + 2)}]
}
proc ExprSqrt { x } {
   return [expr {sqrt($x)}]
}
proc ExprRecipSqrt { x } {
   if {$x == 0.0} {
      return Inf
   }
   return [expr {sqrt(1.0/$x)}]
}

# Arithmetic-geometric mean
proc AGM { a0 b0 error } {
   while { [mpexpr {abs($a0-$b0) >= $error*0.5*abs($a0+$b0)}] } {
      set t [mpexpr {0.5*($a0+$b0)}]
      set b0 [mpexpr {sqrt($a0*$b0)}]
      set a0 $t
   }
   return [mpexpr {0.5*($a0+$b0)}]
}
proc ExprAGM { a0 b0 error } {
   while {abs($a0-$b0) >= $error*0.5*abs($a0+$b0)} {
      set t [expr {0.5*($a0+$b0)}]
      set b0 [expr {sqrt($a0*$b0)}]
      set a0 $t
   }
   return [expr {0.5*($a0+$b0)}]
}

# Exponential and logs
proc Exp { x } {
   global EXP_CHECK_MIN EXP_CHECK_MAX EXP_TEST_MIN
   if {[mpexpr {$x<$EXP_CHECK_MIN}]} {
      return 0.0
   }
   if {[mpexpr {$x>$EXP_CHECK_MAX}]} {
      return Inf
   }
   if {[mpexpr {$x>$EXP_TEST_MIN}]} {
      return [mpexpr {exp($x)}]
   }
   # Otherwise we need extra precision.  For very negative
   # values it can be thousands of times faster to compute
   # exp for -x, and then increase the precision only for
   # computing 1/exp(-x).
   global mp_precision
   set y [mpexpr {exp(-1*$x)}]
   set save_precision $mp_precision
   set mp_precision [expr {int(ceil($mp_precision/2 - $x/2.3))}]
   set result [mpexpr {1.0/$y}]
   set mp_precision $save_precision
   return $result
}
proc Exp1m { x } {
   global EXP_CHECK_MIN EXP_CHECK_MAX EXP_TEST_MIN
   if {[mpexpr {$x<$EXP_CHECK_MIN}]} {
      return -1.0
   }
   if {[mpexpr {$x>$EXP_CHECK_MAX}]} {
      return Inf
   }
   if {[mpexpr {$x>$EXP_TEST_MIN}]} {
      return [mpexpr {exp($x) - 1.0}]
   }
   # Otherwise we need extra precision.
   global mp_precision
   set y [mpexpr {exp(-1*$x)}]
   set save_precision $mp_precision
   set mp_precision [expr {int(ceil($mp_precision/2 - $x/2.3))}]
   set result [mpexpr {1.0/$y - 1.0}]
   set mp_precision $save_precision
   return $result
}
proc Log { x } {
   if {[mpexpr {$x==0.0}]} {
      return -Inf
   }
   return [mpexpr {log($x)}]
}
proc Log1p { x } {
   # Computes log(1+x)
   if {[mpexpr {$x == -1.0}]} {
      return -Inf
   }
   return [mpexpr {log(1+$x)}]
}

proc ExprExp { x } {
   if {$x<-1000.0} {
      return 0.0
   }
   if {$x>1000.0} {
      return Inf
   }
   return [expr {exp($x)}]
}
proc ExprExp1m { x } {
   if {$x<-1000.0} {
      return -1.0
   }
   if {$x>1000.0} {
      return Inf
   }
   return [expr {exp($x)-1}]
}
proc ExprLog { x } {
   if {$x==0.0} {
      return -Inf
   }
   return [expr {log($x)}]
}
proc ExprLog1p { x } {
   # Computes log(1+x)
   if {[mpexpr {$x == -1.0}]} {
      return -Inf
   }
   return [expr {log(1+$x)}]
}


# Trig
proc Reduce { angle } { ;# Reduce import modulo pi
   global mp_precision
   set save_precision $mp_precision
   if {[mpexpr {abs($angle)>1}]} {
      set mp_precision \
          [mpexpr {$mp_precision+int(ceil(log10(abs($angle))))+5}]
   }
   # The fmod function leaves a result <|2pi|, not <|pi| as we want.
   # Adding in tests to reduce the result further takes longer time
   # than the code below.
   set m [mpexpr {round($angle/pi())}]
   set result [mpexpr {$angle - $m*pi()}]
   set mp_precision $save_precision
   return $result
}
proc ReduceModTwoPiOld { angle } { ;# Reduce import modulo 2*pi
   # The fmod function leaves a result <|2pi|, not <|pi| as we want.
   # Adding in tests to reduce the result further takes longer time
   # than the code below.
   set m [mpexpr {round(0.5*$angle/pi())}]
   set result [mpexpr {$angle - $m*2*pi()}]
   return $result
}
proc ReduceModTwoPi { angle } { ;# Reduce import modulo 2*pi
   global mp_precision
   set save_precision $mp_precision
   if {[mpexpr {abs($angle)>1}]} {
      set mp_precision \
          [mpexpr {$mp_precision+int(ceil(log10(abs($angle))))+5}]
   }
   # The fmod function leaves a result <|2pi|, not <|pi| as we want.
   # Adding in tests to reduce the result further takes longer time
   # than the code below.
   set m [mpexpr {round(0.5*$angle/pi())}]
   set result [mpexpr {$angle - $m*2*pi()}]
   set mp_precision $save_precision
   return $result
}

proc ExprReduce { angle } { ;# Reduce import modulo pi
   global expr_pi
   set m [expr {round($angle/$expr_pi)}]
   set result [expr {$angle - $m*$expr_pi}]
   return $result
}
proc ExprReduceModTwoPi { angle } { ;# Reduce import modulo 2*pi
   set m [expr {round(0.5*$angle/$expr_pi)}]
   set result [expr {$angle - $m*2*$expr_pi}]
   return $result
}

proc Sin { angle } {
   return [mpexpr {sin([ReduceModTwoPi $angle])}]
   ## It is faster to call ReduceModTwoPi directly
   ## than relying on reduction code inside sin.
}
proc Cos { angle } {
   return [mpexpr {cos([ReduceModTwoPi $angle])}]
}
proc Tan { angle } {
   return [mpexpr {tan([ReduceModTwoPi $angle])}]
}
proc ExprSin { angle } {
   return [expr {sin($angle)}]
}
proc ExprCos { angle } {
   return [expr {cos($angle)}]
}
proc ExprTan { angle } {
   return [expr {tan($angle)}]
}

proc Atan { x } {
   if {$x>1} { # Improve accuracy and speed for large |x|
      return [mpexpr {0.5*pi()-atan(1.0/$x)}]
   } elseif {$x<-1} {
      return [mpexpr {-0.5*pi()-atan(1.0/$x)}]
   }
   return [mpexpr {atan($x)}]
}
proc Arctan { x } { return [Atan $x] }

proc Asin { x } {
   return [mpexpr {asin($x)}]
}
proc Arcsin { x } { return [Asin $x] }

proc Acos { x } {
   return [mpexpr {acos($x)}]
}
proc Arccos { x } { return [Acos $x] }

proc ExprAtan { x } {
   return [expr {atan($x)}]
}
proc ExprArctan { x } { return [ExprAtan $x] }

proc ExprAsin { x } {
   return [expr {asin($x)}]
}
proc ExprArcsin { x } { return [ExprAsin $x] }

proc ExprAcos { x } {
   return [expr {acos($x)}]
}
proc ExprArccos { x } { return [ExprAcos $x] }

proc PrintFormatSample {} {
   # Prints examples for test Xp_DoubleDouble scientific output format

   global mp_precision float_precision

   set save_mp_precision $mp_precision
   set work_precision 500
   set mp_precision $work_precision
   
   set float_eps [mpexpr {pow(2,-1*$float_precision+1)}]
   
   set eps_12 [RoundToDouble [mpexpr {$float_eps/12.}]]

   set tests [subst {
      [mpexpr {pi()}]
      [mpexpr {pi()*pow(2,520)}]
      [mpexpr {pi()*pow(2,1022)}]
      [mpexpr {pi()*pow(2,-1024+55)}]
      [mpexpr {pi()*pow(2,-1024+55-3)}]
      [mpexpr {pi()*pow(2,-1024+55-6)}]
      [mpexpr {pi()*pow(2,-1024+55-59)}]
      -999999.75
      {1.0 [mpexpr {-0.25*$float_eps}]}
      {1.0 [mpexpr {-$eps_12}]}
      $eps_12
   }]

   set testno 0
   foreach testval $tests {
      set testval [RoundToDoubleDouble {*}$testval]
      set mp_precision 42
      if {$testno != 0} { puts {} }
      puts [format "--- TEST %2d, VAL: %20.12e ---" \
               $testno [mpformat %.17e $testval]]
      puts [format "+++ HEXVAL: %s" [WriteHexFloat $testval]]
      set mp_precision $work_precision
      for {set i 0} {$i<38} {incr i} {
         puts [format " check %2d: %s" $i [FormatMpNum 0 $i $testval]]
      }
      incr testno
   }
   set mp_precision $save_mp_precision
   return
}


########################################################################
### Support STOP
if { [info script] ne $::argv0 } {
   # Script is being sourced by another script
   return
}

########################################################################
### Top-level run

if {$format_sample} {
   PrintFormatSample
   exit
}


# Read header line
while {[gets stdin line]>=0} {
   if {[regexp {\# *Index *x approx *xt *(.*)$} $line dummy testname]} {
      set paramcount 1
      break
   }
   if {[regexp {\# *Index *x approx *y approx *xt  *yt  *(.*)$} \
           $line dummy testname]} {
      set paramcount 2
      break
   }

}
if {![info exists testname]} {
   puts stderr "Can't identify header line"
   if {![string match {} $line]} { puts stderr "Last line read: $line" }
   exit 2
}
set testname [string trim $testname]
set testname_width [string length $testname]
set testname_rightpad [expr {(25-$testname_width)/2}]
set testname_leftpad [expr {25-$testname_width-$testname_rightpad}]
set testname_str [format "%*s%s%*s" $testname_leftpad "" $testname \
                      $testname_rightpad ""] ;# Centered version
set truthname [regsub {^Sloppy} [regsub -- {_.*$} $testname {}] {}]

if {$paramcount == 1} {
   if {!$flag_quiet} {
      puts [format "# Index %20s %43s %44s %32s %12s" \
               xt $testname_str True_value Rel_Err ULP_Err]
   }
} elseif {$paramcount == 2} {
   if {!$flag_quiet} {
      puts [format "# Index %20s %25s %43s %44s %32s %12s" \
               xt yt $testname_str True_value Rel_Err ULP_Err]
   }
} else {
   error "Unsupported paramcount: $paramcount"
}

set start_click [GetClicks]
set maxerr -Inf
set minerr  Inf
set maxerrdata {}
set minerrdata {}
set save_precision $mp_precision
while {[gets stdin line]>=0} {
   set mp_precision $save_precision
   if {$paramcount == 1} {
      if {![regexp {^ *([0-9]+) +([^ ]+) +(\[[^\]]+\]) +(\[[^\]]+\])} \
               $line dummy \
               index xapp xt yt]} {
         puts stderr "Can't parse line:\n$line"
         exit 3
      }
      set inputdata [format "%-50s" $xt]
      set xt [ReadHexBin $xt float_precision work_precision_xt]
      if {$work_precision_xt>$mp_precision} {
         set mp_precision $work_precision_xt
      }
      if {[IsInf $xt] || [IsNan $xt]} {
         set rxt $xt
         append inputdata [format "= %13s" $rxt]
         set sxt [format %20s "$rxt     "]
      } else {
         set rxt [mpformat %.16e $xt]
         if {[mpexpr {1e-100<abs($rxt) && abs($rxt)<1e100}]} {
            append inputdata [format "= %13.5e" $rxt]
            set sxt [format %20g $rxt]
         } else {
            append inputdata [mpformat "= %13.5e" $rxt]
            set sxt [mpformat %20e $rxt]
         }
      }
      set inputdatasize 65
   } else {
      if {![regexp {^ *([0-9]+) +([^ ]+) +([^ ]+)\
                       +(\[[^\]]+\]) +(\[[^\]]+\]) +(\[[^\]]+\])} \
               $line dummy \
               index xapp bapp xt bt yt]} {
         puts stderr "Can't parse line:\n$line"
         exit 3
      }
      set inputdata [format "%-50s %-50s" $xt $bt]
      set xt [ReadHexBin $xt float_precision work_precision_xt]
      set bt [ReadHexBin $bt float_precision work_precision_bt]
      if {$work_precision_xt>$mp_precision} {
         set mp_precision $work_precision_xt
      }
      if {[IsInf $xt] || [IsNan $xt]} {
         set rxt $xt
         append inputdata [format "= %13s" $rxt]
         set sxt [format %20s "$rxt     "]
      } else {
         set rxt [mpformat %.16e $xt]
         if {[mpexpr {1e-100<abs($rxt) && abs($rxt)<1e100}]} {
            append inputdata [format "= %13.5e" $rxt]
            set sxt [format %20g $rxt]
         } else {
            append inputdata [mpformat "= %13.5e" $rxt]
            set sxt [mpformat %20e $rxt]
         }
      }
      if {$work_precision_bt>$mp_precision} {
         set mp_precision $work_precision_bt
      }
      if {[IsInf $bt] || [IsNan $bt]} {
         set rbt $bt
         append inputdata [format " , %13s" $rbt]
         set sbt [format %20s "$rbt     "]
      } else {
         set rbt [mpformat %.16e $bt]
         if {[mpexpr {1e-100<abs($rbt) && abs($rbt)<1e100}]} {
            append inputdata [format " , %13.5e" $rbt]
            set sbt [format %20g $rbt]
         } else {
            append inputdata [mpformat " , %13.5e" $rbt]
            set sbt [mpformat %20e $rbt]
         }
      }
      set inputdatasize 132
   }
   if {[catch {ReadHexBin $yt float_precision work_precision_yt} val]} {
      puts stderr "Can't parse line:\n$line"
      puts stderr "Input to ReadHexBin: \"$yt\""
      puts stderr "ERRMSG: $val"
      exit 3
   }
   if {$work_precision_yt>$mp_precision} {
      set mp_precision $work_precision_yt
   }
   # Tweak yt at extremes to agree with mpexpr limits set above
   if {[IsNan $val]} {
      set yt NaN
   } elseif {[IsInf $val]} {
      if {[string match {-*} $val]} {
         set yt -Inf
      } else {
         set yt Inf
      }
   } elseif {[mpexpr {$val>$MAXVAL}]} {
      set yt Inf
   } elseif {[mpexpr {$val < -1*$MAXVAL}]} {
      set yt -Inf
   } elseif {[mpexpr {abs($val) < $MINVAL}]} {
      set yt 0.0
   } else {
      set yt $val
   }
   set relerror "-"
   set ulperror "-"
   if {[IsInf $yt] || [IsNan $yt]} {
      set ryt $yt
      set syt "$yt        "
      set hyt $yt
   } else {
      set ryt [mpformat %.16e $yt]
      set syt $ryt
      set hyt [WriteHexBin $float_precision $yt]
   }
   if {$paramcount == 1} {
      if {[IsNan $xt]} {
         set result NaN
      } elseif {[catch {$truthname $xt} result]} {
         puts "Error on input line $line"
         puts "Inval xt = $xt"
         error $result
      }
   } else {
      if {[IsNan $xt] || [IsNan $bt]} {
         set result NaN
      } elseif {[catch {$truthname $xt $bt} result]} {
         puts "Error on input line $line"
         puts "Inval xt = $xt"
         puts "Inval bt = $bt"
         error $result
      }
   }
   if {[regexp {(-|\+|)Inf} $result dummy sign]} {
      if {[string match "-" $sign]} {
         set truth -Inf
      } else {
         set truth Inf
      }
      set rtruth $truth
      set htruth $truth
   } elseif {[regexp {NaN} $result]} {
      set rtruth [set truth NaN]
      set htruth $truth
   } else {
      if {[mpexpr {$result>$MAXVAL}]} {
         set [htruth set rtruth [set truth Inf]]
      } elseif {[mpexpr {$result<-$MAXVAL}]} {
         set [htruth set rtruth [set truth -Inf]]
      } else {
         if {[mpexpr {abs($result)<$MINVAL}]} {
            set result 0.0
         }
         set rtruth [mpformat %.16e [set truth $result]]
         set rtruth [regsub {1.*INF} $rtruth Inf]
         set htruth [WriteHexBin $float_precision $truth]
      }
   }
   set ulp 0.0
   if {![IsNan $rtruth] && ![IsInf $rtruth] && [mpexpr {$truth != 0.}]} {
      set relerror [mpformat %.16e [mpexpr {($yt-$truth)/$truth}]]
      if {[mpexpr {$relerror<pow(10,-$save_precision)}]} {
         set relerror 0.0
      }
      set relerror [format %12.3g $relerror]
      set ulp [EstimateULP [expr {1+2*$float_precision}] $truth]
   }
   if {![IsNan $rtruth] && ![IsInf $rtruth] && ![IsInf $ryt] \
          && [mpexpr {$ulp != 0.0}]} {
      if {[catch {
         set ulperror [mpformat %.16e [mpexpr {($yt-$truth)/$ulp}]]
         set ulperror [format %12.6f $ulperror]
         if {$ulperror>$maxerr} {
            set maxerr $ulperror
            set maxerrdata [format "%s    %s = %s" $inputdata $ryt $hyt]
         }
         if {$ulperror<$minerr} {
            set minerr $ulperror
            set minerrdata [format "%s    %s = %s" $inputdata $ryt $hyt]
         }
      } errmsg]} {
         puts stderr $errmsg
         puts stderr "   yt := \"$yt\""
         puts stderr "truth := \"$truth\""
         puts stderr "  ulp := \"$ulp\""
         exit 1
      }
   } else {
      if {[string compare $yt $truth] == 0} {
         if {$maxerr<0.0} {
            set minerr [set maxerr [format %12.6f 0.0]]
            set minerrdata [set maxerrdata \
                               [format "%s    %s = %s" $inputdata $ryt $hyt]]
         }
      } else {
         if {[IsInf $rtruth] || [IsInf $ryt] || [mpexpr {$yt != $truth}]} {
            puts stderr "Irregular check: $line :\
                         [WriteHexBin $float_precision $truth]"
         }
      }
   }
   if {!$flag_quiet} {
      set struth $rtruth
      if {[string length $struth]<5} {
         set struth "$struth        "
      }
      foreach name [list relerror ulperror] {
         set s$name [set $name]
         if {[string length [set $name]]<5} {
         set s$name "[set $name]   "
         }
      }
      if {$paramcount == 1} {
         puts [format "%7d %25s %51s %51s %12s %12s" \
                  $index $sxt $hyt $htruth $srelerror $sulperror]
      } else {
         puts [format "%7d %25s %25s %51s %51s %12s %12s" \
                  $index $sxt $sbt $hyt $htruth $srelerror $sulperror]
      }
   }
}
set mp_precision $save_precision

if {!$flag_quiet} { puts {} }
puts [format "%-14s%-8s %-*s  %-25s" \
         $testname "ULP err" $inputdatasize \
         "Input value" " Computed value"]
puts "MAXERR: $maxerr : $maxerrdata"
puts "MINERR: $minerr : $minerrdata"
puts "Precision: [expr {2*$float_precision+1}] bits"
set stop_click [GetClicks]
puts [format "Run  time: %.2f seconds" [expr {($stop_click-$start_click)*1e-6}]]
