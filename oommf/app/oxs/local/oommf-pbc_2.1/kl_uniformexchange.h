/* FILE: kl_uniformexchange.h            -*-Mode: c++-*-
 *
 * Uniform 6 neighbor exchange energy on rectangular mesh,
 * derived from Oxs_Energy class.
 * Based on file: uniformexchange.h
 *
 */

#ifndef _KLM_UNIFORMEXCHANGE
#define _KLM_UNIFORMEXCHANGE

#include "oc.h"
#include "director.h"
#include "energy.h"
#include "meshvalue.h"
#include "simstate.h"
#include "threevector.h"
#include "util.h"

/* End includes */

class Klm_UniformExchange:public Oxs_Energy {
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
			NGBR_6_Z_PERIOD,	//kl z-periodic conditions. Based on NGBR_6_MIRROR                        
			NGBR_12_FREE, NGBR_12_ZD1, NGBR_12_ZD1B,
			NGBR_12_MIRROR,	NGBR_26 } kernel;
  /// Exchange formulation to use.  "unknown" is invalid; it
  /// is defined for error detection.
  /// NOTE: "kernel" is set inside the constructor, and should
  ///  be fixed thereafter.

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

  void InitCoef_12NgbrZD1(OC_INDEX size,
			  OC_REAL8m wgt[3],
			  Nb_2DArrayWrapper<OC_REAL8m>& coef) const;
  /// Sets "coef" and "integ" arrays for use by NGBR_12_ZD1 kernel.

  // Utility routine for CalcEnergy6NgbrBigAngleMirror
  OC_REAL8m ComputeAngle(const ThreeVector& u1,const ThreeVector& u2) const;

  // Calculation routines for each of the
  // aforementioned energy formulations.
  void CalcEnergy6NgbrFree
  (const Oxs_MeshValue<ThreeVector>& spin,
   const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
   const Oxs_RectangularMesh* mesh,
   Oxs_MeshValue<OC_REAL8m>& energy,
   Oxs_MeshValue<ThreeVector>& field) const;
  void CalcEnergy6NgbrMirror
  (const Oxs_MeshValue<ThreeVector>& spin,
   const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
   const Oxs_RectangularMesh* mesh,
   Oxs_MeshValue<OC_REAL8m>& energy,
   Oxs_MeshValue<ThreeVector>& field) const;
  void CalcEnergy6NgbrMirrorStd
  (const Oxs_MeshValue<ThreeVector>& spin,
   const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
   const Oxs_RectangularMesh* mesh,
   Oxs_MeshValue<OC_REAL8m>& energy,
   Oxs_MeshValue<ThreeVector>& field) const;
  void CalcEnergy6NgbrMirror_lex
  (const Oxs_MeshValue<ThreeVector>& spin,
   const Oxs_MeshValue<OC_REAL8m>& Ms,
   const Oxs_RectangularMesh* mesh,
   Oxs_MeshValue<OC_REAL8m>& energy,
   Oxs_MeshValue<ThreeVector>& field) const;
  void CalcEnergy6NgbrBigAngMirror
  (const Oxs_MeshValue<ThreeVector>& spin,
   const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
   const Oxs_RectangularMesh* mesh,
   Oxs_MeshValue<OC_REAL8m>& energy,
   Oxs_MeshValue<ThreeVector>& field) const;
  void CalcEnergy6NgbrZD2
  (const Oxs_MeshValue<ThreeVector>& spin,
   const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
   const Oxs_RectangularMesh* mesh,
   Oxs_MeshValue<OC_REAL8m>& energy,
   Oxs_MeshValue<ThreeVector>& field) const;
  //kl(m)
  void CalcEnergy6NgbrZPeriodicCond
  (const Oxs_MeshValue<ThreeVector>& spin,
   const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
   const Oxs_RectangularMesh* mesh,
   Oxs_MeshValue<OC_REAL8m>& energy,
   Oxs_MeshValue<ThreeVector>& field) const;
  void CalcEnergy6NgbrZPeriodicCond_lex
  (const Oxs_MeshValue<ThreeVector>& spin,
   const Oxs_MeshValue<OC_REAL8m>& Ms,
   const Oxs_RectangularMesh* mesh,
   Oxs_MeshValue<OC_REAL8m>& energy,
   Oxs_MeshValue<ThreeVector>& field) const;
  //   
  void CalcEnergy12NgbrFree
  (const Oxs_MeshValue<ThreeVector>& spin,
   const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
   const Oxs_RectangularMesh* mesh,
   Oxs_MeshValue<OC_REAL8m>& energy,
   Oxs_MeshValue<ThreeVector>& field) const;
  void CalcEnergy12NgbrZD1
  (const Oxs_MeshValue<ThreeVector>& spin,
   const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
   const Oxs_RectangularMesh* mesh,
   Oxs_MeshValue<OC_REAL8m>& energy,
   Oxs_MeshValue<ThreeVector>& field) const;
  void CalcEnergy12NgbrZD1B
  (const Oxs_MeshValue<ThreeVector>& spin,
   const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
   const Oxs_RectangularMesh* mesh,
   Oxs_MeshValue<OC_REAL8m>& energy,
   Oxs_MeshValue<ThreeVector>& field) const;
  void CalcEnergy12NgbrMirror
  (const Oxs_MeshValue<ThreeVector>& spin,
   const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
   const Oxs_RectangularMesh* mesh,
   Oxs_MeshValue<OC_REAL8m>& energy,
   Oxs_MeshValue<ThreeVector>& field) const;
  void CalcEnergy26Ngbr
  (const Oxs_MeshValue<ThreeVector>& spin,
   const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
   const Oxs_RectangularMesh* mesh,
   Oxs_MeshValue<OC_REAL8m>& energy,
   Oxs_MeshValue<ThreeVector>& field) const;

protected:
  virtual void GetEnergy(const Oxs_SimState& state,
			 Oxs_EnergyData& oed) const;

public:
  virtual const char* ClassName() const; // ClassName() is
  /// automatically generated by the OXS_EXT_REGISTER macro.
  Klm_UniformExchange(const char* name,     // Child instance id
		    Oxs_Director* newdtr, // App director
		    const char* argstr);  // MIF input block parameters
  virtual ~Klm_UniformExchange();
  virtual OC_BOOL Init();
};


#endif // _KLM_UNIFORMEXCHANGE
