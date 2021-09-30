/* FILE: uniformexchange.h            -*-Mode: c++-*-
 *
 * Uniform 6 neighbor exchange energy on rectangular mesh,
 * derived from Oxs_Energy class.
 *
 */

#ifndef _OXS_UNIFORMEXCHANGE
#define _OXS_UNIFORMEXCHANGE

#include "oc.h"
#include "director.h"
#include "chunkenergy.h"
#include "energy.h"
#include "meshvalue.h"
#include "simstate.h"
#include "threevector.h"
#include "util.h"

/* End includes */

class Oxs_UniformExchange
  : public Oxs_ChunkEnergy, public Oxs_EnergyPreconditionerSupport {
private:
  enum ExchangeCoefType {
    A_UNKNOWN, A_TYPE, LEX_TYPE
  }  excoeftype;
  OC_REAL8m A;
  OC_REAL8m lex;

  enum ExchangeKernel { NGBR_UNKNOWN,
			NGBR_6_FREE,
                        NGBR_6_MIRROR, NGBR_6_MIRROR_STD,
                        NGBR_6_BIGANG_MIRROR, NGBR_6_ZD2,
                        NGBR_6_ALT,
			NGBR_12_FREE, NGBR_12_ZD1, NGBR_12_ZD1B,
			NGBR_12_MIRROR,	NGBR_26 } kernel;
  /// Exchange formulation to use.  "unknown" is invalid; it
  /// is defined for error detection.
  /// NOTE: "kernel" is set inside the constructor, and should
  ///  be fixed thereafter.

  // Periodic boundaries?
  mutable int xperiodic;
  mutable int yperiodic;
  mutable int zperiodic;

  // "?coef" and "?integ" are coefficient matrices used by some
  // of the CalcEnergy routines.  (Well, currently just 12NgbrZD1.)
  // They are "mutable" so they can be changed from inside the
  // (const) CalcEnergy routines.
  mutable OC_UINT4m mesh_id;
  mutable Nb_2DArrayWrapper<OC_REAL8m> xcoef;
  mutable Nb_2DArrayWrapper<OC_REAL8m> ycoef;
  mutable Nb_2DArrayWrapper<OC_REAL8m> zcoef;
  mutable OC_REAL8m xinteg[3];
  mutable OC_REAL8m yinteg[3];
  mutable OC_REAL8m zinteg[3];

  mutable OC_REAL8m energy_density_error_estimate; // Cached value,
                                 /// initialized when mesh changes.

  void InitCoef_12NgbrZD1(OC_INDEX size,
			  OC_REAL8m wgt[3],
			  Nb_2DArrayWrapper<OC_REAL8m>& coef) const;
  /// Sets "coef" and "integ" arrays for use by NGBR_12_ZD1 kernel.

  // Support for threaded maxdot calculations
  mutable vector<OC_REAL8m> maxdot;

  // Utility routine for CalcEnergy6NgbrBigAngleMirror
  OC_REAL8m ComputeAngle(const ThreeVector& u1,const ThreeVector& u2) const;

  // Calculation routines for each of the aforementioned energy
  // formulations.  Note that the following kernels have not been
  // upgraded to supported periodic meshes: 
  //   NGBR_12_FREE, NGBR_12_ZD1, NGBR_12_ZD1B, NGBR_26
  void CalcEnergy6NgbrFree
  (const Oxs_MeshValue<ThreeVector>& spin,
   const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
   const Oxs_CommonRectangularMesh* mesh,
   Oxs_ComputeEnergyDataThreaded& ocedt,
   Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
   OC_INDEX node_start,OC_INDEX node_stop,
   int threadnumber) const;
  void CalcEnergy6NgbrMirror
  (const Oxs_MeshValue<ThreeVector>& spin,
   const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
   const Oxs_CommonRectangularMesh* mesh,
   Oxs_ComputeEnergyDataThreaded& ocedt,
   Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
   OC_INDEX node_start,OC_INDEX node_stop,
   int threadnumber) const;
  void CalcEnergy6NgbrMirror_lex
  (const Oxs_MeshValue<ThreeVector>& spin,
   const Oxs_MeshValue<OC_REAL8m>& Ms,
   const Oxs_CommonRectangularMesh* mesh,
   Oxs_ComputeEnergyDataThreaded& ocedt,
   Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
   OC_INDEX node_start,OC_INDEX node_stop,
   int threadnumber) const;
  void CalcEnergy6NgbrMirrorStd
  (const Oxs_MeshValue<ThreeVector>& spin,
   const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
   const Oxs_CommonRectangularMesh* mesh,
   Oxs_ComputeEnergyDataThreaded& ocedt,
   Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
   OC_INDEX node_start,OC_INDEX node_stop,
   int threadnumber) const;
  void CalcEnergy6NgbrBigAngMirror
  (const Oxs_MeshValue<ThreeVector>& spin,
   const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
   const Oxs_CommonRectangularMesh* mesh,
   Oxs_ComputeEnergyDataThreaded& ocedt,
   Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
   OC_INDEX node_start,OC_INDEX node_stop,
   int threadnumber) const;
  void CalcEnergy6NgbrZD2
  (const Oxs_MeshValue<ThreeVector>& spin,
   const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
   const Oxs_CommonRectangularMesh* mesh,
   Oxs_ComputeEnergyDataThreaded& ocedt,
   Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
   OC_INDEX node_start,OC_INDEX node_stop,
   int threadnumber) const;
  void CalcEnergy6NgbrAlt
  (const Oxs_MeshValue<ThreeVector>& spin,
   const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
   const Oxs_CommonRectangularMesh* mesh,
   Oxs_ComputeEnergyDataThreaded& ocedt,
   Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
   OC_INDEX node_start,OC_INDEX node_stop,
   int threadnumber) const;
  void CalcEnergy12NgbrFree
  (const Oxs_MeshValue<ThreeVector>& spin,
   const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
   const Oxs_RectangularMesh* rmesh,
   Oxs_ComputeEnergyDataThreaded& ocedt,
   Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
   OC_INDEX node_start,OC_INDEX node_stop,
   int threadnumber) const;
  void CalcEnergy12NgbrZD1
  (const Oxs_MeshValue<ThreeVector>& spin,
   const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
   const Oxs_RectangularMesh* rmesh,
   Oxs_ComputeEnergyDataThreaded& ocedt,
   Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
   OC_INDEX node_start,OC_INDEX node_stop,
   int threadnumber) const;
  void CalcEnergy12NgbrZD1B
  (const Oxs_MeshValue<ThreeVector>& spin,
   const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
   const Oxs_RectangularMesh* rmesh,
   Oxs_ComputeEnergyDataThreaded& ocedt,
   Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
   OC_INDEX node_start,OC_INDEX node_stop,
   int threadnumber) const;
  void CalcEnergy12NgbrMirror
  (const Oxs_MeshValue<ThreeVector>& spin,
   const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
   const Oxs_CommonRectangularMesh* mesh,
   Oxs_ComputeEnergyDataThreaded& ocedt,
   Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
   OC_INDEX node_start,OC_INDEX node_stop,
   int threadnumber) const;
  void CalcEnergy26Ngbr
  (const Oxs_MeshValue<ThreeVector>& spin,
   const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
   const Oxs_RectangularMesh* rmesh,
   Oxs_ComputeEnergyDataThreaded& ocedt,
   Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
   OC_INDEX node_start,OC_INDEX node_stop,
   int threadnumber) const;

  // Supplied outputs, in addition to those provided by Oxs_Energy.
  Oxs_ScalarOutput<Oxs_UniformExchange> maxspinangle_output;
  Oxs_ScalarOutput<Oxs_UniformExchange> stage_maxspinangle_output;
  Oxs_ScalarOutput<Oxs_UniformExchange> run_maxspinangle_output;
  void UpdateDerivedOutputs(const Oxs_SimState& state);
  String MaxSpinAngleStateName() const {
    return DataName("Max Spin Angle");
  }
  String StageMaxSpinAngleStateName() const {
    return DataName("Stage Max Spin Angle");
  }
  String RunMaxSpinAngleStateName() const {
    return DataName("Run Max Spin Angle");
  }

protected:
  virtual void GetEnergy(const Oxs_SimState& state,
			 Oxs_EnergyData& oed) const {
    GetEnergyAlt(state,oed);
  }

  virtual void ComputeEnergy(const Oxs_SimState& state,
                             Oxs_ComputeEnergyData& oced) const {
    ComputeEnergyAlt(state,oced);
  }

  virtual void ComputeEnergyChunkInitialize
  (const Oxs_SimState& state,
   Oxs_ComputeEnergyDataThreaded& ocedt,
   Oc_AlignedVector<Oxs_ComputeEnergyDataThreadedAux>& thread_ocedtaux,
   int number_of_threads) const;

  virtual void ComputeEnergyChunkFinalize
  (const Oxs_SimState& state,
   Oxs_ComputeEnergyDataThreaded& ocedt,
   Oc_AlignedVector<Oxs_ComputeEnergyDataThreadedAux>& thread_ocedtaux,
   int number_of_threads) const;

  virtual void ComputeEnergyChunk(const Oxs_SimState& state,
                                  Oxs_ComputeEnergyDataThreaded& ocedt,
                                  Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
                                  OC_INDEX node_start,OC_INDEX node_stop,
                                  int threadnumber) const;

public:
  virtual const char* ClassName() const; // ClassName() is
  /// automatically generated by the OXS_EXT_REGISTER macro.
  Oxs_UniformExchange(const char* name,     // Child instance id
		    Oxs_Director* newdtr, // App director
		    const char* argstr);  // MIF input block parameters
  virtual ~Oxs_UniformExchange();
  virtual OC_BOOL Init();

  // Optional interface for conjugate-gradient evolver.
  virtual int IncrementPreconditioner(PreconditionerData& pcd);
};


#endif // _OXS_UNIFORMEXCHANGE
