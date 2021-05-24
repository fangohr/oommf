/* FILE: yy_interpolatestagemel.cc                 -*-Mode: c++-*-
 *
 * OOMMF magnetoelastic coupling extension module.
 * YY_InterpolateStageMEL class.
 * Interpolates the elasticity input defined at the start of each stage.
 * 
 */

#include "oc.h"
#include "nb.h"
#include "director.h"
#include "mesh.h"
#include "meshvalue.h"
#include "simstate.h"
#include "threevector.h"
#include "rectangularmesh.h"
#include "energy.h"             // Needed to make MSVC++ 5 happy

#include "yy_interpolatestagemel.h"
#include "yy_mel_util.h"

OC_USE_STRING;

/* End includes */

// Oxs_Ext registration support
OXS_EXT_REGISTER(YY_InterpolateStageMEL);

// Constructor
YY_InterpolateStageMEL::YY_InterpolateStageMEL(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr), number_of_stages(0),
  mesh_id(0), stage_valid(0), working_stage(0),
  working_stage_elapsed_time(0.0), previous_stage_elapsed_time(0.0)
{
  working_stage = static_cast<OC_UINT4m>(static_cast<OC_INT4m>(-1));
  // First ChangeFileInitializer() call should be triggered
  // by the stage_valid boolean.

  // Process arguments
  hmult = GetRealInitValue("multiplier", 1.0);

  // stopping_time list identical to the one specified in Oxs_TimeDriver
  if(!HasInitValue("stopping_time")) {
    // This list needs to be specified for interpolation.
    throw Oxs_ExtError(this,"\"stopping_time\" needs to be specified"
        " for calculating interpolation and checking the conditions"
        " of stage changes.");
  } else {
    GetGroupedRealListInitValue("stopping_time",stopping_time);
  }

  // Generate MELCoef initializer
  OXS_GET_INIT_EXT_OBJECT("B1",Oxs_ScalarField,MELCoef1_init);
  OXS_GET_INIT_EXT_OBJECT("B2",Oxs_ScalarField,MELCoef2_init);

  // set use_u_filelist or use_e_filelist
  SelectElasticityInputType();

  String xform_type_string = GetStringInitValue("type","linear");
  if(xform_type_string.compare("linear")==0) {
    interpolation_type = linear;
  } else if(xform_type_string.compare("cubic")==0) {
    interpolation_type = cubic;
    String msg = String("Cubic interpolation not implemented yet.");
    throw Oxs_ExtError(this,msg.c_str());
  } else {
    String msg = String("Invalid interpolation type: \"")
      + xform_type_string
      + String("\".  Should be linear or cubic.");
    throw Oxs_ExtError(this,msg.c_str());
  }

  // Initialize outputs.
  B_MEL_output.Setup(this, InstanceName(), "B max", "mT", 1,
    &YY_InterpolateStageMEL::Fill__B_MEL_output);
  B_MELx_output.Setup(this, InstanceName(), "Bx max", "mT", 1,
    &YY_InterpolateStageMEL::Fill__B_MEL_output);
  B_MELy_output.Setup(this, InstanceName(), "By max", "mT", 1,
    &YY_InterpolateStageMEL::Fill__B_MEL_output);
  B_MELz_output.Setup(this, InstanceName(), "Bz max", "mT", 1,
    &YY_InterpolateStageMEL::Fill__B_MEL_output);

  // Register outputs.
  B_MEL_output.Register(director, 0);
  B_MELx_output.Register(director, 0);
  B_MELy_output.Register(director, 0);
  B_MELz_output.Register(director, 0);

  VerifyAllInitArgsUsed();

  // Run SetBaseCommand *after* VerifyAllInitArgsUsed(); this produces
  // a more intelligible error message in case a command line argument
  // error is really due to a misspelled parameter label.

}

YY_InterpolateStageMEL::~YY_InterpolateStageMEL()
{}

OC_BOOL YY_InterpolateStageMEL::Init()
{
  stage_valid = 0;

  mesh_id = 0;
  MELField.Release();
  MELField1.Release();
  MELField2.Release();

  return Oxs_Energy::Init();
}

