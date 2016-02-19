This is short user documentation for MF_CurrentFlowEvolver class.

version 1.1.1

Class introduces evolver that allows calculating magnetoresistance and 
effects driven by current flow through magnetic tunnel junction
(spin transfer torqe and Oersted field)
 
author: Marek Frankowski
contact: mfrankow[at]agh.edu.pl
website: layer.uci.agh.edu.pl/M.Frankowski

This code is public domain work based on other public domains contributions.

Inputs:

gamma_LL - Landau-Lifshitz gyromagnetic ratio

method - numerical method for solving LLGS equation (see code for posible methods)

alpha - Gilbert damping

bJ0, bJ1, bJ2 - perpendicular torque coefficients, component in LLGS gamma_LL*bJ0+bJ1*j+bJ2*j^2*mxp,
where j is current desity, m and p are normalized magnetizations of free and reference layer, respectively

RA_p, RA_ap - R*A (resistance times area) of parallel and anti-parallel states, repectively

current_mode - if 0.0 simualtion takes into account current-resistance feedback,
if different then 0.0 current depends only on voltage and RA_p, now magnetization configuration dependency

oersted_mode - if 0.0 no Oersted field calculated, if 1.0 Oersted field calcuated from channels that sticks out
from junction in both directions for pillar_height value

vrange - similar to Oxs_UZeeman {start_value, end_value, number_of_stages_beetwen_them}

Inter_down and Inter_up - specify in the same as to Oxs_TwoSurfaceExchange to select interfaces of STT and TMR

x,y,z - numbers of cells (from 0 to n-1) to select area for probing Oersted field value
dx,dy,dz - numbers addition of cells in each direction to enlarge area for probing Oersted field value
for example:
x 3
y 4
z 2
dx 2
dy 2
dz 2
is cube 3x3x3 with corner located in cell 3,4,2, counting from 0. Defalut value of x,y,z,dx,dy,dz is 0.

Example sepcify block:

Specify MF_CurrentFlowEvolver [subst {
  gamma_LL 2.21e5
  method rkf54s
  alpha 0.017
  bJ0	0.0
  bJ1	0.28e-8
  bJ2 -0.28e-18
  eta0  0.7

  RA_p 0.81e-11
  RA_ap 1.25e-11

  current_mode 0.0
  oersted_mode 1.0
  pillar_height 50e-9
 
  vrange {
  { 0.0 -1.0 3 }
  { -1.0 1.0 6 }
 }
 
 Inter_down {
  atlas :atlas
  region refer
  scalarfield :zheight
  scalarvalue $zref
  scalarside -
 }
 Inter_up {
  atlas :atlas
  region top
  scalarfield :zheight
  scalarvalue $ztop
  scalarside +
 }  
} ] 	

Outputs:

Spin torque - values of LLGS in-plane torqe component in each cell

Field Like torque - values of LLGS perpendicular component in each cell

channel conductance - conductance of each channel, represented as z component in output vector field

current density - conductance of each channel, represented as z component in output vector field

total resistance - resistance of the junction

voltage - voltage value 

Oersted field, Oersted field x, Oersted field y, Oersted field z - Oersted field value and its 
components in each direction in area selected by specify block (or in cell 0,0,0 if no area selected)
