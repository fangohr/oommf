/* FILE: driver.cc                 -*-Mode: c++-*-
 *
 * Abstract driver class; fills states and initiates steps.
 * Also registers the command "Oxs_Run" with the Tcl intepreter.
 *
 */

#include <map>
#include <string>
#include <vector>

#include "oc.h"
#include "director.h"
#include "driver.h"
#include "key.h"
#include "scalarfield.h"
#include "simstate.h"
#include "util.h"
#include "energy.h"     // Needed to make MSVC++ 5 happy
#include "oxswarn.h"
#include "vectorfield.h"

/* End includes */

// Revision information, set via CVS keyword substitution
static const Oxs_WarningMessageRevisionInfo revision_info
  (__FILE__,
   "$Revision: 1.92 $",
   "$Date: 2012-09-06 23:46:03 $",
   "$Author: donahue $",
   "Michael J. Donahue (michael.donahue@nist.gov)");

// Conversion support code for OxsDriverProblemStatus enum
OC_REAL8m
Oxs_Driver::ProblemStatusToFloat
(OxsDriverProblemStatus ps_enum)
{
  OC_REAL8m ps_float;
  switch(ps_enum) {
  case OXSDRIVER_PS_INVALID:
  case OXSDRIVER_PS_STAGE_START:
  case OXSDRIVER_PS_INSIDE_STAGE:
  case OXSDRIVER_PS_STAGE_END:
  case OXSDRIVER_PS_DONE:
    ps_float = static_cast<OC_REAL8m>(static_cast<int>(ps_enum));
    break;
  default: {
      char buf[1024];
      Oc_Snprintf(buf,sizeof(buf),
                  "Programming error? Unrecognized problem status: %d",
                  static_cast<int>(ps_enum));
      throw Oxs_ExtError(this,buf);
    }
  }
  return ps_float;
}


Oxs_Driver::OxsDriverProblemStatus
Oxs_Driver::FloatToProblemStatus
(OC_REAL8m ps_float)
{
  int ps_int = static_cast<int>(ps_float);
  if(static_cast<OC_REAL8m>(ps_int)!=ps_float) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),"Input error?"
                "  Problem status is non-integral: %g",
                static_cast<double>(ps_float));
    throw Oxs_ExtError(this,String(buf));
  }
  OxsDriverProblemStatus ps_enum;
  switch(static_cast<OxsDriverProblemStatus>(ps_int)) {
  case OXSDRIVER_PS_INVALID:
    ps_enum = OXSDRIVER_PS_INVALID;      break;
  case OXSDRIVER_PS_STAGE_START:
    ps_enum = OXSDRIVER_PS_STAGE_START;  break;
  case OXSDRIVER_PS_INSIDE_STAGE:
    ps_enum = OXSDRIVER_PS_INSIDE_STAGE; break;
  case OXSDRIVER_PS_STAGE_END:
    ps_enum = OXSDRIVER_PS_STAGE_END;    break;
  case OXSDRIVER_PS_DONE:
    ps_enum = OXSDRIVER_PS_DONE;         break;
  default: {
      char buf[1024];
      Oc_Snprintf(buf,sizeof(buf),
                  "Input error? Unrecognized problem status: %d",
                  static_cast<int>(ps_int));
      throw Oxs_ExtError(this,String(buf));
    }
  }
  return ps_enum;
}


#define OSO_INIT(name,descript,units) \
   name##_output.Setup(this,InstanceName(),descript,units,0, \
           &Oxs_Driver::Fill__##name##_output)
