/* FILE: exchangeptwise.cc            -*-Mode: c++-*-
 *
 * 6 neighbor exchange energy on rectangular mesh, with
 * exchange coefficient A specified on a cell-by-cell
 * basis.  The effective coefficient between two neighboring
 * cells is calculated on the fly via 2.A_1.A_2/(A_1+A_2)
 *
 */

#include <string>

#include "atlas.h"
#include "nb.h"
#include "key.h"
#include "director.h"
#include "mesh.h"
#include "meshvalue.h"
#include "oxswarn.h"
#include "simstate.h"
#include "threevector.h"
#include "rectangularmesh.h"
#include "exchangeptwise.h"
#include "energy.h"     // Needed to make MSVC++ 5 happy

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_ExchangePtwise);

/* End includes */

// Revision information, set via CVS keyword substitution
static const Oxs_WarningMessageRevisionInfo revision_info
  (__FILE__,
   "$Revision: 1.20 $",
   "$Date: 2013/05/22 07:15:20 $",
   "$Author: donahue $",
   "Michael J. Donahue (michael.donahue@nist.gov)");


// Constructor
Oxs_ExchangePtwise::Oxs_ExchangePtwise(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr), mesh_id(0)
{
  // Process arguments
  OXS_GET_INIT_EXT_OBJECT("A",Oxs_ScalarField,A_init);

  VerifyAllInitArgsUsed();

  // Setup outputs
  maxspinangle_output.Setup(this,InstanceName(),"Max Spin Ang","deg",1,
             &Oxs_ExchangePtwise::UpdateDerivedOutputs);
  maxspinangle_output.Register(director,0);
  stage_maxspinangle_output.Setup(this,InstanceName(),
             "Stage Max Spin Ang","deg",1,
             &Oxs_ExchangePtwise::UpdateDerivedOutputs);
  stage_maxspinangle_output.Register(director,0);
  run_maxspinangle_output.Setup(this,InstanceName(),
             "Run Max Spin Ang","deg",1,
             &Oxs_ExchangePtwise::UpdateDerivedOutputs);
  run_maxspinangle_output.Register(director,0);
}

Oxs_ExchangePtwise::~Oxs_ExchangePtwise()
{}

OC_BOOL Oxs_ExchangePtwise::Init()
{
  mesh_id = 0;
  A.Release();
  return Oxs_Energy::Init();
}

