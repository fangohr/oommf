/* FILE: simstate.cc             -*-Mode: c++-*-
 *
 * Simulation state class.
 *
 */

#include "nb.h"
#include "vf.h"

#include "director.h"
#include "mesh.h"
#include "oxsexcept.h"
#include "simstate.h"
#include "util.h"

/* End includes */

// Constructor
Oxs_SimState::Oxs_SimState()
  : previous_state_id(0),iteration_count(0),
    stage_number(0),stage_iteration_count(0),
    stage_start_time(0.),stage_elapsed_time(0.),
    last_timestep(0.),
    mesh(NULL),Ms(NULL),Ms_inverse(NULL),max_absMs(-1),
    step_done(UNKNOWN), stage_done(UNKNOWN), run_done(UNKNOWN)
{}

//Destructor
Oxs_SimState::~Oxs_SimState()
{}

const Vf_Ovf20_MeshNodes* Oxs_SimState::GetMeshNodesPtr() const
{
  return static_cast<const Vf_Ovf20_MeshNodes*>(mesh);
}

void Oxs_SimState::Reset()
// Called by Oxs_Director (and others) to reuse state structure.  The
// (potentially large) Oxs_MeshValue array "spin" is not touched, for
// reasons of efficiency.  However, for debugging checks one might want
// to wipe "spin" too.
{
  ClearDerivedData();
  ClearAuxData();
  
  previous_state_id=0;
  iteration_count=0;
  stage_number=0;
  stage_iteration_count=00;
  stage_start_time=0.;
  stage_elapsed_time=0.;
  last_timestep=0.;

  mesh=NULL;
  Ms=NULL;
  Ms_inverse=NULL;
  max_absMs=-1;

  step_done=UNKNOWN;
  stage_done=UNKNOWN;
  run_done=UNKNOWN;

#ifdef FOR_DEBUGGING_USE_ONLY
  // "spin" is potentially very large; the whole point of reusing
  // Oxs_SimState's is to avoid the overhead of repeated allocation
  // and deallocation of this array.  This following code is
  // presented FOR DEBUGGING PURPOSES ONLY!!!
  spin.Release();
#endif // FOR_DEBUGGING_USE_ONLY

}

OC_BOOL Oxs_SimState::AddDerivedData
(const char* name,
 OC_REAL8m value) const
{ // Note: The derived_data map object is mutable.
  const String csname = name;
  pair<const String,OC_REAL8m> newdata(csname,value);
  // According to the C++ spec, the first member of pair<>, given as
  // input to map.insert(), is const.  Compilers should promote
  // non-const to const, but some compilers are broken.  It doesn't
  // hurt us here to be precise, so we specify const String for
  // portability.
  pair<map<DerivedDataKey,OC_REAL8m>::iterator,bool> p
    = derived_data.insert(newdata);
  return p.second;
}

OC_BOOL Oxs_SimState::GetDerivedData
(const char* name,
 OC_REAL8m& value) const
{
  map<DerivedDataKey,OC_REAL8m>::const_iterator it
    = derived_data.find(String(name));
  if(it==derived_data.end()) {
    return 0;
  }
  value = it->second;
  return 1;
}

void Oxs_SimState::AppendListDerivedData(vector<String>& names) const
{ // Class internal routine
  for(map<DerivedDataKey,OC_REAL8m>::const_iterator
        cit = derived_data.begin(); cit!=derived_data.end(); ++cit) {
    names.push_back(cit->first);
  }
}

void Oxs_SimState::ListDerivedData(vector<String>& names) const
{
  names.clear();
  AppendListDerivedData(names);
}

void Oxs_SimState::ClearDerivedData()
{
  derived_data.clear();
}

void Oxs_SimState::AddAuxData
(const char* name,
 OC_REAL8m value) const
{ // Note: The auxiliary_data map object is mutable.
  const String csname = name;
  auxiliary_data[csname] = value;
}

OC_BOOL Oxs_SimState::GetAuxData
(const char* name,
 OC_REAL8m& value) const
{
  map<AuxDataKey,OC_REAL8m>::const_iterator it
    = auxiliary_data.find(String(name));
  if(it==auxiliary_data.end()) {
    return 0;
  }
  value = it->second;
  return 1;
}

void Oxs_SimState::AppendListAuxData(vector<String>& names) const
{ // Class internal routine
  for(map<AuxDataKey,OC_REAL8m>::const_iterator
        cit = auxiliary_data.begin(); cit!=auxiliary_data.end(); ++cit) {
    names.push_back(cit->first);
  }
}