// Constructor
Oxs_Driver::Oxs_Driver
( const char* name,        // Child instance id
  Oxs_Director* newdtr,    // App director
  const char* argstr       // Args
  ) : Oxs_Ext(name,newdtr,argstr),
      report_max_spin_angle(0),normalize_aveM(1),scaling_aveM(1.0),
      number_of_stages(1),
      checkpoint_id(0),checkpoint_writes(0),checkpoint_interval(0.),
      checkpoint_cleanup(OXSDRIVER_CCT_INVALID),
      problem_status(OXSDRIVER_PS_INVALID),
      start_iteration(0),start_stage(0),start_stage_iteration(0),
      start_stage_start_time(0.),start_stage_elapsed_time(0.),
      start_last_timestep(0.)
{
  // Reserve state space in director
  director->ReserveSimulationStateRequest(2);


  //////////////////////////////////////////////////////////////////////
  /////////////////////// DEPRECATED FIELDS ////////////////////////////
  // Backward compatibility is provided by the Oxs_Mif class
  // (see oxs/base/mif.tcl), which yanks these keys out of the
  // driver init string and moves them to the "option" array.
#if 0
  // Attribute strings
  GetStringInitValue("basename","oxs");
  GetStringInitValue("vector_field_output_format","binary 8");
  GetStringInitValue("vector_field_output_meshtype","rectangular");
  GetStringInitValue("vector_field_output_writeheaders","1");
  GetStringInitValue("scalar_output_format","%.17g");
  GetStringInitValue("scalar_output_writeheaders","1");
#endif
  /////////////////////// DEPRECATED FIELDS ////////////////////////////
  //////////////////////////////////////////////////////////////////////

  String basename = "oxs";
  director->GetMifOption("basename",basename);

  // Restart (checkpoint) file, interval and clean up behavior.
  // The interval time is in minutes.  '0' means save on each
  // step, <0 means no checkpointing.
  checkpoint_file = GetStringInitValue("checkpoint_file",
                                       basename + String(".restart"));
  checkpoint_interval = 60*GetRealInitValue("checkpoint_interval",15.0);
  /// Convert from minutes to seconds.
  String cp_cleanup = GetStringInitValue("checkpoint_cleanup",
                                         "normal");
  /// Checkpoint cleanup options: normal, done_only, never
  if(cp_cleanup.compare("normal")==0) {
    checkpoint_cleanup = OXSDRIVER_CCT_NORMAL;
  } else if(cp_cleanup.compare("done_only")==0) {
    checkpoint_cleanup = OXSDRIVER_CCT_DONE_ONLY;
  } else if(cp_cleanup.compare("never")==0) {
    checkpoint_cleanup = OXSDRIVER_CCT_NEVER;
  } else {
    String msg=String("Invalid checkpoint_cleanup request: ")
      + cp_cleanup
      + String("\n Should be one of normal, done_only, or never.");
    throw Oxs_ExtError(this,msg);
  }

  OXS_GET_INIT_EXT_OBJECT("mesh",Oxs_Mesh,mesh_obj);
  mesh_key.Set(mesh_obj.GetPtr());  // Sets a dep lock

  Oxs_OwnedPointer<Oxs_ScalarField> Msinit;
  OXS_GET_INIT_EXT_OBJECT("Ms",Oxs_ScalarField,Msinit);

  OXS_GET_INIT_EXT_OBJECT("m0",Oxs_VectorField,m0);

  // Fill Ms and Ms_inverse array, and verify that Ms is non-negative.
  Msinit->FillMeshValue(mesh_obj.GetPtr(),Ms);
  Ms_inverse.AdjustSize(mesh_obj.GetPtr());
  for(OC_INDEX icell=0;icell<mesh_obj->Size();icell++) {
    if(Ms[icell]<0.0) {
      char buf[1024];
      Oc_Snprintf(buf,sizeof(buf),
                  "Negative Ms value (%g) detected at mesh index %u.",
                  static_cast<double>(Ms[icell]),icell);
      throw Oxs_ExtError(this,String(buf));
    } else if(Ms[icell]==0.0) {
      Ms_inverse[icell]=0.0; // Special case handling
    } else {
      Ms_inverse[icell]=1.0/Ms[icell];
    }
  }

  // Report max spin angle?  This value is preferably reported by
  // the exchange term, but can be computed by the driver if desired.
  if(GetIntInitValue("report_max_spin_angle",0)) {
    report_max_spin_angle = 1;
  } else {
    report_max_spin_angle = 0;
  }

  // Should Mx, My and Mz outputs be normalized?
  if(GetIntInitValue("normalize_aveM_output",1)) {
    normalize_aveM = 1;
  } else {
    normalize_aveM = 0;
  }

  // Iteration count and stage limits
  if(HasInitValue("stage_iteration_limit")) {
    // Optional; default is no control
    GetGroupedUIntListInitValue("stage_iteration_limit",
                                stage_iteration_limit);
  } else {
    stage_iteration_limit.push_back(0); // Default: no control
  }
  total_iteration_limit = GetUIntInitValue("total_iteration_limit",0);
  stage_count_request = GetUIntInitValue("stage_count",0);
  stage_count_check = GetIntInitValue("stage_count_check",1);

  // Start values
  start_iteration = GetUIntInitValue("start_iteration",0);
  start_stage = GetUIntInitValue("start_stage",0);
  start_stage_iteration = GetUIntInitValue("start_stage_iteration",0);
  start_stage_start_time = GetRealInitValue("start_stage_start_time",0.0);
  start_stage_elapsed_time = GetRealInitValue("start_stage_elapsed_time",0.0);
  start_last_timestep = GetRealInitValue("start_last_timestep",0.0);

  // Optional projection outputs
  map<String,String> projection_options; // Maps projection output
  /// names to options.
  if(HasInitValue("projection_options")) {
    Nb_SplitList pieces;
    String projopts = GetStringInitValue("projection_options");
    if(pieces.Split(projopts)!=TCL_OK) {
      char bit[4000];
      Oc_EllipsizeMessage(bit,sizeof(bit),projopts.c_str());
      char temp_buf[4500];
      Oc_Snprintf(temp_buf,sizeof(temp_buf),
                  "Format error in projection_options subblock---"
                  "block is not a proper Tcl list: %.4000s",
                  bit);
      throw Oxs_ExtError(this,temp_buf);
    }
    if(pieces.Count()%2!=0) {
      char temp_buf[1024];
      Oc_Snprintf(temp_buf,sizeof(temp_buf),
                  "Format error in projection_options subblock---"
                  "block should be composed of name+options pairs,"
                  " but element count is %d, which is not even.",
                  pieces.Count());
      throw Oxs_ExtError(this,temp_buf);
    }
    const int item_count = pieces.Count();
    for(int item=0;item<item_count;item+=2) {
      if(strcmp(pieces[item],"comment")==0) continue; // Ignore comments
      map<String,String>::const_iterator it;
      it = projection_options.find(pieces[item]);
      if(it != projection_options.end()) {
        char bit[4000];
        Oc_EllipsizeMessage(bit,sizeof(bit),pieces[item]);
        char temp_buf[4500];
        Oc_Snprintf(temp_buf,sizeof(temp_buf),
                    "Format error in projection_options subblock---"
                    "label \"%.4000s\" appears more than once.",
                    bit);
        throw Oxs_ExtError(this,temp_buf);
      }
      projection_options[pieces[item]] = pieces[item+1];
    }
  }
  if(HasInitValue("projection_outputs")) {
    Nb_SplitList pieces;
    String projouts = GetStringInitValue("projection_outputs");
    if(pieces.Split(projouts)!=TCL_OK) {
      char bit[4000];
      Oc_EllipsizeMessage(bit,sizeof(bit),projouts.c_str());
      char temp_buf[4500];
      Oc_Snprintf(temp_buf,sizeof(temp_buf),
                  "Format error in projection_outputs subblock---"
                  "block is not a proper Tcl list: %.4000s",
                  bit);
      throw Oxs_ExtError(this,temp_buf);
    }
    if(pieces.Count()%2!=0) {
      char temp_buf[1024];
      Oc_Snprintf(temp_buf,sizeof(temp_buf),
                  "Format error in projection_outputs subblock---"
                  "block should be composed of name+vector_field pairs,"
                  " but element count is %d, which is not even.",
                  pieces.Count());
      throw Oxs_ExtError(this,temp_buf);
    }
    const OC_INDEX piece_count = pieces.Count();
    if(piece_count>0) {
      OC_INDEX i;
      OC_INDEX item_index = 0;
      for(i=0;i<piece_count;i+=2) {
        // Ignore "comment" labels
        if(strcmp(pieces[i],"comment")!=0) item_index++;
      }
      projection_output.SetSize(item_index);
      for(i=0,item_index=0;i<piece_count;i+=2) {
        if(strcmp(pieces[i],"comment")==0) continue;
        OxsDriverProjectionOutput& po = projection_output[item_index++];
        po.name = pieces[i]; // Output name

        // Output trellis initializer
        Nb_SplitList tmpparams;
        if(tmpparams.Split(pieces[i+1])!=TCL_OK) {
          char bit1[4000];
          Oc_EllipsizeMessage(bit1,sizeof(bit1),pieces[i]);
          char bit2[4000];
          Oc_EllipsizeMessage(bit2,sizeof(bit2),pieces[i+1]);
          char temp_buf[8500];
          Oc_Snprintf(temp_buf,sizeof(temp_buf),
                      "Format error in projection_outputs subblock---"
                      "vector field spec for output \"%.4000s\" is not"
                      " a proper Tcl list: %.4000s",
                      bit1,bit2);
          throw Oxs_ExtError(this,temp_buf);
        }
        tmpparams.FillParams(po.trellis_init);

        // Output options
        OC_BOOL units_set=0;
        map<String,String>::iterator it;
        it = projection_options.find(po.name);
        if(it != projection_options.end()) {
          Nb_SplitList tmpoptions;
          if(tmpoptions.Split(it->second)!=TCL_OK) {
            char bit1[4000];
            Oc_EllipsizeMessage(bit1,sizeof(bit1),it->first.c_str());
            char bit2[4000];
            Oc_EllipsizeMessage(bit2,sizeof(bit2),it->second.c_str());
            char temp_buf[8500];
            Oc_Snprintf(temp_buf,sizeof(temp_buf),
                        "Format error in projection_options subblock---"
                        "options for output \"%.4000s\" is not a proper"
                        " Tcl list: %.4000s",
                        bit1,bit2);
            throw Oxs_ExtError(this,temp_buf);
          }
          if(tmpoptions.Count()%2!=0) {
            char bit[4000];
            Oc_EllipsizeMessage(bit,sizeof(bit),po.name.c_str());
            char temp_buf[4500];
            Oc_Snprintf(temp_buf,sizeof(temp_buf),
                        "Format error in projection_options subblock---"
                        "value subentries should be composed of "
                        "option+value pairs, but the value associated "
                        "with name \"%.4000s\" has %d elements, which is "
                        "not even.",bit,tmpoptions.Count());
            throw Oxs_ExtError(this,temp_buf);
          }
          for(int j=0;j<tmpoptions.Count();j+=2) {
            if(strcmp("normalize",tmpoptions[j])==0) {
              char* endptr;
              long int result = strtol(tmpoptions[j+1],&endptr,10);
              if(endptr == tmpoptions[j+1] || *endptr!='\0') {
                char bit1[4000];
                Oc_EllipsizeMessage(bit1,sizeof(bit1),tmpoptions[j+1]);
                char bit2[4000];
                Oc_EllipsizeMessage(bit2,sizeof(bit2),po.name.c_str());
                char temp_buf[8500];
                Oc_Snprintf(temp_buf,sizeof(temp_buf),
                            "Format error in projection_options subblock---"
                            "\"normalize\" option value \"%.4000s\" associated"
                            " with name \"%.4000s\" is not an integer.",
                            bit1,bit2);
                throw Oxs_ExtError(this,temp_buf);
              }
              if(result==0) po.normalize=0;
              else          po.normalize=1;
            } else if(strcmp("scaling",tmpoptions[j])==0) {
              OC_BOOL error;
              OC_REAL8m result = Nb_Atof(tmpoptions[j+1],error);
              if(error) {
                char bit1[4000];
                Oc_EllipsizeMessage(bit1,sizeof(bit1),tmpoptions[j+1]);
                char bit2[4000];
                Oc_EllipsizeMessage(bit2,sizeof(bit2),po.name.c_str());
                char temp_buf[8500];
                Oc_Snprintf(temp_buf,sizeof(temp_buf),
                  "Format error in projection_options subblock---"
                  "\"scaling\" option value \"%.4000s\" associated with"
                  " name \"%.4000s\" is not a floating point value.",
                  bit1,bit2);
                throw Oxs_ExtError(this,temp_buf);
              }
              po.user_scaling = result;
            } else if(strcmp("units",tmpoptions[j])==0) {
              po.units = tmpoptions[j+1];
              units_set = 1;
            } else {
              char bit1[4000];
              Oc_EllipsizeMessage(bit1,sizeof(bit1),tmpoptions[j]);
              char bit2[4000];
              Oc_EllipsizeMessage(bit2,sizeof(bit2),po.name.c_str());
              char temp_buf[8500];
              Oc_Snprintf(temp_buf,sizeof(temp_buf),
                          "Format error in projection_options"
                          " subblock---option \"%.4000s\" associated"
                          " with name \"%.4000s\" is unknown.",
                          bit1,bit2);
              throw Oxs_ExtError(this,temp_buf);
            }
          }
          projection_options.erase(it);
        }
        if(!units_set) {
          // Guess units based on normalization settings
          if(!normalize_aveM) po.units = "A/m";
          else                po.units = ""; // Best guess
        }
      }
    }
  }
  if(!projection_options.empty()) {
    String msg = "Error in projection_options subblock: "
      "the following projection_outputs are unknown---";
    map<String,String>::const_iterator it;
    for(it=projection_options.begin();it!=projection_options.end();++it) {
      msg += "\n   ";
      msg += it->first;
    }
    throw Oxs_ExtError(this,msg);
  }

  // Setup outputs
  OSO_INIT(iteration_count,"Iteration","");
  OSO_INIT(stage_iteration_count,"Stage iteration","");
  OSO_INIT(stage_number,"Stage","");
  spin_output.Setup(this,InstanceName(),"Spin","",1,
                    &Oxs_Driver::Fill__spin_output);
  magnetization_output.Setup(this,InstanceName(),
                             "Magnetization","A/m",1,
                             &Oxs_Driver::Fill__magnetization_output);
  if(normalize_aveM) {
    aveMx_output.Setup(this,InstanceName(),"mx","",1,
                       &Oxs_Driver::Fill__aveM_output);
    aveMy_output.Setup(this,InstanceName(),"my","",1,
                       &Oxs_Driver::Fill__aveM_output);
    aveMz_output.Setup(this,InstanceName(),"mz","",1,
                       &Oxs_Driver::Fill__aveM_output);
    if(report_max_spin_angle) {
      maxSpinAng_output.Setup(this,InstanceName(),"Max Spin Ang","deg",1,
                              &Oxs_Driver::Fill__maxSpinAng_output);
    }
  } else {
    aveMx_output.Setup(this,InstanceName(),"Mx","A/m",1,
                       &Oxs_Driver::Fill__aveM_output);
    aveMy_output.Setup(this,InstanceName(),"My","A/m",1,
                       &Oxs_Driver::Fill__aveM_output);
    aveMz_output.Setup(this,InstanceName(),"Mz","A/m",1,
                       &Oxs_Driver::Fill__aveM_output);
    if(report_max_spin_angle) {
      maxSpinAng_output.Setup(this,InstanceName(),"Max Spin Ang","deg",1,
                              &Oxs_Driver::Fill__maxSpinAng_output);
    }
  }
  if(report_max_spin_angle) {
    stage_maxSpinAng_output.Setup(this,InstanceName(),
                                  "Stage Max Spin Ang","deg",1,
                                  &Oxs_Driver::Fill__maxSpinAng_output);
    run_maxSpinAng_output.Setup(this,InstanceName(),
                                "Run Max Spin Ang","deg",1,
                                &Oxs_Driver::Fill__maxSpinAng_output);
  }

  iteration_count_output.Register(director,0);
  stage_iteration_count_output.Register(director,0);
  stage_number_output.Register(director,0);
  spin_output.Register(director,0);
  magnetization_output.Register(director,0);
  aveMx_output.Register(director,0);
  aveMy_output.Register(director,0);
  aveMz_output.Register(director,0);
  if(report_max_spin_angle) {
    maxSpinAng_output.Register(director,0);
    stage_maxSpinAng_output.Register(director,0);
    run_maxSpinAng_output.Register(director,0);
  }

  const OC_INDEX projection_count = projection_output.GetSize();
  for(OC_INDEX ipo=0;ipo<projection_count;++ipo) {
    OxsDriverProjectionOutput& po = projection_output[ipo];
    Oxs_ScalarOutput<Oxs_Driver>& output = po.output;
    output.Setup(this,InstanceName(),po.name.c_str(),po.units.c_str(),
                 0,&Oxs_Driver::Fill__projection_outputs);
    po.output.Register(director,0);
  }

}

