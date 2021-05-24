/* FILE: randomsiteexchange.cc            -*-Mode: c++-*-
 *
 * 6 neighbor exchange energy on random links on rectangular mesh,
 * with random A.
 *
 */

#include <string>

#include "nb.h"
#include "director.h"
#include "mesh.h"
#include "meshvalue.h"
#include "oxswarn.h"
#include "simstate.h"
#include "threevector.h"
#include "rectangularmesh.h"
#include "randomsiteexchange.h"
#include "energy.h"             // Needed to make MSVC++ 5 happy

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_RandomSiteExchange);

/* End includes */

// Revision information, set via CVS keyword substitution
static const Oxs_WarningMessageRevisionInfo revision_info
  (__FILE__,
   "$Revision: 1.17 $",
   "$Date: 2013/05/22 07:15:22 $",
   "$Author: donahue $",
   "Michael J. Donahue (michael.donahue@nist.gov)");


// Constructor
Oxs_RandomSiteExchange::Oxs_RandomSiteExchange(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr),
    mesh_id(0), energy_density_error_estimate(-1)
{
  // Process arguments
  Amin = GetRealInitValue("Amin");
  Amax = GetRealInitValue("Amax");
  if(Amax<Amin) {
    String msg = String("Invalid initialization detected for object ")
      + String(InstanceName())
      + String(": Amax must be >= Amin");
    throw Oxs_ExtError(msg.c_str());
  }
  linkprob = GetRealInitValue("linkprob");
  if(linkprob<0.) {
    String msg = String("Invalid initialization detected for object ")
      + String(InstanceName())
      + String(": linkprob value must be bigger than 0.");
    throw Oxs_ExtError(msg.c_str());
  }
  if(linkprob>1.) {
    String msg = String("Invalid initialization detected for object ")
      + String(InstanceName())
      + String(": linkprob value must lie in the range [0,1].");
    throw Oxs_ExtError(msg.c_str());
  }

  VerifyAllInitArgsUsed();

  // Setup outputs
  maxspinangle_output.Setup(this,InstanceName(),"Max Spin Ang","deg",1,
                            &Oxs_RandomSiteExchange::UpdateDerivedOutputs);
  maxspinangle_output.Register(director,0);
  stage_maxspinangle_output.Setup(this,InstanceName(),"Stage Max Spin Ang","deg",1,
                            &Oxs_RandomSiteExchange::UpdateDerivedOutputs);
  stage_maxspinangle_output.Register(director,0);
  run_maxspinangle_output.Setup(this,InstanceName(),"Run Max Spin Ang","deg",1,
                            &Oxs_RandomSiteExchange::UpdateDerivedOutputs);
  run_maxspinangle_output.Register(director,0);
}

Oxs_RandomSiteExchange::~Oxs_RandomSiteExchange()
{}

OC_BOOL Oxs_RandomSiteExchange::Init()
{
  mesh_id = 0;
  energy_density_error_estimate = -1;
  links.clear();
  return Oxs_Energy::Init();
}

