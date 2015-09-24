/* FILE: driver.h                 -*-Mode: c++-*-
 *
 * Abstract driver class; fills states and steps evolvers.
 *
 */

#ifndef _OXS_DRIVER
#define _OXS_DRIVER

#include <string>

#include "nb.h"
#include "ext.h"
#include "labelvalue.h"
#include "mesh.h"
#include "simstate.h"
#include "key.h"
#include "outputderiv.h"
#include "vectorfield.h"

OC_USE_STRING;

/* End includes */

class Oxs_Evolver; // Forward references
struct OxsRunEvent;

struct Oxs_DriverStepInfo {
public:
  OC_UINT4m total_attempt_count; // Total number of attempts
  OC_UINT4m current_attempt_count;  // Number of attempts at current step
  Oxs_DriverStepInfo()
    : total_attempt_count(0),current_attempt_count(0) {}
  Oxs_DriverStepInfo(OC_UINT4m tc,OC_UINT4m ac)
    : total_attempt_count(tc),current_attempt_count(ac) {}
};

class Oxs_Driver: public Oxs_Ext {
private:
#if REPORT_TIME
  // driversteptime records time (cpu and wall) spent in the driver's
  // Step function.  This information is reported to stderr when
  // Oxs_Driver is re-initialized or destroyed.  NOTE: This does NOT
  // include time spent in Stage processing, In particular, stage end
  // output may include calls to the various energy computation
  // routines that can soak up a lot of cpu cycles that will not be
  // reflected in the driversteptime stopwatch.  If, for example,
  // demag initialization occurs during stage processing, then that
  // time will not be included either.
  Nb_StopWatch driversteptime;
#endif // REPORT_TIME

  Oxs_ConstKey<Oxs_SimState> current_state;

  Oxs_DriverStepInfo step_info;

  Oxs_OwnedPointer<Oxs_Mesh> mesh_obj; // Mesh basket
  Oxs_ConstKey<Oxs_Mesh> mesh_key;

  Oxs_MeshValue<OC_REAL8m> Ms;  // Saturation magnetization
  Oxs_MeshValue<OC_REAL8m> Ms_inverse;  // 1/Ms
  Oxs_OwnedPointer<Oxs_VectorField> m0; // Initial spin configuration

  // State-based outputs, maintained by the driver.  These are
  // conceptually public, but are specified private to force
  // clients to use the output_map interface in Oxs_Director.
#define OSO_DECL(name) \
void Fill__##name##_output(const Oxs_SimState&); \
Oxs_ScalarOutput<Oxs_Driver> name##_output
  OSO_DECL(iteration_count);
  OSO_DECL(stage_iteration_count);
  OSO_DECL(stage_number);
  Oxs_VectorFieldOutput<Oxs_Driver> spin_output;
  void Fill__spin_output(const Oxs_SimState&);
  Oxs_VectorFieldOutput<Oxs_Driver> magnetization_output;
  void Fill__magnetization_output(const Oxs_SimState&);
  Oxs_ScalarOutput<Oxs_Driver> aveMx_output;
  Oxs_ScalarOutput<Oxs_Driver> aveMy_output;
  Oxs_ScalarOutput<Oxs_Driver> aveMz_output;
  void Fill__aveM_output(const Oxs_SimState&);

  struct OxsDriverProjectionOutput {
  public:
    Oxs_ScalarOutput<Oxs_Driver> output;
    Oxs_MeshValue<ThreeVector> trellis;
    OC_BOOL normalize;
    OC_REAL8m scaling; // For use with regular meshes (includes user_scaling)
    OC_REAL8m user_scaling;
    String name;
    String units;
    vector<String> trellis_init;
    OxsDriverProjectionOutput()
      : normalize(1), scaling(0.), user_scaling(1.) {}
  private:
    // Disable copy constructor and assignment operators by
    // declaring them w/o providing definition.
    OxsDriverProjectionOutput(const OxsDriverProjectionOutput&);
    OxsDriverProjectionOutput& operator=(const OxsDriverProjectionOutput&);
  };
  Nb_ArrayWrapper<OxsDriverProjectionOutput> projection_output;
  void Fill__projection_outputs(const Oxs_SimState&);

  OC_BOOL report_max_spin_angle;
  Oxs_ScalarOutput<Oxs_Driver> maxSpinAng_output;
  Oxs_ScalarOutput<Oxs_Driver> stage_maxSpinAng_output;
  Oxs_ScalarOutput<Oxs_Driver> run_maxSpinAng_output;
  void UpdateSpinAngleData(const Oxs_SimState& state) const;
  void Fill__maxSpinAng_output(const Oxs_SimState&);

