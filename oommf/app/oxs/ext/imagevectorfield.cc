/* FILE: imagevectorfield.cc      -*-Mode: c++-*-
 *
 * Image vector field object, derived from Oxs_VectorField class.
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
#include "imagevectorfield.h"
#include "energy.h"             // Needed to make MSVC++ 5 happy

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_ImageVectorField);

/* End includes */

// Constructor
Oxs_ImageVectorField::Oxs_ImageVectorField(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_VectorField(name,newdtr,argstr),
    view(xy), exterior(EXTERIOR_INVALID),default_value(0,0,0),
    image_width(0),image_height(0),
    column_offset(0),column_scale(1),
    row_offset(0),row_scale(1)
{
  // Process arguments.

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
                           " Please specify atlas or x/y/zrange.");
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

  // Viewplane
  String viewstr = GetStringInitValue("viewplane","xy");
  if(viewstr.compare("xy")==0)      view = xy;
  else if(viewstr.compare("zx")==0) view = zx;
  else if(viewstr.compare("yz")==0) view = yz;
  else {
    String msg = String("Invalid viewplane init string: ")
      + viewstr + String("; should be one of xy, zx or yz.");
    throw Oxs_ExtError(this,msg.c_str());
  }

  // Input file
  String image_filename = GetStringInitValue("image");

  // Value scaling and offset
  OC_REAL8m xmult=1.0,ymult=1.0,zmult=1.0;
  if(HasInitValue("multiplier")) {
    xmult = ymult = zmult = GetRealInitValue("multiplier");
  } else {
    xmult = GetRealInitValue("vx_multiplier",1.0);
    ymult = GetRealInitValue("vy_multiplier",1.0);
    zmult = GetRealInitValue("vz_multiplier",1.0);
  }
  OC_REAL8m xoff = GetRealInitValue("vx_offset",0.0);
  OC_REAL8m yoff = GetRealInitValue("vy_offset",0.0);
  OC_REAL8m zoff = GetRealInitValue("vz_offset",0.0);
  ThreeVector voff(xoff,yoff,zoff);

  OC_BOOL set_norm=0;
  OC_REAL8m norm=1.0;
  if(HasInitValue("norm")) {
    norm = GetRealInitValue("norm");
    set_norm = 1;
  }

  // Exterior handling
  vector<String> params;
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

  VerifyAllInitArgsUsed();

  // Load image into an Nb_ImgObj object
  Nb_ImgObj image;
  image.LoadFile(image_filename.c_str());

  // Fill value_array with values
  image_width = image.Width();
  image_height = image.Height();
  OC_INT4m maxvalue = image.MaxValue();
  if(maxvalue<1) {
    String msg = String("Invalid image file: \"")
      + image_filename + String("\"");
    throw Oxs_ExtError(this,msg);
  }
  OC_REAL8m pix_scale = 1.0/static_cast<OC_REAL8m>(maxvalue);
  if(image_width<1 || image_height<1) {
    String msg = String("Input image file ")
      + String(image_filename) + String(" is an empty image");
    throw Oxs_ExtError(this,msg.c_str());
  }
  for(OC_INT4m i=0;i<image_height;++i) {
    for(OC_INT4m j=0;j<image_width;++j) {
      OC_INT4m r,g,b;
      image.PixelValue(i,j,r,g,b);
      OC_REAL8m red   = static_cast<OC_REAL8m>(r)*pix_scale;
      OC_REAL8m green = static_cast<OC_REAL8m>(g)*pix_scale;
      OC_REAL8m blue  = static_cast<OC_REAL8m>(b)*pix_scale;
      ThreeVector value(red,green,blue);
      if(set_norm) value.SetMag(norm);
      value.x *= xmult;
      value.y *= ymult;
      value.z *= zmult;
      value += voff;
      value_array.push_back(value);
    }
  }
  if(set_norm) {
    default_value.SetMag(norm);
  }
  default_value.x *= xmult;
  default_value.y *= ymult;
  default_value.z *= zmult;
  default_value += voff;

  const vector<ThreeVector>::size_type image_size = value_array.size();
  if(view == zx) { // Transform values to view
    for(vector<ThreeVector>::size_type index=0;index<image_size;index++) {
      ThreeVector temp = value_array[index];
      value_array[index].Set(temp.y,temp.z,temp.x);
    }
    ThreeVector temp = default_value;
    default_value.Set(temp.y,temp.z,temp.x);
  } else if(view == yz) {
    for(vector<ThreeVector>::size_type index=0;index<image_size;index++) {
      ThreeVector temp = value_array[index];
      value_array[index].Set(temp.z,temp.x,temp.y);
    }
    ThreeVector temp = default_value;
    default_value.Set(temp.z,temp.x,temp.y);
  }

  // Initialize transformation coefficients
  switch(view) {
  case xy:
    column_offset = bbox.GetMinX();
    column_scale  =  static_cast<OC_REAL8m>(image_width)
      /(bbox.GetMaxX()-bbox.GetMinX());
    row_offset = bbox.GetMinY();
    row_scale  = static_cast<OC_REAL8m>(image_height)
      /(bbox.GetMaxY()-bbox.GetMinY());
    break;
  case zx:
    column_offset = bbox.GetMinZ();
    column_scale  = static_cast<OC_REAL8m>(image_width)
      /(bbox.GetMaxZ()-bbox.GetMinZ());
    row_offset = bbox.GetMinX();
    row_scale  = static_cast<OC_REAL8m>(image_height)
      /(bbox.GetMaxX()-bbox.GetMinX());
    break;
  case yz:
    column_offset = bbox.GetMinY();
    column_scale  = static_cast<OC_REAL8m>(image_width)
      /(bbox.GetMaxY()-bbox.GetMinY());
    row_offset = bbox.GetMinZ();
    row_scale  = static_cast<OC_REAL8m>(image_height)
      /(bbox.GetMaxZ()-bbox.GetMinZ());
    break;
  }
}

