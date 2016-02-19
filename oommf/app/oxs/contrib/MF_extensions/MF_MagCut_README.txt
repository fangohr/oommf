This is short user documentation for MF_CurrentFlowEvolver class.

version 1.1.1

Class allows cutting magnetisation value in x direction of selected area

author: Marek Frankowski
contact: mfrankow[at]agh.edu.pl
website: layer.uci.agh.edu.pl/M.Frankowski

This code is public domain work based on other public domains contributions.

Inputs:

x,y,z - numbers of cells (from 0 to n-1) to select area for probing magnetizatopm value
dx,dy,dz - numbers addition of cells in each direction to enlarge area for probing magnetization value
for example:
x 3
y 4
z 2
dx 2
dy 2
dz 2
is cube 3x3x3 with corner located in cell 3,4,2, counting from 0.

Example sepcify block:

Specify MF_X_MagCut:example {

x 0
y 0
z 0
dx 124
dy 74
dz 7

}

Outputs:

area mx - magnetization components in area selected by specify block in direction depending of extention used (in this case x)
