/* FILE: yy_stagemel.cc                 -*-Mode: c++-*-
 *
 * OOMMF magnetoelastic coupling extension module.
 * YY_StageMEL class.
 * Calculates MEL field/energy for each stage.
 * 
 * Release ver. 1.0.1 (2015-03-03)
 * 
 */

#include <string>
#include <vector>

#include "oc.h"
#include "nb.h"
#include "director.h"
#include "mesh.h"
#include "meshvalue.h"
#include "simstate.h"
#include "threevector.h"
#include "energy.h"             // Needed to make MSVC++ 5 happy

#include "yy_stagemel.h"

OC_USE_STRING;

/* End includes */

// Oxs_Ext registration support
OXS_EXT_REGISTER(YY_StageMEL);

// Constructor
YY_StageMEL::YY_StageMEL(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr), number_of_stages(0),
  mesh_id(0), stage_valid(0)
{
  working_stage = static_cast<OC_UINT4m>(static_cast<OC_INT4m>(-1));
  // First ChangeFileInitializer() call should be triggered
  // by the stage_valid boolean.

  // Process arguments
  hmult = GetRealInitValue("multiplier", 1.0);

  // Generate MELCoef initializer
  OXS_GET_INIT_EXT_OBJECT("B1",Oxs_ScalarField,MELCoef1_init);
  OXS_GET_INIT_EXT_OBJECT("B2",Oxs_ScalarField,MELCoef2_init);

  // set use_u_filelist or use_e_filelist
  SelectElasticityInputType();

  // Initialize outputs.
  B_MEL_output.Setup(this, InstanceName(), "B max", "mT", 1,
    &YY_StageMEL::Fill__B_MEL_output);
  B_MELx_output.Setup(this, InstanceName(), "Bx max", "mT", 1,
    &YY_StageMEL::Fill__B_MEL_output);
  B_MELy_output.Setup(this, InstanceName(), "By max", "mT", 1,
    &YY_StageMEL::Fill__B_MEL_output);
  B_MELz_output.Setup(this, InstanceName(), "Bz max", "mT", 1,
    &YY_StageMEL::Fill__B_MEL_output);

  // Register outputs.
  B_MEL_output.Register(director, 0);
  B_MELx_output.Register(director, 0);
  B_MELy_output.Register(director, 0);
  B_MELz_output.Register(director, 0);

  VerifyAllInitArgsUsed();
}

YY_StageMEL::~YY_StageMEL()
{}

OC_BOOL YY_StageMEL::Init()
{
  stage_valid = 0;

  mesh_id = 0;
  MELField.Release();

  return Oxs_Energy::Init();
}

void YY_StageMEL::StageRequestCount(unsigned int& min,
    unsigned int& max) const
{
  if(number_of_stages == 0) {
    min = 0; max = UINT_MAX;  // No restriction on stage count
  } else {
    min = max = number_of_stages;
  }
}

void YY_StageMEL::ChangeDisplacementInitializer(
    OC_UINT4m stage, const Oxs_Mesh* mesh) const
{
  // Setup displacement
  vector<String> params;
  if(u_filelist.empty()) {
    // Use u_cmd to generate field initializer
    u_cmd.SaveInterpResult();
    u_cmd.SetCommandArg(0,stage);
    u_cmd.Eval();
    u_cmd.GetResultList(params);
    u_cmd.RestoreInterpResult();
  } else {
    // Construct field initializer using Oxs_FileVectorField
    // with filename from u_filelist and range from mesh.
    OC_UINT4m index = stage;
    OC_UINT4m filecount = static_cast<OC_UINT4m>(u_filelist.size());
    if(index >= filecount) index = filecount - 1;
    vector<String> options;
    options.push_back(String("file"));
    options.push_back(u_filelist[index]);

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

  OXS_GET_EXT_OBJECT(params,Oxs_VectorField,u_init);
  working_stage = stage;
  stage_valid = 1;
}

void YY_StageMEL::ChangeStrainInitializer(
    OC_UINT4m stage, const Oxs_Mesh* mesh) const
{
  // Setup strain
  vector<String> params_diag, params_offdiag;
  if( use_e_script ) {
    // Use e_*diag_cmd to generate field initializer
    e_diag_cmd.SaveInterpResult();
    e_diag_cmd.SetCommandArg(0,stage);
    e_diag_cmd.Eval();
    e_diag_cmd.GetResultList(params_diag);
    e_diag_cmd.RestoreInterpResult();
    e_offdiag_cmd.SaveInterpResult();
    e_offdiag_cmd.SetCommandArg(0,stage);
    e_offdiag_cmd.Eval();
    e_offdiag_cmd.GetResultList(params_offdiag);
    e_offdiag_cmd.RestoreInterpResult();
  } else {  // use_e_filelist
    // Construct field initializer using Oxs_FileVectorField
    // with filename from e_diag_filelist and range from mesh.
    OC_UINT4m index = stage;
    OC_UINT4m filecount = static_cast<OC_UINT4m>(e_diag_filelist.size());
    if(index >= filecount) index = filecount - 1;
    vector<String> options_diag, options_offdiag;
    Oxs_Box bbox;    mesh->GetBoundingBox(bbox);
    char buf[64];

    // Diagonal elements
    params_diag.push_back(String("Oxs_FileVectorField"));
    options_diag.push_back(String("xrange"));
    Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g",
                static_cast<double>(bbox.GetMinX()),
                static_cast<double>(bbox.GetMaxX()));
    options_diag.push_back(String(buf));
    options_diag.push_back(String("yrange"));
    Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g",
                static_cast<double>(bbox.GetMinY()),
                static_cast<double>(bbox.GetMaxY()));
    options_diag.push_back(String(buf));
    options_diag.push_back(String("zrange"));
    Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g",
                static_cast<double>(bbox.GetMinZ()),
                static_cast<double>(bbox.GetMaxZ()));
    options_diag.push_back(String(buf));
    options_diag.push_back(String("file"));
    options_diag.push_back(e_diag_filelist[index]);

    params_diag.push_back(Nb_MergeList(options_diag));

    // Off-diagonal elements
    params_offdiag.push_back(String("Oxs_FileVectorField"));
    options_offdiag.push_back(String("xrange"));
    Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g",
                static_cast<double>(bbox.GetMinX()),
                static_cast<double>(bbox.GetMaxX()));
    options_offdiag.push_back(String(buf));
    options_offdiag.push_back(String("yrange"));
    Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g",
                static_cast<double>(bbox.GetMinY()),
                static_cast<double>(bbox.GetMaxY()));
    options_offdiag.push_back(String(buf));
    options_offdiag.push_back(String("zrange"));
    Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g",
                static_cast<double>(bbox.GetMinZ()),
                static_cast<double>(bbox.GetMaxZ()));
    options_offdiag.push_back(String(buf));
    options_offdiag.push_back(String("file"));
    options_offdiag.push_back(e_offdiag_filelist[index]);

    params_offdiag.push_back(Nb_MergeList(options_offdiag));
  }

  OXS_GET_EXT_OBJECT(params_diag,Oxs_VectorField,e_diag_init);
  OXS_GET_EXT_OBJECT(params_offdiag,Oxs_VectorField,e_offdiag_init);
  working_stage = stage;
  stage_valid = 1;
}