void Oxs_ExchangePtwise::GetEnergy
(const Oxs_SimState& state,
 Oxs_EnergyData& oed
 ) const
{
  // Check mesh type
  const Oxs_CommonRectangularMesh* mesh
    = dynamic_cast<const Oxs_CommonRectangularMesh*>(state.mesh);
  if(mesh==NULL) {
    String msg =
      String("Import mesh (\"")
      + String(state.mesh->InstanceName())
      + String("\") to Oxs_ExchangePtwise::GetEnergy()"
             " routine of object \"") + String(InstanceName())
      + String("\" is not a rectangular mesh object.");
    throw Oxs_ExtError(msg.c_str());
  }

  // If periodic, collect data for distance determination
  // Periodic boundaries?
  int xperiodic=0, yperiodic=0, zperiodic=0;
  const Oxs_PeriodicRectangularMesh* pmesh
    = dynamic_cast<const Oxs_PeriodicRectangularMesh*>(mesh);
  if(pmesh!=NULL) {
    xperiodic = pmesh->IsPeriodicX();
    yperiodic = pmesh->IsPeriodicY();
    zperiodic = pmesh->IsPeriodicZ();
  }

  const Oxs_MeshValue<OC_REAL8m>& Ms_inverse = *(state.Ms_inverse);
  const Oxs_MeshValue<ThreeVector>& spin = state.spin;

  // Use supplied buffer space, and reflect that use in oed.
  oed.energy = oed.energy_buffer;
  oed.field = oed.field_buffer;
  Oxs_MeshValue<OC_REAL8m>& energy = *oed.energy_buffer;
  Oxs_MeshValue<ThreeVector>& field = *oed.field_buffer;

  OC_INDEX xdim = mesh->DimX();
  OC_INDEX ydim = mesh->DimY();
  OC_INDEX zdim = mesh->DimZ();
  OC_INDEX xydim = xdim*ydim;
  OC_INDEX xyzdim = xdim*ydim*zdim;

  if(mesh_id != state.mesh->Id()) {
    // This is either the first pass through, or else mesh
    // has changed.
    mesh_id = 0;
    A_init->FillMeshValue(state.mesh,A);
    // Check that we won't have any divide-by-zero problems
    // when we use A.
    for(OC_INDEX z=0;z<zdim;z++) {
      for(OC_INDEX y=0;y<ydim;y++) {
        for(OC_INDEX x=0;x<xdim;x++) {
          OC_INDEX i = mesh->Index(x,y,z); // Get base linear address
          OC_REAL8m Ai = A[i];
          for(OC_INDEX dir=0;dir<3;dir++) { // 0=>x, 1=>y, 2=>z
            OC_INDEX j = -1;
            OC_INDEX x2=x,y2=y,z2=z;
            switch(dir) {
            case 0:
              if(x+1<xdim) { j=i+1;     x2+=1; }
              else if(xperiodic) { j=i+1-xdim; x2=0; }
              break;
            case 1:
              if(y+1<ydim) { j=i+xdim;  y2+=1; }
              else if(yperiodic) { j=i+xdim-xydim; y2=0; }
              break;
            default:
              if(z+1<zdim) { j=i+xydim; z2+=1; }
              else if(zperiodic) { j=i+xydim-xyzdim; z2=0; }
              break;
            }
            if(j<0) continue; // Edge case; skip
            OC_REAL8m Aj=A[j];
            if(fabs(Ai+Aj)<1.0
               && fabs(Ai*Aj)>(DBL_MAX/2.)*(fabs(Ai+Aj))) {
              char buf[2000];
              Oc_Snprintf(buf,sizeof(buf),
                          "Exchange coefficient A initialization in"
                          " Oxs_ExchangePtwise::GetEnergy()"
                          " routine of object %.1000s yields infinite value"
                          " for A: A(%u,%u,%u)=%g, A(%u,%u,%u)=%g\n",
                          InstanceName(),x,y,z,static_cast<double>(Ai),
                          x2,y2,z2,static_cast<double>(Aj));
              throw Oxs_ExtError(buf);
            }
          }
        } // x++
      } // y++
    } // z++
    mesh_id = state.mesh->Id();
  } // mesh_id != state.mesh->Id()

  // First loop through and zero out entire energy and field
  // meshes.  This could be done inside the main body of the
  // loop, but it makes the logic a bit more complicated.
  OC_INDEX size = state.mesh->Size();
  OC_INDEX index;
  for(index=0;index<size;index++) energy[index]=0.;
  for(index=0;index<size;index++) field[index].Set(0.,0.,0.);

  // Now run through mesh and accumulate energies and fields.
  OC_REAL8m wgtx = 1.0/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
  OC_REAL8m wgty = 1.0/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
  OC_REAL8m wgtz = 1.0/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

  OC_REAL8m hcoef = 2/MU0;

  // Exchange energy density is
  //
  //     A.(1-m_i*m_j)/d^2                      (1)
  //
  // where A is effective exchange coefficient, and d is
  // the distance between the centers of cells i and j.  In
  // this code effective A is calculated from A_i and A_j
  // via A = 2.A_i.A_j/(A_i+A_j).  This energy density
  // is accumated at both cells i & j.
  //   The fields are
  //
  //     H_i = (2/MU0).[A/(Ms_i.d^2)].m_j       (2)
  //     H_j = (2/MU0).[A/(Ms_j.d^2)].m_i
  //
  // where the lead factor of 2 comes because in the total
  // energy expression, (1) occurs twice, once for cell i
  // and once for cell j.

  OC_REAL8m maxdot = 0;

  for(OC_INDEX z=0;z<zdim;++z) {
    for(OC_INDEX y=0;y<ydim;++y) {
      for(OC_INDEX x=0;x<xdim;++x) {
        OC_INDEX i = mesh->Index(x,y,z); // Get base linear address
        const ThreeVector& spini = spin[i];
        OC_REAL8m Msii = Ms_inverse[i];
        OC_REAL8m Ai = A[i];
        if(Msii == 0.0 || Ai==0.0) continue;
        // Iterate through 3 neighbor directions, +x, +y and +z
        for(OC_INDEX dir=0;dir<3;++dir) { // 0=>x, 1=>y, 2=>z
          OC_INDEX j = -1;
          OC_REAL8m wgt=0.;
          switch(dir) {
          case 0:
            wgt=wgtx;
            if(x+1<xdim)       j=i+1;
            else if(xperiodic) j=i+1-xdim;
            break;
          case 1:
            wgt=wgty;
            if(y+1<ydim)       j=i+xdim;
            else if(yperiodic) j=i+xdim-xydim;
            break;
          default:
            wgt=wgtz;
            if(z+1<zdim)       j=i+xydim;
            else if(zperiodic) j=i+xydim-xyzdim;
            break;
          }
          if(j<0) continue; // Edge case; skip
          OC_REAL8m Aj=A[j];
          if(Aj == 0.0) continue;
          OC_REAL8m Msij=Ms_inverse[j];
          if(Msij==0.0) continue;
          OC_REAL8m acoef = 2*wgt*((Ai*Aj)/(Ai+Aj));
          const ThreeVector& spinj = spin[j];
          ThreeVector mdiff = spini - spinj;
          OC_REAL8m dot = mdiff.MagSq();
          if(dot>maxdot) maxdot = dot;
          mdiff *= acoef;
          OC_REAL8m eij = mdiff*spini;
          energy[i]+=eij;
          energy[j]+=eij;
          mdiff *= hcoef;
          field[i] += -1*Msii*mdiff;
          field[j] +=    Msij*mdiff;
        } // Neighbor direction loop
      } // ++x
    } // ++y
  } // ++z

  // Set maxang data
  const OC_REAL8m arg = 0.5*Oc_Sqrt(maxdot);
  const OC_REAL8m maxang = ( arg >= 1.0 ? 180.0 : asin(arg)*(360.0/PI));

  OC_REAL8m dummy_value;
  String msa_name = MaxSpinAngleStateName();
  if(state.GetDerivedData(msa_name,dummy_value)) {
    // Ideally, energy values would never be computed more than once
    // for any one state, but in practice it seems inevitable that
    // such will occur on occasion.  For example, suppose a user
    // requests output on a state obtained by a stage crossing (as
    // opposed to a state obtained through a normal intrastage step);
    // a subsequent ::Step operation will re-compute the energies
    // because not all the information needed by the step transition
    // machinery is cached from an energy computation.  Even user
    // output requests on post-::Step states is problematic if some of
    // the requested output is not cached as part of the step
    // proceedings.  A warning is put into place below for debugging
    // purposes, but in general an error is raised only if results
    // from the recomputation are different than originally.
#ifndef NDEBUG
    static Oxs_WarningMessage maxangleset(3);
    maxangleset.Send(revision_info,OC_STRINGIFY(__LINE__),
                     "Programming error?"
                     " Oxs_ExchangePtwise max spin angle set twice.");
#endif
    // Max angle is computed by taking acos of the dot product
    // of neighboring spin vectors.  The relative error can be
    // quite large if the spins are nearly parallel.  The proper
    // error comparison is between the cos of the two values.
    // See NOTES VI, 6-Sep-2012, p71.
    OC_REAL8m diff = (dummy_value-maxang)*(PI/180.);
    OC_REAL8m sum  = (dummy_value+maxang)*(PI/180.);
    if(sum > PI ) sum = 2*PI - sum;
    if(fabs(diff*sum)>8*OC_REAL8_EPSILON) {
      char errbuf[1024];
      Oc_Snprintf(errbuf,sizeof(errbuf),
                  "Programming error:"
                  " Oxs_ExchangePtwise max spin angle set to"
                  " two different values;"
                  " orig val=%#.17g, new val=%#.17g",
                  dummy_value,maxang);
      throw Oxs_ExtError(this,errbuf);
    }
  } else {
    state.AddDerivedData(msa_name,maxang);
  }

  // Run and stage angle data depend on data from the previous state.
  // In the case that the energy (and hence max stage and run angle)
  // for the current state was computed previously, then the previous
  // state may have been dropped.  So, compute and save run and stage
  // angle data iff they are not already computed.

  // Check stage and run max angle data from previous state
  const Oxs_SimState* oldstate = NULL;
  OC_REAL8m stage_maxang = -1;
  OC_REAL8m run_maxang = -1;
  String smsa_name = StageMaxSpinAngleStateName();
  String rmsa_name = RunMaxSpinAngleStateName();
  if(state.previous_state_id &&
     0 != (oldstate
      = director->FindExistingSimulationState(state.previous_state_id)) ) {
    if(oldstate->stage_number != state.stage_number) {
      stage_maxang = 0.0;
    } else {
      if(oldstate->GetDerivedData(smsa_name,dummy_value)) {
        stage_maxang = dummy_value;
      }
    }
    if(oldstate->GetDerivedData(rmsa_name,dummy_value)) {
      run_maxang = dummy_value;
    }
  }
  if(stage_maxang<maxang) stage_maxang = maxang;
  if(run_maxang<maxang)   run_maxang = maxang;

  // Stage max angle data
  if(!state.GetDerivedData(smsa_name,dummy_value)) {
    state.AddDerivedData(smsa_name,stage_maxang);
  }

  // Run max angle data
  if(!state.GetDerivedData(rmsa_name,dummy_value)) {
    state.AddDerivedData(rmsa_name,run_maxang);
  }
}

