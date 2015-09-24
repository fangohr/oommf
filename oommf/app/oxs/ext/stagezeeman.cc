/* FILE: stagezeeman.cc            -*-Mode: c++-*-
 *
 * Uniform Zeeman (applied field) energy, derived from Oxs_Energy class,
 * specified from a Tcl proc.
 *
 */

#include "oc.h"
#include "nb.h"

#include "director.h"
#include "stagezeeman.h"


// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_StageZeeman);

/* End includes */


// Constructor
Oxs_StageZeeman::Oxs_StageZeeman(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr),
    number_of_stages(0), mesh_id(0), stage_valid(0)
{
  working_stage = static_cast<OC_UINT4m>(static_cast<OC_INT4m>(-1));
  /// Safety;  First ChangeFileInitializer() call should be
  /// be triggered by the stage_valid boolean.

  // Process arguments
  hmult = GetRealInitValue("multiplier",1.0);

  OC_BOOL use_file_list = HasInitValue("files");
  OC_BOOL use_script = HasInitValue("script");
  if(use_file_list && use_script) {
    const char *cptr =
      "Initialization string specifies both \"files\""
      " and \"script\".  These are mutually exclusive"
      " options; select only one.";
    throw Oxs_ExtError(this,cptr);
  }
  if(!use_file_list && !use_script) {
    const char *cptr =
      "Initialization string specifies neither \"files\""
      " nor \"script\".  These are mutually exclusive"
      " options, but one or the other must be specified.";
    throw Oxs_ExtError(this,cptr);
  }

  if(use_file_list) {
    // Make certain list contains at least 1 file, because
    // length of filelist is used as a flag to determine
    // whether to call cmd or not.
    GetGroupedStringListInitValue("files",filelist);
    if(filelist.empty()) {
      throw Oxs_ExtError(this,"\"files\" parameter value is empty."
                           " At least one filename is required.");
    }
    // As a failsafe, set up a dummy command that generates
    // a clear error message if in fact cmd is accidentally
    // called.
    String dummy_cmd =
      "error \"Programming error; Oxs_StageZeeman script called"
      " from filelist mode.\" ;# ";
    cmd.SetBaseCommand(InstanceName(),
                       director->GetMifInterp(),
                       dummy_cmd,1);

    number_of_stages 
      = GetUIntInitValue("stage_count",
                         static_cast<OC_UINT4m>(filelist.size()));
    /// Default number_of_stages in this case is the length
    /// of filelist.
  } else {
    cmd.SetBaseCommand(InstanceName(),
                       director->GetMifInterp(),
                       GetStringInitValue("script"),1);
    /// cmd takes 1 integer argument: the current stage.
    /// Return value should be a vector field spec, i.e.,
    /// a vector field reference or an initialization
    /// list.

    number_of_stages = GetUIntInitValue("stage_count",0);
    /// Default number_of_stages in this case is 0, i.e.,
    /// no preference.
  }

  // Initialize outputs.
  Bapp_output.Setup(this,InstanceName(),"B max","mT",1,
                    &Oxs_StageZeeman::Fill__Bapp_output);
  Bappx_output.Setup(this,InstanceName(),"Bx max","mT",1,
                     &Oxs_StageZeeman::Fill__Bapp_output);
  Bappy_output.Setup(this,InstanceName(),"By max","mT",1,
                     &Oxs_StageZeeman::Fill__Bapp_output);
  Bappz_output.Setup(this,InstanceName(),"Bz max","mT",1,
                     &Oxs_StageZeeman::Fill__Bapp_output);

  // Register outputs.
  Bapp_output.Register(director,0);
  Bappx_output.Register(director,0);
  Bappy_output.Register(director,0);
  Bappz_output.Register(director,0);

  VerifyAllInitArgsUsed();
}

Oxs_StageZeeman::~Oxs_StageZeeman()
{}

OC_BOOL Oxs_StageZeeman::Init()
{
  stage_valid = 0;

  mesh_id = 0;
  stagefield.Release();

  // Run parent initializer.
  return Oxs_Energy::Init();
}

void Oxs_StageZeeman::StageRequestCount(unsigned int& min,
                                        unsigned int& max) const
{
  if(number_of_stages==0) {
    min=0; max=UINT_MAX;
  } else {
    min = max = number_of_stages;
  }
}