//Destructor
Oxs_Driver::~Oxs_Driver()
{
  if(director->GetErrorStatus()==0 && checkpoint_writes!=0) {
    // Check that checkpoint_writes is not 0 to insure that a checkpoint
    // has been written during this run.  This is not to insure the
    // existence of a checkpoint file (which anyway may have failed
    // to write or been removed by a third party), but rather to
    // protect against the accidental loading of a file w/o the
    // restart_flag set causing the checkpoint file to be
    // automatically destroyed when the user tries to reload the
    // problem.
    static Oxs_WarningMessage noremove(3);
    switch(checkpoint_cleanup) {
      case OXSDRIVER_CCT_INVALID:
        break;
      case OXSDRIVER_CCT_NORMAL:
        if(Nb_FileExists(checkpoint_file.c_str())
           && Nb_Remove(checkpoint_file.c_str())!=0) {
          String msg = String("Unable to remove checkpoint file \"")
            + checkpoint_file
            + String("\".");
          noremove.Send(revision_info,OC_STRINGIFY(__LINE__),msg.c_str());
        }
        break;
      case OXSDRIVER_CCT_DONE_ONLY:
        if(problem_status==OXSDRIVER_PS_DONE) {
          if(Nb_FileExists(checkpoint_file.c_str())
             && Nb_Remove(checkpoint_file.c_str())!=0) {
            String msg = String("Unable to remove checkpoint file \"")
              + checkpoint_file
              + String("\".");
            noremove.Send(revision_info,OC_STRINGIFY(__LINE__),msg.c_str());
          }
        }
        break;
      case OXSDRIVER_CCT_NEVER:
        break;
      }
    checkpoint_writes = 0; // Safety
  }
#if REPORT_TIME
  Oc_TimeVal cpu,wall;
  driversteptime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"Full Step time (secs)%7.2f cpu /%7.2f wall,"
            " module %.1000s (%u iterations)\n",
            double(cpu),double(wall),InstanceName(),GetIteration());
  }
#endif // REPORT_TIME
}


