/* FILE: rectangularmesh.cc         -*-Mode: c++-*-
 *
 * Rectangular mesh, derived from Oxs_Mesh class.
 *
 */

#include "nb.h"
#include "vf.h"
#include "atlas.h"
#include "meshvalue.h"
#include "oxsexcept.h"
#include "rectangularmesh.h"
#include "director.h"
#include "nb.h"
#include "energy.h"             // Needed to make MSVC++ 5 happy
#include "util.h"

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_RectangularMesh);
OXS_EXT_REGISTER(Oxs_PeriodicRectangularMesh);

/* End includes */

////////////////////////////////////////////////////////////////////////
/// Support for max angle routines.  See NOTES VI, 6-Sep-2012, p 71-73.
class OxsRectangularMeshAngle {
  // This (internal) class is used to store a representation of the
  // angle between two vectors.  It is designed so that setting and
  // order comparisons are quick; the radian angle (between 0 and pi)
  // can be extracted via the GetAngle() call, but GetAngle() is slow
  // and so shouldn't be called more often than necessary.
public:
  void Set(const ThreeVector& a, const ThreeVector& b) {
    OC_REAL8m dot = a*b;
    ThreeVector cross = a^b;
    sdotsq = dot * fabs(dot);
    crosssq = cross.MagSq();
  }
  void SetAngle(OC_REAL8m angle) {
    // Note: SetAngle(ang) == SetAngle(|ang|)
    OC_REAL8m dot = cos(angle);     
    OC_REAL8m cross = sin(angle);
    sdotsq = dot*fabs(dot);
    crosssq = cross*cross;    
  }
  OC_REAL8m GetAngle() {
    // Returns angle in radians, 0<= ang <= pi
    if(sdotsq < 0.0) {
      return Oc_Atan2(sqrt(crosssq),-1*sqrt(-1*sdotsq));
    }
    return Oc_Atan2(sqrt(crosssq),sqrt(sdotsq));
  }
  friend OC_BOOL operator<(const OxsRectangularMeshAngle&,
                           const OxsRectangularMeshAngle&);
  friend OC_BOOL operator>(const OxsRectangularMeshAngle&,
                           const OxsRectangularMeshAngle&);
  friend OC_BOOL operator==(const OxsRectangularMeshAngle&,
                            const OxsRectangularMeshAngle&);
  friend OC_BOOL operator!=(const OxsRectangularMeshAngle&,
                            const OxsRectangularMeshAngle&);
  // Constructors:
  OxsRectangularMeshAngle(const ThreeVector& a, const ThreeVector& b) {
    Set(a,b);
  }
  OxsRectangularMeshAngle(OC_REAL8m angle) {
    SetAngle(angle);
  }
  // Note: Use default destructor and assignment operator.
private:
  // For vectors a, b:
  OC_REAL8m crosssq;    // |axb|^2 (non-negative)
  OC_REAL8m sdotsq;   // (a*b).|a*b|  (signed)
};

OC_BOOL operator>(const OxsRectangularMeshAngle& a,
                  const OxsRectangularMeshAngle& b){
  return (a.crosssq * b.sdotsq > a.sdotsq * b.crosssq);
}
OC_BOOL operator<(const OxsRectangularMeshAngle& a,
                  const OxsRectangularMeshAngle& b){
  return (a.crosssq * b.sdotsq < a.sdotsq * b.crosssq);
}
OC_BOOL operator==(const OxsRectangularMeshAngle& a,
                 const OxsRectangularMeshAngle& b) {
  return (a.crosssq * b.sdotsq == a.sdotsq * b.crosssq);
}
OC_BOOL operator!=(const OxsRectangularMeshAngle& a,
                const OxsRectangularMeshAngle& b) {
  return !(a == b);
}

/////////////////////////////////////////////////////////////////////
// Oxs_CommonRectangularMesh
void Oxs_CommonRectangularMesh::InitScaling(const Oxs_Box& box)
{ // Constructor helper function.  Assumes cellsize is already set.
  if(cellsize.x<=0.0 || cellsize.y<=0.0 || cellsize.z<=0.0) {
    String msg = String("Invalid MIF input block detected for object ")
      + String(InstanceName());
    throw Oxs_ExtError(msg.c_str());
  }

  cellvolume=cellsize.x*cellsize.y*cellsize.z;

  OC_REAL8m xrange = box.GetMaxX() - box.GetMinX();
  OC_REAL8m yrange = box.GetMaxY() - box.GetMinY();
  OC_REAL8m zrange = box.GetMaxZ() - box.GetMinZ();
  if(xrange<=0. || yrange<=0. || zrange<=0.) {
    String msg = String("Invalid atlas range detected for object ")
      + String(InstanceName());
    throw Oxs_ExtError(msg.c_str());
  }

  base = ThreeVector(box.GetMinX()+cellsize.x/2.,
                     box.GetMinY()+cellsize.y/2.,
                     box.GetMinZ()+cellsize.z/2.);

  xdim = static_cast<OC_INDEX>(OC_ROUND(xrange/cellsize.x));
  ydim = static_cast<OC_INDEX>(OC_ROUND(yrange/cellsize.y));
  zdim = static_cast<OC_INDEX>(OC_ROUND(zrange/cellsize.z));
  if(xdim<1 || ydim<1 || zdim<1) {
    String msg = String("Invalid MIF input block detected for object ")
      + String(InstanceName())
      + String("; minimum range smaller than cell dimension.");
    throw Oxs_ExtError(msg.c_str());
  }

  // Overflow test; restrict to signed value range
  OC_INDEX testval = OC_INDEX((OC_UINDEX(1)<<(sizeof(OC_INDEX)*8-1))-1);
  /// Maximum allowed value; Assumes 2's complement arithmetic
  testval /= xdim;
  testval /= ydim;
  testval /= zdim;
  if(testval<1) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),"Requested mesh size (%ld x %ld x %ld)"
                " has too many elements",(long)xdim,
                (long)ydim,(long)zdim);
    throw Oxs_ExtError(this,buf);
  }

  xydim = xdim*ydim;
  elementcount = xydim*zdim;

  if( fabs(xdim*cellsize.x - xrange) > 0.01*cellsize.x ||
      fabs(ydim*cellsize.y - yrange) > 0.01*cellsize.y ||
      fabs(zdim*cellsize.z - zrange) > 0.01*cellsize.z   ) {
    String msg = String("Invalid MIF input block detected for object ")
      + String(InstanceName())
      + String(": range is not an integral multiple of cellsize.");
    throw Oxs_ExtError(msg.c_str());
  }

}

