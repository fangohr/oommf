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

  if(range_min == range_max) use_cache = 0;  // All values same.

  if(use_cache) {
    // Fill map here (as opposed to a lazy fill), so that concurrent
    // accesses are supported.
    const OC_INDEX count = cache_mesh->Size();
    if(count<1) {
      throw Oxs_ExtError(this,"Empty mesh");
    }
    results_cache.AdjustSize(cache_mesh.GetPtr());
    for(OC_INDEX i=0;i<count;++i) {
      results_cache[i] = (range_max-range_min)*Oc_UnifRand() + range_min;
    }
  }
}

OC_REAL8m Oxs_RandomScalarField::Value(const ThreeVector& pt) const
{
  if(use_cache) {
    OC_INDEX index = cache_mesh->FindNearestIndex(pt);
    if(index>results_cache.Size()) {
      String msg = String("Import pt not mapped, indicating that mesh \"");
      msg += String(cache_mesh->InstanceName());
      msg += String("\" has changed since initialization of"
                    " Oxs_ScalarVectorField \"");
      msg += String(InstanceName());
      msg += String("\"");
      throw Oxs_ExtError(this,msg);
    }
    return results_cache[index];
  }
  if(range_min>=range_max) {
    return range_max; // No spread
  }
  // Else, return random value.  Note that if this is called from
  // multithreaded code, then results will vary from run to run
  // depending on thread run ordering.  Oc_UnifRand is mutex locked,
  // so results will be valid, though perhaps slow.
  return (range_max-range_min)*Oc_UnifRand() + range_min;
}

void
Oxs_RandomScalarField::FillMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& array) const
{
  array.AdjustSize(mesh);
  OC_INDEX size=mesh->Size();
  if(range_min>=range_max) {
    // No spread
    for(OC_INDEX i=0;i<size;i++) {
      array[i] = range_max;
    }
  } else if(!use_cache) {
    OC_REAL8m spread = range_max - range_min;
    for(OC_INDEX i=0;i<size;i++) {
      array[i] = spread*Oc_UnifRand() + range_min;
    }
  } else { // Use cache
    if(mesh != cache_mesh.GetPtr()) {
      // Import mesh is not the same mesh as the cache mesh,
      // so interpolate between meshes.
      ThreeVector pt;
      for(OC_INDEX i=0;i<size;++i) {
        mesh->Center(i,pt);
        OC_INDEX cache_index = cache_mesh->FindNearestIndex(pt);
        if(cache_index>results_cache.Size()) {
          String msg = String("Import pt not mapped, indicating that mesh \"");
          msg += String(cache_mesh->InstanceName());
          msg += String("\" has changed since initialization of"
                        " Oxs_ScalarVectorField \"");
          msg += String(InstanceName());
          msg += String("\"");
          throw Oxs_ExtError(this,msg);
        }
        array[i] = results_cache[cache_index];
      }
    } else {
      // Import mesh is same as cache mesh, so just copy
      // values between arrays.
      if(size != results_cache.Size()) {
        String msg = String("Cache and mesh sizes differ,"
                            " indicating that mesh \"");
        msg += String(cache_mesh->InstanceName());
        msg += String("\" has changed since initialization of"
                      " Oxs_ScalarVectorField \"");
        msg += String(InstanceName());
        msg += String("\"");
        throw Oxs_ExtError(this,msg);
      }
      array = results_cache; // Threaded copy
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
  OC_INDEX size=mesh->Size();
  if(range_min>=range_max) {
    // No spread
    array += range_max; // Threaded increment
  } else if(!use_cache) {
    OC_REAL8m spread = range_max - range_min;
    for(OC_INDEX i=0;i<size;i++) {
      array[i] += spread*Oc_UnifRand() + range_min;
    }
  } else { // Use cache
    if(mesh != cache_mesh.GetPtr()) {
      // Import mesh is not the same mesh as the cache mesh,
      // so interpolate between meshes.
      ThreeVector pt;
      for(OC_INDEX i=0;i<size;++i) {
        mesh->Center(i,pt);
        OC_INDEX cache_index = cache_mesh->FindNearestIndex(pt);
        if(cache_index>results_cache.Size()) {
          String msg = String("Import pt not mapped, indicating that mesh \"");
          msg += String(cache_mesh->InstanceName());
          msg += String("\" has changed since initialization of"
                        " Oxs_ScalarVectorField \"");
          msg += String(InstanceName());
          msg += String("\"");
          throw Oxs_ExtError(this,msg);
        }
        array[i] += results_cache[cache_index];
      }
    } else {
      // Import mesh is same as cache mesh, so just copy
      // values between arrays.
      if(size != results_cache.Size()) {
        String msg = String("Cache and mesh sizes differ,"
                            " indicating that mesh \"");
        msg += String(cache_mesh->InstanceName());
        msg += String("\" has changed since initialization of"
                      " Oxs_ScalarVectorField \"");
        msg += String(InstanceName());
        msg += String("\"");
        throw Oxs_ExtError(this,msg);
      }
      array += results_cache; // Threaded increment
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
  OC_INDEX size=mesh->Size();
  if(range_min>=range_max) {
    // No spread
    array *= range_max; // Threaded multiply
  } else if(!use_cache) {
    OC_REAL8m spread = range_max - range_min;
    for(OC_INDEX i=0;i<size;i++) {
      array[i] *= spread*Oc_UnifRand() + range_min;
    }
  } else { // Use cache
    if(mesh != cache_mesh.GetPtr()) {
      // Import mesh is not the same mesh as the cache mesh,
      // so interpolate between meshes.
      ThreeVector pt;
      for(OC_INDEX i=0;i<size;++i) {
        mesh->Center(i,pt);
        OC_INDEX cache_index = cache_mesh->FindNearestIndex(pt);
        if(cache_index>results_cache.Size()) {
          String msg = String("Import pt not mapped, indicating that mesh \"");
          msg += String(cache_mesh->InstanceName());
          msg += String("\" has changed since initialization of"
                        " Oxs_ScalarVectorField \"");
          msg += String(InstanceName());
          msg += String("\"");
          throw Oxs_ExtError(this,msg);
        }
        array[i] *= results_cache[cache_index];
      }
    } else {
      // Import mesh is same as cache mesh, so just copy
      // values between arrays.
      if(size != results_cache.Size()) {
        String msg = String("Cache and mesh sizes differ,"
                            " indicating that mesh \"");
        msg += String(cache_mesh->InstanceName());
        msg += String("\" has changed since initialization of"
                      " Oxs_ScalarVectorField \"");
        msg += String(InstanceName());
        msg += String("\"");
        throw Oxs_ExtError(this,msg);
      }
      array *= results_cache; // Threaded multiply
    }
  }
}
