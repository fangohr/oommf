/* FILE: imagescalarfield.cc      -*-Mode: c++-*-
 *
 * Image scalar field object, derived from Oxs_ScalarField class.
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
#include "imagescalarfield.h"
#include "energy.h"		// Needed to make MSVC++ 5 happy

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_ImageScalarField);

/* End includes */

// Constructor
Oxs_ImageScalarField::Oxs_ImageScalarField(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_ScalarField(name,newdtr,argstr),
    view(xy),exterior(EXTERIOR_INVALID),default_value(0),
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
  OC_BOOL invert = GetIntInitValue("invert",0);

  // Value scaling and offset
  OC_REAL8m mult=1.0;
  if(HasInitValue("multiplier")) {
    mult = GetRealInitValue("multiplier");
  }
  OC_REAL8m offset = GetRealInitValue("offset",0.0);

  // Exterior handling
  String exthandling = GetStringInitValue("exterior","error");
  if(exthandling.compare("error")==0) {
    exterior = EXTERIOR_ERROR;
  } else if(exthandling.compare("boundary")==0) {
    exterior = EXTERIOR_BOUNDARY;
  } else {
    OC_BOOL errcheck;
    default_value = Nb_Atof(exthandling.c_str(),errcheck);
    if(errcheck) {
      String msg = String("Invalid exterior init string: \"")
        + exthandling
	+ String("\"; should be error, boundary, or a floating point value.");
      throw Oxs_ExtError(this,msg.c_str());
    }
    exterior = EXTERIOR_DEFAULT;
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
  OC_REAL8m scaling = 1.0/(3.0*static_cast<OC_REAL8m>(maxvalue));
  if(image_width<1 || image_height<1) {
    String msg = String("Input image file ")
      + String(image_filename) + String(" is an empty image");
    throw Oxs_ExtError(this,msg.c_str());
  }
  for(OC_INT4m i=0;i<image_height;++i) {
    for(OC_INT4m j=0;j<image_width;++j) {
      OC_INT4m red,green,blue;
      image.PixelValue(i,j,red,green,blue);
      OC_REAL8m val = static_cast<OC_REAL8m>(red+green+blue)*scaling;
      if(invert) val = 1.0 - val;
      value_array.push_back(mult*val+offset);
    }
  }
  default_value = default_value*mult + offset;

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

Oxs_ImageScalarField::~Oxs_ImageScalarField()
{}


OC_REAL8m
Oxs_ImageScalarField::Value
(const ThreeVector& pt) const
{
  OC_REAL8m image_x=0,image_y=0,value;
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
		  " (Error in member function Oxs_ImageScalarField::Value)",
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
  return value;
}
