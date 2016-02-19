/* FILE: boxatlas.cc                 -*-Mode: c++-*-
 *
 * Atlas class derived from Oxs_Atlas class that uses an import
 * image file for region demarcation.
 */

#include <vector>
#include "nb.h"
#include "director.h"
#include "boxatlas.h"

OC_USE_STD_NAMESPACE;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_BoxAtlas);

/* End includes */


// Constructor
Oxs_BoxAtlas::Oxs_BoxAtlas
( const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr    // MIF block argument string
  ) : Oxs_Atlas(name,newdtr,argstr)
{
  // Process input string
  vector<OC_REAL8m> xrange; GetRealVectorInitValue("xrange",2,xrange);
  vector<OC_REAL8m> yrange; GetRealVectorInitValue("yrange",2,yrange);
  vector<OC_REAL8m> zrange; GetRealVectorInitValue("zrange",2,zrange);
  if(!world.CheckOrderSet(xrange[0],xrange[1],
			  yrange[0],yrange[1],
			  zrange[0],zrange[1])) {
    throw Oxs_ExtError(this,"One of [xyz]range in Specify block"
			 " is improperly ordered.");
  }

  String instance_tail = ExtractInstanceTail(InstanceName());
  region_name = GetStringInitValue("name",instance_tail);

  VerifyAllInitArgsUsed();
}

OC_BOOL Oxs_BoxAtlas::GetRegionExtents(OC_INDEX id,Oxs_Box &mybox) const
{ // If id is 0 or 1, sets mybox to world and returns 1.
  // If id > 1, leaves mybox untouched and returns 0.
  if(id>=GetRegionCount()) return 0;
  mybox = world;
  return 1;
}

OC_INDEX Oxs_BoxAtlas::GetRegionId(const ThreeVector& point) const
{ // Returns the id number for the region containing point.
  // The return value is 0 if the point is not contained in
  // the atlas, i.e., belongs to the "universe" region.
  if(!world.IsIn(point)) return 0; // Outside atlas
  return 1; // Oxs_BoxAtlas only has one region.
}

OC_INDEX Oxs_BoxAtlas::GetRegionId(const String& name) const
{ // Given a region id string (name), returns
  // the corresponding region id index.  If
  // "name" is not included in the atlas, then
  // -1 is returned.  Note: If name == "universe",
  // then the return value will be 0.
  if(name.compare(region_name)==0) return 1;
  if(name.compare("universe")==0) return 0;
  return -1;
}

OC_BOOL Oxs_BoxAtlas::GetRegionName(OC_INDEX id,String& name) const
{ // Given an id number, fills in "name" with
  // the corresponding region id string.  Returns
  // 1 on success, 0 if id is invalid.  If id is 0,
  // then name is set to "universe", and the return
  // value is 1.
  name.erase();
  if(id>=GetRegionCount()) return 0;
  if(id==0) name = "universe";
  else      name = region_name;
  return 1;
}


