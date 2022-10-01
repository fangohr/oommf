/* FILE: display.cc                 -*-Mode: c++-*-
 *
 * C++ code for display windows
 *
 */

#include <cstring>

#include "display.h"
#include "psdraw.h"
#include "planepoint.h"

/* End includes */     // This is an optional directive to build.tcl,
                       // that excludes the remainder of the file from
                       // '#include ".*"' dependency building

namespace { // unnamed namespace. Utility routines confined to this file
  inline OC_REAL4m PickMedian(Nb_Vec3<OC_REAL4>& v)
  { // Returns the middle (median) component value
    return (v.x<=v.y ?
            /* v.x<=v.y */ (v.y<=v.z ? v.y : (v.x<=v.z ? v.z : v.x)) :
            /* v.x >v.y */ (v.x<=v.z ? v.x : (v.y<=v.z ? v.z : v.y)));
  }
  inline OC_REAL4m PickMinimum(Nb_Vec3<OC_REAL4>& v)
  { // Returns the smallest component value
    return (v.x<=v.y ?
            /* v.x<=v.y */ (v.x<=v.z ? v.x : v.z) :
            /* v.x >v.y */ (v.y<=v.z ? v.y : v.z));
  }
}

//////////////////////////////////////////////////////////////////////////
// PlotConfiguration structure
//
const ClassDoc PlotConfiguration::class_doc("PlotConfiguration",
                    "Michael J. Donahue (michael.donahue@nist.gov)",
                    "1.0.1","16-Jul-1998");
PlotConfiguration::PlotConfiguration()
  : displaystate(1),autosample(0),subsample(0.),size(1.),viewscale(1),
    antialias(0),outlinewidth(0),stipple(""),phase(0.),invert(0)
{}

PlotConfiguration& PlotConfiguration::operator=(const PlotConfiguration& pc)
{
  displaystate=pc.displaystate;
  autosample=pc.autosample;
  subsample=pc.subsample;
  size=pc.size;
  viewscale=pc.viewscale;
  antialias=pc.antialias;
  outlinewidth=pc.outlinewidth;
  outlinecolor=pc.outlinecolor;
  stipple=pc.stipple;
  colorquantity=pc.colorquantity;
  phase=pc.phase;
  invert=pc.invert;
  colormap=pc.colormap;
  return *this;
}

PlotConfiguration::PlotConfiguration(const PlotConfiguration& pc)
  : displaystate(pc.displaystate),
    autosample(pc.autosample),subsample(pc.subsample),
    size(pc.size),viewscale(pc.viewscale),
    antialias(pc.antialias),
    outlinewidth(pc.outlinewidth),outlinecolor(pc.outlinecolor),
    stipple(pc.stipple),
    colorquantity(pc.colorquantity),phase(pc.phase),invert(pc.invert),
    colormap(pc.colormap)
{}

//////////////////////////////////////////////////////////////////////////
// CoordinateSystem helper function
OC_INT4m CoordinateAngleDifference(CoordinateSystem cs1,CoordinateSystem cs2,
                                OC_REAL4m &angle)
{ // Calculates the difference (in degrees, from 0 to 360) of cs1 - cs2, if
  // both are CS_DISPLAY_* coordinates, sets the difference in 'angle' and
  // returns 0;  If cs1 is not a CS_DISPLAY_* c.s., then the return value
  // is 1; if cs2 is not a CS_DISPLAY_* c.s., then the return value is 2.
  // In these latter two cases angle is 0.0 on return.
  OC_REAL4m ang1,ang2;
  angle=0.0;
  switch(cs1) {
  case CS_DISPLAY_STANDARD: ang1=  0; break;
  case CS_DISPLAY_ROT_90:   ang1= 90; break;
  case CS_DISPLAY_ROT_180:  ang1=180; break;
  case CS_DISPLAY_ROT_270:  ang1=270; break;
  default: return 1;  // Illegal coordinates
    // break;
  }
  switch(cs2) {
  case CS_DISPLAY_STANDARD: ang2=  0; break;
  case CS_DISPLAY_ROT_90:   ang2= 90; break;
  case CS_DISPLAY_ROT_180:  ang2=180; break;
  case CS_DISPLAY_ROT_270:  ang2=270; break;
  default: return 2;  // Illegal coordinates
    // break;
  }
  angle=ang1-ang2;
  if(angle<0) angle+=360;
  return 0;
}

//////////////////////////////////////////////////////////////////////////
// Display frame class
const ClassDoc DisplayFrame::class_doc("DisplayFrame",
                    "Michael J. Donahue (michael.donahue@nist.gov)",
                    "1.0.0","14-Dec-1996");
// Recommended display window width, height and cellsize, in pixels:
const OC_REAL4m DisplayFrame::default_arrow_cellsize=OC_REAL4m(20);
const OC_REAL4m DisplayFrame::default_pixel_cellsize=OC_REAL4m(4);
const OC_REAL4m DisplayFrame::default_arrow_ratio=OC_REAL4m(0.85);
const OC_REAL4m DisplayFrame::default_pixel_ratio=OC_REAL4m(1.00);
const OC_REAL4m DisplayFrame::max_display_object_dimension=OC_REAL4m(1024.);

void DisplayFrame::SetMesh(Vf_Mesh *newmesh) {
  if(newmesh==NULL) {
    ClearFrame();
  } else {
    mesh=newmesh;
    arrow_valid=pixel_valid=boundary_valid=0;
    OC_REAL8m minmag,maxmag;
    mesh->GetMagHints(minmag,maxmag);
    if(maxmag<=0.0) {
      // Bad hints; set from data
      mesh->GetValueMagSpan(minmag,maxmag);
    }
#if 0
    // NOTE 15-Mar-2007: Do we really need this span restriction? -mjd
    // Insure value span is at least 6 orders of magnitude
    if(minmag >= maxmag * 1e-6) {
      minmag = maxmag * 1e-6;
    }
#endif
    mesh->SetMagHints(minmag,maxmag);
  }
}

void DisplayFrame::ClearFrame()
{
  mesh=(Vf_Mesh *)NULL;
  tcl_interp=Oc_GlobalInterpreter();
  coords=CS_DISPLAY_STANDARD;
  arrow_valid=pixel_valid=boundary_valid=0;
  mesh_per_pixel=1.0;
  draw_boundary=1;
  boundary_width=1.0;
  background_color="#FFFFFF";  // White
  boundary_on_top=1;
  preferred_arrow_cellsize=default_arrow_cellsize;
  preferred_pixel_cellsize=default_pixel_cellsize;
}

DisplayFrame::DisplayFrame() {
  ClearFrame();
}

void
DisplayFrame::ChangeCoordinates(CoordinateSystem old_coords,
                                CoordinateSystem new_coords,
                                Nb_List<Vf_DisplayVector> &display_list) const
{
#define MEMBERNAME "ChangeCoordinates(CS,CS,Nb_List<Vf_DisplayVector> &)"
  if(old_coords==new_coords) return;
  if(mesh_per_pixel<=0.0)
    FatalError(-1,STDDOC,"mesh_per_pixel = %g <= 0.0",
               static_cast<double>(mesh_per_pixel));
  OC_REAL4m pixel_per_mesh=static_cast<OC_REAL4m>(1.0/mesh_per_pixel);
  Nb_Vec3<OC_REAL4> mesh_min,mesh_max;
  Nb_BoundingBox<OC_REAL4> mesh_range;
  mesh->GetRange(mesh_range);
  mesh_range.GetExtremes(mesh_min,mesh_max);

  // Branch through cases
  if(old_coords==CS_CALCULATION_STANDARD
     && new_coords==CS_DISPLAY_STANDARD) {
    // Convert from mesh coordinates to display coordinates
    for(Vf_DisplayVector* display_vector=display_list.GetFirst();
        display_vector!=(Vf_DisplayVector *)NULL;
        display_vector=display_list.GetNext()) {
      display_vector->location.x -= mesh_min.x;
      display_vector->location.y  = mesh_max.y - display_vector->location.y;
      display_vector->location.z -= mesh_min.z;
      display_vector->location*=pixel_per_mesh;
      display_vector->value.y*=-1;
    }
  }
  else if(old_coords==CS_DISPLAY_STANDARD
     && new_coords==CS_CALCULATION_STANDARD) {
    // Convert from display coordinates to mesh coordinates
    for(Vf_DisplayVector* display_vector=display_list.GetFirst();
        display_vector!=(Vf_DisplayVector *)NULL;
        display_vector=display_list.GetNext()) {
      display_vector->location*=mesh_per_pixel;
      display_vector->location.x += mesh_min.x;
      display_vector->location.y  = mesh_max.y - display_vector->location.y;
      display_vector->location.z += mesh_min.z;
      display_vector->value.y*=-1;
    }
  }
  else if(old_coords==CS_CALCULATION_STANDARD
          || new_coords==CS_CALCULATION_STANDARD) {
    if(Verbosity>=100) {
      NonFatalError(STDDOC,
            "Requested coordinate transformation (%d->%d) not yet coded.\n"
            "  Using 2-step transform (through CS_DISPLAY_STANDARD) instead.",
            int(old_coords),int(new_coords));
    }
    ChangeCoordinates(old_coords,CS_DISPLAY_STANDARD,display_list);
    ChangeCoordinates(CS_DISPLAY_STANDARD,new_coords,display_list);
  }
  else {  // Flipping between 2 "DISPLAY" coordinate systems
    OC_REAL4m dangle_diff;
    if(CoordinateAngleDifference(new_coords,old_coords,dangle_diff)!=0)
      FatalError(-1,STDDOC,"Non-comparable coordinate systems (%d & %d)",
                 old_coords,new_coords);
    OC_INT4m angle_diff=OC_INT4m(OC_ROUND(dangle_diff));
    OC_REAL4m display_max_x = (mesh_max.x - mesh_min.x)*pixel_per_mesh;
    OC_REAL4m display_max_y = (mesh_max.y - mesh_min.y)*pixel_per_mesh;
    if(old_coords==CS_DISPLAY_ROT_90 || old_coords==CS_DISPLAY_ROT_270) {
      OC_REAL4 temp = display_max_x;
      display_max_x = display_max_y;
      display_max_y = temp;
    }
    if(angle_diff==90) {
      for(Vf_DisplayVector* display_vector=display_list.GetFirst();
          display_vector!=(Vf_DisplayVector *)NULL;
          display_vector=display_list.GetNext()) {
        OC_REAL4 ltempx = display_vector->location.x;
        OC_REAL4 ltempy = display_vector->location.y;
        OC_REAL4 vtempx = display_vector->value.x;
        OC_REAL4 vtempy = display_vector->value.y;
        display_vector->location.x = ltempy;
        display_vector->location.y = display_max_x - ltempx;
        display_vector->value.x =  vtempy;
        display_vector->value.y = -vtempx;
      }
    } else if(angle_diff==270) {
      for(Vf_DisplayVector* display_vector=display_list.GetFirst();
          display_vector!=(Vf_DisplayVector *)NULL;
          display_vector=display_list.GetNext()) {
        OC_REAL4 ltempx = display_vector->location.x;
        OC_REAL4 ltempy = display_vector->location.y;
        OC_REAL4 vtempx = display_vector->value.x;
        OC_REAL4 vtempy = display_vector->value.y;
        display_vector->location.x = display_max_y - ltempy;
        display_vector->location.y = ltempx;
        display_vector->value.x = -vtempy;
        display_vector->value.y =  vtempx;
      }
    } else if(angle_diff==180) {
      for(Vf_DisplayVector* display_vector=display_list.GetFirst();
          display_vector!=(Vf_DisplayVector *)NULL;
          display_vector=display_list.GetNext()) {
        OC_REAL4 ltempx = display_vector->location.x;
        OC_REAL4 ltempy = display_vector->location.y;
        OC_REAL4 vtempx = display_vector->value.x;
        OC_REAL4 vtempy = display_vector->value.y;
        display_vector->location.x = display_max_x - ltempx;
        display_vector->location.y = display_max_y - ltempy;
        display_vector->value.x = -1*vtempx;
        display_vector->value.y = -1*vtempy;
      }
    } else {
      FatalError(-1,STDDOC,"%s: Illegal angle_diff (%d)",
                 ErrProgramming,angle_diff*90);
    }
  }
#undef MEMBERNAME
}