void
Oxs_StageZeeman::ChangeFieldInitializer
(OC_UINT4m stage,
 const Oxs_Mesh* mesh) const
{ // Setup stagefield_init
  vector<String> params;
  if(filelist.empty()) {
    // Use cmd to generate field initializer
    cmd.SaveInterpResult();
    cmd.SetCommandArg(0,stage);
    cmd.Eval();
    cmd.GetResultList(params);
    cmd.RestoreInterpResult();
  } else {
    // Construct field initializer using Oxs_FileVectorField
    // with filename from filelist and range from mesh.
    OC_UINT4m index = stage;
    OC_UINT4m filecount = static_cast<OC_UINT4m>(filelist.size());
    if(index >= filecount) index = filecount - 1;
    vector<String> options;
    options.push_back(String("file"));
    options.push_back(filelist[index]);

    Oxs_Box bbox;    mesh->GetBoundingBox(bbox);
    char buf[64];
    options.push_back(String("xrange"));
    Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g",
                static_cast<double>(bbox.GetMinX()),
                static_cast<double>(bbox.GetMaxX()));
    options.push_back(String(buf));

    options.push_back(String("yrange"));
    Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g",
                static_cast<double>(bbox.GetMinY()),
                static_cast<double>(bbox.GetMaxY()));
    options.push_back(String(buf));

    options.push_back(String("zrange"));
    Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g",
                static_cast<double>(bbox.GetMinZ()),
                static_cast<double>(bbox.GetMaxZ()));
    options.push_back(String(buf));

    params.push_back(String("Oxs_FileVectorField"));
    params.push_back(Nb_MergeList(options));
  }
  OXS_GET_EXT_OBJECT(params,Oxs_VectorField,stagefield_init);
  working_stage = stage;
  stage_valid = 1;
}

void
Oxs_StageZeeman::FillStageFieldCache(const Oxs_Mesh* mesh) const
{
  max_field.Set(0.,0.,0.);
  stagefield_init->FillMeshValue(mesh,stagefield);
  OC_INDEX size = mesh->Size();
  if(size>0) {
    if(hmult!=1.0) stagefield *= hmult;
    OC_INDEX max_i=0;
    OC_REAL8m max_magsq = stagefield[OC_INDEX(0)].MagSq();
    for(OC_INDEX i=1;i<size;i++) {
      OC_REAL8m magsq = stagefield[i].MagSq();
      if(magsq>max_magsq) {
        max_magsq = magsq;
        max_i = i;
      }
    }
    max_field = stagefield[max_i];
  }
}

void Oxs_StageZeeman::UpdateCache(const Oxs_SimState& state) const
{
  // Update cache as necessary.
  if(!stage_valid || working_stage!=state.stage_number) {
    mesh_id = 0;
    ChangeFieldInitializer(state.stage_number,state.mesh);
    FillStageFieldCache(state.mesh);
    mesh_id = state.mesh->Id();
  } else if(mesh_id != state.mesh->Id()) {
    mesh_id = 0;
    FillStageFieldCache(state.mesh);
    mesh_id = state.mesh->Id();
  }
}

void Oxs_StageZeeman::GetEnergy
(const Oxs_SimState& state,
 Oxs_EnergyData& oed
 ) const
{
  OC_INDEX size = state.mesh->Size();
  if(size<1) return;

  UpdateCache(state);

  if(!stagefield.CheckMesh(state.mesh)) {
    throw Oxs_ExtError(this,"Programming error; stagefield size"
                         " incompatible with mesh.");
  }

  // Use supplied buffer space, and reflect that use in oed.
  oed.energy = oed.energy_buffer;
  oed.field = oed.field_buffer;
  Oxs_MeshValue<OC_REAL8m>& energy = *oed.energy_buffer;
  Oxs_MeshValue<ThreeVector>& field = *oed.field_buffer;
  energy.AdjustSize(state.mesh);
  field.AdjustSize(state.mesh);

  field = stagefield; // Copy field

  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);
  OC_INDEX i=0;
  do { // Calculate energy density
    energy[i] = -MU0 * Ms[i] * (spin[i]*field[i]);
    ++i;
  } while(i<size);
}

void
Oxs_StageZeeman::Fill__Bapp_output(const Oxs_SimState& state)
{
  UpdateCache(state);

  ThreeVector B = max_field;
  B *= MU0 * 1000; /// Report Bapp in mT

  if(Bapp_output.GetCacheRequestCount()>0) {
    Bapp_output.cache.state_id=0;
    Bapp_output.cache.value = sqrt(B.MagSq());
    Bapp_output.cache.state_id=state.Id();
  }

  if(Bappx_output.GetCacheRequestCount()>0) {
    Bappx_output.cache.state_id=0;
    Bappx_output.cache.value = B.x;
    Bappx_output.cache.state_id=state.Id();
  }

  if(Bappy_output.GetCacheRequestCount()>0) {
    Bappy_output.cache.state_id=0;
    Bappy_output.cache.value = B.y;
    Bappy_output.cache.state_id=state.Id();
  }

  if(Bappz_output.GetCacheRequestCount()>0) {
    Bappz_output.cache.state_id=0;
    Bappz_output.cache.value = B.z;
    Bappz_output.cache.state_id=state.Id();
  }

}
