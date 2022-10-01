/* FILE: mmdispcmds.cc                 -*-Mode: c++-*-
 *
 * Tcl commands implemented in C/C++ and supported by shell mmdispsh
 *
 */

#include "mmdispcmds.h"

#include "oc.h"
#include "nb.h"
#include "vf.h"

#include <cassert>
#include <climits>
#include <cstring>     /* strncpy(), ... */

#include "display.h"    /* DisplayFrame, CoordinateSystem */
#include "bitmap.h"

OC_USE_STD_NAMESPACE;  // Specify std namespace, if supported.
/// For some compilers this is needed to get "long double"
/// versions of the basic math library functions.

/* End includes */

#ifndef USE_TCL_COMMAND_CLEANUP_INFO
# if (TCL_MAJOR_VERSION > 7)
#  define USE_TCL_COMMAND_CLEANUP_INFO 1
# else
#  define USE_TCL_COMMAND_CLEANUP_INFO 0
# endif
#endif

#define MY_MESH_ARRAY_SIZE 3
static Vf_Mesh* myMeshArray[MY_MESH_ARRAY_SIZE];
static int activeMeshId = 0;
static DisplayFrame myFrame;

#define MY_BUF_SIZE 32000
static OC_CHAR buf[MY_BUF_SIZE];
#if MY_BUF_SIZE < 1
# error "buf size must be greater than 0"
#endif

namespace { // unnamed namespace, used for file-only access
  OC_REAL4m CoordsToAngle(CoordinateSystem coords)
  {
    OC_REAL4m rotang=0.;
    switch(coords) {
    case CS_CALCULATION_STANDARD: // Safety
    case CS_DISPLAY_STANDARD:  rotang=0.;   break;
    case CS_DISPLAY_ROT_90:    rotang=90.;  break;
    case CS_DISPLAY_ROT_180:   rotang=180.; break;
    case CS_DISPLAY_ROT_270:   rotang=270.; break;
    case CS_ILLEGAL:
    default:
      PlainError(1,"Unrecognized or illegal coordinate system (%d)"
                 " detected in CoordsToAngle() (File tkcmds.cc)\n",
                 int(coords));
      break; // Shouldn't get to here
    }
    return rotang;
  }

  CoordinateSystem AngleToCoords(OC_REAL4m angle_in_degrees)
  { // Rounds to nearest supported coordinate system rotation
    CoordinateSystem coords=CS_ILLEGAL;
    int quad=int(OC_ROUND(fmod(double(angle_in_degrees),
                               double(360.))/90.));
    if(quad<0) quad+=4;
    if(quad>3) quad-=4;
    switch(quad) {
    case 0:  coords=CS_DISPLAY_STANDARD;  break;
    case 1:  coords=CS_DISPLAY_ROT_90;    break;
    case 2:  coords=CS_DISPLAY_ROT_180;   break;
    case 3:  coords=CS_DISPLAY_ROT_270;   break;
    default:
      PlainError(1,"Programming error: Illegal coordinate system (quad=%d)"
                 " occurred in AngleToCoords() (File tkcmds.cc)\n",quad);
      break; // Shouldn't get to here
    }
    return coords;
  }

  OC_REAL4m SetZoom(OC_REAL4m zoom)
  {
    return myFrame.SetZoom(zoom);
  }

  OC_REAL4m SetZoom(Tcl_Interp *interp,OC_REAL4m width,OC_REAL4m height)
  {
    // Get margin and scroll information
    OC_REAL4m margin=0.0,scrollbar_cross_dimension=0.0;
    const char *cptr;
    cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                     OC_CONST84_CHAR("misc,margin"),TCL_GLOBAL_ONLY);
    if(!Nb_StrIsSpace(cptr)) {
      margin=static_cast<OC_REAL4m>(Nb_Atof(cptr));
    }
    cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                     OC_CONST84_CHAR("misc,scrollcrossdim"),TCL_GLOBAL_ONLY);
    if(!Nb_StrIsSpace(cptr)) {
      scrollbar_cross_dimension=static_cast<OC_REAL4m>(Nb_Atof(cptr));
    }
    return myFrame.SetZoom(width,height,
                           margin,scrollbar_cross_dimension);
  }
} // unnamed namespace

