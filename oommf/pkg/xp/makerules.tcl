# FILE: makerules.tcl
#
# This file controls the application 'pimake' by defining rules which
# describe how to build the applications and/or extensions produced by
# the source code in this directory.
#
# NOTICE: Please see the file ../../LICENSE
#
# Last modified on: $Date: 2000/08/25 17:01:25 $
# Last modified by: $Author: donahue $
#
# Verify that this script is being sourced by pimake
if {[llength [info commands MakeRule]] == 0} {
    error "'[info script]' must be evaluated by pimake"
}
#
########################################################################

# Note: There is a "chicken and egg" problem with xpport.h, analogous to
# that with ocport.h in ../oc/makerules.tcl (q.v.).  In particular,
# files outside of the xp package that need the package place a
# dependency on xp.h.  When a CSourceFile Dependencies call looks into
# xp.h, it finds a dependency to xpport.h.  If xpport.h doesn't exist
# then this makerules.tcl is sourced to find a rule for xpport.h.  If
# there are any Dependencies calls in this file that lead to xp.h then
# Dependencies considers that to be a circular dependency and bails.
#
# A two part workaround is:
#
#    1) The public header xp.h explicitly #include's each constructed
#       file.  These should precede any other #include's.  If there are
#       multiple constructed files, say a.h and b.h, and if b.h
#       #include's a.h, then #include "a.h" should precede #include
#       "b.h" in this file.
#
#    2) Files inside the xp package don't #include the public header
#       xp.h, but instead #include a private internal header, xpint.h,
#       as needed.  The public header xp.h also includes xpint.h.
#
# When an external dependency check looks inside xp.h, part 1 will
# insure that if xpport.h doesn't exist then the subsequent sourcing of
# xp/makerules.tcl will be triggered while processing xp.h.  Any
# CSourceFile Dependencies occuring inside the xp/makerules.tcl file
# will then be grafted onto the dependency graph at xp.h.  (The
# grafting protects against infinite recursion arising from potential
# circular dependencies.)  Since by part 2 none of the files in the xp
# package have a dependency on xp.h, no false circular dependencies are
# encountered.

# Check user build requests
set xpconfig [Oc_Config RunPlatform]
set XP_USE_ALT 0
set XP_FLOATTYPE auto
set XP_RANGECHECK -1
if {![catch \
      {$xpconfig GetValue program_compiler_xp_doubledouble_basetype} \
      check]} {
   set XP_FLOATTYPE $check
}
if {![catch \
      {$xpconfig GetValue program_compiler_xp_doubledouble_altsingle} \
      check]} {
   set XP_USE_ALT 1
   set XP_FLOATTYPE $check
}
if {![catch \
      {$xpconfig GetValue program_compiler_xp_doubledouble_rangecheck} \
      check]} {
   set XP_RANGECHECK $check
}
if {[string compare "longdouble" $XP_FLOATTYPE]==0} {
   # Allow user to drop space in "long double" request
   set XP_FLOATTYPE "long double"
}
if {[string compare -nocase "MPFR" [lindex $XP_FLOATTYPE 0]]==0} {
   # Allow user to drop case in MPFR request
   set XP_FLOATTYPE [lreplace $XP_FLOATTYPE 0 0 MPFR]
}
if {![catch \
      {$xpconfig GetValue program_compiler_xp_use_fma} \
      check]} {
   set XP_USE_FMA $check
}

if {$XP_USE_ALT} {
   set objects {
      alt_doubledouble
   }
} else {
   set objects {
      doubledouble
   }
}
lappend objects xp xpcommon

set alldeps [concat configure \
                [Platform StaticLibrary xp] \
                [Platform Executables ddtest]]
if {[catch {$xpconfig \
                GetValue program_pimake_xp_doubledouble_disable_test} _] \
       || !$_} {
   lappend alldeps test
} else {
   puts stderr "*** WARNING: Automatic build testing\
                of Xp_DoubleDouble class disabled. ***"
}

MakeRule Define {
    -targets        all
    -dependencies   $alldeps
}

MakeRule Define {
   -targets         configure
   -dependencies    [Platform Specific xpport.h]
}

