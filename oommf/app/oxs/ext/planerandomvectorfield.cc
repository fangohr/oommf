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

// Utility function
ThreeVector
Oxs_PlaneRandomVectorField::CreatePlaneVector
(ThreeVector plane_normal) const
{ // NB: Don't call this function until after all member variables
  //     have been set.
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
  ThreeVector value;
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
  return value;
}

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

  // Get vector field object
  OXS_GET_INIT_EXT_OBJECT("plane_normal",Oxs_VectorField,
                          plane_normal_field);

  use_cache = 0;
  if(HasInitValue("cache_grid")) {
    // Get pointer to mesh object
    OXS_GET_INIT_EXT_OBJECT("cache_grid",Oxs_Mesh,cache_mesh);
    use_cache = 1;
  }

  if(HasInitValue("base_field")) {
    // Get pointer to base field object
    OXS_GET_INIT_EXT_OBJECT("base_field",Oxs_VectorField,base_field);
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

  // Squared weights are necessary to get a uniform sampling by area.
  // Pre-compute for convenience.  (Assume no overflow.)
  minsq = min_norm*min_norm;
  maxsq = max_norm*max_norm;

  if(use_cache) {
    // Fill map here (as opposed to a lazy fill), so that repeatable
    // concurrent accesses are supported.
    const OC_INDEX count = cache_mesh->Size();
    if(count<1) {
      throw Oxs_ExtError(this,"Empty mesh");
    }
    results_cache.AdjustSize(cache_mesh.GetPtr());
    for(OC_INDEX i=0;i<count;++i) {
      ThreeVector pt, plane_normal;
      cache_mesh->Center(i,pt);
      plane_normal_field->Value(pt,plane_normal);
      results_cache[i] = CreatePlaneVector(plane_normal);
      if(base_field.GetPtr()) {
        ThreeVector tmpvalue;
        base_field->Value(pt,tmpvalue);
        results_cache[i] += tmpvalue;
      }
    }
  }
}

void
Oxs_PlaneRandomVectorField::Value
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
    ThreeVector plane_normal;
    plane_normal_field->Value(pt,plane_normal);
    value = CreatePlaneVector(plane_normal);
    if(base_field.GetPtr()) {
      ThreeVector tmpvalue;
      base_field->Value(pt,tmpvalue);
      value += tmpvalue;
    }
  }
}

void
Oxs_PlaneRandomVectorField::FillMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<ThreeVector>& array) const
{
  if(use_cache && mesh == cache_mesh.GetPtr()) {
    // Copy data directly from cache. Don't do this unless the meshes
    // are exactly the same, as otherwise the index-to-location mapping
    // might be different. In particular, the example/randomcache.mif
    // sample file caches values on a coarse mesh but then retrieves
    // using a fine mesh (which results in block "grains"). The
    // alternative branch will function correctly, if somewhat slower,
    // dues to the need to translate between mesh indices and spatial
    // locations. But in practice this branch is expected to be more
    // common, so the optimization may be worthwhile. (Note: there is
    // similar code in planerandomvectorfield.cc.)
    if(mesh->Size() != results_cache.Size()) {
      String msg = String("Cache and mesh sizes differ,"
                          " indicating that mesh \"");
      msg += String(cache_mesh->InstanceName());
      msg += String("\" has changed since initialization of"
                    " Oxs_PlaneRandomVectorField \"");
      msg += String(InstanceName());
      msg += String("\"");
      throw Oxs_ExtError(this,msg);
    }
    array = results_cache;
  } else {
    array.AdjustSize(mesh);
    const OC_INDEX size=mesh->Size();
    for(OC_INDEX i=0;i<size;i++) {
      ThreeVector pt;
      mesh->Center(i,pt);
      Value(pt,array[i]);
    }
  }
}