// The following routine is called by GetInitialState() in child classes.
void Oxs_Driver::SetStartValues (Oxs_SimState& istate) const
{
  OC_BOOL fresh_start = 1;
  int rflag = director->GetRestartFlag();
  if(rflag!=0) {
    // If checkpoint file can be opened for reading,
    // then restore state from there.  Otherwise either
    // throw an error if rflag = 1, or else use
    // start (initial) state values.
    FILE *check = Nb_FOpen(checkpoint_file.c_str(),"r");
    if(check!=NULL) {
      fclose(check);
      String MIF_info;
      istate.RestoreState(checkpoint_file.c_str(),
                          mesh_key.GetPtr(),&Ms,&Ms_inverse,
                          director,MIF_info);
      fresh_start = 0;
    } else if(rflag==1) {
      char bit[4000];
      Oc_EllipsizeMessage(bit,sizeof(bit),checkpoint_file.c_str());
      char buf[4500];
      Oc_Snprintf(buf,sizeof(buf),
                  "Unable to open requested checkpoint file"
                  " \"%.4000s\"",bit);
      throw Oxs_ExtError(this,String(buf));
    }
  }
  if(fresh_start) {
    istate.previous_state_id = 0;
    istate.iteration_count       = start_iteration;
    istate.stage_number          = start_stage;
    istate.stage_iteration_count = start_stage_iteration;
    istate.stage_start_time      = start_stage_start_time;
    istate.stage_elapsed_time    = start_stage_elapsed_time;
    istate.last_timestep         = start_last_timestep;
    istate.mesh = mesh_key.GetPtr();
    istate.Ms = &Ms;
    istate.Ms_inverse = &Ms_inverse;
    m0->FillMeshValue(istate.mesh,istate.spin);
    // Insure that all spins are unit vectors
    OC_INDEX size = istate.spin.Size();
    for(OC_INDEX i=0;i<size;i++) istate.spin[i].MakeUnit();
  }
}

// Oxs_Driver version of Init().  All children of Oxs_Driver *must*
// call this function in their Init() routines.  The main purpose
// of this function is to setup base driver outputs and to initialize
// the current state.
OC_BOOL Oxs_Driver::Init()
{
  if(!Oxs_Ext::Init()) return 0;

  OC_BOOL success=1;
  problem_status = OXSDRIVER_PS_INVALID; // Safety

#if REPORT_TIME
  Oc_TimeVal cpu,wall;
  driversteptime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"Full Step time (secs)%7.2f cpu /%7.2f wall,"
            " module %.1000s (%u iterations)\n",
            double(cpu),double(wall),InstanceName(),GetIteration());
  }
  driversteptime.Reset();
#endif // REPORT_TIME

  // Finish output initializations.
  if(!mesh_obj->HasUniformCellVolumes()) {
    // Magnetization averaging should be weighted by cell volume.  At
    // present, however, the only available mesh is
    // Oxs_RectangularMesh, which has uniform cell volumes.  The
    // computation in this case can be faster, so for now we code only
    // for that case.  Check and throw an error, though, so we will be
    // reminded to change this if new mesh types become available in
    // the future.
    throw Oxs_ExtError(this,"NEW CODE REQUIRED: Current Oxs_Driver"
                         " aveM and projection outputs require meshes "
                         " with uniform cell sizes, such as "
                         "Oxs_RectangularMesh.");
  }
  if(normalize_aveM) {
    const OC_INDEX mesh_size = Ms.Size();
    OC_REAL8m sum = 0.0;
    for(OC_INDEX j=0;j<mesh_size;++j) sum += fabs(Ms[j]);
    if(sum>0.0) scaling_aveM = 1.0/sum;
    else        scaling_aveM = 1.0; // Safety
  } else {
    if(Ms.Size()>0) scaling_aveM = 1.0/static_cast<OC_REAL8m>(Ms.Size());
    else            scaling_aveM = 1.0; // Safety
  }

  const OC_INDEX projection_count = projection_output.GetSize();
  for(OC_INDEX i=0;i<projection_count;++i) {
    OxsDriverProjectionOutput& po = projection_output[i];

    // Fill projection trellis with vector fields sized to mesh
    Oxs_MeshValue<ThreeVector>& trellis = po.trellis;
    Oxs_OwnedPointer<Oxs_VectorField> tmpinit; // Initializer
    OXS_GET_EXT_OBJECT(po.trellis_init,Oxs_VectorField,tmpinit);
    tmpinit->FillMeshValue(mesh_obj.GetPtr(),trellis);

    // Adjust scaling
    po.scaling = 1.0; // Safety
    if(po.normalize) {
      const OC_INDEX mesh_size = trellis.Size();
      OC_REAL8m sum = 0.0;
      if(normalize_aveM) {
        for(OC_INDEX j=0;j<mesh_size;++j) {
          sum += fabs(Ms[j])*sqrt(trellis[j].MagSq());
        }
      } else {
        for(OC_INDEX j=0;j<mesh_size;++j) {
          sum += sqrt(trellis[j].MagSq());
        }
      }
      if(sum>0.0) po.scaling = 1.0/sum;
    } else {
      po.scaling = scaling_aveM;
    }
    po.scaling *= po.user_scaling;
  }

  // Adjust spin output to always use full precision
  String default_format = spin_output.GetOutputFormat();
  Nb_SplitList arglist;
  if(arglist.Split(default_format.c_str())!=TCL_OK) {
    char bit[4000];
    Oc_EllipsizeMessage(bit,sizeof(bit),default_format.c_str());
    char temp_buf[4500];
    Oc_Snprintf(temp_buf,sizeof(temp_buf),
                "Format error in spin output format string---"
                "not a proper Tcl list: %.4000s",
                bit);
    throw Oxs_ExtError(this,temp_buf);
  }
  if(arglist.Count()!=2) {
    OXS_THROW(Oxs_ProgramLogicError,
              "Wrong number of arguments in spin output format string, "
              "detected in Oxs_Driver Init");
  } else {
    vector<String> sarr;
    sarr.push_back(arglist[0]); // Data type
    if(sarr[0].compare("binary") == 0) {
      sarr.push_back("8");      // Precision
    } else {
      sarr.push_back("%.17g");
    }
    String precise_format = Nb_MergeList(&sarr);
    spin_output.SetOutputFormat(precise_format.c_str());
  }

  // Determine total stage count requirements
  unsigned int min,max;
  director->ExtObjStageRequestCounts(min,max);
  if(stage_count_check!=0 && min>max) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
                "Stage count request incompatibility detected;"
                " request range is [%u,%u].  Double check stage"
                " lists and applied field specifications.  The"
                " stage count compatibility check may be disabled"
                " in the driver Specify block by setting"
                " stage_count_check to 0.",
                min,max);
    throw Oxs_ExtError(this,String(buf));
  }

  // Parameter stage_count_request overrides all automatic settings
  // if set to a value different from 0.  Otherwise, use maximal
  // "min" value requested by all ext objects, unless that value is
  // zero, in which case we use a 1 stage default.
  if(stage_count_request>0) number_of_stages = stage_count_request;
  else                      number_of_stages = min;
  if(number_of_stages<1) number_of_stages=1; // Default.


  // Initialize current state from initial state provided by
  // concrete child class.
  problem_status = OXSDRIVER_PS_INVALID;
  checkpoint_id = 0;
  current_state = GetInitialState();
  if (current_state.GetPtr() == NULL) {
    success = 0; // Error.  Perhaps an exception throw would be better?
  } else {
    const Oxs_SimState& cstate = current_state.GetReadReference();
    // If initial state was loaded from a checkpoint file, then
    // the problem status should be available from the state
    // derived data.  Otherwise, use the default STAGE_START
    // status.
    OC_REAL8m value;
    if(cstate.GetDerivedData("Oxs_Driver Problem Status",value)) {
      problem_status = FloatToProblemStatus(value);
    } else {
      problem_status = OXSDRIVER_PS_STAGE_START;
    }
    // There is no need (presumably?) to write the initial
    // state as a checkpoint file, so save the id of it.
    checkpoint_id = cstate.Id();
  }

  Oc_TimeVal dummy_time;
  Oc_Times(dummy_time,checkpoint_time,0); // Initialize checkpoint time

  return success;
}

