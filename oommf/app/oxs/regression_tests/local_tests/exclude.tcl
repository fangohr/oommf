lappend excludelist {2dpbc-Oct-2013/2dpbc_film {} "Bad numerics?"}
lappend excludelist {2dpbc-Oct-2013/2dpbc_film_alt {} "Bad numerics?"}
lappend excludelist {spinxfer-miltat {} "Bad numerics?"}
lappend excludelist {stdprob1-weak {} "Bad numerics?"}
lappend excludelist {semianalyticevolve-test {}
   "Bad numerics? (Mac, cpuarch=generic)"}
lappend excludelist {modllg_onespin {} "Bad numerics? (Windows + gcc or bcc)"}
lappend excludelist {spinxfer {} "Bad numerics? (Windows + gcc or bcc)"}
lappend excludelist {lagtest {} "Bad numerics?"}
lappend excludelist {xcgtest {} "In development"}
lappend excludelist {oommf-pbc_2.1/kl_infinite_prism {} "Bad numerics?"}
lappend excludelist {xf_thermspinxferevolve/xf_thermspinxferevolve_example1 \
                        {} "Bad numerics?"}
lappend excludelist {xf_thermspinxferevolve/xf_thermspinxferevolve_example2 \
                        {} "Bad numerics?"}
lappend excludelist {xf_thermheunevolve/xf_thermheunevolve_example1 \
                        {} "Bad numerics?"}
lappend excludelist {xf_thermheunevolve/xf_thermheunevolve_example2 \
                        {} "Bad numerics?"}
lappend excludelist {sttevolve/Example_MTJ {} "Segfaults"}
lappend excludelist {xf_stt/xf_stt_example1 {} "Segfaults"}

# If unix, is X11 server accessible?
global no_display
if {$no_display} {
   lappend excludelist {oommf-pbc_2.1/kl_infinite_prism {} "No display"}
   lappend excludelist {anv_spintevolve/dw-160-8-4-3D {} "No display"}
}
