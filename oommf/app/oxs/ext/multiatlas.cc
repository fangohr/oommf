/* FILE: multiatlas.cc                 -*-Mode: c++-*-
 *
 * Combining atlas class, derived from OXS extension class.
 *
 */

#include "nb.h"
#include "multiatlas.h"

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_MultiAtlas);

/* End includes */

// Constructor
Oxs_MultiAtlas::Oxs_MultiAtlas
( const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr    // MIF block argument string
  ) : Oxs_Atlas(name,newdtr)
{
  // Initialize global lists
  region_name.push_back("universe");
  region_bbox.resize(1); // Starts out empty

  // Use Tcl_SplitList to break argstr into components
  Nb_SplitList arglist;
  if(arglist.Split(argstr)!=TCL_OK) {
    // Input string not a valid Tcl list; Throw exception
    String msg= String("MIF input block is not a valid Tcl list: ")
      + String(argstr);
    throw Oxs_ExtError(this,msg.c_str());
  }
  if(arglist.Count()%2 != 0) {
    // Input string is not a list of pairs; Throw exception
    String msg=String("MIF input block does not have"
		      " an even number of elements: ")
      + String(argstr);
    throw Oxs_ExtError(this,msg.c_str());
  }

  // Scan through arglist to determine how many subatlases
  // will be needed.
  OC_INDEX subatlas_count=0;
  int i;
  for(i=0;i+1<arglist.Count();i+=2) {
    if(strcmp(arglist[i],"atlas")==0) ++subatlas_count;
  }
  subatlas.SetSize(subatlas_count);
  subatlas_count=0; // Reset

  // Each pair is either the keyword "atlas" followed by an
  // atlas reference, or else a x/y/zrange spec.
  OC_REAL8m xmin=1.,xmax=0.,ymin=1.,ymax=0.,zmin=1.,zmax=0.;
  OC_BOOL xrange_set=0,yrange_set=0,zrange_set=0;
  Nb_SplitList paramlist;
  for(i=0;i+1<arglist.Count();i+=2) {
    // Skip "comment" pairs
    if(strcmp(arglist[i],"comment")==0) continue;

    // Otherwise, branch handling using keywords
    // xrange, yrange, zrange and atlas.
    paramlist.Split(arglist[i+1]);
    if(strcmp(arglist[i],"xrange")==0) {
      OC_BOOL errcheck=0;
      if(paramlist.Count()!=2) errcheck=1;
      if(!errcheck) xmin = Nb_Atof(paramlist[0],errcheck);
      if(!errcheck) xmax = Nb_Atof(paramlist[1],errcheck);
      if(errcheck) {
	String msg = String("Invalid xrange data: ")
	  + String(arglist[i+1]);
	throw Oxs_ExtError(this,msg.c_str());
      } else {
	xrange_set = 1;
      }
    } else if(strcmp(arglist[i],"yrange")==0) {
      OC_BOOL errcheck=0;
      if(paramlist.Count()!=2) errcheck=1;
      if(!errcheck) ymin = Nb_Atof(paramlist[0],errcheck);
      if(!errcheck) ymax = Nb_Atof(paramlist[1],errcheck);
      if(errcheck) {
	String msg = String("Invalid yrange data: ")
	  + String(arglist[i+1]);
	throw Oxs_ExtError(this,msg.c_str());
      } else {
	yrange_set = 1;
      }
    } else if(strcmp(arglist[i],"zrange")==0) {
      OC_BOOL errcheck=0;
      if(paramlist.Count()!=2) errcheck=1;
      if(!errcheck) zmin = Nb_Atof(paramlist[0],errcheck);
      if(!errcheck) zmax = Nb_Atof(paramlist[1],errcheck);
      if(errcheck) {
	String msg = String("Invalid zrange data: ")
	  + String(arglist[i+1]);
	throw Oxs_ExtError(this,msg.c_str());
      } else {
	zrange_set = 1;
      }
    } else if(strcmp(arglist[i],"atlas")==0) {
      // Update last Oxs_OwnedPointer<Oxs_Atlas> subatlas
      // entry.  Note that corresponding global_id array
      // is initially empty
      SubAtlas& newatlas = subatlas[subatlas_count++];
      vector<String> newatlas_params;
      paramlist.FillParams(newatlas_params);
      OXS_GET_EXT_OBJECT(newatlas_params,Oxs_Atlas,newatlas.atlas);
      // SHOULD WE PUT A READ LOCK ON newatlas.atlas???

      // Add newatlas region names into region_name list as
      // necessary, adjust region_bbox's accordingly, and set
      // up the global_id mapping for newatlas.
      String temp_name;
      Oxs_Box temp_box;
      for(OC_INDEX j=0;j<newatlas.atlas->GetRegionCount();j++) {
	// Note: j==0 should always be "universe"
	newatlas.atlas->GetRegionName(j,temp_name);
	newatlas.atlas->GetRegionExtents(j,temp_box);
	OC_INDEX k;
	for(k=0;k<static_cast<OC_INDEX>(region_name.size());k++) {
	  if(temp_name.compare(region_name[k])==0) break;
	}
	if(k==static_cast<OC_INDEX>(region_name.size())) {
	  // New region.  Expand multiatlas data structures
	  region_name.push_back(temp_name);
	  region_bbox.push_back(temp_box);
	} else {
	  // Else, k<region_name.size() and name was already in use.
	  region_bbox[k].Expand(temp_box);
	}
	newatlas.global_id.push_back(k);
      }
    } else {
      String msg=String("Invalid MIF input block sublabel: <")
	+ String(arglist[i])
	+ String(">.  Should be one of"
		 " xrange, yrange, zrange or atlas.");
      throw Oxs_ExtError(this,msg.c_str());
    }
  }

  // Initialize multiatlas bbox
  if(!xrange_set) {
    xmin = region_bbox[0].GetMinX(); xmax = region_bbox[0].GetMaxX();
  }
  if(!yrange_set) {
    ymin = region_bbox[0].GetMinY(); ymax = region_bbox[0].GetMaxY();
  }
  if(!zrange_set) {
    zmin = region_bbox[0].GetMinZ(); zmax = region_bbox[0].GetMaxZ();
  }
  if(!region_bbox[0].CheckOrderSet(xmin,xmax,ymin,ymax,zmin,zmax)) {
    throw Oxs_ExtError(this,"Invalid range order.");
  }
}