void
DisplayFrame::ChangeCoordinates(CoordinateSystem old_coords,
                                CoordinateSystem new_coords,
                                Nb_List< Nb_Vec3<OC_REAL4> > &pt_list) const
{ // Same as previous, but for locations only
#define MEMBERNAME "ChangeCoordinates(CS,CS,Nb_List< Nb_Vec3<OC_REAL4> > &)"
  if(old_coords==new_coords) return;
  if(mesh_per_pixel<=0.0)
    FatalError(-1,STDDOC,"mesh_per_pixel = %g <= 0.0",
               static_cast<double>(mesh_per_pixel));
  OC_REAL4m pixel_per_mesh=static_cast<OC_REAL4m>(1.0/mesh_per_pixel);
  Nb_Vec3<OC_REAL4> mesh_min,mesh_max;
  Nb_BoundingBox<OC_REAL4> mesh_range;
  mesh->GetRange(mesh_range);
  mesh_range.GetExtremes(mesh_min,mesh_max);

  // Branch through cases
  if(old_coords==CS_CALCULATION_STANDARD
     && new_coords==CS_DISPLAY_STANDARD) {
    // Convert from mesh coordinates to display coordinates
    for(Nb_Vec3<OC_REAL4>* pt=pt_list.GetFirst();pt!=(Nb_Vec3<OC_REAL4> *)NULL;
        pt=pt_list.GetNext()) {
      pt->x -= mesh_min.x;
      pt->y  = mesh_max.y - pt->y;
      pt->z -= mesh_min.z;
      (*pt) *= pixel_per_mesh;
    }
  }
  else if(old_coords==CS_DISPLAY_STANDARD
     && new_coords==CS_CALCULATION_STANDARD) {
    // Convert from display coordinates to mesh coordinates
    for(Nb_Vec3<OC_REAL4>* pt=pt_list.GetFirst();pt!=(Nb_Vec3<OC_REAL4> *)NULL;
        pt=pt_list.GetNext()) {
      (*pt) *= mesh_per_pixel;
      pt->x += mesh_min.x;
      pt->y  = mesh_max.y - pt->y;
      pt->z += mesh_min.z;
    }
  }
  else if(old_coords==CS_CALCULATION_STANDARD
          || new_coords==CS_CALCULATION_STANDARD) {
    if(Verbosity>=100) {
      NonFatalError(STDDOC,
            "Requested coordinate transformation (%d->%d) not yet coded.\n"
            "  Using 2-step transform (through CS_DISPLAY_STANDARD) instead.",
            int(old_coords),int(new_coords));
    }
    ChangeCoordinates(old_coords,CS_DISPLAY_STANDARD,pt_list);
    ChangeCoordinates(CS_DISPLAY_STANDARD,new_coords,pt_list);
  }
  else {  // Flipping between 2 "DISPLAY" coordinate systems
    OC_REAL4m dangle_diff;
    OC_REAL4 temp;
    if(CoordinateAngleDifference(new_coords,old_coords,dangle_diff)!=0)
      FatalError(-1,STDDOC,"Non-comparable coordinate systems (%d & %d)",
                 old_coords,new_coords);
    OC_INT4m angle_diff=OC_INT4m(OC_ROUND(dangle_diff));
    Nb_Vec3<OC_REAL4> display_max=mesh_max;
    display_max-=mesh_min;
    display_max*=pixel_per_mesh;
    if(old_coords==CS_DISPLAY_ROT_90 || old_coords==CS_DISPLAY_ROT_270) {
      temp = display_max.x;
      display_max.x = display_max.y;
      display_max.y = temp;
    }
    for(Nb_Vec3<OC_REAL4>* pt=pt_list.GetFirst() ;
        pt!=(Nb_Vec3<OC_REAL4> *)NULL ; pt=pt_list.GetNext()) {
      switch(angle_diff) {
      case 90:
        temp=pt->x;
        pt->x=pt->y;
        pt->y=display_max.x-temp;
        break;
      case 180:
        pt->x = display_max.x - pt->x;
        pt->y = display_max.y - pt->y;
        break;
      case 270:
        temp=pt->x;
        pt->x = display_max.y - pt->y;
        pt->y = temp;
        break;
      default:
        FatalError(-1,STDDOC,"%s: Illegal angle_diff (%d)",
                   ErrProgramming,angle_diff);
        break;
      }
    }
  }
#undef MEMBERNAME
}

void DisplayFrame::CoordinatePointTransform
(CoordinateSystem old_coords,
 CoordinateSystem new_coords,
 Nb_Vec3<OC_REAL4> &pt) const
{ // Same as ChangeCoordinates, but for one point only
#define MEMBERNAME "CoordinatePointTransform(CS,CS,Nb_Vec3<OC_REAL4> &)"
  if(old_coords==new_coords) return;
  if(mesh_per_pixel<=0.0)
    FatalError(-1,STDDOC,"mesh_per_pixel = %g <= 0.0",
               static_cast<double>(mesh_per_pixel));
  OC_REAL4m pixel_per_mesh=static_cast<OC_REAL4m>(1.0/mesh_per_pixel);
  Nb_Vec3<OC_REAL4> mesh_min,mesh_max;
  Nb_BoundingBox<OC_REAL4> mesh_range;
  mesh->GetRange(mesh_range);
  mesh_range.GetExtremes(mesh_min,mesh_max);

  // Branch through cases
  if(old_coords==CS_CALCULATION_STANDARD
     && new_coords==CS_DISPLAY_STANDARD) {
    // Convert from mesh coordinates to display coordinates
    pt.x -= mesh_min.x;
    pt.y  = mesh_max.y - pt.y;
    pt.z -= mesh_min.z;
    pt *= pixel_per_mesh;
  }
  else if(old_coords==CS_DISPLAY_STANDARD
          && new_coords==CS_CALCULATION_STANDARD) {
    // Convert from display coordinates to mesh coordinates
    pt *= mesh_per_pixel;
    pt.x += mesh_min.x;
    pt.y  = mesh_max.y - pt.y;
    pt.z += mesh_min.z;
  }
  else if(old_coords==CS_CALCULATION_STANDARD
          || new_coords==CS_CALCULATION_STANDARD) {
    if(Verbosity>=100) {
      NonFatalError(STDDOC,
            "Requested coordinate transformation (%d->%d) not yet coded.\n"
            "  Using 2-step transform (through CS_DISPLAY_STANDARD) instead.",
            int(old_coords),int(new_coords));
    }
    CoordinatePointTransform(old_coords,CS_DISPLAY_STANDARD,pt);
    CoordinatePointTransform(CS_DISPLAY_STANDARD,new_coords,pt);
  }
  else {  // Flipping between 2 "DISPLAY" coordinate systems
    OC_REAL4m dangle_diff;
    OC_REAL4 temp;
    if(CoordinateAngleDifference(new_coords,old_coords,dangle_diff)!=0)
      FatalError(-1,STDDOC,"Non-comparable coordinate systems (%d & %d)",
                 old_coords,new_coords);
    OC_INT4m angle_diff=OC_INT4m(OC_ROUND(dangle_diff));
    Nb_Vec3<OC_REAL4> display_max=mesh_max;
    display_max-=mesh_min;
    display_max*=pixel_per_mesh;
    if(old_coords==CS_DISPLAY_ROT_90 || old_coords==CS_DISPLAY_ROT_270) {
      temp = display_max.x;
      display_max.x = display_max.y;
      display_max.y = temp;
    }
    switch(angle_diff) {
    case 90:
      temp=pt.x;
      pt.x=pt.y;
      pt.y=display_max.x-temp;
      break;
    case 180:
      pt.x = display_max.x - pt.x;
      pt.y = display_max.y - pt.y;
      break;
    case 270:
      temp=pt.x;
      pt.x = display_max.y - pt.y;
      pt.y = temp;
      break;
    default:
      FatalError(-1,STDDOC,"%s: Illegal angle_diff (%d)",
                 ErrProgramming,angle_diff);
      break;
    }
  }
#undef MEMBERNAME
}

OC_INT4m DisplayFrame::GetDisplayList(SampleInfo &si)
{
  // Use full-size mesh bounding box
  Nb_BoundingBox<OC_REAL4> mesh_box;
  mesh->GetRange(mesh_box);
  return GetDisplayList(mesh_box,si);
}

OC_INT4m
DisplayFrame::GetDisplayList(const Nb_BoundingBox<OC_REAL4>& select_box,
                             SampleInfo &si)
{ // Fills display_list from *mesh using requested step size and
  // color quantity. Use step_request=0 to get _all_ points in mesh.
  //  Import select_box is in mesh units, i.e. CS_CALCULATION_STANDARD
  // coordinate system.
  //  Converts to current display_coords.
  // NOTE: Samples mesh in terms of *mesh cells*, as opposed to
  //       *mesh units*.
  // NOTE: mesh_per_pixel must be set before calling this routine.
#define MEMBERNAME "GetDisplayList"
  if(si.subsample<0)
    FatalError(-1,STDDOC,"%s: step_request=%g",
               ErrBadParam,static_cast<double>(si.subsample));
  if(mesh==(Vf_Mesh *)NULL) FatalError(-1,STDDOC,"Mesh pointer not set.");

  // Limit input to intersection of select_box and mesh bounding box
  Nb_BoundingBox<OC_REAL4> mesh_box;
  mesh->GetRange(mesh_box);
  mesh_box.IntersectWith(select_box);

  // Determine stepping rates.  The code below is awkward; It is a
  // result of legacy code/design (or non-design, as the case may be).
  Nb_Vec3<OC_REAL4> mesh_celldim=mesh->GetApproximateCellDimensions();
  if(mesh_celldim.x<=0.0) mesh_celldim.x=1.0; //Safety
  if(mesh_celldim.y<=0.0) mesh_celldim.y=1.0; //Safety
  OC_REAL4m mesh_cellsize
    = (mesh_celldim.x<mesh_celldim.y ? mesh_celldim.x :mesh_celldim.y);

  // Get list of vectors to display
  OC_REAL4m xstep=mesh_celldim.x*si.subsample;
  OC_REAL4m ystep=mesh_celldim.y*si.subsample;
  OC_REAL4m zstep=0.;
  mesh->GetDisplayList(xstep,ystep,zstep,mesh_box,
                       si.colorquantity,si.phase,
                       si.invert,si.trimtiny,
                       si.dvlist);
  xstep/=mesh_celldim.x;
  ystep/=mesh_celldim.y;
  si.subsample=OC_MAX(xstep,ystep);
  OC_REAL4m grit=mesh->GetSubsampleGrit()*mesh_cellsize;
  if(0.<si.subsample && si.subsample<grit/2.) si.subsample=grit;

  ChangeCoordinates(CS_CALCULATION_STANDARD,coords,si.dvlist);

  return 0;
#undef MEMBERNAME

}