void Oxs_ExchangePtwise::UpdateDerivedOutputs(const Oxs_SimState& state)
{
  maxspinangle_output.cache.state_id
    = stage_maxspinangle_output.cache.state_id
    = run_maxspinangle_output.cache.state_id
    = 0;  // Mark change in progress

  String dummy_name = MaxSpinAngleStateName();
  if(!state.GetDerivedData(dummy_name.c_str(),
                           maxspinangle_output.cache.value)) {
    // Error; This should always be set.  For now, just set the value to
    // -1, but in the future should consider throwing an exception.
    maxspinangle_output.cache.value = -1.0;
  }

  String stage_dummy_name = StageMaxSpinAngleStateName();
  if(!state.GetDerivedData(stage_dummy_name.c_str(),
                           stage_maxspinangle_output.cache.value)) {
    // Error; This should always be set.  For now, just set the value to
    // -1, but in the future should consider throwing an exception.
    stage_maxspinangle_output.cache.value = -1.0;
  }

  String run_dummy_name = RunMaxSpinAngleStateName();
  if(!state.GetDerivedData(run_dummy_name.c_str(),
                           run_maxspinangle_output.cache.value)) {
    // Error; This should always be set.  For now, just set the value to
    // -1, but in the future should consider throwing an exception.
    run_maxspinangle_output.cache.value = -1.0;
  }

  maxspinangle_output.cache.state_id
    = stage_maxspinangle_output.cache.state_id
    = run_maxspinangle_output.cache.state_id
    = state.Id();
}