OC_BOOL Oxs_MultiAtlas::GetRegionExtents(OC_INDEX id,Oxs_Box &mybox) const
{ // If 0<id<GetRegionCount, then sets mybox to bounding box
  // for the specified region, and returns 1.  If id is 0,
  // sets mybox to atlas bounding box (which is region_bbox[0]),
  // and returns 1. Otherwise, leaves mybox untouched and returns 0.
  if(id>=GetRegionCount()) return 0;
  mybox = region_bbox[id];
  return 1;
}

OC_INDEX Oxs_MultiAtlas::GetRegionId(const ThreeVector& point) const
{ // Returns the id number for the region containing point.
  // The return value is 0 if the point is not contained in
  // the atlas, i.e., belongs to the "universe" region.
  // The return value is guaranteed to be in the range
  // [0,GetRegionCount()-1].

  if(!region_bbox[0].IsIn(point)) return 0; // Point outside atlas
  /// This check is not for efficiency, but rather to support
  /// bboxes defined at the multiatlas level to restrict the size
  /// of the multiatlas to a subset of the union of all the
  /// subatlases.

  // Iterate through subatlas list, stopping at first atlas
  // that claims "point".
  for(OC_INDEX i=0;i<subatlas.GetSize();i++) {
    OC_INDEX id = subatlas[i].atlas->GetRegionId(point);
    if(id>0) return subatlas[i].global_id[id];
  }

  return 0; // Point not claimed by any subatlas.
}

OC_INDEX Oxs_MultiAtlas::GetRegionId(const String& name) const
{ // Given a region id string (name), returns
  // the corresponding region id index.  If
  // "name" is not included in the atlas, then
  // -1 is returned.  Note: If name == "universe",
  // then the return value will be 0.
  OC_INDEX count = GetRegionCount();
  for(OC_INDEX i=0;i<count;i++) {
    if(region_name[i].compare(name)==0) return i;
  }
  return -1;
}

OC_BOOL Oxs_MultiAtlas::GetRegionName(OC_INDEX id,String& name) const
{ // Given an id number, fills in "name" with
  // the corresponding region id string.  Returns
  // 1 on success, 0 if id is invalid.  If id is 0,
  // then name is set to "universe", and the return
  // value is 1.
  if(id>=GetRegionCount()) return 0;
  name = region_name[id];
  return 1;
}
