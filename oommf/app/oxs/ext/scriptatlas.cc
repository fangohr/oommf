/* FILE: scriptatlas.cc                 -*-Mode: c++-*-
 *
 * Script atlas class, derived from OXS extension class.
 *
 */

#include "oc.h"
#include "nb.h"

#include "director.h"
#include "scriptatlas.h"

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_ScriptAtlas);

/* End includes */


// Constructor
Oxs_ScriptAtlas::Oxs_ScriptAtlas
( const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr    // MIF block argument string
  ) : Oxs_Atlas(name,newdtr,argstr)
{
  // Process arguments.
  vector<OC_REAL8m> xrange; GetRealVectorInitValue("xrange",2,xrange);
  vector<OC_REAL8m> yrange; GetRealVectorInitValue("yrange",2,yrange);
  vector<OC_REAL8m> zrange; GetRealVectorInitValue("zrange",2,zrange);
  if(!bounding_box.CheckOrderSet(xrange[0],xrange[1],
				 yrange[0],yrange[1],
				 zrange[0],zrange[1])) {
    throw Oxs_ExtError(this,"One of [xyz]range in Specify block"
			 " is improperly ordered.");
  }
  if(bounding_box.GetVolume()<=0.0) {
    throw Oxs_ExtError(this,"Atlas bounding box has zero volume.");
  }

  vector<String> regionlist;
  FindRequiredInitValue("regions",regionlist);
  if(regionlist.empty()) {
    throw Oxs_ExtError(this,"Empty region list");
  }
  // Copy regions list into region_name_list, setting special
  // "universe" region at id 0;
  region_name_list.push_back("universe");
  vector<String>::const_iterator it = regionlist.begin();
  while(it != regionlist.end()) {
    if(it->compare("universe")==0) {
      throw Oxs_ExtError(this,
			   "universe is a reserved region label, and"
			   " must not be included in user region list.");
    }
    region_name_list.push_back(*it);
    ++it;
  }
  DeleteInitValue("regions");
  
  ThreeVector minpt(bounding_box.GetMinX(),bounding_box.GetMinY(),
		    bounding_box.GetMinZ());
  ThreeVector maxpt(bounding_box.GetMaxX(),bounding_box.GetMaxY(),
		    bounding_box.GetMaxZ());
  ThreeVector span = maxpt - minpt;
  if(span.x<=0) scale.x = 1.0; // Safety
  else          scale.x = 1.0/span.x;
  if(span.y<=0) scale.y = 1.0;
  else          scale.y = 1.0/span.y;
  if(span.z<=0) scale.z = 1.0;
  else          scale.z = 1.0/span.z;

  String cmdoptreq = GetStringInitValue("script_args","relpt");
  command_options.push_back(Nb_TclCommandLineOption("minpt",3));
  command_options.push_back(Nb_TclCommandLineOption("maxpt",3));
  command_options.push_back(Nb_TclCommandLineOption("span",3));
  command_options.push_back(Nb_TclCommandLineOption("rawpt",3));
  command_options.push_back(Nb_TclCommandLineOption("relpt",3));

  String runscript = GetStringInitValue("script");

  VerifyAllInitArgsUsed();

  // Run SetBaseCommand *after* VerifyAllInitArgsUsed(); this produces
  // a more intelligible error message in case a command line argument
  // error is really due to a misspelled parameter label.
  cmd.SetBaseCommand(InstanceName(),
		     director->GetMifInterp(),
		     runscript,
		     Nb_ParseTclCommandLineRequest(InstanceName(),
                                                   command_options,
                                                   cmdoptreq));
  // Fill fixed command line args
  int index;
  index = command_options[0].position; // minpt
  if(index>=0) {
    cmd.SetCommandArg(index,minpt.x);
    cmd.SetCommandArg(index+1,minpt.y);
    cmd.SetCommandArg(index+2,minpt.z);
  }
  index = command_options[1].position; // maxpt
  if(index>=0) {
    cmd.SetCommandArg(index,maxpt.x);
    cmd.SetCommandArg(index+1,maxpt.y);
    cmd.SetCommandArg(index+2,maxpt.z);
  }
  index = command_options[2].position; // span
  if(index>=0) {
    cmd.SetCommandArg(index,span.x);
    cmd.SetCommandArg(index+1,span.y);
    cmd.SetCommandArg(index+2,span.z);
  }
}

