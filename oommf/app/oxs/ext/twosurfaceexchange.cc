/* FILE: twosurfaceexchange.cc            -*-Mode: c++-*-
 *
 * Long range (RKKY-style) exchange energy on links across spacer
 * layer, with surface (interfacial) exchange energy coefficents
 * sigma (bilinear) and sigma2 (biquadratic).
 *
 */

#include "nb.h"
#include "director.h"
#include "mesh.h"
#include "meshvalue.h"
#include "oxswarn.h"
#include "simstate.h"
#include "threevector.h"
#include "rectangularmesh.h"
#include "twosurfaceexchange.h"
#include "energy.h"             // Needed to make MSVC++ 5 happy

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_TwoSurfaceExchange);

/* End includes */

// Revision information, set via CVS keyword substitution
static const Oxs_WarningMessageRevisionInfo revision_info
  (__FILE__,
   "$Revision: 1.25 $",
   "$Date: 2013/05/22 07:15:25 $",
   "$Author: donahue $",
   "Michael J. Donahue (michael.donahue@nist.gov)");


// Constructor
Oxs_TwoSurfaceExchange::Oxs_TwoSurfaceExchange(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr),
    mesh_id(0), energy_density_error_estimate(-1),
    report_max_spin_angle(0)
{
  // Process arguments
  init_sigma = GetRealInitValue("sigma",0.);
  init_sigma2 = GetRealInitValue("sigma2",0.);

  // First boundary.  Import parameter string should be a 10 element
  // list of name-value pairs:
  //         NAME         VALUE
  //         atlas        atlas reference
  //         region       region name
  //         scalarfield  scalar field reference
  //         scalarvalue  scalar field level surface reference value
  //         scalarside   one of <, <=, >=, or > denoting inner surface
  //                      side; the values - and + (equivalent to <= and
  //                      >= respectively) are accepted for backwards
  //                      compatibility.
  CheckInitValueParamCount("surface1",10);
  vector<String> surface1;
  FindRequiredInitValue("surface1",surface1);
  OC_BOOL badinput1=0;
  String error1;
  if(surface1[0].compare("atlas")!=0) {
    badinput1 = 1;
    error1 = String("Bad name \"") + surface1[0]
      + String("\"; should be \"atlas\"");
  } else if(surface1[2].compare("region")      != 0) {
    badinput1 = 1;
    error1 = String("Bad name \"") + surface1[2]
      + String("\"; should be \"region\"");
  } else if(surface1[4].compare("scalarfield") != 0) {
    badinput1 = 1;
    error1 = String("Bad name \"") + surface1[4]
      + String("\"; should be \"scalarfield\"");
  } else if(surface1[6].compare("scalarvalue") != 0) {
    badinput1 = 1;
    error1 = String("Bad name \"") + surface1[6]
      + String("\"; should be \"scalarvalue\"");
  } else if(surface1[8].compare("scalarside")  != 0) {
    badinput1 = 1;
    error1 = String("Bad name \"") + surface1[8]
      + String("\"; should be \"scalarside\"");
  } else {
    Nb_SplitList param_strings;
    vector<String> params;
    param_strings.Split(surface1[1].c_str());
    param_strings.FillParams(params);
    OXS_GET_EXT_OBJECT(params,Oxs_Atlas,atlas1);
    region1 = surface1[3];
    param_strings.Split(surface1[5].c_str());
    param_strings.FillParams(params);
    OXS_GET_EXT_OBJECT(params,Oxs_ScalarField,bdry1);
    bdry1_value = Nb_Atof(surface1[7].c_str());
    bdry1_side = surface1[9];
    if(atlas1->GetRegionId(region1) == -1) {
      badinput1 = 1;
      error1 = String("Unrecognized region \"")
        + region1 + String("\"");
    } else if(bdry1_side.compare("<")!=0 && bdry1_side.compare("<=")!=0
           && bdry1_side.compare(">")!=0 && bdry1_side.compare(">=")!=0
           && bdry1_side.compare("+")!=0 && bdry1_side.compare("-")!=0) {
      badinput1 = 1;
      error1 = String("Bad scalarside string \"")
        + bdry1_side + String("\"; should be one of <, <=, >=, >");
    }
  }
  DeleteInitValue("surface1");

  // Second boundary.  Import parameter string same format
  // as for first boundary.
  CheckInitValueParamCount("surface2",10);
  vector<String> surface2;
  FindRequiredInitValue("surface2",surface2);
  OC_BOOL badinput2=0;
  String error2;
  if(surface2[0].compare("atlas")!=0) {
    badinput2 = 1;
    error2 = String("Bad name \"") + surface2[0]
      + String("\"; should be \"atlas\"");
  } else if(surface2[2].compare("region")      != 0) {
    badinput2 = 1;
    error2 = String("Bad name \"") + surface2[2]
      + String("\"; should be \"region\"");
  } else if(surface2[4].compare("scalarfield") != 0) {
    badinput2 = 1;
    error2 = String("Bad name \"") + surface2[4]
      + String("\"; should be \"scalarfield\"");
  } else if(surface2[6].compare("scalarvalue") != 0) {
    badinput2 = 1;
    error2 = String("Bad name \"") + surface2[6]
      + String("\"; should be \"scalarvalue\"");
  } else if(surface2[8].compare("scalarside")  != 0) {
    badinput2 = 1;
    error2 = String("Bad name \"") + surface2[8]
      + String("\"; should be \"scalarside\"");
  } else {
    Nb_SplitList param_strings;
    vector<String> params;
    param_strings.Split(surface2[1].c_str());
    param_strings.FillParams(params);
    OXS_GET_EXT_OBJECT(params,Oxs_Atlas,atlas2);
    region2 = surface2[3];
    param_strings.Split(surface2[5].c_str());
    param_strings.FillParams(params);
    OXS_GET_EXT_OBJECT(params,Oxs_ScalarField,bdry2);
    bdry2_value = Nb_Atof(surface2[7].c_str());
    bdry2_side = surface2[9];
    if(atlas2->GetRegionId(region2) == -1) {
      badinput2 = 1;
      error2 = String("Unrecognized region \"")
        + region2 + String("\"");
    } else if(bdry2_side.compare("<")!=0 && bdry2_side.compare("<=")!=0
           && bdry2_side.compare(">")!=0 && bdry2_side.compare(">=")!=0
           && bdry2_side.compare("+")!=0 && bdry2_side.compare("-")!=0) {
      badinput2 = 1;
      error2 = String("Bad scalarside string \"")
        + bdry2_side + String("\"; should be one of <, <=, >=, >");
    }
  }
  DeleteInitValue("surface2");

  if(badinput1 || badinput2) {
    String msg=
      String("Bad surface parameter block, inside "
             "Oxs_TwoSurfaceExchange mif Specify block for object ")
      +  String(InstanceName()) + String(":");
    if(badinput1) msg += String(" surface1 block is bad: ") + error1;
    if(badinput1 && badinput2) msg += String(", ");
    if(badinput2) msg += String(" surface2 block is bad: ") + error2;
    msg += String(".");
    throw Oxs_ExtError(msg.c_str());
  }

  // Report max spin angle?  Generally this value is used to detect
  // grids that are too coarse, which is not applicable in this instance
  // because the connected spins are assumed to be spatially separated.
  // This output is provided in case it proves useful, but is disabled
  // by default.
  if(GetIntInitValue("report_max_spin_angle",0)) {
    report_max_spin_angle = 1;
  } else {
    report_max_spin_angle = 0;
  }

  VerifyAllInitArgsUsed();

  // Setup outputs
  if(report_max_spin_angle) {
    maxspinangle_output.Setup(this,InstanceName(),"Max Spin Ang","deg",1,
                              &Oxs_TwoSurfaceExchange::UpdateDerivedOutputs);
    maxspinangle_output.Register(director,0);
    stage_maxspinangle_output.Setup(this,InstanceName(),
                           "Stage Max Spin Ang","deg",1,
                           &Oxs_TwoSurfaceExchange::UpdateDerivedOutputs);
    stage_maxspinangle_output.Register(director,0);
    run_maxspinangle_output.Setup(this,InstanceName(),
                           "Run Max Spin Ang","deg",1,
                           &Oxs_TwoSurfaceExchange::UpdateDerivedOutputs);
    run_maxspinangle_output.Register(director,0);
  }
}

