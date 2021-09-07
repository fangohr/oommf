/* FILE: yy_interpolatestagemel.h                 -*-Mode: c++-*-
 *
 * OOMMF magnetoelastic coupling extension module.
 * YY_InterpolateStageMEL class.
 * Interpolates the elasticity input defined at the start of each stage.
 * 
 */

#ifndef _YY_INTERPOLATESTAGEMEL
#define _YY_INTERPOLATESTAGEMEL

#include <vector>

#include "oc.h"
#include "nb.h"
#include "director.h"
#include "energy.h"
#include "meshvalue.h"
#include "simstate.h"
#include "threevector.h"
#include "vectorfield.h"
#include "util.h"

#include "yy_mel_util.h"

OC_USE_STRING; // typedef std::string String

/* End includes */

class YY_InterpolateStageMEL:public Oxs_Energy {
private:
  // magnetoelastic coefficient MELCoef1,2 = B1,2 (J/m^3)
  Oxs_OwnedPointer<Oxs_ScalarField> MELCoef1_init, MELCoef2_init;
  mutable Oxs_OwnedPointer<Oxs_VectorField> u_init1;
  mutable Oxs_OwnedPointer<Oxs_VectorField> u_init2;
  mutable Oxs_OwnedPointer<Oxs_VectorField> e_diag_init1, e_offdiag_init1;
  mutable Oxs_OwnedPointer<Oxs_VectorField> e_diag_init2, e_offdiag_init2;

  mutable YY_MELField MELField; // Total MEL field
  mutable YY_MELField MELField1, MELField2;

  OC_REAL8m hmult;  // multiplier
  OC_UINT4m number_of_stages;

  // Note: For determining the length of each stage, it needs the list of
  // stopping_time, identical to the one specified in the Oxs_TimeDriver.
  // This an ugly hack but I suppose the risk of accidentally setting a
  // wrong list is relatively low.
  vector<OC_REAL8m> stopping_time;  // Seconds
  OC_BOOL VerifyLastStageDoneCondition(const Oxs_SimState& state) const;
  OC_REAL8m GetWorkingStageStoppingTime(const Oxs_SimState& state) const;

  enum InterpolationType { linear, cubic };
  InterpolationType interpolation_type;

  mutable OC_UINT4m mesh_id;
  mutable OC_BOOL stage_valid;
  mutable OC_UINT4m working_stage;  // Stage index
  mutable OC_REAL8m working_stage_elapsed_time;
  mutable OC_REAL8m previous_stage_elapsed_time;
  mutable OC_REAL8m working_stage_stopping_time;
  mutable ThreeVector max_field;

  // Note: Elastic strain can be specified in various ways. It can be
  // directly set by specifying strain or displacement can be used for
  // calculation of strain. This choise is judged by the presence of
  // displacement input (u_script or u_files) or by the presence of
  // strain input(e_*diag_script or e_*diag_files). Also, the vector fields
  // (either displacement or strain) may be specified either with a list of
  // files or a Tcl command that returns a vector field spec. The choise
  // between the file list and the Tcl command is made by the length of the
  // file list; when it is >0, file list is used, otherwise *_cmd is called
  // with the stage number as the single appended argument.
  vector<String> u_filelist;
  vector<String> e_diag_filelist, e_offdiag_filelist;
  Nb_TclCommand u_cmd1, e_diag_cmd1, e_offdiag_cmd1;
  Nb_TclCommand u_cmd2, e_diag_cmd2, e_offdiag_cmd2;

  // Flags to specify the input source(s) of strain.
  OC_BOOL use_u, use_u_filelist, use_u_script;
  OC_BOOL use_e, use_e_filelist, use_e_script;

  // Member function to determine the way of specifying
  // strain. Whether directly using strain or indirectly by
  // specifying displacement and calculating strain with it.
  // Also determines whether filelists or scripts are used.
  // It sets use_u and use_e (== !use_u) and use_u_filelist
  // or use_e_filelist depending on use_u.
  void SelectElasticityInputType();

  // Update the initializers for displacement or strain. ChangeInitializer()
  // checks use_u flag, calls either one of ChangeDisplacementInitializer()
  // or ChangeStrainInitializer(). After that, call SetStrain() to sets the
  // MELField strain in a proper way by calling either 
  // MELField.SetDisplacement() or MELField.SetStrain().
  void ChangeInitializer(const Oxs_SimState& state) const;
  void ChangeDisplacementInitializer(OC_UINT4m stage, const Oxs_Mesh* mesh) const;
  void ChangeStrainInitializer(OC_UINT4m stage, const Oxs_Mesh* mesh) const;
  void SetStrain(const Oxs_SimState& state) const;
  void UpdateCache(const Oxs_SimState& state) const;

  // Additional outputs
  Oxs_ScalarOutput<YY_InterpolateStageMEL> B_MEL_output;
  Oxs_ScalarOutput<YY_InterpolateStageMEL> B_MELx_output;
  Oxs_ScalarOutput<YY_InterpolateStageMEL> B_MELy_output;
  Oxs_ScalarOutput<YY_InterpolateStageMEL> B_MELz_output;
  void Fill__B_MEL_output(const Oxs_SimState& state);

protected:
  virtual void GetEnergy(const Oxs_SimState& state,
      Oxs_EnergyData& oed) const;

public:
  virtual const char* ClassName() const; // ClassName() is
  /// automatically generated by the OXS_EXT_REGISTER macro.
  YY_InterpolateStageMEL(const char* name, // Child instance id
		    Oxs_Director* newdtr,   // App director
		    const char* argstr);    // MIF input block parameters
  virtual ~YY_InterpolateStageMEL();
  virtual OC_BOOL Init();
  virtual void StageRequestCount(unsigned int& min,
      unsigned int& max) const;

#ifdef YY_DEBUG
  // For debug. Display variables in specified range
  void DisplayValues(
      const Oxs_SimState& state,
      OC_INDEX xmin, OC_INDEX xmax,
      OC_INDEX ymin, OC_INDEX ymax,
      OC_INDEX zmin, OC_INDEX zmax) const;
#endif
};

#endif // _YY_INTERPOLATESTAGEMEL