/* FILE: transformzeeman.cc         -*-Mode: c++-*-
 *
 * Zeeman (applied field) energy, derived from Oxs_Energy class,
 * specified from a Tcl proc.  This is a generalization of the
 * Oxs_ScriptUZeeman and Oxs_FixedZeeman classes.
 *
 */

#include <limits>
#include <string>

#include "oc.h"
#include "nb.h"
#include "transformzeeman.h"

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_TransformZeeman);

/* End includes */


// Constructor
Oxs_TransformZeeman::Oxs_TransformZeeman(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_ChunkEnergy(name,newdtr,argstr),
    hmult(1.0), number_of_stages(0),
    transform_type(identity), mesh_id(0),max_fixedfield(-1),
    tfrm_row1(0,0,0),tfrm_row2(0,0,0),tfrm_row3(0,0,0),
    Dtfrm_row1(0,0,0),Dtfrm_row2(0,0,0),Dtfrm_row3(0,0,0)
{
  // Process arguments
  hmult = GetRealInitValue("multiplier",1.0);
  number_of_stages = GetUIntInitValue("stage_count",0);
  /// Default number_of_stages is 0, i.e., no preference.

  String xform_type_string = GetStringInitValue("type","identity");
  if(xform_type_string.compare("identity")==0) {
    transform_type = identity;
  } else if(xform_type_string.compare("diagonal")==0) {
    transform_type = diagonal;
  } else if(xform_type_string.compare("symmetric")==0) {
    transform_type = symmetric;
  } else if(xform_type_string.compare("general")==0) {
    transform_type = general;
  } else {
    String msg = String("Invalid transform type: \"")
      + xform_type_string
      + String("\".  Should be identity, diagonal, symmetric or general.");
    throw Oxs_ExtError(this,msg.c_str());
  }

  String cmdoptreq;
  String runscript;
  if(transform_type != identity) {
    cmdoptreq = GetStringInitValue("script_args",
                                   "stage stage_time total_time");
    runscript = GetStringInitValue("script");
  }

  OXS_GET_INIT_EXT_OBJECT("field",Oxs_VectorField,fixedfield_init);

  VerifyAllInitArgsUsed();

  // Run SetBaseCommand *after* VerifyAllInitArgsUsed(); this produces
  // a more intelligible error message in case a command line argument
  // error is really due to a misspelled parameter label.
  if(transform_type != identity) {
    command_options.push_back(Nb_TclCommandLineOption("stage",1));
    command_options.push_back(Nb_TclCommandLineOption("stage_time",1));
    command_options.push_back(Nb_TclCommandLineOption("total_time",1));
    cmd.SetBaseCommand(InstanceName(),
		       director->GetMifInterp(),
		       runscript,
		       Nb_ParseTclCommandLineRequest(InstanceName(),
                                                     command_options,
                                                     cmdoptreq));
  }

}

Oxs_TransformZeeman::~Oxs_TransformZeeman()
{}

OC_BOOL Oxs_TransformZeeman::Init()
{
  mesh_id = 0;
  fixedfield.Release();
  max_fixedfield = -1;
  tfrm_row1.Set(0,0,0);
  tfrm_row2.Set(0,0,0);
  tfrm_row3.Set(0,0,0);
  Dtfrm_row1.Set(0,0,0);
  Dtfrm_row2.Set(0,0,0);
  Dtfrm_row3.Set(0,0,0);
  return Oxs_Energy::Init();
}

void
Oxs_TransformZeeman::StageRequestCount
(unsigned int& min,
 unsigned int& max) const
{
  if(number_of_stages==0) {
    min=0; max=UINT_MAX;
  } else {
    min = max = number_of_stages;
  }
}

void
Oxs_TransformZeeman::GetAppliedField
(const Oxs_SimState& state,
 ThreeVector& row1, ThreeVector& row2, ThreeVector& row3,
 ThreeVector& drow1, ThreeVector& drow2, ThreeVector& drow3
) const
{
  // Special handling for identity transform (no script)
  if(transform_type == identity) {
    row1.Set(1,0,0); row2.Set(0,1,0); row3.Set(0,0,1);
    drow1.Set(0,0,0); drow2.Set(0,0,0); drow3.Set(0,0,0);
    return;
  }

  // Run script
  cmd.SaveInterpResult();

  int index;
  if((index = command_options[0].position)>=0) { // stage
    cmd.SetCommandArg(index,state.stage_number);
  }
  if((index = command_options[1].position)>=0) { // stage_time
    cmd.SetCommandArg(index,state.stage_elapsed_time);
  }
  if((index = command_options[2].position)>=0) { // total_time
    cmd.SetCommandArg(index,
		      state.stage_start_time+state.stage_elapsed_time);
  }

  cmd.Eval();

  if(cmd.GetResultListSize()>18) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
		"Script return list too large: %d values",
		cmd.GetResultListSize());
    cmd.DiscardInterpResult();
    throw Oxs_ExtError(this,buf);
  }

  // Convert script string values to OC_REAL8m's.
  vector<OC_REAL8m> srv; // "Script Return Value"
  for(int i=0;i<cmd.GetResultListSize();i++) {
    OC_REAL8m value;
    cmd.GetResultListItem(i,value);
    srv.push_back(value);
  }

  // Fill row vectors
  switch(transform_type) {
  case diagonal:
    if(srv.size()!=6) {
      String msg
	= String("Error detected during field evaluation.")
	+ String(" Script return is not a 6-tuple: ")
	+ cmd.GetWholeResult();
      cmd.DiscardInterpResult();
      throw Oxs_ExtError(this,msg.c_str());
    }
    row1.Set(srv[0],0,0);
    row2.Set(0,srv[1],0);
    row3.Set(0,0,srv[2]);
    drow1.Set(srv[3],0,0);
    drow2.Set(0,srv[4],0);
    drow3.Set(0,0,srv[5]);
    break;
  case symmetric:
    if(srv.size()!=12) {
      String msg
	= String("Error detected during field evaluation.")
	+ String(" Script return is not a 12-tuple: ")
	+ cmd.GetWholeResult();
      cmd.DiscardInterpResult();
      throw Oxs_ExtError(this,msg.c_str());
    }
    row1.Set(srv[0],srv[1],srv[2]);
    row2.Set(srv[1],srv[3],srv[4]);
    row3.Set(srv[2],srv[4],srv[5]);
    drow1.Set(srv[6],srv[7],srv[8]);
    drow2.Set(srv[7],srv[9],srv[10]);
    drow3.Set(srv[8],srv[10],srv[11]);
    break;
  case general:
    if(srv.size()!=18) {
      String msg
	= String("Error detected during field evaluation.")
	+ String(" Script return is not a 18-tuple: ")
	+ cmd.GetWholeResult();
      cmd.DiscardInterpResult();
      throw Oxs_ExtError(this,msg.c_str());
    }
    row1.Set(srv[0],srv[1],srv[2]);
    row2.Set(srv[3],srv[4],srv[5]);
    row3.Set(srv[6],srv[7],srv[8]);
    drow1.Set(srv[9],srv[10],srv[11]);
    drow2.Set(srv[12],srv[13],srv[14]);
    drow3.Set(srv[15],srv[16],srv[17]);
    break;
  default:
    cmd.DiscardInterpResult();
    throw Oxs_ExtError(this,"Programming error; invalid transform type");
  }
  cmd.RestoreInterpResult();
}

