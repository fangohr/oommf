/* FILE: uniaxialanisotropy.h            -*-Mode: c++-*-
 *
 * Uniaxial Anisotropy, derived from Oxs_Energy class.
 *
 */

#ifndef _OXS_UNIAXIALANISOTROPY
#define _OXS_UNIAXIALANISOTROPY

#include "nb.h"
#include "threevector.h"
#include "util.h"
#include "chunkenergy.h"
#include "energy.h"
#include "key.h"
#include "simstate.h"
#include "mesh.h"
#include "meshvalue.h"
#include "oxsthread.h"
#include "scalarfield.h"
#include "vectorfield.h"

/* End includes */

class Oxs_UniaxialAnisotropy
  : public Oxs_ChunkEnergy, public Oxs_EnergyPreconditionerSupport {
private:
  enum AnisotropyCoefType {
    ANIS_UNKNOWN, K1_TYPE, Ha_TYPE
  } aniscoeftype;

  Oxs_OwnedPointer<Oxs_ScalarField> K1_init;
  Oxs_OwnedPointer<Oxs_ScalarField> Ha_init;
  Oxs_OwnedPointer<Oxs_VectorField> axis_init;
  mutable Oxs_ThreadControl thread_control;
  mutable OC_UINT4m mesh_id;
  mutable Oxs_MeshValue<OC_REAL8m> K1;
  mutable Oxs_MeshValue<OC_REAL8m> Ha;
  mutable Oxs_MeshValue<ThreeVector> axis;
  /// K1, Ha, and axis are cached values filled by corresponding
  /// *_init members when a change in mesh is detected.

  // It is not uncommon for the anisotropy to be specified by uniform
  // fields.  In this case, memory traffic can be significantly
  // reduced, which may be helpful in parallelized code.  The
  // variables uniform_K1/Ha/axis_value are valid iff the corresponding
  // boolean is true.
  OC_BOOL K1_is_uniform;
  OC_BOOL Ha_is_uniform;
  OC_BOOL axis_is_uniform;
  OC_REAL8m uniform_K1_value;
  OC_REAL8m uniform_Ha_value;
  ThreeVector uniform_axis_value;

  enum IntegrationMethod {
    UNKNOWN_INTEG, RECT_INTEG, QUAD_INTEG
  } integration_method;
  /// Integration formulation to use.  "unknown" is invalid; it
  /// is defined for error detection.

  // RectIntegEnergy is a helper function for ComputeEnergyChunk;
  // it computes using "RECT_INTEG" method.
  void RectIntegEnergy(const Oxs_SimState& state,
                       Oxs_ComputeEnergyDataThreaded& ocedt,
                       Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
                       OC_INDEX node_start,OC_INDEX node_stop) const;


  OC_BOOL has_multscript;
  vector<Nb_TclCommandLineOption> command_options;
  Nb_TclCommand cmd;
  OC_UINT4m number_of_stages;

  // Variables to track and store multiplier value for each simulation
  // state.  This data is computed once per state by the main thread,
  // and shared with all the children.
  mutable Oxs_ThreadControl mult_thread_control;
  mutable OC_UINT4m mult_state_id;
  mutable OC_REAL8m mult;
  mutable OC_REAL8m dmult; // Partial derivative of multiplier wrt t

  void GetMultiplier(const Oxs_SimState& state,
                     OC_REAL8m& mult,
                     OC_REAL8m& dmult) const;

protected:
  virtual void GetEnergy(const Oxs_SimState& state,
			 Oxs_EnergyData& oed) const {
    GetEnergyAlt(state,oed);
  }

  virtual void ComputeEnergy(const Oxs_SimState& state,
                             Oxs_ComputeEnergyData& oced) const {
    ComputeEnergyAlt(state,oced);
  }

  virtual void ComputeEnergyChunk(const Oxs_SimState& state,
                                  Oxs_ComputeEnergyDataThreaded& ocedt,
                                  Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
                                  OC_INDEX node_start,OC_INDEX node_stop,
                                  int threadnumber) const;
public:
  virtual const char* ClassName() const; // ClassName() is
  /// automatically generated by the OXS_EXT_REGISTER macro.
  Oxs_UniaxialAnisotropy(const char* name,  // Child instance id
			 Oxs_Director* newdtr, // App director
			 const char* argstr);  // MIF input block parameters
  virtual ~Oxs_UniaxialAnisotropy() {}
  virtual OC_BOOL Init();
  virtual void StageRequestCount(unsigned int& min,
				 unsigned int& max) const;

  // Optional interface for conjugate-gradient evolver.
  virtual int IncrementPreconditioner(PreconditionerData& pcd);
};


#endif // _OXS_UNIAXIALANISOTROPY