void Oxs_SimState::ListAuxData(vector<String>& names) const
{
  names.clear();
  AppendListAuxData(names);
}

void Oxs_SimState::ClearAuxData()
{
  auxiliary_data.clear();
}

void Oxs_SimState::QueryScalarNames(vector<String>& keys) const
{
  // Start with built-ins.  NB: This code section must be kept
  // consistent with the corresponding section in QueryScalarValues().
  keys.clear();
  keys.push_back("state_id");
  keys.push_back("previous_state_id");
  keys.push_back("iteration_count");
  keys.push_back("stage_number");
  keys.push_back("stage_iteration_count");
  keys.push_back("stage_start_time");
  keys.push_back("stage_elapsed_time");
  keys.push_back("total_elapsed_time");
  keys.push_back("last_timestep");
  keys.push_back("max_absMs");
  keys.push_back("step_done");  // Good step==1, Rejected step==0, Unknown==-1
  keys.push_back("stage_done"); // Done==1, Not done==0, Unknown==-1
  keys.push_back("run_done");   // Done==1, Not done==0, Unknown==-1

  // Append keys from derived and aux data
  AppendListDerivedData(keys);
  AppendListAuxData(keys);
}

void Oxs_SimState::QueryScalarValues
(const vector<String>& keys, vector<Oxs_MultiType>& values) const
{
  OC_REAL8m real_data;
  values.clear();
  for(vector<String>::const_iterator cit = keys.begin();
      cit != keys.end(); ++cit) {
    const String& key = *cit;
    // Check built-ins first.  NB: This code section must be kept
    // consistent with the corresponding section in QueryScalarNames().
    Oxs_MultiType dummy;
    if(key.compare("state_id")==0) {
      values.push_back(Id());
      continue;
    } else if(key.compare("previous_state_id")==0) {
      values.push_back(previous_state_id);
      continue;
    } else if(key.compare("iteration_count")==0) {
      values.push_back(iteration_count);
      continue;
    } else if(key.compare("stage_number")==0) {
      values.push_back(stage_number);
      continue;
    } else if(key.compare("stage_iteration_count")==0) {
      values.push_back(stage_iteration_count);
      continue;
    } else if(key.compare("stage_start_time")==0) {
      values.push_back(stage_start_time);
      continue;
    } else if(key.compare("stage_elapsed_time")==0) {
      values.push_back(stage_elapsed_time);
      continue;
    } else if(key.compare("total_elapsed_time")==0) {
      values.push_back(stage_start_time + stage_elapsed_time);
      continue;
    } else if(key.compare("last_timestep")==0) {
      values.push_back(last_timestep);
      continue;
    } else if(key.compare("max_absMs")==0) {
      values.push_back(max_absMs);
      continue;
    } else if(key.compare("step_done")==0) {
      values.push_back(static_cast<int>(step_done));
      /// Good (accepted) step==1, Rejected step==0, Unknown==-1
      continue;
    } else if(key.compare("stage_done")==0) {
      values.push_back(static_cast<int>(stage_done));
      /// Done==1, Not done==0, Unknown==-1
      continue;
    } else if(key.compare("run_done")==0) {
      values.push_back(static_cast<int>(run_done));
      /// Done==1, Not done==0, Unknown==-1
      continue;
    }

    // Next try derived and then aux data maps
    if(GetDerivedData(key,real_data) || GetAuxData(key,real_data)) {
      values.push_back(static_cast<Oxs_MultiType>(real_data));
      continue;
    }

    // Otherwise, error
    String msg=String("Error detected in Oxs_SimState::QueryScalarValues; ");
    msg += String("Unknown key request: ");
    msg += key;
    OXS_THROW(Oxs_BadParameter,msg);
  }
}

void Oxs_SimState::CloneHeader(Oxs_SimState& new_state) const
{
  new_state.previous_state_id     = previous_state_id;
  new_state.iteration_count       = iteration_count;
  new_state.stage_number          = stage_number;
  new_state.stage_iteration_count = stage_iteration_count;
  new_state.stage_start_time      = stage_start_time;
  new_state.stage_elapsed_time    = stage_elapsed_time;
  new_state.last_timestep         = last_timestep;
  new_state.mesh                  = mesh;
  new_state.Ms                    = Ms;
  new_state.Ms_inverse            = Ms_inverse;
  new_state.max_absMs             = max_absMs;
  new_state.spin.AdjustSize(mesh);
  new_state.ClearDerivedData();
  new_state.ClearAuxData();
}

