/* FILE: cubicanisotropy.cc            -*-Mode: c++-*-
 *
 * Cubic Anisotropy, derived from Oxs_Energy class.
 *
 */

#include <string>

#include "oc.h"
#include "nb.h"
#include "threevector.h"
#include "director.h"
#include "simstate.h"
#include "ext.h"
#include "key.h"
#include "mesh.h"
#include "meshvalue.h"
#include "uniformscalarfield.h"
#include "uniformvectorfield.h"
#include "cubicanisotropy.h"
#include "energy.h"		// Needed to make MSVC++ 5 happy

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_CubicAnisotropy);

/* End includes */


// Constructor
Oxs_CubicAnisotropy::Oxs_CubicAnisotropy(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_ChunkEnergy(name,newdtr,argstr),
    aniscoeftype(ANIS_UNKNOWN), mesh_id(0),
    max_K1(-1.0),
    K1_is_uniform(0), Ha_is_uniform(0),
    axis1_is_uniform(0), axis2_is_uniform(0),
    uniform_K1_value(0.), uniform_Ha_value(0.)
{
  // Process arguments
  OC_BOOL has_K1 = HasInitValue("K1");
  OC_BOOL has_Ha = HasInitValue("Ha");
  if(has_K1 && has_Ha) {
    throw Oxs_ExtError(this,"Invalid anisotropy coefficient request:"
			 " both K1 and Ha specified; only one should"
			 " be given.");
  } else if(has_K1) {
    OXS_GET_INIT_EXT_OBJECT("K1",Oxs_ScalarField,K1_init);
    Oxs_UniformScalarField* tmpK1ptr
      = dynamic_cast<Oxs_UniformScalarField*>(K1_init.GetPtr());
    if(tmpK1ptr) {
      // Initialization is via a uniform field; set up uniform
      // K1 variables.
      K1_is_uniform = 1;
      uniform_K1_value = tmpK1ptr->SoleValue();
    }
    aniscoeftype = K1_TYPE;
  } else {
    OXS_GET_INIT_EXT_OBJECT("Ha",Oxs_ScalarField,Ha_init);
    Oxs_UniformScalarField* tmpHaptr
      = dynamic_cast<Oxs_UniformScalarField*>(Ha_init.GetPtr());
    if(tmpHaptr) {
      // Initialization is via a uniform field; set up uniform
      // Ha variables.
      Ha_is_uniform = 1;
      uniform_Ha_value = tmpHaptr->SoleValue();
    }
    aniscoeftype = Ha_TYPE;
  }

  OXS_GET_INIT_EXT_OBJECT("axis1",Oxs_VectorField,axis1_init);
  Oxs_UniformVectorField* tmpptr1
    = dynamic_cast<Oxs_UniformVectorField*>(axis1_init.GetPtr());
  if(tmpptr1) {
    // Initialization is via a uniform field.  For convenience,
    // modify the size of the field components to norm 1, as
    // required for the axis specification.  This allows the
    // user to specify the axis direction as, for example, {1,1,1},
    // as opposed to {0.57735027,0.57735027,0.57735027}, or
    //
    //      Specify Oxs_UniformVectorField {
    //        norm 1 
    //        vector { 1 1 1 } 
    //    }
    tmpptr1->SetMag(1.0);
    axis1_is_uniform = 1;
    uniform_axis1_value = tmpptr1->SoleValue();
  }
  OXS_GET_INIT_EXT_OBJECT("axis2",Oxs_VectorField,axis2_init);
  Oxs_UniformVectorField* tmpptr2
    = dynamic_cast<Oxs_UniformVectorField*>(axis2_init.GetPtr());
  if(tmpptr2) {
    tmpptr2->SetMag(1.0);
    axis2_is_uniform = 1;
    uniform_axis2_value = tmpptr2->SoleValue();
  }
  VerifyAllInitArgsUsed();
}

OC_BOOL Oxs_CubicAnisotropy::Init()
{
  mesh_id = 0;
  K1.Release();
  Ha.Release();
  axis1.Release();
  axis2.Release();
  max_K1 = -1.0;
  return Oxs_ChunkEnergy::Init();
}

void Oxs_CubicAnisotropy::ComputeEnergyChunkInitialize
(const Oxs_SimState& state,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oc_AlignedVector<Oxs_ComputeEnergyDataThreadedAux>& /* thread_ocedtaux */,
 int /* number_of_threads */) const
{
  const Oxs_Mesh* mesh = state.mesh;
  if(mesh_id !=  mesh->Id()) {
    // This is either the first pass through, or else mesh has changed.
    // Initialize/update data fields.  NB: At a lower level, this may
    // potentially involve calls back into the Tcl interpreter.  Per Tcl
    // spec, only the thread originating the interpreter is allowed to
    // make calls into it.  The ComputeEnergyChunkInitialize member is
    // guaranteed to be called in the main thread.
    if(aniscoeftype == K1_TYPE) {
      if(K1_is_uniform) {
        max_K1 = fabs(uniform_K1_value);
      } else {
        K1_init->FillMeshValue(state.mesh,K1);
        max_K1=0.0;
        const OC_INDEX size = state.mesh->Size();
        for(OC_INDEX i=0;i<size;i++) {
          OC_REAL8m test = fabs(K1[i]);
          if(test>max_K1) max_K1 = test;
        }
      }
    } else if(aniscoeftype == Ha_TYPE) {
      if(!Ha_is_uniform) Ha_init->FillMeshValue(state.mesh,Ha);
      max_K1=0.0;
      const OC_INDEX size = state.mesh->Size();
      const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);
      OC_REAL8m tmpHa = uniform_Ha_value;
      for(OC_INDEX i=0;i<size;i++) {
        // Note: This code assumes that Ms doesn't change
        // for the lifetime of state.mesh.
        if(!Ha_is_uniform) tmpHa = Ha[i];
        OC_REAL8m test = fabs(tmpHa*Ms[i]);
        if(test>max_K1) max_K1 = test;
      }
      max_K1 *= 0.5*MU0;
    }
    if(!axis1_is_uniform || !axis2_is_uniform) {
      axis1_init->FillMeshValue(mesh,axis1);
      axis2_init->FillMeshValueOrthogonal(mesh,axis1,axis2);
      const OC_INDEX size = mesh->Size();
      for(OC_INDEX i=0;i<size;i++) {
        // Much of the code below requires axis1 and axis2 to be
        // orthogonal unit vectors.  Guarantee this is the case:
        const OC_REAL8m eps = 1e-14;
        if(fabs(axis1[i].MagSq()-1)>eps) {
          String msg =
            String("Invalid initialization detected for object ")
            + String(InstanceName())
            + String(": Anisotropy axis 1 isn't norm 1");
          throw Oxs_ExtError(msg.c_str());
        }
        if(fabs(axis2[i].MagSq()-1)>eps) {
          String msg = 
            String("Invalid initialization detected for object ")
            + String(InstanceName())
            + String(": Anisotropy axis 2 isn't norm 1");
          throw Oxs_ExtError(msg.c_str());
        }
        if(fabs(axis1[i]*axis2[i])>eps) {
          String msg =
            String("Invalid initialization detected for object ")
            + String(InstanceName())
            + String(": Specified anisotropy axes aren't perpendicular");
          throw Oxs_ExtError(msg.c_str());
        }
      }
    } else {
      // axis1 and axis2 are uniform.  Check norm and orthogonality
      const OC_REAL8m eps = 1e-14;
      if(fabs(uniform_axis1_value.MagSq()-1)>eps) { // Safety
        String msg =
          String("Invalid initialization detected for object ")
          + String(InstanceName())
          + String(": Anisotropy axis 1 isn't norm 1");
        throw Oxs_ExtError(msg.c_str());
      }
      if(fabs(uniform_axis2_value.MagSq()-1)>eps) { // Safety
        String msg = 
          String("Invalid initialization detected for object ")
          + String(InstanceName())
          + String(": Anisotropy axis 2 isn't norm 1");
        throw Oxs_ExtError(msg.c_str());
      }
      if(fabs(uniform_axis1_value*uniform_axis2_value)>eps) {
        String msg =
          String("Invalid initialization detected for object ")
          + String(InstanceName())
          + String(": Specified anisotropy axes aren't perpendicular");
        throw Oxs_ExtError(msg.c_str());
      }
    }
    mesh_id = mesh->Id();
  }
  ocedt.energy_density_error_estimate = 4*OC_REAL8m_EPSILON*max_K1;
}