void YY_InterpolateStageMEL::StageRequestCount(unsigned int& min,
    unsigned int& max) const
{
  if(number_of_stages == 0) {
    min = 0; max = UINT_MAX;  // No restriction on stage count
  } else {
    min = max = number_of_stages;
  }
}

void YY_InterpolateStageMEL::ChangeDisplacementInitializer(
    OC_UINT4m stage, const Oxs_Mesh* mesh) const
{
  // Setup displacement
  vector<String> params1, params2;
  if(u_filelist.empty()) {
    // Use u_cmd to generate field initializer
    u_cmd1.SaveInterpResult();
    u_cmd2.SaveInterpResult();
    u_cmd1.SetCommandArg(0,stage);
    u_cmd2.SetCommandArg(0,stage+1);
    u_cmd1.Eval();
    u_cmd2.Eval();
    u_cmd1.GetResultList(params1);
    u_cmd2.GetResultList(params2);
    u_cmd1.RestoreInterpResult();
    u_cmd2.RestoreInterpResult();
  } else {
    // Construct field initializer using Oxs_FileVectorField
    // with filename from u_filelist and range from mesh.
    OC_UINT4m index1 = stage, index2 = stage+1;
    OC_UINT4m filecount = static_cast<OC_UINT4m>(u_filelist.size());
    if(index1 >= filecount) index1 = filecount - 1;
    if(index2 >= filecount) index2 = filecount - 1;
    vector<String> options1, options2;
    options1.push_back(String("file"));
    options2.push_back(String("file"));
    options1.push_back(u_filelist[index1]);
    options2.push_back(u_filelist[index2]);

    Oxs_Box bbox;    mesh->GetBoundingBox(bbox);
    char buf[64];
    options1.push_back(String("xrange"));
    options2.push_back(String("xrange"));
    Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g",
                static_cast<double>(bbox.GetMinX()),
                static_cast<double>(bbox.GetMaxX()));
    options1.push_back(String(buf));
    options2.push_back(String(buf));

    options1.push_back(String("yrange"));
    options2.push_back(String("yrange"));
    Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g",
                static_cast<double>(bbox.GetMinY()),
                static_cast<double>(bbox.GetMaxY()));
    options1.push_back(String(buf));
    options2.push_back(String(buf));

    options1.push_back(String("zrange"));
    options2.push_back(String("zrange"));
    Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g",
                static_cast<double>(bbox.GetMinZ()),
                static_cast<double>(bbox.GetMaxZ()));
    options1.push_back(String(buf));
    options2.push_back(String(buf));

    params1.push_back(String("Oxs_FileVectorField"));
    params2.push_back(String("Oxs_FileVectorField"));
    params1.push_back(Nb_MergeList(options1));
    params2.push_back(Nb_MergeList(options2));
  }

  OXS_GET_EXT_OBJECT(params1,Oxs_VectorField,u_init1);
  OXS_GET_EXT_OBJECT(params2,Oxs_VectorField,u_init2);
  working_stage = stage;
  stage_valid = 1;
}