void Oxs_SimState::SaveState(const char* filename,
			     const char* title,
			     const Oxs_Director* director) const
{ // Write state to file.  Primarily, this is done by dumping the
  // spin configuration as an OVF file via the mesh WriteOvf interface.
  // The non-spin state data is stored as name-value pairs as OVF
  // "desc" lines.  The Ms, Ms_inverse, and mesh data are not written,
  // but must be supplied independently for the restore operation.
  //   Note: WriteOvf throws an exception on failure, and deletes
  // any partially written file.  In particular, this happens if
  // the disk is (or becomes) full.

  char buf[1024];

  String desc;
  vector<String> string_array;

  // Write MIF info export and numeric portions of state.  Note that
  // the RestoreState code depends sensitively on the order and number
  // of fixed state variables (iteration_count, etc.).  Any changes
  // here must be reflected in that routine.

  string_array.clear();
  string_array.push_back(String("MIF_file"));
  string_array.push_back(String(director->GetProblemName()));
  desc = Nb_MergeList(string_array);

  Oc_Snprintf(buf,sizeof(buf),"\nMIF_crc %lu",
	      static_cast<unsigned long>(director->GetMifCrc()));
  desc += String(buf);

  string_array.clear();
  string_array.push_back(String("MIF_params"));
  string_array.push_back(director->GetMifParameters());
  desc += String("\n");
  desc += Nb_MergeList(string_array);

  Oc_Snprintf(buf,sizeof(buf),"\niteration_count %u",
	      iteration_count);
  desc += String(buf);
  Oc_Snprintf(buf,sizeof(buf),"\nstage_number %u",
	      stage_number);
  desc += String(buf);
  Oc_Snprintf(buf,sizeof(buf),"\nstage_iteration_count %u",
	      stage_iteration_count);
  desc += String(buf);
  Oc_Snprintf(buf,sizeof(buf),"\nstage_start_time %.17g",
	      static_cast<double>(stage_start_time));
  desc += String(buf);
  Oc_Snprintf(buf,sizeof(buf),"\nstage_elapsed_time %.17g",
	      static_cast<double>(stage_elapsed_time));
  desc += String(buf);
  Oc_Snprintf(buf,sizeof(buf),"\nlast_timestep %.17g",
	      static_cast<double>(last_timestep));
  desc += String(buf);
  Oc_Snprintf(buf,sizeof(buf),"\nstep_done %d",
	      step_done);
  desc += String(buf);
  Oc_Snprintf(buf,sizeof(buf),"\nstage_done %d",
	      stage_done);
  desc += String(buf);
  Oc_Snprintf(buf,sizeof(buf),"\nrun_done %d",
	      run_done);
  desc += String(buf);

  // Derived data
  map<DerivedDataKey,OC_REAL8m>::const_iterator it = derived_data.begin();
  while(it!=derived_data.end()) {
    string_array.clear();
    string_array.push_back("DRV");
    string_array.push_back(it->first);
    Oc_Snprintf(buf,sizeof(buf),"%.17g",
                static_cast<double>(it->second));
    string_array.push_back(String(buf));
    desc += String("\n");
    desc += Nb_MergeList(string_array);
    ++it;
  }

  // Auxiliary data
  map<AuxDataKey,OC_REAL8m>::const_iterator ait = auxiliary_data.begin();
  while(ait!=auxiliary_data.end()) {
    string_array.clear();
    string_array.push_back("AUX");
    string_array.push_back(ait->first);
    Oc_Snprintf(buf,sizeof(buf),"%.17g",
                static_cast<double>(ait->second));
    string_array.push_back(String(buf));
    desc += String("\n");
    desc += Nb_MergeList(string_array);
    ++ait;
  }

  vector<String> valuelabels;
  valuelabels.push_back(String("m_x"));
  valuelabels.push_back(String("m_y"));
  valuelabels.push_back(String("m_z"));

  vector<String> valueunits;
  valueunits.push_back(String(""));
  valueunits.push_back(String(""));
  valueunits.push_back(String(""));

  const int fsync = 1; // Flush data all the way to disk
  Oxs_MeshValueOutputField<ThreeVector>
    (filename,1,title,desc.c_str(),valuelabels,valueunits,
     mesh->NaturalType(),vf_obin8,NULL,mesh,&spin,vf_ovf_latest,
     fsync);
}


