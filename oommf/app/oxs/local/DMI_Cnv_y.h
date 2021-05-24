/* FILE: DMI_Cnv_y.h            -*-Mode: c++-*-
 *
 * Dzyaloshinskii-Moriya energy for the Cnv crystallographic class [1]:
 *
 * $w_\text{dmi} = D ( L_{xy}^{(x)} + L_{zy}^{(z)} )
 *
 * This extension works both with and without periodic boundary conditions.
 *
 * Extension and modification by David Cortes-Ortuno, Marijan Beg and Hans Fangohr (University of Southampton and European XFEL GmbH) of Oxs_DMexchange6ngbr.h [2].
 *
 * Developed as a part of OpenDreamKit Horizon 2020 European Research Infrastructure
 * project (676541), and the EPSRC Programme grant on Skyrmionics (EP/N032128/1).
 *
 * [1] A. N. Bogdanov and D. A. Yablonskii. Zh. Eksp. Teor. Fiz. 95, 178-182 (1989).
 * [2] Rohart, S., & Thiaville, A. Physical Review B, 88, 184422 (2013).
 *
 */

#ifndef _OXS_DMI_CNV_Y
#define _OXS_DMI_CNV_Y

#include "atlas.h"
#include "key.h"
#include "energy.h"
#include "mesh.h"
#include "meshvalue.h"
#include "simstate.h"
#include "threevector.h"
#include "rectangularmesh.h"

/* End includes */

class Oxs_DMI_Cnv_y:public Oxs_Energy {
private:
  OC_INT4m A_size;
  OC_REAL8m** D;
  Oxs_Key<Oxs_Atlas> atlaskey;
  Oxs_OwnedPointer<Oxs_Atlas> atlas;
  mutable OC_INT4m mesh_id;
  mutable Oxs_MeshValue<OC_INT4m> region_id;

  // Periodic boundaries?
  mutable int xperiodic;
  mutable int yperiodic;
  mutable int zperiodic;

protected:
  virtual void GetEnergy(const Oxs_SimState& state,
			 Oxs_EnergyData& oed) const;

public:
  virtual const char* ClassName() const; // ClassName() is
  /// automatically generated by the OXS_EXT_REGISTER macro.
  Oxs_DMI_Cnv_y(const char* name,     // Child instance id
		    Oxs_Director* newdtr, // App director
		    const char* argstr);  // MIF input block parameters
  virtual ~Oxs_DMI_Cnv_y();
  virtual OC_BOOL Init();
};


#endif // _OXS_DMI_CNV_Y
