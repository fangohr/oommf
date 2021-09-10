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
  : Oxs_ChunkEnergy(name,newdtr,argstr),
    mesh_id(0), energy_density_error_estimate(-1)
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
  energy_density_error_estimate = -1;
  return Oxs_Energy::Init();
}

void Oxs_ExchangePtwise::ComputeEnergyChunkInitialize
(const Oxs_SimState& state,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oc_AlignedVector<Oxs_ComputeEnergyDataThreadedAux>& /* thread_ocedtaux */,
 int number_of_threads) const
{
  if(mesh_id != state.mesh->Id()) {
    // This is either the first pass through, or else mesh
    // has changed.
    mesh_id = 0;

    // Check mesh type
    const Oxs_CommonRectangularMesh* mesh
      = dynamic_cast<const Oxs_CommonRectangularMesh*>(state.mesh);
    if(mesh==NULL) {
      String msg =
        String("Import mesh (\"")
        + String(state.mesh->InstanceName())
        + String("\") to Oxs_ExchangePtwise::ComputeEnergyChunkInitialize()"
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

    OC_INDEX xdim = mesh->DimX();
    OC_INDEX ydim = mesh->DimY();
    OC_INDEX zdim = mesh->DimZ();
    OC_INDEX xydim = xdim*ydim;
    OC_INDEX xyzdim = xdim*ydim*zdim;

    OC_REAL8m wgtx = 1.0/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
    OC_REAL8m wgty = 1.0/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
    OC_REAL8m wgtz = 1.0/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

    A_init->FillMeshValue(state.mesh,A);
    // Check that we won't have any divide-by-zero problems
    // when we use A.
    OC_REAL8m max_Aeff = 0.0;
    for(OC_INDEX z=0;z<zdim;z++) {
      for(OC_INDEX y=0;y<ydim;y++) {
        for(OC_INDEX x=0;x<xdim;x++) {
          OC_INDEX i = mesh->Index(x,y,z); // Get base linear address
          OC_REAL8m Ai = A[i];
          for(OC_INDEX dir=0;dir<3;dir++) { // 0=>x, 1=>y, 2=>z
            OC_INDEX j = -1;
            OC_INDEX x2=x,y2=y,z2=z;
            OC_REAL8m dir_wgt=0;
            switch(dir) {
            case 0:
              if(x+1<xdim) { j=i+1;     x2+=1; }
              else if(xperiodic) { j=i+1-xdim; x2=0; }
              dir_wgt = wgtx;
              break;
            case 1:
              if(y+1<ydim) { j=i+xdim;  y2+=1; }
              else if(yperiodic) { j=i+xdim-xydim; y2=0; }
              dir_wgt = wgty;
              break;
            default:
              if(z+1<zdim) { j=i+xydim; z2+=1; }
              else if(zperiodic) { j=i+xydim-xyzdim; z2=0; }
              dir_wgt = wgtz;
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
            OC_REAL8m testA = fabs(2*Ai*Aj*dir_wgt/(Ai+Aj));
            if(testA>max_Aeff) max_Aeff = testA;
          }
        } // x++
      } // y++
    } // z++

    energy_density_error_estimate = 16*OC_REAL8m_EPSILON*max_Aeff;
    mesh_id = state.mesh->Id();
  } // mesh_id != state.mesh->Id()

  if(maxdot.size() != (vector<OC_REAL8m>::size_type)number_of_threads) {
    maxdot.resize(number_of_threads);
  }
  for(int i=0;i<number_of_threads;++i) {
    maxdot[i] = 0.0; // Minimum possible value for (m_i-m_j).MagSq()
  }

  ocedt.energy_density_error_estimate = energy_density_error_estimate;
}

void Oxs_ExchangePtwise::ComputeEnergyChunkFinalize
(const Oxs_SimState& state,
 Oxs_ComputeEnergyDataThreaded& /* ocedt */,
 Oc_AlignedVector<Oxs_ComputeEnergyDataThreadedAux>&
    /* thread_ocedtaux */,
 int number_of_threads) const
{
  // Set max angle data
  OC_REAL8m total_maxdot = 0.0;
  for(int i=0;i<number_of_threads;++i) {
    if(maxdot[i]>total_maxdot) total_maxdot = maxdot[i];
  }
  const OC_REAL8m arg = 0.5*sqrt(total_maxdot);
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

void Oxs_ExchangePtwise::ComputeEnergyChunk
(const Oxs_SimState& state,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
 OC_INDEX node_start,
 OC_INDEX node_stop,
 int threadnumber
 ) const
{
#ifndef NDEBUG
  if(node_stop>state.mesh->Size() || node_start>node_stop) {
    throw Oxs_ExtError(this,"Programming error:"
                       " Invalid node_start/node_stop values");
  }
#endif
  const Oxs_MeshValue<OC_REAL8m>& Ms_inverse = *(state.Ms_inverse);
  const Oxs_MeshValue<ThreeVector>& spin = state.spin;


  // Downcast mesh
  const Oxs_CommonRectangularMesh* mesh
    = dynamic_cast<const Oxs_CommonRectangularMesh*>(state.mesh);
  assert(mesh!=0);

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

  OC_INDEX xdim = mesh->DimX();
  OC_INDEX ydim = mesh->DimY();
  OC_INDEX zdim = mesh->DimZ();
  OC_INDEX xydim = xdim*ydim;
  OC_INDEX xyzdim = xdim*ydim*zdim;

  OC_REAL8m wgtx = -1.0/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
  OC_REAL8m wgty = -1.0/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
  OC_REAL8m wgtz = -1.0/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

  OC_REAL8m hcoef = -2/MU0;

  Nb_Xpfloat energy_sum = 0.0;
  OC_REAL8m thread_maxdot = maxdot[threadnumber];
  // Note: For maxangle calculation, it suffices to check
  // spin[j]-spin[i] for j>i.

  OC_INDEX x,y,z;
  mesh->GetCoords(node_start,x,y,z);

  OC_INDEX i = node_start;
  while(i<node_stop) {
    OC_INDEX xstop = xdim;
    if(xdim-x>node_stop-i) xstop = x + (node_stop-i);
    while(x<xstop) {
      ThreeVector base = spin[i];
      OC_REAL8m Msii = Ms_inverse[i];
      if(0.0 == Msii) {
        if(ocedt.energy) (*ocedt.energy)[i] = 0.0;
        if(ocedt.H)      (*ocedt.H)[i].Set(0.,0.,0.);
        if(ocedt.mxH)    (*ocedt.mxH)[i].Set(0.,0.,0.);
        ++i;   ++x;
        continue;
      }
      OC_REAL8m Ai = A[i];
      ThreeVector sum(0.,0.,0.);
      if(z>0 || zperiodic) {
        OC_INDEX j = i-xydim;
        if(z==0) j += xyzdim;
        OC_REAL8m Aj=A[j];
        if(Aj!=0 && Ms_inverse[j]!=0.0) {
          OC_REAL8m acoef = 2*((Ai*Aj)/(Ai+Aj));
          ThreeVector diff = (spin[j] - base);
          OC_REAL8m dot = diff.MagSq();
          sum += acoef*wgtz*diff;
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
      }
      if(y>0 || yperiodic) {
        OC_INDEX j = i-xdim;
        if(y==0) j += xydim;
        OC_REAL8m Aj=A[j];
        if(Aj!=0 && Ms_inverse[j]!=0.0) {
          OC_REAL8m acoef = 2*((Ai*Aj)/(Ai+Aj));
          ThreeVector diff = (spin[j] - base);
          OC_REAL8m dot = diff.MagSq();
          sum += acoef*wgty*diff;
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
      }
      if(x>0 || xperiodic) {
        OC_INDEX j = i-1;
        if(x==0) j += xdim;
        OC_REAL8m Aj=A[j];
        if(Aj!=0 && Ms_inverse[j]!=0.0) {
          OC_REAL8m acoef = 2*((Ai*Aj)/(Ai+Aj));
          ThreeVector diff = (spin[j] - base);
          OC_REAL8m dot = diff.MagSq();
          sum += acoef*wgtx*diff;
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
      }
      if(x<xdim-1 || xperiodic) {
        OC_INDEX j = i+1;
        if(x==xdim-1) j -= xdim;
        OC_REAL8m Aj=A[j];
        OC_REAL8m acoef = 2*((Ai*Aj)/(Ai+Aj));
        if(Ms_inverse[j]!=0.0) sum += acoef*wgtx*(spin[j] - base);
      }
      if(y<ydim-1 || yperiodic) {
        OC_INDEX j = i+xdim;
        if(y==ydim-1) j -= xydim;
        OC_REAL8m Aj=A[j];
        OC_REAL8m acoef = 2*((Ai*Aj)/(Ai+Aj));
        if(Ms_inverse[j]!=0.0) sum += acoef*wgty*(spin[j] - base);
      }
      if(z<zdim-1 || zperiodic) {
        OC_INDEX j = i+xydim;
        if(z==zdim-1) j -= xyzdim;
        OC_REAL8m Aj=A[j];
        OC_REAL8m acoef = 2*((Ai*Aj)/(Ai+Aj));
        if(Ms_inverse[j]!=0.0) sum += acoef*wgtz*(spin[j]- base);
      }

      OC_REAL8m ei = base.x*sum.x + base.y*sum.y + base.z*sum.z;
      OC_REAL8m hmult = hcoef*Msii;
      sum.x *= hmult;  sum.y *= hmult;   sum.z *= hmult;
      OC_REAL8m tx = base.y*sum.z - base.z*sum.y;
      OC_REAL8m ty = base.z*sum.x - base.x*sum.z;
      OC_REAL8m tz = base.x*sum.y - base.y*sum.x;

      energy_sum += ei;
      if(ocedt.energy)       (*ocedt.energy)[i] = ei;
      if(ocedt.energy_accum) (*ocedt.energy_accum)[i] += ei;
      if(ocedt.H)       (*ocedt.H)[i] = sum;
      if(ocedt.H_accum) (*ocedt.H_accum)[i] += sum;
      if(ocedt.mxH)       (*ocedt.mxH)[i] = ThreeVector(tx,ty,tz);
      if(ocedt.mxH_accum) (*ocedt.mxH_accum)[i] += ThreeVector(tx,ty,tz);
      ++i;   ++x;
    }
    x=0;
    if((++y)>=ydim) {
      y=0;
      ++z;
    }
  }

  ocedtaux.energy_total_accum += energy_sum * mesh->Volume(0);
  /// All cells have same volume in an Oxs_RectangularMesh.

  maxdot[threadnumber] = thread_maxdot;
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