// Main constructor, for use by Specify command in MIF input file
Oxs_CommonRectangularMesh::Oxs_CommonRectangularMesh
(const char* name,     // Child instance id
 Oxs_Director* newdtr, // App director
 const char* argstr)   // MIF input block parameters
  : Oxs_Mesh(name,newdtr,argstr)
{
  // Process arguments
  cellsize = GetThreeVectorInitValue("cellsize");
  
  Oxs_OwnedPointer<Oxs_Atlas> atlas;
  OXS_GET_INIT_EXT_OBJECT("atlas",Oxs_Atlas,atlas);

  Oxs_Box box;
  atlas->GetWorldExtents(box);
  InitScaling(box);
}

// Secondary constructor; provides a function-level API
// for use by other Oxs_Ext objects.
Oxs_CommonRectangularMesh::Oxs_CommonRectangularMesh
(const char* name,     // Child instance id
 Oxs_Director* newdtr, // App director
 const char* argstr,   // MIF input block parameters
 const ThreeVector& in_cellsize,
 const Oxs_Box& range_box)
  : Oxs_Mesh(name,newdtr,argstr)
{
  cellsize = in_cellsize;
  InitScaling(range_box);
}


/////////////////////////////////////////////////////////////////////
// Constructor for internal use by MakeRefinedMesh member function.
Oxs_CommonRectangularMesh::Oxs_CommonRectangularMesh
(const char* name,     // Child instance id
 Oxs_Director* newdtr, // App director
 const ThreeVector& in_base,
 const ThreeVector& in_cellsize,
 OC_INDEX in_xdim,OC_INDEX in_ydim,OC_INDEX in_zdim)
  : Oxs_Mesh(name,newdtr),
    base(in_base),cellsize(in_cellsize),
    xdim(in_xdim),ydim(in_ydim),zdim(in_zdim)
{
  if(cellsize.x<=0.0 || cellsize.y<=0.0 || cellsize.z<=0.0) {
    String msg = String("Invalid cellsize data in constructor of"
                        " refined rectangular mesh object ")
      + String(InstanceName());
    throw Oxs_ExtError(msg.c_str());
  }
  cellvolume=cellsize.x*cellsize.y*cellsize.z;

  if(xdim<1 || ydim<1 || zdim<1) {
    String msg = String("Invalid x/y/zdim data in constructor of"
                        " refined rectangular mesh object ")
      + String(InstanceName());
    throw Oxs_ExtError(msg.c_str());
  }

  // Overflow test; restrict to signed value range
  OC_INDEX testval = OC_INDEX((OC_UINDEX(1)<<(sizeof(OC_INDEX)*8-1))-1);
  /// Maximum allowed value; Assumes 2's complement arithmetic
  testval /= xdim;
  testval /= ydim;
  testval /= zdim;
  if(testval<1) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),"Requested refined mesh size"
                " (%lu x %lu x %lu)"
                " has too many elements",(unsigned long)xdim,
                (unsigned long)ydim,(unsigned long)zdim);
    throw Oxs_ExtError(this,buf);
  }

  xydim = xdim*ydim;
  elementcount = xydim*zdim;
}

void Oxs_CommonRectangularMesh::GetBoundingBox(Oxs_Box& bbox) const
{
  bbox.Set(base.x-cellsize.x/2., base.x-cellsize.x/2.+xdim*cellsize.x,
           base.y-cellsize.y/2., base.y-cellsize.y/2.+ydim*cellsize.y,
           base.z-cellsize.z/2., base.z-cellsize.z/2.+zdim*cellsize.z);
}

void
Oxs_CommonRectangularMesh::Center(OC_INDEX index,ThreeVector &location) const
{
#ifndef NDEBUG
  if(index>elementcount) {
    String msg = String("Index out of range "
      "(Oxs_CommonRectangularMesh::Location(OC_INDEX) const)");
    throw Oxs_ExtError(msg.c_str());
  }
#endif
  OC_INDEX iz = index/(xdim*ydim);  index -= iz*xdim*ydim;
  OC_INDEX iy = index/xdim;         index -= iy*xdim;
  OC_INDEX ix = index;
  location.Set(base.x+ix*cellsize.x,
               base.y+iy*cellsize.y,
               base.z+iz*cellsize.z);
  return;
}

OC_INDEX
Oxs_CommonRectangularMesh::FindNearestIndex(const ThreeVector& location) const
{ // Note: This code assumes cellsize.{x,y,z} are all > 0.
  ThreeVector pt = location - base;

  OC_INDEX ix=0;
  if(pt.x>0) {
    ix = static_cast<OC_INDEX>(OC_ROUND(pt.x / cellsize.x));
    if(ix>=xdim) ix = xdim-1;
  }

  OC_INDEX iy=0;
  if(pt.y>0) {
    iy = static_cast<OC_INDEX>(OC_ROUND(pt.y / cellsize.y));
    if(iy>=ydim) iy = ydim-1;
  }

  OC_INDEX iz=0;
  if(pt.z>0) {
    iz = static_cast<OC_INDEX>(OC_ROUND(pt.z / cellsize.z));
    if(iz>=zdim) iz = zdim-1;
  }

  return Index(ix,iy,iz);
}


OC_BOOL
Oxs_CommonRectangularMesh::GetNeighborPoint
(const ThreeVector& pt,
 OC_INDEX ngbr_index,
 ThreeVector& ngbr_pt) const
{ // Fills ngbr_pt with location of neighbor element indexed
  // by "ngbr_index", relative to pt.  Returns 1 if
  // ngbr_pt < number of neighbors (currently 6); otherwise
  // 0 is returned, in which case ngbr_pt is unchanged.
  // NB: ngbr_index is 0-based.
  if(ngbr_index>5) return 0;
  int sign = 1 - 2*(ngbr_index%2); // 0,2,4 => +1, 1,3,5 => -1
  ngbr_pt = pt;
  switch(ngbr_index/2) {
    case 0:  ngbr_pt.x += sign*cellsize.x; break;
    case 1:  ngbr_pt.y += sign*cellsize.y; break;
    default: ngbr_pt.z += sign*cellsize.z; break;
  }
  return 1;
}