int ReportActiveMesh(ClientData,Tcl_Interp *interp,
                     int argc,CONST84 char** argv)
{
  Tcl_ResetResult(interp);
  if(argc!=1) {
    Oc_Snprintf(buf,sizeof(buf),
                "wrong # args: should be \"%s\"", argv[0]);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  Oc_Snprintf(buf,sizeof(buf),"%d",activeMeshId);
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}


int SelectActiveMesh(ClientData,Tcl_Interp *interp,
                     int argc,CONST84 char** argv)
{
  Tcl_ResetResult(interp);
  if(argc!=2) {
    Oc_Snprintf(buf,sizeof(buf),
                "SelectActiveMesh must be called with 1 argument:"
                " Mesh Id to make active (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  int meshId = atoi(argv[1]);
  if(meshId<0 || meshId>=MY_MESH_ARRAY_SIZE) {
    Oc_Snprintf(buf,sizeof(buf),
                "Invalid mesh id request: %d; should be between 0 and %d",
                meshId,MY_MESH_ARRAY_SIZE-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  int oldId = activeMeshId;
  activeMeshId = meshId;

  myFrame.SetMesh(myMeshArray[activeMeshId]);

  Oc_Snprintf(buf,sizeof(buf),"%d",oldId);
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}

int FreeMesh(ClientData,Tcl_Interp *interp,int argc,CONST84 char** argv)
{
  Tcl_ResetResult(interp);
  if(argc!=2) {
    Oc_Snprintf(buf,sizeof(buf),
                "wrong # args: should be \"%s meshid\"", argv[0]);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  int meshId = atoi(argv[1]);
  if(meshId<0 || meshId>=MY_MESH_ARRAY_SIZE) {
    Oc_Snprintf(buf,sizeof(buf),
                "Invalid mesh id request: %d; should be between 0 and %d",
                meshId,MY_MESH_ARRAY_SIZE-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  delete myMeshArray[meshId];
  myMeshArray[meshId]=new Vf_EmptyMesh();
  if(activeMeshId == meshId) {
    myFrame.SetMesh(myMeshArray[meshId]);
  }

  return TCL_OK;
}

int CopyMesh(ClientData,Tcl_Interp *interp,int argc,CONST84 char** argv)
{ // Note: new mesh id is allowed to be the same as original mesh id.
  Tcl_ResetResult(interp);
  if(argc<3 || argc>7) {
    Oc_Snprintf(buf,sizeof(buf),
                "CopyMesh must be called with 2-6 arguments: "
                "original mesh id, new mesh id, "
                "subsample, flip string, clipbox, clip_range"
                " (%d arguments passed)",
                argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  int origmeshId = atoi(argv[1]);
  if(origmeshId<0 || origmeshId>=MY_MESH_ARRAY_SIZE) {
    Oc_Snprintf(buf,sizeof(buf),
                "Invalid mesh id request: %d; should be between 0 and %d",
                origmeshId,MY_MESH_ARRAY_SIZE-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  int newmeshId = atoi(argv[2]);
  if(newmeshId<0 || newmeshId>=MY_MESH_ARRAY_SIZE) {
    Oc_Snprintf(buf,sizeof(buf),
                "Invalid mesh id request: %d; should be between 0 and %d",
                newmeshId,MY_MESH_ARRAY_SIZE-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  OC_REAL8m subsample = 0.;
  if(argc>3) subsample = Nb_Atof(argv[3]);
  const char* flipstr = "x:y:z";
  if(argc>4) flipstr = argv[4];

  // Clipbox
  Nb_BoundingBox<OC_REAL8> clipbox;
  myMeshArray[origmeshId]->GetPreciseRange(clipbox); // All by default
  if(argc>5 && argv[5][0]!='\0') {
    const char* parsestr=" \t\n:";
    char* workbuf = new char[strlen(argv[5])+1]; // clipbox corners
    strcpy(workbuf,argv[5]);
    char* nexttoken=workbuf;
    char *token1 = Nb_StrSep(&nexttoken,parsestr);
    char *token2 = Nb_StrSep(&nexttoken,parsestr);
    char *token3 = Nb_StrSep(&nexttoken,parsestr);
    char *token4 = Nb_StrSep(&nexttoken,parsestr);
    char *token5 = Nb_StrSep(&nexttoken,parsestr);
    char *token6 = Nb_StrSep(&nexttoken,parsestr);

    Nb_Vec3<OC_REAL8m> minpt,maxpt;
    clipbox.GetExtremes(minpt,maxpt);
    if(token1[0]!='\0' && strcmp("-",token1)!=0) {
      minpt.x = Nb_Atof(token1);
    }
    if(token2[0]!='\0' && strcmp("-",token2)!=0) {
      minpt.y = Nb_Atof(token2);
    }
    if(token3[0]!='\0' && strcmp("-",token3)!=0) {
      minpt.z = Nb_Atof(token3);
    }
    if(token4[0]!='\0' && strcmp("-",token4)!=0) {
      maxpt.x = Nb_Atof(token4);
    }
    if(token5[0]!='\0' && strcmp("-",token5)!=0) {
      maxpt.y = Nb_Atof(token5);
    }
    if(token6[0]!='\0' && strcmp("-",token6)!=0) {
      maxpt.z = Nb_Atof(token6);
    }
    delete[] workbuf;
    clipbox.Set(minpt,maxpt);
  }

  // Clip range?
  OC_BOOL clip_range = 0;
  if(argc>6 && argv[6][0]!='\0') {
    if(atoi(argv[6])) clip_range = 1;
  }

  // Create new mesh; if the original mesh is rectangular (i.e.,
  // Vf_GridVec3f or Vf_Empty), then make the new one of the
  // same type.
  Vf_Mesh* newmesh=NULL;
  const char* origtype = myMeshArray[origmeshId]->GetMeshType();
  if(strcmp(origtype,"Vf_EmptyMesh")==0) {
    newmesh = new Vf_EmptyMesh();
  } else if(strcmp(origtype,"Vf_GridVec3f")==0) {
    newmesh = new
      Vf_GridVec3f(*((Vf_GridVec3f*)myMeshArray[origmeshId]),
                   subsample,flipstr,clipbox,clip_range);
  } else {
    newmesh = new
      Vf_GeneralMesh3f(*myMeshArray[origmeshId],subsample,
                       flipstr,clipbox,clip_range);
  }

  // Place new mesh into myMeshArray
  if(myMeshArray[newmeshId]!=NULL) {
    delete myMeshArray[newmeshId];
  }
  myMeshArray[newmeshId] = newmesh;
  if(activeMeshId == newmeshId) {
    myFrame.SetMesh(newmesh);
  }

  return TCL_OK;
}

int PeriodicTranslate(ClientData,Tcl_Interp *interp,
                      int argc,CONST84 char** argv)
{ // Note: This code translate by whole cells only.  The input offset
  // is automatically adjusted to the nearest whole cell.
  Tcl_ResetResult(interp);
  if(argc!=6) {
    Oc_Snprintf(buf,sizeof(buf),
                "PeriodicTranslate must be called with 5 arguments: "
                "original mesh id, new mesh id, "
                "x-offset, y-offset, z-offset"
                " (%d arguments passed)",
                argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  int origmeshId = atoi(argv[1]);
  if(origmeshId<0 || origmeshId>=MY_MESH_ARRAY_SIZE) {
    Oc_Snprintf(buf,sizeof(buf),
                "Invalid mesh id request: %d; should be between 0 and %d",
                origmeshId,MY_MESH_ARRAY_SIZE-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  int newmeshId = atoi(argv[2]);
  if(newmeshId<0 || newmeshId>=MY_MESH_ARRAY_SIZE) {
    Oc_Snprintf(buf,sizeof(buf),
                "Invalid mesh id request: %d; should be between 0 and %d",
                newmeshId,MY_MESH_ARRAY_SIZE-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  OC_REAL8m xoff = Nb_Atof(argv[3]);
  OC_REAL8m yoff = Nb_Atof(argv[4]);
  OC_REAL8m zoff = Nb_Atof(argv[5]);

  // Special case handling for import Vf_EmptyMesh
  if(strcmp(myMeshArray[origmeshId]->GetMeshType(),"Vf_EmptyMesh")==0) {
    if(newmeshId != origmeshId) {
      if(myMeshArray[newmeshId]!=NULL) delete myMeshArray[newmeshId];
      myMeshArray[newmeshId] = new Vf_EmptyMesh();
    }
    return TCL_OK; // Nothing to do
  }

  // Rectangular mesh is required
  if(strcmp(myMeshArray[origmeshId]->GetMeshType(),"Vf_GridVec3f")!=0) {
    Oc_Snprintf(buf,sizeof(buf),"Invalid mesh type: %s\n",
                myMeshArray[origmeshId]->GetMeshType());
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  Vf_GridVec3f* grid =
    dynamic_cast<Vf_GridVec3f*>(myMeshArray[origmeshId]);
  if(grid==0) {
    Oc_Snprintf(buf,sizeof(buf),
                "Downcast of input mesh to Vf_GridVec3f failed.\n");
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  // Adjust (x,y,z) offset to cell index offset
  Nb_BoundingBox<OC_REAL8> range;
  grid->GetPreciseRange(range);
  OC_REAL8 xrange = range.GetWidth();
  OC_REAL8 yrange = range.GetHeight();
  OC_REAL8 zrange = range.GetDepth();
  if(xoff<0.0 || xrange<=xoff) xoff -= floor(xoff/xrange)*xrange;
  if(yoff<0.0 || yrange<=yoff) yoff -= floor(yoff/yrange)*yrange;
  if(zoff<0.0 || zrange<=zoff) zoff -= floor(zoff/zrange)*zrange;

  Nb_Vec3<OC_REAL8> cellsize = grid->GetGridStep();
  OC_INDEX ixoff = static_cast<OC_INDEX>(rint(xoff/cellsize.x));
  OC_INDEX iyoff = static_cast<OC_INDEX>(rint(yoff/cellsize.y));
  OC_INDEX izoff = static_cast<OC_INDEX>(rint(zoff/cellsize.z));

  // Create new mesh
  Vf_GridVec3f* ngrid = new Vf_GridVec3f(*grid,ixoff,iyoff,izoff);

  // Put new mesh into myMeshArray.  Note that this works
  // including the case where newmeshId == origmeshId.
  if(myMeshArray[newmeshId]!=NULL) delete myMeshArray[newmeshId];
  myMeshArray[newmeshId] = ngrid;

  if(activeMeshId == newmeshId) {
    myFrame.SetMesh(ngrid);
  }

  return TCL_OK;
}

int Resample(ClientData,Tcl_Interp *interp,
             int argc,CONST84 char** argv)
{
  Tcl_ResetResult(interp);
  if(argc!=13) {
    Oc_Snprintf(buf,sizeof(buf),
                "Resample must be called with 12 arguments: "
                "original mesh id, new mesh id, "
                "xmin, ymin, zmin, xmax, ymax, zmax, "
                "icount, jcount, kcount, method_order "
                "(%d arguments passed)",
                argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  int origmeshId = atoi(argv[1]);
  if(origmeshId<0 || origmeshId>=MY_MESH_ARRAY_SIZE) {
    Oc_Snprintf(buf,sizeof(buf),
                "Invalid mesh id request: %d; should be between 0 and %d",
                origmeshId,MY_MESH_ARRAY_SIZE-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  int newmeshId = atoi(argv[2]);
  if(newmeshId<0 || newmeshId>=MY_MESH_ARRAY_SIZE) {
    Oc_Snprintf(buf,sizeof(buf),
                "Invalid mesh id request: %d; should be between 0 and %d",
                newmeshId,MY_MESH_ARRAY_SIZE-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  Nb_Vec3<OC_REAL8> corner1(Nb_Atof(argv[3]),
                            Nb_Atof(argv[4]),
                            Nb_Atof(argv[5]));
  Nb_Vec3<OC_REAL8> corner2(Nb_Atof(argv[6]),
                            Nb_Atof(argv[7]),
                            Nb_Atof(argv[8]));
  Nb_BoundingBox<OC_REAL8> newrange;
  newrange.SortAndSet(corner1,corner2);

  OC_INDEX icount = static_cast<OC_INDEX>(atol(argv[9]));
  OC_INDEX jcount = static_cast<OC_INDEX>(atol(argv[10]));
  OC_INDEX kcount = static_cast<OC_INDEX>(atol(argv[11]));
  int method_order = atoi(argv[12]);

  // Rectangular mesh is required
  if(strcmp(myMeshArray[origmeshId]->GetMeshType(),"Vf_GridVec3f")!=0) {
    Oc_Snprintf(buf,sizeof(buf),"Invalid mesh type: %s\n",
                myMeshArray[origmeshId]->GetMeshType());
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  Vf_GridVec3f* origgrid =
    dynamic_cast<Vf_GridVec3f*>(myMeshArray[origmeshId]);
  if(origgrid==0) {
    Oc_Snprintf(buf,sizeof(buf),
                "Downcast of input mesh to Vf_GridVec3f failed.\n");
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  // Create new mesh
  Vf_GridVec3f* ngrid = new Vf_GridVec3f();
  ngrid->ResampleCopy(*origgrid,newrange,icount,jcount,kcount,method_order);

  // Put new mesh into myMeshArray.  Note that this works
  // including the case where newmeshId == origmeshId.
  if(myMeshArray[newmeshId]!=NULL) delete myMeshArray[newmeshId];
  myMeshArray[newmeshId] = ngrid;

  if(activeMeshId == newmeshId) {
    myFrame.SetMesh(ngrid);
  }

  return TCL_OK;
}

int ResampleAverage(ClientData,Tcl_Interp *interp,
                    int argc,CONST84 char** argv)
{
  Tcl_ResetResult(interp);
  if(argc!=12) {
    Oc_Snprintf(buf,sizeof(buf),
                "Resample must be called with 11 arguments: "
                "original mesh id, new mesh id, "
                "xmin, ymin, zmin, xmax, ymax, zmax, "
                "icount, jcount, kcount "
                "(%d arguments passed)",
                argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  int origmeshId = atoi(argv[1]);
  if(origmeshId<0 || origmeshId>=MY_MESH_ARRAY_SIZE) {
    Oc_Snprintf(buf,sizeof(buf),
                "Invalid mesh id request: %d; should be between 0 and %d",
                origmeshId,MY_MESH_ARRAY_SIZE-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  int newmeshId = atoi(argv[2]);
  if(newmeshId<0 || newmeshId>=MY_MESH_ARRAY_SIZE) {
    Oc_Snprintf(buf,sizeof(buf),
                "Invalid mesh id request: %d; should be between 0 and %d",
                newmeshId,MY_MESH_ARRAY_SIZE-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  Nb_Vec3<OC_REAL8> corner1(Nb_Atof(argv[3]),
                            Nb_Atof(argv[4]),
                            Nb_Atof(argv[5]));
  Nb_Vec3<OC_REAL8> corner2(Nb_Atof(argv[6]),
                            Nb_Atof(argv[7]),
                            Nb_Atof(argv[8]));
  Nb_BoundingBox<OC_REAL8> newrange;
  newrange.SortAndSet(corner1,corner2);

  OC_INDEX icount = static_cast<OC_INDEX>(atol(argv[9]));
  OC_INDEX jcount = static_cast<OC_INDEX>(atol(argv[10]));
  OC_INDEX kcount = static_cast<OC_INDEX>(atol(argv[11]));

  // Rectangular mesh is required
  if(strcmp(myMeshArray[origmeshId]->GetMeshType(),"Vf_GridVec3f")!=0) {
    Oc_Snprintf(buf,sizeof(buf),"Invalid mesh type: %s\n",
                myMeshArray[origmeshId]->GetMeshType());
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  Vf_GridVec3f* origgrid =
    dynamic_cast<Vf_GridVec3f*>(myMeshArray[origmeshId]);
  if(origgrid==0) {
    Oc_Snprintf(buf,sizeof(buf),
                "Downcast of input mesh to Vf_GridVec3f failed.\n");
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  // Create new mesh
  Vf_GridVec3f* ngrid = new Vf_GridVec3f();
  ngrid->ResampleCopyAverage(*origgrid,newrange,icount,jcount,kcount);

  // Put new mesh into myMeshArray.  Note that this works
  // including the case where newmeshId == origmeshId.
  if(myMeshArray[newmeshId]!=NULL) delete myMeshArray[newmeshId];
  myMeshArray[newmeshId] = ngrid;

  if(activeMeshId == newmeshId) {
    myFrame.SetMesh(ngrid);
  }

  return TCL_OK;
}

int CrossProductMesh(ClientData,Tcl_Interp *interp,
                     int argc,CONST84 char** argv)
{
  Tcl_ResetResult(interp);

  if(argc<2 || argc>3) {
    Oc_Snprintf(buf,sizeof(buf),
            "CrossProductMesh must be called with 1 or 2 arguments:"
                " ?meshA? meshB (%d arguments passed)\n",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  int meshA = activeMeshId;
  int meshB = atoi(argv[1]);
  if(argc>2) {
    meshA = meshB;
    meshB = atoi(argv[2]);
  }
  if(meshA<0 || meshA>=MY_MESH_ARRAY_SIZE) {
    Oc_Snprintf(buf,sizeof(buf),
            "Invalid meshA request: %d; should be between 0 and %d",
            meshA,MY_MESH_ARRAY_SIZE-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  if(meshB<0 || meshB>=MY_MESH_ARRAY_SIZE) {
    Oc_Snprintf(buf,sizeof(buf),
            "Invalid meshB request: %d; should be between 0 and %d",
            meshB,MY_MESH_ARRAY_SIZE-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  // Take pointwise cross product of meshes here
  if((*myMeshArray[meshA]).CrossProductMesh(*myMeshArray[meshB])!=0) {
    Oc_Snprintf(buf,sizeof(buf),"Meshes aren't compatible");
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  // Update maghints
  OC_REAL8m minmag, maxmag;
  myMeshArray[meshA]->GetNonZeroValueMagSpan(minmag,maxmag);
  myMeshArray[meshA]->SetMagHints(minmag,maxmag);

  return TCL_OK;
}

int DifferenceMesh(ClientData,Tcl_Interp *interp,
                   int argc,CONST84 char** argv)
{
  Tcl_ResetResult(interp);

  if(argc<2 || argc>3) {
    Oc_Snprintf(buf,sizeof(buf),
            "DifferenceMesh must be called with 1 or 2 arguments:"
                " ?meshA? meshB (%d arguments passed)\n",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  int meshA = activeMeshId;
  int meshB = atoi(argv[1]);
  if(argc>2) {
    meshA = meshB;
    meshB = atoi(argv[2]);
  }
  if(meshA<0 || meshA>=MY_MESH_ARRAY_SIZE) {
    Oc_Snprintf(buf,sizeof(buf),
            "Invalid meshA request: %d; should be between 0 and %d",
            meshA,MY_MESH_ARRAY_SIZE-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  if(meshB<0 || meshB>=MY_MESH_ARRAY_SIZE) {
    Oc_Snprintf(buf,sizeof(buf),
            "Invalid meshB request: %d; should be between 0 and %d",
            meshB,MY_MESH_ARRAY_SIZE-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  // Difference meshes here
  if((*myMeshArray[meshA]).SubtractMesh(*myMeshArray[meshB])!=0) {
    Oc_Snprintf(buf,sizeof(buf),"Meshes aren't compatible");
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  // Update maghints
  OC_REAL8m minmag, maxmag;
  myMeshArray[meshA]->GetNonZeroValueMagSpan(minmag,maxmag);
  myMeshArray[meshA]->SetMagHints(minmag,maxmag);

  return TCL_OK;
}

int GetMeshType(ClientData,Tcl_Interp *interp,int argc,CONST84 char**)
{
  Tcl_ResetResult(interp);
  if(argc!=1) {
    Oc_Snprintf(buf,sizeof(buf),
            "GetMeshType must be called with no arguments"
            " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  const char *name=myMeshArray[activeMeshId]->GetMeshType();
  if(name!=NULL) strncpy(buf,name,sizeof(buf)-1);
  else           buf[0]='\0';
  buf[sizeof(buf)-1] = '\0'; // Safety
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}

int GetMeshValueMagSpan(ClientData,Tcl_Interp *interp,int argc,CONST84 char**)
{
  Tcl_ResetResult(interp);
  if(argc!=1) {
    Oc_Snprintf(buf,sizeof(buf),
            "GetMeshValueMagSpan must be called with no arguments"
            " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  OC_REAL8m min,max;
  myMeshArray[activeMeshId]->GetValueMagSpan(min,max);
  Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g",
              static_cast<double>(min),
              static_cast<double>(max));
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}

int GetMeshValueMean(ClientData,Tcl_Interp *interp,int argc,CONST84 char**)
{
  Tcl_ResetResult(interp);
  if(argc!=1) {
    Oc_Snprintf(buf,sizeof(buf),
            "GetMeshValueMean must be called with no arguments"
            " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  Nb_Vec3<OC_REAL8> mean;
  myMeshArray[activeMeshId]->GetValueMean(mean);
  Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g %.17g",
              static_cast<double>(mean.x),
              static_cast<double>(mean.y),
              static_cast<double>(mean.z));
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}

int GetMeshValueRMS(ClientData,Tcl_Interp *interp,int argc,CONST84 char**)
{
  Tcl_ResetResult(interp);
  if(argc!=1) {
    Oc_Snprintf(buf,sizeof(buf),
            "GetMeshValueRMS must be called with no arguments"
            " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  OC_REAL8m rms = myMeshArray[activeMeshId]->GetValueRMS();
  Oc_Snprintf(buf,sizeof(buf),"%.17g",
              static_cast<double>(rms));
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}

int GetMeshValueL1(ClientData,Tcl_Interp *interp,int argc,CONST84 char**)
{
  Tcl_ResetResult(interp);
  if(argc!=1) {
    Oc_Snprintf(buf,sizeof(buf),
            "GetMeshValueL1 must be called with no arguments"
            " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  OC_REAL8m rms = myMeshArray[activeMeshId]->GetValueL1();
  Oc_Snprintf(buf,sizeof(buf),"%.17g",
              static_cast<double>(rms));
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}

int GetMeshValueUnit(ClientData,Tcl_Interp *interp,int argc,CONST84 char**)
{
  Tcl_ResetResult(interp);
  if(argc!=1) {
    Oc_Snprintf(buf,sizeof(buf),
            "GetMeshValueUnit must be called with no arguments"
            " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  const char* units = myMeshArray[activeMeshId]->GetValueUnit();
  Tcl_AppendResult(interp,Oc_AutoBuf(units).GetStr(),(char *)NULL);
  return TCL_OK;
}

static int IsRectangularMesh(const Vf_Mesh* mesh)
{
  const char *name=mesh->GetMeshType();
  int rect=0;
  if(strcmp(name,"Vf_GridVec3f")==0 ||
     strcmp(name,"Vf_EmptyMesh")==0) {
    // Known regular rectangular meshes
    rect=1;
  } // Otherwise assume irregular
  return rect;
}

int IsRectangularMesh(ClientData,Tcl_Interp *interp,int argc,CONST84 char**)
{
  Tcl_ResetResult(interp);
  if(argc!=1) {
    Oc_Snprintf(buf,sizeof(buf),
            "IsRectangularMesh must be called with no arguments"
            " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  int rect = IsRectangularMesh(myMeshArray[activeMeshId]);
  Oc_Snprintf(buf,sizeof(buf),"%d",rect);
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}

int GetVecColor(ClientData,Tcl_Interp *interp,
                int argc,CONST84 char** argv)
{
  Tcl_ResetResult(interp);
  if(argc!=2) {
    Oc_Snprintf(buf,sizeof(buf),
            "GetVecColor must be called with 1 argument:"
            " A 3 element list representing a vector"
            " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  int carr_size;
  CONST84 char** carr;
  if (Tcl_SplitList(interp,argv[1],&carr_size,&carr)!=TCL_OK) {
    return TCL_ERROR;
  }
  if(carr_size!=3) {
    Oc_Snprintf(buf,sizeof(buf),
                "Input list has %d != 3 elements",carr_size);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  Nb_Vec3<OC_REAL4> v(static_cast<OC_REAL4m>(Nb_Atof(carr[0])),
                   static_cast<OC_REAL4m>(Nb_Atof(carr[1])),
                   static_cast<OC_REAL4m>(Nb_Atof(carr[2])));
  Tcl_Free((char *)carr);
  myFrame.GetVecColor(v);
  Tcl_AppendResult(interp,Oc_AutoBuf(myFrame.GetVecColor(v)).GetStr(),
                   (char *)NULL);
  return TCL_OK;
}


int GetMeshName(ClientData,Tcl_Interp *interp,int argc,
                CONST84 char** argv)
{
  Tcl_ResetResult(interp);

  if(argc<1 || argc>2) {
    Oc_Snprintf(buf,sizeof(buf),
            "GetMeshName must be called with 0 or 1 argument:"
                " ?meshId? (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  int meshId = activeMeshId;
  if(argc>1) meshId = atoi(argv[1]);
  if(meshId<0 || meshId>=MY_MESH_ARRAY_SIZE) {
    Oc_Snprintf(buf,sizeof(buf),
            "Invalid mesh id request: %d; should be between 0 and %d",
            meshId,MY_MESH_ARRAY_SIZE-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  const char *name=myMeshArray[meshId]->GetName();
  if(name!=NULL) strncpy(buf,name,sizeof(buf)-1);
  else           buf[0]='\0';
  buf[sizeof(buf)-1] = '\0'; // Safety
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}

int GetMeshStructureInfo(ClientData,Tcl_Interp *interp,
                         int argc,CONST84 char** argv)
{
  Tcl_ResetResult(interp);
  if(argc<1 || argc>2) {
    Oc_Snprintf(buf,sizeof(buf),
            "GetMeshStructureInfo must be called with 0 or 1 argument:"
                " ?meshId? (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  int meshId = activeMeshId;
  if(argc>1) meshId = atoi(argv[1]);
  if(meshId<0 || meshId>=MY_MESH_ARRAY_SIZE) {
    Oc_Snprintf(buf,sizeof(buf),
            "Invalid mesh id request: %d; should be between 0 and %d",
            meshId,MY_MESH_ARRAY_SIZE-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  const Vf_Mesh* myMesh = myMeshArray[meshId];
  OC_INDEX size=myMesh->GetSize();

  Nb_BoundingBox<OC_REAL8> datarange;
  myMesh->GetPreciseDataRange(datarange);
  Nb_Vec3<OC_REAL8m> minpt,maxpt;
  datarange.GetExtremes(minpt,maxpt);

  Nb_BoundingBox<OC_REAL8> meshrange;
  myMesh->GetPreciseRange(meshrange);
  Nb_Vec3<OC_REAL8m> mesh_minpt,mesh_maxpt;
  meshrange.GetExtremes(mesh_minpt,mesh_maxpt);

  OC_REAL8m value_min,value_max;
  Nb_LocatedVector<OC_REAL8> min_vec,max_vec;
  const char *meshunit = myMesh->GetMeshUnit();

  myMesh->GetValueMagSpan(min_vec,max_vec);
  value_min = min_vec.value.Mag();
  value_max = max_vec.value.Mag();

  const char *valueunit = myMesh->GetValueUnit();
  if(IsRectangularMesh(myMesh)) {
    if(size<1 || strcmp("Vf_GridVec3f",myMesh->GetMeshType())!=0) {
      Oc_Snprintf(buf,sizeof(buf),
                  "Rectangular mesh\n Mesh size: %d\n",size);
    } else {
      Vf_GridVec3f* regmesh = (Vf_GridVec3f*)myMesh;
      OC_INDEX dimx,dimy,dimz;
      regmesh->GetDimens(dimx,dimy,dimz);
      Nb_Vec3<OC_REAL8> basept = regmesh->GetBasePoint();
      Nb_Vec3<OC_REAL8> step = regmesh->GetGridStep();
      Oc_Snprintf(buf,sizeof(buf),
          "Rectangular mesh\n"
          " Mesh size: %d\n"
          " Dimensions: %d %d %d\n"
          " Value magnitude span: %#.17g [(%g,%g,%g) at (%g,%g,%g)]\n"
          "                    to %#.17g [(%g,%g,%g) at (%g,%g,%g)] (in %s)\n"
          " Data range: (%g,%g,%g) x (%g,%g,%g) (in %s)\n"
          " Mesh range: (%g,%g,%g) x (%g,%g,%g) (in %s)\n"
          " Mesh base/step: (%g,%g,%g)/(%g,%g,%g) (in %s)",
          size,
          dimx,dimy,dimz,
          static_cast<double>(value_min),
          static_cast<double>(min_vec.value.x),
          static_cast<double>(min_vec.value.y),
          static_cast<double>(min_vec.value.z),
          static_cast<double>(min_vec.location.x),
          static_cast<double>(min_vec.location.y),
          static_cast<double>(min_vec.location.z),
          static_cast<double>(value_max),
          static_cast<double>(max_vec.value.x),
          static_cast<double>(max_vec.value.y),
          static_cast<double>(max_vec.value.z),
          static_cast<double>(max_vec.location.x),
          static_cast<double>(max_vec.location.y),
          static_cast<double>(max_vec.location.z),
          valueunit,
          static_cast<double>(minpt.x),
          static_cast<double>(minpt.y),
          static_cast<double>(minpt.z),
          static_cast<double>(maxpt.x),
          static_cast<double>(maxpt.y),
          static_cast<double>(maxpt.z),meshunit,
          static_cast<double>(mesh_minpt.x),
          static_cast<double>(mesh_minpt.y),
          static_cast<double>(mesh_minpt.z),
          static_cast<double>(mesh_maxpt.x),
          static_cast<double>(mesh_maxpt.y),
          static_cast<double>(mesh_maxpt.z),meshunit,
          static_cast<double>(basept.x),
          static_cast<double>(basept.y),
          static_cast<double>(basept.z),
          static_cast<double>(step.x),
          static_cast<double>(step.y),
          static_cast<double>(step.z),meshunit);
    }
  } else {
    Oc_Snprintf(buf,sizeof(buf),
          "Irregular mesh\n"
          " Size: %d\n"
          " Value magnitude span: %#.17g [(%g,%g,%g) at (%g,%g,%g)]\n"
          "                    to %#.17g [(%g,%g,%g) at (%g,%g,%g)] (in %s)\n"
          " Data range: (%g,%g,%g) x (%g,%g,%g) (in %s)\n"
          " Mesh range: (%g,%g,%g) x (%g,%g,%g) (in %s)",
          size,
          static_cast<double>(value_min),
          static_cast<double>(min_vec.value.x),
          static_cast<double>(min_vec.value.y),
          static_cast<double>(min_vec.value.z),
          static_cast<double>(min_vec.location.x),
          static_cast<double>(min_vec.location.y),
          static_cast<double>(min_vec.location.z),
          static_cast<double>(value_max),
          static_cast<double>(max_vec.value.x),
          static_cast<double>(max_vec.value.y),
          static_cast<double>(max_vec.value.z),
          static_cast<double>(max_vec.location.x),
          static_cast<double>(max_vec.location.y),
          static_cast<double>(max_vec.location.z),
          valueunit,
          static_cast<double>(minpt.x),
          static_cast<double>(minpt.y),
          static_cast<double>(minpt.z),
          static_cast<double>(maxpt.x),
          static_cast<double>(maxpt.y),
          static_cast<double>(maxpt.z),meshunit,
          static_cast<double>(mesh_minpt.x),
          static_cast<double>(mesh_minpt.y),
          static_cast<double>(mesh_minpt.z),
          static_cast<double>(mesh_maxpt.x),
          static_cast<double>(mesh_maxpt.y),
          static_cast<double>(mesh_maxpt.z),meshunit);
  }
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}

int GetMeshTitle(ClientData,Tcl_Interp *interp,int argc,
                 CONST84 char** argv)
{
  Tcl_ResetResult(interp);
  if(argc<1 || argc>2) {
    Oc_Snprintf(buf,sizeof(buf),
            "GetMeshTitle must be called with 0 or 1 argument: ?meshId?"
            " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  int meshId = activeMeshId;
  if(argc>1) meshId = atoi(argv[1]);
  if(meshId<0 || meshId>=MY_MESH_ARRAY_SIZE) {
    Oc_Snprintf(buf,sizeof(buf),
            "Invalid mesh id request: %d; should be between 0 and %d",
            meshId,MY_MESH_ARRAY_SIZE-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  const char *title=myMeshArray[meshId]->GetTitle();
  if(title!=NULL) strncpy(buf,title,sizeof(buf)-1);
  else            buf[0]='\0';
  Tcl_AppendResult(interp,buf,(char *)NULL);
  buf[sizeof(buf)-1] = '\0'; // Safety
  return TCL_OK;
}

int SetMeshTitle(ClientData,Tcl_Interp *interp,
                 int argc,CONST84 char** argv)
{
  Tcl_ResetResult(interp);
  if(argc<2 || argc>3) {
    Oc_Snprintf(buf,sizeof(buf),
            "SetMeshTitle must be called with 1-2 arguments:"
            " ?meshId? new_title (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  int meshId = activeMeshId;
  const char* title = argv[1];
  if(argc>2) {
    meshId = atoi(argv[1]);
    title = argv[2];
  }
  if(meshId<0 || meshId>=MY_MESH_ARRAY_SIZE) {
    Oc_Snprintf(buf,sizeof(buf),
            "Invalid mesh id request: %d; should be between 0 and %d",
            meshId,MY_MESH_ARRAY_SIZE-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  myMeshArray[meshId]->SetTitle(title);
  return TCL_OK;
}

int GetMeshDescription(ClientData,Tcl_Interp *interp,
                       int argc,CONST84 char** argv)
{
  Tcl_ResetResult(interp);
  if(argc<1 || argc>2) {
    Oc_Snprintf(buf,sizeof(buf),
            "GetMeshDescription must be called with 0 or 1 argument:"
                " ?meshId? (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  int meshId = activeMeshId;
  if(argc>1) meshId = atoi(argv[1]);
  if(meshId<0 || meshId>=MY_MESH_ARRAY_SIZE) {
    Oc_Snprintf(buf,sizeof(buf),
            "Invalid mesh id request: %d; should be between 0 and %d",
            meshId,MY_MESH_ARRAY_SIZE-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  const char *desc=myMeshArray[meshId]->GetDescription();
  if(desc!=NULL) strncpy(buf,desc,sizeof(buf)-1);
  else           buf[0]='\0';
  buf[sizeof(buf)-1] = '\0'; // Safety
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}

int GetMeshSize(ClientData,Tcl_Interp *interp,
                int argc,CONST84 char** argv)
{
  // Returns an integer reporting the number of nodes in the mesh
  Tcl_ResetResult(interp);
  if(argc<1 || argc>2) {
    Oc_Snprintf(buf,sizeof(buf),
            "GetMeshSize must be called with 0 or 1 argument:"
                " ?meshId? (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  int meshId = activeMeshId;
  if(argc>1) meshId = atoi(argv[1]);
  if(meshId<0 || meshId>=MY_MESH_ARRAY_SIZE) {
    Oc_Snprintf(buf,sizeof(buf),
            "Invalid mesh id request: %d; should be between 0 and %d",
            meshId,MY_MESH_ARRAY_SIZE-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  OC_INDEX size=myMeshArray[meshId]->GetSize();
  Oc_Snprintf(buf,sizeof(buf),"%ld",static_cast<long>(size));
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}

int GetMeshCellSize(ClientData,Tcl_Interp *interp,
                    int argc,CONST84 char** argv)
{
  // Returns a triple: xstep ystep zstep (in mesh units)
  Tcl_ResetResult(interp);
  if(argc<1 || argc>2) {
    Oc_Snprintf(buf,sizeof(buf),
            "GetMeshCellSize must be called with 0 or 1 argument:"
                " ?meshId? (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  int meshId = activeMeshId;
  if(argc>1) meshId = atoi(argv[1]);
  if(meshId<0 || meshId>=MY_MESH_ARRAY_SIZE) {
    Oc_Snprintf(buf,sizeof(buf),
            "Invalid mesh id request: %d; should be between 0 and %d",
            meshId,MY_MESH_ARRAY_SIZE-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  Nb_Vec3<OC_REAL4> dim=myMeshArray[meshId]->GetApproximateCellDimensions();
  Oc_Snprintf(buf,sizeof(buf),"%.8g %.8g %.8g",
              static_cast<double>(dim.x),
              static_cast<double>(dim.y),
              static_cast<double>(dim.z));
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}

int GetMeshCoordinates(ClientData,Tcl_Interp *interp,
                    int argc,CONST84 char** argv)
{ // Import: (x,y,z) in display coordinates
  // Export: (x,y,z) in mesh coordinates
  // The returned results are OC_REAL4 precision.
  Tcl_ResetResult(interp);
  if(argc!=4) {
    Oc_Snprintf(buf,sizeof(buf),
                "GetMeshCoordinates must be called with"
                " 3 arguments: x y z, in display coordinates"
                " (%d arguments passed).",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  OC_REAL4 x = static_cast<OC_REAL4>(Nb_Atof(argv[1]));
  OC_REAL4 y = static_cast<OC_REAL4>(Nb_Atof(argv[2]));
  OC_REAL4 z = static_cast<OC_REAL4>(Nb_Atof(argv[3]));
  Nb_Vec3<OC_REAL4> pt(x,y,z);
  myFrame.CoordinatePointTransform(myFrame.GetCoordinates(),
                                   CS_CALCULATION_STANDARD,
                                   pt);
  Oc_Snprintf(buf,sizeof(buf),"%.8g %.8g %.8g",
              static_cast<double>(pt.x),
              static_cast<double>(pt.y),
              static_cast<double>(pt.z));
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}

int GetDisplayCoordinates(ClientData,Tcl_Interp *interp,
                    int argc,CONST84 char** argv)
{ // Import: (x,y,z) in mesh coordinates
  // Export: (x,y,z) in display coordinates
  // The returned results are OC_REAL4 precision.
  Tcl_ResetResult(interp);
  if(argc!=4) {
    Oc_Snprintf(buf,sizeof(buf),
                "GetDisplayCoordinates must be called with"
                " 3 arguments: x y z, in display coordinates"
                " (%d arguments passed).",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  OC_REAL4 x = static_cast<OC_REAL4>(Nb_Atof(argv[1]));
  OC_REAL4 y = static_cast<OC_REAL4>(Nb_Atof(argv[2]));
  OC_REAL4 z = static_cast<OC_REAL4>(Nb_Atof(argv[3]));
  Nb_Vec3<OC_REAL4> pt(x,y,z);
  myFrame.CoordinatePointTransform(CS_CALCULATION_STANDARD,
                                   myFrame.GetCoordinates(),
                                   pt);
  Oc_Snprintf(buf,sizeof(buf),"%.8g %.8g %.8g",
              static_cast<double>(pt.x),
              static_cast<double>(pt.y),
              static_cast<double>(pt.z));
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}

int FindMeshVector(ClientData,Tcl_Interp *interp,
                    int argc,CONST84 char** argv)
{ // Import: x y z, in mesh coordinates
  // Export: x y z vx vy vz, in mesh coordinates,
  //   with viewaxis transformation applied.
  // The returned results are OC_REAL8 precision.
  Tcl_ResetResult(interp);
  if(argc!=4) {
    Oc_Snprintf(buf,sizeof(buf),
                "FindMeshVector must be called with"
                " 3 arguments: x y z, in mesh coordinates"
                " (%d arguments passed).",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  OC_REAL8 x = Nb_Atof(argv[1]);
  OC_REAL8 y = Nb_Atof(argv[2]);
  OC_REAL8 z = Nb_Atof(argv[3]);
  Nb_Vec3<OC_REAL8> pos(x,y,z);
  Nb_LocatedVector<OC_REAL8> lv;
  myMeshArray[activeMeshId]->FindPreciseClosest(pos,lv);
  lv.value *= myMeshArray[activeMeshId]->GetValueMultiplier();

  // viewaxis coordinate transform
  const char* viewaxis
    = Tcl_GetVar2(interp, OC_CONST84_CHAR("plot_config"),
                  OC_CONST84_CHAR("viewaxis"),
                  TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG);
  if(viewaxis==NULL) return TCL_ERROR;
  if(strcmp("+z",viewaxis)!=0 && strcmp("z",viewaxis)!=0) {
    static OC_CHAR cmdbuf[MY_BUF_SIZE];
    Oc_Snprintf(cmdbuf,sizeof(cmdbuf),
                "ApplyAxisTransform %s +z %.17g %.17g %.17g",
                viewaxis,
                static_cast<double>(lv.location.x),
                static_cast<double>(lv.location.y),
                static_cast<double>(lv.location.z));
    if(Tcl_Eval(interp,cmdbuf)!=TCL_OK) return TCL_ERROR;
    if(lv.location.Set(Tcl_GetStringResult(interp))!=0) {
      Oc_Snprintf(buf,sizeof(buf),
                  "Import string to lv.location.Set"
                  " not a numeric triplet: %s",
                  Tcl_GetStringResult(interp));
      Tcl_SetResult(interp,buf,TCL_VOLATILE);
      return TCL_ERROR;
    }
    Oc_Snprintf(cmdbuf,sizeof(cmdbuf),
                "ApplyAxisTransform %s +z %.17g %.17g %.17g",
                viewaxis,
                static_cast<double>(lv.value.x),
                static_cast<double>(lv.value.y),
                static_cast<double>(lv.value.z));
    if(Tcl_Eval(interp,cmdbuf)!=TCL_OK) return TCL_ERROR;
    if(lv.value.Set(Tcl_GetStringResult(interp))!=0) {
      Oc_Snprintf(buf,sizeof(buf),
                  "Import string to lv.value.Set"
                  " not a numeric triplet: %s",
                  Tcl_GetStringResult(interp));
      Tcl_SetResult(interp,buf,TCL_VOLATILE);
      return TCL_ERROR;
    }
  }

  // Save and return
  Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g %.17g %.17g %.17g %.17g",
              static_cast<double>(lv.location.x),
              static_cast<double>(lv.location.y),
              static_cast<double>(lv.location.z),
              static_cast<double>(lv.value.x),
              static_cast<double>(lv.value.y),
              static_cast<double>(lv.value.z));
  Tcl_SetResult(interp,buf,TCL_VOLATILE);

  return TCL_OK;
}

int GetMeshIncrement(ClientData,Tcl_Interp *interp,
                     int argc,CONST84 char**)
{
  Tcl_ResetResult(interp);
  if(argc!=1) {
    Oc_Snprintf(buf,sizeof(buf),
            "GetMeshIncrement must be called with no arguments"
            " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  Oc_Snprintf(buf,sizeof(buf),"%f",
              myMeshArray[activeMeshId]->GetSubsampleGrit());
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}

int GetMeshSpatialUnitString(ClientData,Tcl_Interp *interp,
                  int argc,CONST84 char** argv)
{ // Returns string denoting mesh spatial units
  Tcl_ResetResult(interp);
  if(argc!=1) {
    Oc_Snprintf(buf,sizeof(buf),
                "wrong # args: should be \"%s\"", argv[0]);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  Oc_Snprintf(buf,sizeof(buf),"%s",
              myMeshArray[activeMeshId]->GetMeshUnit());
  if(strcmp("unknown",buf)==0) buf[0]='\0';
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}

int GetMeshRange(ClientData,Tcl_Interp *interp,
                  int argc,CONST84 char** argv)
{ // Returns 6-tuple: xmin ymin zmin xmax ymax zmax, in mesh units.
  Tcl_ResetResult(interp);
  if(argc<1 || argc>2) {
    Oc_Snprintf(buf,sizeof(buf),
            "GetMeshRange must be called with 0 or 1 argument:"
                " ?meshId? (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  int meshId = activeMeshId;
  if(argc>1) meshId = atoi(argv[1]);
  if(meshId<0 || meshId>=MY_MESH_ARRAY_SIZE) {
    Oc_Snprintf(buf,sizeof(buf),
            "Invalid mesh id request: %d; should be between 0 and %d",
            meshId,MY_MESH_ARRAY_SIZE-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  Nb_Vec3<OC_REAL8> minpt,maxpt;
  Nb_BoundingBox<OC_REAL8> range;
  myMeshArray[meshId]->GetPreciseRange(range);
  range.GetExtremes(minpt,maxpt);
  Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g %.17g %.17g %.17g %.17g",
              static_cast<double>(minpt.x),
              static_cast<double>(minpt.y),
              static_cast<double>(minpt.z),
              static_cast<double>(maxpt.x),
              static_cast<double>(maxpt.y),
              static_cast<double>(maxpt.z));
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}

int GetMeshZRange(ClientData,Tcl_Interp *interp,
                  int argc,CONST84 char** /* argv */)
{ // Returns a pair: zmin zmax.  These are in mesh units.
  Tcl_ResetResult(interp);
  if(argc!=1) {
    Oc_Snprintf(buf,sizeof(buf),
            "GetMeshZRange must be called with"
            " no arguments (%d arguments passed).",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  Nb_Vec3<OC_REAL8> minpt,maxpt;
  Nb_BoundingBox<OC_REAL8> range;
  myMeshArray[activeMeshId]->GetPreciseRange(range);
  OC_REAL8m zmin=0.,zmax=0.;
  if(!range.IsEmpty()) {
    range.GetExtremes(minpt,maxpt);
    zmin=minpt.z;
    zmax=maxpt.z;
  }
  Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g",
              static_cast<double>(zmin),static_cast<double>(zmax));
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}

int GetZsliceCount(ClientData,Tcl_Interp *interp,
                   int argc,CONST84 char** /* argv */)
{
  Tcl_ResetResult(interp);
  if(argc!=1) {
    Oc_Snprintf(buf,sizeof(buf),
            "GetZsliceCount must be called with no arguments"
            " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  OC_INDEX slicecount = myMeshArray[activeMeshId]->GetZsliceCount();

  Oc_Snprintf(buf,sizeof(buf),"%d",slicecount);
  Tcl_AppendResult(interp,buf,(char *)NULL);

  return TCL_OK;
}

int GetZsliceLevels(ClientData,Tcl_Interp *interp,
                    int argc,CONST84 char** argv)
{
  Tcl_ResetResult(interp);
  if(argc!=3) {
    Oc_Snprintf(buf,sizeof(buf),
            "GetZsliceLevels must be called with 2 arguments:"
            " z-low z-high (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  OC_REAL8m zlow  = Nb_Atof(argv[1]);
  OC_REAL8m zhigh = Nb_Atof(argv[2]);
  OC_INDEX islicelow,islicehigh;
  myMeshArray[activeMeshId]->GetZslice(zlow,zhigh,islicelow,islicehigh);

  Oc_Snprintf(buf,sizeof(buf),"%d %d",islicelow,islicehigh);
  Tcl_AppendResult(interp,buf,(char *)NULL);

  return TCL_OK;
}


int ChangeMesh(ClientData cd,Tcl_Interp *interp,
               int argc,CONST84 char** argv)
{
  Tcl_ResetResult(interp);
  if(argc<5 || argc>6) {
    Oc_Snprintf(buf,sizeof(buf),
            "ChangeMesh must be called with 4 or 5 arguments:"
            " mesh_filename"
            " frame_width frame_height rotation_degrees"
            " [zoom]"
            " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  if(activeMeshId<0 || activeMeshId>=MY_MESH_ARRAY_SIZE) {
    Oc_Snprintf(buf,sizeof(buf),
            "PROGRAMMING ERROR: activeMeshId=%d is out-of-range: [0,%d]",
            activeMeshId,MY_MESH_ARRAY_SIZE-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  delete myMeshArray[activeMeshId];
  myMeshArray[activeMeshId]=NULL; // Safety
  if(argv[1]==NULL || argv[1][0]=='\0') { // Null request
    myMeshArray[activeMeshId]=new Vf_EmptyMesh();
  }
  else {
    Vf_FileInput *vffreader
      = Vf_FileInput::NewReader(argv[1]);
    if(vffreader==NULL) { // Unknown file type
      myMeshArray[activeMeshId]=new Vf_EmptyMesh();
    }
    else { // Reader found for file argv[1]
      myMeshArray[activeMeshId]=vffreader->NewMesh();
      delete vffreader;
      if(myMeshArray[activeMeshId]==NULL)
        myMeshArray[activeMeshId]=new Vf_EmptyMesh(); // Safety
    }
  }
  myFrame.SetMesh(myMeshArray[activeMeshId]);

  OC_REAL4m width=static_cast<OC_REAL4m>(Nb_Atof(argv[2]));
  OC_REAL4m height=static_cast<OC_REAL4m>(Nb_Atof(argv[3]));
  CoordinateSystem coords =
    AngleToCoords(static_cast<OC_REAL4m>(Nb_Atof(argv[4])));
  OC_REAL4m zoom=-1.;
  if(argc>5) {
    zoom=static_cast<OC_REAL4m>(Nb_Atof(argv[5]));
  }

  myFrame.SetCoordinates(coords);

  if(zoom>0.) SetZoom(zoom);
  else        SetZoom(interp,width,height);

  UpdatePlotConfiguration(cd,interp,0,NULL);
  /// For now, just use current configuration /**/

  return TCL_OK;
}

int DrawFrame(ClientData,Tcl_Interp *interp,
              int argc,CONST84 char** argv)
{
  /*
   * We use the following buffer to store commands for passing to
   * Tcl_Eval().  It is important that we do not use the global
   * buffer, buf, for commands evaluted by Tcl_Eval() since that is
   * used to write result strings to Tcl_AppendResult().  If the
   * contents of a buffer are overwritten while Tcl_Eval() is
   * evaluating them, bad things can happen.
   *
   * Note that similar problems can occur if the contents of
   * cmdbuf are overwritten while Tcl_Eval() is evaluate it.  This
   * means it is a bad thing for DrawFrame() to appear on the call
   * stack twice.  Take care that none of the Tcl_Eval()'s below make
   * another call to DrawFrame().
   */
  static OC_CHAR cmdbuf[MY_BUF_SIZE];

  int error_code;
  Tcl_ResetResult(interp);
  if(argc!=3) {
    Oc_Snprintf(buf,sizeof(buf),
            "DrawFrame must be called with 2 arguments: "
            "canvas_name SliceCompatMode (%d arguments passed)",
            argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  const char* canvas=argv[1];
  int zslicecompat=atoi(argv[2]);

  Oc_Snprintf(cmdbuf,sizeof(cmdbuf),
              "{%s} delete all ;# update idletasks",canvas);
  /// Uncomment the "update idletasks" command to get a white
  /// screen between frames
  error_code=Tcl_Eval(interp,cmdbuf); // Clear canvas
  if(error_code!=TCL_OK) return error_code;

  // Redraw canvas
  if(zslicecompat) error_code=myFrame.Render(canvas,0);
  else error_code=myFrame.Render(canvas,1);  // Hide arrows and pixels
  if(error_code!=TCL_OK) {
    Oc_Snprintf(buf,sizeof(buf),
                "\nError from myFrame.Render(%s,0)",canvas);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return error_code;
  }

  Oc_Snprintf(cmdbuf,sizeof(cmdbuf),"InitializeSliceDisplay");
  error_code=Tcl_Eval(interp,cmdbuf);
  if(error_code!=TCL_OK) {
    Oc_Snprintf(buf,sizeof(buf),"\nError from InitializeSliceDisplay");
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return error_code;
  }

  Oc_Snprintf(cmdbuf,sizeof(cmdbuf),
              "global DrawFrameCount; incr DrawFrameCount");
  error_code=Tcl_Eval(interp,cmdbuf); // Increment DrawFrameCount variable
  if(error_code!=TCL_OK) return error_code;

/* Adjust subsample setting to actual value.
 * Note: Any tracings on plot_config triggered as a result
 *  of the Tcl_SetVar2 calls will send the name used in the
 *  call (here, e.g., "plot_config arrow_subsample") to the
 *  trace callback, and the call will appear to be from the
 *  proc that called this routine (DrawFrame).  If that routine
 *  has not made plot_config visible through a 'global plot_config'
 *  command, then a "upvar plot_config" will *fail* in the trace
 *  callback.  To protect against this, trace callbacks on plot_config
 *  should have the global name hardcoded into the trace call, and the
 *  callback itself should use the hardcoded value instead of the
 *  "name" argument.  For example:
 *
 *            trace variable plot_config(arrow_subsample) w \
 *                "MyCallback plot_config"
 *  and
 *        proc MyCallback { globalname name elt op } {
 *                upvar #0 $globalname x
 *                ...
 *        }
 *
 *  (The "elt" value can be either hardcoded into globalname, or
 *  processed separately.)
 */
  OC_REAL4m arrow_sample,pixel_sample;
  OC_REAL4m arrow_sample_request,pixel_sample_request;
  myFrame.GetRequestedSubsampleRates(arrow_sample_request,
                                     pixel_sample_request);
  myFrame.GetActualSubsampleRates(arrow_sample,pixel_sample);
  if(arrow_sample_request<0.) {
    arrow_sample*=-1;
    if(arrow_sample > OC_REAL4m(-0.01)) arrow_sample = OC_REAL4m(-0.01);
  }
  Oc_Snprintf(buf,sizeof(buf),"%g",static_cast<double>(arrow_sample));
  Tcl_SetVar2(interp,OC_CONST84_CHAR("plot_config"),
              OC_CONST84_CHAR("arrow,subsample"),
              buf,TCL_GLOBAL_ONLY);
  if(pixel_sample_request<0.) {
    pixel_sample*=-1;
    if(pixel_sample > OC_REAL4m(-0.01)) pixel_sample = OC_REAL4m(-0.01);
  }
  Oc_Snprintf(buf,sizeof(buf),"%g",static_cast<double>(pixel_sample));
  Tcl_SetVar2(interp,OC_CONST84_CHAR("plot_config"),
              OC_CONST84_CHAR("pixel,subsample"),
              buf,TCL_GLOBAL_ONLY);

  return TCL_OK;
}

int GetFrameRotation(ClientData,Tcl_Interp *interp,
                     int argc,CONST84 char** argv)
{
  Tcl_ResetResult(interp);
  if(argc!=2) {
    Oc_Snprintf(buf,sizeof(buf),"GetFrameRotation must be called with"
            " 1 argument: name of Tk's Frame Rotation variable"
            " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  int rotang=int(OC_ROUND(CoordsToAngle(myFrame.GetCoordinates())));

  Oc_Snprintf(buf,sizeof(buf),"%d",rotang);
  Tcl_SetVar(interp,argv[1],buf,0);

  return TCL_OK;
}

// Following routine added by SLW May-1998, modified by MJD June-1998.
// It copies the plot configuration values from the Tcl variable
// plot_config into the corresponding Frame plot_config structures.
// Also sets quantitylist value from Frame.
int UpdatePlotConfiguration(ClientData,Tcl_Interp *interp,
                            int,CONST84 char **)
{
  Vf_Mesh* myMesh = myMeshArray[activeMeshId];

  Tcl_ResetResult(interp);
  // Ignore argument list; just read plot_config() directly and update
  // all fields.
  const char *cptr; int colorcount;

  // Variables to handle "out-of-plane rotations", i.e., axis
  // transforms.
  Nb_DString flipstr("+x:+y:+z"),colorquantity;
  OC_REAL8m phase=0.;
  OC_BOOL invert=0;
  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                   OC_CONST84_CHAR("viewaxis"),
                   TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr) && strcmp(cptr,"+x:+y:+z")!=0) {
    char axisbuf[8];
    Oc_Snprintf(axisbuf,sizeof(axisbuf),"+z,%s",cptr);
    cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("view_transform"),axisbuf,
                       TCL_GLOBAL_ONLY);
    if(!Nb_StrIsSpace(cptr)) {
      flipstr = cptr;
    }
  }

  PlotConfiguration arrow_config,pixel_config;
  myFrame.GetPlotConfiguration(arrow_config,pixel_config);
  /// Initialize with old values

  // Arrow plot configuration
  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                   OC_CONST84_CHAR("arrow,status"),
                   TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr))
    arrow_config.displaystate=atoi(cptr);

  colorcount=-1;
  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                   OC_CONST84_CHAR("arrow,colorcount"),TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr)) colorcount=atoi(cptr);
  if(colorcount<0) colorcount=arrow_config.colormap.GetColorCount();
  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                         OC_CONST84_CHAR("arrow,colormap"),TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr) && arrow_config.displaystate) {
    arrow_config.colormap.Setup(colorcount,cptr);
  }

  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                   OC_CONST84_CHAR("arrow,quantity"),
                   TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr))
    colorquantity=cptr;

  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                   OC_CONST84_CHAR("arrow,colorphase"),
                   TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr)) phase=Nb_Atof(cptr);
  // else phase keeps default value of 0.0

  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                   OC_CONST84_CHAR("arrow,colorreverse"),
                   TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr)) invert=atoi(cptr);
  // else invert keeps default value of 0

  myMesh->ColorQuantityTransform(flipstr,colorquantity,phase,invert,
                                 arrow_config.colorquantity,
                                 arrow_config.phase,arrow_config.invert);

  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                   OC_CONST84_CHAR("arrow,autosample"),
                   TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr)) arrow_config.autosample=atoi(cptr);

  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                   OC_CONST84_CHAR("arrow,subsample"),
                   TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr))
    arrow_config.subsample=static_cast<OC_REAL4m>(Nb_Atof(cptr));

  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                   OC_CONST84_CHAR("arrow,size"),
                   TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr))
    arrow_config.size=static_cast<OC_REAL4m>(Nb_Atof(cptr));

  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                   OC_CONST84_CHAR("arrow,viewscale"),
                   TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr)) arrow_config.viewscale=atoi(cptr);

  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                   OC_CONST84_CHAR("arrow,antialias"),
                   TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr)) arrow_config.antialias=atoi(cptr);

  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                   OC_CONST84_CHAR("arrow,outlinewidth"),
                   TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr)) arrow_config.outlinewidth=Nb_Atof(cptr);
  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                   OC_CONST84_CHAR("arrow,outlinecolor"),
                   TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr)) arrow_config.outlinecolor.Set(cptr);

  // Pixel plot configuration
  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                   OC_CONST84_CHAR("pixel,status"),
                   TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr)) pixel_config.displaystate=atoi(cptr);

  colorcount=-1;
  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                   OC_CONST84_CHAR("pixel,colorcount"),TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr)) colorcount=atoi(cptr);
  if(colorcount<0) colorcount=pixel_config.colormap.GetColorCount();
  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                         OC_CONST84_CHAR("pixel,colormap"),TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr) && pixel_config.displaystate) {
    pixel_config.colormap.Setup(colorcount,cptr);
  }

  pixel_config.stipple="";
  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                   OC_CONST84_CHAR("pixel,opaque"),
                   TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr)) {
    int opaque=atoi(cptr);
    if(!opaque) pixel_config.stipple="gray25";
  }

  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                   OC_CONST84_CHAR("pixel,quantity"),
                   TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr)) colorquantity=cptr;

  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                   OC_CONST84_CHAR("pixel,colorphase"),
                   TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr)) phase=Nb_Atof(cptr);
  else                     phase=0.;

  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                   OC_CONST84_CHAR("pixel,colorreverse"),
                   TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr)) invert=atoi(cptr);
  else                     invert=0;

  myMesh->ColorQuantityTransform(flipstr,colorquantity,phase,invert,
                                 pixel_config.colorquantity,
                                 pixel_config.phase,pixel_config.invert);

  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                   OC_CONST84_CHAR("pixel,autosample"),
                   TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr)) pixel_config.autosample=atoi(cptr);

  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                   OC_CONST84_CHAR("pixel,subsample"),
                   TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr)) {
    pixel_config.subsample=static_cast<OC_REAL4m>(Nb_Atof(cptr));
  }

  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                   OC_CONST84_CHAR("pixel,size"),
                   TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr)) {
    pixel_config.size=static_cast<OC_REAL4m>(Nb_Atof(cptr));
  }

  // Misc.
  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                   OC_CONST84_CHAR("misc,drawboundary"),
                   TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr) && !atoi(cptr)) {
    myFrame.SetDrawBoundary(0);
  } else {
    myFrame.SetDrawBoundary(1);
  }

  OC_BOOL bw_error;
  OC_REAL8m boundary_width;
  cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                     OC_CONST84_CHAR("misc,boundarywidth"),TCL_GLOBAL_ONLY);
  boundary_width = Nb_Atof(cptr,bw_error);
  if(!bw_error) myFrame.SetBoundaryWidth(boundary_width);

  cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                     OC_CONST84_CHAR("misc,boundarycolor"),TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr)) myFrame.SetBoundaryColor(cptr);

  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                   OC_CONST84_CHAR("misc,background"),
                   TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr)) myFrame.SetBackgroundColor(cptr);

  // Get and set quantity type list
  Nb_DString* sp;
  Nb_List<Nb_DString> cqtypes;
  assert(myFrame.GetColorQuantityTypes(cqtypes)<INT_MAX);
  int cqtypec = static_cast<int>(myFrame.GetColorQuantityTypes(cqtypes));
  if (cqtypec>0) {
    char **cqtypev=new char*[cqtypec];
    OC_INDEX i;
    for(i=0, sp=cqtypes.GetFirst() ;
        sp!=NULL && i<cqtypec ; i++,sp=cqtypes.GetNext()) {
      cqtypev[i]=new char[sp->Length()+1];
      strcpy(cqtypev[i],sp->GetStr());
    }
    OC_INDEX itop=i;
    char *cqlist=Tcl_Merge(cqtypec,cqtypev);
    Tcl_SetVar2(interp,OC_CONST84_CHAR("plot_config"),
                OC_CONST84_CHAR("quantitylist"),
                cqlist,TCL_GLOBAL_ONLY);
    Tcl_Free(cqlist);
    for(i=0;i<itop;i++) delete[] cqtypev[i];
    delete[] cqtypev;
  } else {
    Tcl_SetVar2(interp,OC_CONST84_CHAR("plot_config"),
                OC_CONST84_CHAR("quantitylist"),OC_CONST84_CHAR(""),
                TCL_GLOBAL_ONLY);
  }

  // Try to make selected color quantities valid
  int acq_match=0,pcq_match=0;
  const char* dfcq=NULL;
  for(sp=cqtypes.GetFirst() ; sp!=NULL ; sp=cqtypes.GetNext()) {
    if(strcmp(arrow_config.colorquantity,sp->GetStr())==0) acq_match=1;
    if(strcmp(pixel_config.colorquantity,sp->GetStr())==0) pcq_match=1;
    if(dfcq==NULL) dfcq=sp->GetStr();
  }
  if(!acq_match && dfcq!=NULL) {
    arrow_config.colorquantity=dfcq;
    Tcl_SetVar2(interp,OC_CONST84_CHAR("plot_config"),
                OC_CONST84_CHAR("arrow,quantity"),
                OC_CONST84_CHAR(dfcq),TCL_GLOBAL_ONLY);
  }
  if(!pcq_match && dfcq!=NULL) {
    pixel_config.colorquantity=dfcq;
    Tcl_SetVar2(interp,OC_CONST84_CHAR("plot_config"),
                OC_CONST84_CHAR("pixel,quantity"),
                OC_CONST84_CHAR(dfcq),TCL_GLOBAL_ONLY);
  }

  // Apply new configuration
  myFrame.SetPlotConfiguration(arrow_config,pixel_config);
  return TCL_OK;
}

int SetFrameRotation(ClientData,Tcl_Interp *interp,
                     int argc,CONST84 char** argv)
{
  Tcl_ResetResult(interp);
  if(argc!=2) {
    Oc_Snprintf(buf,sizeof(buf),"SetFrameRotation must be called with"
            " 1 argument: new_rotation_angle"
            " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  myFrame.SetCoordinates(AngleToCoords(
                         static_cast<OC_REAL4m>(Nb_Atof(argv[1]))));

  return TCL_OK;
}

int GetFrameBox(ClientData,Tcl_Interp *interp,
                 int argc,CONST84 char**)
{ // NOTE: The return value from this routine is a *6*-tuple,
  //  "xmin ymin zmin xmax ymax zmax".
  Tcl_ResetResult(interp);
  if(argc!=1) {
    Oc_Snprintf(buf,sizeof(buf),
            "GetFrameBox should be called with no arguments."
            " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  Nb_BoundingBox<OC_REAL4> bb=myFrame.GetDisplayBox();
  Nb_Vec3<OC_REAL4> minpt,maxpt;
  bb.GetExtremes(minpt,maxpt);
  if(bb.IsEmpty()) maxpt=minpt;  // Empty boxes may have maxpt<minpt.

  Oc_Snprintf(buf,sizeof(buf),"%g %g %g %g %g %g",
              static_cast<double>(minpt.x),
              static_cast<double>(minpt.y),
              static_cast<double>(minpt.z),
              static_cast<double>(maxpt.x),
              static_cast<double>(maxpt.y),
              static_cast<double>(maxpt.z));
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}

int GetZoom(ClientData,Tcl_Interp *interp,int argc,CONST84 char** argv)
{
  Tcl_ResetResult(interp);
  if(argc!=2) {
    Oc_Snprintf(buf,sizeof(buf),"GetZoom must be called with"
            " 1 argument: name of zoom variable"
            " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  Oc_Snprintf(buf,sizeof(buf),"%.6g",
              static_cast<double>(myFrame.GetZoom()));
  Tcl_SetVar(interp,argv[1],buf,0);

  return TCL_OK;
}

int SetZoom(ClientData,Tcl_Interp *interp,
            int argc,CONST84 char** argv)
{
  Tcl_ResetResult(interp);
  if(argc<2 || argc>3) {
    Oc_Snprintf(buf,sizeof(buf),"SetZoom must be called with"
                " either 1 or 2 arguments: <new_zoom|new_width new_height>"
                " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  // Change mesh scale in display list
  OC_REAL4m newzoom;
  if(argc==2) newzoom=SetZoom(OC_REAL4m(Nb_Atof(argv[1])));
  else        newzoom=SetZoom(interp,
                              OC_REAL4m(Nb_Atof(argv[1])),
                              OC_REAL4m(Nb_Atof(argv[2])));

  Oc_Snprintf(buf,sizeof(buf),"%g",static_cast<double>(newzoom));
  Tcl_SetResult(interp,buf,TCL_VOLATILE);

  return TCL_OK;
}

int GetDefaultColorMapList(ClientData,Tcl_Interp *interp,
                     int argc,CONST84 char**)
{
  Tcl_ResetResult(interp);
  if(argc!=1) {
    Oc_Snprintf(buf,sizeof(buf),"GetDefaultColorMapList must be called with"
            " no arguments (%d arguments passed).",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  Nb_List<const char*> maps;
  DisplayColorMap::DefaultColorMapList(maps);
  OC_INDEX mapcount = maps.GetSize();
  if(mapcount>0) {
    char **mapv=new char*[mapcount];
    OC_INDEX i;
    const char** cpp;
    Nb_List_Index<const char*> key;
    for(i=0, cpp=maps.GetFirst(key);
        i<mapcount && cpp!=NULL ;
        i++, cpp=maps.GetNext(key)) {
      mapv[i] = new char[strlen(*cpp)+1];
      strcpy(mapv[i],*cpp);
    }
    assert(mapcount<=INT_MAX);
    char* maplist=Tcl_Merge(static_cast<int>(mapcount),mapv);
    Tcl_AppendResult(interp,maplist,(char *)NULL);
    Tcl_Free(maplist);
    while(i>0) delete[] mapv[--i];
    delete[] mapv;
  }
  return TCL_OK;
}

int GetAutosamplingRates(ClientData,Tcl_Interp *interp,
                         int argc,CONST84 char**)
{
  Tcl_ResetResult(interp);
  if(argc!=1) {
    Oc_Snprintf(buf,sizeof(buf),"GetAutosamplingRates must be called with"
            " no arguments (%d arguments passed).",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  OC_REAL4m arrow_rate
    = myFrame.GetAutoSampleRate(myFrame.GetPreferredArrowCellsize());
  OC_REAL4m pixel_rate
    = myFrame.GetAutoSampleRate(myFrame.GetPreferredPixelCellsize());
  Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g",
              static_cast<double>(arrow_rate),
              static_cast<double>(pixel_rate));
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}


int GetDataValueUnit(ClientData,Tcl_Interp *interp,
                     int argc,CONST84 char**)
{
  Tcl_ResetResult(interp);
  if(argc!=1) {
    Oc_Snprintf(buf,sizeof(buf),"GetDataValueUnit must be called with"
            " no arguments (%d arguments passed).",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  Oc_Snprintf(buf,sizeof(buf),"%.*s",MY_BUF_SIZE,myFrame.GetValueUnit());
  Tcl_AppendResult(interp,buf,(char *)NULL);

  return TCL_OK;
}

int GetDataValueScaling(ClientData,Tcl_Interp *interp,
                        int argc,CONST84 char**)
{
  Tcl_ResetResult(interp);
  if(argc!=1) {
    Oc_Snprintf(buf,sizeof(buf),
            "GetDataValueScaling must be called with"
            " no arguments (%d arguments passed).",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  Oc_Snprintf(buf,sizeof(buf),"%.6g",
              static_cast<double>(myFrame.GetValueScaling()));
  Tcl_AppendResult(interp,buf,(char *)NULL);

  return TCL_OK;
}


int SetDataValueScaling(ClientData,Tcl_Interp *interp,
                      int argc,CONST84 char** argv)
{
  // Adjust data value scaling
  Tcl_ResetResult(interp);
  if(argc!=2) {
    Oc_Snprintf(buf,sizeof(buf),
            "SetDataValueScaling must be called with"
            " 1 argument: <new_scale>"
            " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  OC_REAL8m newscale=myFrame.SetValueScaling(Nb_Atof(argv[1]));
  Oc_Snprintf(buf,sizeof(buf),"%g",static_cast<double>(newscale));
  Tcl_SetResult(interp,buf,TCL_VOLATILE);

  return TCL_OK;
}

int WriteMeshUsingDeprecatedVIOFormat(ClientData,Tcl_Interp *interp,
              int argc,CONST84 char** argv)
{
  Tcl_ResetResult(interp);
  if(argc != 2) {
    Oc_Snprintf(buf,sizeof(buf),
            "WriteMeshDeprecatedVIOFormat must be called with"
            " 1 argument: filename"
            " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  int errcode=TCL_OK;

  // Open file
  Tcl_Channel channel;
  Tcl_DString saveTranslation;
  Tcl_DStringInit(&saveTranslation);

  const OC_BOOL use_stdout = (argv[1][0] == '\0');
  if (use_stdout) {
    int mode;
    channel = Tcl_GetChannel(interp, OC_CONST84_CHAR("stdout"), &mode);
    Tcl_GetChannelOption(interp, channel, OC_CONST84_CHAR("-translation"),
                         &saveTranslation);
  } else {
    channel = Tcl_OpenFileChannel(interp, argv[1], OC_CONST84_CHAR("w"), 0666);
  }
  if (channel == NULL) return TCL_ERROR;

  Tcl_SetChannelOption(interp, channel, OC_CONST84_CHAR("-translation"),
                       OC_CONST84_CHAR("lf"));
  Vf_VioFileOutput vfo;
  if(vfo.WriteMesh(myMeshArray[activeMeshId],channel,NULL)!=0) {
      Oc_Snprintf(buf,sizeof(buf),"WriteMeshDeprecatedVIOFormat error");
      Tcl_AppendResult(interp,buf,(char *)NULL);
      errcode=TCL_ERROR;
  }

  if (use_stdout) {
    Tcl_Flush(channel);
    Tcl_SetChannelOption(interp, channel, OC_CONST84_CHAR("-translation"),
        Tcl_DStringValue(&saveTranslation));
  } else {
    Tcl_Close(NULL, channel);
  }

  Tcl_DStringFree(&saveTranslation);
  return errcode;
}

int WriteMesh(ClientData,Tcl_Interp *interp,
              int argc,CONST84 char** argv)
{
  Tcl_ResetResult(interp);
  if(argc<4 || argc>6) {
    Oc_Snprintf(buf,sizeof(buf),"WriteMesh must be called with"
            " 3-5 arguments: filename <text|binary4|binary8>"
            " <rectangular|irregular> [title] [description]"
            " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  // Special case handling
  if(dynamic_cast<Vf_EmptyMesh*>(myMeshArray[activeMeshId])) {
    Oc_AutoBuf errmsg = "No data to output.";
    Tcl_AppendResult(interp,errmsg.GetStr(),(char *)NULL);
    return TCL_ERROR;
  }

  int errcode=TCL_OK;

  Vf_OvfDataStyle ods=vf_oascii;
  if(strcmp("binary4",argv[2])==0)      ods=vf_obin4;
  else if(strcmp("binary8",argv[2])==0) ods=vf_obin8;

  OC_BOOL force_irreg=0;
  if(strcmp("irregular",argv[3])==0) force_irreg=1;

  const char *title="",*desc="";
  if(argc>4) title=argv[4];
  if(argc>5) desc=argv[5];

  // Open file
  Tcl_Channel channel;
  const char* filename = NULL;
  Tcl_DString saveTranslation;
  Tcl_DStringInit(&saveTranslation);

  const OC_BOOL use_stdout = (argv[1][0] == '\0');
  if (use_stdout) {
    int mode;
    channel = Tcl_GetChannel(interp, OC_CONST84_CHAR("stdout"), &mode);
    Tcl_GetChannelOption(interp, channel, OC_CONST84_CHAR("-translation"),
                         &saveTranslation);
    filename = "stdout";
  } else {
    channel = Tcl_OpenFileChannel(interp, argv[1], OC_CONST84_CHAR("w"), 0666);
    filename = argv[1];
  }
  if (channel == NULL) return TCL_ERROR;

  Tcl_SetChannelOption(interp, channel, OC_CONST84_CHAR("-translation"),
                       OC_CONST84_CHAR("lf"));

  Vf_OvfFileOutput ofo;
  OC_INT4m writecheck = 0;
  try {
    writecheck = ofo.WriteMesh(myMeshArray[activeMeshId],
                               channel,ods,force_irreg,title,desc);
  } catch(...) {
    writecheck = 1;
  }
  if(writecheck != 0) {
    Oc_AutoBuf errmsg;
    if(writecheck == -1) {
      const Vf_Mesh* mesh = myMeshArray[activeMeshId];
      errmsg.SetLength(sizeof(filename)+sizeof(mesh->GetMeshType())+256);
      Oc_Snprintf(errmsg.GetStr(),errmsg.GetLength(),
                  "WriteMesh error writing to \"%s\";"
                  " output not supported for mesh type %s",
                  filename,mesh->GetMeshType());
    } else {
      errmsg.SetLength(sizeof(filename)+256);
      Oc_Snprintf(errmsg.GetStr(),errmsg.GetLength(),
                  "WriteMesh error writing to \"%s\"; device full?",
                  filename);
    }
    Tcl_AppendResult(interp,errmsg.GetStr(),(char *)NULL);
    errcode=TCL_ERROR;
  }
  if (use_stdout) {
    Tcl_Flush(channel);
    Tcl_SetChannelOption(interp, channel, OC_CONST84_CHAR("-translation"),
        Tcl_DStringValue(&saveTranslation));
  } else {
    Tcl_Close(NULL, channel);
  }

  Tcl_DStringFree(&saveTranslation);
  return errcode;
}

int WriteMeshOVF2
(ClientData,Tcl_Interp *interp,
 int argc,CONST84 char** argv)
{
  Tcl_ResetResult(interp);
  if(argc<4 || argc>6) {
    Oc_Snprintf(buf,sizeof(buf),"WriteMeshOVF2 must be called with"
            " 3-5 arguments: filename <text|binary4|binary8>"
            " <rectangular|irregular> [title] [description]"
            " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  int errcode=TCL_OK;

  const char *text_fmt = "%# .17g"; // Default format for data in text mode
  Vf_OvfDataStyle ods=vf_oascii;
  if(strcmp("binary4",argv[2])==0)      ods=vf_obin4;
  else if(strcmp("binary8",argv[2])==0) ods=vf_obin8;
  else if(strncmp("text",argv[2],4)==0 && argv[2][4]!='\0') {
    // User specified text data format
    size_t skip = strspn(argv[2]+4," \t\r\n"); // Skip whitespace
    text_fmt = argv[2]+4+skip;
  }

  Vf_Ovf20_MeshType reqtype = vf_ovf20mesh_rectangular;
  if(strcmp("irregular",argv[3])==0) {
    reqtype = vf_ovf20mesh_irregular;
  }

  Vf_Mesh* mesh = myMeshArray[activeMeshId];
  Vf_Ovf20FileHeader header;

  Vf_Mesh_MeshNodes meshnodes(mesh);
  meshnodes.DumpGeometry(header,reqtype);

  // Additional details
  if(argc>4) header.title.Set(String(argv[4]));
  if(argc>5) header.desc.Set(String(argv[5]));

  vector<String> valueunits;   // Value units, such as "J/m^3"
  vector<String> valuelabels;  // Value label, such as "Exchange energy density"
  valueunits.push_back(String(mesh->GetValueUnit()));
  valueunits.push_back(String(mesh->GetValueUnit()));
  valueunits.push_back(String(mesh->GetValueUnit()));
  valuelabels.push_back(String("")); // Empty placeholder for now.
  valuelabels.push_back(String(""));
  valuelabels.push_back(String(""));
  header.valuedim.Set(3); // Vector field
  header.valuelabels.Set(valuelabels);
  header.valueunits.Set(valueunits);

  if(!header.IsValidGeom()) {
    OC_THROW("Programming error?"
             " Invalid file header constructed in WriteMeshOVF2");
  }

  // Copy (scaled) vector components into vanilla OC_REAL8m array
  const OC_INDEX size = meshnodes.GetSize();
  Nb_ArrayWrapper<OC_REAL8m> vecvals(3*size);
  for(OC_INDEX i=0; i<size ; ++i) {
    const Nb_Vec3<OC_REAL8m>& nbvec = meshnodes.GetValue(i);
    vecvals[3*i]   = nbvec.x;
    vecvals[3*i+1] = nbvec.y;
    vecvals[3*i+2] = nbvec.z;
  }

  // Open file
  Tcl_Channel channel;
  Tcl_DString saveTranslation;
  const char* filename = NULL;
  if (argv[1][0] == '\0') {
    int mode;
    channel = Tcl_GetChannel(interp, OC_CONST84_CHAR("stdout"), &mode);
    Tcl_DStringInit(&saveTranslation);
    Tcl_GetChannelOption(interp, channel, OC_CONST84_CHAR("-translation"),
                         &saveTranslation);
    filename = "stdout";
  } else {
    channel = Tcl_OpenFileChannel(interp, argv[1], OC_CONST84_CHAR("w"), 0666);
    filename = argv[1];
  }
  if (channel == NULL) {
    return TCL_ERROR;
  }
  Tcl_SetChannelOption(interp, channel, OC_CONST84_CHAR("-translation"),
                       OC_CONST84_CHAR("lf"));


  // Write
  OC_INT4m writecheck = 0;
  try {
    Vf_Ovf20VecArrayConst data_info(3,size,vecvals.GetPtr());
    header.WriteHeader(channel);
    header.WriteData(channel, ods,text_fmt,
                     &meshnodes,data_info);
  } catch(...) {
    writecheck = 1;
  }
  if(writecheck != 0) {
    Oc_AutoBuf errmsg;
    errmsg.SetLength(sizeof(filename)+256);
    Oc_Snprintf(errmsg.GetStr(),errmsg.GetLength(),
                "WriteMeshOVF2 error writing to \"%s\"; device full?",
                filename);
    Tcl_AppendResult(interp,errmsg.GetStr(),(char *)NULL);
    errcode=TCL_ERROR;
  }

  if (argv[1][0] != '\0') {
    Tcl_Close(NULL, channel);
  } else {
    Tcl_Flush(channel);
    Tcl_SetChannelOption(interp, channel, OC_CONST84_CHAR("-translation"),
        Tcl_DStringValue(&saveTranslation));
    Tcl_DStringFree(&saveTranslation);
  }

  return errcode;
}

// Code for writing Python NumPY NPY version 1.0 files.
int WriteMeshNPY
(ClientData,Tcl_Interp *interp,
 int argc,CONST84 char** argv)
{
  Tcl_ResetResult(interp);
  if(argc<4 || argc>6) {
    Oc_Snprintf(buf,sizeof(buf),"WriteMeshNPY must be called with"
            " 3-5 arguments: filename <text|binary4|binary8>"
            " <rectangular|irregular> [textwidth] [textfmt]"
            " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  int errcode=TCL_OK;

  Vf_OvfDataStyle ods=vf_oinvalid;
  if(strcmp("text",argv[2])==0)         ods=vf_oascii;
  else if(strcmp("binary4",argv[2])==0) ods=vf_obin4;
  else if(strcmp("binary8",argv[2])==0) ods=vf_obin8;
  else {
    Oc_AutoBuf errmsg;
    errmsg.SetLength(strlen(argv[2])+256);
    Oc_Snprintf(errmsg.GetStr(),errmsg.GetLength(),
       "WriteMeshNPY error: Unrecognized output format request: \"%s\"",
       argv[2]);
    Tcl_AppendResult(interp,errmsg.GetStr(),(char *)NULL);
    return TCL_ERROR;
  }

  Vf_Ovf20_MeshType reqtype = vf_ovf20mesh_rectangular;
  if(strcmp("irregular",argv[3])==0) {
    reqtype = vf_ovf20mesh_irregular;
  }

  int textwidth = 0;
  if(argc>4) textwidth = atoi(argv[4]);

  const char* textfmt = nullptr;
  if(argc>5) textfmt = argv[5];;

  Vf_Mesh* mesh = myMeshArray[activeMeshId];
  Vf_Ovf20FileHeader header;

  Vf_Mesh_MeshNodes meshnodes(mesh);
  meshnodes.DumpGeometry(header,reqtype);

  vector<String> valueunits;   // Value units, such as "J/m^3"
  vector<String> valuelabels;  // Value label, such as "Exchange energy density"
  valueunits.push_back(String(mesh->GetValueUnit()));
  valueunits.push_back(String(mesh->GetValueUnit()));
  valueunits.push_back(String(mesh->GetValueUnit()));
  valuelabels.push_back(String("")); // Empty placeholder for now.
  valuelabels.push_back(String(""));
  valuelabels.push_back(String(""));
  header.valuedim.Set(3); // Vector field
  header.valuelabels.Set(valuelabels);
  header.valueunits.Set(valueunits);

  if(!header.IsValidGeom()) {
    OC_THROW("Programming error?"
             " Invalid file header constructed in WriteMeshNPY");
  }

  // Copy (scaled) vector components into vanilla OC_REAL8m array
  const OC_INDEX size = meshnodes.GetSize();
  Nb_ArrayWrapper<OC_REAL8m> vecvals(3*size);
  for(OC_INDEX i=0; i<size ; ++i) {
    const Nb_Vec3<OC_REAL8m>& nbvec = meshnodes.GetValue(i);
    vecvals[3*i]   = nbvec.x;
    vecvals[3*i+1] = nbvec.y;
    vecvals[3*i+2] = nbvec.z;
  }

  // Open file
  Tcl_Channel channel;
  Tcl_DString saveTranslation;
  const char* filename = NULL;
  if (argv[1][0] == '\0') {
    int mode;
    channel = Tcl_GetChannel(interp, OC_CONST84_CHAR("stdout"), &mode);
    Tcl_DStringInit(&saveTranslation);
    Tcl_GetChannelOption(interp, channel, OC_CONST84_CHAR("-translation"),
                         &saveTranslation);
    filename = "stdout";
  } else {
    channel = Tcl_OpenFileChannel(interp, argv[1], OC_CONST84_CHAR("w"), 0666);
    filename = argv[1];
  }
  if (channel == NULL) {
    return TCL_ERROR;
  }
  Tcl_SetChannelOption(interp, channel, OC_CONST84_CHAR("-translation"),
                       OC_CONST84_CHAR("lf"));


  // Write
  OC_INT4m writecheck = 0;
  try {
    Vf_Ovf20VecArrayConst data_info(3,size,vecvals.GetPtr());
    if(argc==4) {
      header.WriteNPY(channel,ods,data_info,&meshnodes);
    } else if(argc==5) {
      header.WriteNPY(channel,ods,data_info,&meshnodes,textwidth);
    } else {
      header.WriteNPY(channel,ods,data_info,&meshnodes,textwidth,textfmt);
    }
  } catch(...) {
    writecheck = 1;
  }
  if(writecheck != 0) {
    Oc_AutoBuf errmsg;
    errmsg.SetLength(sizeof(filename)+256);
    Oc_Snprintf(errmsg.GetStr(),errmsg.GetLength(),
                "WriteMeshNPY error writing to \"%s\"; device full?",
                filename);
    Tcl_AppendResult(interp,errmsg.GetStr(),(char *)NULL);
    errcode=TCL_ERROR;
  }

  if (argv[1][0] != '\0') {
    Tcl_Close(NULL, channel);
  } else {
    Tcl_Flush(channel);
    Tcl_SetChannelOption(interp, channel, OC_CONST84_CHAR("-translation"),
        Tcl_DStringValue(&saveTranslation));
    Tcl_DStringFree(&saveTranslation);
  }

  return errcode;
}

int WriteMeshMagnitudes
(ClientData,Tcl_Interp *interp,
 int argc,CONST84 char** argv)
{
  Tcl_ResetResult(interp);
  if(argc<4 || argc>6) {
    Oc_Snprintf(buf,sizeof(buf),"WriteMeshMagnitudes must be called with"
            " 3-5 arguments: filename <text|binary4|binary8>"
            " <rectangular|irregular> [title] [description]"
            " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  int errcode=TCL_OK;

  Vf_OvfDataStyle ods=vf_oascii;
  if(strcmp("binary4",argv[2])==0)      ods=vf_obin4;
  else if(strcmp("binary8",argv[2])==0) ods=vf_obin8;

  Vf_Ovf20_MeshType reqtype = vf_ovf20mesh_rectangular;
  if(strcmp("irregular",argv[3])==0) {
    reqtype = vf_ovf20mesh_irregular;
  }

  Vf_Mesh* mesh = myMeshArray[activeMeshId];
  Vf_Ovf20FileHeader header;

  Vf_Mesh_MeshNodes meshnodes(mesh);
  meshnodes.DumpGeometry(header,reqtype);

  // Additional details
  if(argc>4) header.title.Set(String(argv[4]));
  if(argc>5) header.desc.Set(String(argv[5]));

  vector<String> valueunits;   // Value units, such as "J/m^3"
  vector<String> valuelabels;  // Value label, such as "Exchange energy density"
  valueunits.push_back(String(mesh->GetValueUnit()));
  valuelabels.push_back(String("")); // Empty placeholder for now.
  header.valuedim.Set(1); // Scalar field
  header.valuelabels.Set(valuelabels);
  header.valueunits.Set(valueunits);

  if(!header.IsValidGeom()) {
    OC_THROW("Programming error?"
             " Invalid file header constructed in WriteMeshMagnitude");
  }

  // Compute magnitudes.
  const OC_INDEX size = meshnodes.GetSize();
  Nb_ArrayWrapper<OC_REAL8m> mag(size);
  for(OC_INDEX i=0; i<size ; ++i) {
    mag[i] = meshnodes.GetValue(i).Mag();
  }

  // Open file
  Tcl_Channel channel;
  Tcl_DString saveTranslation;
  const char* filename = NULL;
  if (argv[1][0] == '\0') {
    int mode;
    channel = Tcl_GetChannel(interp, OC_CONST84_CHAR("stdout"), &mode);
    Tcl_DStringInit(&saveTranslation);
    Tcl_GetChannelOption(interp, channel, OC_CONST84_CHAR("-translation"),
                         &saveTranslation);
    filename = "stdout";
  } else {
    channel = Tcl_OpenFileChannel(interp, argv[1], OC_CONST84_CHAR("w"), 0666);
    filename = argv[1];
  }
  if (channel == NULL) {
    return TCL_ERROR;
  }
  Tcl_SetChannelOption(interp, channel, OC_CONST84_CHAR("-translation"),
                       OC_CONST84_CHAR("lf"));


  // Write
  OC_INT4m writecheck = 0;
  try {
    Vf_Ovf20VecArrayConst data_info(1,size,mag.GetPtr());
    header.WriteHeader(channel);
    header.WriteData(channel, ods,
                     "%# .17g",  // Might want to allow user to set this
                     &meshnodes,data_info);
  } catch(...) {
    writecheck = 1;
  }
  if(writecheck != 0) {
    Oc_AutoBuf errmsg;
    errmsg.SetLength(sizeof(filename)+256);
    Oc_Snprintf(errmsg.GetStr(),errmsg.GetLength(),
                "WriteMeshMagnitudes error writing to \"%s\"; device full?",
                filename);
    Tcl_AppendResult(interp,errmsg.GetStr(),(char *)NULL);
    errcode=TCL_ERROR;
  }

  if (argv[1][0] != '\0') {
    Tcl_Close(NULL, channel);
  } else {
    Tcl_Flush(channel);
    Tcl_SetChannelOption(interp, channel, OC_CONST84_CHAR("-translation"),
        Tcl_DStringValue(&saveTranslation));
    Tcl_DStringFree(&saveTranslation);
  }

  return errcode;
}


////////////////////////////////////////////////////////////////////////
// Support classes for WriteMeshAverages ///////////////////////////////

class TripleIndex {
public:
  // The Step*() member functions increment i, j, and k as
  // appropriate for a single forward step.  The return
  // value is 0 when the step stays inside the inner
  // loop range, 1 when stepping outside the inner
  // loop to the middle loop, 2 when stepping outside
  // the middle loop to the outer loop, and 3 when all
  // iterates are complete.
  //   The existing member functions are designed with Fortran
  // loop order, i.e., the natural (storage) order is i
  // increments fastest, then j, and finally k.  Different
  // members step through the values differently, however
  // --- this is the raison d'etre of the TripleIndex
  // class.  If one wants the usual C ordering, then either
  // introduce additional members, or just interchange i and k.
  //   The Reset*() member functions set i, j, and k to
  // just before the first in-range value.  This means that
  // the Step*() functions should be called before using
  // i, j, and k.
  //   The loop parameters i, j, k are initialized to be
  // just in front of the first in-range value.  This means
  // that Step* should be called before using the x, y, and
  // z values.  The Reset*() members can be used to reset
  // i, j, and k to their initialized values.
  //   Example usage for volume or point averaging:
  //
  //      foo.ResetXYZ();
  //      const int& i = foo.iref();
  //      const int& j = foo.jref();
  //      const int& k = foo.kref();
  //      while(StepXYZ()<3) {
  //           do_stuff(i,j,k);
  //           write_stuff // If point averaging
  //      }
  //      write_stuff // If volume averaging
  //
  // For x-line averaging:
  //
  //      foo.ResetXYZ();
  //      const int& i = foo.iref();
  //      const int& j = foo.jref();
  //      const int& k = foo.kref();
  //      int stepcontrol = foo.StepXYZ();
  //      while(stepcontrol<3) {
  //         do {
  //            do_stuff(i,j,k);
  //         } while((stepcontrol=foo.StepXYZ())==0);
  //         write_stuff;
  //      }
  //
  // For z-plane averaging:
  //
  //      foo.ResetXYZ();
  //      const int& i = foo.iref();
  //      const int& j = foo.jref();
  //      const int& k = foo.kref();
  //      int stepcontrol = foo.StepXYZ();
  //      while(stepcontrol<3) {
  //         do {
  //            do_stuff(i,j,k);
  //         } while((stepcontrol=foo.StepXYZ())<2);
  //         write_stuff;
  //      }
  //
  // In order to do ?-plane or ?-line averaging, one can
  // setup a member function pointer to the appropriate
  // Step???() member function.
  //
  TripleIndex(OC_INDEX set_imin,OC_INDEX set_imax,OC_INDEX set_isize,
              OC_INDEX set_jmin,OC_INDEX set_jmax,OC_INDEX set_jsize,
              OC_INDEX set_kmin,OC_INDEX set_kmax,OC_INDEX set_ksize);
  ~TripleIndex() {}

  // Preferred access order.  Use for volume and pointwise
  // averaging, also for x-lines or z-planes
  void ResetXYZ(); // Increments i first, then j, and last k
  int StepXYZ();
  int LineStepXYZ();
  int PlaneStepXYZ();

  // Use for y-lines
  void ResetYXZ(); // Increments j first, then i, and last k
  int StepYXZ();
  int LineStepYXZ();

  // Use for z-lines
  void ResetZXY(); // Increments k first, then i, and last j
  int StepZXY();
  int LineStepZXY();

  // Use for x-planes
  void ResetYZX(); // Increments j first, then k, and last i
  int StepYZX();
  int PlaneStepYZX();

  // Use for y-planes
  void ResetXZY(); // Increments i first, then k, and last j
  int StepXZY();
  int PlaneStepXZY();

  // The reference member functions return references to the
  // class index variables.  These values may change across
  // calls to Step and Reset.
  const OC_INDEX& iref() const { return i; }
  const OC_INDEX& jref() const { return j; }
  const OC_INDEX& kref() const { return k; }

private:
  OC_INDEX i,j,k;
  OC_INDEX imin,imax,isize;
  OC_INDEX jmin,jmax,jsize;
  OC_INDEX kmin,kmax,ksize;
};

TripleIndex::TripleIndex
(OC_INDEX set_imin,OC_INDEX set_imax,OC_INDEX set_isize,
 OC_INDEX set_jmin,OC_INDEX set_jmax,OC_INDEX set_jsize,
 OC_INDEX set_kmin,OC_INDEX set_kmax,OC_INDEX set_ksize) :
  i(-1),j(-1),k(-1),
  imin(set_imin),imax(set_imax),isize(set_isize),
  jmin(set_jmin),jmax(set_jmax),jsize(set_jsize),
  kmin(set_kmin),kmax(set_kmax),ksize(set_ksize)
{
  if(imin<0 || imin>imax || imax>isize) {
    OC_THROW("Error in TripleIndex initializer: Bad i value(s)");
  }
  if(jmin<0 || jmin>jmax || jmax>jsize) {
    OC_THROW("Error in TripleIndex initializer: Bad j value(s)");
  }
  if(kmin<0 || kmin>kmax || kmax>ksize) {
    OC_THROW("Error in TripleIndex initializer: Bad k value(s)");
  }
}
void TripleIndex::ResetXYZ()
{
  i=imin-1; j=jmin; k=kmin  ;
}

int TripleIndex::StepXYZ()
{
  if((++i)<imax) return 0;
  i=imin;  if((++j)<jmax) return 1;
  j=jmin;  if((++k)<kmax) return 2;
  return 3;
}

int TripleIndex::LineStepXYZ()
{
  if((++j)<jmax) return 1;
  j=jmin;  if((++k)<kmax) return 2;
  return 3;
}

int TripleIndex::PlaneStepXYZ()
{
  if((++k)<kmax) return 2;
  return 3;
}

void TripleIndex::ResetYXZ()
{
  i=imin; j=jmin-1; k=kmin;
}

int TripleIndex::StepYXZ()
{
  if((++j)<jmax) return 0;
  j=jmin;  if((++i)<imax) return 1;
  i=imin;  if((++k)<kmax) return 2;
  return 3;
}

int TripleIndex::LineStepYXZ()
{
  if((++i)<imax) return 1;
  i=imin;  if((++k)<kmax) return 2;
  return 3;
}

void TripleIndex::ResetZXY()
{
  i=imin; j=jmin; k=kmin-1;
}

int TripleIndex::StepZXY()
{
  if((++k)<kmax) return 0;
  k=kmin;  if((++i)<imax) return 1;
  i=imin;  if((++j)<jmax) return 2;
  return 3;
}

int TripleIndex::LineStepZXY()
{
  if((++i)<imax) return 1;
  i=imin;  if((++j)<jmax) return 2;
  return 3;
}

void TripleIndex::ResetYZX()
{
  i=imin; j=jmin-1; k=kmin;
}

int TripleIndex::StepYZX()
{
  if((++j)<jmax) return 0;
  j=jmin;  if((++k)<kmax) return 1;
  k=kmin;  if((++i)<imax) return 2;
  return 3;
}

int TripleIndex::PlaneStepYZX()
{
  if((++i)<imax) return 2;
  return 3;
}

void TripleIndex::ResetXZY()
{
  i=imin-1; j=jmin; k=kmin;
}

int TripleIndex::StepXZY()
{
  if((++i)<imax) return 0;
  i=imin;  if((++k)<kmax) return 1;
  k=kmin;  if((++j)<jmax) return 2;
  return 3;
}

int TripleIndex::PlaneStepXZY()
{
  if((++j)<jmax) return 2;
  return 3;
}

class LinearTripleIndex {
  // This class is conceptually similiar to the TripleIndex
  // class, except here the output is a single index, "offset"
  // into a linear representation of a 3D array.  The array
  // ordering is interpreted as FORTRAN ordering, i.e., the
  // natural access pattern is to increment i first, then j,
  // and last k.
  //  Example usage for volume or ptwise averaging:
  //
  //      foo.SetStepXYZ();
  //      const OC_INDEX& offset = foo.offsetref();
  //      while(Step()<3) {
  //           do_stuff(offset);
  //           write_stuff // If point averaging
  //      }
  //      write_stuff // If volume averaging
  //
  // For x-line averaging:
  //
  //      foo.SetStepXYZ();
  //      const OC_INDEX& offset = foo.offsetref();
  //      int stepcontrol = foo.Step();
  //      while(stepcontrol<3) {
  //         do {
  //            do_stuff(offset);
  //         } while((stepcontrol=foo.Step())==0);
  //         write_stuff;
  //      }
  //
  // For z-plane averaging:
  //
  //      foo.SetStepXYZ();
  //      const OC_INDEX& offset = foo.offsetref();
  //      int stepcontrol = foo.Step();
  //      while(stepcontrol<3) {
  //         do {
  //            do_stuff(offset);
  //         } while((stepcontrol=foo.Step())<2);
  //         write_stuff;
  //      }
  //
public:
  LinearTripleIndex(OC_INDEX set_imin,OC_INDEX set_imax,OC_INDEX set_isize,
                    OC_INDEX set_jmin,OC_INDEX set_jmax,OC_INDEX set_jsize,
                    OC_INDEX set_kmin,OC_INDEX set_kmax);
  ~LinearTripleIndex() {}
  void SetStepXYZ();
  void SetStepXZY();
  void SetStepYZX();
  void SetStepYXZ();
  void SetStepZXY();
  void Reset();
  OC_INT4m Step();
  void SaveIndex() { a_save=a; b_save=b; c_save=c; }
  void RestoreIndex();
  const OC_INDEX& offsetref() const { return offset; }
private:
  OC_INDEX imin,imax,isize;
  OC_INDEX jmin,jmax,jsize;
  OC_INDEX kmin,kmax;
  OC_INDEX offset;
  OC_INDEX a,b,c;
  OC_INDEX a_save,b_save,c_save;
  OC_INDEX amin,amax,astep;
  OC_INDEX bmin,bmax,bstep;
  OC_INDEX cmin,cmax,cstep;
};

LinearTripleIndex::LinearTripleIndex
(OC_INDEX set_imin,OC_INDEX set_imax,OC_INDEX set_isize,
 OC_INDEX set_jmin,OC_INDEX set_jmax,OC_INDEX set_jsize,
 OC_INDEX set_kmin,OC_INDEX set_kmax) :
  imin(set_imin),imax(set_imax),isize(set_isize),
  jmin(set_jmin),jmax(set_jmax),jsize(set_jsize),
  kmin(set_kmin),kmax(set_kmax),
  offset(0),a(-1),b(-1),c(-1),a_save(-1),b_save(-1),c_save(-1),
  amin(-1),amax(-1),astep(-1),
  bmin(-1),bmax(-1),bstep(-1),
  cmin(-1),cmax(-1),cstep(-1)
{
  if(imin<0 || imin>imax || imax>isize) {
    OC_THROW("Error in LinearTripleIndex initializer: Bad i value(s)");
  }
  if(jmin<0 || jmin>jmax || jmax>jsize) {
    OC_THROW("Error in LinearTripleIndex initializer: Bad j value(s)");
  }
  if(kmin<0 || kmin>kmax) {
    OC_THROW("Error in LinearTripleIndex initializer: Bad k value(s)");
  }
}

void LinearTripleIndex::Reset()
{
  a = amin - 1;
  b = bmin;
  c = cmin;
  offset = a*astep + b*bstep + c*cstep;
  // offset is unsigned, so if amin=bmin=cmin=0, the
  // initial value for offset will be 0 - astep.  This
  // should be okay, as long as (0 - astep) + astep = 0,
  // since the latter should be the first value actually used.
}

void LinearTripleIndex::RestoreIndex()
{
  a=a_save; b=b_save; c=c_save;
  offset = a*astep + b*bstep + c*cstep;
}

void LinearTripleIndex::SetStepXYZ()
{
  amin=imin; amax=imax;  astep=1;
  bmin=jmin; bmax=jmax;  bstep=isize;
  cmin=kmin; cmax=kmax;  cstep=isize*jsize;
  Reset();
}

void LinearTripleIndex::SetStepXZY()
{
  amin=imin; amax=imax;  astep=1;
  bmin=kmin; bmax=kmax;  bstep=isize*jsize;
  cmin=jmin; cmax=jmax;  cstep=isize;
  Reset();
}

void LinearTripleIndex::SetStepYZX()
{
  amin=jmin; amax=jmax;  astep=isize;
  bmin=kmin; bmax=kmax;  bstep=isize*jsize;
  cmin=imin; cmax=imax;  cstep=1;
  Reset();
}

void LinearTripleIndex::SetStepYXZ()
{
  amin=jmin; amax=jmax;  astep=isize;
  bmin=imin; bmax=imax;  bstep=1;
  cmin=kmin; cmax=kmax;  cstep=isize*jsize;
  Reset();
}

void LinearTripleIndex::SetStepZXY()
{
  amin=kmin; amax=kmax;  astep=isize*jsize;
  bmin=imin; bmax=imax;  bstep=1;
  cmin=jmin; cmax=jmax;  cstep=isize;
  Reset();
}

OC_INT4m LinearTripleIndex::Step()
{
  if(++a<amax) {
    offset += astep;
    return 0;
  }
  a = amin;
  offset -= (amax-1-amin)*astep;
  if(++b<bmax) {
   offset += bstep;
   return 1;
  }
  b = bmin;
  offset -= (bmax-1-bmin)*bstep;
  if(++c<cmax) {
    offset += cstep;
    return 2;
  }
  return 3;
}

// Utility functions used by WriteMeshAverages
char* CenterStringOutput(char* bufptr,size_t bufsize,
                         size_t field_width,const char* str)
{
  const size_t strsize = strlen(str);
  if(strsize>field_width) {
    bufptr += Oc_Snprintf(bufptr,bufsize,"%s",str);
  } else {
    const size_t lmargin = (field_width - strsize)/2;
    const size_t rmargin = field_width - lmargin - strsize;
    bufptr += Oc_Snprintf(bufptr,bufsize,"%*s",lmargin+strsize,str);
    if(rmargin>0) {
      bufptr += Oc_Snprintf(bufptr,bufsize,"%*s",rmargin,"");
    }
  }
  return bufptr;
}

char* WriteCenteredLabels
(char * inbuf,
 size_t bufsize,
 size_t field_width,
 Nb_ArrayWrapper<Nb_DString>& labels)
{
  OC_INDEX i;
  char* bufptr = inbuf;
  for(i=0;i<labels.GetSize();i++) {
    bufptr = CenterStringOutput(bufptr,bufsize-(bufptr-inbuf)-1,
                                field_width,labels[i].GetStr());
    if(i+1<labels.GetSize()) *(bufptr++) = ' ';
  }
  return bufptr;
}

// Tcl_Write to stdout broken in Tcl 8.5 and 8.6.  Use Obj interface
// instead.  (Tcl_ObjPrintf first appears in Tcl 8.5.)
#if TCL_MAJOR_VERSION<8 || (TCL_MAJOR_VERSION==8 && TCL_MINOR_VERSION<5)
# define WMA_USE_OBJ 0
#else
# define WMA_USE_OBJ 1
#endif

// #define WMA_DEFAULT_NUM_FMT "%- #24.17g"
#define WMA_DEFAULT_NUM_FMT "%- #20.15g"

int
WriteMeshAverages(ClientData, Tcl_Interp *interp,
                    int argc, CONST84 char** argv)
{
  Tcl_Channel channel;
  int mode;
  const char* cptr;
#if WMA_USE_OBJ
  Tcl_Obj* tmpobj;
#endif
  // Import config array:
  //   average  -- one of (space|plane|line|point|ball) (required)
  //   axis     -- one of (x|y|z)                       (required)
  //   ball_radius -- floating point value denoting radius
  //        of averaging ball, in problem coordinates.  Required
  //        iff average is "ball".
  //   range    -- 6-tuple xmin ymin zmin xmax ymax zmax (prob. coords.)
  //   rrange   -- 6-tuple, xmin ymin zmin xmax ymax zmax,
  //                each in range [0,1] (relative coordinates)
  //   normalize -- 1 or 0.  If 1, then each output point is
  //        divided by the maximum magnitude that would occur
  //        if all vectors in the manifold are aligned.  Thus
  //        the magnitude of the output vectors are all <=1.
  //   header   -- one of (fullhead|shorthead|nohead) (required)
  //   trailer  -- one of (tail|notail)               (required)
  //   numfmt   -- Format for numeric output default is WMA_DEFAULT_NUM_FMT
  //   descript -- description string
  //   index    -- list of triplets; each triplet is: label units value
  //   vallab   -- Value label.  Default is "M".
  //   valfuncs -- List of triplets.  Each triplet is label unit
  //        expr-expression where "label" and "unit" are headers for a
  //        column in the output data table.  "expr-expression" is a
  //        Tcl expr expression which is applied point-by-point on the
  //        input file before any averaging is done.  Available
  //        variables are x, y, z, r, vx, vy, vz, vmag.  A couple of
  //        examples are
  //                    Ms A/m $vmag
  //                    M110 A/m {($vx+$vy)*0.70710678}
  //   defaultvals -- 1 or 0.  If 1, the vx, vy, and vz are included
  //        automatically in output table.  If 0, then only columns
  //        specified by the "functions" option are output.
  //
  //   defaultpos  -- 1 or 0.  If 1, the x, y, and/or z point
  //        coordinate values (as appropriate for averaging type)
  //        are automatically included in output table.  If 0, then
  //        these values are not automatically output, but may be
  //        included as desired through the valfuncs functionality.
  //
  //   extravals -- 1 or 0.  If 1, then columns for L1, L2 norms
  //        and min and max absolute component values are included.
  //        L1 column is (\sum_i |vx|+|vy|+|vz|)/point_count,
  //        L2 column is sqrt((\sum_i v*v)/point_count).
  //
  // The active volume is set from range, if set.  Otherwise,
  // rrange is used.  If neither is set, the default is the
  // entire mesh volume.

  Tcl_ResetResult(interp);
  if(argc!=3) {
    Oc_Snprintf(buf,sizeof(buf),
            "WriteMeshAverages must be called 2 arguments:"
            " output channel, config array name;"
            " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  // Output channel
  if ((channel = Tcl_GetChannel(interp, argv[1], &mode)) == NULL) {
    return TCL_ERROR;
  }
  if (!(mode & TCL_WRITABLE)) {
    Oc_Snprintf(buf,sizeof(buf),"%s is not a writable channel",argv[1]);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  // Averaging type
  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR(argv[2]),
                   OC_CONST84_CHAR("average"),TCL_LEAVE_ERR_MSG);
  if(cptr==NULL) return TCL_ERROR;
  enum { point, line, plane, space, ball } avetype = space;
  if(strcmp(cptr,"point")==0) {
    avetype = point;
  } else if(strcmp(cptr,"line")==0) {
    avetype = line;
  } else if(strcmp(cptr,"plane")==0) {
    avetype = plane;
  } else if(strcmp(cptr,"ball")==0) {
    avetype = ball;
  } else if(strcmp(cptr,"space")!=0) {
    Oc_Snprintf(buf,sizeof(buf),"Invalid average type string: %s;"
            " Should be one of point, line, plane or space",
            cptr);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  // Average type axis spec
  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR(argv[2]),
                   OC_CONST84_CHAR("axis"),TCL_LEAVE_ERR_MSG);
  if(cptr==NULL) return TCL_ERROR;
  if(cptr[1]!='\0' ||
     (cptr[0]!='x' && cptr[0]!='y' && cptr[0]!='z')) {
    Oc_Snprintf(buf,sizeof(buf),"Invalid axis spec: %s;"
            " Should be x, y, or z",
            cptr);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  char axis = cptr[0];

  // Is this a rectangular mesh?
  if(strcmp(myMeshArray[activeMeshId]->GetMeshType(),"Vf_GridVec3f")!=0) {
    Oc_Snprintf(buf,sizeof(buf),"Invalid mesh type: %s\n",
                myMeshArray[activeMeshId]->GetMeshType());
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  // Downcast
  Vf_GridVec3f& grid =
    *(dynamic_cast<Vf_GridVec3f*>(myMeshArray[activeMeshId]));
  OC_REAL8m scale=grid.GetValueMultiplier();

  // Check dimensions
  OC_INDEX i,j,k,isize,jsize,ksize;
  grid.GetDimens(isize,jsize,ksize);
  if(isize<1 || jsize<1 || ksize<1) {
    Oc_Snprintf(buf,sizeof(buf),"Bad mesh dimensions: %d %d %d",
                isize,jsize,ksize);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  OC_INDEX whole_mesh_size = OC_INDEX(isize)*OC_INDEX(jsize)*OC_INDEX(ksize);

  Nb_Vec3<OC_REAL8> base=grid.GetBasePoint();
  Nb_Vec3<OC_REAL8> step=grid.GetGridStep();

  // Range select
  OC_REAL8m xmin = base.x - step.x/2.;
  OC_REAL8m ymin = base.y - step.y/2.;
  OC_REAL8m zmin = base.z - step.z/2.;
  OC_REAL8m xmax = base.x + (isize-0.5)*step.x;
  OC_REAL8m ymax = base.y + (jsize-0.5)*step.y;
  OC_REAL8m zmax = base.z + (ksize-0.5)*step.z;
  if(xmin>xmax) { OC_REAL8m temp=xmin; xmin=xmax; xmax=temp; }
  if(ymin>ymax) { OC_REAL8m temp=ymin; ymin=ymax; ymax=temp; }
  if(zmin>zmax) { OC_REAL8m temp=zmin; zmin=zmax; zmax=temp; }

  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR(argv[2]),
                   OC_CONST84_CHAR("range"),0);
  if(cptr!=NULL) {
    // range specified
    int range_arr_size;
    CONST84 char ** range_arr;
    if (Tcl_SplitList(interp,OC_CONST84_CHAR(cptr),
                      &range_arr_size,&range_arr)!=TCL_OK) {
      return TCL_ERROR;
    }
    if(range_arr_size!=6) {
      Tcl_Free((char *)range_arr);
      Oc_Snprintf(buf,sizeof(buf),"Range list has %d != 6 elements",
                  range_arr_size);
      Tcl_AppendResult(interp,buf,(char *)NULL);
      return TCL_ERROR;
    }
    if(strcmp("-",range_arr[0])!=0) {
      OC_REAL8m temp = Nb_Atof(range_arr[0]);
      Nb_NOP(temp); // Hack for g++ 3.2.3 bug
      if(temp>xmin) xmin=temp;
    }
    if(strcmp("-",range_arr[1])!=0) {
      OC_REAL8m temp = Nb_Atof(range_arr[1]);
      Nb_NOP(temp); // Hack for g++ 3.2.3 bug
      if(temp>ymin) ymin=temp;
    }
    if(strcmp("-",range_arr[2])!=0) {
      OC_REAL8m temp = Nb_Atof(range_arr[2]);
      Nb_NOP(temp); // Hack for g++ 3.2.3 bug
      if(temp>zmin) zmin=temp;
    }
    if(strcmp("-",range_arr[3])!=0) {
      OC_REAL8m temp = Nb_Atof(range_arr[3]);
      Nb_NOP(temp); // Hack for g++ 3.2.3 bug
      if(temp<xmax) xmax=temp;
    }
    if(strcmp("-",range_arr[4])!=0) {
      OC_REAL8m temp = Nb_Atof(range_arr[4]);
      Nb_NOP(temp); // Hack for g++ 3.2.3 bug
      if(temp<ymax) ymax=temp;
    }
    if(strcmp("-",range_arr[5])!=0) {
      OC_REAL8m temp = Nb_Atof(range_arr[5]);
      Nb_NOP(temp); // Hack for g++ 3.2.3 bug
      if(temp<zmax) zmax=temp;
    }
    Tcl_Free((char *)range_arr);
  } else if((cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR(argv[2]),
                              OC_CONST84_CHAR("rrange"),0))!=NULL) {
    // rrange specified
    int range_arr_size;
    CONST84 char ** range_arr;
    if (Tcl_SplitList(interp,OC_CONST84_CHAR(cptr),
                      &range_arr_size,&range_arr)!=TCL_OK) {
      return TCL_ERROR;
    }
    if(range_arr_size!=6) {
      Tcl_Free((char *)range_arr);
      Oc_Snprintf(buf,sizeof(buf),"Rrange list has %d != 6 elements",
                  range_arr_size);
      Tcl_AppendResult(interp,buf,(char *)NULL);
      return TCL_ERROR;
    }
    OC_REAL8m txmin=xmin,txmax=xmax;
    OC_REAL8m tymin=ymin,tymax=ymax;
    OC_REAL8m tzmin=zmin,tzmax=zmax;
    if(strcmp("-",range_arr[0])!=0) {
      OC_REAL8m temp = Nb_Atof(range_arr[0]);
      Nb_NOP(temp); // Hack for g++ 3.2.3 bug
      if(temp>0) txmin = (1.0-temp)*xmin + temp*xmax;
    }
    if(strcmp("-",range_arr[1])!=0) {
      OC_REAL8m temp = Nb_Atof(range_arr[1]);
      Nb_NOP(temp); // Hack for g++ 3.2.3 bug
      if(temp>0) tymin = (1.0-temp)*ymin + temp*ymax;
    }
    if(strcmp("-",range_arr[2])!=0) {
      OC_REAL8m temp = Nb_Atof(range_arr[2]);
      Nb_NOP(temp); // Hack for g++ 3.2.3 bug
      if(temp>0) tzmin = (1.0-temp)*zmin + temp*zmax;
    }
    if(strcmp("-",range_arr[3])!=0) {
      OC_REAL8m temp = Nb_Atof(range_arr[3]);
      Nb_NOP(temp); // Hack for g++ 3.2.3 bug
      if(temp<1) txmax = (1.0-temp)*xmin + temp*xmax;
    }
    if(strcmp("-",range_arr[4])!=0) {
      OC_REAL8m temp = Nb_Atof(range_arr[4]);
      Nb_NOP(temp); // Hack for g++ 3.2.3 bug
      if(temp<1) tymax = (1.0-temp)*ymin + temp*ymax;
    }
    if(strcmp("-",range_arr[5])!=0) {
      OC_REAL8m temp = Nb_Atof(range_arr[5]);
      Nb_NOP(temp); // Hack for g++ 3.2.3 bug
      if(temp<1) tzmax = (1.0-temp)*zmin + temp*zmax;
    }
    Tcl_Free((char *)range_arr);
    xmin = txmin;  xmax = txmax;
    ymin = tymin;  ymax = tymax;
    zmin = tzmin;  zmax = tzmax;
  }
  /// Otherwise, if neither range or rrange specified, default is full span

  // Scaling
  OC_BOOL normalize = 0;
  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR(argv[2]),
                   OC_CONST84_CHAR("normalize"),0);
  if(cptr!=NULL) {
    if(atoi(cptr)) normalize=1;
  }

  // Data table numeric format
  const char* numfmt = Tcl_GetVar2(interp,OC_CONST84_CHAR(argv[2]),
                                   OC_CONST84_CHAR("numfmt"),0);
  if(numfmt==NULL) numfmt = WMA_DEFAULT_NUM_FMT;

  // Determine base numeric format width
  Oc_Snprintf(buf,sizeof(buf),numfmt,1.0);
  const size_t colwidth = strlen(buf);

  // Default point position output?
  OC_BOOL defaultpos = 1;
  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR(argv[2]),
                   OC_CONST84_CHAR("defaultpos"),0);
  if(cptr!=NULL) {
    if(!atoi(cptr)) defaultpos=0;
  }

  // Default outputs?
  OC_BOOL defaultvals = 1;
  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR(argv[2]),
                   OC_CONST84_CHAR("defaultvals"),0);
  if(cptr!=NULL) {
    if(!atoi(cptr)) defaultvals=0;
  }
  OC_INDEX value_column_count = ( defaultvals ? 3 : 0 );
  /// value_column_count is the number of output columns, excluding the
  /// index column (if any), and the x/y/z position columns (if any).

  // Extra outputs?
  OC_BOOL extravals = 0;
  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR(argv[2]),
                   OC_CONST84_CHAR("extravals"),0);
  if(cptr!=NULL) {
    if(atoi(cptr)) extravals=1;
  }
  value_column_count += ( extravals ? 4 : 0 );

  // User-supplied index columns
  Nb_ArrayWrapper<Nb_DString> index_labels,index_units,index_values;
  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR(argv[2]),
                   OC_CONST84_CHAR("index"),0);
  OC_INDEX index_count = 0;
  if(cptr!=NULL) {
    // Index (or indices) specified
    int errcode;
    CONST84 char **index_elts;
    int index_elt_count; // Tcl_SplitList wants an int*, not unsigned int*.
    errcode = Tcl_SplitList(interp,OC_CONST84_CHAR(cptr),
                            &index_elt_count,&index_elts);
    if(errcode != TCL_OK) {
      return TCL_ERROR;
    }
    if(index_elt_count%3!=0) {
      Tcl_Free((char *)index_elts);
      Oc_Snprintf(buf,sizeof(buf),"index element count (%d)"
                  " not divisible by 3",index_elt_count);
      Tcl_AppendResult(interp,buf,(char *)NULL);
      return TCL_ERROR;
    }
    index_count = static_cast<OC_INDEX>(index_elt_count/3);
    index_labels.SetSize(index_count);
    index_units.SetSize(index_count);
    index_values.SetSize(index_count);
    for(i=0;i<static_cast<int>(index_count);++i) {
      // Use Nb_DString MergeArgs function to provide
      // embedded space protection in labels and units.
      Nb_DString dsarr[1];
      dsarr[0] = index_elts[3*i];
      index_labels[i].MergeArgs(1,dsarr);
      dsarr[0] = index_elts[3*i+1];
      index_units[i].MergeArgs(1,dsarr);

      // Set index_value using numfmt
      Oc_Snprintf(buf,sizeof(buf),numfmt,atof(index_elts[3*i+2]));
      index_values[i] = buf;
    }
    Tcl_Free((char *)index_elts);
  }

  // User supplied outputs
  Nb_ArrayWrapper<Nb_DString> user_labels,user_units,user_funcs;
  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR(argv[2]),
                   OC_CONST84_CHAR("valfuncs"),0);
  if(cptr!=NULL) {
    int valfuncs_count, errcode;
    CONST84 char **valfuncs_values;
    errcode = Tcl_SplitList(interp,OC_CONST84_CHAR(cptr),
                            &valfuncs_count,&valfuncs_values);
    if(errcode != TCL_OK) {
      return TCL_ERROR;
    }
    if(valfuncs_count%3!=0) {
      Tcl_Free((char *)valfuncs_values);
      Oc_Snprintf(buf,sizeof(buf),"valfuncs element count (%d)"
                  " not divisible by 3",valfuncs_count);
      Tcl_AppendResult(interp,buf,(char *)NULL);
      return TCL_ERROR;
    }
    OC_INDEX user_cols = static_cast<OC_INDEX>(valfuncs_count/3);
    user_labels.SetSize(user_cols);
    user_units.SetSize(user_cols);
    user_funcs.SetSize(user_cols);
    for(i=0;i<valfuncs_count;i+=3) {
      // Use Nb_DString MergeArgs function to provide
      // embedded space protection in labels and units.
      Nb_DString dsarr[1];
      dsarr[0] = valfuncs_values[i];
      user_labels[i/3].MergeArgs(1,dsarr);
      dsarr[0] = valfuncs_values[i+1];
      user_units[i/3].MergeArgs(1,dsarr);
      user_funcs[i/3] = Nb_DString(valfuncs_values[i+2]);
    }
    Tcl_Free((char *)valfuncs_values);
  }
  value_column_count += user_funcs.GetSize();
  Nb_ArrayWrapper<OC_REAL8m> value_column_results(value_column_count);

  // Column "units" header info.  Use Nb_DString MergeArgs
  // function to provide embedded space protection.
  Nb_DString meshunit,valueunit;
  { // There is some weird bug in the Open64 compiler version 4.2.4
    // that causes a "free() of invalid pointer" fatal error in glibc
    // if Nb_DString dstrarr[1] is not wrapped up inside (extreme)
    // local scope and mmdispcmds.o appears on the link line before
    // both colormap.o and display.o.  So, put braces around this
    // stanza to work around the bug.
    Nb_DString dstrarr[1];
    dstrarr[0] = grid.GetMeshUnit();
    meshunit.MergeArgs(1,dstrarr);
    if(normalize) dstrarr[0] = "";
    else          dstrarr[0] = grid.GetValueUnit();
    valueunit.MergeArgs(1,dstrarr);
  }

  // Column labels
  const char* vallab = Tcl_GetVar2(interp,OC_CONST84_CHAR(argv[2]),
                                   OC_CONST84_CHAR("vallab"),0);
  if(vallab==NULL) vallab = "M";
  Nb_DString xlab = vallab;   xlab.Append("_x");
  Nb_DString ylab = vallab;   ylab.Append("_y");
  Nb_DString zlab = vallab;   zlab.Append("_z");
  if(normalize || strcmp("",grid.GetValueUnit())==0) {
    xlab.ToLower();  ylab.ToLower();  zlab.ToLower();
  }

  // Header type
  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR(argv[2]),
                   OC_CONST84_CHAR("header"),TCL_LEAVE_ERR_MSG);
  if(cptr==NULL) return TCL_ERROR;
  enum { fullhead, shorthead, nohead } headtype = fullhead;
  if(strcmp(cptr,"shorthead")==0) {
    headtype = shorthead;
  } else if(strcmp(cptr,"nohead")==0) {
    headtype = nohead;
  } else if(strcmp(cptr,"fullhead")!=0) {
    Oc_Snprintf(buf,sizeof(buf),"Invalid header type string: %s;"
            " Should be one of fullhead, shorthead or nohead",
            cptr);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  // Trailer type
  cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR(argv[2]),
                   OC_CONST84_CHAR("trailer"),TCL_LEAVE_ERR_MSG);
  if(cptr==NULL) return TCL_ERROR;
  enum { tail, notail } tailtype = tail;
  if(strcmp(cptr,"notail")==0) {
    tailtype = notail;
  } else if(strcmp(cptr,"tail")!=0) {
    Oc_Snprintf(buf,sizeof(buf),"Invalid trailer type string: %s;"
            " Should be either tail or notail",
            cptr);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  // Determine computation sub-grid
  if(xmin>xmax || ymin>ymax || zmin>zmax) {
    // Nothing to do.
    if(headtype != nohead) {
#if WMA_USE_OBJ
      tmpobj = Tcl_NewStringObj("## Empty range\n",-1);
      Tcl_IncrRefCount(tmpobj);
      Tcl_WriteObj(channel,tmpobj);
      Tcl_DecrRefCount(tmpobj);
#else
      Oc_Snprintf(buf,sizeof(buf),"## Empty range\n");
      Tcl_Write(channel, buf, int(strlen(buf)));
#endif
    }
    return TCL_OK;
    /// NOTE: Checking here on the ?min/?max float values
    ///  protects against some overflow conditions on the
    ///  parallel integer constructions below
  }
  OC_INDEX imin=0,imax=isize;
  if(step.x>0) {
    imin = OC_INDEX(ceil((xmin - base.x)/step.x));
    imax = OC_INDEX(floor((xmax - base.x)/step.x));
  } else if(step.x<0) {
    imin = OC_INDEX(ceil((xmax - base.x)/step.x));
    imax = OC_INDEX(floor((xmin - base.x)/step.x));
  }
  ++imax;
  if(imin<0)     imin = 0;
  if(imax>isize) imax = isize;
  const OC_INDEX icount = imax - imin;

  OC_INDEX jmin=0,jmax=jsize;
  if(step.y>0) {
    jmin = OC_INDEX(ceil((ymin - base.y)/step.y));
    jmax = OC_INDEX(floor((ymax - base.y)/step.y));
  } else if(step.y<0) {
    jmin = OC_INDEX(ceil((ymax - base.y)/step.y));
    jmax = OC_INDEX(floor((ymin - base.y)/step.y));
  }
  ++jmax;
  if(jmin<0)     jmin = 0;
  if(jmax>jsize) jmax = jsize;
  const OC_INDEX jcount = jmax - jmin;

  OC_INDEX kmin=0,kmax=ksize;
  if(step.z>0) {
    kmin = OC_INDEX(ceil((zmin - base.z)/step.z));
    kmax = OC_INDEX(floor((zmax - base.z)/step.z));
  } else if(step.z<0) {
    kmin = OC_INDEX(ceil((zmax - base.z)/step.z));
    kmax = OC_INDEX(floor((zmin - base.z)/step.z));
  }
  ++kmax;
  if(kmin<0)     kmin = 0;
  if(kmax>ksize) kmax = ksize;
  const OC_INDEX kcount = kmax - kmin;

  if(icount<1 || jcount<1 || kcount<1) {
    // Nothing to do
#if WMA_USE_OBJ
      tmpobj = Tcl_NewStringObj("## Empty range\n",-1);
      Tcl_IncrRefCount(tmpobj);
      Tcl_WriteObj(channel,tmpobj);
      Tcl_DecrRefCount(tmpobj);
#else
      Oc_Snprintf(buf,sizeof(buf),"## Empty range\n");
      Tcl_Write(channel, buf, int(strlen(buf)));
#endif
    return TCL_OK;
  }
  const OC_INDEX volume_point_count = icount*jcount*kcount;

  // Evaluate user supplied outputs.
  Nb_ArrayWrapper< Nb_ArrayWrapper<OC_REAL8m> > uservals;
  uservals.SetSize(user_funcs.GetSize());
  OC_INDEX ielt;
  {
    Nb_TclCommand usercmd;
    usercmd.SetBaseCommand("WriteMeshAverages",interp,"expr",1);
#if NB_TCLOBJARRAY_AVAILABLE
    Nb_TclObjArray objarr(9);
#endif
#if TCL_MAJOR_VERSION<8 || (TCL_MAJOR_VERSION==8 && TCL_MINOR_VERSION<4) \
                        || !NB_TCLOBJARRAY_AVAILABLE
    // Argh.  Pre-"const" era Tcl.
    Oc_AutoBuf ab_x  = "x";
    Oc_AutoBuf ab_y  = "y";
    Oc_AutoBuf ab_z  = "z";
    Oc_AutoBuf ab_r  = "r";
    Oc_AutoBuf ab_vx = "vx";
    Oc_AutoBuf ab_vy = "vy";
    Oc_AutoBuf ab_vz = "vz";
    Oc_AutoBuf ab_vmag = "vmag";
#endif

    for(ielt=0;ielt<user_funcs.GetSize();ielt++) {
      uservals[ielt].SetSize(whole_mesh_size);
      usercmd.SetCommandArg(0,user_funcs[ielt].GetStr());

#if NB_TCLOBJARRAY_AVAILABLE
      objarr.WriteString(0,user_funcs[ielt].GetStr());
#else
      Oc_AutoBuf cmd = user_funcs[ielt].GetStr();
#endif

      OC_INDEX offset=kmin*jsize*isize;
      for(k=kmin;k<kmax;k++) {
        offset += jmin*isize;
        for(j=jmin;j<jmax;j++) {
          offset += imin;
          for(i=imin;i<imax;i++) {
            Nb_Vec3<OC_REAL8m>  v  = grid(i,j,k);
            Nb_Vec3<OC_REAL8m> pos = grid.Position(i,j,k);
#if NB_TCLOBJARRAY_AVAILABLE
# if TCL_MAJOR_VERSION<8 || (TCL_MAJOR_VERSION==8 && TCL_MINOR_VERSION<4)
            objarr.WriteDouble(1,pos.x);     Tcl_SetVar2Ex(interp,ab_x,0,objarr[1],0);
            objarr.WriteDouble(2,pos.y);     Tcl_SetVar2Ex(interp,ab_y,0,objarr[2],0);
            objarr.WriteDouble(3,pos.z);     Tcl_SetVar2Ex(interp,ab_z,0,objarr[3],0);
            objarr.WriteDouble(4,pos.Mag()); Tcl_SetVar2Ex(interp,ab_r,0,objarr[4],0);
            objarr.WriteDouble(5,scale*v.x); Tcl_SetVar2Ex(interp,ab_vx,0,objarr[5],0);
            objarr.WriteDouble(6,scale*v.y); Tcl_SetVar2Ex(interp,ab_vy,0,objarr[6],0);
            objarr.WriteDouble(7,scale*v.z); Tcl_SetVar2Ex(interp,ab_vz,0,objarr[7],0);
            objarr.WriteDouble(8,scale*v.Mag()); Tcl_SetVar2Ex(interp,ab_vmag,0,objarr[8],0);
# else
            objarr.WriteDouble(1,pos.x);     Tcl_SetVar2Ex(interp,"x",0,objarr[1],0);
            objarr.WriteDouble(2,pos.y);     Tcl_SetVar2Ex(interp,"y",0,objarr[2],0);
            objarr.WriteDouble(3,pos.z);     Tcl_SetVar2Ex(interp,"z",0,objarr[3],0);
            objarr.WriteDouble(4,pos.Mag()); Tcl_SetVar2Ex(interp,"r",0,objarr[4],0);
            objarr.WriteDouble(5,scale*v.x); Tcl_SetVar2Ex(interp,"vx",0,objarr[5],0);
            objarr.WriteDouble(6,scale*v.y); Tcl_SetVar2Ex(interp,"vy",0,objarr[6],0);
            objarr.WriteDouble(7,scale*v.z); Tcl_SetVar2Ex(interp,"vz",0,objarr[7],0);
            objarr.WriteDouble(8,scale*v.Mag()); Tcl_SetVar2Ex(interp,"vmag",0,objarr[8],0);
#endif
            double result_value;
            int result_code = Tcl_ExprDoubleObj(interp,objarr[0],&result_value);
#else // !NB_TCLOBJARRAY_AVAILABLE
            // Include '#' in the format specifier is included to insure
            // that a decimal point is output.  Otherwise Tcl may interpret
            // some of these values as integers, which can lead to oddities
            // such as integer division and overflow.
            Oc_Snprintf(buf,sizeof(buf),"%#.17g",pos.x);     Tcl_SetVar(interp,ab_x,buf,0);
            Oc_Snprintf(buf,sizeof(buf),"%#.17g",pos.y);     Tcl_SetVar(interp,ab_y,buf,0);
            Oc_Snprintf(buf,sizeof(buf),"%#.17g",pos.z);     Tcl_SetVar(interp,ab_z,buf,0);
            Oc_Snprintf(buf,sizeof(buf),"%#.17g",pos.Mag()); Tcl_SetVar(interp,ab_r,buf,0);
            Oc_Snprintf(buf,sizeof(buf),"%#.17g",scale*v.x);     Tcl_SetVar(interp,ab_vx,buf,0);
            Oc_Snprintf(buf,sizeof(buf),"%#.17g",scale*v.y);     Tcl_SetVar(interp,ab_vy,buf,0);
            Oc_Snprintf(buf,sizeof(buf),"%#.17g",scale*v.z);     Tcl_SetVar(interp,ab_vz,buf,0);
            Oc_Snprintf(buf,sizeof(buf),"%#.17g",scale*v.Mag()); Tcl_SetVar(interp,ab_vmag,buf,0);
            double result_value;
            int result_code = Tcl_ExprDouble(interp,cmd,&result_value);
#endif // NB_TCLOBJARRAY_AVAILABLE

            if(result_code != TCL_OK) {
              Oc_Snprintf(buf,sizeof(buf),
                          "\nwith args: x=%#.17g,  y=%#.17g,  z=%#.17g,    r=%#.17g"
                          "\n          vx=%#.17g, vy=%#.17g, vz=%#.17g, vmag=%#.17g",
                          pos.x,pos.y,pos.z,pos.Mag(),
                          scale*v.x,scale*v.y,scale*v.z,scale*v.Mag());
              Tcl_AppendResult(interp,"\n---------------------"
                               "\nBad Tcl script:\n",
                               user_funcs[ielt].GetStr(),buf,
                               "\n---------------------",(char *)NULL);
              return TCL_ERROR;
            }
            uservals[ielt][offset] = result_value;

            offset++;
          }
          offset += (isize-imax);
        }
        offset += (jsize-jmax)*isize;
      }
    }
  }

  if(headtype != nohead) {
    // Print ODT file header
#if WMA_USE_OBJ
    tmpobj = Tcl_NewStringObj("# ODT 1.0\n",-1);
    Tcl_IncrRefCount(tmpobj);
    Tcl_WriteObj(channel,tmpobj);
    Tcl_DecrRefCount(tmpobj);
#else
    Oc_Snprintf(buf,sizeof(buf),"# ODT 1.0\n");
    Tcl_Write(channel, buf, int(strlen(buf)));
#endif

    cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR(argv[2]),
                     OC_CONST84_CHAR("descript"),0);
    if(cptr!=NULL) {
#if WMA_USE_OBJ
      tmpobj = Tcl_ObjPrintf("## Desc: %s\n",cptr);
      Tcl_IncrRefCount(tmpobj);
      Tcl_WriteObj(channel,tmpobj);
      Tcl_DecrRefCount(tmpobj);
#else
      Oc_Snprintf(buf,sizeof(buf),"## Desc: %s\n",cptr);
      Tcl_Write(channel, buf, int(strlen(buf)));
#endif
    }

    if(headtype == fullhead) {
      OC_CHAR headbuf[MY_BUF_SIZE];
      Oc_Snprintf(headbuf,sizeof(headbuf),
        "## Active volume: (%.15g,%.15g,%.15g) x (%.15g,%.15g,%.15g)\n"
        "## Cell size: %.15g x %.15g x %.15g\n"
        "## Cells in active volume: %lld\n",
        static_cast<double>(xmin),
        static_cast<double>(ymin),
        static_cast<double>(zmin),
        static_cast<double>(xmax),
        static_cast<double>(ymax),
        static_cast<double>(zmax),
        static_cast<double>(fabs(step.x)),
        static_cast<double>(fabs(step.y)),
        static_cast<double>(fabs(step.z)),
        static_cast<long long int>(volume_point_count));
      // At least as late as Tcl 8.6, Tcl_ObjPrintf doesn't provide
      // support for 64-bit integers on platforms where 'long int' width
      // is 32 bits (e.g., Windows), so use snprintf with %lld instead.
      // Note that long long int, with format code %lld, is supported
      // by both C99 and C++11. Using %lld may be more robust than
      // "%" OC_INDEX_MOD "d" on Windows, where the Window-ish "I64"
      // format modifier may confuse some g++ installs.
#if WMA_USE_OBJ
      tmpobj = Tcl_NewStringObj(headbuf, -1);
      Tcl_IncrRefCount(tmpobj);
      Tcl_WriteObj(channel,tmpobj);
      Tcl_DecrRefCount(tmpobj);
#else
      Tcl_Write(channel, headbuf, int(strlen(headbuf)));
#endif
    }
#if WMA_USE_OBJ
    tmpobj = Tcl_NewStringObj("#\n# Table Start\n",-1);
    Tcl_IncrRefCount(tmpobj);
    Tcl_WriteObj(channel,tmpobj);
    Tcl_DecrRefCount(tmpobj);
#else
    Oc_Snprintf(buf,sizeof(buf),"#\n# Table Start\n");
    Tcl_Write(channel, buf, int(strlen(buf)));
#endif
  }

  char* bufptr=NULL;
  Nb_Vec3<OC_REAL8m> m;
  if(avetype==space) {
    // Volume average
    if(headtype != nohead) {
#if WMA_USE_OBJ
      tmpobj
        = Tcl_NewStringObj("# Title: Average across active volume\n",-1);
      Tcl_IncrRefCount(tmpobj);
      Tcl_WriteObj(channel,tmpobj);
      Tcl_DecrRefCount(tmpobj);
#else
      Oc_Snprintf(buf,sizeof(buf),
                  "# Title: Average across active volume\n");
      Tcl_Write(channel, buf, int(strlen(buf)));
#endif
      // Column headers
      bufptr = buf;
      bufptr += Oc_Snprintf(buf,sizeof(buf),"# Columns:\\\n#");
      if(index_count>0) {
        bufptr = WriteCenteredLabels(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                     colwidth,index_labels);
        *(bufptr++) = ' ';
      }
      if(defaultvals) {
        bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                    colwidth,xlab.GetStr());
        *(bufptr++) = ' ';
        bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                    colwidth,ylab.GetStr());
        *(bufptr++) = ' ';
        bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                    colwidth,zlab.GetStr());
        if(extravals || user_labels.GetSize()>0) *(bufptr++) = ' ';
      }
      if(extravals) {
        bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                    colwidth,"L1");
        *(bufptr++) = ' ';
        bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                    colwidth,"L2");
        *(bufptr++) = ' ';
        bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                    colwidth,"Min abs");
        *(bufptr++) = ' ';
        bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                    colwidth,"Max abs");
        if(user_labels.GetSize()>0) *(bufptr++) = ' ';
      }
      bufptr = WriteCenteredLabels(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                   colwidth,user_labels);
      *(bufptr++) = '\n';
      *bufptr = '\0'; // Safety
#if WMA_USE_OBJ
      tmpobj = Tcl_NewStringObj(buf,static_cast<int>(bufptr-buf));
      Tcl_IncrRefCount(tmpobj);
      Tcl_WriteObj(channel,tmpobj);
      Tcl_DecrRefCount(tmpobj);
#else
      Tcl_Write(channel, buf, static_cast<int>(bufptr-buf));
#endif

      // Unit headers
      bufptr = buf;
      bufptr += Oc_Snprintf(buf,sizeof(buf),"#   Units:\\\n#");
      if(index_count>0) {
        bufptr = WriteCenteredLabels(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                     colwidth,index_units);
        *(bufptr++) = ' ';
      }
      if(defaultvals) {
        bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                    colwidth,valueunit.GetStr());
        *(bufptr++) = ' ';
        bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                    colwidth,valueunit.GetStr());
        *(bufptr++) = ' ';
        bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                    colwidth,valueunit.GetStr());
        if(extravals || user_units.GetSize()>0) *(bufptr++) = ' ';
      }
      if(extravals) {
        bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                    colwidth,valueunit.GetStr());
        *(bufptr++) = ' ';
        bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                    colwidth,valueunit.GetStr());
        *(bufptr++) = ' ';
        bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                    colwidth,valueunit.GetStr());
        *(bufptr++) = ' ';
        bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                    colwidth,valueunit.GetStr());
        if(user_units.GetSize()>0) *(bufptr++) = ' ';
      }
      bufptr = WriteCenteredLabels(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                   colwidth,user_units);
      *(bufptr++) = '\n';
      *bufptr = '\0'; // Safety
#if WMA_USE_OBJ
      tmpobj = Tcl_NewStringObj(buf,static_cast<int>(bufptr-buf));
      Tcl_IncrRefCount(tmpobj);
      Tcl_WriteObj(channel,tmpobj);
      Tcl_DecrRefCount(tmpobj);
#else
      Tcl_Write(channel, buf, static_cast<int>(bufptr-buf));
#endif
    }

    OC_INDEX user_col_offset = 0;
    if(defaultvals || extravals) {
      Nb_Xpfloat mx, my, mz, magsum;
      Nb_Xpfloat L1,L2;
      OC_REAL8m minval,maxval;
      minval = maxval = abs(grid(imin,jmin,kmin).x);
      for(k=kmin;k<kmax;k++) for(j=jmin;j<jmax;j++) for(i=imin;i<imax;i++) {
        Nb_Vec3<OC_REAL8m>  v  = grid(i,j,k);
        mx += v.x;
        my += v.y;
        mz += v.z;
        if(normalize) magsum += v.Mag();
        if(extravals) {
          OC_REAL8m ax = abs(v.x);
          if(ax>maxval) maxval=ax;
	  if(ax<minval) minval=ax;
          L1 += ax;   L2 += ax*ax;
          OC_REAL8m ay = abs(v.y);
          if(ay>maxval) maxval=ay;
	  if(ay<minval) minval=ay;
          L1 += ay;   L2 += ay*ay;
          OC_REAL8m az = abs(v.z);
          if(az>maxval) maxval=az;
	  if(az<minval) minval=az;
          L1 += az;   L2 += az*az;
        }
      }
      if(defaultvals) {
        if(normalize && magsum.GetValue()>0.0) {
          m.Set(mx.GetValue()/magsum.GetValue(),
                my.GetValue()/magsum.GetValue(),
                mz.GetValue()/magsum.GetValue());
        } else {
          m.Set(mx.GetValue()*scale/OC_REAL8m(volume_point_count),
                my.GetValue()*scale/OC_REAL8m(volume_point_count),
                mz.GetValue()*scale/OC_REAL8m(volume_point_count));
        }
        value_column_results[OC_INDEX(0)] = m.x;
        value_column_results[OC_INDEX(1)] = m.y;
        value_column_results[OC_INDEX(2)] = m.z;
        user_col_offset += 3;
      }
      if(extravals) {
        value_column_results[user_col_offset++]
          = L1.GetValue()*scale/OC_REAL8m(volume_point_count);
        value_column_results[user_col_offset++]
          = sqrt(L2.GetValue()/OC_REAL8m(volume_point_count))*scale;
        value_column_results[user_col_offset++] = minval*scale;
        value_column_results[user_col_offset++] = maxval*scale;
      }
    }
    for(ielt=0;ielt<user_funcs.GetSize();ielt++) {
      Nb_Xpfloat sum;
      OC_INDEX kstep = jsize*isize;
      OC_INDEX koffset = kmin*kstep;
      for(k=kmin;k<kmax;k++,koffset+=kstep) {
        OC_INDEX joffset = koffset + jmin*isize;
        for(j=jmin;j<jmax;j++,joffset+=isize) {
          for(i=imin;i<imax;i++) {
            sum += uservals[ielt][joffset+i];
          }
        }
      }
      value_column_results[user_col_offset+ielt]
        = sum.GetValue()/double(volume_point_count);
    }

    bufptr = buf;
    for(ielt=0;ielt<index_count;ielt++) {
      bufptr += Oc_Snprintf(bufptr,sizeof(buf)-(bufptr-buf)-1," %s",
                            index_values[ielt].GetStr());
    }
    for(ielt=0;ielt<value_column_count;ielt++) {
      *(bufptr++) = ' ';
      bufptr += Oc_Snprintf(bufptr,sizeof(buf)-(bufptr-buf)-1,numfmt,
                            static_cast<double>(value_column_results[ielt]));
    }
    *(bufptr++) = '\n';
    *bufptr = '\0'; // Safety
#if WMA_USE_OBJ
    tmpobj = Tcl_NewStringObj(buf,static_cast<int>(bufptr-buf));
    Tcl_IncrRefCount(tmpobj);
    Tcl_WriteObj(channel,tmpobj);
    Tcl_DecrRefCount(tmpobj);
#else
    Tcl_Write(channel, buf, static_cast<int>(bufptr-buf));
#endif
  } else {
    // Plane averages
    TripleIndex triple_index(imin,imax,isize,
                             jmin,jmax,jsize,
                             kmin,kmax,ksize);
    const OC_INDEX& iref = triple_index.iref();
    const OC_INDEX& jref = triple_index.jref();
    const OC_INDEX& kref = triple_index.kref();
    int (TripleIndex::* tripstep)() = NULL;

    OC_REAL8m base_a=0,base_b=0,step_a=0,step_b=0;
    const OC_INDEX* a_index_ptr=NULL;
    const OC_INDEX* b_index_ptr=NULL;

    LinearTripleIndex linear_index(imin,imax,isize,
                                   jmin,jmax,jsize,
                                   kmin,kmax);
    const OC_INDEX& linear_offset = linear_index.offsetref();

    OC_INDEX manifold_size = 0; // Dummy values.  Actuals values
    int stepcontrol = -1;     // set inside switch case blocks.
    switch(avetype) {
    case plane:
      switch(axis) {
        case 'x':
          if(headtype != nohead) {
            Oc_Snprintf(buf,sizeof(buf),
                        "# Title: Averages across x-axis (%d points each)\n",
                        jcount*kcount);
          }
          triple_index.ResetYZX();
          if(defaultvals || extravals) tripstep = &TripleIndex::StepYZX;
          else                    tripstep = &TripleIndex::PlaneStepYZX;
          linear_index.SetStepYZX();
          manifold_size = jcount*kcount;
          base_a = base.x;
          step_a = step.x;
          a_index_ptr = &iref;
          break;
        case 'y':
          if(headtype != nohead) {
            Oc_Snprintf(buf,sizeof(buf),
                        "# Title: Averages across y-axis (%d points each)\n",
                        icount*kcount);
          }
          triple_index.ResetXZY();
          if(defaultvals || extravals) tripstep = &TripleIndex::StepXZY;
          else                    tripstep = &TripleIndex::PlaneStepXZY;
          linear_index.SetStepXZY();
          manifold_size = icount*kcount;
          base_a = base.y;
          step_a = step.y;
          a_index_ptr = &jref;
          break;
        case 'z':
          if(headtype != nohead) {
            Oc_Snprintf(buf,sizeof(buf),
                        "# Title: Averages across z-axis (%d points each)\n",
                        icount*jcount);
          }
          triple_index.ResetXYZ();
          if(defaultvals || extravals) tripstep = &TripleIndex::StepXYZ;
          else                    tripstep = &TripleIndex::PlaneStepXYZ;
          linear_index.SetStepXYZ();
          manifold_size = icount*jcount;
          base_a = base.z;
          step_a = step.z;
          a_index_ptr = &kref;
          break;
        default:
          Oc_Snprintf(buf,sizeof(buf),"Invalid axis character: %c",axis);
          Tcl_AppendResult(interp,buf,(char *)NULL);
          return TCL_ERROR;
      }
      if(headtype != nohead) {
#if WMA_USE_OBJ
        tmpobj = Tcl_NewStringObj(buf,-1);
        Tcl_IncrRefCount(tmpobj);
        Tcl_WriteObj(channel,tmpobj);
        Tcl_DecrRefCount(tmpobj);
#else
        Tcl_Write(channel, buf, int(strlen(buf))); // Title
#endif
        // Column headers
        bufptr = buf;
        bufptr += Oc_Snprintf(buf,sizeof(buf),"# Columns:\\\n#");
        if(index_count>0) {
          bufptr = WriteCenteredLabels(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                       colwidth,index_labels);
          *(bufptr++) = ' ';
        }
        if(defaultpos) {
          char axisstr[] = { axis , '\0' };
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,axisstr);
          *(bufptr++) = ' ';
        }
        if(defaultvals) {
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,xlab.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,ylab.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,zlab.GetStr());
          if(extravals || user_labels.GetSize()>0) *(bufptr++) = ' ';
        }
        if(extravals) {
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,"L1");
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,"L2");
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,"Min abs");
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,"Max abs");
          if(user_labels.GetSize()>0) *(bufptr++) = ' ';
        }
        bufptr = WriteCenteredLabels(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                     colwidth,user_labels);
        *(bufptr++) = '\n';
        *bufptr = '\0'; // Safety

#if WMA_USE_OBJ
        tmpobj = Tcl_NewStringObj(buf,static_cast<int>(bufptr-buf));
        Tcl_IncrRefCount(tmpobj);
        Tcl_WriteObj(channel,tmpobj);
        Tcl_DecrRefCount(tmpobj);
#else
        Tcl_Write(channel, buf, static_cast<int>(bufptr-buf));
#endif

        // Unit headers
        bufptr = buf;
        bufptr += Oc_Snprintf(buf,sizeof(buf),"#   Units:\\\n#");
        if(index_count>0) {
          bufptr = WriteCenteredLabels(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                       colwidth,index_units);
          *(bufptr++) = ' ';
        }
        if(defaultpos) {
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,meshunit.GetStr());
          *(bufptr++) = ' ';
        }
        if(defaultvals) {
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          if(extravals || user_units.GetSize()>0) *(bufptr++) = ' ';
        }
        if(extravals) {
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          if(user_units.GetSize()>0) *(bufptr++) = ' ';
        }
        bufptr = WriteCenteredLabels(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                     colwidth,user_units);
        *(bufptr++) = '\n';
        *bufptr = '\0'; // Safety
#if WMA_USE_OBJ
        tmpobj = Tcl_NewStringObj(buf,static_cast<int>(bufptr-buf));
        Tcl_IncrRefCount(tmpobj);
        Tcl_WriteObj(channel,tmpobj);
        Tcl_DecrRefCount(tmpobj);
#else
        Tcl_Write(channel, buf, static_cast<int>(bufptr-buf));
#endif
      }
      if(defaultvals || extravals) stepcontrol = (triple_index.*tripstep)();
      else                         stepcontrol = 0;
      linear_index.Step();
      while(stepcontrol<3) {
        OC_INDEX user_col_offset = 0;
        OC_REAL8m position = base_a+double(*a_index_ptr)*step_a;
        /// Note: This is a bit of a hack.  In the !defaultvals case,
        /// the triple_index pointer is one back from the working
        /// location.  But that difference affects only the averaging
        /// variable, so doesn't influence this "position".
        if(defaultvals || extravals) {
          Nb_Xpfloat mx, my, mz, magsum;
          Nb_Xpfloat L1,L2;
          OC_REAL8m minval,maxval;
          minval = maxval = abs(grid(iref,jref,kref).x);
          do {
            Nb_Vec3<OC_REAL8m>  v  = grid(iref,jref,kref);
            mx += v.x;
            my += v.y;
            mz += v.z;
            if(normalize) magsum += v.Mag();
            if(extravals) {
              OC_REAL8m ax = abs(v.x);
              if(ax>maxval) maxval=ax;
	      if(ax<minval) minval=ax;
              L1 += ax;   L2 += ax*ax;
              OC_REAL8m ay = abs(v.y);
              if(ay>maxval) maxval=ay;
	      if(ay<minval) minval=ay;
              L1 += ay;   L2 += ay*ay;
              OC_REAL8m az = abs(v.z);
              if(az>maxval) maxval=az;
	      if(az<minval) minval=az;
              L1 += az;   L2 += az*az;
            }
          } while((stepcontrol=(triple_index.*tripstep)())<2);
          if(defaultvals) {
            if(normalize && magsum.GetValue()>0.0) {
              m.Set(mx.GetValue()/magsum.GetValue(),
                    my.GetValue()/magsum.GetValue(),
                    mz.GetValue()/magsum.GetValue());
            } else {
              m.Set(mx.GetValue()*scale/OC_REAL8m(manifold_size),
                    my.GetValue()*scale/OC_REAL8m(manifold_size),
                    mz.GetValue()*scale/OC_REAL8m(manifold_size));
            }
            value_column_results[OC_INDEX(0)] = m.x;
            value_column_results[OC_INDEX(1)] = m.y;
            value_column_results[OC_INDEX(2)] = m.z;
            user_col_offset = 3;
          }
          if(extravals) {
            value_column_results[user_col_offset++]
              = L1.GetValue()*scale/OC_REAL8m(manifold_size);
            value_column_results[user_col_offset++]
              = sqrt(L2.GetValue()/OC_REAL8m(manifold_size))*scale;
            value_column_results[user_col_offset++] = minval*scale;
            value_column_results[user_col_offset++] = maxval*scale;
          }
        } else {
          stepcontrol = (triple_index.*tripstep)();
        }
        linear_index.SaveIndex();
        for(ielt=0;ielt<user_funcs.GetSize();ielt++) {
          linear_index.RestoreIndex();
          Nb_Xpfloat sum;
          do {
            sum += uservals[ielt][linear_offset];
          } while(linear_index.Step()<2);
          value_column_results[user_col_offset+ielt]
            = sum.GetValue()/double(manifold_size);
        }
        bufptr = buf;
        for(ielt=0;ielt<index_count;ielt++) {
          bufptr += Oc_Snprintf(bufptr,sizeof(buf)-(bufptr-buf)-1," %s",
                                index_values[ielt].GetStr());
        }
        if(defaultpos) {
          *(bufptr++) = ' ';
          bufptr += Oc_Snprintf(bufptr,sizeof(buf)-(bufptr-buf)-1,numfmt,
                                static_cast<double>(position)); // x, y, or z
        }
        for(ielt=0;ielt<value_column_count;ielt++) { // Other values
          *(bufptr++) = ' ';
          bufptr += Oc_Snprintf(bufptr,sizeof(buf)-(bufptr-buf)-1,numfmt,
                             static_cast<double>(value_column_results[ielt]));
        }
        *(bufptr++) = '\n';
        *bufptr = '\0'; // Safety
#if WMA_USE_OBJ
        tmpobj = Tcl_NewStringObj(buf,static_cast<int>(bufptr-buf));
        Tcl_IncrRefCount(tmpobj);
        Tcl_WriteObj(channel,tmpobj);
        Tcl_DecrRefCount(tmpobj);
#else
        Tcl_Write(channel, buf, static_cast<int>(bufptr-buf));
#endif
      }
      break;
    case line:
      switch(axis) {
        case 'x':
          if(headtype != nohead) {
            bufptr = buf;
            bufptr += Oc_Snprintf(buf,sizeof(buf),
                                  "# Title: Averages parallel to"
                                  " x-axis (%d points each)\n",icount);
            bufptr += Oc_Snprintf(bufptr,sizeof(buf),"# Columns:\\\n#");
            if(index_count>0) {
              bufptr = WriteCenteredLabels(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                           colwidth,index_labels);
              *(bufptr++) = ' ';
            }
            if(defaultpos) {
              bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                          colwidth,"y");
              *(bufptr++) = ' ';
              bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                          colwidth,"z");
              *(bufptr++) = ' ';
            }
          }
          triple_index.ResetXYZ();
          if(defaultvals || extravals) tripstep = &TripleIndex::StepXYZ;
          else                     tripstep = &TripleIndex::LineStepXYZ;
          linear_index.SetStepXYZ();
          manifold_size = icount;
          base_a = base.y;   step_a = step.y;   a_index_ptr = &jref;
          base_b = base.z;   step_b = step.z;   b_index_ptr = &kref;
          break;
        case 'y':
          if(headtype != nohead) {
            bufptr = buf;
            bufptr += Oc_Snprintf(buf,sizeof(buf),
                                  "# Title: Averages parallel to"
                                  " y-axis (%d points each)\n",jcount);
            bufptr += Oc_Snprintf(bufptr,sizeof(buf),"# Columns:\\\n#");
            if(index_count>0) {
              bufptr = WriteCenteredLabels(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                           colwidth,index_labels);
              *(bufptr++) = ' ';
            }
            if(defaultpos) {
              bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                          colwidth,"x");
              *(bufptr++) = ' ';
              bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                          colwidth,"z");
              *(bufptr++) = ' ';
            }
          }
          triple_index.ResetYXZ();
          if(defaultvals || extravals) tripstep = &TripleIndex::StepYXZ;
          else                     tripstep = &TripleIndex::LineStepYXZ;
          linear_index.SetStepYXZ();
          manifold_size = jcount;
          base_a = base.x;   step_a = step.x;   a_index_ptr = &iref;
          base_b = base.z;   step_b = step.z;   b_index_ptr = &kref;
          break;
        case 'z':
          if(headtype != nohead) {
            bufptr = buf;
            bufptr += Oc_Snprintf(buf,sizeof(buf),
                                  "# Title: Averages parallel to"
                                  " z-axis (%d points each)\n",kcount);
            bufptr += Oc_Snprintf(bufptr,sizeof(buf),"# Columns:\\\n#");
            if(index_count>0) {
              bufptr = WriteCenteredLabels(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                           colwidth,index_labels);
              *(bufptr++) = ' ';
            }
            if(defaultpos) {
              bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                          colwidth,"x");
              *(bufptr++) = ' ';
              bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                          colwidth,"y");
              *(bufptr++) = ' ';
            }
          }
          triple_index.ResetZXY();
          if(defaultvals || extravals) tripstep = &TripleIndex::StepZXY;
          else                     tripstep = &TripleIndex::LineStepZXY;
          linear_index.SetStepZXY();
          manifold_size = kcount;
          base_a = base.x;   step_a = step.x;   a_index_ptr = &iref;
          base_b = base.y;   step_b = step.y;   b_index_ptr = &jref;
          break;
        default:
          Oc_Snprintf(buf,sizeof(buf),"Invalid axis character: %c",axis);
          Tcl_AppendResult(interp,buf,(char *)NULL);
          return TCL_ERROR;
      }
      if(headtype != nohead) {
        // Finish column headers
        if(defaultvals) {
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,xlab.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,ylab.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,zlab.GetStr());
          if(extravals || user_labels.GetSize()>0) *(bufptr++) = ' ';
        }
        if(extravals) {
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,"L1");
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,"L2");
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,"Min abs");
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,"Max abs");
          if(user_labels.GetSize()>0) *(bufptr++) = ' ';
        }
        bufptr = WriteCenteredLabels(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                     colwidth,user_labels);
        *(bufptr++) = '\n';
        *bufptr = '\0'; // Safety
        // Write title and column headers
#if WMA_USE_OBJ
        tmpobj = Tcl_NewStringObj(buf,static_cast<int>(bufptr-buf));
        Tcl_IncrRefCount(tmpobj);
        Tcl_WriteObj(channel,tmpobj);
        Tcl_DecrRefCount(tmpobj);
#else
        Tcl_Write(channel, buf, static_cast<int>(bufptr-buf));
#endif
        // Unit headers
        bufptr = buf;
        bufptr += Oc_Snprintf(buf,sizeof(buf),"#   Units:\\\n#");
        if(index_count>0) {
          bufptr = WriteCenteredLabels(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                       colwidth,index_units);
          *(bufptr++) = ' ';
        }
        if(defaultpos) {
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,meshunit.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,meshunit.GetStr());
          *(bufptr++) = ' ';
        }
        if(defaultvals) {
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          if(extravals || user_units.GetSize()>0) *(bufptr++) = ' ';
        }
        if(extravals) {
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          if(user_units.GetSize()>0) *(bufptr++) = ' ';
        }
        bufptr = WriteCenteredLabels(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                     colwidth,user_units);
        *(bufptr++) = '\n';
        *bufptr = '\0'; // Safety
        // Write units line to table
#if WMA_USE_OBJ
        tmpobj = Tcl_NewStringObj(buf,static_cast<int>(bufptr-buf));
        Tcl_IncrRefCount(tmpobj);
        Tcl_WriteObj(channel,tmpobj);
        Tcl_DecrRefCount(tmpobj);
#else
        Tcl_Write(channel, buf, static_cast<int>(bufptr-buf));
#endif
      }
      if(defaultvals || extravals) stepcontrol = (triple_index.*tripstep)();
      else                         stepcontrol = 0;
      linear_index.Step();
      while(stepcontrol<3) {
        OC_INDEX user_col_offset = 0;
        OC_REAL8m position_a = base_a+double(*a_index_ptr)*step_a;
        OC_REAL8m position_b = base_b+double(*b_index_ptr)*step_b;
        /// Note: This is a bit of a hack.  In the !defaultvals case,
        /// the triple_index pointer is one back from the working
        /// location.  But that difference affects only the averaging
        /// variable, so doesn't influence this "position".
        if(defaultvals || extravals) {
          Nb_Xpfloat mx, my, mz, magsum;
          Nb_Xpfloat L1,L2;
          OC_REAL8m minval,maxval;
          minval = maxval = abs(grid(iref,jref,kref).x);
          do {
            Nb_Vec3<OC_REAL8m>  v  = grid(iref,jref,kref);
            mx += v.x;
            my += v.y;
            mz += v.z;
            if(normalize) magsum += v.Mag();
            if(extravals) {
              OC_REAL8m ax = abs(v.x);
              if(ax>maxval) maxval=ax;
	      if(ax<minval) minval=ax;
              L1 += ax;   L2 += ax*ax;
              OC_REAL8m ay = abs(v.y);
              if(ay>maxval) maxval=ay;
	      if(ay<minval) minval=ay;
              L1 += ay;   L2 += ay*ay;
              OC_REAL8m az = abs(v.z);
              if(az>maxval) maxval=az;
	      if(az<minval) minval=az;
              L1 += az;   L2 += az*az;
            }
          } while((stepcontrol=(triple_index.*tripstep)())==0);
          if(defaultvals) {
            if(normalize && magsum.GetValue()>0.0) {
              m.Set(mx.GetValue()/magsum.GetValue(),
                    my.GetValue()/magsum.GetValue(),
                    mz.GetValue()/magsum.GetValue());
            } else {
              m.Set(mx.GetValue()*scale/OC_REAL8m(manifold_size),
                    my.GetValue()*scale/OC_REAL8m(manifold_size),
                    mz.GetValue()*scale/OC_REAL8m(manifold_size));
            }
            value_column_results[OC_INDEX(0)] = m.x;
            value_column_results[OC_INDEX(1)] = m.y;
            value_column_results[OC_INDEX(2)] = m.z;
            user_col_offset = 3;
          }
          if(extravals) {
            value_column_results[user_col_offset++]
              = L1.GetValue()*scale/OC_REAL8m(manifold_size);
            value_column_results[user_col_offset++]
              = sqrt(L2.GetValue()/OC_REAL8m(manifold_size))*scale;
            value_column_results[user_col_offset++] = minval*scale;
            value_column_results[user_col_offset++] = maxval*scale;
          }
        } else {
          stepcontrol = (triple_index.*tripstep)();
        }
        linear_index.SaveIndex();
        for(ielt=0;ielt<user_funcs.GetSize();ielt++) {
          linear_index.RestoreIndex();
          Nb_Xpfloat sum;
          do {
            sum += uservals[ielt][linear_offset];
          } while(linear_index.Step()==0);
          value_column_results[user_col_offset+ielt]
            = sum.GetValue()/double(manifold_size);
        }
        bufptr = buf;
        for(ielt=0;ielt<index_count;ielt++) {
          bufptr += Oc_Snprintf(bufptr,sizeof(buf)-(bufptr-buf)-1," %s",
                                index_values[ielt].GetStr());
        }
        if(defaultpos) {
          *(bufptr++) = ' ';
          bufptr += Oc_Snprintf(bufptr,sizeof(buf)-(bufptr-buf)-1,numfmt,
                     static_cast<double>(position_a)); // One of x, y, or z
          *(bufptr++) = ' ';
          bufptr += Oc_Snprintf(bufptr,sizeof(buf)-(bufptr-buf)-1,numfmt,
                     static_cast<double>(position_b)); // Another of y or z
        }
        for(ielt=0;ielt<value_column_count;ielt++) { // Other values
          *(bufptr++) = ' ';
          bufptr += Oc_Snprintf(bufptr,sizeof(buf)-(bufptr-buf)-1,numfmt,
                 static_cast<double>(value_column_results[ielt]));
        }
        *(bufptr++) = '\n';
        *bufptr = '\0'; // Safety
#if WMA_USE_OBJ
        tmpobj = Tcl_NewStringObj(buf,static_cast<int>(bufptr-buf));
        Tcl_IncrRefCount(tmpobj);
        Tcl_WriteObj(channel,tmpobj);
        Tcl_DecrRefCount(tmpobj);
#else
        Tcl_Write(channel, buf, static_cast<int>(bufptr-buf));
#endif
      }
      break;
    case point:
      // Point output (no averaging)
      if(headtype != nohead) {
#if WMA_USE_OBJ
        tmpobj = Tcl_NewStringObj("# Title: Points in specified volume\n",-1);
        Tcl_IncrRefCount(tmpobj);
        Tcl_WriteObj(channel,tmpobj);
        Tcl_DecrRefCount(tmpobj);
#else
        Oc_Snprintf(buf,sizeof(buf),
                    "# Title: Points in specified volume\n");
        Tcl_Write(channel, buf, int(strlen(buf)));
#endif
        // Column headers
        bufptr = buf;
        bufptr += Oc_Snprintf(buf,sizeof(buf),"# Columns:\\\n#");
        if(index_count>0) {
          bufptr = WriteCenteredLabels(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                       colwidth,index_labels);
          *(bufptr++) = ' ';
        }
        if(defaultpos) {
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,"x");
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,"y");
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,"z");
          *(bufptr++) = ' ';
        }
        if(defaultvals) {
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,xlab.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,ylab.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,zlab.GetStr());
          if(extravals || user_labels.GetSize()>0) *(bufptr++) = ' ';
        }
        if(extravals) {
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,"L1");
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,"L2");
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,"Min abs");
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,"Max abs");
          if(user_labels.GetSize()>0) *(bufptr++) = ' ';
        }
        bufptr = WriteCenteredLabels(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                     colwidth,user_labels);
        *(bufptr++) = '\n';
        *bufptr = '\0'; // Safety
#if WMA_USE_OBJ
        tmpobj = Tcl_NewStringObj(buf,static_cast<int>(bufptr-buf));
        Tcl_IncrRefCount(tmpobj);
        Tcl_WriteObj(channel,tmpobj);
        Tcl_DecrRefCount(tmpobj);
#else
        Tcl_Write(channel, buf, static_cast<int>(bufptr-buf));
#endif
        // Unit headers
        bufptr = buf;
        bufptr += Oc_Snprintf(buf,sizeof(buf),"#   Units:\\\n#");
        if(index_count>0) {
          bufptr = WriteCenteredLabels(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                       colwidth,index_units);
          *(bufptr++) = ' ';
        }
        if(defaultpos) {
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,meshunit.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,meshunit.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,meshunit.GetStr());
          *(bufptr++) = ' ';
        }
        if(defaultvals) {
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          if(extravals || user_units.GetSize()>0) *(bufptr++) = ' ';
        }
        if(extravals) {
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          if(user_units.GetSize()>0) *(bufptr++) = ' ';
        }
        bufptr = WriteCenteredLabels(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                     colwidth,user_units);
        *(bufptr++) = '\n';
        *bufptr = '\0'; // Safety
#if WMA_USE_OBJ
        tmpobj = Tcl_NewStringObj(buf,static_cast<int>(bufptr-buf));
        Tcl_IncrRefCount(tmpobj);
        Tcl_WriteObj(channel,tmpobj);
        Tcl_DecrRefCount(tmpobj);
#else
        Tcl_Write(channel, buf, static_cast<int>(bufptr-buf));
#endif
      }

      for(k=kmin;k<kmax;k++) {
        OC_INDEX joffset = (k*jsize + jmin)*isize;
        for(j=jmin;j<jmax;j++,joffset+=isize) for(i=imin;i<imax;i++) {
          OC_INDEX user_col_offset = 0;
          if(defaultvals || extravals) {
            m=grid(i,j,k);
            if(defaultvals) {
              if(normalize) {
                OC_REAL8m mag = m.Mag();
                if(mag>0.) { m.x /= mag;  m.y /= mag;  m.z /= mag; }
              } else {
                m *= scale;
              }
              value_column_results[OC_INDEX(0)] = m.x;
              value_column_results[OC_INDEX(1)] = m.y;
              value_column_results[OC_INDEX(2)] = m.z;
              user_col_offset = 3;
            }
            if(extravals) {
              OC_REAL8m ax = abs(m.x);
              OC_REAL8m ay = abs(m.y);
              OC_REAL8m az = abs(m.z);
              OC_REAL8m minval=(ax<ay ? (ax<az? ax : az) : (ay<az ? ay : az));
              OC_REAL8m maxval=(ax>ay ? (ax>az? ax : az) : (ay>az ? ay : az));
              value_column_results[user_col_offset++] = (ax+ay+az)*scale;
              value_column_results[user_col_offset++]
                = sqrt(ax*ax+ay*ay+az*az)*scale;
              value_column_results[user_col_offset++] = minval*scale;
              value_column_results[user_col_offset++] = maxval*scale;
            }
          }
          for(ielt=0;ielt<user_funcs.GetSize();ielt++) {
            value_column_results[user_col_offset+ielt]
              = uservals[ielt][joffset+i];
          }

          bufptr = buf;
          for(ielt=0;ielt<index_count;ielt++) {
            bufptr += Oc_Snprintf(bufptr,sizeof(buf)-(bufptr-buf)-1," %s",
                                  index_values[ielt].GetStr());
          }
          if(defaultpos) {
            *(bufptr++) = ' ';
            bufptr += Oc_Snprintf(bufptr,sizeof(buf)-(bufptr-buf)-1,numfmt,
                                static_cast<double>(base.x+double(i)*step.x));
            *(bufptr++) = ' ';
            bufptr += Oc_Snprintf(bufptr,sizeof(buf)-(bufptr-buf)-1,numfmt,
                                static_cast<double>(base.y+double(j)*step.y));
            *(bufptr++) = ' ';
            bufptr += Oc_Snprintf(bufptr,sizeof(buf)-(bufptr-buf)-1,numfmt,
                                static_cast<double>(base.z+double(k)*step.z));
          }
          for(ielt=0;ielt<value_column_count;ielt++) {
            *(bufptr++) = ' ';
            bufptr += Oc_Snprintf(bufptr,sizeof(buf)-(bufptr-buf)-1,numfmt,
                            static_cast<double>(value_column_results[ielt]));
          }
          *(bufptr++) = '\n';
          *bufptr = '\0'; // Safety
#if WMA_USE_OBJ
          tmpobj = Tcl_NewStringObj(buf,static_cast<int>(bufptr-buf));
          Tcl_IncrRefCount(tmpobj);
          Tcl_WriteObj(channel,tmpobj);
          Tcl_DecrRefCount(tmpobj);
#else
          Tcl_Write(channel, buf, static_cast<int>(bufptr-buf));
#endif
        }
      }
      break;
    case ball: {
      // Compute ball averaged values inside specified volume
      OC_BOOL br_error;
      cptr=Tcl_GetVar2(interp,OC_CONST84_CHAR(argv[2]),
                       OC_CONST84_CHAR("ball_radius"),0);
      OC_REAL8m ball_radius = Nb_Atof(cptr,br_error);
      if(br_error) {
        Oc_Snprintf(buf,sizeof(buf),"Input error: ball averaging"
                    " radius not specified");
        Tcl_AppendResult(interp, buf, (char *) NULL);
        return TCL_ERROR;
      }
      OC_REAL8m ball_radsq = ball_radius*ball_radius;
      OC_INDEX irad = OC_INDEX(floor(ball_radius/step.x));
      OC_INDEX jrad = OC_INDEX(floor(ball_radius/step.y));
      OC_INDEX krad = OC_INDEX(floor(ball_radius/step.z));

      // If specified range is not 3D, then reduce ball dimension
      // accordingly.
      if(icount<=1) irad=0;
      if(jcount<=1) jrad=0;
      if(kcount<=1) krad=0;

      // Count number of points (grid nodes) inside ball.  Special case:
      // If ball radius is larger than half of any range dimension, then
      // no averages will be computed and we can short-circuit the
      // computation.  Denote this by keeping ball_point_count == 0.
      OC_INDEX ball_point_count = 0;
      if(2*irad<icount && 2*jrad<jcount && 2*krad<kcount) {
        for(k=-krad;k<=krad;k++) {
          OC_REAL8m zoff = k*step.z;
          OC_REAL8m zoffsq=zoff*zoff;
          for(j=-jrad;j<=jrad;j++) {
            OC_REAL8m yoff = j*step.y;
            OC_REAL8m yoffsq=yoff*yoff;
            for(i=-irad;i<=irad;i++) {
              OC_REAL8m xoff = i*step.x;
              OC_REAL8m xoffsq=xoff*xoff;
              if(zoffsq+yoffsq+xoffsq<=ball_radsq) {
                ball_point_count++;
              }
            }
          }
        }
      }

      if(headtype != nohead) {
        if(ball_point_count>1) {
          Oc_Snprintf(buf,sizeof(buf),"# Title: Radius %g ball averages"
                      " through specified volume"
                      " (%ld points per ball)\n",
                      static_cast<double>(ball_radius),
                      static_cast<long>(ball_point_count));
        } else {
          Oc_Snprintf(buf,sizeof(buf),"# Title: Radius %g ball averages"
                      " through specified volume"
                      " (1 point per ball)\n",
                      static_cast<double>(ball_radius));
        }
#if WMA_USE_OBJ
        tmpobj = Tcl_NewStringObj(buf,-1);
        Tcl_IncrRefCount(tmpobj);
        Tcl_WriteObj(channel,tmpobj);
        Tcl_DecrRefCount(tmpobj);
#else
        Tcl_Write(channel, buf, int(strlen(buf)));
#endif
        // Column headers
        bufptr = buf;
        bufptr += Oc_Snprintf(buf,sizeof(buf),"# Columns:\\\n#");
        if(index_count>0) {
          bufptr = WriteCenteredLabels(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                       colwidth,index_labels);
          *(bufptr++) = ' ';
        }
        if(defaultpos) {
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,"x");
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,"y");
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,"z");
          *(bufptr++) = ' ';
        }
        if(defaultvals) {
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,xlab.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,ylab.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,zlab.GetStr());
          if(extravals || user_labels.GetSize()>0) *(bufptr++) = ' ';
        }
        if(extravals) {
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,"L1");
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,"L2");
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,"Min abs");
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,"Max abs");
          if(user_labels.GetSize()>0) *(bufptr++) = ' ';
        }
        bufptr = WriteCenteredLabels(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                     colwidth,user_labels);
        *(bufptr++) = '\n';
        *bufptr = '\0'; // Safety
#if WMA_USE_OBJ
        tmpobj = Tcl_NewStringObj(buf,static_cast<int>(bufptr-buf));
        Tcl_IncrRefCount(tmpobj);
        Tcl_WriteObj(channel,tmpobj);
        Tcl_DecrRefCount(tmpobj);
#else
        Tcl_Write(channel, buf, static_cast<int>(bufptr-buf));
#endif

        // Unit headers
        bufptr = buf;
        bufptr += Oc_Snprintf(buf,sizeof(buf),"#   Units:\\\n#");
        if(index_count>0) {
          bufptr = WriteCenteredLabels(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                       colwidth,index_units);
          *(bufptr++) = ' ';
        }
        if(defaultpos) {
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,meshunit.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,meshunit.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,meshunit.GetStr());
          *(bufptr++) = ' ';
        }
        if(defaultvals) {
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          if(extravals || user_units.GetSize()>0) *(bufptr++) = ' ';
        }
        if(extravals) {
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          *(bufptr++) = ' ';
          bufptr = CenterStringOutput(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                      colwidth,valueunit.GetStr());
          if(user_units.GetSize()>0) *(bufptr++) = ' ';
        }
        bufptr = WriteCenteredLabels(bufptr,sizeof(buf)-(bufptr-buf)-1,
                                     colwidth,user_units);
        *(bufptr++) = '\n';
        *bufptr = '\0'; // Safety
#if WMA_USE_OBJ
        tmpobj = Tcl_NewStringObj(buf,static_cast<int>(bufptr-buf));
        Tcl_IncrRefCount(tmpobj);
        Tcl_WriteObj(channel,tmpobj);
        Tcl_DecrRefCount(tmpobj);
#else
        Tcl_Write(channel, buf, static_cast<int>(bufptr-buf));
#endif
      }

      // Step through points about which a ball of radius ball_radius
      // fits entirely inside the active region.
      if(ball_point_count>0) {
        for(k=kmin+krad;k<kmax-krad;k++) for(j=jmin+jrad;j<jmax-jrad;j++)
          for(i=imin+irad;i<imax-irad;i++) {
            OC_INDEX user_col_offset = 0;
            if(defaultvals || extravals) {
              m.Set(0.,0.,0.);
              OC_REAL8m magsum=0.0;
              Nb_Xpfloat L1,L2;
              OC_REAL8m minval = OC_REAL8m_MAX, maxval = -1.0;
              for(OC_INDEX k2=k-krad;k2<k+krad+1;k2++) {
                OC_REAL8m zoff = (k2-k)*step.z;
                OC_REAL8m zoffsq=zoff*zoff;
                for(OC_INDEX j2=j-jrad;j2<j+jrad+1;j2++) {
                  OC_REAL8m yoff = (j2-j)*step.y;
                  OC_REAL8m yoffsq=yoff*yoff;
                  for(OC_INDEX i2=i-irad;i2<i+irad+1;i2++) {
                    OC_REAL8m xoff = (i2-i)*step.x;
                    OC_REAL8m xoffsq=xoff*xoff;
                    if(zoffsq+yoffsq+xoffsq>ball_radsq) continue;
                    Nb_Vec3<OC_REAL8m>  v  = grid(i2,j2,k2);
                    m+=v;
                    if(normalize) magsum += v.Mag();
                    if(extravals) {
                      OC_REAL8m ax = abs(v.x);
                      if(ax>maxval) maxval=ax;
		      if(ax<minval) minval=ax;
                      L1 += ax;   L2 += ax*ax;
                      OC_REAL8m ay = abs(v.y);
                      if(ay>maxval) maxval=ay;
		      if(ay<minval) minval=ay;
                      L1 += ay;   L2 += ay*ay;
                      OC_REAL8m az = abs(v.z);
                      if(az>maxval) maxval=az;
		      if(az<minval) minval=az;
                      L1 += az;   L2 += az*az;
                    }
                  }
                }
              }
              if(defaultvals) {
                if(normalize && magsum>0.0) m *= 1.0/magsum;
                else m *= scale/OC_REAL8m(ball_point_count);
                value_column_results[OC_INDEX(0)] = m.x;
                value_column_results[OC_INDEX(1)] = m.y;
                value_column_results[OC_INDEX(2)] = m.z;
                user_col_offset = 3;
              }
              if(extravals) {
                value_column_results[user_col_offset++]
                  = L1.GetValue()*scale/OC_REAL8m(ball_point_count);
                value_column_results[user_col_offset++]
                  = sqrt(L2.GetValue()/OC_REAL8m(ball_point_count))*scale;
                value_column_results[user_col_offset++] = minval*scale;
                value_column_results[user_col_offset++] = maxval*scale;
              }
            }
            for(ielt=0;ielt<user_funcs.GetSize();ielt++) {
              OC_REAL8m sum=0.0;
              OC_INDEX kstep = jsize*isize;
              OC_INDEX koffset = (k-krad)*kstep;
              for(OC_INDEX k2=k-krad;k2<k+krad+1;k2++,koffset+=kstep) {
                OC_REAL8m zoff = (k2-k)*step.z;
                OC_REAL8m zoffsq=zoff*zoff;
                OC_INDEX joffset = koffset + (j-jrad)*isize;
                for(OC_INDEX j2=j-jrad;j2<j+jrad+1;j2++,joffset+=isize) {
                  OC_REAL8m yoff = (j2-j)*step.y;
                  OC_REAL8m yoffsq=yoff*yoff;
                  for(OC_INDEX i2=i-irad;i2<i+irad+1;i2++) {
                    OC_REAL8m xoff = (i2-i)*step.x;
                    OC_REAL8m xoffsq=xoff*xoff;
                    if(zoffsq+yoffsq+xoffsq>ball_radsq) continue;
                    sum += uservals[ielt][joffset+i2];
                  }
                }
              }
              value_column_results[user_col_offset+ielt]
                = sum/double(ball_point_count);
            }

            bufptr = buf;
            for(ielt=0;ielt<index_count;ielt++) {
              bufptr += Oc_Snprintf(bufptr,sizeof(buf)-(bufptr-buf)-1," %s",
                                    index_values[ielt].GetStr());
            }
            if(defaultpos) {
              *(bufptr++) = ' ';
              bufptr += Oc_Snprintf(bufptr,sizeof(buf)-(bufptr-buf)-1,numfmt,
                                static_cast<double>(base.x+double(i)*step.x));
              *(bufptr++) = ' ';
              bufptr += Oc_Snprintf(bufptr,sizeof(buf)-(bufptr-buf)-1,numfmt,
                                static_cast<double>(base.y+double(j)*step.y));
              *(bufptr++) = ' ';
              bufptr += Oc_Snprintf(bufptr,sizeof(buf)-(bufptr-buf)-1,numfmt,
                                static_cast<double>(base.z+double(k)*step.z));
            }
            for(ielt=0;ielt<value_column_count;ielt++) {
              *(bufptr++) = ' ';
              bufptr += Oc_Snprintf(bufptr,sizeof(buf)-(bufptr-buf)-1,numfmt,
                            static_cast<double>(value_column_results[ielt]));
            }
            *(bufptr++) = '\n';
            *bufptr = '\0'; // Safety
#if WMA_USE_OBJ
            tmpobj = Tcl_NewStringObj(buf,static_cast<int>(bufptr-buf));
            Tcl_IncrRefCount(tmpobj);
            Tcl_WriteObj(channel,tmpobj);
            Tcl_DecrRefCount(tmpobj);
#else
            Tcl_Write(channel, buf, static_cast<int>(bufptr-buf));
#endif
          }
        }
      }
      break;
    default:
      Oc_Snprintf(buf,sizeof(buf),"Programming error: avetype %d"
                  " invalid or not properly handled",
                  int(avetype));
      Tcl_AppendResult(interp, buf, (char *) NULL);
      return TCL_ERROR;
      // Implicit "break"
    }
  }

  if(tailtype != notail) {
    // Print ODT file trailer
#if WMA_USE_OBJ
    tmpobj = Tcl_NewStringObj("# Table End\n",-1);
    Tcl_IncrRefCount(tmpobj);
    Tcl_WriteObj(channel,tmpobj);
    Tcl_DecrRefCount(tmpobj);
#else
    Oc_Snprintf(buf,sizeof(buf),"# Table End\n");
    Tcl_Write(channel, buf, int(strlen(buf)));
#endif
  }

  Tcl_Flush(channel);
  return TCL_OK;
}

#undef WMA_DEFAULT_NUM_FMT
#undef WMA_USE_OBJ

int
BitmapCmd(ClientData clientData, Tcl_Interp *interp,
          int argc, CONST84 char** argv)
{
  char c;
  const char* cptr;
  int margin, width, height, crop, isNew, mode;
  int xmin, ymin, xmax, ymax, xmarginadj, ymarginadj;
  int draw_boundary;
  OC_REAL8m boundary_width;
  OC_REAL8m mat_width=0.0;  // Mat width, in pixels
  Nb_DString mat_color = "0xFFFFFF";  // Default color is white
  OommfBitmap *bitmapPtr;
  Tcl_HashTable *tablePtr = (Tcl_HashTable *)clientData;
  Tcl_HashEntry *entryPtr;
  Tcl_Channel channel;

  OC_CHAR cmdbuf[MY_BUF_SIZE];

  if (argc < 2) {
    Oc_Snprintf(buf,sizeof(buf),
                "wrong # args: should be \"%s option bitmapName"
                " ?arg ...?\"", argv[0]);
    Tcl_AppendResult(interp, buf, (char *) NULL);
    return TCL_ERROR;
  }
  c = argv[1][0];
  if ((c == 'C') && (strcmp(argv[1], "Create") == 0)) {
    if (argc != 3) {
      Oc_Snprintf(buf,sizeof(buf),
                  "wrong # args: should be \"%s Create bitmapName\"",
                  argv[0]);
      Tcl_AppendResult(interp, buf, (char *) NULL);
      return TCL_ERROR;
    }
    entryPtr = Tcl_CreateHashEntry(tablePtr, argv[2], &isNew);
    if (!isNew) {
      Oc_Snprintf(buf,sizeof(buf),
                  "bitmap \"%s\" already exists", argv[2]);
      Tcl_AppendResult(interp, buf, (char *) NULL);
      return TCL_ERROR;
    }

    // Create a new OommfBitmap from the global DisplayFrame
    bitmapPtr = new OommfBitmap;

    margin = atoi(Tcl_GetVar2(interp, OC_CONST84_CHAR("plot_config"),
                              OC_CONST84_CHAR("misc,margin"),
                              TCL_GLOBAL_ONLY));
    width = atoi(Tcl_GetVar2(interp, OC_CONST84_CHAR("plot_config"),
                             OC_CONST84_CHAR("misc,width"),
                             TCL_GLOBAL_ONLY));
    height = atoi(Tcl_GetVar2(interp, OC_CONST84_CHAR("plot_config"),
                              OC_CONST84_CHAR("misc,height"),
                              TCL_GLOBAL_ONLY));
    crop = atoi(Tcl_GetVar2(interp, OC_CONST84_CHAR("plot_config"),
                            OC_CONST84_CHAR("misc,crop"),
                            TCL_GLOBAL_ONLY));

    cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                       OC_CONST84_CHAR("misc,drawboundary"),TCL_GLOBAL_ONLY);
    draw_boundary=1;
    if(!Nb_StrIsSpace(cptr)) draw_boundary = atoi(cptr);
    if(draw_boundary) myFrame.SetDrawBoundary(1);
    else              myFrame.SetDrawBoundary(0);

    OC_BOOL bw_error;
    cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                       OC_CONST84_CHAR("misc,boundarywidth"),TCL_GLOBAL_ONLY);
    boundary_width = Nb_Atof(cptr,bw_error);
    if(!bw_error) myFrame.SetBoundaryWidth(boundary_width);

    cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                       OC_CONST84_CHAR("misc,boundarycolor"),TCL_GLOBAL_ONLY);
    if(!Nb_StrIsSpace(cptr)) myFrame.SetBoundaryColor(cptr);

    cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                       OC_CONST84_CHAR("misc,boundarypos"),TCL_GLOBAL_ONLY);
    if(!Nb_StrIsSpace(cptr)) {
      if(Nb_StrCaseCmp(cptr,"back")==0) {
        myFrame.SetBoundaryOnTop(0);
      } else if(Nb_StrCaseCmp(cptr,"front")==0) {
        myFrame.SetBoundaryOnTop(1);
      } else {
        Oc_Snprintf(buf,sizeof(buf),
                    "Invalid plot_config(misc,boundarypos) value:"
                    " \"%s\"; should be either \"front\" or \"back\"",
                    cptr);
        Tcl_AppendResult(interp, buf, (char *) NULL);
        return TCL_ERROR;
      }
    }

    OC_BOOL mw_error;
    cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                       OC_CONST84_CHAR("misc,matwidth"),TCL_GLOBAL_ONLY);
    OC_REAL8m mw_temp = Nb_Atof(cptr,mw_error);
    if(!mw_error) mat_width = mw_temp; // Default is 0.0
    cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                       OC_CONST84_CHAR("misc,matcolor"),TCL_GLOBAL_ONLY);
    if(!Nb_StrIsSpace(cptr)) {
      mat_color = cptr;   // Default is OxFFFFFF (white)
    }

    // Mesh box
    Nb_Vec3<OC_REAL8> minpt, maxpt;
    Nb_BoundingBox<OC_REAL8> meshbox;
    myMeshArray[activeMeshId]->GetPreciseRange(meshbox);
    meshbox.GetExtremes(minpt,maxpt);

    OC_REAL8 default_span = maxpt.z-minpt.z;
    if(IsRectangularMesh(myMeshArray[activeMeshId])) {
      Nb_Vec3<OC_REAL4> celldim
        = myMeshArray[activeMeshId]->GetApproximateCellDimensions();
      default_span = celldim.z;
      if(default_span<=0.0) default_span=1.0; // Safety
      if(default_span>maxpt.z-minpt.z) {
        default_span = maxpt.z-minpt.z;
      } else if(20*default_span<maxpt.z-minpt.z) {
        default_span = (maxpt.z-minpt.z)/20.;
      }
    }

    // Set up slice selection
    char viewaxis = 'z';
    char viewdir[3] = "+z";
    OC_BOOL negative_viewdir = 0;
    OC_BOOL arrowspan_error,pixelspan_error;
    OC_REAL8m arrowspan,pixelspan;
    cptr = Tcl_GetVar2(interp, OC_CONST84_CHAR("plot_config"),
                       OC_CONST84_CHAR("viewaxis"), TCL_GLOBAL_ONLY);
    if(!Nb_StrIsSpace(cptr)) {
      size_t strsize = strlen(cptr);
      if(strsize==1) {
        viewaxis = cptr[0];
        viewdir[0] = '+'; viewdir[1]=viewaxis; viewdir[2]='\0';
      } else if(strsize==2) {
        viewaxis = cptr[1];
        strcpy(viewdir,cptr);
        if(cptr[0]=='-') {
          negative_viewdir = 1; // Viewing from the backside
        }
      } else {
        Oc_Snprintf(buf,sizeof(buf),
                    "Invalid plot_config(viewaxis) string : \"%s\"",
                    cptr);
        Tcl_AppendResult(interp, buf, (char *) NULL);
        return TCL_ERROR;
      }
    }
    if(viewaxis!='x' && viewaxis!='y' && viewaxis!='z') {
      Oc_Snprintf(buf,sizeof(buf),
                  "Invalid viewaxis detected: %c; Check setting of"
                  " plot_config(viewaxis)",viewaxis);
      Tcl_AppendResult(interp, buf, (char *) NULL);
      return TCL_ERROR;
    }

    OC_BOOL centerpt_set = 0;
    Nb_Vec3<OC_REAL8m> centerpt;
    cptr = Tcl_GetVar2(interp, OC_CONST84_CHAR("plot_config"),
                       OC_CONST84_CHAR("misc,centerpt"), TCL_GLOBAL_ONLY);
    if(!Nb_StrIsSpace(cptr)) {
      if(centerpt.Set(cptr)!=0) {
        Oc_Snprintf(buf,sizeof(buf),
                    "Error processing plot_config(misc,centerpt): %s",
                    cptr);
        Tcl_AppendResult(interp, buf, (char *) NULL);
        return TCL_ERROR;
      }
      centerpt_set = 1;
    } else {
      cptr = Tcl_GetVar2(interp, OC_CONST84_CHAR("plot_config"),
                         OC_CONST84_CHAR("misc,relcenterpt"),
                         TCL_GLOBAL_ONLY);
      if(!Nb_StrIsSpace(cptr)) {
        if(centerpt.Set(cptr)!=0) {
          Oc_Snprintf(buf,sizeof(buf),
                      "Error processing plot_config(misc,relcenterpt): %s",
                      cptr);
          Tcl_AppendResult(interp, buf, (char *) NULL);
          return TCL_ERROR;
        }
        // Convert mesh range from viewaxis to standard coords
        Nb_Vec3<OC_REAL8> scmin,scmax;
        Oc_Snprintf(cmdbuf,sizeof(cmdbuf),
                    "ApplyAxisTransform %s +z %.17g %.17g %.17g",viewdir,
                    static_cast<double>(minpt.x),
                    static_cast<double>(minpt.y),
                    static_cast<double>(minpt.z));
        Tcl_SavedResult saved;
        Tcl_SaveResult(interp, &saved);
        int error_code=Tcl_Eval(interp,cmdbuf);
        if(error_code!=TCL_OK) {
          Tcl_DiscardResult(&saved);
          Oc_Snprintf(buf,sizeof(buf),"Error processing minpt: %s",
                      cmdbuf);
          Tcl_AppendResult(interp, buf, (char *) NULL);
          return error_code;
        }
        if(scmin.Set(Tcl_GetStringResult(interp))!=0) {
          Tcl_RestoreResult(interp, &saved);
          Oc_Snprintf(buf,sizeof(buf),"Error reading minpt: %s",
                      cmdbuf);
          Tcl_AppendResult(interp, buf, (char *) NULL);
          return TCL_ERROR;
        }
        Oc_Snprintf(cmdbuf,sizeof(cmdbuf),
                    "ApplyAxisTransform %s +z %.17g %.17g %.17g",
                    viewdir,static_cast<double>(maxpt.x),
                    static_cast<double>(maxpt.y),
                    static_cast<double>(maxpt.z));
        error_code=Tcl_Eval(interp,cmdbuf);
        if(error_code!=TCL_OK) {
          Tcl_DiscardResult(&saved);
          Oc_Snprintf(buf,sizeof(buf),"Error processing maxpt: %s",
                      cmdbuf);
          Tcl_AppendResult(interp, buf, (char *) NULL);
          return error_code;
        }
        if(scmax.Set(Tcl_GetStringResult(interp))!=0) {
          Tcl_RestoreResult(interp, &saved);
          Oc_Snprintf(buf,sizeof(buf),"Error reading maxpt: %s",
                      cmdbuf);
          Tcl_AppendResult(interp, buf, (char *) NULL);
          return TCL_ERROR;
        }
        Tcl_RestoreResult(interp, &saved);

        // Convert from relative to absolute problem coordinates
        if(negative_viewdir) {
          Nb_Vec3<OC_REAL8m> tmpvec;
          tmpvec = scmin;  scmin = scmax;  scmax = tmpvec;
        }
        centerpt.x = (1-centerpt.x)*scmin.x + centerpt.x*scmax.x;
        centerpt.y = (1-centerpt.y)*scmin.y + centerpt.y*scmax.y;
        centerpt.z = (1-centerpt.z)*scmin.z + centerpt.z*scmax.z;
        centerpt_set = 1;
      }
    }

    if(centerpt_set) {
      // Convert from problem to viewaxis coords
      Oc_Snprintf(cmdbuf,sizeof(cmdbuf),
                  "ApplyAxisTransform +z %s %.17g %.17g %.17g",viewdir,
                  static_cast<double>(centerpt.x),
                  static_cast<double>(centerpt.y),
                  static_cast<double>(centerpt.z));
      Tcl_SavedResult saved;
      Tcl_SaveResult(interp, &saved);
      int error_code=Tcl_Eval(interp,cmdbuf);
      if(error_code!=TCL_OK) {
        Tcl_DiscardResult(&saved);
        Oc_Snprintf(buf,sizeof(buf),"Error processing centerpt: %s",
                    cmdbuf);
        Tcl_AppendResult(interp, buf, (char *) NULL);
        return error_code;
      }
      if(centerpt.Set(Tcl_GetStringResult(interp))!=0) {
        Tcl_RestoreResult(interp, &saved);
        Oc_Snprintf(buf,sizeof(buf),"Error reading centerpt: %s",
                    cmdbuf);
        Tcl_AppendResult(interp, buf, (char *) NULL);
        return TCL_ERROR;
      }
      Tcl_RestoreResult(interp, &saved);
    }

    Oc_Snprintf(buf,sizeof(buf),"viewaxis,%carrowspan",viewaxis);
    cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                       buf,TCL_GLOBAL_ONLY);
    arrowspan = Nb_Atof(cptr,arrowspan_error);
    if(arrowspan_error || arrowspan==0) {
      arrowspan = default_span;
    } else if(arrowspan<0.0) {
      arrowspan = (maxpt.z-minpt.z);
    }

    Oc_Snprintf(buf,sizeof(buf),"viewaxis,%cpixelspan",viewaxis);
    cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                       buf,TCL_GLOBAL_ONLY);
    pixelspan = Nb_Atof(cptr,pixelspan_error);
    if(pixelspan_error || pixelspan==0) {
      pixelspan = default_span;
    } else if(arrowspan<0.0) {
      pixelspan = (maxpt.z-minpt.z);
    }

    // Calculate bounding boxes, in mesh coords.  Use Zslice interface
    // to better mimic mmDisp.  (Do we always want to mimic mmDisp?
    // Probably doesn't matter for rectangular grids, but may for
    // irregular grids.)
    Nb_Vec3<OC_REAL4> temp_min,temp_max;
    Convert(minpt,temp_min);  Convert(maxpt,temp_max);
    myMeshArray[activeMeshId]->GetZslice(centerpt.z - arrowspan/2,
                                         centerpt.z + arrowspan/2,
                                         temp_min.z,temp_max.z);
    Nb_BoundingBox<OC_REAL4> arrow_box(temp_min,temp_max);
    myMeshArray[activeMeshId]->GetZslice(centerpt.z - pixelspan/2,
                                         centerpt.z + pixelspan/2,
                                         temp_min.z,temp_max.z);
    Nb_BoundingBox<OC_REAL4> pixel_box(temp_min,temp_max);

    // Setup bitmap
    Nb_Vec3<OC_REAL4> disp_minpt,disp_maxpt;
    Nb_BoundingBox<OC_REAL4> bbox;
    bbox = myFrame.GetDisplayBox();
    bbox.GetExtremes(disp_minpt, disp_maxpt);

    // If centerpt is specified, then pan bitmap window
    OC_REAL4 xadj=0.0,yadj=0.0;
    if(centerpt_set) {
      Nb_Vec3<OC_REAL4> view_center,display_center;
      Convert(centerpt,view_center);
      display_center.x = static_cast<OC_REAL4>((minpt.x+maxpt.x)/2.);
      display_center.y = static_cast<OC_REAL4>((minpt.y+maxpt.y)/2.);
      display_center.z = static_cast<OC_REAL4>((minpt.z+maxpt.z)/2.);
      myFrame.CoordinatePointTransform(CS_CALCULATION_STANDARD,
                                       myFrame.GetCoordinates(),
                                       view_center);
      myFrame.CoordinatePointTransform(CS_CALCULATION_STANDARD,
                                       myFrame.GetCoordinates(),
                                       display_center);
      xadj = view_center.x-display_center.x;  // Pan amount
      yadj = view_center.y-display_center.y;
    }
    xmin = int(floor(disp_minpt.x+xadj)) - margin;
    ymin = int(floor(disp_minpt.y+yadj)) - margin;
    xmax = int(ceil(disp_maxpt.x+xadj))  + margin;
    ymax = int(ceil(disp_maxpt.y+yadj))  + margin;

    // Check size
    xmarginadj = (xmax - xmin + 1) - width;
    ymarginadj = (ymax - ymin + 1) - height;
    if(xmarginadj>0) {
      // Resulting size is bigger than requested. Trim.
      xmin += int(floor(xmarginadj/2.));
      xmax -= int(ceil(xmarginadj/2.));
    }
    if(ymarginadj>0) {
      // Resulting size is bigger than requested. Trim.
      ymin += int(floor(ymarginadj/2.));
      ymax -= int(ceil(ymarginadj/2.));
    }
    if(!crop) {
      // No-crop request, so make bitmap exact size requested
      xmax = xmin + width - 1;
      ymax = ymin + height - 1;
    }

    bitmapPtr->Setup(xmin,ymin,xmax,ymax);

    // Fill bitmap
    myFrame.FillBitmap(arrow_box,pixel_box,negative_viewdir,*bitmapPtr);

    // Add mat, if requested
    if(mat_width>0) {
      OommfPackedRGB color(mat_color.GetStr());
      bitmapPtr->AddMat(mat_width,color);
    }

    Tcl_SetHashValue(entryPtr, (ClientData)bitmapPtr);
    return TCL_OK;
  }
  if ((c == 'D') && (strcmp(argv[1], "Delete") == 0)) {
    if (argc != 3) {
      Oc_Snprintf(buf,sizeof(buf),
                  "wrong # args: should be \"%s Delete bitmapName\"",
                  argv[0]);
      Tcl_AppendResult(interp, buf, (char *) NULL);
      return TCL_ERROR;
    }
    entryPtr = Tcl_FindHashEntry(tablePtr, argv[2]);
    if (entryPtr == NULL) {
      Oc_Snprintf(buf,sizeof(buf), "can't find bitmap \"%s\"",
                  argv[2]);
      Tcl_AppendResult(interp, buf, (char *) NULL);
      return TCL_ERROR;
    }
    delete (OommfBitmap *) Tcl_GetHashValue(entryPtr);
    Tcl_DeleteHashEntry(entryPtr);
    return TCL_OK;
  }
  if ((c == 'W') && (strcmp(argv[1], "Write") == 0)) {
    if (argc != 5) {
      Oc_Snprintf(buf,sizeof(buf), "wrong # args: should be "
                  "\"%s Write bitmapName format channel\"", argv[0]);
      Tcl_AppendResult(interp, buf, (char *) NULL);
      return TCL_ERROR;
    }
    if ((channel = Tcl_GetChannel(interp, argv[4], &mode)) == NULL) {
      return TCL_ERROR;
    }
    if (!(mode & TCL_WRITABLE)) {
      Oc_Snprintf(buf,sizeof(buf),
                  "%s is not a writable channel", argv[4]);
      Tcl_AppendResult(interp, buf, (char *) NULL);
      return TCL_ERROR;
    }
    entryPtr = Tcl_FindHashEntry(tablePtr, argv[2]);
    if (entryPtr == NULL) {
      Oc_Snprintf(buf,sizeof(buf),
                  "can't find bitmap \"%s\"", argv[2]);
      Tcl_AppendResult(interp, buf, (char *) NULL);
      return TCL_ERROR;
    }
    bitmapPtr = (OommfBitmap *) Tcl_GetHashValue(entryPtr);
    if ( strcmp("P3", argv[3]) == 0) {
      bitmapPtr->WritePPMChannel(channel, 3);
      return TCL_OK;
    }
    if ( strcmp("P6", argv[3]) == 0) {
      bitmapPtr->WritePPMChannel(channel, 6);
      return TCL_OK;
    }
    if ( strcmp("B24", argv[3]) == 0) {
      bitmapPtr->WriteBMPChannel(channel, 24);
      return TCL_OK;
    }
    Oc_Snprintf(buf,sizeof(buf),
                "ERROR: Unknown file format request: %s\n",argv[3]);
    Tcl_AppendResult(interp, buf, (char *) NULL);
    return TCL_ERROR;
  }
  Oc_Snprintf(buf,sizeof(buf),
              "bad option \"%s\": must be Create, Delete, or Write",
              argv[1]);
  Tcl_AppendResult(interp, buf, (char *) NULL);
  return TCL_ERROR;
}

void
BitmapDeleteProc(ClientData clientData)
{
  Tcl_HashTable *tablePtr = (Tcl_HashTable *) clientData;
  Tcl_HashEntry *entryPtr;
  Tcl_HashSearch searchId;
  OommfBitmap *bitmapPtr;
  for (entryPtr = Tcl_FirstHashEntry(tablePtr, &searchId);
       entryPtr != NULL; entryPtr = Tcl_NextHashEntry(&searchId)) {
    bitmapPtr = (OommfBitmap *) Tcl_GetHashValue(entryPtr);
    delete bitmapPtr;
  }
  Tcl_DeleteHashTable(tablePtr);
  delete tablePtr;
}

int PSWriteMesh(ClientData,Tcl_Interp *interp,
                int argc,CONST84 char** argv)
{
  const char* cptr;

  Tcl_ResetResult(interp);
  if(argc!=2) {
    Oc_Snprintf(buf,sizeof(buf),"PSWriteMesh must be called with"
            " 1 argument: channel"
            " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  int mode;
  Tcl_Channel channel;
  if ((channel = Tcl_GetChannel(interp, argv[1], &mode)) == NULL) {
    return TCL_ERROR;
  }
  if (!(mode & TCL_WRITABLE)) {
    Oc_Snprintf(buf,sizeof(buf),
                "%s is not a writable channel", argv[1]);
    Tcl_AppendResult(interp, buf, (char *) NULL);
    return TCL_ERROR;
  }

  OC_CHAR cmdbuf[MY_BUF_SIZE];

  int margin = atoi(Tcl_GetVar2(interp, OC_CONST84_CHAR("plot_config"),
                                OC_CONST84_CHAR("misc,margin"),
                                TCL_GLOBAL_ONLY));
  int width = atoi(Tcl_GetVar2(interp, OC_CONST84_CHAR("plot_config"),
                               OC_CONST84_CHAR("misc,width"),
                               TCL_GLOBAL_ONLY));
  if(width<1) width = 1;
  int height = atoi(Tcl_GetVar2(interp, OC_CONST84_CHAR("plot_config"),
                                OC_CONST84_CHAR("misc,height"),
                                TCL_GLOBAL_ONLY));
  if(height<1) height = 1;

  int croptoview = atoi(Tcl_GetVar2(interp, OC_CONST84_CHAR("print_config"),
                                    OC_CONST84_CHAR("croptoview"),
                                    TCL_GLOBAL_ONLY));
  int crop = atoi(Tcl_GetVar2(interp, OC_CONST84_CHAR("plot_config"),
                              OC_CONST84_CHAR("misc,crop"), TCL_GLOBAL_ONLY));
  // If plot_config(misc,crop) is true, then the print area (bounding box)
  // is cropped to fit the display margins.  This control is active when
  // the zoom value is small.
  //   Conversely, if print_config(croptoview) is true then the
  // specified pan (center offset) values are ignored, and the display
  // window is sized to wrap the mesh at the specified zoom, which is
  // then scaled to the print area accordingly.  "croptoview" is a
  // legacy value from the interactive print dialog, where it is used
  // primarily for overriding big zoom values.

  cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("print_config"),
                     OC_CONST84_CHAR("pwidth"),TCL_GLOBAL_ONLY);
  OC_REAL8m pwidth = Nb_Atof(cptr);
  cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("print_config"),
                     OC_CONST84_CHAR("pheight"),TCL_GLOBAL_ONLY);
  OC_REAL8m pheight = Nb_Atof(cptr);
  cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("print_config"),
                     OC_CONST84_CHAR("tmargin"),TCL_GLOBAL_ONLY);
  OC_REAL8m ptmargin = Nb_Atof(cptr);
  cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("print_config"),
                     OC_CONST84_CHAR("lmargin"),TCL_GLOBAL_ONLY);
  OC_REAL8m plmargin = Nb_Atof(cptr);
  cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("print_config"),
                     OC_CONST84_CHAR("units"),TCL_GLOBAL_ONLY);
  OC_REAL8m printscale = 72.; // Default is inches
  if(Nb_StrCaseCmp(cptr,"cm")==0) {
    printscale = 72./2.54;
  } else if(Nb_StrCaseCmp(cptr,"pt")==0) {
    printscale = 1.0;
  }
  pwidth *= printscale;
  pheight *= printscale;
  ptmargin *= printscale;
  plmargin *= printscale;

  // Determine page dimensions, in points (where 72 points = 1 inch)
  Nb_DString paper_type = Tcl_GetVar2(interp,OC_CONST84_CHAR("print_config"),
                                      OC_CONST84_CHAR("paper"),
                                      TCL_GLOBAL_ONLY);
  paper_type.Trim();  paper_type.ToLower();
  OC_REAL8m page_width  = 0.0, page_height = 0.0;
  if(strcmp("letter",paper_type.GetStr())==0) {
    page_width  = 72 *  8.5;
    page_height = 72 * 11;
  } else if(strcmp("legal",paper_type.GetStr())==0) {
    page_width  = 72 *  8.5;
    page_height = 72 * 14;
  } else if(strcmp("executive",paper_type.GetStr())==0) {
    page_width  = 72 *  7.5;
    page_height = 72 * 10;
  } else if(strcmp("tabloid",paper_type.GetStr())==0) {
    page_width  = 72 * 11;
    page_height = 72 * 17;
  } else if(strcmp("ledger",paper_type.GetStr())==0) {
    page_width  = 72 * 17;
    page_height = 72 * 11;
  } else if(strcmp("statement",paper_type.GetStr())==0) {
    page_width  = 72 * 5.5;
    page_height = 72 * 8.5;
  } else if(strcmp("folio",paper_type.GetStr())==0) {
    page_width  = 72 * 8.5;
    page_height = 72 * 13;
  } else if(strcmp("10x14",paper_type.GetStr())==0
            || strcmp("10 x 14",paper_type.GetStr())==0) {
    page_width  = 72 * 10;
    page_height = 72 * 14;
  } else if(strcmp("quarto",paper_type.GetStr())==0) {
    page_width  = 610;
    page_height = 780;
  } else if(paper_type[0]=='a') {
    OC_BOOL error;
    double n = Nb_Atof(paper_type.GetStr()+1,error);
    if(!error) {
      // The official A<N> sizes are fudged a bit from the simple
      // exponential formula to insure: (1) each paper dimension is
      // a whole number of millimeters, and (2) the width of each
      // paper size is not more than 1/2 the length of the next
      // larger size.  The "floor" and +0.22 fudge factor adjust
      // for these irregularities for N in the range [-2,10].
      page_width  = floor(1000.*(pow(2.,-0.5*n-0.25))+0.22)*72./25.4;
      page_height = floor(1000.*(pow(2.,-0.5*n+0.25))+0.22)*72./25.4;
    }
  } else if(paper_type[0]=='b') {
    OC_BOOL error;
    double n = Nb_Atof(paper_type.GetStr()+1,error);
    if(!error) {
      // Note 1: Comments above for A<N> sizes apply to B<N> sizes too.
      // Note 2: Ghostview appears to have wrong values for B4 and B5.
      //  The correct values for the ISO B<N> series, in points, are
      //  709x1001 and 499x709, respectively, where 1 inch == 72 points
      //  (aka a "PostScript point", as opposed to a "Printer's point",
      //  tpt, where 72.27 tpt = 1 inch).  Ghostview uses 729x1032 and
      //  516x729, respectively.  Further poking around hints that
      //  ghostview has both "B<n>" and "ISO B<n>", the latter agreeing
      //  with the values quoted above.  But I haven't been to find any
      //  other source listing the Ghostview B<n> values.
      page_width  = floor(1000.*pow(2.,-0.5*n))*72./25.4;
      page_height = floor(1000.*pow(2.,-0.5*n+0.5))*72./25.4;
    }
  }
  page_width  = OC_ROUND(page_width);
  page_height = OC_ROUND(page_height);
  if(page_width<=0.0 || page_height<=0.0) {
    Oc_Snprintf(buf,sizeof(buf),
                "Unrecognized paper request: %s",paper_type.GetStr());
    Tcl_AppendResult(interp, buf, (char *) NULL);
    return TCL_ERROR;
  }

  // Check orientation request
  Nb_DString page_orientation =
    Tcl_GetVar2(interp,OC_CONST84_CHAR("print_config"),
                OC_CONST84_CHAR("orient"),TCL_GLOBAL_ONLY);
  page_orientation.Trim(); page_orientation.ToLower();
  if(strcmp("portrait",page_orientation.GetStr())==0) {
    page_orientation = "Portrait";
  } else if(strcmp("landscape",page_orientation.GetStr())==0) {
    page_orientation = "Landscape";
  } else {
    Oc_Snprintf(buf,sizeof(buf),
                "Unrecognized orientation request: %s",
                page_orientation.GetStr());
    Tcl_AppendResult(interp, buf, (char *) NULL);
    return TCL_ERROR;
  }

  // Compute print page offsets, in points
  Nb_DString page_hpos =
    Tcl_GetVar2(interp,OC_CONST84_CHAR("print_config"),
                OC_CONST84_CHAR("hpos"),TCL_GLOBAL_ONLY);
  page_hpos.Trim(); page_hpos.ToLower();
  Nb_DString page_vpos =
    Tcl_GetVar2(interp,OC_CONST84_CHAR("print_config"),
                OC_CONST84_CHAR("vpos"),TCL_GLOBAL_ONLY);
  page_vpos.Trim(); page_vpos.ToLower();

  cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                     OC_CONST84_CHAR("misc,drawboundary"),TCL_GLOBAL_ONLY);
  int draw_boundary=1;
  if(!Nb_StrIsSpace(cptr)) draw_boundary = atoi(cptr);
  if(draw_boundary) myFrame.SetDrawBoundary(1);
  else              myFrame.SetDrawBoundary(0);

  OC_BOOL bw_error;
  cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                     OC_CONST84_CHAR("misc,boundarywidth"),TCL_GLOBAL_ONLY);
  OC_REAL8m boundary_width = Nb_Atof(cptr,bw_error);
  if(!bw_error) myFrame.SetBoundaryWidth(boundary_width);

  cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                     OC_CONST84_CHAR("misc,boundarycolor"),TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr)) myFrame.SetBoundaryColor(cptr);

  cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                     OC_CONST84_CHAR("misc,boundarypos"),TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr)) {
    if(Nb_StrCaseCmp(cptr,"back")==0) {
      myFrame.SetBoundaryOnTop(0);
    } else if(Nb_StrCaseCmp(cptr,"front")==0) {
      myFrame.SetBoundaryOnTop(1);
    } else {
      Oc_Snprintf(buf,sizeof(buf),
                  "Invalid plot_config(misc,boundarypos) value:"
                  " \"%s\"; should be either \"front\" or \"back\"",
                  cptr);
      Tcl_AppendResult(interp, buf, (char *) NULL);
      return TCL_ERROR;
    }
  }

  // Mat info
  OC_BOOL mw_error;
  OC_REAL8m mat_width = 0.0;
  Nb_DString mat_color = "0xFFFFFF";  // Default color is white
  cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                     OC_CONST84_CHAR("misc,matwidth"),TCL_GLOBAL_ONLY);
  OC_REAL8m mw_temp = Nb_Atof(cptr,mw_error);
  if(!mw_error) mat_width = mw_temp;
  cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                     OC_CONST84_CHAR("misc,matcolor"),TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr)) {
    mat_color = cptr;   // Default is 0xFFFFFF (white)
  }

  // Arrow outline parameters
  OC_BOOL ao_error;
  OC_REAL8m arrow_outline_width = 0.0;  // Default is no boundary
  Nb_DString arrow_outline_color = "0x000000";
  cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                     OC_CONST84_CHAR("arrow,outlinewidth"),TCL_GLOBAL_ONLY);
  OC_REAL8m ao_temp = Nb_Atof(cptr,ao_error);
  if(!ao_error) arrow_outline_width = ao_temp;
  cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                     OC_CONST84_CHAR("arrow,outlinecolor"),TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr)) {
    arrow_outline_color = cptr;   // Default is 0x000000 (black)
  }

  // Mesh box
  Nb_Vec3<OC_REAL8> minpt, maxpt;
  Nb_BoundingBox<OC_REAL8> meshbox;
  myMeshArray[activeMeshId]->GetPreciseRange(meshbox);
  meshbox.GetExtremes(minpt,maxpt);

  OC_REAL8 default_span = maxpt.z-minpt.z;
  if(IsRectangularMesh(myMeshArray[activeMeshId])) {
    Nb_Vec3<OC_REAL4> celldim
      = myMeshArray[activeMeshId]->GetApproximateCellDimensions();
    default_span = celldim.z;
    if(default_span<=0.0) default_span=1.0; // Safety
    if(default_span>maxpt.z-minpt.z) {
      default_span = maxpt.z-minpt.z;
    } else if(20*default_span<maxpt.z-minpt.z) {
      default_span = (maxpt.z-minpt.z)/20.;
    }
  }

  // Set up slice selection
  char viewaxis = 'z';
  char viewdir[3] = "+z";
  OC_BOOL negative_viewdir = 0;
  OC_BOOL arrowspan_error,pixelspan_error;
  OC_REAL8m arrowspan,pixelspan;
  cptr = Tcl_GetVar2(interp, OC_CONST84_CHAR("plot_config"),
                     OC_CONST84_CHAR("viewaxis"), TCL_GLOBAL_ONLY);
  if(!Nb_StrIsSpace(cptr)) {
    size_t strsize = strlen(cptr);
    if(strsize==1) {
      viewaxis = cptr[0];
      viewdir[0] = '+'; viewdir[1]=viewaxis; viewdir[2]='\0';
    } else if(strsize==2) {
      viewaxis = cptr[1];
      strcpy(viewdir,cptr);
      if(cptr[0]=='-') {
        negative_viewdir = 1; // Viewing from the backside
      }
    } else {
      Oc_Snprintf(buf,sizeof(buf),
                  "Invalid plot_config(viewaxis) string : \"%s\"",
                  cptr);
      Tcl_AppendResult(interp, buf, (char *) NULL);
      return TCL_ERROR;
    }
  }
  if(viewaxis!='x' && viewaxis!='y' && viewaxis!='z') {
    Oc_Snprintf(buf,sizeof(buf),
                "Invalid viewaxis detected: %c; Check setting of"
                " plot_config(viewaxis)",viewaxis);
    Tcl_AppendResult(interp, buf, (char *) NULL);
    return TCL_ERROR;
  }

  OC_BOOL centerpt_set = 0;
  Nb_Vec3<OC_REAL8m> centerpt;
  if(croptoview) {
    cptr = Tcl_GetVar2(interp, OC_CONST84_CHAR("plot_config"),
                       OC_CONST84_CHAR("misc,centerpt"), TCL_GLOBAL_ONLY);
    if(!Nb_StrIsSpace(cptr)) {
      if(centerpt.Set(cptr)!=0) {
        Oc_Snprintf(buf,sizeof(buf),
                    "Error processing plot_config(misc,centerpt): %s",
                    cptr);
        Tcl_AppendResult(interp, buf, (char *) NULL);
        return TCL_ERROR;
      }
      centerpt_set = 1;
    } else {
      cptr = Tcl_GetVar2(interp, OC_CONST84_CHAR("plot_config"),
                         OC_CONST84_CHAR("misc,relcenterpt"), TCL_GLOBAL_ONLY);
      if(!Nb_StrIsSpace(cptr)) {
        if(centerpt.Set(cptr)!=0) {
          Oc_Snprintf(buf,sizeof(buf),
                      "Error processing plot_config(misc,relcenterpt): %s",
                      cptr);
          Tcl_AppendResult(interp, buf, (char *) NULL);
          return TCL_ERROR;
        }
        // Convert mesh range from viewaxis to standard coords
        Nb_Vec3<OC_REAL8> scmin,scmax;
        Oc_Snprintf(cmdbuf,sizeof(cmdbuf),
                    "ApplyAxisTransform %s +z %.17g %.17g %.17g",
                    viewdir,static_cast<double>(minpt.x),
                    static_cast<double>(minpt.y),
                    static_cast<double>(minpt.z));
        Tcl_SavedResult saved;
        Tcl_SaveResult(interp, &saved);
        int error_code=Tcl_Eval(interp,cmdbuf);
        if(error_code!=TCL_OK) {
          Tcl_DiscardResult(&saved);
          Oc_Snprintf(buf,sizeof(buf),"Error processing minpt: %s",
                      cmdbuf);
          Tcl_AppendResult(interp, buf, (char *) NULL);
          return error_code;
        }
        if(scmin.Set(Tcl_GetStringResult(interp))!=0) {
          Tcl_RestoreResult(interp, &saved);
          Oc_Snprintf(buf,sizeof(buf),"Error reading minpt: %s",
                      cmdbuf);
          Tcl_AppendResult(interp, buf, (char *) NULL);
          return TCL_ERROR;
        }
        Oc_Snprintf(cmdbuf,sizeof(cmdbuf),
                    "ApplyAxisTransform %s +z %.17g %.17g %.17g",
                    viewdir,static_cast<double>(maxpt.x),
                    static_cast<double>(maxpt.y),
                    static_cast<double>(maxpt.z));
        error_code=Tcl_Eval(interp,cmdbuf);
        if(error_code!=TCL_OK) {
          Tcl_DiscardResult(&saved);
          Oc_Snprintf(buf,sizeof(buf),"Error processing maxpt: %s",
                      cmdbuf);
          Tcl_AppendResult(interp, buf, (char *) NULL);
          return error_code;
        }
        if(scmax.Set(Tcl_GetStringResult(interp))!=0) {
          Tcl_RestoreResult(interp, &saved);
          Oc_Snprintf(buf,sizeof(buf),"Error reading maxpt: %s",
                      cmdbuf);
          Tcl_AppendResult(interp, buf, (char *) NULL);
          return TCL_ERROR;
        }
        Tcl_RestoreResult(interp, &saved);

        // Convert from relative to absolute problem coordinates
        if(negative_viewdir) {
          Nb_Vec3<OC_REAL8m> tmpvec;
          tmpvec = scmin;  scmin = scmax;  scmax = tmpvec;
        }
        centerpt.x = (1-centerpt.x)*scmin.x + centerpt.x*scmax.x;
        centerpt.y = (1-centerpt.y)*scmin.y + centerpt.y*scmax.y;
        centerpt.z = (1-centerpt.z)*scmin.z + centerpt.z*scmax.z;
        centerpt_set = 1;
      }
    }
  }

  if(centerpt_set) {
    // Convert from problem to viewaxis coords
    Oc_Snprintf(cmdbuf,sizeof(cmdbuf),
                "ApplyAxisTransform +z %s %.17g %.17g %.17g",viewdir,
                static_cast<double>(centerpt.x),
                static_cast<double>(centerpt.y),
                static_cast<double>(centerpt.z));
    Tcl_SavedResult saved;
    Tcl_SaveResult(interp, &saved);
    int error_code=Tcl_Eval(interp,cmdbuf);
    if(error_code!=TCL_OK) {
      Tcl_DiscardResult(&saved);
      Oc_Snprintf(buf,sizeof(buf),"Error processing centerpt: %s",
                  cmdbuf);
      Tcl_AppendResult(interp, buf, (char *) NULL);
      return error_code;
    }
    if(centerpt.Set(Tcl_GetStringResult(interp))!=0) {
      Tcl_RestoreResult(interp, &saved);
      Oc_Snprintf(buf,sizeof(buf),"Error reading centerpt: %s",
                  cmdbuf);
      Tcl_AppendResult(interp, buf, (char *) NULL);
      return TCL_ERROR;
    }
    Tcl_RestoreResult(interp, &saved);
  }

  Oc_Snprintf(buf,sizeof(buf),"viewaxis,%carrowspan",viewaxis);
  cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                     buf,TCL_GLOBAL_ONLY);
  arrowspan = Nb_Atof(cptr,arrowspan_error);
  if(arrowspan_error || arrowspan==0) {
    arrowspan = default_span;
  } else if(arrowspan<0.0) {
    arrowspan = (maxpt.z-minpt.z);
  }

  Oc_Snprintf(buf,sizeof(buf),"viewaxis,%cpixelspan",viewaxis);
  cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("plot_config"),
                     buf,TCL_GLOBAL_ONLY);
  pixelspan = Nb_Atof(cptr,pixelspan_error);
  if(pixelspan_error || pixelspan==0) {
    pixelspan = default_span;
  } else if(arrowspan<0.0) {
    pixelspan = (maxpt.z-minpt.z);
  }

  // Setup image
  Nb_Vec3<OC_REAL4> disp_minpt,disp_maxpt;
  Nb_BoundingBox<OC_REAL4> bbox;
  bbox = myFrame.GetDisplayBox();
  bbox.GetExtremes(disp_minpt, disp_maxpt);

  // If centerpt is specified, then pan display window
  OC_REAL4 xadj=0.0,yadj=0.0;
  if(centerpt_set) {
    Nb_Vec3<OC_REAL4> view_center,display_center;
    Convert(centerpt,view_center);
    display_center.x = static_cast<OC_REAL4>((minpt.x+maxpt.x)/2.);
    display_center.y = static_cast<OC_REAL4>((minpt.y+maxpt.y)/2.);
    display_center.z = static_cast<OC_REAL4>((minpt.z+maxpt.z)/2.);
    myFrame.CoordinatePointTransform(CS_CALCULATION_STANDARD,
                                     myFrame.GetCoordinates(),
                                     view_center);
    myFrame.CoordinatePointTransform(CS_CALCULATION_STANDARD,
                                     myFrame.GetCoordinates(),
                                     display_center);
    xadj = view_center.x-display_center.x;  // Pan amount
    yadj = view_center.y-display_center.y;
  }
  OC_REAL8m xmin = disp_minpt.x + xadj - margin;
  OC_REAL8m ymin = disp_minpt.y + yadj - margin;
  OC_REAL8m xmax = disp_maxpt.x + xadj + margin;
  OC_REAL8m ymax = disp_maxpt.y + yadj + margin;

  // Check size
  if(croptoview) {
    // Adjust data size to match print size
    OC_REAL8m xmarginadj = (xmax - xmin + 1 - width)/2.;
    OC_REAL8m ymarginadj = (ymax - ymin + 1 - height)/2.;
    xmin += xmarginadj;
    xmax -= xmarginadj;
    ymin += ymarginadj;
    ymax -= ymarginadj;
    if(crop) {
      // Remove any slop
      if(xmin<disp_minpt.x-margin) xmin=disp_minpt.x-margin;
      if(ymin<disp_minpt.y-margin) ymin=disp_minpt.y-margin;
      if(xmax>disp_maxpt.x+margin) xmax=disp_maxpt.x+margin;
      if(ymax>disp_maxpt.y+margin) ymax=disp_maxpt.y+margin;
      OC_REAL8m newwidth  = xmax - xmin + 1;
      OC_REAL8m newheight = ymax - ymin + 1;
      width  = static_cast<int>(OC_ROUND(newwidth));
      height = static_cast<int>(OC_ROUND(newheight));

      OC_REAL8m xscale = pwidth/newwidth;
      OC_REAL8m yscale = pheight/newheight;
      if(xscale<yscale) {
        pheight = xscale*newheight;
      } else {
        pwidth = yscale*newwidth;
      }
    }
  }

  // Calculate bounding boxes, in mesh coords
  OC_REAL8m arrowsize = myFrame.GetArrowSize();
  Nb_Vec3<OC_REAL4> temp_min(static_cast<OC_REAL4>(xmin-arrowsize/2),
                          static_cast<OC_REAL4>(ymin-arrowsize/2),0.0f);
  Nb_Vec3<OC_REAL4> temp_max(static_cast<OC_REAL4>(xmax+arrowsize/2),
                          static_cast<OC_REAL4>(ymax+arrowsize/2),0.0f);
  myFrame.CoordinatePointTransform(myFrame.GetCoordinates(),
                                   CS_CALCULATION_STANDARD,
                                   temp_min);
  myFrame.CoordinatePointTransform(myFrame.GetCoordinates(),
                                   CS_CALCULATION_STANDARD,
                                   temp_max);
  if(temp_min.x>temp_max.x) {
    OC_REAL4 tx = temp_min.x ; temp_min.x=temp_max.x; temp_max.x=tx;
  }
  if(temp_min.y>temp_max.y) {
    OC_REAL4 ty = temp_min.y ; temp_min.y=temp_max.y; temp_max.y=ty;
  }

  // Compute zspan, using Zslice interface to better mimic mmDisp.
  // (Do we always want to mimic mmDisp?  Probably doesn't matter
  // for rectangular grids, but may for irregular grids.)
  myMeshArray[activeMeshId]->GetZslice(centerpt.z - arrowspan/2,
                                       centerpt.z + arrowspan/2,
                                       temp_min.z,temp_max.z);
  Nb_BoundingBox<OC_REAL4> arrow_box(temp_min,temp_max);

  Nb_Vec3<OC_REAL4> pixelsize = myFrame.GetPixelDimensions();
  temp_min.Set(static_cast<OC_REAL4>(xmin-pixelsize.x/2),
               static_cast<OC_REAL4>(ymin-pixelsize.y/2),0.0f);
  temp_max.Set(static_cast<OC_REAL4>(xmax+pixelsize.x/2),
               static_cast<OC_REAL4>(ymax+pixelsize.y/2),0.0f);
  myFrame.CoordinatePointTransform(myFrame.GetCoordinates(),
                                   CS_CALCULATION_STANDARD,
                                   temp_min);
  myFrame.CoordinatePointTransform(myFrame.GetCoordinates(),
                                   CS_CALCULATION_STANDARD,
                                   temp_max);
  if(temp_min.x>temp_max.x) {
    OC_REAL4 tx = temp_min.x ; temp_min.x=temp_max.x; temp_max.x=tx;
  }
  if(temp_min.y>temp_max.y) {
    OC_REAL4 ty = temp_min.y ; temp_min.y=temp_max.y; temp_max.y=ty;
  }

  // Compute zspan, using Zslice interface to better mimic mmDisp.
  // (Do we always want to mimic mmDisp?  Probably doesn't matter
  // for rectangular grids, but may for irregular grids.)
  myMeshArray[activeMeshId]->GetZslice(centerpt.z - pixelspan/2,
                                       centerpt.z + pixelspan/2,
                                       temp_min.z,temp_max.z);
  Nb_BoundingBox<OC_REAL4> pixel_box(temp_min,temp_max);

  // Compute print offset
  OC_REAL8m pxoff, pyoff;
  if(strcmp("Landscape",page_orientation.GetStr())==0) {
    // Landscape orientation
    pxoff = (page_width - pheight)/2.0;
    pyoff = (page_height - pwidth)/2.0;
    if(strcmp("top",page_vpos.GetStr())==0) {
      pxoff = ptmargin;
    } else if(strcmp("bottom",page_vpos.GetStr())==0) {
      pxoff = page_width - ptmargin - pheight;
    }
    if(strcmp("left",page_hpos.GetStr())==0) {
      pyoff = plmargin;
    } else if(strcmp("right",page_hpos.GetStr())==0) {
      pyoff = page_height - plmargin - pwidth;
    }
  } else {
    // Portrait orientation
    pxoff = (page_width - pwidth)/2.0;
    pyoff = (page_height - pheight)/2.0;
    if(strcmp("left",page_hpos.GetStr())==0) {
      pxoff = plmargin;
    } else if(strcmp("right",page_hpos.GetStr())==0) {
      pxoff = page_width - plmargin - pwidth;
    }
    if(strcmp("bottom",page_vpos.GetStr())==0) {
      pyoff = ptmargin;
    } else if(strcmp("top",page_vpos.GetStr())==0) {
      pyoff = page_height - ptmargin - pheight;
    }
  }

  // Write PostScript
  pxoff = OC_ROUND(pxoff);
  pyoff = OC_ROUND(pyoff);
  myFrame.PSDump(channel,
                 pxoff,pyoff,
                 pwidth,pheight,
                 page_orientation.GetStr(),
                 xmin,ymin,xmax,ymax,
                 arrow_box,pixel_box,negative_viewdir,
                 mat_width,mat_color,
                 arrow_outline_width,arrow_outline_color);

  return TCL_OK;
}

#if USE_TCL_COMMAND_CLEANUP_INFO
struct MmdispcmdsCleanupInfo {
public:
  Tcl_Interp* interp;
  Tcl_Command bitmap_cmd_token;
} mmdispcmds_cleanup_info;
#endif

static void Mmdispcmds_Cleanup
#if USE_TCL_COMMAND_CLEANUP_INFO
(ClientData cd)
#else
(ClientData)
#endif
{
#if USE_TCL_COMMAND_CLEANUP_INFO
  MmdispcmdsCleanupInfo* info = static_cast<MmdispcmdsCleanupInfo*>(cd);
  if(info->bitmap_cmd_token != 0) {
    Tcl_DeleteCommandFromToken(info->interp,info->bitmap_cmd_token);
    info->bitmap_cmd_token = 0; // Safety
  }
#endif
  myFrame.SetMesh(NULL);
  for(int i=0;i<MY_MESH_ARRAY_SIZE;i++) {
    if (myMeshArray[i]!=NULL) {
      delete myMeshArray[i];
      myMeshArray[i]=NULL;  // Safety
    }
  }
  activeMeshId=0;  // Safety
}

int Mmdispcmds_Init(Tcl_Interp *interp)
{
#define RETURN_TCL_ERROR                                                \
    Tcl_AddErrorInfo(interp,                                            \
                  OC_CONST84_CHAR("\n    (in Mmdispcmds_Init())"));     \
    return TCL_ERROR

  Tcl_HashTable *bitmapTablePtr;

/*
 * Need at least Oc 1.1.1.2 to get Oc_Snprintf()
 */
    if (Tcl_PkgRequire(interp, OC_CONST84_CHAR("Oc"),
                       OC_CONST84_CHAR("2"), 0) == NULL) {
        Tcl_AppendResult(interp,
                Oc_AutoBuf("\n\t(Mmdispcmds " MMDISPCMDS_VERSION
                           " needs Oc 2)").GetStr(), NULL);
        RETURN_TCL_ERROR;
    }

/*
 * Need at least Nb 1.2.0.4 to get Nb_GetColor
 */
    if (Tcl_PkgRequire(interp, OC_CONST84_CHAR("Nb"),
                       OC_CONST84_CHAR("2"), 0) == NULL) {
        Tcl_AppendResult(interp,
                Oc_AutoBuf("\n\t(Mmdispcmds " MMDISPCMDS_VERSION
                           " needs Nb 2)").GetStr(), NULL);
        RETURN_TCL_ERROR;
    }

/*
 * Need at least Vf 1.2.0.4 to get WriteMesh routines that take Tcl_Channel
 */
    if (Tcl_PkgRequire(interp, OC_CONST84_CHAR("Vf"),
                       OC_CONST84_CHAR("2"), 0) == NULL) {
        Tcl_AppendResult(interp,
                Oc_AutoBuf("\n\t(Mmdispcmds " MMDISPCMDS_VERSION
                           " needs Vf 2)").GetStr(), NULL);
        RETURN_TCL_ERROR;
    }

  // Record compile and run-time patch levels

  // Initialize C++ data structures
  for(int id=0;id<MY_MESH_ARRAY_SIZE;id++) {
    myMeshArray[id] = new Vf_EmptyMesh();
  }
  activeMeshId=0;
  myFrame.SetTclInterp(interp);
  myFrame.SetMesh(myMeshArray[activeMeshId]);

  // Register C/C++ routines with interpreter
  Oc_RegisterCommand(interp,"ChangeMesh",ChangeMesh);
  Oc_RegisterCommand(interp,"CopyMesh",CopyMesh);
  Oc_RegisterCommand(interp,"CrossProductMesh",CrossProductMesh);
  Oc_RegisterCommand(interp,"DifferenceMesh",DifferenceMesh);
  Oc_RegisterCommand(interp,"DrawFrame",DrawFrame);
  Oc_RegisterCommand(interp,"FindMeshVector",FindMeshVector);
  Oc_RegisterCommand(interp,"FreeMesh",FreeMesh);
  Oc_RegisterCommand(interp,"GetAutosamplingRates",GetAutosamplingRates);
  Oc_RegisterCommand(interp,"GetDataValueUnit",GetDataValueUnit);
  Oc_RegisterCommand(interp,"GetDataValueScaling",GetDataValueScaling);
  Oc_RegisterCommand(interp,"GetDefaultColorMapList",GetDefaultColorMapList);
  Oc_RegisterCommand(interp,"GetFrameBox",GetFrameBox);
  Oc_RegisterCommand(interp,"GetFrameRotation",GetFrameRotation);
  Oc_RegisterCommand(interp,"GetZoom",GetZoom);
  Oc_RegisterCommand(interp,"GetMeshSize",GetMeshSize);
  Oc_RegisterCommand(interp,"GetMeshCellSize",GetMeshCellSize);
  Oc_RegisterCommand(interp,"GetMeshCoordinates",GetMeshCoordinates);
  Oc_RegisterCommand(interp,"GetDisplayCoordinates",GetDisplayCoordinates);
  Oc_RegisterCommand(interp,"GetMeshDescription",GetMeshDescription);
  Oc_RegisterCommand(interp,"GetMeshIncrement",GetMeshIncrement);
  Oc_RegisterCommand(interp,"GetMeshName",GetMeshName);
  Oc_RegisterCommand(interp,"GetMeshRange",GetMeshRange);
  Oc_RegisterCommand(interp,"GetMeshSpatialUnitString",
                     GetMeshSpatialUnitString);
  Oc_RegisterCommand(interp,"GetMeshStructureInfo",GetMeshStructureInfo);
  Oc_RegisterCommand(interp,"GetMeshTitle",GetMeshTitle);
  Oc_RegisterCommand(interp,"GetMeshType",GetMeshType);
  Oc_RegisterCommand(interp,"GetMeshValueMagSpan",GetMeshValueMagSpan);
  Oc_RegisterCommand(interp,"GetMeshValueMean",GetMeshValueMean);
  Oc_RegisterCommand(interp,"GetMeshValueRMS",GetMeshValueRMS);
  Oc_RegisterCommand(interp,"GetMeshValueL1",GetMeshValueL1);
  Oc_RegisterCommand(interp,"GetMeshValueUnit",GetMeshValueUnit);
  Oc_RegisterCommand(interp,"GetMeshZRange",GetMeshZRange);
  Oc_RegisterCommand(interp,"GetVecColor",GetVecColor);
  Oc_RegisterCommand(interp,"GetZsliceCount",GetZsliceCount);
  Oc_RegisterCommand(interp,"GetZsliceLevels",GetZsliceLevels);
  Oc_RegisterCommand(interp,"IsRectangularMesh",IsRectangularMesh);
  Oc_RegisterCommand(interp,"PeriodicTranslate",PeriodicTranslate);
  Oc_RegisterCommand(interp,"Resample",Resample);
  Oc_RegisterCommand(interp,"ResampleAverage",ResampleAverage);
  Oc_RegisterCommand(interp,"PSWriteMesh",PSWriteMesh);
  Oc_RegisterCommand(interp,"ReportActiveMesh",ReportActiveMesh);
  Oc_RegisterCommand(interp,"SelectActiveMesh",SelectActiveMesh);
  Oc_RegisterCommand(interp,"SetDataValueScaling",SetDataValueScaling);
  Oc_RegisterCommand(interp,"SetFrameRotation",SetFrameRotation);
  Oc_RegisterCommand(interp,"SetZoom",SetZoom);
  Oc_RegisterCommand(interp,"SetMeshTitle",SetMeshTitle);
  Oc_RegisterCommand(interp,"UpdatePlotConfiguration",
                     UpdatePlotConfiguration);
  Oc_RegisterCommand(interp,"WriteMesh",WriteMesh);
  Oc_RegisterCommand(interp,"WriteMeshUsingDeprecatedVIOFormat",
                     WriteMeshUsingDeprecatedVIOFormat);
  Oc_RegisterCommand(interp,"WriteMeshOVF2",WriteMeshOVF2);
  Oc_RegisterCommand(interp,"WriteMeshNPY",WriteMeshNPY);
  Oc_RegisterCommand(interp,"WriteMeshMagnitudes",WriteMeshMagnitudes);
  Oc_RegisterCommand(interp,"WriteMeshAverages",WriteMeshAverages);

  bitmapTablePtr = new Tcl_HashTable;
  Tcl_InitHashTable(bitmapTablePtr, TCL_STRING_KEYS);

#if USE_TCL_COMMAND_CLEANUP_INFO
  mmdispcmds_cleanup_info.bitmap_cmd_token
    = Tcl_CreateCommand(interp, OC_CONST84_CHAR("Bitmap"),BitmapCmd,
                        (ClientData) bitmapTablePtr,BitmapDeleteProc);
  mmdispcmds_cleanup_info.interp = interp;
  Tcl_CreateExitHandler((Tcl_ExitProc *) Mmdispcmds_Cleanup,
                        (ClientData) &mmdispcmds_cleanup_info);
#else
  Tcl_CreateCommand(interp, OC_CONST84_CHAR("Bitmap"),BitmapCmd,
                    (ClientData) bitmapTablePtr,BitmapDeleteProc);
  Tcl_CreateExitHandler((Tcl_ExitProc *) Mmdispcmds_Cleanup, 0);
#endif

  if (Tcl_PkgProvide(interp, OC_CONST84_CHAR("Mmdispcmds"),
                     OC_CONST84_CHAR(MMDISPCMDS_VERSION))
      != TCL_OK) {
    RETURN_TCL_ERROR;
  }
  if (Oc_InitScript(interp, "Mmdispcmds", MMDISPCMDS_VERSION) != TCL_OK) {
    RETURN_TCL_ERROR;
  }
  return TCL_OK;

#undef RETURN_TCL_ERROR

}
