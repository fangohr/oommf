/* FILE: driver.h                 -*-Mode: c++-*-
 *
 * Abstract driver class; fills states and steps evolvers.
 *
 */

#ifndef _OXS_DRIVER
#define _OXS_DRIVER

#include <string>

#include "oc.h"
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

void OxsDriverCheckpointShutdownHandler(int,ClientData);

class Oxs_Driver: public Oxs_Ext {
public:
  enum OxsDriverProblemStatus {
    OXSDRIVER_PS_INVALID,OXSDRIVER_PS_STAGE_START,
    OXSDRIVER_PS_INSIDE_STAGE,OXSDRIVER_PS_STAGE_END,
    OXSDRIVER_PS_DONE };
  OxsDriverProblemStatus FloatToProblemStatus(OC_REAL8m ps_float);
  OC_REAL8m ProblemStatusToFloat(OxsDriverProblemStatus ps_enum);
private:
  OxsDriverProblemStatus problem_status;
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
  // problem_status is also used in conjuction with checkpoint_disposal
  // to decide checkpoint file disposal inside ~Oxs_Driver().
  //

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
  OC_REAL8m max_absMs; // Maximum value of |Ms| across array.  At present,
  /// Ms is restricted to >=0, so max_absMs is the same as max Ms.

  Oxs_OwnedPointer<Oxs_VectorField> m0; // Initial spin configuration
  OC_REAL8m m0_perturb;  // Amount to perturb initial spin configuration

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
  Oxs_ChunkScalarOutput<Oxs_Driver> aveMx_output;
  Oxs_ChunkScalarOutput<Oxs_Driver> aveMy_output;
  Oxs_ChunkScalarOutput<Oxs_Driver> aveMz_output;
  void Fill__aveM_output_init(const Oxs_SimState&,int);
  void Fill__aveM_output(const Oxs_SimState&,OC_INDEX,OC_INDEX,int);
  void Fill__aveM_output_fini(const Oxs_SimState&,int);
  void Fill__aveM_output_shares(std::vector<Oxs_BaseChunkScalarOutput*>&);
  std::vector<ThreeVector> fill_aveM_output_storage; // Temp storage for
  /// chunk computation; each entry holds the sum for one thread.

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

  // Max angle reporting.  Ideally this is reported by the exchange
  // term, which can determine the max angle for almost free as a
  // natural part of the exchange computations.  But as a fall-back,
  // the user may request the driver to compute and report this
  // infomation.  There are two interfaces: UpdateSpinAngleData which
  // can be called by code outside the normal output call chain, and
  // Fill_maxSpinAng_output(_init//_fini) which is called as part of
  // the chunk scalar output call chain.  The former runs in parallel,
  // but the latter is generally more efficient because it runs not
  // only in parallel but also in chunks mated with other chunk
  // outputs.  The latter interface will set both the cache value in
  // the output objects and the derived data values in the state.  The
  // UpdateSpinAngleData interface sets the derived data values in the
  // state only if they aren't already set.  The output cache values
  // are not set, in keeping with UpdateSpinAngleData() being a const
  // member function.
  OC_BOOL report_max_spin_angle;
  void UpdateSpinAngleData(const Oxs_SimState& state) const;
  Oxs_ChunkScalarOutput<Oxs_Driver> maxSpinAng_output;
  Oxs_ChunkScalarOutput<Oxs_Driver> stage_maxSpinAng_output;
  Oxs_ChunkScalarOutput<Oxs_Driver> run_maxSpinAng_output;
  void Fill__maxSpinAng_output_init(const Oxs_SimState&,int);
  void Fill__maxSpinAng_output(const Oxs_SimState&,OC_INDEX,OC_INDEX,int);
  void Fill__maxSpinAng_output_fini(const Oxs_SimState&,int);
  void Fill__maxSpinAng_output_shares
  (std::vector<Oxs_BaseChunkScalarOutput*>&);
  std::vector<OC_REAL8m> fill_maxSpinAng_output_storage; // Temp storage for
  /// chunk computation; each entry holds the max angle for one thread.

  OC_BOOL normalize_aveM;
  OC_REAL8m scaling_aveM; // For use with regular meshes

  // Optional output that dumps wall clock time to DataTable output
  OC_INT4m report_wall_time;
  OSO_DECL(wall_time);

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