OC_INDEX
Oxs_CommonRectangularMesh::BoundaryList
(const Oxs_Atlas& atlas,
 const String& region_name,
 const Oxs_ScalarField& bdry_surface,
 OC_REAL8m bdry_value,
 const String& bdry_side,
 vector<OC_INDEX> &BoundaryIndexList) const
{ // Boundary extraction.  Returns list (technically, an STL vector) of
  // indices for those elements inside base_region that have a neighbor
  // (in the 6 nearest ngbr sense) such that the first element lies on
  // one side (the "inside") of the surface specified by the
  // Oxs_ScalarField bdry_surface + bdry_value, and the neighbor lies on
  // the other (the "outside").  If the bdry_side argument is "<" or
  // "<=", then the "inside" of the surface is the set of those points x
  // for which bdry_surface.Value(x) < or <= (resp.) bdry_value. The
  // bdry_side arguments ">" and ">=" are treated analogously. For
  // backwards compatibility, "-" and "+" are accepted as synonyms for
  // <= and >=, respectively.
  // Return value is the number of entries in the export list.
  // NB: The tested neighbors may lie outside the mesh proper, allowing
  // elements on the edge of the mesh to be specified.
  BoundaryIndexList.clear();

  Oxs_Box world_box,work_box;
  GetBoundingBox(world_box);
  const OC_INDEX region_id = atlas.GetRegionId(region_name);
  if(region_id<0) {
    String msg=String("Region name ")
      + region_name
      + String(" not recognized by atlas ")
      + String(atlas.InstanceName())
      + String(" (Oxs_CommonRectangularMesh::BoundaryList() in object")
      + String(InstanceName())
      + String(").");
    msg += String("  Known regions:");
    vector<String> regions;
    atlas.GetRegionList(regions);
    for(unsigned int j=0;j<regions.size();++j) {
      msg += String("\n ");
      msg += regions[j];
    }
    throw Oxs_ExtError(msg);
  }
  atlas.GetRegionExtents(region_id,work_box);
  work_box.Intersect(world_box); // work_box contains extents of
  /// base_region contained inside mesh
  OC_INDEX ixstart = OC_INDEX(ceil((work_box.GetMinX()-base.x)/cellsize.x));
  OC_INDEX iystart = OC_INDEX(ceil((work_box.GetMinY()-base.y)/cellsize.y));
  OC_INDEX izstart = OC_INDEX(ceil((work_box.GetMinZ()-base.z)/cellsize.z));
  OC_INDEX ixstop = OC_INDEX(floor((work_box.GetMaxX()-base.x)/cellsize.x))+1;
  OC_INDEX iystop = OC_INDEX(floor((work_box.GetMaxY()-base.y)/cellsize.y))+1;
  OC_INDEX izstop = OC_INDEX(floor((work_box.GetMaxZ()-base.z)/cellsize.z))+1;

  // Check sign
  enum BdrySide { INVALID, LT, LE, GE, GT };
  BdrySide side_check = INVALID;
  if(bdry_side.compare("<")==0)       side_check = LT;
  else if(bdry_side.compare("<=")==0) side_check = LE;
  else if(bdry_side.compare(">=")==0) side_check = GE;
  else if(bdry_side.compare(">")==0)  side_check = GT;
  else if(bdry_side.compare("+")==0)  side_check = GE;
  else if(bdry_side.compare("-")==0)  side_check = LE;
  else {
    String msg=String("Invalid boundary side representation: ")
      + bdry_side
      + String("  Should be one of <, <=, >=, or >.")
      + String(" (Oxs_CommonRectangularMesh::BoundaryList() in object")
      + String(InstanceName())
      + String(")");
    throw Oxs_ExtError(msg.c_str());
  }
  const int check_sign = (side_check == LT || side_check == LE ? -1 : 1);
  const int equal_check = (side_check == LE || side_check == GE ?  1 : 0);

  OC_INDEX ixsize = ixstop-ixstart;
  for(OC_INDEX iz=izstart;iz<izstop;iz++) {
    for(OC_INDEX iy=iystart;iy<iystop;iy++) {
      OC_INDEX row_index = Index(ixstart,iy,iz);
      ThreeVector pt;
      Center(row_index,pt);
      for(OC_INDEX i=0; i<ixsize; ++i,pt.x+=cellsize.x) {
        if(check_sign*(bdry_surface.Value(pt)-bdry_value)<0 ||
           (!equal_check && bdry_surface.Value(pt) == bdry_value)) {
          continue; // basept on wrong side of boundary
        }
        if(atlas.GetRegionId(pt) != region_id) {
          continue; // Point not in specified region
        }
        OC_INDEX ngbr_index=0;
        ThreeVector ngbr_pt;
        while(GetNeighborPoint(pt,ngbr_index++,ngbr_pt)) {
          if(check_sign*(bdry_surface.Value(ngbr_pt)-bdry_value)<0 ||
             (!equal_check && bdry_surface.Value(ngbr_pt) == bdry_value)) {
            // Neighbor on "other" side of boundary
            BoundaryIndexList.push_back(row_index+i);
            break;  // Don't include pt more than once
          }
        }
      }
    }
  }
  return static_cast<OC_INDEX>(BoundaryIndexList.size());
}



