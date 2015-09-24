/* FILE: scriptorientvectorfield.cc      -*-Mode: c++-*-
 *
 * Vector field object, derived from Oxs_VectorField class,
 * that applies an script orientation (pre-)transformation
 * to another vector field object.
 *
 */

#include <string>

#include "oc.h"

#include "atlas.h"
#include "util.h"
#include "threevector.h"
#include "meshvalue.h"
#include "director.h"
#include "scriptorientvectorfield.h"

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_ScriptOrientVectorField);

/* End includes */

// Constructor
Oxs_ScriptOrientVectorField::Oxs_ScriptOrientVectorField(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_VectorField(name,newdtr,argstr)
{
  // Get vector field object
  OXS_GET_INIT_EXT_OBJECT("field",Oxs_VectorField,field);

  // Determine bounding box.
  OC_BOOL bbox_set = 0;
  Oxs_Box bbox;
  if(HasInitValue("atlas")) {
    // Get bounding box from specified atlas.
    Oxs_OwnedPointer<Oxs_Atlas> atlas;
    OXS_GET_INIT_EXT_OBJECT("atlas",Oxs_Atlas,atlas);
    atlas->GetWorldExtents(bbox);
    bbox_set = 1;
  } else {
    // Atlas not specified, so check for and use x/y/zrange data.
    if(HasInitValue("xrange") && HasInitValue("yrange")
       && HasInitValue("zrange")) {
      vector<OC_REAL8m> xrange,yrange,zrange;
      GetRealVectorInitValue("xrange",2,xrange);
      GetRealVectorInitValue("yrange",2,yrange);
      GetRealVectorInitValue("zrange",2,zrange);
      if(!bbox.CheckOrderSet(xrange[0],xrange[1],
			     yrange[0],yrange[1],
			     zrange[0],zrange[1])) {
	throw Oxs_ExtError(this,"Bad order in x/y/zrange data.");
      }
      bbox_set = 1;
    }
  }

  ThreeVector maxpt,span;
  if(bbox_set) {
    basept.Set(bbox.GetMinX(),bbox.GetMinY(),bbox.GetMinZ());
    maxpt.Set(bbox.GetMaxX(),bbox.GetMaxY(),bbox.GetMaxZ());
    span = maxpt - basept;
    if(span.x<=0) scale.x = 1.0; // Safety
    else          scale.x = 1.0/span.x;
    if(span.y<=0) scale.y = 1.0;
    else          scale.y = 1.0/span.y;
    if(span.z<=0) scale.z = 1.0;
    else          scale.z = 1.0/span.z;
  }

  String cmdoptreq = GetStringInitValue("script_args","relpt");
  if(bbox_set) {
    command_options.push_back(Nb_TclCommandLineOption("minpt",3));
    command_options.push_back(Nb_TclCommandLineOption("maxpt",3));
    command_options.push_back(Nb_TclCommandLineOption("span",3));
    command_options.push_back(Nb_TclCommandLineOption("relpt",3));
  } else {
    // bbox not set, so disable options that require it
    command_options.push_back(Nb_TclCommandLineOption("minpt",0));
    command_options.push_back(Nb_TclCommandLineOption("maxpt",0));
    command_options.push_back(Nb_TclCommandLineOption("span",0));
    command_options.push_back(Nb_TclCommandLineOption("relpt",0));
  }
  command_options.push_back(Nb_TclCommandLineOption("rawpt",3));

  cmd.SetBaseCommand(InstanceName(),
		     director->GetMifInterp(),
		     GetStringInitValue("script"),
		     Nb_ParseTclCommandLineRequest(InstanceName(),
                                                   command_options,
                                                   cmdoptreq));
  VerifyAllInitArgsUsed();

  // Fill fixed command line args
  int index;
  index = command_options[0].position; // minpt
  if(index>=0) {
    cmd.SetCommandArg(index,basept.x);
    cmd.SetCommandArg(index+1,basept.y);
    cmd.SetCommandArg(index+2,basept.z);
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

Oxs_ScriptOrientVectorField::~Oxs_ScriptOrientVectorField()
{}

void
Oxs_ScriptOrientVectorField::Value
(const ThreeVector& pt,
 ThreeVector& value) const
{
  cmd.SaveInterpResult();

  // Fill variable command line args
  int index;
  index = command_options[3].position; // relpt
  if(index>=0) {
    cmd.SetCommandArg(index,(pt.x-basept.x)*scale.x);
    cmd.SetCommandArg(index+1,(pt.y-basept.y)*scale.y);
    cmd.SetCommandArg(index+2,(pt.z-basept.z)*scale.z);
  }
  index = command_options[4].position; // rawpt
  if(index>=0) {
    cmd.SetCommandArg(index,pt.x);
    cmd.SetCommandArg(index+1,pt.y);
    cmd.SetCommandArg(index+2,pt.z);
  }

  cmd.Eval();

  if(cmd.GetResultListSize()!=3) {
    String msg
      = String("Return script value is not a triplet of 3 numbers: ");
    if(cmd.GetResultListSize()>0) msg += cmd.GetWholeResult();
    else                          msg += String("{}");
    cmd.RestoreInterpResult();
    throw Oxs_ExtError(this,msg.c_str());
  }
  ThreeVector newpt;
  cmd.GetResultListItem(0,newpt.x);
  cmd.GetResultListItem(1,newpt.y);
  cmd.GetResultListItem(2,newpt.z);

  cmd.RestoreInterpResult();

  // Evalute vector field at newpt
  field->Value(newpt,value);
}

void
Oxs_ScriptOrientVectorField::FillMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<ThreeVector>& array) const
{
  DefaultFillMeshValue(mesh,array);
}