void YY_InterpolateStageMEL::ChangeStrainInitializer(
    OC_UINT4m stage, const Oxs_Mesh* mesh) const
{
  // Setup strain
  vector<String> params_diag1, params_offdiag1;
  vector<String> params_diag2, params_offdiag2;
  if( use_e_script ) {
    // Use e_*diag_cmd to generate field initializer
    e_diag_cmd1.SaveInterpResult();
    e_diag_cmd2.SaveInterpResult();
    e_diag_cmd1.SetCommandArg(0,stage);
    e_diag_cmd2.SetCommandArg(0,stage+1);
    e_diag_cmd1.Eval();
    e_diag_cmd2.Eval();
    e_diag_cmd1.GetResultList(params_diag1);
    e_diag_cmd2.GetResultList(params_diag2);
    e_diag_cmd1.RestoreInterpResult();
    e_diag_cmd2.RestoreInterpResult();
    e_offdiag_cmd1.SaveInterpResult();
    e_offdiag_cmd2.SaveInterpResult();
    e_offdiag_cmd1.SetCommandArg(0,stage);
    e_offdiag_cmd2.SetCommandArg(0,stage+1);
    e_offdiag_cmd1.Eval();
    e_offdiag_cmd2.Eval();
    e_offdiag_cmd1.GetResultList(params_offdiag1);
    e_offdiag_cmd2.GetResultList(params_offdiag2);
    e_offdiag_cmd1.RestoreInterpResult();
    e_offdiag_cmd2.RestoreInterpResult();
  } else {  // use_e_filelist
    // Construct field initializer using Oxs_FileVectorField
    // with filename from e_diag_filelist and range from mesh.
    OC_UINT4m index1 = stage, index2 = stage+1;
    OC_UINT4m filecount = static_cast<OC_UINT4m>(e_diag_filelist.size());
    if(index1 >= filecount) index1 = filecount - 1;
    if(index2 >= filecount) index2 = filecount - 1;
    vector<String> options_diag1, options_offdiag1;
    vector<String> options_diag2, options_offdiag2;
    Oxs_Box bbox;    mesh->GetBoundingBox(bbox);
    char buf[64];

    // Diagonal elements
    params_diag1.push_back(String("Oxs_FileVectorField"));
    params_diag2.push_back(String("Oxs_FileVectorField"));
    options_diag1.push_back(String("xrange"));
    options_diag2.push_back(String("xrange"));
    Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g",
                static_cast<double>(bbox.GetMinX()),
                static_cast<double>(bbox.GetMaxX()));
    options_diag1.push_back(String(buf));
    options_diag2.push_back(String(buf));
    options_diag1.push_back(String("yrange"));
    options_diag2.push_back(String("yrange"));
    Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g",
                static_cast<double>(bbox.GetMinY()),
                static_cast<double>(bbox.GetMaxY()));
    options_diag1.push_back(String(buf));
    options_diag2.push_back(String(buf));
    options_diag1.push_back(String("zrange"));
    options_diag2.push_back(String("zrange"));
    Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g",
                static_cast<double>(bbox.GetMinZ()),
                static_cast<double>(bbox.GetMaxZ()));
    options_diag1.push_back(String(buf));
    options_diag2.push_back(String(buf));
    options_diag1.push_back(String("file"));
    options_diag2.push_back(String("file"));
    options_diag1.push_back(e_diag_filelist[index1]);
    options_diag2.push_back(e_diag_filelist[index2]);

    params_diag1.push_back(Nb_MergeList(options_diag1));
    params_diag2.push_back(Nb_MergeList(options_diag2));

    // Off-diagonal elements
    params_offdiag1.push_back(String("Oxs_FileVectorField"));
    params_offdiag2.push_back(String("Oxs_FileVectorField"));
    options_offdiag1.push_back(String("xrange"));
    options_offdiag2.push_back(String("xrange"));
    Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g",
                static_cast<double>(bbox.GetMinX()),
                static_cast<double>(bbox.GetMaxX()));
    options_offdiag1.push_back(String(buf));
    options_offdiag2.push_back(String(buf));
    options_offdiag1.push_back(String("yrange"));
    options_offdiag2.push_back(String("yrange"));
    Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g",
                static_cast<double>(bbox.GetMinY()),
                static_cast<double>(bbox.GetMaxY()));
    options_offdiag1.push_back(String(buf));
    options_offdiag2.push_back(String(buf));
    options_offdiag1.push_back(String("zrange"));
    options_offdiag2.push_back(String("zrange"));
    Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g",
                static_cast<double>(bbox.GetMinZ()),
                static_cast<double>(bbox.GetMaxZ()));
    options_offdiag1.push_back(String(buf));
    options_offdiag2.push_back(String(buf));
    options_offdiag1.push_back(String("file"));
    options_offdiag2.push_back(String("file"));
    options_offdiag1.push_back(e_offdiag_filelist[index1]);
    options_offdiag2.push_back(e_offdiag_filelist[index1]);

    params_offdiag1.push_back(Nb_MergeList(options_offdiag1));
    params_offdiag2.push_back(Nb_MergeList(options_offdiag2));
  }

  OXS_GET_EXT_OBJECT(params_diag1,Oxs_VectorField,e_diag_init1);
  OXS_GET_EXT_OBJECT(params_diag2,Oxs_VectorField,e_diag_init2);
  OXS_GET_EXT_OBJECT(params_offdiag1,Oxs_VectorField,e_offdiag_init1);
  OXS_GET_EXT_OBJECT(params_offdiag2,Oxs_VectorField,e_offdiag_init2);
  working_stage = stage;
  stage_valid = 1;
}

