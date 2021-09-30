/* FILE: ellipseatlas.cc                 -*-Mode: c++-*-
 *
 * Atlas class derived from Oxs_Atlas class for regions with elliptical
 * cylinder shape.  Very similar to the Oxs_EllipsoidAtlas class.
 */

#include <vector>
#include "nb.h"
#include "director.h"
#include "ellipseatlas.h"

OC_USE_STD_NAMESPACE;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_EllipseAtlas);

/* End includes */


// Constructor
Oxs_EllipseAtlas::Oxs_EllipseAtlas
( const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr    // MIF block argument string
  ) : Oxs_Atlas(name,newdtr,argstr), axis(AD_Z)
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

  // Cylinder axis
  String axisdir = GetStringInitValue("axis","z");
  if(axisdir.compare("x")==0) {
    axis = AD_X;
  } else if(axisdir.compare("y")==0) {
    axis = AD_Y;
  } else if(axisdir.compare("z")==0) {
    axis = AD_Z;
  } else {
    throw Oxs_ExtError(this,"Entry \"axis\" in Specify block"
                       " should be one of x, y, or z (default).");
  }

  // Margins. Value order in fully expanded 6-element list is
  // xmin xmax ymin ymax zmin zmax
  vector<OC_REAL8m> margin;
  if(HasInitValue("margin")) {
    GetGroupedRealListInitValue("margin",margin);
    switch(margin.size()) {
    case 0:
      margin.assign(6,0.0);
      break;
    case 1:
      margin.assign(6,margin[0]);
      break;
    case 3:
      margin.resize(6);
      margin[5]=margin[4]=margin[2];
      margin[3]=margin[2]=margin[1];
      margin[1]=margin[0];
      break;
    case 6:
      break;
    default:
      throw Oxs_ExtError(this,"Entry \"margin\" in Specify block"
                       " should have 1, 3, or 6 entries.");
    }
  } else {
    margin.assign(6,0.0);
  }

  // Region names
  region_name_list.push_back("universe"); // Universe is always id 0
  vector<String> tmp_list;
  if(HasInitValue("name")) {
    GetGroupedStringListInitValue("name",tmp_list);
  } else {
    // Default
    tmp_list.push_back(ExtractInstanceTail(InstanceName()));
  }
  if(tmp_list.size()==1) {
    if(tmp_list[0].compare("universe")==0) {
      interior_id = exterior_id = 0;  // Degenerate case: No regions!
    } else {
      region_name_list.push_back(tmp_list[0]);
      interior_id = 1; // Named region
      exterior_id = 0; // "universe"
    }
  } else if(tmp_list.size()==2) {
    if(tmp_list[0].compare("universe")==0) {
      interior_id = 0;
    } else {
      interior_id = region_name_list.size(); // Named region
      region_name_list.push_back(tmp_list[0]);
    }
    if(tmp_list[1].compare("universe")==0) {
      exterior_id = 0;
    } else {
      exterior_id = region_name_list.size(); // Named region
      region_name_list.push_back(tmp_list[1]);
    }
  } else {
    throw Oxs_ExtError(this,"Entry \"name\" in Specify block"
                       " should be a 1 or 2 element list.");
  }

  VerifyAllInitArgsUsed();

  // Include margins into cylinder extent
  xrange[0] += margin[0];  xrange[1] -= margin[1];
  yrange[0] += margin[2];  yrange[1] -= margin[3];
  zrange[0] += margin[4];  zrange[1] -= margin[5];

  if(!cylinder.CheckOrderSet(xrange[0],xrange[1],
                             yrange[0],yrange[1],
                             zrange[0],zrange[1])) {
    throw Oxs_ExtError(this,"Margins in Specify block"
			 " too big; cylinder region is empty.");
  }

  // Compute working variables
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
    throw Oxs_ExtError(this,"One or more of ellipse dimensions"
			 " has zero extent.");
  }

}

OC_BOOL Oxs_EllipseAtlas::GetRegionExtents(OC_INDEX id,Oxs_Box &mybox) const
{ // Copies the extent of the specified region into mybox and returns 1.
  // If import id is not a valid id then leaves mybox untouched and
  // returns 0.
  if(id>=GetRegionCount()) return 0;
  if(id == exterior_id) {
    mybox = world;
  } else {
    mybox = cylinder;
  }
  return 1;
}

OC_INDEX Oxs_EllipseAtlas::GetRegionId(const ThreeVector& point) const
{ // Returns the id number for the region containing point.
  // The return value is 0 if the point is not contained in
  // the atlas, i.e., belongs to the "universe" region.
  if(!world.IsIn(point)) return 0; // Outside atlas
  OC_REAL8m xd = (point.x - centerpt.x)*invaxes.x;
  OC_REAL8m yd = (point.y - centerpt.y)*invaxes.y;
  OC_REAL8m zd = (point.z - centerpt.z)*invaxes.z;
  OC_REAL8m sumsq = 0.0;
  OC_REAL8m height = 0.0;
  switch(axis) {
  case AD_X: height = xd;  sumsq = yd*yd + zd*zd;  break;
  case AD_Y: height = yd;  sumsq = xd*xd + zd*zd;  break;
  case AD_Z: height = zd;  sumsq = xd*xd + yd*yd;  break;
  }
  if(sumsq>1.0 || fabs(height)>1.0) return exterior_id; // Outside ellipse
  return interior_id; // Inside or on boundary of ellipse
}

OC_INDEX Oxs_EllipseAtlas::GetRegionId(const String& name) const
{ // Given a region id string (name), returns the corresponding region
  // id index.  If "name" is not included in the atlas, then -1 is
  // returned.  Note: If name == "universe", then the return value will
  // be 0.
  for(size_t i=0;i<region_name_list.size();++i) {
    if(region_name_list[i].compare(name)==0) {
      return static_cast<OC_INDEX>(i);
    }
  }
  return -1;
}

OC_BOOL Oxs_EllipseAtlas::GetRegionName(OC_INDEX id,String& name) const
{ // Given an id number, fills in "name" with the corresponding region
  // id string.  Returns 1 on success, 0 if id is invalid.  If id is 0,
  // then name is set to "universe", and the return value is 1.
  name.erase();
  if(id > static_cast<OC_INDEX>(region_name_list.size()-1)) return 0;
  name = region_name_list[id];
  return 1;
}