Oxs_ImageVectorField::~Oxs_ImageVectorField()
{}


void
Oxs_ImageVectorField::Value
(const ThreeVector& pt,
 ThreeVector& value) const
{
  OC_REAL8m image_x=0,image_y=0;
  switch(view) {
  case xy:
    image_x = (pt.x - column_offset)*column_scale;
    image_y = (pt.y - row_offset)*row_scale;
    break;
  case zx:
    image_x = (pt.z - column_offset)*column_scale;
    image_y = (pt.x - row_offset)*row_scale;
    break;
  case yz:
    image_x = (pt.y - column_offset)*column_scale;
    image_y = (pt.z - row_offset)*row_scale;
    break;
  default:
    char buf[128];
    Oc_Snprintf(buf,sizeof(buf),"Invalid view request: %d",
                static_cast<int>(view));
    throw Oxs_ExtError(this,buf);
  }
  OC_INT4m i = static_cast<OC_INT4m>(floor(image_x));
  OC_INT4m j = static_cast<OC_INT4m>(floor(image_y));

  OC_BOOL outside=0;
  if(i<0) {
    i=0; outside=1;
  } else if(i>=image_width) {
    i=image_width-1; outside=1;
  }
  if(j<0) {
    j=0; outside=1;
  } else if(j>=image_height) {
    j=image_height-1; outside=1;
  }
  if(!outside || exterior==EXTERIOR_BOUNDARY) {
    OC_INT4m index = i + (image_height-1-j)*image_width;
    value = value_array[index];
  } else {
    if(exterior==EXTERIOR_DEFAULT) {
      value = default_value;
    } else {
      char buf[1024];
      Oc_Snprintf(buf,sizeof(buf),"Requested point (%g,%g,%g)"
                  " outside mesh range: (%g,%g,%g) x (%g,%g,%g)"
                  " (Error in member function Oxs_ImageVectorField::Value)",
                  static_cast<double>(pt.x),
                  static_cast<double>(pt.y),
                  static_cast<double>(pt.z),
                  static_cast<double>(bbox.GetMinX()),
                  static_cast<double>(bbox.GetMinY()),
                  static_cast<double>(bbox.GetMinZ()),
                  static_cast<double>(bbox.GetMaxX()),
                  static_cast<double>(bbox.GetMaxY()),
                  static_cast<double>(bbox.GetMaxZ()));
      throw Oxs_ExtError(this,buf);
    }
  }
}