# Does linker require -lm?
set mathlib {}
if {![catch {$xpconfig GetValue TCL_LIBS} _]} {
    foreach _ [split $_] {
       if {[string match -lm $_]} {
          set mathlib {-lm}
          break
       }
    }
}

# build_port build
set XP_MANTDIGS {}
if {[string compare -nocase [lindex $XP_FLOATTYPE 0] mpfr]==0} {
   set XP_USE_MPFR "XP_USE_MPFR=1"
   $xpconfig SetValue EXTRA_LIBS_xp mpfr
   set extralibs [list -lib mpfr]
   if {[llength $XP_FLOATTYPE]>1} {
      set XP_MANTDIGS "XP_MPFR_MANTDIG=[lindex $XP_FLOATTYPE 1]"
      set XP_FLOATTYPE MPFR
   }
} else {
   set XP_USE_MPFR "XP_USE_MPFR=0"
   set extralibs {}
}
if {[info exists XP_USE_FMA] && ![string match {} $XP_USE_FMA]} {
   set XP_USE_FMA "XP_USE_FMA=$XP_USE_FMA"
} else {
   set XP_USE_FMA {}
}

set compilecmd [subst {Platform CompileCmd C++ \
                   -valuesafeopt 1 \
                   -inc [[CSourceFile New _ build_port.cc] DepPath] \
                   -def {$XP_USE_MPFR $XP_MANTDIGS $XP_USE_FMA} \
                   -out build_port -src build_port.cc}]
MakeRule Define {
    -targets        [Platform Objects build_port]
    -dependencies   [concat [list [Platform Name]] \
                       [[CSourceFile New _ build_port.cc] Dependencies]]
    -script         [subst -nocommands {Platform Compile C++ \
                       -valuesafeopt 1 \
                       -inc [[CSourceFile New _ build_port.cc] DepPath] \
                       -def {$XP_USE_MPFR $XP_MANTDIGS $XP_USE_FMA} \
                       -out build_port -src build_port.cc}]
}

MakeRule Define {
   -targets         [Platform Executables build_port]
   -dependencies    [Platform Objects build_port]
   -script          [subst -nocommands {Platform Link \
                      -obj build_port -sub CONSOLE \
                      $mathlib $extralibs -out build_port}]
}

# xpport.h construction
proc Xp_BuildExec {build outfile args} {
   # Use Xp_DoubleDouble
   if {[catch {eval exec $build $args 2>@ stderr} outdump]} {
      # error running program
      set msg "Error running $build $args:\n$outdump"
      error $msg $msg
   }
   set fileid [open $outfile w]
   puts $fileid $outdump
   close $fileid
}
set XP_FLOATTYPE [list $XP_FLOATTYPE] ;# Protect spaces
MakeRule Define {
   -targets         [Platform Specific xpport.h]
   -dependencies    [Platform Executables build_port]
   -script          [subst -nocommands {Xp_BuildExec \
                       [Platform Executables build_port] \
                       [Platform Specific xpport.h] \
                       $XP_USE_ALT $XP_RANGECHECK $XP_FLOATTYPE $compilecmd}]
}

# Build xp library
foreach o [concat $objects] {
   MakeRule Define {
    -targets        [Platform Objects $o]
    -dependencies   [concat [list [Platform Name]] \
                        [[CSourceFile New _ $o.cc] Dependencies]]
    -script         [format {Platform Compile C++ -valuesafeopt 1 \
                            -inc [[CSourceFile New _ %s.cc] DepPath] \
                            -out %s -src %s.cc
                    } $o $o $o]
   }
   # Q: Do all files in this package need "-valuesageopt 1"?  Does it matter?
}

MakeRule Define {
    -targets        [Platform StaticLibrary xp]
    -dependencies   [concat tclIndex [Platform Objects $objects]]
    -script         [format {
                        eval DeleteFiles [Platform StaticLibrary xp]
                        Platform MakeLibrary -out xp -obj {%s}
                        Platform IndexLibrary xp
                    } $objects]
}

# Test program build and run
MakeRule Define {
   -targets         [Platform Objects ddtest]
   -dependencies    [concat [list [Platform Name]] \
                        [[CSourceFile New _ ddtest.cc] Dependencies]]
   -script          {Platform Compile C++ -opt 1 \
                              -inc [[CSourceFile New _ ddtest.cc] DepPath] \
                        -out ddtest -src ddtest.cc}
}