void Oxs_TransformZeeman::ComputeEnergyChunkInitialize
(const Oxs_SimState& state,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oc_AlignedVector<Oxs_ComputeEnergyDataThreadedAux>& /* thread_ocedtaux */,
 int number_of_threads) const
{
  if(mesh_id != state.mesh->Id()) {
    // This is either the first pass through, or else mesh
    // has changed.
    mesh_id = 0;
    fixedfield_init->FillMeshValue(state.mesh,fixedfield);
    if(hmult!=1.0) {
      fixedfield *= hmult; // On threaded builds the multiplication
      /// will run in parallel
    }
    // Find max fixedfield value
    std::vector<OC_REAL8m> thread_maxvalsq(number_of_threads,0);
    Oxs_RunThreaded<ThreeVector,
                    std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (fixedfield,
       [&](OC_INT4m thread_id,OC_INDEX istart,OC_INDEX istop) {
        OC_REAL8m thd_maxvalsq = thread_maxvalsq[thread_id]; // Thread local copy
        for(OC_INDEX i=istart;i<istop;++i) {
          OC_REAL8m magsq = fixedfield[i].MagSq();
          if(magsq>thd_maxvalsq) thd_maxvalsq=magsq;
        }
        thread_maxvalsq[thread_id]    = thd_maxvalsq;
      });
    for(int i=1;i<number_of_threads;++i) {
      if(thread_maxvalsq[0]<thread_maxvalsq[i]) {
        thread_maxvalsq[0] = thread_maxvalsq[i];
      }
    }
    max_fixedfield = sqrt(thread_maxvalsq[0]);
    mesh_id = state.mesh->Id();
  }
  OC_REAL8m Tmax=1;
  if(transform_type != identity) {
    GetAppliedField(state,tfrm_row1,tfrm_row2,tfrm_row3,
                    Dtfrm_row1,Dtfrm_row2,Dtfrm_row3);
    Tmax= tfrm_row1.MagSq();
    OC_REAL8m tmp2 = tfrm_row2.MagSq();
    if(tmp2>Tmax) Tmax=tmp2;
    OC_REAL8m tmp3 = tfrm_row3.MagSq();
    if(tmp3>Tmax) Tmax=tmp3;
    Tmax = sqrt(Tmax);
  }

  ocedt.energy_density_error_estimate
    = 16*OC_REAL8m_EPSILON*MU0*state.max_absMs*max_fixedfield*Tmax;

}

