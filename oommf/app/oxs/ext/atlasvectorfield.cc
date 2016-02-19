/* FILE: atlasvectorfield.cc      -*-Mode: c++-*-
 *
 * Atlas vector field object, derived from Oxs_VectorField
 * class.
 *
 */

#include <string>

#include "oc.h"
#include "nb.h"

#include "util.h"
#include "threevector.h"
#include "meshvalue.h"
#include "director.h"
#include "atlas.h"
#include "atlasvectorfield.h"
#include "energy.h"             // Needed to make MSVC++ 5 happy

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_AtlasVectorField);

/* End includes */

// Constructor
Oxs_AtlasVectorField::Oxs_AtlasVectorField(
  const char* xname,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_VectorField(xname,newdtr,argstr),
    set_norm(0),norm(0.0), multiplier(1.0)
{
  // Process arguments
  OXS_GET_INIT_EXT_OBJECT("atlas",Oxs_Atlas,atlas_obj);
  atlas_key.Set(atlas_obj.GetPtr());
  atlas_key.GetReadReference(); // Read lock held until *this is deleted.

  if(HasInitValue("norm")) {
    norm = GetRealInitValue("norm");
    set_norm = 1;
  }

  multiplier = GetRealInitValue("multiplier",1.0);

  // Optional "default_value"
  if(HasInitValue("default_value")) {
    OXS_GET_INIT_EXT_OBJECT("default_value",Oxs_VectorField,
                            default_value.field_init);
    default_value.value_is_set = 1;
  }

  // Set up value array.
  values.SetSize(atlas_obj->GetRegionCount());

  // Get and fill value vector with non-default values
  size_t i;
  vector<String> params;
  FindRequiredInitValue("values",params);
  if(params.size()%2!=0) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
                "List associated with parameter \"values\" has"
                " %u entries, which is not an even number.",
                (unsigned int)params.size());
    throw Oxs_ExtError(this,buf);
  }
  for(i=0;i<params.size();i+=2) {
    const String& region_name = params[i];
    OC_INDEX region_id = atlas_obj->GetRegionId(region_name);
    if(region_id<0) {
      char item[275];  // Safety
      item[250]='\0';
      Oc_Snprintf(item,sizeof(item),"%.252s",region_name.c_str());
      if(item[250]!='\0') strcpy(item+250,"..."); // Overflow
      char buf[2000];
      Oc_Snprintf(buf,sizeof(buf),
                  "Specified name, \"%.255s\","
                  " is not a known region in atlas \"%.1000s\"."
                  "  Known regions:",
                  item,atlas_obj->InstanceName());
      String msg = String(buf);
      vector<String> regions;
      atlas_obj->GetRegionList(regions);
      for(unsigned int j=0;j<regions.size();++j) {
        msg += String("\n ");
        msg += regions[j];
      }
      throw Oxs_ExtError(this,msg);
    }
    if(values[region_id].value_is_set) {
      char item[275];  // Safety
      item[250]='\0';
      Oc_Snprintf(item,sizeof(item),"%.252s",region_name.c_str());
      if(item[250]!='\0') strcpy(item+250,"..."); // Overflow
      char buf[2000];
      Oc_Snprintf(buf,sizeof(buf),
        "Region \"%.255s\" in atlas \"%.1000s\" is set more than once.",
        item,atlas_obj->InstanceName());
      throw Oxs_ExtError(this,buf);
    }
    Nb_SplitList field_paramlist;
    vector<String> field_params;
    field_paramlist.Split(params[i+1]);
    field_paramlist.FillParams(field_params);
    OXS_GET_EXT_OBJECT(field_params,Oxs_VectorField,
                       values[region_id].field_init);
    values[region_id].value_is_set = 1;
  }
  DeleteInitValue("values");

  VerifyAllInitArgsUsed();
}

void
Oxs_AtlasVectorField::Value
(const ThreeVector& pt,
 ThreeVector& export_value) const
{
  const Oxs_Atlas* atlas = atlas_key.GetPtr();
  OC_INDEX id = atlas->GetRegionId(pt);
  if(values[id].value_is_set) {
    values[id].field_init->Value(pt,export_value);
  } else if(default_value.value_is_set) {
    default_value.field_init->Value(pt,export_value);
  } else {
    String region_name;
    atlas->GetRegionName(id,region_name);
    char item[275];  // Safety
    item[250]='\0';
    Oc_Snprintf(item,sizeof(item),"%.252s",region_name.c_str());
    if(item[250]!='\0') strcpy(item+250,"..."); // Overflow
    char buf[2000];
    Oc_Snprintf(buf,sizeof(buf),
                "Error in function Value(): input point (%g,%g,%g)"
                " in region \"%.255s\" of atlas \"%.1000s\""
                " has not been assigned a value.",
                static_cast<double>(pt.x),
                static_cast<double>(pt.y),
                static_cast<double>(pt.z),
                item,atlas->InstanceName());
    throw Oxs_ExtError(this,buf);
  }
  if(set_norm) {
    if(norm==1.0) export_value.MakeUnit();
    else          export_value.SetMag(norm);
  }
  export_value *= multiplier;
}
