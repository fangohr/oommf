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

  if(use_cache) {
    // Fill map here (as opposed to a lazy fill), so that concurrent
    // accesses are supported.
    const OC_INDEX count = cache_mesh->Size();
    if(count<1) {
      throw Oxs_ExtError(this,"Empty mesh");
    }
    results_cache.AdjustSize(cache_mesh.GetPtr());
    ThreeVector value;
    if(min_norm == max_norm) {
      // No spread in magnitudes
      for(OC_INDEX i=0;i<count;++i) {
        value.Random(max_norm);
        results_cache[i] = value;
      }
    } else {
      // Vary both direction and magnitude.  Magnitude needs to be
      // adjusted on a cube scale to get uniform sampling by volume.
      for(OC_INDEX i=0;i<count;++i) {
        OC_REAL8m randval = Oc_UnifRand();
        OC_REAL8m mag = pow((1-randval)*mincubed+randval*maxcubed,
                            OC_REAL8m(1.)/OC_REAL8m(3.));
        value.Random(mag);
        results_cache[i] = value;
      }
    }
  }
}

void
Oxs_RandomVectorField::Value
(const ThreeVector& pt,
 ThreeVector& value) const
{
  if(use_cache) {
    OC_INDEX index = cache_mesh->FindNearestIndex(pt);
    if(index>results_cache.Size()) {
      String msg = String("Import pt not mapped, indicating that mesh \"");
      msg += String(cache_mesh->InstanceName());
      msg += String("\" has changed since initialization of"
                    " Oxs_RandomVectorField \"");
      msg += String(InstanceName());
      msg += String("\"");
      throw Oxs_ExtError(this,msg);
    }
    value = results_cache[index];
  } else {
    // Cache not in use, so compute and return random value.  Note that
    // if this is called from multithreaded code, then results will vary
    // from run to run depending on thread run ordering.  Oc_UnifRand is
    // mutex locked, so results will be valid, though perhaps slow.
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
  }
}