// File (channel) output for ThreeVectors.  Throws an exception on error.
void Oxs_CommonRectangularMesh::WriteOvf
(Tcl_Channel channel,   // Output channel
 OC_BOOL headers,          // If false, then output only raw data
 const char* title,     // Long filename or title
 const char* desc,      // Description to embed in output file
 const char* valueunit, // Field units, such as "A/m".
 const char* meshtype,  // Either "rectangular" or "irregular"
 const char* datatype,  // Either "binary" or "text"
 const char* precision, // For binary, "4" or "8";
                        ///  for text, a printf-style format
 const Oxs_MeshValue<ThreeVector>* vec,  // Vector array
 const Oxs_MeshValue<OC_REAL8m>* scale      // Optional scaling for vec
 /// Set scale to NULL to use vec values directly.
 ) const
{
  // Use default file writer for irregular mesh.
  if(strcmp("irregular",meshtype)==0) {
    ThreeVector stephints(cellsize.x,cellsize.y,cellsize.z);
    WriteOvfIrregular(channel,headers,title,desc,valueunit,
                      datatype,precision,
                      vec,scale,&stephints);
    return;
  }

  if(strcmp("rectangular",meshtype)!=0) {
    String msg=String("Unrecognized mesh type request: ")
      + String(meshtype)
      + String("(Oxs_CommonRectangularMesh::WriteOvf() in object")
      + String(InstanceName()) + String(")");
    OXS_EXTTHROW(Oxs_BadParameter,msg.c_str(),-1);
  }

  // Rectangular Mesh ////////////////////////////
  // Check import validity
  enum DataType { BINARY, TEXT };
  DataType dt = BINARY;
  if(strcmp("text",datatype)==0) {
    dt = TEXT;
  } else if(strcmp("binary",datatype)!=0){
    String errmsg = String("Bad datatype: \"") + String(datatype)
      + String("\"  Should be either \"binary\" or \"text\"");
    OXS_EXTTHROW(Oxs_BadParameter,errmsg.c_str(),-1);
  }

  int datawidth = 0;
  String dataformat;
  if(dt == BINARY) {
    datawidth = atoi(precision);
    if(datawidth != 4 && datawidth != 8) {
      String errmsg = String("Bad precision: ") + String(precision)
        + String("  Should be either 4 or 8.");
      OXS_EXTTHROW(Oxs_BadParameter,errmsg.c_str(),-1);
    }
  } else {
    if(precision==NULL || precision[0]=='\0')
      precision="%# .17g";  // Default format
    String temp = String(precision) + String(" ");
    dataformat = temp + temp + String(precision) + String("\n");
  }

  if(!vec->CheckMesh(this)) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),"Size mismatch; input data length=%u,"
                " which is different than mesh size=%u",
                vec->Size(),Size());
    OXS_EXTTHROW(Oxs_BadParameter,buf,-1);
  }

  if(scale!=NULL && !scale->CheckMesh(this)) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),"Size mismatch; input scale data length=%u,"
                " which is different than mesh size=%u",
                scale->Size(),Size());
    OXS_EXTTHROW(Oxs_BadParameter,buf,-1);
  }

  if(headers) {
    try {
      // Write header
      Nb_FprintfChannel(channel,NULL,1024,"# OOMMF: rectangular mesh v1.0\n");
      Nb_FprintfChannel(channel,NULL,1024,"# Segment count: 1\n");
      Nb_FprintfChannel(channel,NULL,1024,"# Begin: Segment\n");
      Nb_FprintfChannel(channel,NULL,1024,"# Begin: Header\n");
      if(title==NULL || title[0]=='\0')
        Nb_FprintfChannel(channel,NULL,1024,"# Title: unknown\n");
      else
        Nb_FprintfChannel(channel,NULL,1024,"# Title: %s\n",title);
      if(desc!=NULL && desc[0]!='\0') {
        // Print out description, breaking at newlines as necessary.
        // Note: This block is optional
        const char *cptr1,*cptr2;
        cptr1=desc;
        while((cptr2=strchr(cptr1,'\n'))!=NULL) {
          Nb_FprintfChannel(channel,NULL,1024,"# Desc: %.*s\n",
                            (int)(cptr2-cptr1),cptr1);
          cptr1=cptr2+1;
        }
        if(*cptr1!='\0')
          Nb_FprintfChannel(channel,NULL,1024,"# Desc: %s\n",cptr1);
      }
      Nb_FprintfChannel(channel,NULL,1024,"# meshtype: rectangular\n");
      Nb_FprintfChannel(channel,NULL,1024,"# meshunit: m\n");

      Nb_FprintfChannel(channel,NULL,1024,"# xbase: %.17g\n"
                        "# ybase: %.17g\n# zbase: %.17g\n",
                        static_cast<double>(base.x),
                        static_cast<double>(base.y),
                        static_cast<double>(base.z));
      Nb_FprintfChannel(channel,NULL,1024,"# xstepsize: %.17g\n"
                        "# ystepsize: %.17g\n# zstepsize: %.17g\n",
                        static_cast<double>(cellsize.x),
                        static_cast<double>(cellsize.y),
                        static_cast<double>(cellsize.z));
      Nb_FprintfChannel(channel,NULL,1024,"# xnodes: %d\n"
                        "# ynodes: %d\n# znodes: %d\n",
                        xdim,ydim,zdim);

      Oxs_Box bbox;
      GetBoundingBox(bbox);
      Nb_FprintfChannel(channel,NULL,1024,
                        "# xmin: %.17g\n# ymin: %.17g\n# zmin: %.17g\n"
                        "# xmax: %.17g\n# ymax: %.17g\n# zmax: %.17g\n",
                        static_cast<double>(bbox.GetMinX()),
                        static_cast<double>(bbox.GetMinY()),
                        static_cast<double>(bbox.GetMinZ()),
                        static_cast<double>(bbox.GetMaxX()),
                        static_cast<double>(bbox.GetMaxY()),
                        static_cast<double>(bbox.GetMaxZ()));

      Nb_FprintfChannel(channel,NULL,1024,"# valueunit: %s\n",valueunit);
      Nb_FprintfChannel(channel,NULL,1024,"# valuemultiplier: 1\n");

      // As of 6/2001, mmDisp supports display of out-of-plane rotations;
      // Representing the boundary under these conditions is awkward with
      // a single polygon.  So don't write the boundary line, and rely
      // on defaults based on the data range and step size.

      OC_REAL8m minmag=0,maxmag=0;
      if(Size()>0) {
        minmag=DBL_MAX;
        for(OC_INDEX i=0;i<Size();i++) {
          OC_REAL8m val=(*vec)[i].MagSq();
          if(scale!=NULL) {
            OC_REAL8m tempscale=(*scale)[i];
            val*=tempscale*tempscale;
          }
          if(val<minmag && val>0) minmag=val; // minmag is smallest non-zero
          if(val>maxmag) maxmag=val;         /// magnitude.
        }
        if(minmag>maxmag) minmag=maxmag;
        maxmag=sqrt(maxmag);
        minmag=sqrt(minmag);
        minmag*=0.9999;  // Underestimate lower bound by 0.01% to protect
        /// against rounding errors.
      }

      Nb_FprintfChannel(channel,NULL,1024,
                        "# ValueRangeMinMag: %.17g\n",
                        static_cast<double>(minmag));
      Nb_FprintfChannel(channel,NULL,1024,
                        "# ValueRangeMaxMag: %.17g\n",
                        static_cast<double>(maxmag));

      Nb_FprintfChannel(channel,NULL,1024,"# End: Header\n");
    } catch(...) {
    OXS_EXTTHROW(Oxs_DeviceFull,
                 "Error writing OVF file header;"
                 " disk full or buffer overflow?",-1);
    }
  }

  // Write data block
  try {
    if(dt == BINARY) {
      if(datawidth==4) {
        OC_REAL4 buf[3];
        if(headers) {
          Nb_FprintfChannel(channel,NULL,1024,"# Begin: Data Binary 4\n");
          buf[0]=1234567.;  // 4-Byte checkvalue
          if(Vf_OvfFileFormatSpecs::WriteBinary(channel,buf,1)) {
            throw Oxs_CommonRectangularMesh_WBError();
          }
        }
        OC_INDEX size=Size();
        for(OC_INDEX i=0 ; i<size ; ++i) {
          const ThreeVector& v = (*vec)[i];
          if(scale==NULL) {
            buf[0] = static_cast<OC_REAL4>(v.x);
            buf[1] = static_cast<OC_REAL4>(v.y);
            buf[2] = static_cast<OC_REAL4>(v.z);
          } else {
            OC_REAL8m tempscale=(*scale)[i];
            buf[0] = static_cast<OC_REAL4>(tempscale*v.x);
            buf[1] = static_cast<OC_REAL4>(tempscale*v.y);
            buf[2] = static_cast<OC_REAL4>(tempscale*v.z);
          }
          // Vf_OvfFileFormatSpecs::WriteBinary performs
          // byte-swapping as needed.
          if(Vf_OvfFileFormatSpecs::WriteBinary(channel,buf,3)) {
            throw Oxs_CommonRectangularMesh_WBError();
          }
        }
        if(headers) {
          Nb_FprintfChannel(channel,NULL,1024,"\n# End: Data Binary 4\n");
        }
      } else {
        // datawidth==8
        OC_REAL8 buf[3];
        if(headers) {
          Nb_FprintfChannel(channel,NULL,1024,"# Begin: Data Binary 8\n");
          buf[0]=123456789012345.;  // 8-Byte checkvalue
          if(Vf_OvfFileFormatSpecs::WriteBinary(channel,buf,1)) {
            throw Oxs_CommonRectangularMesh_WBError();
          }
        }
        OC_INDEX size=Size();
        for(OC_INDEX i=0 ; i<size ; ++i) {
          const ThreeVector& v = (*vec)[i];
          if(scale==NULL) {
            buf[0] = static_cast<OC_REAL8>(v.x);
            buf[1] = static_cast<OC_REAL8>(v.y);
            buf[2] = static_cast<OC_REAL8>(v.z);
          } else {
            OC_REAL8m tempscale=(*scale)[i];
            buf[0] = static_cast<OC_REAL8>(tempscale*v.x);
            buf[1] = static_cast<OC_REAL8>(tempscale*v.y);
            buf[2] = static_cast<OC_REAL8>(tempscale*v.z);
          }
          // Vf_OvfFileFormatSpecs::WriteBinary performs
          // byte-swapping as needed.
          if(Vf_OvfFileFormatSpecs::WriteBinary(channel,buf,3)) {
            throw Oxs_CommonRectangularMesh_WBError();
          }
        }
        if(headers) {
          Nb_FprintfChannel(channel,NULL,1024,"\n# End: Data Binary 8\n");
        }
      }
    } else {
      if(headers) {
        Nb_FprintfChannel(channel,NULL,1024,"# Begin: Data Text\n");
      }
      OC_INDEX size=Size();
      for(OC_INDEX i=0 ; i<size ; ++i) {
        const ThreeVector& v = (*vec)[i];
        if(scale==NULL) {
          Nb_FprintfChannel(channel,NULL,1024,dataformat.c_str(),
                            static_cast<double>(v.x),
                            static_cast<double>(v.y),
                            static_cast<double>(v.z));
        } else {
          OC_REAL8m tempscale=(*scale)[i];
          Nb_FprintfChannel(channel,NULL,1024,dataformat.c_str(),
                            static_cast<double>(tempscale*v.x),
                            static_cast<double>(tempscale*v.y),
                            static_cast<double>(tempscale*v.z));
        }
      }
      if(headers) {
        Nb_FprintfChannel(channel,NULL,1024,"# End: Data Text\n");
      }
    }
    if(headers) {
      Nb_FprintfChannel(channel,NULL,1024,"# End: Segment\n");
    }
  } catch(Oxs_CommonRectangularMesh_WBError&) {
    OXS_EXTTHROW(Oxs_DeviceFull,
                 "Error writing OVF file binary data block;"
                 " disk full?",-1);
  } catch(...) {
    OXS_EXTTHROW(Oxs_DeviceFull,
                 "Error writing OVF file data block;"
                 " disk full or buffer overflow?",-1);
  }
}