void DisplayFrame::SetPlotConfiguration(
                const PlotConfiguration& _arrow_config,
                const PlotConfiguration& _pixel_config)
{
  // Copy arrow configuration update arrow display list
  arrow_config=_arrow_config;
  arrow_valid=0;

  // Ditto for pixel configuration
  pixel_config=_pixel_config;
  pixel_valid=0;
}

OC_REAL4m DisplayFrame::GetAutoSampleRate(OC_REAL4m cellsize) const
{ // Returns a recommended subsample rate, for the current mesh at
  // current zoom.
  Nb_Vec3<OC_REAL4> celldim=mesh->GetApproximateCellDimensions();
  OC_REAL8m minxy = (celldim.x<celldim.y ? celldim.x : celldim.y);
  OC_REAL8m step=OC_ROUND(cellsize*mesh_per_pixel/minxy);
  step=OC_ROUND(step*100.)/100.;  // Make "round" value
  if(step<=1.) step=0.;
  return static_cast<OC_REAL4m>(step);
}

OC_BOOL DisplayFrame::MakeTkColor
(const char* req_color,Nb_DString& tkcolor) const
{ // Converts "color" to Tk 8-bit format, "#XXXXXX", and stores
  // result in color.  If color cannot be interpreted, then
  // a warning message is printed, and tkcolor is unchanged.
  // Returns 1 on success, 0 on failure.
#define MEMBERNAME "MakeTkColor(const char*,Nb_DString&)"
  OC_REAL8m red,green,blue;
  if(Nb_GetColor(req_color,red,green,blue)==0) {
    const char *fmt="Unrecognized color request %s";
    unsigned int tmpsiz =
      static_cast<unsigned int>(strlen(fmt)+strlen(req_color)+16);
    char *tmpbuf=new char[tmpsiz];
    Oc_Snprintf(tmpbuf,tmpsiz,fmt,req_color);
    NonFatalError(STDDOC,tmpbuf);
    delete[] tmpbuf;
    return 0;
  }

  OC_UINT4 r = (OC_UINT4)OC_ROUND(red*256.);    if(r>255) r=255;
  OC_UINT4 g = (OC_UINT4)OC_ROUND(green*256.);  if(g>255) g=255;
  OC_UINT4 b = (OC_UINT4)OC_ROUND(blue*256.);   if(b>255) b=255;

  char buf[16];
  Oc_Snprintf(buf,sizeof(buf),"#%02X%02X%02X",r,g,b);
  tkcolor=buf;

  return 1;
#undef MEMBERNAME
}

OC_BOOL DisplayFrame::SetBackgroundColor(const char* req_color) {
#define MEMBERNAME "SetBackgroundColor(const char*)"
  return MakeTkColor(req_color,background_color);
#undef MEMBERNAME
}

OC_BOOL DisplayFrame::SetBoundaryColor(const char* req_color) {
#define MEMBERNAME "SetBoundaryColor(const char*)"
  return MakeTkColor(req_color,boundary_color);
#undef MEMBERNAME
}

void
DisplayFrame::SetDrawPixelBaseCmd
(const char* canvas_name,
 const char* stipple,
 OC_BOOL hide,
 OC_REAL4m xsize,
 OC_REAL4m ysize,
 DrawPixelData& draw_data) const
{
  draw_data.xsize = xsize;
  draw_data.ysize = ysize;

  Nb_TclCommand& cmd = draw_data.pixelcmd; // For convenience
  String buf = canvas_name;
  buf += " create rectangle";
  cmd.SetBaseCommand("DisplayFrame::DrawPixel",
                     tcl_interp,buf.c_str(),12 + (hide ? 2 : 0));
  // Args 0,1,2,3 are rectangle corner coordinates
  cmd.SetCommandArg(4,"-fill");
  // Arg 5 is fill color
  cmd.SetCommandArg(6,"-outline");
  cmd.SetCommandArg(7,"");
  cmd.SetCommandArg(8,"-tag");
  // Arg 9 is a two item list, "pixels p%d" where %d is zslice
  cmd.SetCommandArg(10,"-stipple");
  cmd.SetCommandArg(11,stipple);
  if(hide) {
    cmd.SetCommandArg(12,"-state");
    cmd.SetCommandArg(13,"hidden");
  }
}

void
DisplayFrame::DrawPixel
(const Vf_DisplayVector& dspvec,
 DrawPixelData& draw_data) const
{ // Based on code contributed by SLW
#define MEMBERNAME "DrawPixel"

  Nb_TclCommand& cmd = draw_data.pixelcmd; // For convenience
  OC_REAL4& xsize = draw_data.xsize;
  OC_REAL4& ysize = draw_data.ysize;

  // For some reason, the rendering code runs significantly faster if
  // coordinates are converted to integer before passing into the canvas
  // create rectangle command --- at least on the tested system which is
  // a 64-bit Linux/Xeon box running Tcl/Tk 8.5.5.
  const Nb_Vec3<OC_REAL4> *postemp=&(dspvec.location);
  OC_REAL8m x1 = (postemp->x)-xsize/2;
  OC_INT4m ix1 = static_cast<OC_INT4m>(floor(x1));
  OC_INT4m ix2 = static_cast<OC_INT4m>(ceil(x1+xsize));
  OC_REAL8m y1 = (postemp->y)-ysize/2;
  OC_INT4m iy1 = static_cast<OC_INT4m>(floor(y1));
  OC_INT4m iy2 = static_cast<OC_INT4m>(ceil(y1+ysize));

  cmd.SetCommandArg(0,ix1);  cmd.SetCommandArg(1,iy1);
  cmd.SetCommandArg(2,ix2);  cmd.SetCommandArg(3,iy2);

  // Color
  const char *color=pixel_config.Color(dspvec.shade);
  cmd.SetCommandArg(5,color);

  // Slice
  Oc_Snprintf(draw_data.slicetag,DrawPixelData::SCRATCHSIZE,
              "pixels p%d",dspvec.zslice);
  cmd.SetCommandArg(9,draw_data.slicetag);

  cmd.Eval();
#undef MEMBERNAME
}

Nb_Vec3<OC_REAL4> DisplayFrame::GetPixelDimensions() const {
  Nb_Vec3<OC_REAL4> dimens = mesh->GetApproximateCellDimensions();
  dimens *= default_pixel_ratio * pixel_config.size / mesh_per_pixel;
  if(pixel_sample.subsample>1.)
    dimens *= pixel_sample.subsample;
  return dimens;
}

OC_REAL4m DisplayFrame::GetViewArrowScale() const
{ // Returns a recommended arrow scaling adjustment based
  // on current view axis.
  Nb_Vec3<OC_REAL4> celldim=mesh->GetApproximateCellDimensions();
  OC_REAL8m scale = 1.0;
  OC_REAL8m minxyz = PickMinimum(celldim);
  if(minxyz>0.0) {
    scale = (celldim.x<celldim.y ? celldim.x : celldim.y)/minxyz;
    if(scale>1e8) {
      scale=1e8; // Safety
    }
  }
  scale = OC_ROUND(scale*100.)/100.; // Make "round" value
  return static_cast<OC_REAL4m>(scale);
}

OC_REAL8m DisplayFrame::GetArrowSize() const
{ // Returns base arrow size, including zoom, subsampling, and
  // configured arrow size. View axis scaling is also included if (and
  // only if) arrow_config.viewscale is true.
  Nb_Vec3<OC_REAL4> dimens = mesh->GetApproximateCellDimensions();
  OC_REAL4m arrowsize = PickMinimum(dimens);
  arrowsize *= default_arrow_ratio * arrow_config.size / mesh_per_pixel;
  if(arrow_sample.subsample>1.) {
    arrowsize*=arrow_sample.subsample;
  }
  if(arrow_config.viewscale) {
    arrowsize *= GetViewArrowScale();
  }
  return arrowsize;
}