Oxs_TwoSurfaceExchange::~Oxs_TwoSurfaceExchange()
{}

OC_BOOL Oxs_TwoSurfaceExchange::Init()
{
  mesh_id = 0;
  energy_density_error_estimate = -1;
  links.clear();
  return Oxs_Energy::Init();
}

void Oxs_TwoSurfaceExchange::FillLinkList
(const Oxs_Mesh* gmesh) const
{ // Queries the mesh with each surface spec,
  // and then pairs up cells between surfaces.
  // Each cell on the first surface is paired up with
  // the closest cell from the second surface.  Note
  // the asymmetry in this pairing procedure---generally,
  // one will probably want to make the smaller surface
  // the "first" surface.  Consider these pictures:
  //
  //          22222222222222 <-- Surface 2
  //              ||||||           
  //              ||||||  <-- Links
  //              ||||||         
  //              111111  <-- Surface 1
  //
  //  as compared to
  //
  //          11111111111111 <-- Surface 1
  //           \\\||||||///        
  //            \\||||||// <-- Links
  //             \||||||/        
  //              222222  <-- Surface 2
  //
  // In the second picture, all the outer cells in the top
  // surface get paired up with the outer elements in the
  // lower surface.

  // Check mesh type
  const Oxs_CommonRectangularMesh* mesh
    = dynamic_cast<const Oxs_CommonRectangularMesh*>(gmesh);
  if(mesh==NULL) {
    String msg =
      String("Import mesh (\"")
      + String(gmesh->InstanceName())
      + String("\") to Oxs_TwoSurfaceExchange::FillLinkList()"
             " routine of object \"") + String(InstanceName())
      + String("\" is not a rectangular mesh object.");
    throw Oxs_ExtError(msg.c_str());
  }

  // If periodic, collect data for distance determination
  OC_REAL8m xpbcadj=0.0, ypbcadj=0.0, zpbcadj=0.0;
  const Oxs_PeriodicRectangularMesh* pmesh
    = dynamic_cast<const Oxs_PeriodicRectangularMesh*>(mesh);
  if(pmesh!=NULL) {
    Oxs_Box bbox;
    mesh->GetBoundingBox(bbox);
    if(pmesh->IsPeriodicX()) xpbcadj = (bbox.GetMaxX()-bbox.GetMinX())/2.0;
    if(pmesh->IsPeriodicY()) ypbcadj = (bbox.GetMaxY()-bbox.GetMinY())/2.0;
    if(pmesh->IsPeriodicZ()) zpbcadj = (bbox.GetMaxZ()-bbox.GetMinZ())/2.0;
  }

  // Get cell lists for each surface
  vector<OC_INDEX> bdry1_cells,bdry2_cells;
  mesh->BoundaryList(*atlas1,region1,*bdry1,bdry1_value,bdry1_side,
                     bdry1_cells);
  mesh->BoundaryList(*atlas2,region2,*bdry2,bdry2_value,bdry2_side,
                     bdry2_cells);
  if(bdry1_cells.empty() || bdry2_cells.empty()) {
    String msg =
      String("Empty bdry/surface list with current mesh"
             " detected in Oxs_TwoSurfaceExchange::FillLinkList()"
             " routine of object ") + String(InstanceName());
    throw Oxs_ExtError(msg.c_str());
  }

  // For each cell in bdry1_cells, find closest cell in bdry2_cells,
  // and store this pair in "links".
  OC_REAL8m max_Awork = 0.0;
  links.clear();  // Throw away previous links, if any.
  vector<OC_INDEX>::iterator it1 = bdry1_cells.begin();
  while(it1!=bdry1_cells.end()) {
    ThreeVector cell1;
    mesh->Center(*it1,cell1);
    vector<OC_INDEX>::iterator it2 = bdry2_cells.begin();
    ThreeVector vtemp;
    mesh->Center(*it2,vtemp);
    vtemp -= cell1;
    if(xpbcadj!=0.0 && fabs(vtemp.x)>xpbcadj) {
      vtemp.x = fabs(vtemp.x) - xpbcadj;
    }
    if(ypbcadj!=0.0 && fabs(vtemp.y)>ypbcadj) {
      vtemp.y = fabs(vtemp.y) - ypbcadj;
    }
    if(zpbcadj!=0.0 && fabs(vtemp.z)>zpbcadj) {
      vtemp.z = fabs(vtemp.z) - zpbcadj;
    }
    OC_REAL8m min_distsq = vtemp.MagSq();
    OC_INDEX min_index = *it2;
    while( (++it2) != bdry2_cells.end()) {
      mesh->Center(*it2,vtemp);
      vtemp -= cell1;
      if(xpbcadj!=0.0 && fabs(vtemp.x)>xpbcadj) {
        vtemp.x = fabs(vtemp.x) - xpbcadj;
      }
      if(ypbcadj!=0.0 && fabs(vtemp.y)>ypbcadj) {
        vtemp.y = fabs(vtemp.y) - ypbcadj;
      }
      if(zpbcadj!=0.0 && fabs(vtemp.z)>zpbcadj) {
        vtemp.z = fabs(vtemp.z) - zpbcadj;
      }
      OC_REAL8m distsq = vtemp.MagSq();
      if(distsq<min_distsq) {
        min_distsq = distsq;
        min_index = *it2;
      }
    }
    if(min_distsq>0.) {
      // The sigma coef's need to be weighted by the inverse
      // of the thickness of the compuational cell in the
      // direction of the link.  This is necessary so that
      // the total surface (interfacial) exchange energy is
      // independent of the discretization size, as the
      // discretization size is reduces to 0.  (As the
      // computational cell size is reduced, the surface
      // energy is squeezed into an ever thinner layer of
      // cells along the surface.)
      ThreeVector wtemp;
      mesh->Center(min_index,wtemp);
      wtemp -= cell1;
      OC_REAL8m cellwgt = fabs(wtemp.x*mesh->EdgeLengthX())
                        + fabs(wtemp.y*mesh->EdgeLengthY())
                        + fabs(wtemp.z*mesh->EdgeLengthZ());
      if(cellwgt>0.) { // Safety
        cellwgt = sqrt(min_distsq)/cellwgt;
        // min_distsq _should_ be wtemp.MagSq(), so
        // cellwgt is |wtemp|/<wtemp,celldims>, i.e.,
        // < wtemp/|wtemp| , celldims >^{-1}
        // This should work properly if the two surfaces are
        // parallel to each other and also to one of the
        // coordinate planes.  This probably needs to be beefed
        // up to handle more general cases.
      }

      Oxs_TwoSurfaceExchangeLinkParams ltemp;
      ltemp.index1 = *it1;    ltemp.index2 = min_index;
      ltemp.exch_coef1 = (init_sigma + 2*init_sigma2) * cellwgt;
      ltemp.exch_coef2 = -init_sigma2 * cellwgt;
      // NB: The init_sigma and init_sigma2 references above
      //  will likely be replaced in the future with a call
      //  to a user-defined function taking as imports the
      //  center locations of the two cells in the link pair.

      links.push_back(ltemp);

      OC_REAL8m Atest = 2*fabs(ltemp.exch_coef1)
        + 4*fabs(ltemp.exch_coef2);
      if(Atest>max_Awork) max_Awork = Atest;
    }
    ++it1;
  }
  energy_density_error_estimate = 16*OC_REAL8m_EPSILON*max_Awork;
}