  //////////////////////////////////////////////////////////////////////
  // CHECKPOINTING /////////////////////////////////////////////////////
  // Class for background checkpointing
  friend void OxsDriverCheckpointShutdownHandler(int,ClientData);
  class BackgroundCheckpoint : public Oxs_ThreadThrowaway
  {
    friend void OxsDriverCheckpointShutdownHandler(int,ClientData);
    /// WaitForBackupThread is private to BackgroundCheckpoint, so we
    /// need to friend OxsDriverCheckpointShutdownHandler to it.  But
    /// BackgroundCheckpoint is private to Oxs_Driver, so we need to
    /// friend OxsDriverCheckpointShutdownHandler to Oxs_Driver as
    /// well.
  private:
    Oc_Mutex mutex;
    OC_INDEX checkpoint_writes; // Number of successful backup requests.
    Oxs_ConstKey<Oxs_SimState> backup_request;  // Holding pen
    Oxs_ConstKey<Oxs_SimState> backup_inprogress; // Active backup.
    enum OxsDriverCheckpointModeTypes {
      OXSDRIVER_CMT_INVALID,   // Invalid
      OXSDRIVER_CMT_ENABLED,   // Normal, operating state
      OXSDRIVER_CMT_DISABLED,  // Paused, backup queue dropped
      OXSDRIVER_CMT_FLUSHED,   // Paused, backup queue flushed to disk
      OXSDRIVER_CMT_SHUTDOWN   // Same as DISABLED, but can't be re-ENABLED.
    } checkpoint_mode;
    /// NB: No code should modify backup_inprogress other than Task().
    /// checkpoint_writes, backup_request, backup_inprogress, and
    /// checkpoint_mode should only be accessed through mutex.

    OC_BOOL WaitForBackupThread(unsigned int timeout,
                                OxsDriverCheckpointModeTypes newmode);
    /// Wait for up to "timeout" seconds for backup thread, if any, to
    /// finish.  The checkpoint_mode is set to the import newmode,
    /// which must be either OXSDRIVER_CMT_DISABLED (for temporary
    /// disabling) or OXSDRIVER_CMT_SHUTDOWN (for permanent closure).
    /// Return value is true on success, false if a backup thread is
    /// either running or stalled.

    Oxs_Director* const director;
    String checkpoint_filename;
    String checkpoint_filename_full; // Full name with path
    String checkpoint_filename_tmpA; // Temp file names.  Note that the same
    String checkpoint_filename_tmpB; // temp files are used for all saves.
    double checkpoint_interval; // In wall-clock seconds.
                               /// Negative values disable checkpointing.
    Oc_Ticks checkpoint_time;   // Time of last checkpoint commit.
    OC_UINT4m checkpoint_id;    // Id of last checkpoint commit.

    enum OxsDriverCheckpointDisposalTypes {
      OXSDRIVER_CDT_INVALID, OXSDRIVER_CDT_STANDARD,
      OXSDRIVER_CDT_DONE_ONLY, OXSDRIVER_CDT_NEVER
    } checkpoint_disposal;

    // Declare but don't define the following members
    BackgroundCheckpoint(const BackgroundCheckpoint&);
    BackgroundCheckpoint& operator=(const BackgroundCheckpoint&);

  protected:
    void Task();

  public:
    // Maximum number of simstates that may be held by this class at one
    // time.
    int ReserveStateCount() const { return 2; }

    BackgroundCheckpoint(Oxs_Director* in_dtr)
      : Oxs_ThreadThrowaway("Oxs_Driver::BackgroundCheckpoint"),
        checkpoint_writes(0), checkpoint_mode(OXSDRIVER_CMT_INVALID),
        director(in_dtr),
        checkpoint_interval(0),checkpoint_id(0),
        checkpoint_disposal(OXSDRIVER_CDT_INVALID) {
      director->ReserveSimulationStateRequest(ReserveStateCount());
      checkpoint_time.ReadWallClock();
    }

    ~BackgroundCheckpoint();

    OC_BOOL TestKeepCheckpoint(OxsDriverProblemStatus probstatus) {
      // Returns 1 if checkpoint should be kept, 0 otherwise.
      if(checkpoint_writes==0) return 0;  // No checkpoint written
      if(checkpoint_disposal==OXSDRIVER_CDT_STANDARD) {
        // STANDARD behavior is to delete checkpoint if possible
        return 0;
      }
      if(checkpoint_disposal==OXSDRIVER_CDT_DONE_ONLY
         && probstatus==OXSDRIVER_PS_DONE) {
        return 0;
      }
      return 1; // Keep checkpoint in any abnormal circumstance
    }

    const char* GetDisposal() const;
    void SetDisposal(const String& in_disposal);

    // Note: Interval times in (wall-clock) seconds
    double GetInterval() const { return checkpoint_interval; }
    void SetInterval(double in_interval) {
      checkpoint_interval = in_interval;
    }

    // Wall time of last checkpoint commit
    Oc_Ticks GetTicks() const {
      return checkpoint_time;
    }

    void Init(const String& in_filename,
              double in_interval,const String& in_disposal);