void
DisplayFrame::SetDrawArrowBaseCmd
(const char* canvas_name,
 OC_BOOL hide,
 DrawArrowData& draw_data) const
{
  String buf; // Work space

  Nb_TclCommand& arrowcmd     = draw_data.arrowcmd;  // For convenience
  Nb_TclCommand& inarrowcmdA  = draw_data.inarrowcmdA;
  Nb_TclCommand& inarrowcmdB  = draw_data.inarrowcmdB;
  Nb_TclCommand& outarrowcmdA = draw_data.outarrowcmdA;
  Nb_TclCommand& outarrowcmdB = draw_data.outarrowcmdB;

  // Standard arrow command
  buf = canvas_name;
  buf += " create line";
  arrowcmd.SetBaseCommand("DisplayFrame::DrawArrow",
                          tcl_interp,buf.c_str(),14 + (hide ? 2 : 0));
  // Args 0,1,2,3 are line endpoint coordinates
  arrowcmd.SetCommandArg(4,"-tag");
  // Arg 5 is a two item list, "arrows a%d" where %d is zslice
  arrowcmd.SetCommandArg(6,"-arrow");
  arrowcmd.SetCommandArg(7,"last");
  arrowcmd.SetCommandArg(8,"-arrowshape");
  // Arg 9 is three item list describing arrow shape
  arrowcmd.SetCommandArg(10,"-width");
  // Arg 11 is width value
  arrowcmd.SetCommandArg(12,"-fill");
  // Arg 13 is fill color
  if(hide) {
    arrowcmd.SetCommandArg(14,"-state");
    arrowcmd.SetCommandArg(15,"hidden");
  }

  // Commands (two) for arrows pointing into-the-plane
  buf = canvas_name;
  buf += " create rectangle";
  inarrowcmdA.SetBaseCommand("DisplayFrame::DrawArrow",
                             tcl_interp,buf.c_str(),10 + (hide ? 2 : 0));
  // Args 0,1,2,3 are rectangle corner coordinates
  inarrowcmdA.SetCommandArg(4,"-tag");
  // Arg 5 is a two item list, "arrows a%d" where %d is zslice
  inarrowcmdA.SetCommandArg(6,"-fill");
  // Arg 7 is fill color
  inarrowcmdA.SetCommandArg(8,"-outline");
  // Arg 9 is outline color
  if(hide) {
    inarrowcmdA.SetCommandArg(10,"-state");
    inarrowcmdA.SetCommandArg(11,"hidden");
  }

  buf = canvas_name;
  buf += " create rectangle";
  inarrowcmdB.SetBaseCommand("DisplayFrame::DrawArrow",
                             tcl_interp,buf.c_str(),10 + (hide ? 2 : 0));
  // Args 0,1,2,3 are rectangle corner coordinates
  inarrowcmdB.SetCommandArg(4,"-tag");
  // Arg 5 is a two item list, "arrows a%d" where %d is zslice
  inarrowcmdB.SetCommandArg(6,"-fill");
  // Arg 7 is fill color
  inarrowcmdB.SetCommandArg(8,"-width");
  inarrowcmdB.SetCommandArg(9,0);
  if(hide) {
    inarrowcmdB.SetCommandArg(10,"-state");
    inarrowcmdB.SetCommandArg(11,"hidden");
  }

  // Commands (two) for arrows pointing out-of-the-plane
  buf = canvas_name;
  buf += " create polygon";
  outarrowcmdA.SetBaseCommand("DisplayFrame::DrawArrow",
                             tcl_interp,buf.c_str(),16 + (hide ? 2 : 0));
  // Args 0,1,2,3,4,5,6,7 are corner coordinates
  outarrowcmdA.SetCommandArg(8,"-tag");
  // Arg 9 is a two item list, "arrows a%d" where %d is zslice
  outarrowcmdA.SetCommandArg(10,"-fill");
  // Arg 11 is fill color
  outarrowcmdA.SetCommandArg(12,"-outline");
  // Arg 13 is outline color
  outarrowcmdA.SetCommandArg(14,"-width");
  // Arg 15 is outline width
  if(hide) {
    outarrowcmdA.SetCommandArg(16,"-state");
    outarrowcmdA.SetCommandArg(17,"hidden");
  }

  buf = canvas_name;
  buf += " create line";
  outarrowcmdB.SetBaseCommand("DisplayFrame::DrawArrow",
                             tcl_interp,buf.c_str(),10 + (hide ? 2 : 0));
  // Args 0,1,2,3 are line endpoint coordinates
  outarrowcmdB.SetCommandArg(4,"-tag");
  // Arg 5 is a two item list, "arrows a%d" where %d is zslice
  outarrowcmdB.SetCommandArg(6,"-fill");
  // Arg 7 is fill color
  outarrowcmdB.SetCommandArg(8,"-width");
  // Arg 9 is outline width
  if(hide) {
    outarrowcmdB.SetCommandArg(10,"-state");
    outarrowcmdB.SetCommandArg(11,"hidden");
  }
}

void
DisplayFrame::DrawArrow
(const Vf_DisplayVector& dspvec,
 OC_REAL4m arrowsize,
 DrawArrowData& draw_data) const
{
#define MEMBERNAME "DrawArrow"
#ifndef SQRT3
#define SQRT3 1.7320508075688772
#endif
  const Nb_Vec3<OC_REAL4> *postemp;
  const Nb_Vec3<OC_REAL4> *vtemp;

  Nb_TclCommand& arrowcmd     = draw_data.arrowcmd;  // For convenience
  Nb_TclCommand& inarrowcmdA  = draw_data.inarrowcmdA;
  Nb_TclCommand& inarrowcmdB  = draw_data.inarrowcmdB;
  Nb_TclCommand& outarrowcmdA = draw_data.outarrowcmdA;
  Nb_TclCommand& outarrowcmdB = draw_data.outarrowcmdB;
  char* slicetag = draw_data.slicetag;

  OC_REAL8m x1,y1,x2,y2,arrow_len,proj_len;
  OC_INT4m r0,c0,r1,c1,r2,c2,ioff,a1,a2,a3;
  postemp=&(dspvec.location);
  vtemp=&(dspvec.value);
  OC_REAL8m delx,dely,delz;

  // Line components, rescaled as necessary to respect
  // max_display_object_dimension
  delx = vtemp->x;
  dely = vtemp->y;
  delz = vtemp->z;
  OC_REAL8m rescale = arrowsize;
  OC_REAL8m maxdim = OC_MAX(fabs(delx),fabs(dely));
  if((rescale>1.0 && maxdim>max_display_object_dimension/rescale) ||
     (rescale<=1.0 && maxdim*rescale>max_display_object_dimension)  ) {
    rescale = max_display_object_dimension/maxdim;
  }
  delx *= rescale;
  dely *= rescale;
  if((rescale>1.0
      && fabs(delz)/16>max_display_object_dimension/rescale) ||
     (rescale<=1.0
      && rescale*fabs(delz)/16>max_display_object_dimension)  ) {
    // delz too big; truncate to 16*max_display_object_dimension to
    // protect against overflow.
    delz = 16*max_display_object_dimension;
  } else {
    delz *= rescale;
  }
  arrow_len = delx*delx + dely*dely + delz*delz;
  if(arrow_len<OC_REAL4_EPSILON*OC_REAL4_EPSILON) {
    return;  // This is a zero vector --- nothing to do.
  }
  arrow_len = sqrt(arrow_len);
  proj_len  = sqrt(delx*delx+dely*dely);

  // Arrowhead components
  a1=(int)OC_ROUND(proj_len*0.3);
  a2=(int)OC_ROUND(proj_len*0.4);
  a3=(int)OC_ROUND(arrow_len*0.15);
  if(a3<5) a1=a2;

  // Attributes
  int linewidth=(int)OC_ROUND(a3*0.6);
  if(linewidth<1) linewidth=1;

  // Main color
  const char* pcolor1=arrow_config.Color(dspvec.shade);

  // Slice
  // Note: Testing (Apr-2015) indicates that outside of the .Eval()
  //       calls, about 20% of the runtime of this function is spent in
  //       the Oc_Snprintf call, so time improvement could be made by
  //       using a fast itoa-like substitute.  OTOH, this same testing
  //       indicates that 98% of the function runtime is tied up in the
  //       .Eval() calls, so the Oc_Sprintf overhead is rather
  //       meaningless unless the .Eval() calls can be made faster.
  Oc_Snprintf(slicetag,DrawArrowData::SCRATCHSIZE,
              "arrows a%d",dspvec.zslice);

  if(proj_len>=arrow_len*0.15) { // Standard arrow ////////////////////////
    x1=(postemp->x)-delx/2;   x2=x1+delx;
    r1=int(OC_ROUND(x1));     r2=int(OC_ROUND(x2));
    y1=(postemp->y)-dely/2;   y2=y1+dely;
    c1=int(OC_ROUND(y1));     c2=int(OC_ROUND(y2));

    arrowcmd.SetCommandArg(0,OC_REAL8m(r1));
    arrowcmd.SetCommandArg(1,OC_REAL8m(c1));
    arrowcmd.SetCommandArg(2,OC_REAL8m(r2));
    arrowcmd.SetCommandArg(3,OC_REAL8m(c2));
    arrowcmd.SetCommandArg(5,slicetag);

    // Arrowhead shape
#if NB_TCL_COMMAND_USE_STRING_INTERFACE
    char ashape[256];
    Oc_Snprintf(ashape,sizeof(ashape),"%d %d %d",a1,a2,a3);
    arrowcmd.SetCommandArg(9,ashape);
#else
    draw_data.arrowshape.WriteInt(0,a1);
    draw_data.arrowshape.WriteInt(1,a2);
    draw_data.arrowshape.WriteInt(2,a3);
    arrowcmd.SetCommandArg(9,draw_data.arrowshape);
#endif // NB_TCL_COMMAND_USE_STRING_INTERFACE

    arrowcmd.SetCommandArg(11,linewidth);
    arrowcmd.SetCommandArg(13,pcolor1);

    arrowcmd.Eval();
  }  else { // Small projection with undefined direction.  Draw a dot
    OC_REAL4m backshade=dspvec.shade+0.5f;  // Maybe there's a better way
    backshade-=static_cast<OC_REAL4m>(floor(backshade));  /// to do this?
    const char* pcolor2=arrow_config.Color(backshade);
    r0=(int)OC_ROUND(postemp->x); c0=(int)OC_ROUND(postemp->y);
    if(vtemp->z<0) { // Arrow pointing into-plane /////////////////////////
      ioff=(int)OC_ROUND(OC_REAL4m(a3)/2.);
      r1=r0-ioff;  c1=c0-ioff;
      r2=r0+ioff;  c2=c0+ioff;

      // Draw outer rectangle
      inarrowcmdA.SetCommandArg(0,OC_REAL8m(r1));
      inarrowcmdA.SetCommandArg(1,OC_REAL8m(c1));
      inarrowcmdA.SetCommandArg(2,OC_REAL8m(r2));
      inarrowcmdA.SetCommandArg(3,OC_REAL8m(c2));
      inarrowcmdA.SetCommandArg(5,slicetag);
      inarrowcmdA.SetCommandArg(7,pcolor1);
      inarrowcmdA.SetCommandArg(9,pcolor1);
      inarrowcmdA.Eval();

      if(a3>6) {
        // Draw inner rectangle
        ioff=(int)OC_ROUND(OC_REAL4m(a3)/6.);
        r1=r0-ioff;  c1=c0-ioff;
        r2=r0+ioff;  c2=c0+ioff;
        inarrowcmdB.SetCommandArg(0,OC_REAL8m(r1));
        inarrowcmdB.SetCommandArg(1,OC_REAL8m(c1));
        inarrowcmdB.SetCommandArg(2,OC_REAL8m(r2));
        inarrowcmdB.SetCommandArg(3,OC_REAL8m(c2));
        inarrowcmdB.SetCommandArg(5,slicetag);
        inarrowcmdB.SetCommandArg(7,pcolor2);
        inarrowcmdB.Eval();
      }
    } else { // Arrow pointing out-of-plane ///////////////////////////////
      ioff=(int)OC_ROUND(OC_REAL4m(a3)*(SQRT2/2.));
      r1=r0-ioff;  c1=c0-ioff;
      r2=r0+ioff;  c2=c0+ioff;

      // Draw diamond
      outarrowcmdA.SetCommandArg(0,OC_REAL8m(r1));
      outarrowcmdA.SetCommandArg(1,OC_REAL8m(c0));
      outarrowcmdA.SetCommandArg(2,OC_REAL8m(r0));
      outarrowcmdA.SetCommandArg(3,OC_REAL8m(c2));
      outarrowcmdA.SetCommandArg(4,OC_REAL8m(r2));
      outarrowcmdA.SetCommandArg(5,OC_REAL8m(c0));
      outarrowcmdA.SetCommandArg(6,OC_REAL8m(r0));
      outarrowcmdA.SetCommandArg(7,OC_REAL8m(c1));
      outarrowcmdA.SetCommandArg(9,slicetag);
      outarrowcmdA.SetCommandArg(11,pcolor1);
      outarrowcmdA.SetCommandArg(13,pcolor1);
      outarrowcmdA.SetCommandArg(15,linewidth/3);
      outarrowcmdA.Eval();
      if(a3>6) {
        // Draw a "+" in center of diamond
        outarrowcmdB.SetCommandArg(0,OC_REAL8m(r1));
        outarrowcmdB.SetCommandArg(1,OC_REAL8m(c0));
        outarrowcmdB.SetCommandArg(2,OC_REAL8m(r2));
        outarrowcmdB.SetCommandArg(3,OC_REAL8m(c0));
        outarrowcmdB.SetCommandArg(5,slicetag);
        outarrowcmdB.SetCommandArg(7,pcolor2);
        outarrowcmdB.SetCommandArg(9,linewidth/3);
        outarrowcmdB.Eval();
        outarrowcmdB.SetCommandArg(0,OC_REAL8m(r0));
        outarrowcmdB.SetCommandArg(1,OC_REAL8m(c2));
        outarrowcmdB.SetCommandArg(2,OC_REAL8m(r0));
        outarrowcmdB.SetCommandArg(3,OC_REAL8m(c1));
        outarrowcmdB.Eval();
      }
    }
  }
#undef MEMBERNAME
}