////////////////////////////////////////////////////////////////////
// Geometry string from common rectangular mesh interface.
String Oxs_CommonRectangularMesh::GetGeometryString() const {
  char buf[1024];
  Oc_Snprintf(buf,sizeof(buf),"%ld x %ld x %ld = %ld cells",
              (long)xdim,(long)ydim,(long)zdim,(long)Size());
  return String(buf);
}

/////////////////////////////////////////////////////////////////
// Vf_Ovf20_MeshNodes interface function DumpGeometry.
void Oxs_CommonRectangularMesh::DumpGeometry
(Vf_Ovf20FileHeader& header,
 Vf_Ovf20_MeshType type) const
{
  if(type == vf_ovf20mesh_irregular) {
    DumpIrregGeometry(header);
    header.xstepsize.Set(cellsize.x);
    header.ystepsize.Set(cellsize.y);
    header.zstepsize.Set(cellsize.z);
    if(!header.IsValidGeom()) {
      String msg=String("Invalid header (irregular mesh type) in"
                        " Oxs_CommonRectangularMesh::DumpGeometry()"
                        " in object ")
        + String(InstanceName());
      throw Oxs_ExtError(msg.c_str());
    }
    return;
  }

  if(type != vf_ovf20mesh_rectangular) {
    String msg=String("Unrecognized mesh type request in"
                      " Oxs_CommonRectangularMesh::DumpGeometry()"
                      " in object ")
      + String(InstanceName());
    throw Oxs_ExtError(msg.c_str());
  }


  header.meshtype.Set(vf_ovf20mesh_rectangular);
  header.meshunit.Set(String("m"));

  Oxs_Box bbox;
  GetBoundingBox(bbox);
  header.xmin.Set(bbox.GetMinX());
  header.ymin.Set(bbox.GetMinY());
  header.zmin.Set(bbox.GetMinZ());
  header.xmax.Set(bbox.GetMaxX());
  header.ymax.Set(bbox.GetMaxY());
  header.zmax.Set(bbox.GetMaxZ());

  header.xbase.Set(base.x);
  header.ybase.Set(base.y);
  header.zbase.Set(base.z);
  header.xnodes.Set(xdim);
  header.ynodes.Set(ydim);
  header.znodes.Set(zdim);
  header.xstepsize.Set(cellsize.x);
  header.ystepsize.Set(cellsize.y);
  header.zstepsize.Set(cellsize.z);

  if(!header.IsValidGeom()) {
    String msg=String("Invalid header (rectangular mesh type) in"
                      " Oxs_CommonRectangularMesh::DumpGeometry()"
                      " in object ")
      + String(InstanceName());
    throw Oxs_ExtError(msg.c_str());
  }
}