void Oxs_Driver::StageRequestCount(unsigned int& min,
                                   unsigned int& max) const
{
  min = static_cast<unsigned int>(stage_iteration_limit.size());
  if(min>1) max = min;
  else      max = UINT_MAX;

  if(stage_count_request>min) min = stage_count_request;
  if(stage_count_request>0 && stage_count_request<max) {
    max = stage_count_request;
  }
}

OC_BOOL Oxs_Driver::IsStageDone(const Oxs_SimState& state) const
{
  if(state.stage_done == Oxs_SimState::DONE) return 1;
  if(state.stage_done == Oxs_SimState::NOT_DONE) return 0;
  /// Otherwise, state.stage_done == Oxs_SimState::UNKNOWN

  // Check state against parent Oxs_Driver class stage limiters.
  if(total_iteration_limit > 0
     && state.iteration_count >= total_iteration_limit ) {
    state.stage_done = Oxs_SimState::DONE;
    return 1;
  }

  // Stage iteration check
  OC_UINT4m stop_iteration=0;
  if(state.stage_number >= stage_iteration_limit.size()) {
    stop_iteration
      = stage_iteration_limit[stage_iteration_limit.size()-1];
  } else {
    stop_iteration = stage_iteration_limit[state.stage_number];
  }
  if(stop_iteration>0
     && state.stage_iteration_count + 1 >= stop_iteration) {
    // Note: stage_iteration_count is 0 based, so the number
    // of iterations is stage_iteration_count + 1.
    state.stage_done = Oxs_SimState::DONE;
    return 1;
  }

  // Otherwise, leave it up to the child
  if(ChildIsStageDone(state)) {
    state.stage_done = Oxs_SimState::DONE;
    return 1;
  }

  state.stage_done = Oxs_SimState::NOT_DONE;
  return 0;
}

OC_BOOL Oxs_Driver::IsRunDone(const Oxs_SimState& state) const
{
  if(state.run_done == Oxs_SimState::DONE) return 1;
  if(state.run_done == Oxs_SimState::NOT_DONE) return 0;
  /// Otherwise, state.run_done == Oxs_SimState::unknown

  // Check state against parent Oxs_Driver class run limiters.
  if(total_iteration_limit > 0
     && state.iteration_count >= total_iteration_limit) {
    state.run_done = Oxs_SimState::DONE;
    return 1;
  }

  if(number_of_stages > 0) {
    if( state.stage_number >= number_of_stages ||
        (state.stage_number+1 == number_of_stages
        && IsStageDone(state))) {
      state.run_done = Oxs_SimState::DONE;
      return 1;
    }
  }      

  // Otherwise, leave it up to the child
  if(ChildIsRunDone(state)) {
    state.run_done = Oxs_SimState::DONE;
    return 1;
  }

  state.run_done = Oxs_SimState::NOT_DONE;
  return 0;
}

void Oxs_Driver::FillStateMemberData
(const Oxs_SimState& old_state,
 Oxs_SimState& new_state) const
{
  old_state.CloneHeader(new_state);
  new_state.previous_state_id = old_state.Id();
  new_state.stage_done = Oxs_SimState::UNKNOWN;
  new_state.run_done = Oxs_SimState::UNKNOWN;

  // State variables iteration_count, stage_iteration_count,
  // stage_elapsed_time, last_timestep and DerivedData should
  // be set elsewhere.  Initialize to dummy values to aid
  // fault detection.
  new_state.iteration_count
    = new_state.stage_iteration_count
    = static_cast<OC_UINT4m>(-1);
  new_state.stage_elapsed_time
    = new_state.last_timestep
    = -1.0;
  // Might want to add energy & old_energy to all states, to simplify
  // Delta_E rendering.  Or maybe every state has a
  // previous_state_energy parallel to previous_state_id, and a Energy
  // entry in DerivedData?  See also FillNewStageStateMemberData.
}

void Oxs_Driver::FillStateDerivedData
(const Oxs_SimState& old_state,
 const Oxs_SimState& new_state) const
{
  if(report_max_spin_angle) {
    // Carry across old stage and run maxang data
    OC_REAL8m run_maxang = -1.;
    if(!old_state.GetDerivedData("Run Max Spin Ang",run_maxang)) {
      // Spin angle data not filled.  Insure that maxang is
      // computed every step.  This is a little extra work; if we
      // wanted, we could coordinate this with the solver so that
      // if maxang at previous step + 2*(allowed spin movement)
      // was less than stage_maxang, then maxang would not need
      // to be computed this step.  This could be handled by
      // adding a maxang_guess value to the state, but it is
      // probably not worth the trouble.
      UpdateSpinAngleData(old_state);
      old_state.GetDerivedData("Run Max Spin Ang",run_maxang);
    }
    new_state.AddDerivedData("PrevState Run Max Spin Ang",run_maxang);
    OC_REAL8m stage_maxang = -1.;
    if(!old_state.GetDerivedData("Stage Max Spin Ang",stage_maxang)) {
      UpdateSpinAngleData(old_state);
      old_state.GetDerivedData("Stage Max Spin Ang",stage_maxang);
    }
    new_state.AddDerivedData("PrevState Stage Max Spin Ang",stage_maxang);
  }
}

void Oxs_Driver::FillState
(const Oxs_SimState& old_state,
 Oxs_SimState& new_state) const
{ // This routine is intended for backward compatibility.
  // New code should use the separate FillStateMemberData
  // and FillStateDerivedData functions.
  FillStateMemberData(old_state,new_state);
  FillStateDerivedData(old_state,new_state);
}

void Oxs_Driver::FillNewStageStateMemberData
(const Oxs_SimState& old_state,
 int new_stage_number,
 Oxs_SimState& new_state) const
{
  if(new_stage_number<0 ||
     (0<number_of_stages
      && number_of_stages<static_cast<OC_UINT4m>(new_stage_number))) {
    // Valid stage numbers are 0, 1, ..., number_of_stages,
    // where the last indicates a "Run Done" state.
    char buf[1024];
    if(number_of_stages>0) {
      Oc_Snprintf(buf,sizeof(buf),
                  "Invalid stage request: %d;"
                  " should be in range [0,%u] (inclusive)",
                  new_stage_number,number_of_stages);
    } else {
      Oc_Snprintf(buf,sizeof(buf),
                  "Invalid stage request: %d; must be non-negative",
                  new_stage_number);
    }
    throw Oxs_ExtError(this,String(buf));
  }
  new_state = old_state; // Copy data
  new_state.previous_state_id = old_state.Id();
  new_state.iteration_count = old_state.iteration_count + 1;
  /// Increment iteration count across stage boundary.
  new_state.stage_number = new_stage_number;
  new_state.stage_iteration_count=0;
  new_state.stage_start_time += new_state.stage_elapsed_time;
  new_state.stage_elapsed_time = 0.0;
  new_state.last_timestep = 0.0;
  new_state.stage_done = Oxs_SimState::UNKNOWN;
  new_state.run_done = Oxs_SimState::UNKNOWN;
  new_state.ClearDerivedData(); // Derived data no longer valid
  // Might want to add energy & old_energy to all states, to simplify
  // Delta_E rendering.  Or maybe every state has a
  // previous_state_energy parallel to previous_state_id, and a Energy
  // entry in DerivedData?  See also FillNewStateMemberData.
}

