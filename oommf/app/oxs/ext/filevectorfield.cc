/* FILE: filevectorfield.cc      -*-Mode: c++-*-
 *
 * File vector field object, derived from Oxs_VectorField class.
 *
 */

#include <string>

#include "oc.h"
#include "nb.h"
#include "vf.h"

#include "atlas.h"
#include "threevector.h"
#include "meshvalue.h"
#include "director.h"
#include "filevectorfield.h"
#include "energy.h"             // Needed to make MSVC++ 5 happy

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_FileVectorField);

/* End includes */


// Constructor
Oxs_FileVectorField::Oxs_FileVectorField(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_VectorField(name,newdtr,argstr),
    file_mesh(NULL),set_norm(0),norm(0.0),
    exterior(EXTERIOR_ERROR)
{
  // Process arguments.
  vector<String> params;

  if(HasInitValue("norm")) {
    norm = GetRealInitValue("norm");
    set_norm = 1;
  }

  multiplier = GetRealInitValue("multiplier",1.0);

  String filename=GetStringInitValue("file");

  if(FindInitValue("exterior",params)) {
    if(params.size()==1) {
      if(params[0].compare("error")==0) {
        exterior = EXTERIOR_ERROR;
      } else if(params[0].compare("boundary")==0) {
        exterior = EXTERIOR_BOUNDARY;
      } else {
        String initstr;
        FindInitValue("exterior",initstr);
        String msg = String("Invalid exterior init string: ")
          + initstr
          + String("; should be error, boundary, or a three vector.");
        throw Oxs_ExtError(this,msg.c_str());
      }
      DeleteInitValue("exterior");
    } else {
      default_value = GetThreeVectorInitValue("exterior");
      exterior = EXTERIOR_DEFAULT;
    }
  }

  // Determine spatial scaling and offset.
  OC_BOOL auto_scale = 1;
  OC_BOOL auto_offset = 1;
  if(FindInitValue("spatial_scaling",params)) {
    if(params.size()==1) {
      if(params[0].compare("auto")==0) {
        auto_scale = 1; // Safety
      } else {
        String initstr;
        FindInitValue("spatial_scaling",initstr);
        String msg = String("Invalid spatial_scaling init string: ")
          + initstr
          + String("; should be either auto or a three vector.");
        throw Oxs_ExtError(this,msg.c_str());
      }
      DeleteInitValue("spatial_scaling");
    } else {
      scale = GetThreeVectorInitValue("spatial_scaling");
      auto_scale = 0;
    }
  }
  if(FindInitValue("spatial_offset",params)) {
    if(params.size()==1) {
      if(params[0].compare("auto")==0) {
        auto_offset = 1; // Safety
      } else {
        String initstr;
        FindInitValue("spatial_offset",initstr);
        String msg = String("Invalid spatial_offset init string: ")
          + initstr
          + String("; should be either auto or a three vector.");
        throw Oxs_ExtError(this,msg.c_str());
      }
      DeleteInitValue("spatial_offset");
    } else {
      offset = GetThreeVectorInitValue("spatial_offset");
      auto_offset = 0;
    }
  }

  Oxs_Box bbox;
  if(auto_scale || auto_offset) {
    // Determine bounding box
    if(HasInitValue("atlas")) {
      // Get bounding box from specified atlas.
      Oxs_OwnedPointer<Oxs_Atlas> atlas;
      OXS_GET_INIT_EXT_OBJECT("atlas",Oxs_Atlas,atlas);
      atlas->GetWorldExtents(bbox);
    } else {
      // Atlas not specified, so check for and use x/y/zrange data.
      if(!HasInitValue("xrange") || !HasInitValue("yrange")
         || !HasInitValue("zrange")) {
        throw Oxs_ExtError(this,"Can't determine scaling and/or offset;"
                             " Please specify atlas, x/y/zrange, or"
                             " explicit spatial scaling and offset.");
      }
      vector<OC_REAL8m> xrange,yrange,zrange;
      GetRealVectorInitValue("xrange",2,xrange);
      GetRealVectorInitValue("yrange",2,yrange);
      GetRealVectorInitValue("zrange",2,zrange);
      if(!bbox.CheckOrderSet(xrange[0],xrange[1],
                             yrange[0],yrange[1],
                             zrange[0],zrange[1])) {
        throw Oxs_ExtError(this,"Bad order in x/y/zrange data.");
      }
    }
  }

  VerifyAllInitArgsUsed();

  // Create and load Vf_Mesh object with file data
  Vf_FileInput* vffi=Vf_FileInput::NewReader(filename.c_str());
  if(vffi==NULL) {
    String msg=String("Error detected during construction of ")
      + String(InstanceName()) + String("; Error reading input file ")
      + filename;
    throw Oxs_ExtError(msg.c_str());
  }
  file_mesh=vffi->NewMesh();
  delete vffi;

  if(strcmp("Vf_EmptyMesh",file_mesh->GetMeshType())==0) {
    delete file_mesh;
    file_mesh=NULL;
  }

  if(file_mesh==NULL) {
    String msg=String("Error detected during construction of ")
      + String(InstanceName()) + String(" on input file ")
      + filename + String("; Unable to create Vf_Mesh object.");
    throw Oxs_ExtError(msg.c_str());
  }
  file_mesh->GetPreciseRange(file_box);

  if(auto_scale || auto_offset) {
    // Determine scaling
    OC_REAL8m xmin = bbox.GetMinX();
    OC_REAL8m ymin = bbox.GetMinY();
    OC_REAL8m zmin = bbox.GetMinZ();
    OC_REAL8m xrange = bbox.GetMaxX() - xmin;
    OC_REAL8m yrange = bbox.GetMaxY() - ymin;
    OC_REAL8m zrange = bbox.GetMaxZ() - zmin;

    Nb_Vec3<OC_REAL8> file_basept;
    file_box.GetMinPt(file_basept);
    OC_REAL8m file_xmin = file_basept.x;
    OC_REAL8m file_ymin = file_basept.y;
    OC_REAL8m file_zmin = file_basept.z;
    OC_REAL8m file_xrange = file_box.GetWidth();
    OC_REAL8m file_yrange = file_box.GetHeight();
    OC_REAL8m file_zrange = file_box.GetDepth();

    if(xrange>0.) {
      if(auto_scale)  scale.x = file_xrange/xrange;
      if(auto_offset) offset.x = file_xmin - xmin*scale.x;
    } else {
      if(auto_scale)  scale.x = 0.0;
      if(auto_offset) offset.x = file_xmin/2.0;
    }
    if(yrange>0.) {
      if(auto_scale)  scale.y = file_yrange/yrange;
      if(auto_offset) offset.y = file_ymin - ymin*scale.y;
    } else {
      if(auto_scale)  scale.y = 0.0;
      if(auto_offset) offset.y = file_ymin/2.0;
    }
    if(zrange>0.) {
      if(auto_scale)  scale.z = file_zrange/zrange;
      if(auto_offset) offset.z = file_zmin - zmin*scale.z;
    } else {
      if(auto_scale)  scale.z = 0.0;
      if(auto_offset) offset.z = file_zmin/2.0;
    }
  }

  valuescale = file_mesh->GetValueMultiplier();
  valuescale *= multiplier; // For efficiency, roll multiplier
  /// into valuescale.  Note that valuescale is only used if set_norm
  /// is false.  In the case where set_norm is true, multiplier
  /// is explicitly applied after renormalization.
}

Oxs_FileVectorField::~Oxs_FileVectorField()
{
  if(file_mesh!=NULL) delete file_mesh;
}


void
Oxs_FileVectorField::Value
(const ThreeVector& pt,
 ThreeVector& value) const
{
  Nb_Vec3<OC_REAL8> workpt(offset.x+pt.x*scale.x,
                        offset.y+pt.y*scale.y,
                        offset.z+pt.z*scale.z);
  Nb_LocatedVector<OC_REAL8> lv;
  if(exterior==EXTERIOR_BOUNDARY || file_box.IsIn(workpt)) {
    file_mesh->FindPreciseClosest(workpt,lv);
  } else if(exterior==EXTERIOR_DEFAULT) {
    lv.location = workpt;
    lv.value.Set(default_value.x,default_value.y,default_value.z);
  } else {
    Nb_Vec3<OC_REAL8> minpt,maxpt;
    file_box.GetExtremes(minpt,maxpt);
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),"Requested point (%g,%g,%g)-->(%g,%g,%g)"
                " outside mesh range: (%g,%g,%g) x (%g,%g,%g)"
                " (Error in member function Oxs_FileVectorField::Value)",
                static_cast<double>(pt.x),
                static_cast<double>(pt.y),
                static_cast<double>(pt.z),
                static_cast<double>(workpt.x),
                static_cast<double>(workpt.y),
                static_cast<double>(workpt.z),
                static_cast<double>(minpt.x),
                static_cast<double>(minpt.y),
                static_cast<double>(minpt.z),
                static_cast<double>(maxpt.x),
                static_cast<double>(maxpt.y),
                static_cast<double>(maxpt.z));
    throw Oxs_ExtError(this,buf);
  }

  if(set_norm) {
    value.x=lv.value.x;
    value.y=lv.value.y;
    value.z=lv.value.z;
    if(norm==1.0) value.MakeUnit();
    else          value.SetMag(norm);
    if(multiplier!=1.0) value *= multiplier;
  } else {
    // Note: For efficiency, multiplier is already rolled into
    // valuescale.
    value.x=valuescale*lv.value.x;
    value.y=valuescale*lv.value.y;
    value.z=valuescale*lv.value.z;
  }

}

