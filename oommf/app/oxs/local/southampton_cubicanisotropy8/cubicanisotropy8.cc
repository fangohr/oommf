/* FILE: cubicanisotropy8.cc            -*-Mode: c++-*-
 *
 * Cubic Anisotropy for higher orders, derived from Oxs_Energy class.
 * implementing interface cubicanisotropy8.h
 *
 * This class is a modification of the class
 * /oommf/app/oxs/ext/cubicanisotropy.cc
 * It is designed for handling higher orders of the 
 * power series of the cubic anisotropy
 *
 * The required values are 
 * -scalar 'K1' (for fourth order power)
 * -scalar 'K2' (for sixth order power),
 * -scalar 'K3' (for eigth order power),
 * -vector 'axis1' indicating first cubic anisotropy direction
 * -vector 'axis2' indicating second cubic anisotropy direction
 * (the third axis is assumed to be perpendicular to axis1 and axis2)
 *
 * Juergen Zimmermann
 * Computational Engineering and Design Group
 * University of Southampton
 * 
 * file created Wed Mai 11 2005
 *
 * file updated Thu April 19 2007: 
 * renaming issues Ced_CubicAnisotropy to Southampton_CubicAnisotropy8 
 */

#include "oc.h"
#include "nb.h"
#include "threevector.h"
#include "director.h"
#include "simstate.h"
#include "ext.h"
#include "key.h"
#include "mesh.h"
#include "meshvalue.h"
#include "cubicanisotropy8.h"
#include "energy.h"		// Needed to make MSVC++ 5 happy

// Oxs_Ext registration support
OXS_EXT_REGISTER(Southampton_CubicAnisotropy8);

/* End includes */


// Constructor
Southampton_CubicAnisotropy8::Southampton_CubicAnisotropy8(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr), mesh_id(0)
{
  // Process arguments
  OXS_GET_INIT_EXT_OBJECT("K1",Oxs_ScalarField,K1_init);
  OXS_GET_INIT_EXT_OBJECT("K2",Oxs_ScalarField,K2_init);
  OXS_GET_INIT_EXT_OBJECT("K3",Oxs_ScalarField,K3_init);
  OXS_GET_INIT_EXT_OBJECT("axis1",Oxs_VectorField,axis1_init);
  OXS_GET_INIT_EXT_OBJECT("axis2",Oxs_VectorField,axis2_init);
  VerifyAllInitArgsUsed();
}