void Oxs_Driver::FillNewStageStateDerivedData
(const Oxs_SimState& old_state,
 int /* new_stage_number */,
 const Oxs_SimState& new_state) const
{
  if(report_max_spin_angle) {
    // Carry across run maxang data, and
    // initialize prevstate stage maxang to 0.
    OC_REAL8m run_maxang = -1.0;
    if(!old_state.GetDerivedData("Run Max Spin Ang",run_maxang)) {
      // Spin angle data not filled in current state
      UpdateSpinAngleData(old_state); // Update
      old_state.GetDerivedData("Run Max Spin Ang",run_maxang);
    }
    new_state.AddDerivedData("PrevState Run Max Spin Ang",run_maxang);
    new_state.AddDerivedData("PrevState Stage Max Spin Ang",0.0);
  }
}

void Oxs_Driver::FillNewStageState
(const Oxs_SimState& old_state,
 int new_stage_number,
 Oxs_SimState& new_state) const
{ // This routine is intended for backward compatibility.
  // New code should use the separate FillNewStageStateMemberData
  // and FillNewStageStateDerivedData functions.
  FillNewStageStateMemberData(old_state,new_stage_number,new_state);
  FillNewStageStateDerivedData(old_state,new_stage_number,new_state);
}

void Oxs_Driver::SetStage(OC_UINT4m requestedStage)
{
  if(current_state.GetPtr() == NULL) {
    // Current state is not initialized.
    String msg="Current state in Oxs_Driver is not initialized;"
      " This is probably the fault of the child class "
      + String(ClassName());
    throw Oxs_ExtError(this,msg);
  }

  // Bound checks.  Should this return an error?  For now we'll
  // just adjust to bound.
  if(0<number_of_stages && requestedStage >= number_of_stages ) {
    requestedStage = number_of_stages - 1;
  }

  const Oxs_SimState& cstate = current_state.GetReadReference();
  if(requestedStage != cstate.stage_number) {
    // Change state only if stage number changes.
#if 0
    Oxs_Key<Oxs_SimState> next_state;
    director->GetNewSimulationState(next_state);

    Oxs_SimState& nstate = next_state.GetWriteReference();
    FillNewStageState(cstate,requestedStage,nstate);
    next_state.GetReadReference();  // Release write lock

    current_state = next_state;
    current_state.GetReadReference(); // Hold read lock
    problem_status = OXSDRIVER_PS_STAGE_START;
#else
    // Use machinery in Oxs_Driver::Run to transition states.
    // At present, OxsRunEvent data is simply discarded.  We
    // may want to change this in the future.
    problem_status = OXSDRIVER_PS_STAGE_END;
    vector<OxsRunEvent> dummy_results;
    Run(dummy_results,requestedStage - cstate.stage_number);
#endif
  }
}

OC_UINT4m Oxs_Driver::GetIteration() const
{
  const Oxs_SimState* cstate = current_state.GetPtr();
  if(cstate == NULL) {
    // Current state is not initialized.
    String msg="Current state in Oxs_Driver is not initialized;"
      " This is probably the fault of the child class "
      + String(ClassName());
    throw Oxs_ExtError(this,msg);
  }
  return cstate->iteration_count;
}

OC_UINT4m Oxs_Driver::GetStage() const
{
  const Oxs_SimState* cstate = current_state.GetPtr();
  if(cstate == NULL) {
    // Current state is not initialized.
    String msg="Current state in Oxs_Driver is not initialized;"
      " This is probably the fault of the child class "
      + String(ClassName());
    throw Oxs_ExtError(this,msg);
  }
  return cstate->stage_number;
}