void Oxs_CubicAnisotropy::ComputeEnergyChunk
(const Oxs_SimState& state,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
 OC_INDEX node_start,
 OC_INDEX node_stop,
 int /* threadnumber */
 ) const
{
  assert(node_start<=node_stop && node_stop<=state.mesh->Size());

  const Oxs_Mesh* mesh = state.mesh;
  const Oxs_MeshValue<OC_REAL8m>& Ms         = *(state.Ms);
  const Oxs_MeshValue<OC_REAL8m>& Ms_inverse = *(state.Ms_inverse);
  const Oxs_MeshValue<ThreeVector>& spin = state.spin;

  Nb_Xpfloat energy_sum = 0.0;

  OC_REAL8m k = uniform_K1_value;
  OC_REAL8m field_mult = -1*uniform_Ha_value;
  ThreeVector unifaxis1 = uniform_axis1_value;
  ThreeVector unifaxis2 = uniform_axis2_value;

  for(OC_INDEX i=node_start;i<node_stop;++i) {
    // This code requires u1 and u2 to be orthonormal, and m to be a
    // unit vector.  Basically, decompose
    //
    //             m = a1.u1 + a2.u2 + a3.u3
    //               =  m1   +  m2   +  m3
    //
    // where u3=u1xu2, a1=m*u1, a2=m*u2, a3=m*u3.
    //
    // Then the energy is
    //                 2  2     2  2     2  2
    //            K (a1 a2  + a1 a3  + a2 a3 )
    //
    // and the field in say the u1 direction is
    //              2    2                 2    2
    //         C (a2 + a3 ) a1 . u1 = C (a2 + a3 ) m1
    //
    // where C = -2K/(MU0 Ms).
    //
    // In particular, note that
    //           2         2     2
    //         a3  = 1 - a1  - a2
    // and
    //         m3  = m - m1 - m2
    //
    // This shows that energy and field can be computed without
    // explicitly calculating u3.  Depending upon the processor
    // architecture, the cross product evaluation to get u3 may not be
    // very expensive, and may be more accurate.  At a minimum, in the
    // above expressions one should at least insure that a3^2 is
    // non-negative.

    if(aniscoeftype == K1_TYPE) {
      if(!K1_is_uniform) k = K1[i];
      field_mult = (-2.0/MU0)*k*Ms_inverse[i];
    } else {
      if(!Ha_is_uniform) field_mult = -1*Ha[i];
      k = -0.5*MU0*field_mult*Ms[i];
    }
    if(k==0.0 || field_mult==0.0) { // Includes Ms==0.0 case
      if(ocedt.energy) (*ocedt.energy)[i] = 0.0;
      if(ocedt.H)      (*ocedt.H)[i].Set(0.,0.,0.);
      if(ocedt.mxH)    (*ocedt.mxH)[i].Set(0.,0.,0.);
      continue;
    }

#if 0
    const ThreeVector u1 = (axis1_is_uniform ? unifaxis1 : axis1[i]);
    const ThreeVector u2 = (axis2_is_uniform ? unifaxis2 : axis2[i]);
    const ThreeVector  m = spin[i];
    ThreeVector u3 = u1;    u3 ^= u2;
    OC_REAL8m a1 = u1*m;  OC_REAL8m a1sq = a1*a1;
    OC_REAL8m a2 = u2*m;  OC_REAL8m a2sq = a2*a2;
    OC_REAL8m a3 = u3*m;  OC_REAL8m a3sq = a3*a3;

    ThreeVector H = (a1*(a2sq+a3sq))*u1;
    H.Accum(a2*(a1sq+a3sq),u2);
    H.Accum(a3*(a1sq+a2sq),u3);
    H *= field_mult;

    OC_REAL8m tx = m.y*H.z - m.z*H.y; // mxH
    OC_REAL8m ty = m.z*H.x - m.x*H.z;
    OC_REAL8m tz = m.x*H.y - m.y*H.x;

    OC_REAL8m ei = k * (a1sq*a2sq+a1sq*a3sq+a2sq*a3sq);
    energy_sum += ei * mesh->Volume(i);

    if(ocedt.energy)       (*ocedt.energy)[i] = ei;
    if(ocedt.energy_accum) (*ocedt.energy_accum)[i] += ei;
    if(ocedt.H)       (*ocedt.H)[i] = H;
    if(ocedt.H_accum) (*ocedt.H_accum)[i] += H;
    if(ocedt.mxH)       (*ocedt.mxH)[i] = ThreeVector(tx,ty,tz);
    if(ocedt.mxH_accum) (*ocedt.mxH_accum)[i] += ThreeVector(tx,ty,tz);
#else
    // This #if branch eschews direct computation of a3 and u3.  This
    // may be notably faster, especially on machine with a limited
    // number of floating point registers.
    const ThreeVector& u1 = (axis1_is_uniform ? unifaxis1 : axis1[i]);
    const ThreeVector& u2 = (axis2_is_uniform ? unifaxis2 : axis2[i]);
    const ThreeVector&  m = spin[i];
    OC_REAL8m a2 = m*u2;
    OC_REAL8m a1 = m*u1;
    ThreeVector H = (a1*(1-2*a1*a1-a2*a2))*u1;
    H.Accum(a2*(1-a1*a1-2*a2*a2),u2);
    H.Accum(a1*a1+a2*a2,m);

    OC_REAL8m ei = k * (a1*a1*a2*a2+(a1*a1+a2*a2)*(1.0-(a1*a1+a2*a2)));
    H *= field_mult;

    if(ocedt.H)       (*ocedt.H)[i] = H;
    if(ocedt.H_accum) (*ocedt.H_accum)[i] += H;

    OC_REAL8m tz = m.x*H.y;  // t = m x H
    OC_REAL8m ty = m.x*H.z;
    OC_REAL8m tx = m.y*H.z;
    tz -= m.y*H.x;
    ty  = m.z*H.x - ty;
    tx -= m.z*H.y;

    energy_sum += ei * mesh->Volume(i);

    if(ocedt.energy)       (*ocedt.energy)[i] = ei;
    if(ocedt.energy_accum) (*ocedt.energy_accum)[i] += ei;
    if(ocedt.mxH)       (*ocedt.mxH)[i] = ThreeVector(tx,ty,tz);
    if(ocedt.mxH_accum) (*ocedt.mxH_accum)[i] += ThreeVector(tx,ty,tz);
#endif
  }

  ocedtaux.energy_total_accum += energy_sum;
  // ocedtaux.pE_pt_accum += 0.0;
}

// Optional interface for conjugate-gradient evolver.
// For details on this code, see NOTES VI, 21-July-2011, pp 10-11.
OC_INT4m
Oxs_CubicAnisotropy::IncrementPreconditioner(PreconditionerData& pcd)
{
  if(pcd.type != DIAGONAL) return 0; // Unsupported preconditioning type

  // Nothing to do --- bilinear part of this term is 0.
  return 1;
}