void YY_InterpolateStageMEL::UpdateCache(const Oxs_SimState& state) const
{
  // Update cache as necessary
  if(!stage_valid) {
    // The first go.
    mesh_id = 0;
    MELField1.SetMELCoef(state,MELCoef1_init,MELCoef2_init);
    MELField2.SetMELCoef(state,MELCoef1_init,MELCoef2_init);
    ChangeInitializer(state);
    SetStrain(state);
    working_stage_stopping_time = GetWorkingStageStoppingTime(state);
  } else if(working_stage != state.stage_number) {
    previous_stage_elapsed_time = working_stage_elapsed_time;
    working_stage = state.stage_number;
    if( !VerifyLastStageDoneCondition(state) ) {
      throw Oxs_ExtError(this,"Stage finished before the specified stopping time."
          " Interpolation is not valid.");
    }

    working_stage_stopping_time = GetWorkingStageStoppingTime(state);
    mesh_id = 0;
    ChangeInitializer(state);
    SetStrain(state);
    mesh_id = state.mesh->Id();
  }
  if(mesh_id != state.mesh->Id()) {
    // The mesh has changed.
    mesh_id = 0;
    MELField1.SetMELCoef(state,MELCoef1_init,MELCoef2_init);
    MELField2.SetMELCoef(state,MELCoef1_init,MELCoef2_init);
    ChangeInitializer(state);
    SetStrain(state);
    mesh_id = state.mesh->Id();
    working_stage_stopping_time = GetWorkingStageStoppingTime(state);
  }
  working_stage_elapsed_time = state.stage_elapsed_time;
}

void YY_InterpolateStageMEL::GetEnergy
(const Oxs_SimState& state, Oxs_EnergyData& oed) const
{
  OC_INDEX size = state.mesh->Size();
  if(size<1) return;

  UpdateCache(state);

  const Oxs_Mesh* mesh = state.mesh;

  // Use supplied buffer space, and reflect that use in oed.
  oed.energy = oed.energy_buffer;
  oed.field = oed.field_buffer;
  Oxs_MeshValue<OC_REAL8m>& energy = *oed.energy_buffer;
  Oxs_MeshValue<ThreeVector>& field = *oed.field_buffer;
  energy.AdjustSize(mesh);
  field.AdjustSize(mesh);

  if(interpolation_type == linear) {
    MELField.Interpolate(state, working_stage_stopping_time,
        MELField1, MELField2);
    MELField.CalculateMELField(state, hmult, field, energy);
    max_field = MELField.GetMaxField();
    oed.pE_pt = MELField.GetPEPT();
  } else { // cubic
    // TODO: Cubic interpolation not implemented yet.
    MELField.Interpolate(state, working_stage_stopping_time,
        MELField1, MELField2);
    MELField.CalculateMELField(state, hmult, field, energy);
    max_field = MELField.GetMaxField();
    oed.pE_pt = MELField.GetPEPT();
  }
}

void
YY_InterpolateStageMEL::Fill__B_MEL_output(const Oxs_SimState& state)
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

void YY_InterpolateStageMEL::ChangeInitializer(const Oxs_SimState& state) const
{
  if(use_u) { // Set displacement and let MELField calculate strain.
    ChangeDisplacementInitializer(state.stage_number,state.mesh);
  } else {    // use_e==true. Set strain directly.
    ChangeStrainInitializer(state.stage_number,state.mesh);
  }
}

void YY_InterpolateStageMEL::SetStrain(const Oxs_SimState& state) const
{
  if(use_u) { // Set displacement and let MELField calculate strain.
    MELField.SetDisplacement(state,u_init1);  // Initialize the total MELField too
    MELField1.SetDisplacement(state,u_init1);
    MELField2.SetDisplacement(state,u_init2);
  } else {    // use_e==true. Set strain directly.
    MELField.SetStrain(state,e_diag_init1,e_offdiag_init1); // Initialize the total MELField too
    MELField1.SetStrain(state,e_diag_init1,e_offdiag_init1);
    MELField2.SetStrain(state,e_diag_init2,e_offdiag_init2);
  }
}