void Oxs_Driver::Run(vector<OxsRunEvent>& results,
                     OC_INT4m stage_increment)
{ // Called by director

  if(current_state.GetPtr() == NULL) {
    // Current state is not initialized.
    String msg="Current state in Oxs_Driver is not initialized;"
      " This is probably the fault of the child class "
      + String(ClassName());
    throw Oxs_ExtError(this,msg);
  }

  if(current_state.ObjectId()==0) {
    // Current state is not fixed, i.e., is incomplete or transient.
    // To some extent, this check is not necessary, because key should
    // throw an exception on GetReadReference if the pointed to Oxs_Lock
    // object isn't fixed.
    String msg="PROGRAMMING ERROR:"
      " Current state in Oxs_Driver is incomplete or transient;"
      " This is probably the fault of the child class "
      + String(ClassName());
    throw Oxs_ExtError(this,msg);
  }

  int step_events=0;
  int stage_events=0;
  int done_event=0;
  int step_calls=0; // Number of times child Step() routine is called.

  // There are two considerations involved in the decision to break out
  // of the following step loops: 1) scheduled events should be passed
  // back to the caller for processing while the associated state
  // information is available, and 2) interactive requests should be
  // responded to in a timely manner.  In the future, control criteria
  // for each of these issues should be passed in from the caller.  For
  // the present, though, just ensure that no scheduled events are
  // overlooked by setting max_steps to 1, and guess that 2 step
  // attempts isn't too long between checking for interactive requests.

  const int max_steps=1; // TODO: should be set by caller.
  const int allowed_step_calls=2; // TODO: should be set by caller.

  while (step_events<max_steps && step_calls<allowed_step_calls
         && problem_status!=OXSDRIVER_PS_DONE) {
    Oxs_Key<Oxs_SimState> next_state;
    Oxs_ConstKey<Oxs_SimState> previous_state; // Used for state transitions
    OC_BOOL step_taken=0;
    OC_BOOL step_result=0;
    switch(problem_status) {
      case OXSDRIVER_PS_INSIDE_STAGE:
        // Most common case.
        director->GetNewSimulationState(next_state);
        // NOTE: At this point next_state holds a write lock.
        //   The Step() function can make additional calls
        //   to next_state.GetWriteReference() as needed; write
        //   locks do not accumulate.  However, it is the
        //   responsibility of Step or its callees to release
        //   the write lock, once next_state is fully populated.
#if REPORT_TIME
        driversteptime.Start();
#endif // REPORT_TIME
        step_result = Step(current_state,step_info,next_state);
#if REPORT_TIME
        driversteptime.Stop();
#endif // REPORT_TIME
        if( step_result ) {
          // Good step.  Release read lock on old current_state,
          // and copy key from next_state.
          next_state.GetReadReference();  // Safety write lock release
          current_state = next_state; // Free old read lock
          if(report_max_spin_angle) {
            UpdateSpinAngleData(*(current_state.GetPtr())); // Update
            /// max spin angle data on each accepted step.  Might want
            /// to modify this to instead estimate max angle change,
            /// and only do actually calculation when estimate uncertainty
            /// gets larger than some specified value.
          }
          step_taken=1;
          step_info.current_attempt_count=0;
        } else {
          ++step_info.current_attempt_count;
        }
        ++step_info.total_attempt_count;
        ++step_calls;
        break;

      case OXSDRIVER_PS_STAGE_END: {
        const Oxs_SimState& cstate = current_state.GetReadReference();
        director->GetNewSimulationState(next_state);
        Oxs_SimState& nstate = next_state.GetWriteReference();
        FillNewStageState(cstate,cstate.stage_number+stage_increment,
                          nstate);
        next_state.GetReadReference(); // Release write lock
        previous_state.Swap(current_state); // For state transistion
        current_state = next_state;
      }
      // NB: STAGE_END flow continues through STAGE_START block
      case OXSDRIVER_PS_STAGE_START:
        InitNewStage(current_state,previous_state); // Send state to
                                 /// evolver for bookkeeping updates.
        previous_state.Release();
        step_taken=1;
        ++step_info.total_attempt_count;
        step_info.current_attempt_count=0;
        break;

      case OXSDRIVER_PS_DONE: // DONE status not allowed inside loop
      case OXSDRIVER_PS_INVALID:
        throw Oxs_ExtError(this,"PROGRAMMING ERROR:"
                             " Invalid problem status detected in"
                             " Oxs_Driver::Run().");
    }

    if(step_taken) {
      const Oxs_SimState& cstate = current_state.GetReadReference();
      ++step_events;
      problem_status = OXSDRIVER_PS_INSIDE_STAGE;
      if (IsStageDone(cstate)) {
        ++stage_events;
        problem_status = OXSDRIVER_PS_STAGE_END;
        if (IsRunDone(cstate)) {
          ++done_event;
          problem_status = OXSDRIVER_PS_DONE;
        }
      }
#ifndef NDEBUG
      // For debugging purposes, we verify the return from
      // AddDerivedData, which returns True on success.  Failure might
      // happen if the previous status was STAGE_START, since in that
      // case the new current state is the same as the previous current
      // state, and so may already have a problem status recorded.  I
      // think this shouldn't happen, and I don't see any way it could
      // be a significant problem if it did happen, so for non-debug
      // builds just remove the check, thereby ignoring any hiccups.
      if(!cstate.AddDerivedData("Oxs_Driver Problem Status",
                                static_cast<OC_REAL8m>(problem_status))) {
        OC_REAL8m oldvalue = -1.0;
        if(!cstate.GetDerivedData("Oxs_Driver Problem Status",
                                  oldvalue)) {
          throw Oxs_ExtError(this,"Undiagnosable error trying to"
                               " set Oxs_Driver Problem Status into"
                               " current state.");
        }
        char buf[1000];
        Oc_Snprintf(buf,sizeof(buf),
                    "Error setting Oxs_Driver Problem Status"
                    " into current state; value already set."
                    " Old value: %d, New value: %d",
                    static_cast<int>(oldvalue),
                    static_cast<int>(problem_status));
        throw Oxs_ExtError(this,String(buf));
      }
#else
      cstate.AddDerivedData("Oxs_Driver Problem Status",
                            static_cast<OC_REAL8m>(problem_status));
#endif
    }

    // Checkpoint file save
    if(checkpoint_interval>=0.0
       && checkpoint_id != current_state.GetPtr()->Id()) {
      Oc_TimeVal cpu_time,wall_time;
      Oc_Times(cpu_time,wall_time);
      wall_time -= checkpoint_time;
      if(double(wall_time)>=checkpoint_interval) {
        // Save checkpoint state
        Oc_AutoBuf full_checkname;  // Full path to checkpoint file
        Nb_DString tmpchknam;
        OC_BOOL checkpoint_success = 0;
        String chkptmsg; // Error message, if any
        String chkptsrc = "CHECKPOINT"; // Error source, if any
        String chkptfile = __FILE__;
        String chkptline = OC_STRINGIFY(__LINE__);
        try {
          // Full error recovery might not be possible, depending
          // on the error, but it seems better to try to continue
          // than to just abort.
          Oc_DirectPathname(checkpoint_file.c_str(),full_checkname);
          checkpoint_id = current_state.GetPtr()->Id();
          tmpchknam = Nb_TempName(checkpoint_file.c_str(),".tmp",".");
          current_state.GetPtr()->SaveState(tmpchknam.GetStr(),
                                            "Checkpoint restart file",
                                            director);
          Nb_Rename(tmpchknam.GetStr(),checkpoint_file.c_str());
          ++checkpoint_writes;
          checkpoint_success = 1;
        } catch (Oxs_Exception& err) {
          // Include filename in error message
          char numbuf[256];
          Oc_Snprintf(numbuf,sizeof(numbuf),"%d",checkpoint_id);
          String msg = String("Error saving checkpoint file \"")
            + String(full_checkname) + String("\", state id ")
            + String(numbuf) + String(": ");
          err.Prepend(msg);
          // Use directory of temp file as error message subtype
          Oc_AutoBuf fullname,dirname;
          Oc_DirectPathname(tmpchknam.GetStr(),fullname);
          Oc_FileDirname(fullname,dirname);
          err.SetSubtype(dirname.GetStr());
          chkptmsg = err.MessageText()
            + String("\nError   type: ") + err.FullType()
            + String("\nError source: ") + err.FullSrc();
          chkptsrc = err.FullSrc();
          chkptfile = err.MessageFile();
          Oc_Snprintf(numbuf,sizeof(numbuf),"%d",err.MessageLine());
          chkptline = numbuf;
          checkpoint_success = 0;
        } catch (Oc_Exception& ocerr) {
          Oc_AutoBuf abuf;
          ocerr.ConstructMessage(abuf);
          char numbuf[256];
          Oc_Snprintf(numbuf,sizeof(numbuf),"%d",checkpoint_id);
          chkptmsg = String("Error saving checkpoint file \"")
            + String(full_checkname) + String("\", state id ")
            + String(numbuf) + String(": ")
            + String(abuf.GetStr());
          checkpoint_success = 0;
        } catch (const char* errmsg) {
          char numbuf[256];
          Oc_Snprintf(numbuf,sizeof(numbuf),"%d",checkpoint_id);
          chkptmsg = String("Error saving checkpoint file \"")
            + String(full_checkname) + String("\", state id ")
            + String(numbuf) + String(": ")
            + String(errmsg)
            + String("\n Detected in file:") + chkptfile
            + String("\n          at line:") + chkptline;
          checkpoint_success = 0;
        } catch (...) {
          char numbuf[256];
          Oc_Snprintf(numbuf,sizeof(numbuf),"%d",checkpoint_id);
          chkptmsg = String("Unrecognized error saving checkpoint file \"")
            + String(full_checkname) + String("\", state id ")
            + String(numbuf)
            + String("\n Detected in file:") + chkptfile
            + String("\n          at line:") + chkptline;
          checkpoint_success = 0;
        }

        if(!checkpoint_success) {
          static Oxs_WarningMessage checkpterr(3);
          chkptmsg += String("\n--------------\nWarning thrown");
          checkpterr.Send(revision_info,chkptline.c_str(),
                          chkptmsg.c_str(),chkptsrc.c_str(),
                          "CHKPT 1");
        }

        Oc_Times(cpu_time,checkpoint_time); // Reset checkpoint time
      }
    }
  } // End of 'step_events<max_steps ...' loop

  // Currently above block generates at most a single step.  When it
  // goes multi-step the report mechanism will need to be adjusted.
  results.clear();
  if(step_events) {
    results.push_back(OxsRunEvent(OXS_STEP_EVENT,current_state));
  }
  if(stage_events) {
    results.push_back(OxsRunEvent(OXS_STAGE_DONE_EVENT,current_state));
  }
  if(done_event) {
    results.push_back(OxsRunEvent(OXS_RUN_DONE_EVENT,current_state));
  }
}

////////////////////////////////////////////////////////////////////////
// State-based outputs, maintained by the driver.  These are
// conceptually public, but are specified private to force
// clients to use the output_map interface in Oxs_Director.