//////////////////////////////////////////////////////////////////////
// Conversion routines from Vf_Mesh to Oxs_MeshValue<ThreeVector>.
// IsCompatible returns true iff vfmesh is a Vf_GridVec3f with
//   dimensions identical to those of *this.
// NB: IsCompatible only compares the mesh dimensions, not the
//   underlying physical scale, or the cell aspect ratios.  Do
//   we want to include such a check?, or is it more flexible
//   to leave it out?
// FillMeshValueExact copies the vector field held in vfmesh to the
//   export Oxs_MeshValue<ThreeVector> vec.  This routine throws
//   an exception on error, the primary cause of which is that
//   vfmesh is not compatible with *this.  In other words, if you
//   don't want to catch the exception, call IsCompatible first.
//   The "Exact" in the name refers to the requirement that the
//   dimensions on vfmesh exactly match those of *this.

OC_BOOL Oxs_CommonRectangularMesh::IsCompatible(const Vf_Mesh* vfmesh) const
{
  const Vf_GridVec3f* gridmesh
    = dynamic_cast<const Vf_GridVec3f*>(vfmesh);
  if(gridmesh==NULL) return 0;

  OC_INDEX isize,jsize,ksize;
  gridmesh->GetDimens(isize,jsize,ksize);
  if(xdim != isize ||
     ydim != jsize ||
     zdim != ksize   ) {
    return 0;
  }

  return 1;
}

void Oxs_CommonRectangularMesh::FillMeshValueExact
(const Vf_Mesh* vfmesh,
 Oxs_MeshValue<ThreeVector>& vec) const
{
  const Vf_GridVec3f* gridmesh
    = dynamic_cast<const Vf_GridVec3f*>(vfmesh);
  if(gridmesh==NULL || !IsCompatible(vfmesh)) {
    throw Oxs_ExtError(this,
        "Incompatible Vf_Mesh import to"
        " FillMeshValue(const Vf_Mesh*,"
        " Oxs_MeshValue<ThreeVector>&)");
  }
  vec.AdjustSize(this);

  // Both Oxs_CommonRectangularMesh and Vf_GridVec3f access with the
  // x-dimension index changing fastest, z-dimension index changing
  // slowest.
  OC_INDEX i,j,k;
  OC_REAL8m scale = gridmesh->GetValueMultiplier();
  if(scale==1.0) { // Common case?
    for(k=0;k<zdim;++k) for(j=0;j<ydim;++j) for(i=0;i<xdim;++i) {
      const Nb_Vec3<OC_REAL8>& nbvec = gridmesh->GridVec(i,j,k);
      vec[Index(i,j,k)].Set(nbvec.x,nbvec.y,nbvec.z);
    }
  } else {
    for(k=0;k<zdim;++k) for(j=0;j<ydim;++j) for(i=0;i<xdim;++i) {
      const Nb_Vec3<OC_REAL8>& nbvec = gridmesh->GridVec(i,j,k);
      vec[Index(i,j,k)].Set(scale*nbvec.x,
                            scale*nbvec.y,
                            scale*nbvec.z);
    }
  }
}


//////////////////////////////////////////////////////////////////////
// Volume summing routines.  This have advantage over the generic
// in that for Oxs_CommonRectangularMesh all cells have same volume.
OC_REAL8m Oxs_CommonRectangularMesh::VolumeSum
(const Oxs_MeshValue<OC_REAL8m>& scalar) const
{
  if(!scalar.CheckMesh(this)) {
    throw Oxs_ExtError(this,
                         "Incompatible scalar array import to"
                         " VolumeSum(const Oxs_MeshValue<OC_REAL8m>&)");
  }
  const OC_INDEX size=Size();
  OC_REAL8m sum=0.0;
  for(OC_INDEX i=0;i<size;i++) sum += scalar[i];
  sum *= cellvolume;
  return sum;
}

ThreeVector Oxs_CommonRectangularMesh::VolumeSum
(const Oxs_MeshValue<ThreeVector>& vec) const
{
  if(!vec.CheckMesh(this)) {
    throw Oxs_ExtError(this,
                         "Incompatible import array to"
                         " VolumeSum(const Oxs_MeshValue<ThreeVector>&)");
  }
  const OC_INDEX size=Size();
  ThreeVector sum(0.,0.,0.);
  for(OC_INDEX i=0;i<size;i++) sum += vec[i] ;
  sum *= cellvolume;
  return sum;
}