void YY_StageMEL::UpdateCache(const Oxs_SimState& state) const
{
  // Update cache as necessary
  if(!stage_valid) {
    // The first go.
    mesh_id = 0;
    MELField.SetMELCoef(state,MELCoef1_init,MELCoef2_init);
    ChangeInitializer(state);
    SetStrain(state);
  } else if(working_stage != state.stage_number) {
    mesh_id = 0;
    ChangeInitializer(state);
    SetStrain(state);
    mesh_id = state.mesh->Id();
  } else if(mesh_id != state.mesh->Id()) {
    mesh_id = 0;
    MELField.SetMELCoef(state,MELCoef1_init,MELCoef2_init);
    SetStrain(state);
    mesh_id = state.mesh->Id();
  }
}

void YY_StageMEL::GetEnergy
(const Oxs_SimState& state, Oxs_EnergyData& oed) const
{
  OC_INDEX size = state.mesh->Size();
  if(size<1) return;

  UpdateCache(state);

  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);

  // Use supplied buffer space, and reflect that use in oed.
  oed.energy = oed.energy_buffer;
  oed.field = oed.field_buffer;
  Oxs_MeshValue<OC_REAL8m>& energy = *oed.energy_buffer;
  Oxs_MeshValue<ThreeVector>& field = *oed.field_buffer;
  energy.AdjustSize(state.mesh);
  field.AdjustSize(state.mesh);

  MELField.CalculateMELField(state, hmult, field, energy);
  max_field = MELField.GetMaxField();
}

void
YY_StageMEL::Fill__B_MEL_output(const Oxs_SimState& state)
{
  UpdateCache(state);

  ThreeVector B = max_field;
  B *= MU0 * 1000;  // Report B_MEL in mT

  if(B_MEL_output.GetCacheRequestCount() > 0) {
    B_MEL_output.cache.state_id = 0;
    B_MEL_output.cache.value = sqrt(B.MagSq());
    B_MEL_output.cache.state_id = state.Id();
  }

  if(B_MELx_output.GetCacheRequestCount() > 0) {
    B_MELx_output.cache.state_id = 0;
    B_MELx_output.cache.value = B.x;
    B_MELx_output.cache.state_id = state.Id();
  }

  if(B_MELy_output.GetCacheRequestCount() > 0) {
    B_MELy_output.cache.state_id = 0;
    B_MELy_output.cache.value = B.y;
    B_MELy_output.cache.state_id = state.Id();
  }

  if(B_MELz_output.GetCacheRequestCount() > 0) {
    B_MELz_output.cache.state_id = 0;
    B_MELz_output.cache.value = B.z;
    B_MELz_output.cache.state_id = state.Id();
  }
}

void YY_StageMEL::ChangeInitializer(const Oxs_SimState& state) const
{
  if(use_u) { // Set displacement and let MELField calculate strain.
    ChangeDisplacementInitializer(state.stage_number,state.mesh);
  } else {    // use_e==true. Set strain directly.
    ChangeStrainInitializer(state.stage_number,state.mesh);
  }
}