void Oxs_SimState::RestoreState
(const char* filename,
 const Oxs_Mesh* import_mesh,
 const Oxs_MeshValue<OC_REAL8m>* import_Ms,
 const Oxs_MeshValue<OC_REAL8m>* import_Ms_inverse,
 OC_REAL8m import_max_absMs,
 const Oxs_Director* director,
 String& export_MIF_info)
{ // Fill state from channel
  mesh = import_mesh;
  Ms = import_Ms;
  Ms_inverse = import_Ms_inverse;
  max_absMs = import_max_absMs;

  if(mesh==NULL || Ms==NULL || Ms_inverse==NULL) {
    String msg=String("NULL pointer input as parameter to"
		      " Oxs_SimState::RestoreState.");
    OXS_THROW(Oxs_BadParameter,msg);
  }

  // Create and load Vf_Mesh object with file data
  Vf_FileInput* vffi=Vf_FileInput::NewReader(filename);
  if(vffi==NULL) {
    String msg=String("Error detected inside Oxs_SimState::RestoreState; ");
    msg += String("Error reading input file ");
    msg += String(filename);
    OXS_THROW(Oxs_BadParameter,msg);
  }
  Vf_Mesh* file_mesh = vffi->NewMesh();
  delete vffi;
  if(file_mesh==NULL) {
    String msg=String("Error detected inside Oxs_SimState::RestoreState; ");
    msg += String("Unable to create Vf_Mesh object on input file ");
    msg += String(filename);
    OXS_THROW(Oxs_BadResourceAlloc,msg);
  }

  if(file_mesh->GetSize()!=mesh->Size()) {
    String msg=String("Error detected during Oxs_SimState::RestoreState; ");
    msg += String("simulation state mesh size different"
		  " from mesh size in input file ");
    msg += String(filename);
    delete file_mesh;
    OXS_THROW(Oxs_BadParameter,msg);
  }

  // Fill spin array
  spin.AdjustSize(mesh);
  OC_INDEX size = spin.Size();
  for(OC_INDEX i=0;i<size;i++) {
    ThreeVector location;
    mesh->Center(i,location);
    Nb_Vec3<OC_REAL8> pos(location.x,location.y,location.z);
    Nb_LocatedVector<OC_REAL8> lv;
    file_mesh->FindPreciseClosest(pos,lv);
    ThreeVector value(lv.value.x,lv.value.y,lv.value.z);
    OC_REAL8m magerr = value.MagSq()-1.0;
    if(fabs(magerr)>8*OC_REAL8_EPSILON) {
      char buf[2048];
      Oc_Snprintf(buf,sizeof(buf),
		  "Error detected during Oxs_SimState::RestoreState;"
		  " Input spin vector %u is not a unit vector,"
		  " in input file %.1500s.  Actual magnitude^2: 1+%g.",
		  i,filename,static_cast<double>(magerr));
      OXS_THROW(Oxs_BadParameter,buf);
    }
    spin[i] = value;
  }

  // Fill MIF info export and numeric portions of state.  Note that
  // this code depends sensitively on the order and number of fixed
  // state variables (iteration_count, etc.) as written by SaveState().
  // We could make this more robust, but be careful about label name
  // conflicts with the DerivedData names.
  Nb_SplitList desc;
  desc.Split(file_mesh->GetDescription());
  if(desc.Count()<24) {
      String msg=String("Error detected during Oxs_SimState::RestoreState; ");
      msg += String("Description array has too few elements"
		    " in input file ");
      msg += String(filename);
      OXS_THROW(Oxs_BadParameter,msg);
  }
  int errcount=0;
  String mif_filename,mif_params;
  OC_UINT4m mif_crc = 0;
  if(strcmp("MIF_file",desc[0])!=0) ++errcount;
  else mif_filename = String(desc[1]);
  if(strcmp("MIF_crc",desc[2])!=0) ++errcount;
  else mif_crc = static_cast<OC_UINT4m>(strtoul(desc[3],NULL,0));
  if(strcmp("MIF_params",desc[4])!=0) ++errcount;
  else mif_params = String(desc[5]);
  if(strcmp("iteration_count",desc[6])!=0) ++errcount;
  else iteration_count = static_cast<OC_UINT4m>(atoi(desc[7]));
  if(strcmp("stage_number",desc[8])!=0) ++errcount;
  else  stage_number = static_cast<OC_UINT4m>(atoi(desc[9]));
  if(strcmp("stage_iteration_count",desc[10])!=0) ++errcount;
  else  stage_iteration_count = static_cast<OC_UINT4m>(atoi(desc[11]));
  if(strcmp("stage_start_time",desc[12])!=0) ++errcount;
  else  stage_start_time = Nb_Atof(desc[13]);
  if(strcmp("stage_elapsed_time",desc[14])!=0) ++errcount;
  else  stage_elapsed_time = Nb_Atof(desc[15]);
  if(strcmp("last_timestep",desc[16])!=0) ++errcount;
  else  last_timestep = Nb_Atof(desc[17]);
  if(strcmp("step_done",desc[18])!=0) ++errcount;
  else  step_done
	  = static_cast<Oxs_SimState::SimStateStatus>(atoi(desc[19]));
  if(strcmp("stage_done",desc[20])!=0) ++errcount;
  else  stage_done
	  = static_cast<Oxs_SimState::SimStateStatus>(atoi(desc[21]));
  if(strcmp("run_done",desc[22])!=0) ++errcount;
  else  run_done
	  = static_cast<Oxs_SimState::SimStateStatus>(atoi(desc[23]));
  if(errcount>0) {
      String msg=String("Error detected during Oxs_SimState::RestoreState;");
      msg += String(" Bad header in restart file ");
      msg += String(filename);
      OXS_THROW(Oxs_BadData,msg);
  }

  ClearDerivedData();
  ClearAuxData();
  for(int pair_index=24;pair_index<desc.Count();pair_index+=2) {
    OC_BOOL error;
    enum DataType { DT_DERIVED, DT_AUXILIARY } dt;
    if(strcmp("DRV",desc[pair_index])) {
      dt = DT_DERIVED;
      ++pair_index; // Skip over type descriptor
    } else if(strcmp("AUX",desc[pair_index])) {
      dt = DT_AUXILIARY;
      ++pair_index; // Skip over type descriptor
    } else { // Old style (no type descriptor)
      dt = DT_DERIVED;
    }
    if(pair_index+1 >= desc.Count()) {
      String msg=String("Error detected during Oxs_SimState::RestoreState; ");
      msg += String("Description array has improper structure"
		    " in input file ");
      msg += String(filename);
      OXS_THROW(Oxs_BadParameter,msg);
    }
    double val = Nb_Atof(desc[pair_index+1],error);
    if(error) {
      String msg=String("Error detected during Oxs_SimState::RestoreState; ");
      msg += String("Description array has non-numeric value"
		    " in input file ");
      msg += String(filename);
      delete file_mesh;
      OXS_THROW(Oxs_BadParameter,msg);
    }
    if(dt == DT_AUXILIARY) {
      AddAuxData(desc[pair_index],val);
    } else {
      if(!AddDerivedData(desc[pair_index],val)) {
        String msg=String("Error detected during Oxs_SimState::RestoreState; ");
        msg += String("AddDerivedData() error, repeated name?"
                      " In input file ");
        msg += String(filename);
        delete file_mesh;
        OXS_THROW(Oxs_BadParameter,msg);
      }
    }
  }
  delete file_mesh;

  // Check that current problem matches restored problem.
  if(director->CheckRestartCrc() && director->GetMifCrc()!=mif_crc) {
    char buf[4096];
    Oc_Snprintf(buf,sizeof(buf),
		"Error detected during Oxs_SimState::RestoreState;"
		" CRC of current problem (%lu) doesn't match"
		" CRC (%lu) saved in checkpoint file (%.2048s).",
		static_cast<unsigned long>(director->GetMifCrc()),
		static_cast<unsigned long>(mif_crc),
		filename);
    OXS_THROW(Oxs_BadUserInput,buf);
  }
  if(!mif_params.empty() && !director->CheckMifParameters(mif_params)) {
    char buf[4096];
    String current_params = director->GetMifParameters();
    Oc_Snprintf(buf,sizeof(buf),
		"Error detected during Oxs_SimState::RestoreState;"
		" Parameters of current problem (%.1024s) don't match"
		" Parameters (%.1024s) saved in checkpoint file (%.1024s).",
		current_params.c_str(),mif_params.c_str(),filename);
    OXS_THROW(Oxs_BadUserInput,buf);
  }

  export_MIF_info = mif_filename;
}
