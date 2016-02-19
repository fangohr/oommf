/* FILE: planerandomvectorfield.cc      -*-Mode: c++-*-
 *
 * Vector field object, random in a specified plane.
 * Derived from Oxs_VectorField class.
 *
 */

#include <string>

#include "oc.h"
#include "nb.h"

#include "threevector.h"
#include "meshvalue.h"
#include "director.h"
#include "planerandomvectorfield.h"
#include "energy.h"		// Needed to make MSVC++ 5 happy

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_PlaneRandomVectorField);

/* End includes */

////////////////////////////////////////////////////////////////////////
/// Oxs_PlaneRandomVectorField

// Constructor
Oxs_PlaneRandomVectorField::Oxs_PlaneRandomVectorField(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_VectorField(name,newdtr,argstr), 
    min_norm(-1.), max_norm(-1.), minsq(-1.), maxsq(-1.),
    use_cache(0)
{
  // Process arguments.
  min_norm=GetRealInitValue("min_norm");
  max_norm=GetRealInitValue("max_norm");

  // Squared weights are necessary to get a uniform sampling by area.
  // Pre-compute for convenience.  (Assume no overflow.)
  minsq = min_norm*min_norm;
  maxsq = max_norm*max_norm;

  // Get vector field object
  OXS_GET_INIT_EXT_OBJECT("plane_normal",Oxs_VectorField,
                          plane_normal_field);

  use_cache = 0;
  if(HasInitValue("cache_grid")) {
    // Get pointer to mesh object
    OXS_GET_INIT_EXT_OBJECT("cache_grid",Oxs_Mesh,cache_mesh);
    use_cache = 1;
  }

  VerifyAllInitArgsUsed();
}


void
Oxs_PlaneRandomVectorField::Value
(const ThreeVector& pt,
 ThreeVector& value) const
{
  OC_INDEX key=0; // Initialize to quiet compiler warnings; it is used
  // iff use_cache is true, in which case it gets set two lines below.
  if(use_cache) {
    key = cache_mesh->FindNearestIndex(pt);
    map<OC_INDEX,ThreeVector>::iterator it = results_cache.find(key);
    if(it != results_cache.end()) {
      // Import "pt" already mapped
      value = it->second;
      return;
    }
  }
  // Either cache not used, or else no match found in cache.
  // Create new value.

  // What plane?
  ThreeVector plane_normal;
  plane_normal_field->Value(pt,plane_normal);
  OC_REAL8m plane_normal_magsq = plane_normal.MagSq();
  if(plane_normal_magsq>0.0) {
    // NOTE: Allow zero-length "plane normal" vectors;
    //       interpret these as no plane restriction,
    //       i.e., return value from this function is
    //       uniform across ball in R3.
    plane_normal *= 1.0/sqrt(plane_normal_magsq); // Make unit
  }

  // Produce vector perpendicular to plane
  int j=0;
  OC_REAL8m dot=0;
  do {
    value.Random(1.0);
    dot = plane_normal*value;
  } while(fabs(dot)>0.96875 && ++j<32);
  if(fabs(dot)>0.96875) {
    String msg=
      String("Programming error?  Unable to obtain vector"
	     " in specified plane in"
	     " Oxs_PlaneRandomVectorField::Value()"
	     " method of instance ")
      + String(InstanceName());
    throw Oxs_ExtError(msg.c_str());
  }
  value -= dot*plane_normal;

  // Adjust to proper size
  OC_REAL8m mag = min_norm;
  if(min_norm != max_norm) {
    OC_REAL8m randval = Oc_UnifRand();
    if(plane_normal_magsq>0.0) {
      // Vary magnitude.  Adjust on square scale to get
      // uniform sampling across accessible area.
      mag = sqrt((1-randval)*minsq+randval*maxsq);
    } else {
      // Normal vector is zero; interpret this as no planar restriction,
      // so selected vector is across ball in R3.  In this case the
      // magnitude needs to be adjusted on a cube scale to get uniform
      // sampling by volume.
      mag = pow((1-randval)*min_norm*minsq+randval*max_norm*maxsq,
                OC_REAL8m(1.)/OC_REAL8m(3.));
    }
  }
  value.SetMag(mag);

  if(use_cache) {
    results_cache[key] = value;
  }
}