Oxs_ScriptAtlas::~Oxs_ScriptAtlas()
{}


OC_BOOL Oxs_ScriptAtlas::GetRegionExtents(OC_INDEX id,Oxs_Box &mybox) const
{ // Fills mybox with bounding box for region with specified
  // id number.  Return value is 0 iff id is invalid.  At present,
  // given a valid id number, just returns the world bounding
  // box.  In the future, we might want to add support for refined
  // region bounding boxes.
  if(id>=GetRegionCount()) return 0;
  mybox = bounding_box;
  return 1;
}

OC_INDEX Oxs_ScriptAtlas::GetRegionId(const ThreeVector& point) const
{ // Appends point & bounding box to "script" from the Specify
  // block, and evaluates the resulting command in the MIF
  // interpreter.  The script should return the 1-based index
  // for the corresponding region, or 0 if none apply.
  if(!bounding_box.IsIn(point)) return 0; // Don't claim point
  /// if it doesn't lie within atlas bounding box.

  cmd.SaveInterpResult();

  // Fill variable command line args
  int index;
  index = command_options[3].position; // rawpt
  if(index>=0) {
    cmd.SetCommandArg(index,point.x);
    cmd.SetCommandArg(index+1,point.y);
    cmd.SetCommandArg(index+2,point.z);
  }
  index = command_options[4].position; // relpt
  if(index>=0) {
    OC_REAL8m x = (point.x - bounding_box.GetMinX())*scale.x;
    cmd.SetCommandArg(index,x);
    OC_REAL8m y = (point.y - bounding_box.GetMinY())*scale.y;
    cmd.SetCommandArg(index+1,y);
    OC_REAL8m z = (point.z - bounding_box.GetMinZ())*scale.z;
    cmd.SetCommandArg(index+2,z);
  }

  cmd.Eval();

  if(cmd.GetResultListSize()!=1) {
    String msg
      = String("Return script value is not a single integer: ")
      + cmd.GetWholeResult();
    cmd.RestoreInterpResult();
    throw Oxs_ExtError(this,msg.c_str());
  }
  OC_INT4m id; // GetResultListItem wants an OC_INT4m type
  cmd.GetResultListItem(0,id);
  cmd.RestoreInterpResult();

  if(id < 0 || id >= GetRegionCount()) {
    char buf[512];
    Oc_Snprintf(buf,sizeof(buf),
		"Region id value %d returned by script is out of range.",
		id);
    throw Oxs_ExtError(this,buf);
  }
  return id;
}

OC_INDEX Oxs_ScriptAtlas::GetRegionId(const String& name) const
{ // Given a region id string (name), returns
  // the corresponding region id index.  If
  // "name" is not included in the atlas, then
  // -1 is returned.  Note: If name == "universe",
  // then the return value will be 0.
  OC_INDEX count = static_cast<OC_INDEX>(region_name_list.size());
  for(OC_INDEX i=0;i<count;i++) {
    if(region_name_list[i].compare(name)==0) return i;
  }
  return -1;
}

OC_BOOL Oxs_ScriptAtlas::GetRegionName(OC_INDEX id,String& name) const
{ // Given an id number, fills in "name" with
  // the corresponding region id string.  Returns
  // 1 on success, 0 if id is invalid.  If id is 0,
  // then name is set to "universe", and the return
  // value is 1.
  if(id>=static_cast<OC_INDEX>(region_name_list.size())) return 0;
  name = region_name_list[id];
  return 1;
}

