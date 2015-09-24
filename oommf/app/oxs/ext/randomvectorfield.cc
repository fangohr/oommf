/* FILE: randomvectorfield.cc      -*-Mode: c++-*-
 *
 * Random vector field object, derived from Oxs_VectorField class.
 *
 */

#include <string>

#include "oc.h"
#include "nb.h"

#include "threevector.h"
#include "meshvalue.h"
#include "director.h"
#include "randomvectorfield.h"
#include "energy.h"             // Needed to make MSVC++ 5 happy

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_RandomVectorField);

/* End includes */

////////////////////////////////////////////////////////////////////////
/// Oxs_RandomVectorField

// Constructor
Oxs_RandomVectorField::Oxs_RandomVectorField(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_VectorField(name,newdtr,argstr),
    min_norm(-1.),max_norm(-1.),mincubed(-1.),maxcubed(-1.),
    use_cache(0)
{
  // Process arguments.
  min_norm=GetRealInitValue("min_norm");
  max_norm=GetRealInitValue("max_norm");

  use_cache = 0;
  if(HasInitValue("cache_grid")) {
    // Get pointer to mesh object
    OXS_GET_INIT_EXT_OBJECT("cache_grid",Oxs_Mesh,cache_mesh);
    use_cache = 1;
  }

  VerifyAllInitArgsUsed();

  if(min_norm>max_norm) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
                "Requested min_norm value (%g) bigger than max_norm (%g)",
                static_cast<double>(min_norm),
                static_cast<double>(max_norm));
    throw Oxs_ExtError(this,buf);
  }

  // Cube weights are necessary to get a uniform sampling by volume.
  // Pre-compute for convenience.  (Assume no overflow.)
  mincubed = min_norm*min_norm*min_norm;
  maxcubed = max_norm*max_norm*max_norm;
}


void
Oxs_RandomVectorField::Value
(const ThreeVector& pt,
 ThreeVector& value) const
{
  OC_UINT4m key=0; // Initialize to quiet compiler warnings; it is used
  // iff use_cache is true, in which case it gets set two lines below.
  if(use_cache) {
    key = cache_mesh->FindNearestIndex(pt);
    map<OC_UINT4m,ThreeVector>::iterator it = results_cache.find(key);
    if(it != results_cache.end()) {
      // Import "pt" already mapped
      value = it->second;
      return;
    }
  }

  // Either cache not used, or else no match found in cache.
  // Create new value.
  if(min_norm == max_norm) {
    // No spread in magnitudes
    value.Random(max_norm);
  } else {
    // Vary both direction and magnitude.
    // Magnitude needs to be adjusted on a cube scale
    // to get uniform sampling by volume.
    OC_REAL8m randval = Oc_UnifRand();
    OC_REAL8m mag = pow((1-randval)*mincubed+randval*maxcubed,
                     OC_REAL8m(1.)/OC_REAL8m(3.));
    value.Random(mag);
  }

  if(use_cache) {
    results_cache[key] = value;
  }
}