ThreeVector Oxs_CommonRectangularMesh::VolumeSum
(const Oxs_MeshValue<ThreeVector>& vec,
 const Oxs_MeshValue<OC_REAL8m>& scale
 ) const
{
  if(!vec.CheckMesh(this) || !scale.CheckMesh(this)) {
    throw Oxs_ExtError(this,
                         "Incompatible import array to"
                         " VolumeSum(const Oxs_MeshValue<ThreeVector>&,"
                         " const Oxs_MeshValue<OC_REAL8m>& scale)");
  }
  const OC_INDEX size=Size();
  ThreeVector sum(0.,0.,0.);
  for(OC_INDEX i=0;i<size;i++) {
    sum +=  scale[i] * vec[i];
  }
  sum *= cellvolume;
  return sum;
}


OC_REAL8m Oxs_CommonRectangularMesh::VolumeSumXp
(const Oxs_MeshValue<OC_REAL8m>& scalar) const
{
  if(!scalar.CheckMesh(this)) {
    throw Oxs_ExtError(this,
                         "Incompatible scalar array import to"
                         " VolumeSumXp(const Oxs_MeshValue<OC_REAL8m>&)");
  }

  Nb_Xpfloat sum=0.;
  const OC_INDEX size=Size();
  for(OC_INDEX i=0;i<size;i++) sum += scalar[i];
  sum *= cellvolume;
  return sum.GetValue();
}

ThreeVector Oxs_CommonRectangularMesh::VolumeSumXp
(const Oxs_MeshValue<ThreeVector>& vec) const
{
  if(!vec.CheckMesh(this)) {
    throw Oxs_ExtError(this,
                         "Incompatible import array to"
                         " VolumeSumXp(const Oxs_MeshValue<ThreeVector>&)");
  }

  Nb_Xpfloat sum_x=0.,sum_y=0.,sum_z=0.;
  const OC_INDEX size=Size();
  for(OC_INDEX i=0;i<size;i++) {
    sum_x += vec[i].x;
    sum_y += vec[i].y;
    sum_z += vec[i].z;
  }
  sum_x *= cellvolume;
  sum_y *= cellvolume;
  sum_z *= cellvolume;
  return ThreeVector(sum_x.GetValue(),sum_y.GetValue(),sum_z.GetValue());
}

ThreeVector Oxs_CommonRectangularMesh::VolumeSumXp
(const Oxs_MeshValue<ThreeVector>& vec,
 const Oxs_MeshValue<OC_REAL8m>& scale
 ) const
{
  if(!vec.CheckMesh(this) || !scale.CheckMesh(this)) {
    throw Oxs_ExtError(this,
                         "Incompatible import array to"
                         " VolumeSumXp(const Oxs_MeshValue<ThreeVector>&,"
                         " const Oxs_MeshValue<OC_REAL8m>& scale)");
  }

  Nb_Xpfloat sum_x=0.,sum_y=0.,sum_z=0.;
  const OC_INDEX size=Size();
  for(OC_INDEX i=0;i<size;i++) {
    OC_REAL8m wgt = scale[i];
    sum_x += wgt*vec[i].x;
    sum_y += wgt*vec[i].y;
    sum_z += wgt*vec[i].z;
  }
  sum_x *= cellvolume;
  sum_y *= cellvolume;
  sum_z *= cellvolume;
  return ThreeVector(sum_x.GetValue(),sum_y.GetValue(),sum_z.GetValue());
}



/////////////////////////////////////////////////////////////////////
// Oxs_RectangularMesh  (non-periodic)

// Main constructor, for use by Specify command in MIF input file
Oxs_RectangularMesh::Oxs_RectangularMesh
(const char* name,     // Child instance id
 Oxs_Director* newdtr, // App director
 const char* argstr)   // MIF input block parameters
  : Oxs_CommonRectangularMesh(name,newdtr,argstr)
{
  VerifyAllInitArgsUsed();
}

void Oxs_RectangularMesh::MakeRefinedMesh
(const char* name,
 OC_INDEX refine_x,OC_INDEX refine_y,OC_INDEX refine_z,
 Oxs_OwnedPointer<Oxs_RectangularMesh>& out_mesh) const
{
  if(refine_x<1 || refine_y<1 || refine_z<1) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),"Invalid refinment request:"
                " (%lu x %lu x %lu)"
                " All refinement values must be >=1;"
                " Error in MakeRefinedMesh function in"
                " rectangular mesh object ",
                (unsigned long)refine_x,
                (unsigned long)refine_y,
                (unsigned long)refine_z);
    String msg = String(buf) + String(InstanceName());
    throw Oxs_ExtError(msg.c_str());
  }
  ThreeVector new_cellsize = GetCellsize();
  new_cellsize.x /= static_cast<OC_REAL8m>(refine_x);
  new_cellsize.y /= static_cast<OC_REAL8m>(refine_y);
  new_cellsize.z /= static_cast<OC_REAL8m>(refine_z);

  ThreeVector new_base = GetBase();
  new_base.Accum(-0.5,GetCellsize());
  new_base.Accum( 0.5,new_cellsize);

  Oxs_RectangularMesh *refined_mesh
    = new Oxs_RectangularMesh(name,director,new_base,new_cellsize,
                              refine_x*DimX(),
                              refine_y*DimY(),
                              refine_z*DimZ());

  out_mesh.SetAsOwner(refined_mesh);
}

