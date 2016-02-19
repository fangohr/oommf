/* FILE: scriptscalarfield.cc      -*-Mode: c++-*-
 *
 * Tcl script scalar field object, derived from Oxs_ScalarField
 * class.
 *
 */

#include <string>

#include "oc.h"
#include "nb.h"

#include "atlas.h"
#include "util.h"
#include "threevector.h"
#include "meshvalue.h"
#include "director.h"
#include "scriptscalarfield.h"
#include "energy.h"		// Needed to make MSVC++ 5 happy

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_ScriptScalarField);

/* End includes */

// Constructor
Oxs_ScriptScalarField::Oxs_ScriptScalarField(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_ScalarField(name,newdtr,argstr)
{
  // Optional multiplier value
  multiplier = GetRealInitValue("multiplier",1.0);

  // Scalar and vector field lists
  if(HasInitValue("scalar_fields")) {
    vector<String> speclist;
    GetGroupedStringListInitValue("scalar_fields",speclist);
    OC_INDEX field_count = static_cast<OC_INDEX>(speclist.size());
    if(field_count>0) {
      scalarfields.SetSize(field_count);
      Nb_SplitList paramlist;
      vector<String> params;
      for(OC_INDEX i=0;i<field_count;i++) {
	paramlist.Split(speclist[i]);
	paramlist.FillParams(params);
	OXS_GET_EXT_OBJECT(params,Oxs_ScalarField,scalarfields[i]);
      }
    }
  }
  if(HasInitValue("vector_fields")) {
    vector<String> speclist;
    GetGroupedStringListInitValue("vector_fields",speclist);
    OC_INDEX field_count = static_cast<OC_INDEX>(speclist.size());
    if(field_count>0) {
      vectorfields.SetSize(field_count);
      Nb_SplitList paramlist;
      vector<String> params;
      for(OC_INDEX i=0;i<field_count;i++) {
	paramlist.Split(speclist[i]);
	paramlist.FillParams(params);
	OXS_GET_EXT_OBJECT(params,Oxs_VectorField,vectorfields[i]);
      }
    }
  }
  if(HasInitValue("scalar_field")) {
    throw Oxs_ExtError(this,"Unrecognized parameter label \"scalar_field\"."
                       " Did you mean \"scalar_fields\"?");
  }
  if(HasInitValue("vector_field")) {
    throw Oxs_ExtError(this,"Unrecognized parameter label \"vector_field\"."
                       " Did you mean \"vector_fields\"?");
  }

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
  command_options.push_back(Nb_TclCommandLineOption("scalars",
                             static_cast<int>(scalarfields.GetSize())));
  command_options.push_back(Nb_TclCommandLineOption("vectors",
                             static_cast<int>(3*vectorfields.GetSize())));

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

Oxs_ScriptScalarField::~Oxs_ScriptScalarField()
{}

OC_REAL8m
Oxs_ScriptScalarField::Value
(const ThreeVector& pt) const
{
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
  index = command_options[5].position; // scalars
  if(index>=0) {
    for(int i=0; i < static_cast<int>(scalarfields.GetSize()); i++) {
      cmd.SetCommandArg(index+i,scalarfields[OC_INDEX(i)]->Value(pt));
    }
  }
  index = command_options[6].position; // vectors
  if(index>=0) {
    for(int i=0; i < static_cast<int>(vectorfields.GetSize()); i++) {
      ThreeVector val;
      vectorfields[OC_INDEX(i)]->Value(pt,val);
      cmd.SetCommandArg(index+3*i,val.x);
      cmd.SetCommandArg(index+3*i+1,val.y);
      cmd.SetCommandArg(index+3*i+2,val.z);
    }
  }

  cmd.SaveInterpResult();

  cmd.Eval();

  if(cmd.GetResultListSize()!=1) {
    String msg
      = String("Return script value is not a single scalar: ")
      + cmd.GetWholeResult();
    cmd.RestoreInterpResult();
    throw Oxs_ExtError(this,msg.c_str());
  }
  OC_REAL8m result;
  cmd.GetResultListItem(0,result);

  cmd.RestoreInterpResult();

  return multiplier*result;
}

void
Oxs_ScriptScalarField::FillMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& array) const
{
  DefaultFillMeshValue(mesh,array);
}

void
Oxs_ScriptScalarField::IncrMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& array) const
{
  DefaultIncrMeshValue(mesh,array);
}

void
Oxs_ScriptScalarField::MultMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& array) const
{
  DefaultMultMeshValue(mesh,array);
}

