/* FILE: randomscalarfield.cc      -*-Mode: c++-*-
 *
 * Random scalar field object, derived from Oxs_ScalarField class.
 *
 */

#include "oc.h"
#include "nb.h"

#include "threevector.h"
#include "meshvalue.h"
#include "director.h"
#include "randomscalarfield.h"
#include "energy.h"             // Needed to make MSVC++ 5 happy

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_RandomScalarField);

/* End includes */

////////////////////////////////////////////////////////////////////////
/// Oxs_RandomScalarField

// Constructor
Oxs_RandomScalarField::Oxs_RandomScalarField(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_ScalarField(name,newdtr,argstr),
    range_min(0.0), range_max(0.0), use_cache(0)
{
  // Process arguments.
  range_min=GetRealInitValue("range_min");
  range_max=GetRealInitValue("range_max");

  use_cache = 0;
  if(HasInitValue("cache_grid")) {
    // Get pointer to mesh object
    OXS_GET_INIT_EXT_OBJECT("cache_grid",Oxs_Mesh,cache_mesh);
    use_cache = 1;
  }

  VerifyAllInitArgsUsed();

  if(range_min>range_max) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
                "Requested range_min value (%g) bigger than range_max (%g)",
                static_cast<double>(range_min),
                static_cast<double>(range_max));
    throw Oxs_ExtError(this,buf);
  }
}

OC_REAL8m Oxs_RandomScalarField::Value(const ThreeVector& pt) const
{
  OC_UINT4m key=0; // Initialize to quiet compiler warnings; it is used
  // iff use_cache is true, in which case it gets set two lines below.
  if(range_min>=range_max) return range_max; // No spread
  if(use_cache) {
    key = cache_mesh->FindNearestIndex(pt);
    map<OC_UINT4m,OC_REAL8m>::iterator it = results_cache.find(key);
    if(it != results_cache.end()) {
      // Import "pt" already mapped
      return it->second;
    }
  }

  // Either cache not used, or else no match found in cache.
  // Create new value.
  OC_REAL8m val = (range_max-range_min)*Oc_UnifRand() + range_min;

  if(use_cache) {
    results_cache[key] = val;
  }

  return val;
}

void
Oxs_RandomScalarField::FillMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& array) const
{
  array.AdjustSize(mesh);
  OC_INDEX i,size=mesh->Size();
  if(range_min>=range_max) {
    // No spread
    for(i=0;i<size;i++) {
      array[i] = range_max;
    }
  } else {
    if(!use_cache) {
      OC_REAL8m spread = range_max - range_min;
      for(i=0;i<size;i++) {
        array[i] = spread*Oc_UnifRand() + range_min;
      }
    } else {
      ThreeVector pt;
      for(i=0;i<size;i++) {
        mesh->Center(i,pt);
        array[i] = Value(pt);
      }
    }
  }
}

void
Oxs_RandomScalarField::IncrMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& array) const
{
  if(array.Size() != mesh->Size()) {
    Oxs_ExtError("Import array not initialized in "
                   "Oxs_RandomScalarField::IncrMeshValue");
  }
  OC_INDEX i,size=mesh->Size();
  if(range_min>=range_max) {
    // No spread
    for(i=0;i<size;i++) {
      array[i] += range_max;
    }
  } else {
    if(!use_cache) {
      OC_REAL8m spread = range_max - range_min;
      for(i=0;i<size;i++) {
        array[i] += spread*Oc_UnifRand() + range_min;
      }
    } else {
      ThreeVector pt;
      for(i=0;i<size;i++) {
        mesh->Center(i,pt);
        array[i] += Value(pt);
      }
    }
  }
}

void
Oxs_RandomScalarField::MultMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& array) const
{
  if(array.Size() != mesh->Size()) {
    Oxs_ExtError("Import array not initialized in "
                   "Oxs_RandomScalarField::MultMeshValue");
  }
  OC_INDEX i,size=mesh->Size();
  if(range_min>=range_max) {
    // No spread
    for(i=0;i<size;i++) {
      array[i] *= range_max;
    }
  } else {
    if(!use_cache) {
      OC_REAL8m spread = range_max - range_min;
      for(i=0;i<size;i++) {
        array[i] *= spread*Oc_UnifRand() + range_min;
      }
    } else {
      ThreeVector pt;
      for(i=0;i<size;i++) {
        mesh->Center(i,pt);
        array[i] *= Value(pt);
      }
    }
  }
}
