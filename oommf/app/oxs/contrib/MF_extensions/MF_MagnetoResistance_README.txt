This is short user documentation for MF_CurrentFlowEvolver class.

version 1.1.1

Class allows calculations of resistance of selected areas of the junction
 
author: Marek Frankowski
contact: mfrankow[at]agh.edu.pl
website: layer.uci.agh.edu.pl/M.Frankowski

This code is public domain work based on other public domains contributions.

Inputs:

RA_p, RA_ap - R*A (resistance times area) of parallel and anti-parallel states, repectively

Surface1 and Surface2 - specify in the same as to Oxs_TwoSurfaceExchange to select interfaces of TMR

x,y - numbers of cells (from 0 to n-1) to select area for probing Resistance value
dx,dy - numbers addition of cells in each direction to enlarge area for probing Resistance value
for example:
x 3
y 4
dx 2
dy 2
is square 3x3 with corner located in cell 3,4 counting from 0.

x1,y1,dx1,dy1,x2,y2,dx2,dy2,x3,y3,dx3,dy3

Example sepcify block:

Specify MF_MagnetoResistance:TMRmiddlebotright [subst {

  RA_p 6.48e-12
  RA_ap 9.72e-12

 x 40
 y 45
 dx 3
 dy 3

 x1 80
 y1 25
 dx1 3
 dy1 3
 
 x2 40
 y2 25
 dx2 3
 dy2 3
 
 x3 10
 y3 15
 dx3 2
 dy3 2
 
 surface1 {
  atlas :atlas
  region refer
  scalarfield :zheight
  scalarvalue $zref
  scalarside -
 }
 surface2 {
  atlas :atlas
  region top
  scalarfield :zheight
  scalarvalue $ztop
  scalarside +
 }  
} ]

Outputs:

channel conductance - conductance of each channel, represented as z component in output vector field

total resistance - resistance of the junction

Area0 resistance, Area1 resistance, Area2 resistance, Area3 resistance - resistance of selected areas