void Oxs_TransformZeeman::ComputeEnergyChunk
(const Oxs_SimState& state,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
 OC_INDEX node_start,OC_INDEX node_stop,
 int /* threadnumber */) const
{
  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);
  const Oxs_Mesh* mesh = state.mesh;

  Oxs_Energy::SUMTYPE thd_energy=0.0;
  Oxs_Energy::SUMTYPE thd_pE_pt=0.0;

  if(transform_type == identity) {
    for(OC_INDEX i=node_start;i<node_stop;++i) {
      const ThreeVector& H = fixedfield[i];
      OC_REAL8m ei = (-MU0*Ms[i])*(H*spin[i]);
      ThreeVector mxH = spin[i]^H;
      if(ocedt.H)                       (*ocedt.H)[i]  = H;
      if(ocedt.H_accum)           (*ocedt.H_accum)[i] += H;
      if(ocedt.energy)             (*ocedt.energy)[i]  = ei;
      if(ocedt.energy_accum) (*ocedt.energy_accum)[i] += ei;
      if(ocedt.mxH)                   (*ocedt.mxH)[i]  = mxH;
      if(ocedt.mxH_accum)       (*ocedt.mxH_accum)[i] += mxH;
      thd_energy += mesh->Volume(i)*ei;
    }
  } else {
    for(OC_INDEX i=node_start;i<node_stop;++i) {
      OC_REAL8m vol = mesh->Volume(i);
      const ThreeVector& v = fixedfield[i];
      const ThreeVector H(tfrm_row1*v,
                          tfrm_row2*v,
                          tfrm_row3*v); // Apply transform
      const ThreeVector dH(Dtfrm_row1*v,
                           Dtfrm_row2*v,
                           Dtfrm_row3*v); // dH/dt
      ThreeVector temp = (-MU0*Ms[i])*spin[i];
      ThreeVector mxH = spin[i]^H;
      OC_REAL8m ei = H*temp;

      if(ocedt.H)                       (*ocedt.H)[i]  = H;
      if(ocedt.H_accum)           (*ocedt.H_accum)[i] += H;
      if(ocedt.energy)             (*ocedt.energy)[i]  = ei;
      if(ocedt.energy_accum) (*ocedt.energy_accum)[i] += ei;
      if(ocedt.mxH)                   (*ocedt.mxH)[i]  = mxH;
      if(ocedt.mxH_accum)       (*ocedt.mxH_accum)[i] += mxH;

      thd_energy += vol*ei;
      thd_pE_pt += vol*(dH*temp);
    }
  }

  ocedtaux.energy_total_accum += thd_energy;
  ocedtaux.pE_pt_accum += thd_pE_pt;
}
