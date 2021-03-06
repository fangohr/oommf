/* FILE: pbc_exchangeptwise.h            -*-Mode: c++-*-
 *
 * 6 neighbor exchange energy on rectangular mesh, with
 * exchange coefficient A specified on a cell-by-cell
 * basis.  The effective coefficient between two neighboring
 * cells is calculated on the fly via 2.A_1.A_2/(A_1+A_2)
 */

#ifndef _OXS_PBC_EXCHANGEPTWISE
#define _OXS_PBC_EXCHANGEPTWISE

#include "atlas.h"
#include "key.h"
#include "energy.h"
#include "mesh.h"
#include "meshvalue.h"
#include "simstate.h"
#include "threevector.h"
#include "rectangularmesh.h"
#include "scalarfield.h"
#include "util.h"

/* End includes */

class PBC_ExchangePtwise_2D:public Oxs_Energy {
private:
  Oxs_OwnedPointer<Oxs_ScalarField> A_init;
  mutable OC_INDEX mesh_id;
  mutable Oxs_MeshValue<OC_REAL8m> A;
  /// Exchange coefficient A is filled by A_init routine
  /// each time a change in the mesh is detected.

protected:
  virtual void GetEnergy(const Oxs_SimState& state,
			 Oxs_EnergyData& oed) const;

public:
  virtual const char* ClassName() const; // ClassName() is
  /// automatically generated by the OXS_EXT_REGISTER macro.
  PBC_ExchangePtwise_2D(const char* name,     // Child instance id
		    Oxs_Director* newdtr, // App director
		    const char* argstr);  // MIF input block parameters
  virtual ~PBC_ExchangePtwise_2D();
  virtual OC_BOOL Init();
};


#endif // _OXS_EXCHANGEPTWISE