OC_INT4m DisplayFrame::DrawLine(const char* canvas_name,
                             const Nb_List< Nb_Vec3<OC_REAL4> >& pts,
                             OC_REAL8m width,
                             const char* color,
                             const char* tags)
{
  static char buf[4096];  // *Should* be plenty big enough.

  if(pts.GetSize()<1) return TCL_OK; // Nothing to do

  OC_UINT4m bufsize_guess = static_cast<OC_UINT4m>(64 +
    strlen(canvas_name)+(pts.GetSize()+2)*64+strlen(color)+strlen(tags)
                                             +16+4);
  if(bufsize_guess>sizeof(buf)) {
    Oc_Snprintf(buf,sizeof(buf),"Command line too long in"
                " DisplayFrame::DrawLine; estimated size=%u (%u available)."
                " Too many points? (%u)",
                static_cast<unsigned int>(bufsize_guess),
                static_cast<unsigned int>(sizeof(buf)),
                static_cast<unsigned int>(pts.GetSize()));
    Tcl_AppendResult(tcl_interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  OC_INDEX worklen =
    Oc_Snprintf(buf,sizeof(buf),"%s create line ",canvas_name);

  Nb_List_Index< Nb_Vec3<OC_REAL4> > key;
  const Nb_Vec3<OC_REAL4>* tmppt = pts.GetFirst(key);
  worklen += Oc_Snprintf(buf+worklen,sizeof(buf)-worklen,"%g %g ",
                         static_cast<double>(tmppt->x),
                         static_cast<double>(tmppt->y));
  while((tmppt=pts.GetNext(key))!=(Nb_Vec3<OC_REAL4> *)NULL) {
    worklen +=
      Oc_Snprintf(buf+worklen,sizeof(buf)-worklen,"%g %g ",
                  static_cast<double>(tmppt->x),
                  static_cast<double>(tmppt->y));
  }

  if(width>0) {
    worklen += Oc_Snprintf(buf+worklen,sizeof(buf)-worklen,
                           "-width %g ",static_cast<double>(width));
  }

  if(color!=NULL && color[0]!='\0') {
    worklen += Oc_Snprintf(buf+worklen,sizeof(buf)-worklen,
                           "-fill {%s} ",color);
  }

  if(tags!=NULL || tags[0]!='\0') {
    worklen += Oc_Snprintf(buf+worklen,sizeof(buf)-worklen,
                           "-tag {%s} ",tags);
  }

  // Presently (2007-08-01), this routine is only used by
  // the boundary drawing code.  Today I think "bevel"
  // joins look nice, so set that here.  May want to
  // rethink this in the future.
  worklen += Oc_Snprintf(buf+worklen,sizeof(buf)-worklen,
                           "-joinstyle bevel");

  return Tcl_Eval(tcl_interp,buf);
}

OC_BOOL DisplayFrame::SetCoordinates(CoordinateSystem new_coords)
{
    if(new_coords==coords) return 0;
    coords=new_coords;
    arrow_valid=pixel_valid=boundary_valid=0;
    return 1;
}

const char* DisplayFrame::GetVecColor(const Nb_Vec3<OC_REAL4>& v) const
{
  // Access to the underlying Vf_Mesh::GetVecShade function.  See that
  // routine for details on shade determination.  The mapping from
  // shade to color depends on the state of arrow_config.displaystate
  // and pixel_config.displaystate.  Pixel config colorquantity and
  // colormap are used, unless pixels are turned off and arrows are
  // turned on, in which case arrow config colorquantity and colormap
  // are used.
  OC_REAL4m shade;
  const char* color="";
  if(!pixel_config.displaystate && arrow_config.displaystate) {
    // Use arrow_config data
    shade=mesh->GetVecShade(arrow_config.colorquantity,
                            arrow_config.phase,arrow_config.invert,
                            v);
    if(shade>=0.) color=arrow_config.Color(shade);
  } else {
    // Use pixel_config data
    shade=mesh->GetVecShade(pixel_config.colorquantity,
                            pixel_config.phase,pixel_config.invert,
                            v);
    if(shade>=0.) color=pixel_config.Color(shade);
  }
  return color;
}

OC_INT4m DisplayFrame::Render(const char *canvas_name,OC_BOOL hide)
{
#define MEMBERNAME "Render"

  // Check initialization
  if(tcl_interp==NULL)
    FatalError(-1,STDDOC,"tcl_interp not initialized.");
  if(mesh==NULL)
    FatalError(-1,STDDOC,"mesh not initialized.");

  // Check that current coordinate system is displayable.
  if(coords!=CS_DISPLAY_STANDARD &&
     coords!=CS_DISPLAY_ROT_90   &&
     coords!=CS_DISPLAY_ROT_180  &&
     coords!=CS_DISPLAY_ROT_270    ) {
    FatalError(-1,STDDOC,
               "Can't draw display list in current (%d) coordinate system",
               coords);
  }

  { // Set canvas background color
    const char *fmt="%s configure -background %s";
    size_t bufsz
      = strlen(fmt)+strlen(canvas_name)+background_color.Length()+1;
    Oc_AutoBuf buf; buf.SetLength(bufsz);
    Oc_Snprintf(buf.GetStr(),bufsz,
                fmt,canvas_name,background_color.GetStr());
    Nb_TclCommand cmd;
    cmd.SetBaseCommand("DisplayFrame::Render",tcl_interp,buf,0);
    cmd.Eval();
  }

  // Draw pixel map, if requested
  if(pixel_config.displaystate) {
    if(!pixel_valid) {
      pixel_sample.subsample=pixel_config.subsample;
      if(pixel_config.autosample || pixel_sample.subsample<0.)
        pixel_sample.subsample=GetAutoSampleRate(preferred_pixel_cellsize);
      pixel_sample.colorquantity=pixel_config.colorquantity;
      pixel_sample.phase=pixel_config.phase;
      pixel_sample.invert=pixel_config.invert;
      pixel_sample.trimtiny=0;
      GetDisplayList(pixel_sample);
      pixel_valid=1;
    }

    // Respect max_display_object_dimension restriction
    Nb_Vec3<OC_REAL4> pixel_dimens = GetPixelDimensions();
    if(coords == CS_DISPLAY_ROT_90 || coords == CS_DISPLAY_ROT_270) {
      OC_REAL4 temp=pixel_dimens.x;
      pixel_dimens.x = pixel_dimens.y;
      pixel_dimens.y = temp;
    }
    OC_REAL4m xsize = pixel_dimens.x;
    if(xsize>max_display_object_dimension) {
      xsize=max_display_object_dimension;
    }
    OC_REAL4m ysize = pixel_dimens.y;
    if(ysize>max_display_object_dimension) {
      ysize=max_display_object_dimension;
    }

    DrawPixelData draw_cmd;
    SetDrawPixelBaseCmd(canvas_name,pixel_config.stipple,hide,
                        xsize,ysize,draw_cmd);

    Vf_DisplayVector *dspvec;
    for(dspvec=pixel_sample.dvlist.GetFirst();
        dspvec!=(Vf_DisplayVector *)NULL;
        dspvec=pixel_sample.dvlist.GetNext()) {
      DrawPixel(*dspvec,draw_cmd);
    }
  }

  // Draw arrows, if requested
  if(arrow_config.displaystate) {
    if(!arrow_valid) {
      arrow_sample.subsample=arrow_config.subsample;
      if(arrow_config.autosample || arrow_sample.subsample<0.)
        arrow_sample.subsample=GetAutoSampleRate(preferred_arrow_cellsize);
      arrow_sample.colorquantity=arrow_config.colorquantity;
      arrow_sample.phase=arrow_config.phase;
      arrow_sample.invert=arrow_config.invert;
      arrow_sample.trimtiny=1;  // Exclude small arrows from DisplayList
      GetDisplayList(arrow_sample);
      arrow_valid=1;
    }
    const OC_REAL4m arrowsize=static_cast<OC_REAL4m>(GetArrowSize());
    Nb_TclCommand arrowcmd;
    DrawArrowData draw_cmds;
    SetDrawArrowBaseCmd(canvas_name,hide,draw_cmds);
    Vf_DisplayVector *dspvec;
    for(dspvec=arrow_sample.dvlist.GetFirst();
        dspvec!=(Vf_DisplayVector *)NULL;
        dspvec=arrow_sample.dvlist.GetNext()) {
      DrawArrow(*dspvec,arrowsize,draw_cmds);
    }
  }

  // Draw mesh edge box, if requested
  if(draw_boundary) {
    if(!boundary_valid) {
      mesh->GetBoundaryList(boundary_list);
      ChangeCoordinates(CS_CALCULATION_STANDARD,coords,boundary_list);
      boundary_valid=1;
    }
    if(DrawLine(canvas_name,boundary_list,
                            boundary_width,boundary_color.GetStr(),
                            "bdry")!=TCL_OK) {
      return TCL_ERROR;
    }
  }

  // NOTE: The DrawPixel and DrawArrow routines throw exceptions on
  // error, as opposed to DrawLine which sets the Tcl_Result and
  // returns TCL_ERROR.  One might want to harmonize these error
  // handling methods... <g>

  return TCL_OK;
#undef MEMBERNAME
}

OC_INT4m
DisplayFrame::FillBitmap(OC_BOOL reverse_layers,OommfBitmap& bitmap)
{
  Nb_BoundingBox<OC_REAL4> display_box; // Empty box
  return FillBitmap(display_box,display_box,reverse_layers,bitmap);
}

OC_INT4m
DisplayFrame::FillBitmap(const Nb_BoundingBox<OC_REAL4>& arrow_box,
                         const Nb_BoundingBox<OC_REAL4>& pixel_box,
                         OC_BOOL reverse_layers,OommfBitmap& bitmap)
{ // Writes output into bitmap.  Imports arrow_box and pixel_box are in
  // mesh coordinates.  Empty boxes are interpreted as no restraint,
  // i.e., fill all points.  If reverse_layers is true, then the
  // rendering order of the layers is reversed, with the "top" (i.e.,
  // largest z) first, and the "bottom" last.
#define MEMBERNAME "FillBitmap"
  // Check initialization
  if(mesh==NULL)
    FatalError(-1,STDDOC,"mesh not initialized.");

  // Check that current coordinate system is displayable.
  if(coords!=CS_DISPLAY_STANDARD &&
     coords!=CS_DISPLAY_ROT_90   &&
     coords!=CS_DISPLAY_ROT_180  &&
     coords!=CS_DISPLAY_ROT_270    ) {
    FatalError(-1,STDDOC,
               "Can't draw display list in current (%d) coordinate system",
               coords);
  }

  // Clear and set background color
  bitmap.Erase(OommfPackedRGB(background_color.GetStr()));

  // Get pixel data, if needed
  OC_BOOL clip_pixels=0;
  Nb_Vec3<OC_REAL4> pixelbox_minpt,pixelbox_maxpt;
  Nb_Vec3<OC_REAL4> pixel_dimens;
  if(pixel_config.displaystate && !pixel_valid) {
    pixel_sample.subsample=pixel_config.subsample;
    if(pixel_config.autosample || pixel_sample.subsample<0.) {
      pixel_sample.subsample=GetAutoSampleRate(preferred_pixel_cellsize);
    }
    pixel_sample.colorquantity=pixel_config.colorquantity;
    pixel_sample.phase=pixel_config.phase;
    pixel_sample.invert=pixel_config.invert;
    pixel_sample.trimtiny=0;
    if(pixel_box.IsEmpty()) {
      GetDisplayList(pixel_sample);
    } else {
      GetDisplayList(pixel_box,pixel_sample);
    }
    Nb_BoundingBox<OC_REAL4> bdry_box,data_box;  OC_REAL8m data_pad;
    GetBoundingBoxes(bdry_box,data_box,data_pad);
    data_box.ExpandWith(bdry_box); // Ignore data_pad
    data_box.GetExtremes(pixelbox_minpt,pixelbox_maxpt);
    clip_pixels = !data_box.IsEmpty();
    pixel_dimens = GetPixelDimensions();
    if(coords == CS_DISPLAY_ROT_90 || coords == CS_DISPLAY_ROT_270) {
      OC_REAL4 temp=pixel_dimens.x;
      pixel_dimens.x = pixel_dimens.y;
      pixel_dimens.y = temp;
    }
    pixel_valid=1;
  }

  // Get arrow data, if needed
  if(arrow_config.displaystate && !arrow_valid) {
    arrow_sample.subsample=arrow_config.subsample;
    if(arrow_config.autosample || arrow_sample.subsample<0.) {
      arrow_sample.subsample=GetAutoSampleRate(preferred_arrow_cellsize);
    }
    arrow_sample.colorquantity=arrow_config.colorquantity;
    arrow_sample.phase=arrow_config.phase;
    arrow_sample.invert=arrow_config.invert;
    arrow_sample.trimtiny=1;  // Exclude small arrows from DisplayList
    if(arrow_box.IsEmpty()) {
      GetDisplayList(arrow_sample);
    } else {
      GetDisplayList(arrow_box,arrow_sample);
    }
    arrow_valid=1;
  }

  // Contruct pixel and arrow level slice arrays
  OC_INDEX slice_count = mesh->GetZsliceCount();
  if(slice_count<1) {
    slice_count=1;  // Safety; Presumably this is an empty mesh
  }
  Nb_List<Vf_DisplayVector> *pixel_level_sample = NULL;
  Nb_List<Vf_DisplayVector> *arrow_level_sample = NULL;
  if(pixel_config.displaystate) {
    pixel_level_sample = new Nb_List<Vf_DisplayVector>[slice_count];
    Vf_DisplayVector *dspvec;
    for(dspvec=pixel_sample.dvlist.GetFirst();
        dspvec!=(Vf_DisplayVector *)NULL;
        dspvec=pixel_sample.dvlist.GetNext()) {
      OC_INDEX level = dspvec->zslice;
      if(level>=0 && level<slice_count)
        pixel_level_sample[level].Append(*dspvec);
    }
  }
  if(arrow_config.displaystate) {
    arrow_level_sample = new Nb_List<Vf_DisplayVector>[slice_count];
    Vf_DisplayVector *dspvec;
    for(dspvec=arrow_sample.dvlist.GetFirst();
        dspvec!=(Vf_DisplayVector *)NULL;
        dspvec=arrow_sample.dvlist.GetNext()) {
      OC_INDEX level = dspvec->zslice;
      if(level>=0 && level<slice_count)
        arrow_level_sample[level].Append(*dspvec);
    }
  }

  // Mesh edge box on bottom (i.e., draw first), if requested
  if(draw_boundary && !boundary_on_top) {
    if(!boundary_valid) {
      mesh->GetBoundaryList(boundary_list);
      ChangeCoordinates(CS_CALCULATION_STANDARD,coords,boundary_list);
      boundary_valid=1;
    }
    PlanePoint pt;
    Nb_List<PlanePoint> vlist;
    const Nb_Vec3<OC_REAL4> *bdry;
    Nb_List_Index< Nb_Vec3<OC_REAL4> > key;
    for(bdry=boundary_list.GetFirst(key);bdry!=NULL;
        bdry=boundary_list.GetNext(key)) {
      pt.x=OC_ROUND(bdry->x);
      pt.y=OC_ROUND(bdry->y);
      vlist.Append(pt);
    }
    int joinstyle=2;
    int antialias=0;
    OommfPackedRGB color(boundary_color);
    bitmap.DrawPolyLine(vlist,boundary_width,color,
                        joinstyle,antialias);
  }

  // Loop through levels, drawing first pixels and then arrows.
  // If boundary is requested, that gets layered on last
  for(OC_INDEX i=0;i<slice_count;i++) {

    OC_INDEX level=i;
    if(reverse_layers) level = slice_count - 1 - i;

    // Pixels
    if(pixel_config.displaystate) {
      Vf_DisplayVector *dspvec;
      Nb_List<Vf_DisplayVector>& pixlist = pixel_level_sample[level];
      for(dspvec=pixlist.GetFirst();
          dspvec!=(Vf_DisplayVector *)NULL;
          dspvec=pixlist.GetNext()) {
        OC_REAL4m x1,x2,y1,y2;
        x1=(dspvec->location.x)-pixel_dimens.x/2;  x2=x1+pixel_dimens.x-1;
        y1=(dspvec->location.y)-pixel_dimens.y/2;  y2=y1+pixel_dimens.y-1;
        /// Unlike the Tk canvas widget, OommfBitmap DrawFilledRectangle
        /// includes both corners.
        if(clip_pixels &&
           x1<pixelbox_maxpt.x && pixelbox_minpt.x<x2
           && y1<pixelbox_maxpt.y && pixelbox_minpt.y<y2) {
          // Don't clip if whole draw rectangle is outside mesh range
          if(x1<pixelbox_minpt.x) x1=pixelbox_minpt.x;
          if(x2>pixelbox_maxpt.x) x2=pixelbox_maxpt.x;
          if(y1<pixelbox_minpt.y) y1=pixelbox_minpt.y;
          if(y2>pixelbox_maxpt.y) y2=pixelbox_maxpt.y;
        }
        bitmap.DrawFilledRectangle(x1,y1,x2,y2,
                                   pixel_config.PackedColor(dspvec->shade),
                                   pixel_config.stipple);
      }
    }

    // Arrows
    if(arrow_config.displaystate) {
      OC_REAL8m arrowsize=GetArrowSize();
      Vf_DisplayVector *dspvec;
      Nb_List<Vf_DisplayVector>& arrlist = arrow_level_sample[level];
      for(dspvec=arrlist.GetFirst();
          dspvec!=(Vf_DisplayVector *)NULL;
          dspvec=arrlist.GetNext()) {
        OC_REAL8m arrmag=dspvec->value.MagSq();
        if(arrmag<OC_REAL4_EPSILON) continue; // This is a zero vector; skip.
        arrmag=sqrt(arrmag);
        OC_REAL8m arrmaginv=1./arrmag;
        OC_REAL8m xcos=dspvec->value.x*arrmaginv;
        OC_REAL8m ycos=dspvec->value.y*arrmaginv;
        OC_REAL8m zcos=dspvec->value.z*arrmaginv;
        OC_REAL8m xpos=OC_ROUND(dspvec->location.x); // Round for nicer
        OC_REAL8m ypos=OC_ROUND(dspvec->location.y); // display
        if(fabs(zcos)<0.985) {
          // Angle with xy-plane smaller than 80 degrees => draw arrow
          bitmap.DrawFilledArrow(xpos,ypos,
                                 arrowsize*arrmag,xcos,ycos,zcos,
                                 arrow_config.PackedColor(dspvec->shade),
                                 arrow_config.antialias,
                                 arrow_config.outlinewidth,
                                 arrow_config.outlinecolor);
        } else {
          OC_REAL4m foreshade=dspvec->shade;
          OC_REAL4m backshade=foreshade+0.5f;   // Maybe there's a better way
          backshade-=static_cast<OC_REAL4m>(floor(backshade)); // to do this?
          if(zcos>0) {
            // Vector is almost complete out-of-plane.  Draw diamond-plus
            bitmap.DrawDiamondWithCross(xpos,ypos,arrowsize*arrmag,
                                        arrow_config.PackedColor(foreshade),
                                        arrow_config.PackedColor(backshade));
          } else {
            // Vector is almost complete in-to-plane.  Draw dotted-square
            bitmap.DrawSquareWithDot(xpos,ypos,arrowsize*arrmag,
                                     arrow_config.PackedColor(foreshade),
                                     arrow_config.PackedColor(backshade));
          }
        }
      }
    }

  }

  // Mesh edge box on top (i.e., draw last), if requested
  if(draw_boundary && boundary_on_top) {
    if(!boundary_valid) {
      mesh->GetBoundaryList(boundary_list);
      ChangeCoordinates(CS_CALCULATION_STANDARD,coords,boundary_list);
      boundary_valid=1;
    }
    PlanePoint pt;
    Nb_List<PlanePoint> vlist;
    const Nb_Vec3<OC_REAL4> *bdry;
    Nb_List_Index< Nb_Vec3<OC_REAL4> > key;
    for(bdry=boundary_list.GetFirst(key);bdry!=NULL;
        bdry=boundary_list.GetNext(key)) {
      pt.x=OC_ROUND(bdry->x);
      pt.y=OC_ROUND(bdry->y);
      vlist.Append(pt);
    }
    int joinstyle=2;
    int antialias=0;
    OommfPackedRGB color(boundary_color);
    bitmap.DrawPolyLine(vlist,boundary_width,color,
                        joinstyle,antialias);
  }

  if(arrow_level_sample!=NULL) delete[] arrow_level_sample;
  if(pixel_level_sample!=NULL) delete[] pixel_level_sample;

  return TCL_OK;
#undef MEMBERNAME
}

OC_INT4m
DisplayFrame::PSDump
(Tcl_Channel channel,
 OC_REAL8m pxoff,OC_REAL8m pyoff,
 OC_REAL8m printwidth,OC_REAL8m printheight,
 const char* page_orientation,
 OC_REAL8m xmin,OC_REAL8m ymin,
 OC_REAL8m xmax,OC_REAL8m ymax,
 const Nb_BoundingBox<OC_REAL4>& arrow_box,
 const Nb_BoundingBox<OC_REAL4>& pixel_box,
 OC_BOOL reverse_layers,
 OC_REAL8m mat_width,
 Nb_DString mat_color,
 OC_REAL8m arrow_outline_width,
 Nb_DString arrow_outline_color)
{
#define MEMBERNAME "PSDump"

  // Check initialization
  if(mesh==NULL)
    FatalError(-1,STDDOC,"mesh not initialized.");

  // Check that current coordinate system is displayable.
  if(coords!=CS_DISPLAY_STANDARD &&
     coords!=CS_DISPLAY_ROT_90   &&
     coords!=CS_DISPLAY_ROT_180  &&
     coords!=CS_DISPLAY_ROT_270    ) {
    FatalError(-1,STDDOC,
               "Can't draw display list in current (%d) coordinate system",
               coords);
  }

  // Initialize OommfPSDraw object and write file header
  OommfPSDraw psdraw(channel,
                     pxoff,pyoff,
                     printwidth,printheight,
                     page_orientation,
                     xmin,ymin,xmax,ymax,
                     OommfPackedRGB(background_color.GetStr()),
                     arrow_outline_width,
                     OommfPackedRGB(arrow_outline_color.GetStr()));

  // Get pixel data, if requested
  OC_BOOL clip_pixels=0;
  Nb_Vec3<OC_REAL4> pixelbox_minpt,pixelbox_maxpt;
  Nb_Vec3<OC_REAL4> pixel_dimens;
  if(pixel_config.displaystate) {
    pixel_sample.subsample=pixel_config.subsample;
    if(pixel_config.autosample || pixel_sample.subsample<0.) {
      pixel_sample.subsample=GetAutoSampleRate(preferred_pixel_cellsize);
    }
    pixel_sample.colorquantity=pixel_config.colorquantity;
    pixel_sample.phase=pixel_config.phase;
    pixel_sample.invert=pixel_config.invert;
    pixel_sample.trimtiny=0;
    if(pixel_box.IsEmpty()) {
      GetDisplayList(pixel_sample);
    } else {
      GetDisplayList(pixel_box,pixel_sample);
    }
    Nb_BoundingBox<OC_REAL4> bdry_box,data_box;  OC_REAL8m data_pad;
    GetBoundingBoxes(bdry_box,data_box,data_pad);
    data_box.ExpandWith(bdry_box); // Ignore data_pad
    data_box.GetExtremes(pixelbox_minpt,pixelbox_maxpt);
    clip_pixels = !data_box.IsEmpty();
    pixel_dimens = GetPixelDimensions();
    if(coords == CS_DISPLAY_ROT_90 || coords == CS_DISPLAY_ROT_270) {
      OC_REAL4 temp=pixel_dimens.x;
      pixel_dimens.x = pixel_dimens.y;
      pixel_dimens.y = temp;
    }
    pixel_valid=1;
  }

  // Get arrow data, if needed
  if(arrow_config.displaystate) {
    arrow_sample.subsample=arrow_config.subsample;
    if(arrow_config.autosample || arrow_sample.subsample<0.) {
      arrow_sample.subsample=GetAutoSampleRate(preferred_arrow_cellsize);
    }
    arrow_sample.colorquantity=arrow_config.colorquantity;
    arrow_sample.phase=arrow_config.phase;
    arrow_sample.invert=arrow_config.invert;
    arrow_sample.trimtiny=1;  // Exclude small arrows from DisplayList
    if(arrow_box.IsEmpty()) {
      GetDisplayList(arrow_sample);
    } else {
      GetDisplayList(arrow_box,arrow_sample);
    }
    arrow_valid=1;
  }

  // Contruct pixel and arrow level slice arrays
  OC_INDEX slice_count = mesh->GetZsliceCount();
  if(slice_count<1) {
    slice_count=1;  // Safety; Presumably this is an empty mesh
  }
  Nb_List<Vf_DisplayVector> *pixel_level_sample = NULL;
  Nb_List<Vf_DisplayVector> *arrow_level_sample = NULL;
  if(pixel_config.displaystate) {
    pixel_level_sample = new Nb_List<Vf_DisplayVector>[slice_count];
    Vf_DisplayVector *dspvec;
    for(dspvec=pixel_sample.dvlist.GetFirst();
        dspvec!=(Vf_DisplayVector *)NULL;
        dspvec=pixel_sample.dvlist.GetNext()) {
      OC_INDEX level = dspvec->zslice;
      if(level>=0 && level<slice_count)
        pixel_level_sample[level].Append(*dspvec);
    }
  }
  if(arrow_config.displaystate) {
    arrow_level_sample = new Nb_List<Vf_DisplayVector>[slice_count];
    Vf_DisplayVector *dspvec;
    for(dspvec=arrow_sample.dvlist.GetFirst();
        dspvec!=(Vf_DisplayVector *)NULL;
        dspvec=arrow_sample.dvlist.GetNext()) {
      OC_INDEX level = dspvec->zslice;
      if(level>=0 && level<slice_count)
        arrow_level_sample[level].Append(*dspvec);
    }
  }

  // Mesh edge box on bottom (i.e., draw first), if requested
  if(draw_boundary && !boundary_on_top) {
    if(!boundary_valid) {
      mesh->GetBoundaryList(boundary_list);
      ChangeCoordinates(CS_CALCULATION_STANDARD,coords,boundary_list);
      boundary_valid=1;
    }
    PlanePoint pt;
    Nb_List<PlanePoint> vlist;
    const Nb_Vec3<OC_REAL4> *bdry;
    Nb_List_Index< Nb_Vec3<OC_REAL4> > key;
    for(bdry=boundary_list.GetFirst(key);bdry!=NULL;
        bdry=boundary_list.GetNext(key)) {
      pt.x=OC_ROUND(bdry->x);
      pt.y=OC_ROUND(bdry->y);
      vlist.Append(pt);
    }
    int joinstyle=2;
    OommfPackedRGB color(boundary_color);
    psdraw.DrawPolyLine(vlist,boundary_width,color,joinstyle);
  }


  // Loop through levels, drawing first pixels and then arrows.
  // If boundary is requested, that gets layered on last
  for(OC_INDEX i=0;i<slice_count;i++) {

    OC_INDEX level=i;
    if(reverse_layers) level = slice_count - 1 - i;

    // Pixels
    if(pixel_config.displaystate) {
      Vf_DisplayVector *dspvec;
      Nb_List<Vf_DisplayVector>& pixlist = pixel_level_sample[level];
      for(dspvec=pixlist.GetFirst();
          dspvec!=(Vf_DisplayVector *)NULL;
          dspvec=pixlist.GetNext()) {
        OC_REAL4m x1,x2,y1,y2;
        x1=(dspvec->location.x)-pixel_dimens.x/2;  x2=x1+pixel_dimens.x;
        y1=(dspvec->location.y)-pixel_dimens.y/2;  y2=y1+pixel_dimens.y;
        if(clip_pixels &&
           x1<pixelbox_maxpt.x && pixelbox_minpt.x<x2
           && y1<pixelbox_maxpt.y && pixelbox_minpt.y<y2) {
          // Don't clip if whole draw rectangle is outside mesh range
          if(x1<pixelbox_minpt.x) x1=pixelbox_minpt.x;
          if(x2>pixelbox_maxpt.x) x2=pixelbox_maxpt.x;
          if(y1<pixelbox_minpt.y) y1=pixelbox_minpt.y;
          if(y2>pixelbox_maxpt.y) y2=pixelbox_maxpt.y;
        }
        psdraw.DrawFilledRectangle(x1,y1,x2,y2,
                                   pixel_config.PackedColor(dspvec->shade),
                                   pixel_config.stipple);
      }
    }

    // Arrows
    if(arrow_config.displaystate) {
      OC_REAL8m arrowsize=GetArrowSize();
      Vf_DisplayVector *dspvec;
      Nb_List<Vf_DisplayVector>& arrlist = arrow_level_sample[level];
      for(dspvec=arrlist.GetFirst();
          dspvec!=(Vf_DisplayVector *)NULL;
          dspvec=arrlist.GetNext()) {
        OC_REAL8m arrmag=dspvec->value.MagSq();
        if(arrmag<OC_REAL4_EPSILON) continue; // This is a zero vector; skip.
        arrmag=sqrt(arrmag);
        OC_REAL8m arrmaginv=1./arrmag;
        OC_REAL8m xcos=dspvec->value.x*arrmaginv;
        OC_REAL8m ycos=dspvec->value.y*arrmaginv;
        OC_REAL8m zcos=dspvec->value.z*arrmaginv;
        OC_REAL8m xpos=dspvec->location.x;
        OC_REAL8m ypos=dspvec->location.y;
        if(fabs(zcos)<0.985) {
          // Angle with xy-plane smaller than 80 degrees => draw arrow
          psdraw.DrawFilledArrow(xpos,ypos,
                                 arrowsize*arrmag,xcos,ycos,zcos,
                                 arrow_config.PackedColor(dspvec->shade));
        } else {
          OC_REAL4m foreshade=dspvec->shade;
          OC_REAL4m backshade=foreshade+0.5f;   // Maybe there's a better way
          backshade-=static_cast<OC_REAL4m>(floor(backshade)); // to do this?
          if(zcos>0) {
            // Vector is almost completely out-of-plane.  Draw diamond-plus
            psdraw.DrawDiamondWithCross(xpos,ypos,arrowsize*arrmag,
                                        arrow_config.PackedColor(foreshade),
                                        arrow_config.PackedColor(backshade));
          } else {
            // Vector is almost completely in-to-plane.  Draw dotted-square
            psdraw.DrawSquareWithDot(xpos,ypos,arrowsize*arrmag,
                                     arrow_config.PackedColor(foreshade),
                                     arrow_config.PackedColor(backshade));
          }
        }
      }
    }

  }

  // Mesh edge box on top (i.e., draw last), if requested
  if(draw_boundary && boundary_on_top) {
    if(!boundary_valid) {
      mesh->GetBoundaryList(boundary_list);
      ChangeCoordinates(CS_CALCULATION_STANDARD,coords,boundary_list);
      boundary_valid=1;
    }
    PlanePoint pt;
    Nb_List<PlanePoint> vlist;
    const Nb_Vec3<OC_REAL4> *bdry;
    Nb_List_Index< Nb_Vec3<OC_REAL4> > key;
    for(bdry=boundary_list.GetFirst(key);bdry!=NULL;
        bdry=boundary_list.GetNext(key)) {
      pt.x=OC_ROUND(bdry->x);
      pt.y=OC_ROUND(bdry->y);
      vlist.Append(pt);
    }
    int joinstyle=2;
    OommfPackedRGB color(boundary_color);
    psdraw.DrawPolyLine(vlist,boundary_width,color,joinstyle);
  }

  // Add mat
  if(mat_width>0) {
    OommfPackedRGB color(mat_color.GetStr());
    psdraw.AddMat(mat_width,color);
  }

  if(arrow_level_sample!=NULL) delete[] arrow_level_sample;
  if(pixel_level_sample!=NULL) delete[] pixel_level_sample;

  pixel_valid=arrow_valid=0; // Restricted display bounding
  /// box invalidates subsample arrays.

  // PostScript trailer automatically written on psdraw destruction.

  return TCL_OK;
#undef MEMBERNAME
}


// NOTE: "zoom" in the next several routines is in units of "pixels per
//       approx cell size".
OC_REAL4m DisplayFrame::GetZoom(void) const
{
  Nb_Vec3<OC_REAL4> celldim=mesh->GetApproximateCellDimensions();
  OC_REAL4m mesh_per_cell = PickMinimum(celldim);
  if(mesh_per_cell<=0.) mesh_per_cell=1.0;  // Safety
  return mesh_per_cell/mesh_per_pixel;
}

OC_REAL4m DisplayFrame::SetZoom(OC_REAL4m zoom)
{ // Returns zoom value
  Nb_Vec3<OC_REAL4> celldim=mesh->GetApproximateCellDimensions();
  OC_REAL4m mesh_per_cell = PickMinimum(celldim);
  if(mesh_per_cell<=0.) mesh_per_cell=1.0;  // Safety
  if(OC_REAL4_EPSILON<zoom && zoom<1/OC_REAL4_EPSILON) {
    mesh_per_pixel=mesh_per_cell/zoom;
    if(mesh_per_pixel<=0.) mesh_per_pixel=1.0; // Safety
    arrow_valid=pixel_valid=boundary_valid=0;
  }
  return mesh_per_cell/mesh_per_pixel;
}

OC_REAL4m DisplayFrame::SetZoom
(OC_REAL4m width,
 OC_REAL4m height,
 OC_REAL4m margin,
 OC_REAL4m scrollbar_cross_dimension)
{ // Adjusts mesh_per_pixel scaling factor so that entire
  // mesh will just fit inside width and height.  Marks display
  // lists as out-of-date.  New zoom value is returned.
  // NOTE: Setting either width or height <=0 will cause
  //  that extent to be ignored.  If both are <=0, then
  //  no change is made.

  OC_REAL8m new_mesh_per_pixel=mesh_per_pixel;
  if(new_mesh_per_pixel<=0.) new_mesh_per_pixel=1.0; // Just to be safe.

  width -= 2*margin;
  height -= 2*margin;
  if(width<=0.0) {
    // Width is smaller than 2 x margin size, so it is not
    // possible to fill display along horizontal axis.
    // Instead, assume horizontal scroll bar and ignore
    // width restriction.
    height -= scrollbar_cross_dimension;
  }
  if(height<=0.0) {
    // Ditto above comment, wrt vertical axis.
    width -= scrollbar_cross_dimension;
  }

  Nb_BoundingBox<OC_REAL4> bdry,data;
  OC_REAL8m data_pad;

  GetBoundingBoxes(bdry,data,data_pad);

  OC_REAL8m xscale= -1.0, yscale= -1.0;
  if(width>0.) {
    if(!bdry.IsEmpty()) xscale=bdry.GetWidth()/width;
    if(width>2*data_pad)
      xscale=OC_MAX(xscale,data.GetWidth()/(width-2*data_pad));
  }
  if(height>0.) {
    if(!bdry.IsEmpty()) yscale=bdry.GetHeight()/height;
    if(height>2*data_pad)
      yscale=OC_MAX(yscale,data.GetHeight()/(height-2*data_pad));
  }
  OC_REAL8m scale=OC_MAX(xscale,yscale);

  if(scale>0.) new_mesh_per_pixel*=scale;


  Nb_Vec3<OC_REAL4> celldim=mesh->GetApproximateCellDimensions();
  OC_REAL4m mesh_per_cell = PickMinimum(celldim);
  if(mesh_per_cell<=0.) mesh_per_cell=1.0;  // Safety
  OC_REAL8m zoom=mesh_per_cell/new_mesh_per_pixel; // User value

  // Make zoom a "round" (%.2f) value
  if(zoom>0.01) {
    zoom=OC_ROUND(zoom*100.)/100.;
    /// Arguably this should be "floor" instead of "round," but the
    /// latter appears more stable with respect to dependence of
    /// outcome on initial value of mesh_per_pixel.  Moreover, there
    /// is now (June-1998) some slack in the calculation of whether
    /// scroll bars are required.
    new_mesh_per_pixel=static_cast<OC_REAL8m>(mesh_per_cell)/zoom;
  }

  if(mesh_per_pixel!=static_cast<OC_REAL4m>(new_mesh_per_pixel)) {
    mesh_per_pixel=static_cast<OC_REAL4m>(new_mesh_per_pixel);
    MarkAllSampleListsInvalid();
  }

  if(mesh_per_pixel<=0.0) mesh_per_pixel = 1.0; // Safety

  return static_cast<OC_REAL4m>(zoom);
}

void DisplayFrame::GetBoundingBoxes(Nb_BoundingBox<OC_REAL4> &bdry,
                                    Nb_BoundingBox<OC_REAL4> &data,
                                    OC_REAL8m &data_pad) const
{ // Returns bounding boxes for display, in pixels (at current zoom):
  //   'bdry' is the bounding box for the boundary
  //   'data' is the bounding box for the data
  //   'data_pad' is the margin (in pixels) that needs to be added
  //     to the data bounding box.
  // The first two scale with mesh_per_pixel (i.e., current zoom).  The
  // last is independent of the zoom levels.
  // NB: Either of the returning boxes, in particular bdry, may
  //  be an "empty" box, in which case max<min.

  Nb_Vec3<OC_REAL4> min,max;

  // Check for an empty mesh
  Nb_BoundingBox<OC_REAL4> bbox;
  mesh->GetRange(bbox);
  if(bbox.IsEmpty()) {
    data.Reset();
    bdry.Reset();
    data_pad=0.;
    return;
  }

  // Calculate bounding box for data points
  mesh->GetDataRange(data);
  data.GetExtremes(min,max);
  Nb_List< Nb_Vec3<OC_REAL4> > datalist;
  datalist.SetFirst(min); datalist.Append(max);
  ChangeCoordinates(CS_CALCULATION_STANDARD,coords,datalist);
  min= *(datalist.GetFirst()); max= *(datalist.GetNext());
  data.SortAndSet(min,max);

  data_pad=OC_MAX(preferred_arrow_cellsize,preferred_pixel_cellsize)/2.;

  // Create boundary bounding box by expanding for each point in the
  // boundary list, one at a time.  Use supplemental list (instead of
  // boundary_list) so we don't have to worry about updating boundary_list
  // and loosing the 'const' property of this method.
  Nb_List< Nb_Vec3<OC_REAL4> > bdrylist;
  mesh->GetBoundaryList(bdrylist);
  ChangeCoordinates(CS_CALCULATION_STANDARD,coords,bdrylist);
  bdry.Reset();
  for(Nb_Vec3<OC_REAL4>* bp=bdrylist.GetFirst() ; bp!=NULL ;
      bp=bdrylist.GetNext()) {
    bdry.ExpandWith(*bp);
  }
}

Nb_BoundingBox<OC_REAL4> DisplayFrame::GetDisplayBox() const
{ // Returns bounding box for display, in pixels (at current zoom)
  // Box extents will be (0,0,0) x (0,0,0) in empty case.
  Nb_Vec3<OC_REAL4> min,max;
  Nb_BoundingBox<OC_REAL4> bdry,data,total_box;
  OC_REAL8m wide_data_pad;
  GetBoundingBoxes(bdry,data,wide_data_pad);
  OC_REAL4m data_pad = static_cast<OC_REAL4>(wide_data_pad);

  // Add padding to data box.
  if(!data.IsEmpty()) {
    data.GetExtremes(min,max);
    min.x-=data_pad;    max.x+=data_pad;
    min.y-=data_pad;    max.y+=data_pad;
    // Padding z with data_pad doesn't make too much sense,
    // since data_pad is derived from in-plane cell sizes.
    // So instead just pad z with z component of base cell
    // dimension.
    Nb_Vec3<OC_REAL4> celldim = mesh->GetApproximateCellDimensions();
    OC_REAL4m half_zstep
      = static_cast<OC_REAL4m>(celldim.z/(2.0*mesh_per_pixel));
    min.z -= half_zstep;    max.z += half_zstep;
    total_box.Set(min,max);
  } else {
    total_box.Reset();  // Just for clarity
  }

  // Expand as necessary to include boundary.
  total_box.ExpandWith(bdry);
  if(!data.IsEmpty()) {
    // Don't change z-depth for sake of boundary
    Nb_Vec3<OC_REAL4> temp_min,temp_max;
    total_box.GetExtremes(temp_min,temp_max);
    temp_min.z=min.z;
    temp_max.z=max.z;
    total_box.Set(temp_min,temp_max);
  }

  // Special handling for empty case.
  if(total_box.IsEmpty()) {
    Nb_Vec3<OC_REAL4> mesh_min(0,0,0),mesh_max(0,0,0);
    total_box.Set(mesh_min,mesh_max);
  }

  return total_box;
}

OC_REAL8m DisplayFrame::GetValueScaling() const {
  OC_REAL8m minmag,maxmag;
  mesh->GetMagHints(minmag,maxmag);
  OC_REAL8m scale = mesh->GetDisplayValueScale()*maxmag;
  if(scale<=0.0) scale=1.0;  // Provide safe initial value.
  return scale;
}

OC_INT4m DisplayFrame::SetValueScaling(OC_REAL8m newscaling) {
  if(newscaling<=0.0) return 1;
  OC_INT4m errcode=1;
  OC_REAL8m minmag,maxmag;
  mesh->GetMagHints(minmag,maxmag);
  if(fabs(newscaling)<FLT_MAX*fabs(maxmag)) {
    errcode=0;
    newscaling/=maxmag;
    if(newscaling!=mesh->GetDisplayValueScale()) {
      errcode=mesh->SetDisplayValueScale(newscaling);
      arrow_valid=pixel_valid=0;
    }
  }
  return errcode;
}