void YY_InterpolateStageMEL::SelectElasticityInputType()
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
      u_cmd1.SetBaseCommand(InstanceName(),
                         director->GetMifInterp(),
                         dummy_cmd,1);
      u_cmd2.SetBaseCommand(InstanceName(),
                         director->GetMifInterp(),
                         dummy_cmd,1);

      number_of_stages 
        = GetUIntInitValue("stage_count",
                           static_cast<OC_UINT4m>(u_filelist.size()));
      /// Default number_of_stages in this case is the length
      /// of u_filelist.
    } else {  // use u_script
      String u_script_string = GetStringInitValue("u_script");
      u_cmd1.SetBaseCommand(InstanceName(),
                         director->GetMifInterp(),
                         u_script_string,1);
      u_cmd2.SetBaseCommand(InstanceName(),
                         director->GetMifInterp(),
                         u_script_string,1);
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
      e_diag_cmd1.SetBaseCommand(InstanceName(),
                         director->GetMifInterp(),
                         dummy_cmd,1);
      e_diag_cmd2.SetBaseCommand(InstanceName(),
                         director->GetMifInterp(),
                         dummy_cmd,1);
      e_offdiag_cmd1.SetBaseCommand(InstanceName(),
                         director->GetMifInterp(),
                         dummy_cmd,1);
      e_offdiag_cmd2.SetBaseCommand(InstanceName(),
                         director->GetMifInterp(),
                         dummy_cmd,1);

      number_of_stages 
        = GetUIntInitValue("stage_count",
                           static_cast<OC_UINT4m>(e_diag_filelist.size()));
      /// Default number_of_stages in this case is the length
      /// of e_diag_filelist.
    } else {  // use a script for strain
      String e_diag_script_string = GetStringInitValue("e_diag_script");
      String e_offdiag_script_string = GetStringInitValue("e_offdiag_script");
      e_diag_cmd1.SetBaseCommand(InstanceName(),
                         director->GetMifInterp(),
                         e_diag_script_string,1);
      e_diag_cmd2.SetBaseCommand(InstanceName(),
                         director->GetMifInterp(),
                         e_diag_script_string,1);
      e_offdiag_cmd1.SetBaseCommand(InstanceName(),
                         director->GetMifInterp(),
                         e_offdiag_script_string,1);
      e_offdiag_cmd2.SetBaseCommand(InstanceName(),
                         director->GetMifInterp(),
                         e_offdiag_script_string,1);
      // e_*diag_cmd take one integer argument: the current stage.
      // Return value should be a vector field spec.

      number_of_stages = GetUIntInitValue("stage_count",0);
      // Default value = 0, i.e., no preference.
    }
  }
  // END: Whether filelist(s) or script(s)
}

OC_BOOL YY_InterpolateStageMEL::VerifyLastStageDoneCondition(const Oxs_SimState& state) const
{
  // Stage time check
  OC_REAL8m stop_time = 0.;
  OC_REAL8m prev = state.stage_number-1;  // previous stage index
  if(prev >= stopping_time.size()) {
    stop_time = stopping_time[stopping_time.size()-1];
  } else {
    stop_time = stopping_time[prev];
  }
  if(stop_time>0.0
     && stop_time-previous_stage_elapsed_time<=stop_time*OC_REAL8_EPSILON*2) {
    return 1; // Stage done with stopping_time condition
  }

  return 0;   // Stage done with before stopping_time
}

OC_REAL8m YY_InterpolateStageMEL::GetWorkingStageStoppingTime(const Oxs_SimState& state) const
{
  working_stage = state.stage_number;
  OC_REAL8m stop_time = 0.;
  if(working_stage >= stopping_time.size()) {
    stop_time = stopping_time[stopping_time.size()-1];
  } else {
    stop_time = stopping_time[working_stage];
  }
  return stop_time;
}