// Max angle routine.  This routine returns the maximum angle
// between neighboring vectors across the mesh, for all
// those vectors for which the corresponding entry in zero_check
// is non-zero.
OC_REAL8m Oxs_RectangularMesh::MaxNeighborAngle
(const Oxs_MeshValue<ThreeVector>& vec,
 const Oxs_MeshValue<OC_REAL8m>& zero_check,
 OC_INDEX node_start,OC_INDEX node_stop) const
{
  if(!vec.CheckMesh(this)) {
    throw Oxs_ExtError(this,
                  "Incompatible import array to"
                  " MaxNeighborAngle(const Oxs_MeshValue<ThreeVector>&,"
                  " const Oxs_MeshValue<OC_REAL8m>& scale)");
  }
  if(Size()<2) return 0.0;
  
  OxsRectangularMeshAngle maxangX(0.0); // 0.0 is min possible angle
  OxsRectangularMeshAngle maxangY(0.0);
  OxsRectangularMeshAngle maxangZ(0.0);

  const OC_INDEX dimx = DimX();  // For convenience
  const OC_INDEX dimy = DimY();  // For convenience
  const OC_INDEX dimz = DimZ();  // For convenience
  const OC_INDEX dimxy = dimx*dimy;

  OC_INDEX ix,iy,iz;
  GetCoords(node_start,ix,iy,iz);

  for(OC_INDEX index=node_start;index<node_stop;++index) {
    if(zero_check[index]!=0.0) {
      if(ix+1<dimx && zero_check[index+1]!=0.0) {
        OxsRectangularMeshAngle test(vec[index],vec[index+1]);
        if(test>maxangX) maxangX = test;
      }
      if(iy+1<dimy && zero_check[index+dimx]!=0.0) {
        OxsRectangularMeshAngle test(vec[index],vec[index+dimx]);
        if(test>maxangY) maxangY = test;
      }
      if(iz+1<dimz && zero_check[index+dimxy]!=0.0) {
        OxsRectangularMeshAngle test(vec[index],vec[index+dimxy]);
        if(test>maxangZ) maxangZ = test;
      }
    }
    if(++ix >= dimx) {
      ix=0;
      if(++iy >= dimy) {
        iy=0; ++iz;
      }
    }
  }

  if(maxangY > maxangX) maxangX = maxangY;
  if(maxangZ > maxangX) maxangX = maxangZ;

  return maxangX.GetAngle();
}

/////////////////////////////////////////////////////////////////////
// Oxs_PeriodicRectangularMesh

// Main constructor, for use by Specify command in MIF input file
Oxs_PeriodicRectangularMesh::Oxs_PeriodicRectangularMesh
(const char* name,     // Child instance id
 Oxs_Director* newdtr, // App director
 const char* argstr)   // MIF input block parameters
  : Oxs_CommonRectangularMesh(name,newdtr,argstr),
    xperiodic(0),yperiodic(0),zperiodic(0)
{
  // Periodic boundary selection.
  String periodic = GetStringInitValue("periodic");
  int badbits=0;
  for(size_t i=0;i<periodic.size();++i) {
    switch(tolower(periodic[i])) {
    case 'x': xperiodic=1; break;
    case 'y': yperiodic=1; break;
    case 'z': zperiodic=1; break;
    default:  ++badbits; break;
    }
  }
  if(badbits>0) {
    String msg=String("Invalid periodic request string: ")
      + periodic
      + String("\n Should be a non-empty subset of \"xyz\".");
    throw Oxs_ExtError(this,msg.c_str());
  }
  if(xperiodic+yperiodic+zperiodic==0) {
    String msg=String("Invalid periodic request string ---"
                      " no periodic directions requested. "
                      " For non-periodic systems use"
                      " Oxs_RectangularMesh.");
    throw Oxs_ExtError(this,msg.c_str());
  }

  VerifyAllInitArgsUsed();
}

// Max angle routine.  This routine returns the maximum angle
// between neighboring vectors across the mesh, for all
// those vectors for which the corresponding entry in zero_check
// is non-zero.
OC_REAL8m Oxs_PeriodicRectangularMesh::MaxNeighborAngle
(const Oxs_MeshValue<ThreeVector>& vec,
 const Oxs_MeshValue<OC_REAL8m>& zero_check,
 OC_INDEX node_start,OC_INDEX node_stop) const
{
  if(!vec.CheckMesh(this)) {
    throw Oxs_ExtError(this,
                  "Incompatible import array to"
                  " MaxNeighborAngle(const Oxs_MeshValue<ThreeVector>&,"
                  " const Oxs_MeshValue<OC_REAL8m>& scale)");
  }
  if(Size()<2) return 0.0;
  
  OxsRectangularMeshAngle maxangX(0.0); // 0.0 is min possible angle
  OxsRectangularMeshAngle maxangY(0.0);
  OxsRectangularMeshAngle maxangZ(0.0);

  const OC_INDEX dimx = DimX();  // For convenience
  const OC_INDEX dimy = DimY();  // For convenience
  const OC_INDEX dimz = DimZ();  // For convenience
  const OC_INDEX dimxy = dimx*dimy;
  const OC_INDEX zwrap = (dimz-1)*dimxy;

  OC_INDEX ix,iy,iz;
  GetCoords(node_start,ix,iy,iz);

  for(OC_INDEX index=node_start;index<node_stop;++index) {
    if(zero_check[index]!=0.0) {
      if(ix+1<dimx) {
        if(zero_check[index+1]!=0.0) {
          OxsRectangularMeshAngle test(vec[index],vec[index+1]);
          if(test>maxangX) maxangX = test;
        }
      } else if(xperiodic && zero_check[index+1-dimx]!=0) {
        OxsRectangularMeshAngle test(vec[index],vec[index+1-dimx]);
        if(test>maxangX) maxangX = test;
      }
      if(iy+1<dimy) {
        if(zero_check[index+dimx]!=0.0) {
          OxsRectangularMeshAngle test(vec[index],vec[index+dimx]);
          if(test>maxangY) maxangY = test;
        }
      } else if(yperiodic && zero_check[index+dimx-dimxy]!=0) {
        OxsRectangularMeshAngle test(vec[index],vec[index+dimx-dimxy]);
        if(test>maxangY) maxangY = test;
      }
      if(iz+1<dimz) {
        if(zero_check[index+dimxy]!=0.0) {
          OxsRectangularMeshAngle test(vec[index],vec[index+dimxy]);
          if(test>maxangZ) maxangZ = test;
        }
      } else if(zperiodic && zero_check[index-zwrap]!=0) {
        OxsRectangularMeshAngle test(vec[index],vec[index-zwrap]);
        if(test>maxangZ) maxangZ = test;
      }
    }
    if(++ix >= dimx) {
      ix=0;
      if(++iy >= dimy) {
        iy=0; ++iz;
      }
    }
  }
  if(maxangY > maxangX) maxangX = maxangY;
  if(maxangZ > maxangX) maxangX = maxangZ;
  return maxangX.GetAngle();
}
