/* FILE: mmdispcmds.h                 -*-Mode: c++-*-
 *
 * Tcl commands implemented in C/C++ and supported by shell mmdispsh
 *
 */

#ifndef _MMDISPCMDS_H
#define _MMDISPCMDS_H

#include "oc.h"

/* End includes */     /* Optional directive to build.tcl */

/*
 * NOTE: When version number information is changed here, it must
 * also be changed in the following files:
 *      ./mmdispcmds.tcl
 */

#define MMDISPCMDS_MAJOR_VERSION        2
#define MMDISPCMDS_MINOR_VERSION        0
#define MMDISPCMDS_RELEASE_LEVEL        "b"
#define MMDISPCMDS_RELEASE_SERIAL       0

#define MMDISPCMDS_VERSION OC_MAKE_VERSION(MMDISPCMDS)

/* Functions to be passed to the Tcl/Tk libraries */
Tcl_CmdDeleteProc BitmapDeleteProc;
Tcl_CmdProc BitmapCmd;
Tcl_CmdProc ChangeMesh;
Tcl_CmdProc CopyMesh;
Tcl_CmdProc DifferenceMesh;
Tcl_CmdProc DrawFrame;
Tcl_CmdProc FindMeshVector;
Tcl_CmdProc FreeMesh;
Tcl_CmdProc Gcd;
Tcl_CmdProc GetAutosamplingRates;
Tcl_CmdProc GetDataValueUnit;
Tcl_CmdProc GetDataValueScaling;
Tcl_CmdProc GetDefaultColorMapList;
Tcl_CmdProc GetFirstFileName;
Tcl_CmdProc GetFrameBox;
Tcl_CmdProc GetFrameRotation;
Tcl_CmdProc GetZoom;
Tcl_CmdProc GetMeshCellSize;
Tcl_CmdProc GetMeshCoordinates;
Tcl_CmdProc GetDisplayCoordinates;
Tcl_CmdProc GetMeshDescription;
Tcl_CmdProc GetMeshIncrement;
Tcl_CmdProc GetMeshName;
Tcl_CmdProc GetMeshRange;
Tcl_CmdProc GetMeshSpatialUnitString;
Tcl_CmdProc GetMeshStructureInfo;
Tcl_CmdProc GetMeshTitle;
Tcl_CmdProc GetMeshType;
Tcl_CmdProc GetMeshValueMagSpan;
Tcl_CmdProc GetMeshValueMean;
Tcl_CmdProc GetMeshValueRMS;
Tcl_CmdProc GetMeshValueUnit;
Tcl_CmdProc GetMeshZRange;
Tcl_CmdProc GetVecColor;
Tcl_CmdProc GetZsliceCount;
Tcl_CmdProc GetZsliceLevels;
Tcl_CmdProc IsRectangularMesh;
Tcl_CmdProc PeriodicTranslate;
Tcl_CmdProc Resample;
Tcl_CmdProc PSWriteMesh;
Tcl_CmdProc ReportActiveMesh;
Tcl_CmdProc SelectActiveMesh;
Tcl_CmdProc SetDataValueScaling;
Tcl_CmdProc SetFrameRotation;
Tcl_CmdProc SetZoom;
Tcl_CmdProc SetMeshTitle;
Tcl_CmdProc UpdatePlotConfiguration;
Tcl_CmdProc WriteMesh;
Tcl_CmdProc WriteMeshUsingDeprecatedVIOFormat;
Tcl_CmdProc WriteMeshOVF2;
Tcl_CmdProc WriteMeshNPY;
Tcl_CmdProc WriteMeshMagnitudes;
Tcl_CmdProc WriteMeshAverages;

Tcl_PackageInitProc Mmdispcmds_Init;

#endif // _MMDISPCMDS_H