    void Reset(const Oxs_SimState* start_state) {
      Oc_RemoveSigTermHandler(OxsDriverCheckpointShutdownHandler,this);
      WaitForBackupThread(100,OXSDRIVER_CMT_DISABLED);
      {
        Oc_LockGuard lck(mutex);
        // Note: The following resets might not "take" if
        // WaitForBackupThread failed.  But this is probably not worth
        // worrying about.
        checkpoint_writes = 0;
        if(checkpoint_mode != OXSDRIVER_CMT_DISABLED) {
          throw Oxs_BadCode("checkpoint_mode in improper state");
        }
        checkpoint_mode = OXSDRIVER_CMT_ENABLED;
      }

      if(start_state) {
        // Insure that the start state is not saved as a checkpoint.
        checkpoint_id = start_state->Id();
      }
      checkpoint_time.ReadWallClock();
      Oc_AppendSigTermHandler(OxsDriverCheckpointShutdownHandler,this);
    }

    void WrapUp(OxsDriverProblemStatus probstatus,OC_BOOL flush_queue);

    const char* CheckpointFilename() const {
      return checkpoint_filename.c_str();
    }

    const char* CheckpointFullFilename() const {
      // If checkpointing is enabled, then return the absolute path to
      // the checkpoint file.  Otherwise, return an empty string.
      if(checkpoint_interval >= 0.0) {
        return checkpoint_filename_full.c_str();
      } else {
        return "";
      }
    }

    // RequestBackup queues state onto backup stack unconditionally, and
    // does not adjust checkpoint_time or checkpoint_id.  UpdateBackup
    // checks checkpoint_time and checkpoint_id; if sufficient time has
    // elapsed and the id has changed, then checkpoint_time and
    // checkpoint_id are updated and the state is sent to RequestBackup.
    // Return from UpdateBackup is 1 if RequestBackup is called,
    // otherwise 0.
    void RequestBackup(Oxs_ConstKey<Oxs_SimState>& statekey);
    int UpdateBackup(Oxs_ConstKey<Oxs_SimState>& statekey);
  }  bgcheckpt;  // Restart file control

  // CHECKPOINTING /////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////

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
       Oxs_ConstKey<Oxs_SimState>& next_state)=0;
  // Returns true if step was successful, false if
  // unable to step as requested.

  // External problem "Run" interface; called from director.
  void Run(vector<OxsRunEvent>& results) {
    Run(results,1); // Call internal Run interface with default
    // stage increment (==1).
  }

  // GetIteration, GetCurrentStateId, SetStage and GetStage throw
  // exceptions on errors.
  OC_UINT4m GetIteration() const;
  OC_UINT4m GetCurrentStateId() const;
  void SetStage(OC_UINT4m requestedStage,
                vector<OxsRunEvent>& events);

  OC_UINT4m GetStage() const;
  OC_UINT4m GetNumberOfStages() const { return number_of_stages; }

  const char* GetCheckpointFilename() const {
    return bgcheckpt.CheckpointFullFilename();
  }

  const char* GetCheckpointDisposal() const {
    return bgcheckpt.GetDisposal();
  }
  void SetCheckpointDisposal(const String& in_disposal) {
    bgcheckpt.SetDisposal(in_disposal);
  }

  // Note: bgcheckpt tracks intervals in seconds.  The
  // driver interface tracks minutes.
  double GetCheckpointInterval() const {
    return bgcheckpt.GetInterval()/60.;
  }
  void SetCheckpointInterval(double in_interval) {
    bgcheckpt.SetInterval(in_interval*60.);
  }

  Oc_Ticks GetCheckpointTicks() const {
    return bgcheckpt.GetTicks();
  }

  // Special setup for MIF load tests.  (This is part of the regression
  // test harness.)  Limit all stages to 5 steps, and total step limit
  // to 35.
  void LoadTestSetup() {
    for(size_t i=0;i<stage_iteration_limit.size();++i) {
      if(stage_iteration_limit[i]==0 || stage_iteration_limit[i]>5) {
        stage_iteration_limit[i] = 5;
      }
    }
    if(total_iteration_limit==0 || total_iteration_limit>35) {
      total_iteration_limit = 35;
    }
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
#if 0
  virtual void FillNewStageState(const Oxs_SimState& old_state,
				 int new_stage_number,
				 Oxs_SimState& new_state) const
  { // Deprecated routine.  New code should use the separate
    // FillNewStageStateMemberData and FillNewStageStateDerivedData
    // functions.
    FillNewStageStateMemberData(old_state,new_stage_number,new_state);
    FillNewStageStateDerivedData(old_state,new_stage_number,new_state);
  }
#endif
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
  /// InitNewStage sends first state of new stage to the evolver for
  /// any necessary internal bookkeeping.  This call also runs through
  /// the child driver class, so it can update any internal structures
  /// as well.

};

#endif // _OXS_DRIVER