void Southampton_CubicAnisotropy8::GetEnergy
(const Oxs_SimState& state,
 Oxs_EnergyData& oed
 ) const
{
  const Oxs_MeshValue<OC_REAL8m>& Ms_inverse = *(state.Ms_inverse);
  const Oxs_MeshValue<ThreeVector>& spin = state.spin;

  // Use supplied buffer space, and reflect that use in oed.
  oed.energy = oed.energy_buffer;
  oed.field = oed.field_buffer;
  Oxs_MeshValue<OC_REAL8m>& energy = *oed.energy_buffer;
  Oxs_MeshValue<ThreeVector>& field = *oed.field_buffer;

  OC_INDEX size = state.mesh->Size();
  if(mesh_id != state.mesh->Id()) {
    // This is either the first pass through, or else mesh
    // has changed.
    mesh_id=0;
    K1_init->FillMeshValue(state.mesh,K1);
    K2_init->FillMeshValue(state.mesh,K2);
    K3_init->FillMeshValue(state.mesh,K3);
    axis1_init->FillMeshValue(state.mesh,axis1);
    axis2_init->FillMeshValueOrthogonal(state.mesh,axis1,axis2);
    for(OC_INDEX i=0;i<size;i++) {
      // Much of the code below requires axis1 and axis2 to be
      // orthogonal unit vectors.  Guarantee this is the case:
      const OC_REAL8m eps = 1e-14;
      if(fabs(axis1[i].MagSq()-1)>eps) {
	string msg="Invalid initialization detected for object "
	  + string(InstanceName())
	  + ": Anisotropy axis 1 isn't norm 1";
	throw Oxs_Ext::Error(msg.c_str());
      }
      if(fabs(axis2[i].MagSq()-1)>eps) {
	string msg="Invalid initialization detected for object "
	  + string(InstanceName())
	  + ": Anisotropy axis 2 isn't norm 1";
	throw Oxs_Ext::Error(msg.c_str());
      }
      if(fabs(axis1[i]*axis2[i])>eps) {
	string msg="Invalid initialization detected for object "
	  + string(InstanceName())
	  + ": Specified anisotropy axes aren't perpendicular";
	throw Oxs_Ext::Error(msg.c_str());
      }
    }
    mesh_id=state.mesh->Id();
  }


  for(OC_INDEX i=0;i<size;++i) {
    const ThreeVector& u1 = axis1[i];
    const ThreeVector& u2 = axis2[i];
    const ThreeVector&  m = spin[i];

    // This code requires u1 and u2 to be orthonormal, and m to be a
    // unit vector.  Basically, decompose
    //
    //             m = a1.u1 + a2.u2 + a3.u3
    //               =  m1   +  m2   +  m3
    //
    // where u3=u1xu2, a1=m*u1, a2=m*u2, a3=m*u3.
    //
    // Then the energy is
    //                  2  2     2  2     2  2
    //            K1 (a1 a2  + a1 a3  + a2 a3 ) +
    //
    //                  2  2  2
    //            K2 (a1 a2 a3 ) +
    //
    //                   4  4     4  4     4  4
    //            K3 ((a1 a2  + a1 a3  + a2 a3 )
    //
    // and the field in say the u1 direction is
    //               2    2                 2    2
    //         C1 (a2 + a3 ) a1 . u1 = C1 (a2 + a3 ) m1
    //                           +
    //               2  2                 2   2
    //         C2 (a2 a3 ) a1 . u1 = C2 (a2  a3 ) m1 + 
    //                           +
    //               4    4    3            4    4    2
    //         C3 (a2 + a3 ) a1 . u1 = C3 (a2 + a3 ) a1 m1
    //
    // where C1 = -2K1/(MU0 Ms) 
    // and   C2 = -2K2/(MU0 Ms)
    // and   C3 = -4K3/(MU0 Ms)
    //
    // In particular, note that
    //           2         2     2
    //         a3  = 1 - a1  - a2
    // and
    //         m3  = m - m1 - m2
    //
    // This shows that energy and field can be computed without
    // explicitly calculating u3.  However, the cross product
    // evaluation to get u3 is not that expensive, and appears
    // to be more accurate.  At a minimum, in the above expressions
    // one should at least insure that a3^2 is non-negative.

    OC_REAL8m k1 = K1[i];
    OC_REAL8m k2 = K2[i];
    OC_REAL8m k3 = K3[i];
    OC_REAL8m field_mult1 = (-2/MU0)*k1*Ms_inverse[i];
    OC_REAL8m field_mult2 = (-2/MU0)*k2*Ms_inverse[i];
    OC_REAL8m field_mult3 = (-4/MU0)*k3*Ms_inverse[i];
    if(field_mult1==0.0 && field_mult2==0.0 && field_mult3==0.0) {
      energy[i]=0.0;
      field[i].Set(0.,0.,0.);
      continue;
    }

    ThreeVector u3 = u1;    u3 ^= u2;
    OC_REAL8m a1 = u1*m;  OC_REAL8m a1sq = a1*a1; OC_REAL8m a14t = a1sq*a1sq;
    OC_REAL8m a2 = u2*m;  OC_REAL8m a2sq = a2*a2; OC_REAL8m a24t = a2sq*a2sq;
    OC_REAL8m a3 = u3*m;  OC_REAL8m a3sq = a3*a3; OC_REAL8m a34t = a3sq*a3sq;

    energy[i]  = k1 * (a1sq*a2sq+a1sq*a3sq+a2sq*a3sq);
    energy[i] += k2 * ( a1sq*a2sq*a3sq ); 
    energy[i] += k3 * (a14t*a24t+a14t*a34t+a24t*a34t);

    ThreeVector m1 = a1*u1;
    ThreeVector m2 = a2*u2;
    ThreeVector m3 = a3*u3;
    field[i]  = ( (a2sq+a3sq)*m1 + (a1sq+a3sq)*m2 + (a1sq+a2sq)*m3) *field_mult1; 
    field[i] += ( (a2sq*a3sq)*m1 + (a1sq*a3sq)*m2 + (a1sq*a2sq)*m3) *field_mult2;
    field[i] += ( (a24t+a34t)*a1sq*m1 + (a14t+a34t)*a2sq*m2 + (a14t+a24t)*a3sq*m3) *field_mult3;

  }
}