  OC_BOOL normalize_aveM;
  OC_REAL8m scaling_aveM; // For use with regular meshes

#undef OSO_DECL

  vector<OC_UINT4m> stage_iteration_limit; // Counts; 0 => no limit
  OC_UINT4m total_iteration_limit; // 0 => no limit
  OC_UINT4m stage_count_request;   // 0 => unlimited
  OC_UINT4m number_of_stages;      // 0 => unlimited
  /// stage_count_request is set in Specify block via
  /// stage_count parameter.  number_of_stages is set
  /// via call to Oxs_Director::ExtObjStageRequestCounts().

  OC_INT4m stage_count_check; // 1 => raise error if
  /// stage counts across Oxs_Ext objects aren't compatible.

  OC_UINT4m report_stage_number; // See Run member for details.

  // Restart file and save interval.
  String checkpoint_file;     // Name of checkpoint file
  OC_UINT4m checkpoint_id;       // Id of checkpointed state
  OC_UINT4m checkpoint_writes;   // Number of checkpoint files written
  double checkpoint_interval; // In wall-clock seconds.
  Oc_TimeVal checkpoint_time; // Time of last checkpoint save.
  enum OxsDriverCheckpointCleanupTypes {
    OXSDRIVER_CCT_INVALID, OXSDRIVER_CCT_NORMAL,
    OXSDRIVER_CCT_DONE_ONLY, OXSDRIVER_CCT_NEVER } checkpoint_cleanup;
  enum OxsDriverProblemStatus {
    OXSDRIVER_PS_INVALID,OXSDRIVER_PS_STAGE_START,
    OXSDRIVER_PS_INSIDE_STAGE,OXSDRIVER_PS_STAGE_END,
    OXSDRIVER_PS_DONE } problem_status;
  OxsDriverProblemStatus FloatToProblemStatus(OC_REAL8m ps_float);
  OC_REAL8m ProblemStatusToFloat(OxsDriverProblemStatus ps_enum);
  // Problem Status description
  //
  // STAGE_START: Current state holds magnetization and energy/field
  //   data for the initial state in the stage indicated by the
  //   stage number stored in the state.  This status is used for
  //   the initial state of a problem run, and after interactive
  //   stage adjustment.
  // INSIDE_STAGE: Self-explanatory.  This is the most common status.
  // STAGE_END: Similar to STAGE_START, but in this case the current
  //   state is at the end of previous stage.
  // DONE: End of problem run.
  //
  // Normal run sequence is STAGE_START -> INSIDE_STAGE -> STAGE_END
  //  -> INSIDE_STAGE -> ... -> INSIDE_STAGE -> STAGE_END -> DONE
  // The STAGE_START status is only entered at the start of a sequence
  // either as the start of a problem or if the user interactively
  // changes the stage.
  //
  // A step event is raised when 1) a "good" (i.e., valid) step is
  // taken from INSIDE_STAGE, 2) moving across a stage boundary from
  // STAGE_END to INSIDE_STAGE, and 3) moving into a stage from
  // STAGE_START to INSIDE_STAGE.
  //
  // See the Run() member function for the actual implementation
  // of these rules.
  //
  // problem_status is also used in conjuction with checkpoint_cleanup
  // to decide checkpoint file disposal inside ~Oxs_Driver().
  //

  // Internal "Run" interface.  The difference with the external
  // interface is that the internal version includes a stage_increment
  // parameter for use by the SetStage method.
  void Run(vector<OxsRunEvent>& results,OC_INT4m stage_increment);

  // Starting values.  These shadow fields in Oxs_SimState.
  // Use SetStartValues to copy to Oxs_SimState
  OC_UINT4m start_iteration;
  OC_UINT4m start_stage;
  OC_UINT4m start_stage_iteration;
  OC_REAL8m start_stage_start_time;
  OC_REAL8m start_stage_elapsed_time;
  OC_REAL8m start_last_timestep;

  // Routines defined by child classes to detect stage and
  // problem end events.
  virtual OC_BOOL ChildIsStageDone(const Oxs_SimState& state) const =0;
  virtual OC_BOOL ChildIsRunDone(const Oxs_SimState& state) const =0;

  // Disable copy constructor and assignment operator by declaring
  // them without defining them.
  Oxs_Driver(const Oxs_Driver&);
  Oxs_Driver& operator=(const Oxs_Driver&);

protected:
  Oxs_Driver(const char* name,        // Child instance id
             Oxs_Director* newdtr,    // App director
	     const char* argstr);     // Args