void Oxs_TwoSurfaceExchange::ComputeEnergy
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
  vector<Oxs_TwoSurfaceExchangeLinkParams>::const_iterator it
    = links.begin();
  for(it=links.begin();it!=links.end();++it) {
    OC_INDEX i = it->index1;
    OC_INDEX j = it->index2;
    if(Ms_inverse[i]==0. || Ms_inverse[j]==0.) continue; // Ms=0; skip
    OC_REAL8m ecoef1 = it->exch_coef1;
    OC_REAL8m ecoef2 = it->exch_coef2;
    ThreeVector mdiff = spin[i]-spin[j];
    OC_REAL8m dot = mdiff.MagSq();
    if(dot>maxdot) maxdot = dot;
    OC_REAL8m temp = 0.5*mdiff.MagSq(); // == (1-mi*mj)
    OC_REAL8m elink = (ecoef1 + ecoef2*temp)*temp; // Energy density
    /// elink works out to
    ///    [sigma*(1-mi*mj)+sigma2*(1-mi*mj)*(1+mi*mj)]/cellsize
    esum += elink; // Note that this is half total addition, since elink
                  /// is added to cells i and j.
    mdiff *= hcoef*(ecoef1+2*ecoef2*temp);

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
                     " Oxs_TwoSurfaceExchange max spin angle set twice.");
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
                  " Oxs_TwoSurfaceExchange max spin angle set to"
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

void Oxs_TwoSurfaceExchange::UpdateDerivedOutputs(const Oxs_SimState& state)
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
