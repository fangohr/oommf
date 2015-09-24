/* FILE: transformzeeman.h         -*-Mode: c++-*-
 *
 * Zeeman (applied field) energy, derived from Oxs_Energy class,
 * specified from a Tcl proc.  This is a generalization of the
 * Oxs_ScriptUZeeman and Oxs_FixedZeeman classes.
 *
 */

#ifndef _OXS_TRANSFORMZEEMAN
#define _OXS_TRANSFORMZEEMAN

#include <vector>

#include "nb.h"

#include "director.h"
#include "energy.h"
#include "simstate.h"
#include "threevector.h"
#include "util.h"
#include "vectorfield.h"

OC_USE_STD_NAMESPACE;

/* End includes */

class Oxs_TransformZeeman:public Oxs_Energy {
private:
  vector<Nb_TclCommandLineOption> command_options;
  Nb_TclCommand cmd;

  OC_REAL8m hmult;
  OC_UINT4m number_of_stages;

  void GetAppliedField
  (const Oxs_SimState& state,
   ThreeVector& row1, ThreeVector& row2, ThreeVector& row3,
   ThreeVector& drow1, ThreeVector& drow2, ThreeVector& drow3) const;

  enum TransformType { identity, diagonal, symmetric, general };
  TransformType transform_type;

  mutable OC_UINT4m mesh_id;
  Oxs_OwnedPointer<Oxs_VectorField> fixedfield_init;
  mutable Oxs_MeshValue<ThreeVector> fixedfield;
  /// fixedfield is a cached value filled by
  /// fixedfield_init when a change in mesh is
  /// detected.

protected:
  virtual void GetEnergy(const Oxs_SimState& state,
			 Oxs_EnergyData& oed) const;

public:
  virtual const char* ClassName() const; // ClassName() is
  /// automatically generated by the OXS_EXT_REGISTER macro.
  Oxs_TransformZeeman(const char* name,     // Child instance id
		      Oxs_Director* newdtr, // App director
		      const char* argstr);  // MIF input block parameters
  virtual ~Oxs_TransformZeeman();
  virtual OC_BOOL Init();
  virtual void StageRequestCount(unsigned int& min,
				 unsigned int& max) const;
};


#endif // _OXS_TRANSFORMZEEMAN