MakeRule Define {
 -targets           [Platform Executables ddtest]
 -dependencies      [concat [Platform Objects ddtest] \
                        [Platform StaticLibraries {xp oc}]]
 -script            [subst -nocommands {Platform Link \
                        -obj ddtest -lib {xp oc tk tcl} -sub CONSOLE \
                        $mathlib -out ddtest}]
}

proc Xp_RunTest { testpgm args } {
   puts "---------------------------------------"
   puts "---  Testing Xp_DoubleDouble build  ---\n$testpgm $args"
   # Exec first with the --version option to check that the target can
   # be loaded and run. In particular, this will detect problems with
   # shared libraries.
   if {[catch {eval exec $testpgm --version 2>@1} outdump]} {
      set msg "Unable to execute $testpgm:\n$outdump"
      error $msg $msg
   }
   if {[catch {eval exec $testpgm $args 2>@1} outdump]} {
      # error running program
      set msg "Error running $testpgm $args:\n$outdump"
      append msg "\n\
***********************   Xp_DoubleDouble test failed.   **********************\n\
* Try one of the following:                                                   *\n\
* (1) Adjust \"value safe\" and FMA optimizations via platform config options   *\n\
*       program_compiler_c++_remove_valuesafeflags                            *\n\
*       program_compiler_c++_add_valuesafeflags                               *\n\
*       program_compiler_xp_use_fma                                           *\n\
* (2) Make long double the base type:                                         *\n\
*  \$config SetValue program_compiler_xp_doubledouble_basetype {long double}   *\n\
* (3) Use the BOOST/MPFR multi-precision alternative via                      *\n\
*  \$config SetValue program_compiler_xp_doubledouble_altsingle MPFR           *\n\
*    Note: This may require install of additional software.                   *\n\
* (4) Use standard \"long double\" variables instead of Xp_DoubleDouble:        *\n\
*  \$config SetValue program_compiler_xp_doubledouble_altsingle {long double}  *\n\
* (5) Disable test:                                                           *\n\
*  \$config SetValue program_pimake_xp_doubledouble_disable_test 1             *\n\
* Refer to oommf/pkg/xp/README for additional details.                        *\n\
*******************************************************************************\n"
      error $msg $msg
   }
   puts $outdump
   puts "--- Xp_DoubleDouble build looks OK. ---"
   puts "---------------------------------------"

}

MakeRule Define {
   -targets         test
   -dependencies    [Platform Executables ddtest]
   -script          [subst -nocommands {Xp_RunTest \
                    [Platform Executables ddtest] basetest}]
}

MakeRule Define {
    -targets        [Platform Name]
    -dependencies   {}
    -script         {MakeDirectory [Platform Name]}
}

MakeRule Define {
    -targets        tclIndex
    -dependencies   [concat [file join .. oc tclIndex]]
    -script         { MakeTclIndex . xp.tcl }
}

MakeRule Define {
    -targets        distclean
    -dependencies   clean
    -script         {DeleteFiles [Platform Specific xpport.h]
                     DeleteFiles [Platform Executables {build_port ddtest}]
                     DeleteFiles [Platform Name]}
}

MakeRule Define {
    -targets        clean
    -dependencies   mostlyclean
}

MakeRule Define {
    -targets        mostlyclean
    -dependencies   objclean
    -script         {eval DeleteFiles [Platform StaticLibrary xp]
                     DeleteFiles tclIndex
                    }
}

MakeRule Define {
    -targets        objclean
    -dependencies   {}
    -script         [format {
              eval DeleteFiles [Platform Objects {build_port ddtest %s}]
              eval DeleteFiles [Platform Intermediate build_port]
                   } $objects]
}

foreach _ { xpconfig xpconfig XP_USE_ALT XP_RANGECHECK XP_FLOATTYPE \
            objects shared_scripts alldeps mathlib \
            XP_MANTDIGS XP_USE_MPFR XP_USE_FMA \
            extra_libs compilecmd } {
   unset -nocomplain -- $_
}