void Oxs_RandomSiteExchange::FillLinkList
(const Oxs_Mesh* gmesh) const
{
  // Check mesh type
  const Oxs_CommonRectangularMesh* mesh
    = dynamic_cast<const Oxs_CommonRectangularMesh*>(gmesh);
  if(mesh==NULL) {
    String msg =
      String("Import mesh (\"")
      + String(gmesh->InstanceName())
      + String("\") to Oxs_RandomSiteExchange::FillLinkList()"
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

  // Iterate through mesh; at each cell roll iterate through
  // its 3 inferior neighbors.  For each link, roll dice to
  // see if that link gets a non-zero link energy.  If so,
  // add the two associated cells along with a random
  // exchange coefficient A value into the link list.
  links.clear();  // Throw away previous links, if any.

  if(linkprob<=0.) return; // Set no links

  OC_INDEX xdim = mesh->DimX();
  OC_INDEX ydim = mesh->DimY();
  OC_INDEX zdim = mesh->DimZ();
  OC_INDEX xydim = xdim*ydim;
  OC_INDEX xyzdim = xdim*ydim*zdim;
  OC_REAL8m Arange = Amax-Amin;

  OC_REAL8m wgtx = 1/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
  OC_REAL8m wgty = 1/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
  OC_REAL8m wgtz = 1/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

  OC_REAL8m max_Aeff=0.0;
  for(OC_INDEX z=0;z<zdim;z++) {
    for(OC_INDEX y=0;y<ydim;y++) {
      for(OC_INDEX x=0;x<xdim;x++) {
        for(OC_INDEX li=0;li<3;li++) { // Link index: 0=x, 1=y, 2=z
          OC_REAL8m luck=Oc_UnifRand();
          if(luck<=linkprob) {
            // Make this link
            OC_INDEX offset=0;
            OC_REAL8m wgt=0.;
            switch(li) {
              case 0:
                wgt=wgtx;
                if(x+1<xdim)       offset = 1;
                else if(xperiodic) offset = 1 - xdim;
                break;
              case 1:
                wgt=wgty;
                if(y+1<ydim)       offset = xdim;
                else if(yperiodic) offset = xdim - xydim;
                break;
              default:
                wgt=wgtz;
                if(z+1<zdim)       offset = xydim;
                else if(zperiodic) offset = xydim - xyzdim;
                break;
            }
            if(offset!=0) {
              // Link partner is also inside mesh
              Oxs_RandomSiteExchangeLinkParams link;
              link.index1 = mesh->Index(x,y,z); // Get base linear address
              link.index2 = link.index1 + offset;
              link.Acoef = (Amin + Arange*Oc_UnifRand())*wgt;
              if(fabs(link.Acoef)>max_Aeff) max_Aeff = fabs(link.Acoef);
              links.push_back(link);
            }
          }
        }
      }
    }
  }
  energy_density_error_estimate = 16*OC_REAL8m_EPSILON*max_Aeff;
}

void Oxs_RandomSiteExchange::ComputeEnergy
(const Oxs_SimState& state,
 Oxs_ComputeEnergyData& oced
 ) const
{
  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m>& Ms_inverse = *(state.Ms_inverse);

  // If mesh has changed, re-pick link selections
  const Oxs_Mesh* mesh = state.mesh;
  if(mesh_id !=  mesh->Id()) {
    FillLinkList(mesh);
    mesh_id=mesh->Id();
  }
  oced.energy_density_error_estimate = energy_density_error_estimate;

  // Since some cells might not have links, zero non-accum arrays.  Note
  // the Oxs_MeshValue assignment operator runs parallel on threaded
  // builds.
  if(oced.energy) *(oced.energy) = 0.0;
  if(oced.H)           *(oced.H) = Oxs_ThreeVector(0,0,0);
  if(oced.mxH)       *(oced.mxH) = Oxs_ThreeVector(0,0,0);

  // Iterate through link list and accumulate energies and fields
  OC_REAL8m maxdot = 0;
  OC_REAL8m hcoef = 2.0/MU0;
  Oxs_Energy::SUMTYPE esum = 0.0;
  vector<Oxs_RandomSiteExchangeLinkParams>::const_iterator it;
  for(it=links.begin();it!=links.end();++it) {
    OC_INDEX i = it->index1;
    OC_INDEX j = it->index2;
    if(Ms_inverse[i]==0. || Ms_inverse[j]==0.) continue; // Ms=0; skip
    OC_REAL8m acoef = it->Acoef;
    ThreeVector mdiff = spin[i]-spin[j];
    OC_REAL8m dot = mdiff.MagSq();
    if(dot>maxdot) maxdot = dot;
    mdiff *= acoef;
    OC_REAL8m elink = mdiff*spin[i]; // Energy
    esum += elink; // Note that this is half total addition, since elink
                  /// is added to cells i and j.
    mdiff *= hcoef;
    ThreeVector Hi = -1*Ms_inverse[i]*mdiff;
    ThreeVector Hj =    Ms_inverse[j]*mdiff;
    ThreeVector Ti = spin[i]^Hi;
    ThreeVector Tj = spin[j]^Hj;

    // Note that cells may have multiple links, so we add link energy
    // and fields into both accum and non-accum outputs.
    if(oced.energy) {
      (*oced.energy)[i] += elink;
      (*oced.energy)[j] += elink;
    }
    if(oced.energy_accum) {
      (*oced.energy_accum)[i] += elink;
      (*oced.energy_accum)[j] += elink;
    }
    if(oced.H) {
      (*oced.H)[i] += Hi;
      (*oced.H)[j] += Hj;
    }
    if(oced.H_accum) {
      (*oced.H_accum)[i] += Hi;
      (*oced.H_accum)[j] += Hj;
    }
    if(oced.mxH) {
      (*oced.mxH)[i] += Ti;
      (*oced.mxH)[j] += Tj;
    }
    if(oced.mxH_accum) {
      (*oced.mxH_accum)[i] += Ti;
      (*oced.mxH_accum)[j] += Tj;
    }
  }

  oced.energy_sum = esum * (2*state.mesh->Volume(0));
  /// All cells have same volume in an Oxs_RectangularMesh.  Factor "2"
  /// comes because esum includes contribution from only one side of
  /// each link.

  oced.pE_pt = 0.0;

  // Set maxang data
  const OC_REAL8m arg = 0.5*sqrt(maxdot);
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
                     " Oxs_RandomSiteExchange max spin angle set twice.");
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
                  " Oxs_RandomSiteExchange max spin angle set to"
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

void Oxs_RandomSiteExchange::UpdateDerivedOutputs(const Oxs_SimState& state)
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
