# MIF Example File: sample-attributes.tcl
# Description: This is not a complete MIF file, but rather
#  a piece holding data shared by two other example files,
#  sample-rotate.mif and sample-reflect.mif.  This file
#  contains an Oxs_LabelValue object that holds common
#  values for the Oxs_TimeDriver blocks in the other
#  two files.

Specify Oxs_LabelValue:common_driver_data {
 evolver :evolver
 comment {1 deg/ns = 17453293 rad/sec; If Ms=8.6e5, and lambda is small,
         then mxh=1e-6 translates into dm/dt = 2e5 rad/sec = 0.01 deg/ns}
 mesh :mesh
 stopping_dm_dt {1 2.3e5 {3 49} :expand:}
 stopping_time {1.1e-9 1.2e-9 {2.1e-9 2.2e-9 3}  {3.1e-9 3.2e-9 1}
                {3.2e-9 41} :expand:}
 stage_iteration_limit {{10 20 30 2} 40 {5 44} :expand:}
 comment {Ms :rotate}
 m0 {0 1 0}
}

# Include as many Oxs_LabelValue blocks as you want, just be sure
# to give each a unique instance name.  (The instance name of the
# example above is :common_driver_data.)  Pull these data into a
# MIF file with a command like
#
#   eval [ReadFile sample-attributes.tcl]
#
# You can then bring the label/value pairs into Specify blocks
# by referencing the instance name with an attribute label, e.g.,
#
#   Specify Oxs_TimeDriver {
#    basename sample-rotate
#    Ms :flatten
#    attributes :common_driver_data
#   }
#
# You can also include whole Specify blocks in this file, if you
# want them with the exact same contents in multiple MIF files.
# They will be read-in and registered with all the other Specify
# blocks with the ReadFile command is evaluated.
#