#define OSO_FUNC(NAME) \
void Oxs_Driver::Fill__##NAME##_output(const Oxs_SimState& state) \
{ NAME##_output.cache.state_id=state.Id(); \
  NAME##_output.cache.value=state.NAME; }

OSO_FUNC(iteration_count)
OSO_FUNC(stage_iteration_count)
OSO_FUNC(stage_number)

void
Oxs_Driver::Fill__spin_output(const Oxs_SimState& state)
{
  spin_output.cache.state_id=0;
  spin_output.cache.value = state.spin;
  spin_output.cache.state_id=state.Id();
}

void
Oxs_Driver::Fill__magnetization_output(const Oxs_SimState& state)
{
  magnetization_output.cache.state_id=0;
  magnetization_output.cache.value.AdjustSize(state.mesh);
  OC_INDEX size=state.mesh->Size();
  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m>& sMs = *(state.Ms);
  Oxs_MeshValue<ThreeVector>& mag = magnetization_output.cache.value;
  for(OC_INDEX i=0;i<size;i++) {
    mag[i] = spin[i];
    mag[i] *= sMs[i];
  }
  magnetization_output.cache.state_id=state.Id();
}

void Oxs_Driver::UpdateSpinAngleData(const Oxs_SimState& state) const
{
  if(!report_max_spin_angle) {
    static Oxs_WarningMessage nocall(3);
    nocall.Send(revision_info,OC_STRINGIFY(__LINE__),
                "Programming error?"
                " Input MIF file requested no driver spin angle reports,"
                " but Oxs_Driver::UpdateSpinAngleData is called.");
  }
  OC_REAL8m maxang,stage_maxang,run_maxang;
  stage_maxang = run_maxang = -1.0; // Safety init
  maxang = state.mesh->MaxNeighborAngle(state.spin,*(state.Ms))*(180./PI);
  state.GetDerivedData("PrevState Stage Max Spin Ang",stage_maxang);
  state.GetDerivedData("PrevState Run Max Spin Ang",run_maxang);
  if(maxang>stage_maxang) stage_maxang=maxang;
  if(maxang>run_maxang)   run_maxang=maxang;
  state.AddDerivedData("Max Spin Ang",maxang);
  state.AddDerivedData("Stage Max Spin Ang",stage_maxang);
  state.AddDerivedData("Run Max Spin Ang",run_maxang);
}

void
Oxs_Driver::Fill__maxSpinAng_output(const Oxs_SimState& state) 
{
  maxSpinAng_output.cache.state_id =
  stage_maxSpinAng_output.cache.state_id =
  run_maxSpinAng_output.cache.state_id = 0;

  OC_REAL8m maxang,stage_maxang,run_maxang;
  maxang = stage_maxang = run_maxang = -1.0; // Safety init

  if(!state.GetDerivedData("Max Spin Ang",maxang) ||
     !state.GetDerivedData("Stage Max Spin Ang",stage_maxang) ||
     !state.GetDerivedData("Run Max Spin Ang",run_maxang)) {
    UpdateSpinAngleData(state);
    state.GetDerivedData("Max Spin Ang",maxang);
    state.GetDerivedData("Stage Max Spin Ang",stage_maxang);
    state.GetDerivedData("Run Max Spin Ang",run_maxang);
  }
  maxSpinAng_output.cache.value=maxang;
  stage_maxSpinAng_output.cache.value=stage_maxang;
  run_maxSpinAng_output.cache.value=run_maxang;
  maxSpinAng_output.cache.state_id =
  stage_maxSpinAng_output.cache.state_id =
  run_maxSpinAng_output.cache.state_id = state.Id();
}

void
Oxs_Driver::Fill__aveM_output(const Oxs_SimState& state)
{ // NOTE: This code assumes that the cells in the mesh
  //       are uniformly sized.
#ifndef NDEBUG
  if(!state.mesh->HasUniformCellVolumes()) {
    throw Oxs_ExtError(this,"NEW CODE REQUIRED: Current Oxs_Driver"
                         " aveM and projection outputs require meshes "
                         " with uniform cell sizes, such as "
                         "Oxs_RectangularMesh.");
  }
#endif
  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m>& sMs = *(state.Ms);
  OC_INDEX size = state.mesh->Size();

  if(aveMx_output.GetCacheRequestCount()>0 &&
     aveMy_output.GetCacheRequestCount()>0 &&
     aveMz_output.GetCacheRequestCount()>0) {
    // Preferred case: All three components desired
    // This does not appear to be the usual case, however...
    aveMx_output.cache.state_id=0;
    aveMy_output.cache.state_id=0;
    aveMz_output.cache.state_id=0;
    OC_REAL8m Mx=0.0;
    OC_REAL8m My=0.0;
    OC_REAL8m Mz=0.0;
    for(OC_INDEX i=0;i<size;++i) {
      OC_REAL8m sat_mag = sMs[i];
      Mx += sat_mag*(spin[i].x);
      My += sat_mag*(spin[i].y);
      Mz += sat_mag*(spin[i].z);
    }
    aveMx_output.cache.value=Mx*scaling_aveM;
    aveMx_output.cache.state_id=state.Id();
    aveMy_output.cache.value=My*scaling_aveM;
    aveMy_output.cache.state_id=state.Id();
    aveMz_output.cache.value=Mz*scaling_aveM;
    aveMz_output.cache.state_id=state.Id();
  } else {
    // Calculate components on a case-by-case basis
    if(aveMx_output.GetCacheRequestCount()>0) {
      aveMx_output.cache.state_id=0;
      OC_REAL8m Mx=0.0;
      for(OC_INDEX i=0;i<size;++i) {
        Mx += sMs[i]*(spin[i].x);
      }
      aveMx_output.cache.value=Mx*scaling_aveM;
      aveMx_output.cache.state_id=state.Id();
    }

    if(aveMy_output.GetCacheRequestCount()>0) {
      aveMy_output.cache.state_id=0;
      OC_REAL8m My=0.0;
      for(OC_INDEX i=0;i<size;++i) {
        My += sMs[i]*(spin[i].y);
      }
      aveMy_output.cache.value=My*scaling_aveM;
      aveMy_output.cache.state_id=state.Id();
    }

    if(aveMz_output.GetCacheRequestCount()>0) {
      aveMz_output.cache.state_id=0;
      OC_REAL8m Mz=0.0;
      for(OC_INDEX i=0;i<size;++i) {
        Mz += sMs[i]*(spin[i].z);
      }
      aveMz_output.cache.value=Mz*scaling_aveM;
      aveMz_output.cache.state_id=state.Id();
    }
  }
}

void
Oxs_Driver::Fill__projection_outputs(const Oxs_SimState& state)
{ // NOTE: This code assumes that the cells in the mesh
  //       are uniformly sized.
#ifndef NDEBUG
  if(!state.mesh->HasUniformCellVolumes()) {
    throw Oxs_ExtError(this,"NEW CODE REQUIRED: Current Oxs_Driver"
                         " aveM and projection outputs require meshes "
                         " with uniform cell sizes, such as "
                         "Oxs_RectangularMesh.");
  }
#endif
  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m>& sMs = *(state.Ms);
  const OC_INDEX size = state.mesh->Size();
  for(OC_INDEX proj=0;proj<projection_output.GetSize();++proj) {
    Oxs_ScalarOutput<Oxs_Driver>& output = projection_output[proj].output;
    if(output.GetCacheRequestCount()>0) {
      output.cache.state_id=0;
      OC_REAL8m sum=0.0;
      const Oxs_MeshValue<ThreeVector>& trellis
        = projection_output[proj].trellis;
      for(OC_INDEX i=0;i<size;++i) {
        sum += sMs[i]*(spin[i]*trellis[i]);
      }
      output.cache.value=sum*projection_output[proj].scaling;
      output.cache.state_id=state.Id();
    }
  }
}