  void SetStartValues(Oxs_SimState& istate) const;

  virtual OC_BOOL Init();  // All children of Oxs_Driver *must* call
  /// this function in their Init() routines.  The main purpose
  /// of this function is to initialize the current state.

public:

  virtual ~Oxs_Driver();

  virtual void StageRequestCount(unsigned int& min,
				 unsigned int& max) const;
  // Number of stages wanted by driver

  virtual Oxs_ConstKey<Oxs_SimState> GetInitialState() const =0;

  const Oxs_SimState* GetCurrentState() const {
    return current_state.GetPtr();
  }

  // Checks state against parent Oxs_Driver class stage/run limiters,
  // and if passes (i.e., not done), then calls ChildStage/RunDone
  // functions.  If this is not enough flexibility, we could make
  // these functions virtual, and move them into the protected:
  // or public: sections, to allow child overrides.
  OC_BOOL IsStageDone(const Oxs_SimState& state) const;
  OC_BOOL IsRunDone(const Oxs_SimState& state) const;

  virtual  OC_BOOL
  Step(Oxs_ConstKey<Oxs_SimState> current_state,
       const Oxs_DriverStepInfo& step_info,
       Oxs_Key<Oxs_SimState>& next_state)=0;
  // Returns true if step was successful, false if
  // unable to step as requested.

  // External problem "Run" interface; called from director.
  void Run(vector<OxsRunEvent>& results) {
    Run(results,1); // Call internal Run interface with default
    // stage increment (==1).
  }

  // GetIteration, SetStage and GetStage throw exceptions on errors.
  OC_UINT4m GetIteration() const;
  void SetStage(OC_UINT4m requestedStage);
  OC_UINT4m GetStage() const;
  OC_UINT4m GetNumberOfStages() const { return number_of_stages; }

  // Special setup for MIF load tests.  (This is part of the regression
  // test harness.)  Limit all stages to 3 steps, and total step limit
  // to 20.
  void LoadTestSetup() {
    stage_iteration_limit.clear();
    stage_iteration_limit.push_back(3);
    total_iteration_limit = 20;
  }

  virtual void FillStateMemberData(const Oxs_SimState& old_state,
                                   Oxs_SimState& new_state) const;
  virtual void FillStateDerivedData(const Oxs_SimState& old_state,
                                    const Oxs_SimState& new_state) const;
  virtual void FillState(const Oxs_SimState& old_state,
                         Oxs_SimState& new_state) const;
  /// FillState is called to fill in a new blank state, using
  ///   an old state as a template.  Specific drivers may have
  ///   supplemental state filling routines as part of a cooperative
  ///   agreement with particular evolver subclasses.
  /// A default implementation is provided.  Child classes should
  ///   either use this method as is, or else call it from the
  ///   overriding method.  Complete replacement is discouraged,
  ///   to make it easier to propagate any future changes in
  ///   the simstate structure.
  /// June 2009: Use of FillState is deprecated.  New code should
  ///   use the separate FillStateMemberData and FillStateDerivedData
  ///   routines.

  virtual void FillNewStageStateMemberData(const Oxs_SimState& old_state,
                                     int new_stage_number,
                                     Oxs_SimState& new_state) const;
  virtual void FillNewStageStateDerivedData(const Oxs_SimState& old_state,
                                     int new_stage_number,
                                     const Oxs_SimState& new_state) const;
  virtual void FillNewStageState(const Oxs_SimState& old_state,
				 int new_stage_number,
				 Oxs_SimState& new_state) const;
  /// FillNewStageState is called to copy a state across a stage
  /// boundary.  A default method is provided.  Child classes should
  /// either use this method as is, or else call it from overriding
  /// class.  Complete replacement is discouraged, to make it easier to
  /// propagate any future changes in simstate structure.
  /// June 2009: Use of FillNewStageState is deprecated.  New code
  ///    should use the separate FillNewStageStateMemberData and
  ///    FillNewStageStateDerivedData routines.

  virtual OC_BOOL InitNewStage(Oxs_ConstKey<Oxs_SimState> state,
                            Oxs_ConstKey<Oxs_SimState> prevstate) =0;
  /// InitNewStage sends first state of new stage to evolver for
  /// any necessary internal bookkeeping.  This call also runs through
  /// the child driver class, so it can update any internal structures
  /// as well.

};

#endif // _OXS_DRIVER