void
Oxs_FileVectorField::FillMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<ThreeVector>& array) const
{
  array.AdjustSize(mesh);

  // Variables for file_mesh (Vf_Mesh*) interface
  Nb_Vec3<OC_REAL8> file_pos;
  Nb_LocatedVector<OC_REAL8> file_lv;
  file_lv.value.Set(1.,0.,0.); // Safety

  // Variables for mesh (Oxs_Mesh*) interface
  ThreeVector center,value;

  // Step through range and sample mesh, fitting
  // with zeroth order fit.
  OC_INDEX size=mesh->Size();
  for(OC_INDEX i=0;i<size;i++) {
    mesh->Center(i,center);
    center.x *= scale.x; center.y *= scale.y; center.z *= scale.z;
    center += offset;
    file_pos.Set(center.x,center.y,center.z);
    if(exterior==EXTERIOR_BOUNDARY || file_box.IsIn(file_pos)) {
      file_mesh->FindPreciseClosest(file_pos,file_lv);
    } else if(exterior==EXTERIOR_DEFAULT) {
      file_lv.location = file_pos;
      file_lv.value.Set(default_value.x,default_value.y,default_value.z);
    } else {
      ThreeVector pt;
      mesh->Center(i,pt);
      Nb_Vec3<OC_REAL8> minpt,maxpt;
      file_box.GetExtremes(minpt,maxpt);
      char buf[1024];
      Oc_Snprintf(buf,sizeof(buf),"Requested point (%g,%g,%g)-->(%g,%g,%g)"
                  " outside mesh range: (%g,%g,%g) x (%g,%g,%g)"
                  " (Error in member function"
                  " Oxs_FileVectorField::FillMeshValue)",
                  static_cast<double>(pt.x),
                  static_cast<double>(pt.y),
                  static_cast<double>(pt.z),
                  static_cast<double>(center.x),
                  static_cast<double>(center.y),
                  static_cast<double>(center.z),
                  static_cast<double>(minpt.x),
                  static_cast<double>(minpt.y),
                  static_cast<double>(minpt.z),
                  static_cast<double>(maxpt.x),
                  static_cast<double>(maxpt.y),
                  static_cast<double>(maxpt.z));
      throw Oxs_ExtError(this,buf);
    }


    if(set_norm) {
      value.x=file_lv.value.x;
      value.y=file_lv.value.y;
      value.z=file_lv.value.z;
      if(norm==1.0) value.MakeUnit();
      else          value.SetMag(norm);
      value *= multiplier;
    } else {
      // Note: For efficiency, multiplier is already rolled into
      // valuescale.
      value.x=valuescale*file_lv.value.x;
      value.y=valuescale*file_lv.value.y;
      value.z=valuescale*file_lv.value.z;
    }
    array[i]=value;
  }

}
