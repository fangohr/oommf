/* FILE: ellipsoidatlas.cc                 -*-Mode: c++-*-
 *
 * Atlas class derived from Oxs_Atlas class for ellipsoidal regions.
 */

#include <vector>
#include "nb.h"
#include "director.h"
#include "ellipsoidatlas.h"

OC_USE_STD_NAMESPACE;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_EllipsoidAtlas);

/* End includes */


// Constructor
Oxs_EllipsoidAtlas::Oxs_EllipsoidAtlas
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

  centerpt.x = 0.5*(xrange[0]+xrange[1]);
  centerpt.y = 0.5*(yrange[0]+yrange[1]);
  centerpt.z = 0.5*(zrange[0]+zrange[1]);

  invaxes.x = 0.5*fabs(xrange[1]-xrange[0]);
  if(invaxes.x>0.5 || 1<DBL_MAX*invaxes.x) invaxes.x = 1.0/invaxes.x;
  else                                     invaxes.x = 0.0;
  invaxes.y = 0.5*fabs(yrange[1]-yrange[0]);
  if(invaxes.y>0.5 || 1<DBL_MAX*invaxes.y) invaxes.y = 1.0/invaxes.y;
  else                                     invaxes.y = 0.0;
  invaxes.z = 0.5*fabs(zrange[1]-zrange[0]);
  if(invaxes.z>0.5 || 1<DBL_MAX*invaxes.z) invaxes.z = 1.0/invaxes.z;
  else                                     invaxes.z = 0.0;

  if(invaxes.x==0.0 || invaxes.y==0.0 || invaxes.z==0.0) {
    throw Oxs_ExtError(this,"One of [xyz]range in Specify block"
			 " has zero extent.");
  }

}

OC_BOOL Oxs_EllipsoidAtlas::GetRegionExtents(OC_INDEX id,Oxs_Box &mybox) const
{ // If id is 0 or 1, sets mybox to world and returns 1.
  // If id > 1, leaves mybox untouched and returns 0.
  if(id>=GetRegionCount()) return 0;
  mybox = world;
  return 1;
}

OC_INDEX Oxs_EllipsoidAtlas::GetRegionId(const ThreeVector& point) const
{ // Returns the id number for the region containing point.
  // The return value is 0 if the point is not contained in
  // the atlas, i.e., belongs to the "universe" region.
  if(!world.IsIn(point)) return 0; // Outside atlas
  OC_REAL8m xd = (point.x - centerpt.x)*invaxes.x;  xd *= xd;
  OC_REAL8m yd = (point.y - centerpt.y)*invaxes.y;  yd *= yd;
  OC_REAL8m zd = (point.z - centerpt.z)*invaxes.z;  zd *= zd;
  if(xd+yd+zd>1.0) return 0; // Outside ellipsoid
  return 1; // Inside or on boundary of ellipsoid
}

OC_INDEX Oxs_EllipsoidAtlas::GetRegionId(const String& name) const
{ // Given a region id string (name), returns
  // the corresponding region id index.  If
  // "name" is not included in the atlas, then
  // -1 is returned.  Note: If name == "universe",
  // then the return value will be 0.
  if(name.compare(region_name)==0) return 1;
  if(name.compare("universe")==0) return 0;
  return -1;
}

OC_BOOL Oxs_EllipsoidAtlas::GetRegionName(OC_INDEX id,String& name) const
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


