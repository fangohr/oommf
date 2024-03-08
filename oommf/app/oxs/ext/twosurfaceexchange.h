/* FILE: twosurfaceexchange.h            -*-Mode: c++-*-
 *
 * Long range (RKKY-style) exchange energy on links across spacer
 * layer, with surface (interfacial) exchange energy coefficents
 * sigma (bilinear) and sigma2 (biquadratic).
 *
 */

#ifndef _OXS_TWOSURFACEEXCHANGE
#define _OXS_TWOSURFACEEXCHANGE

#include <string>
#include <vector>

#include "oc.h"
#include "director.h"
#include "energy.h"
#include "meshvalue.h"
#include "simstate.h"
#include "threevector.h"

OC_USE_STD_NAMESPACE;
OC_USE_STRING;

/* End includes */

// MSC++ 5.0 doesn't like embedded classes used for
// vector template types.  So, we just define this
// outside, with a really long name. <g>
struct Oxs_TwoSurfaceExchangeLinkParams {
  OC_INDEX index1,index2;  // Offsets into mesh to cells in pair
  OC_REAL8m exch_coef1;  // Let d be the computational cellsize along
  OC_REAL8m exch_coef2; /// the direction between the linked cells.
  /// And let sigma be the bilinear surface (interfacial) exchange
  /// energy in J/m^2, and sigma2 be the biquadratic surface
  /// (interfacial) exchange energy, also in J/m^2.  Then
  ///          exch_coef1 = (sigma1 + 2*sigma2)/d
  ///          exch_coef2 = -sigma2/d.
  /// The energy density at cell i due to cell j is computed in
  /// terms of b = 0.5*(m_i - m_j)^2 as
  ///
  ///  E_ij = coef1*b + coef2*b*b =
  ///     (1/d)*[(sigma1+sigma2)-sigma1*m_i.m_j-sigma2*(m_i.m_j)^2]
  ///    -(1/d)*[eps*(sigma1+sigma2*m_i.m_j)+eps^2*sigma2*(1-m_i.m_j)]
  ///
  /// where eps = 1 - 0.5*(m_i^2 + m_j^2) is presumably close to 0.
};

// The next 3 operators are defined so MSVC++ 5.0 will accept
// vector<Oxs_RandomSiteExchangeLinkParams>, but are left undefined
// because there is no meaningful way to define them.
OC_BOOL operator<(const Oxs_TwoSurfaceExchangeLinkParams&,
               const Oxs_TwoSurfaceExchangeLinkParams&);
OC_BOOL operator>(const Oxs_TwoSurfaceExchangeLinkParams&,
               const Oxs_TwoSurfaceExchangeLinkParams&);
OC_BOOL operator==(const Oxs_TwoSurfaceExchangeLinkParams&,
                const Oxs_TwoSurfaceExchangeLinkParams&);

class Oxs_TwoSurfaceExchange:public Oxs_Energy {
private:
  OC_REAL8m init_sigma;  // Usual (bilinear) exchange coefficient
  OC_REAL8m init_sigma2; // Bi-quadratic exchange coefficient

  Oxs_OwnedPointer<Oxs_Atlas> atlas1,atlas2;
  String region1,region2;
  Oxs_OwnedPointer<Oxs_ScalarField> bdry1,bdry2;
  OC_REAL8m bdry1_value,bdry2_value;
  String bdry1_side,bdry2_side;

  mutable vector<Oxs_TwoSurfaceExchangeLinkParams> links;
  void FillLinkList(const Oxs_Mesh* mesh) const;

  mutable OC_UINT4m mesh_id;

  mutable OC_REAL8m energy_density_error_estimate; // Cached value,
  /// initialized when mesh changes.

  // Supplied outputs, in addition to those provided by Oxs_Energy.
  OC_BOOL report_max_spin_angle;
  Oxs_ScalarOutput<Oxs_TwoSurfaceExchange> maxspinangle_output;
  Oxs_ScalarOutput<Oxs_TwoSurfaceExchange> stage_maxspinangle_output;
  Oxs_ScalarOutput<Oxs_TwoSurfaceExchange> run_maxspinangle_output;
  void UpdateDerivedOutputs(const Oxs_SimState& state);
  String MaxSpinAngleStateName() const {
    String dummy_name = InstanceName();
    dummy_name += ":Max Spin Angle";
    return dummy_name;
  }
  String StageMaxSpinAngleStateName() const {
    String dummy_name = InstanceName();
    dummy_name += ":Stage Max Spin Angle";
    return dummy_name;
  }
  String RunMaxSpinAngleStateName() const {
    String dummy_name = InstanceName();
    dummy_name += ":Run Max Spin Angle";
    return dummy_name;
  }


protected:
  virtual void GetEnergy(const Oxs_SimState& state,
                         Oxs_EnergyData& oed) const {
    GetEnergyAlt(state,oed);
  }

  virtual void ComputeEnergy(const Oxs_SimState& state,
                             Oxs_ComputeEnergyData& oced) const;

public:
  virtual const char* ClassName() const; // ClassName() is
  /// automatically generated by the OXS_EXT_REGISTER macro.
  Oxs_TwoSurfaceExchange(const char* name,     // Child instance id
                         Oxs_Director* newdtr, // App director
                         const char* argstr);  // MIF input block parameters
  virtual ~Oxs_TwoSurfaceExchange();
  virtual OC_BOOL Init();
};


#endif // _OXS_TWOSURFACEEXCHANGE