void YY_StageMEL::SetStrain(const Oxs_SimState& state) const
{
  if(use_u) { // Set displacement and let MELField calculate strain.
    MELField.SetDisplacement(state,u_init);
  } else {    // use_e==true. Set strain directly.
    MELField.SetStrain(state,e_diag_init,e_offdiag_init);
  }
}

void YY_StageMEL::SelectElasticityInputType()
{
  // Sets several flags for elasticity input.

  // Whether you use displacement or strain
  use_u = HasInitValue("u_script") || HasInitValue("u_files");

  // Whether filelist(s) or script(s)
  if(use_u) {
    use_u_filelist = HasInitValue("u_files");
    use_u_script = HasInitValue("u_script");
    if( use_u_filelist && use_u_script ) {
      const char *cptr =
        "Select only one of u_script and u_files.";
      throw Oxs_ExtError(this,cptr);
    }

    // Script name
    if( use_u_filelist ) {
      // Make certain list contains at least 1 file, because
      // length of filelist is used as a flag to determine
      // whether to call u_cmd or not.
      GetGroupedStringListInitValue("u_files",u_filelist);
      if(u_filelist.empty()) {
        throw Oxs_ExtError(this,"\"u_files\" parameter value is empty."
                             " At least one filename is required.");
      }
      // As a failsafe, set up a dummy command that generates
      // a clear error message if in fact u_cmd is accidentally
      // called.
      String dummy_cmd =
        "error \"Programming error; Oxs_StageZeeman script called"
        " from u_filelist mode.\" ;# ";
      u_cmd.SetBaseCommand(InstanceName(),
                         director->GetMifInterp(),
                         dummy_cmd,1);

      number_of_stages 
        = GetUIntInitValue("stage_count",
                           static_cast<OC_UINT4m>(u_filelist.size()));
      /// Default number_of_stages in this case is the length
      /// of u_filelist.
    } else {  // use u_script
      u_cmd.SetBaseCommand(InstanceName(),
                         director->GetMifInterp(),
                         GetStringInitValue("u_script"),1);
      // u_cmd takes one integer argument: the current stage.
      // Return value should be a vector field spec.

      number_of_stages = GetUIntInitValue("stage_count",0);
      // Default value = 0, i.e., no preference.
    }
  } else { // use_e
    use_e_filelist = HasInitValue("e_diag_files") && HasInitValue("e_offdiag_files");
    use_e_script = HasInitValue("e_diag_script") && HasInitValue("e_offdiag_script");
    if( (use_e_filelist && use_e_script) || (!use_e_filelist && !use_e_script) ) {
      // If both files and script are specified,
      // or if neither is selected.
      const char *cptr =
        "Select only one of e_*diag_script and e_*diag_files. You"
        " need to specify both diagonal and off-diagonal elements.";
      throw Oxs_ExtError(this,cptr);
    }

    // Script name
    if(use_e_filelist) {
      // Make certain list contains at least 1 file, because
      // length of filelist is used as a flag to determine
      // whether to call e_cmd or not.
      GetGroupedStringListInitValue("e_diag_files",e_diag_filelist);
      GetGroupedStringListInitValue("e_offdiag_files",e_offdiag_filelist);
      if( e_diag_filelist.size() != e_offdiag_filelist.size() ) {
        throw Oxs_ExtError(this,
            "\"e_diag_files\" and \"e_offdiag_files\" must be at"
            " the same length.");
      }
      if( e_diag_filelist.empty() || e_offdiag_filelist.empty() ) {
        throw Oxs_ExtError(this,
            "\"e_diag_files\" or \"e_offdiag_files\" parameter"
            " value is empty. At least one filename for each is"
            " required.");
      }
      // As a failsafe, set up a dummy command that generates
      // a clear error message if in fact e_cmd is accidentally
      // called.
      String dummy_cmd =
        "error \"Programming error; Oxs_StageZeeman script called"
        " from e_filelist mode.\" ;# ";
      e_diag_cmd.SetBaseCommand(InstanceName(),
                         director->GetMifInterp(),
                         dummy_cmd,1);
      e_offdiag_cmd.SetBaseCommand(InstanceName(),
                         director->GetMifInterp(),
                         dummy_cmd,1);

      number_of_stages 
        = GetUIntInitValue("stage_count",
                           static_cast<OC_UINT4m>(e_diag_filelist.size()));
      /// Default number_of_stages in this case is the length
      /// of e_diag_filelist.
    } else {  // use a script for strain
      e_diag_cmd.SetBaseCommand(InstanceName(),
                         director->GetMifInterp(),
                         GetStringInitValue("e_diag_script"),1);
      e_offdiag_cmd.SetBaseCommand(InstanceName(),
                         director->GetMifInterp(),
                         GetStringInitValue("e_offdiag_script"),1);
      // e_*diag_cmd take one integer argument: the current stage.
      // Return value should be a vector field spec.

      number_of_stages = GetUIntInitValue("stage_count",0);
      // Default value = 0, i.e., no preference.
    }
  }
  // END: Whether filelist(s) or script(s)
}
