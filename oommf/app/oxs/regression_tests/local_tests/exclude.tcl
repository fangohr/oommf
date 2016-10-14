lappend excludelist {2dpbc_film {} "Bad numerics?"}
lappend excludelist {spinxfer-miltat {} "Bad numerics?"}
lappend excludelist {stdprob1-weak {} "Bad numerics?"}
lappend excludelist {semianalyticevolve-test {} "Bad numerics? (Mac, cpuarch=generic)"}
lappend excludelist {modllg_onespin {} "Bad numerics? (Windows + gcc or bcc)"}
lappend excludelist {spinxfer {} "Bad numerics? (Windows + gcc or bcc)"}
lappend excludelist {lagtest {} "Bad numerics?"}
lappend excludelist [list xcgtest {} "In development"]
lappend excludelist [list kl_infinite_prism {} "Bad numerics?"]
lappend excludelist [list xf_thermspinxferevolve_example1 {} "Bad numerics?"]
lappend excludelist [list xf_thermspinxferevolve_example2 {} "Bad numerics?"]
lappend excludelist [list xf_thermheunevolve_example1 {} "Bad numerics?"]
lappend excludelist [list xf_thermheunevolve_example2 {} "Bad numerics?"]

# If unix, is X11 server accessible?
global no_display
if {$no_display} {
   lappend excludelist {kl_infinite_prism {} "No display"}
   lappend excludelist {dw-160-8-4-3D {} "No display"}
}
