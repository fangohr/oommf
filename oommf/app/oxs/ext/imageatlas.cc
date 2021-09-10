/* FILE: imageatlas.cc                 -*-Mode: c++-*-
 *
 * Atlas class derived from Oxs_Atlas class that uses an import
 * image file for region demarcation.
 */

#include <cctype>  // For isspace

#include "nb.h"
#include "director.h"
#include "imageatlas.h"

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_ImageAtlas);

/* End includes */

// Support routines
OC_BOOL OxsImageAtlasColorData::SetColor(const String& colorstr)
{
  return Nb_GetColor(colorstr.c_str(),red,green,blue);
}

OC_REAL8m
OxsImageAtlasColorData::DistanceSq(OC_REAL8m r,OC_REAL8m g,OC_REAL8m b) const
{
  r-=red;   r*=r;
  g-=green; g*=g;
  b-=blue;  b*=b;
  return r+g+b;
}

OC_BOOL
Oxs_ImageAtlas::FindBestMatchRegion
(OC_REAL8m r, OC_REAL8m g, OC_REAL8m b,
 const vector<OxsImageAtlasColorData>& color_data,
 OC_INDEX& match_id, OC_REAL8m& match_distsq)
{ // Returns 0 if color_data is empty.
  // Otherwise sets match_id and match_distsq to best
  // fit, and returns 1.
  vector<OxsImageAtlasColorData>::const_iterator it
    = color_data.begin();
  if(it==color_data.end()) {
    return 0; // Error
  }
  match_id = it->region_id;
  match_distsq = it->DistanceSq(r,g,b);
  while(++it!=color_data.end()) {
    OC_REAL8m distsq = it->DistanceSq(r,g,b);
    if(distsq < match_distsq) {
      match_distsq = distsq;
      match_id = it->region_id;
    }
  }
  return 1;
}

void
Oxs_ImageAtlas::FillPixels
(const char* filename,
 vector<OxsImageAtlasColorData>& color_data,
 OC_REAL8m match_error,
 OC_INDEX default_id,
 int autocolor)
{
  // Note: The last parameter, autocolor, is a temporary hack for large
  //       region count support.

  String msg; // Error message string.  Define it here so we can
  /// use it inside case blocks of switch statement. Otherwise
  /// VC++ 6.0 complains about "skipped" initializations.
  String imagestring; // Raw image buffer space

  // Load image into an Nb_ImgObj object
  Nb_ImgObj image;
  image.LoadFile(filename);

  // Read and categorize pixels
  image_width = image.Width();
  image_height = image.Height();
  OC_REAL8m invmaxvalue = 1.0/static_cast<OC_REAL8m>(image.MaxValue());
  OC_INDEX pixelcount = image_width*image_height;
  if(pixelcount<1) { // This check insures image width & height > 0.
    msg = String("Input image file ")
      + String(filename) + String(" is an empty image");
    throw Oxs_ExtError(this,msg.c_str());
  }

  if(pixel!=NULL) delete[] pixel;
  pixel = new OxsImageAtlasPixelType[pixelcount];
  if(pixel==NULL) {
    msg = String("Insufficient memory to create pixel image of input file ")
      + String(filename);
    throw Oxs_ExtError(this,msg.c_str());
  }

  OC_REAL8m distcheck = match_error+5*OC_REAL8_EPSILON; distcheck *= distcheck;
  for(OC_INDEX i=0;i<image_height;++i) {
    OC_INDEX base_offset = (image_height-1-i)*image_width;
    for(OC_INDEX j=0;j<image_width;++j) {
      OC_INT4m rcomp,gcomp,bcomp;
      image.PixelValue(static_cast<OC_INT4m>(i),static_cast<OC_INT4m>(j),
                       rcomp,gcomp,bcomp);
      OC_REAL8m red   = static_cast<OC_REAL8m>(rcomp)*invmaxvalue;
      OC_REAL8m green = static_cast<OC_REAL8m>(gcomp)*invmaxvalue;
      OC_REAL8m blue  = static_cast<OC_REAL8m>(bcomp)*invmaxvalue;
      // Image is read in from upper lefthand corner, in normal English
      // reading order.  Pixel coords are based at bottom lefthand corner.
      // The "offset" calculation here accounts for these differences.
      OC_INDEX offset = base_offset + j;
      OC_INDEX id;
      OC_REAL8m distsq;
      if(!FindBestMatchRegion(red,green,blue,color_data,id,distsq)
         || distsq>distcheck) {
        if(!autocolor) {
          id = default_id;
        } else {
          // Autocolor; add new region
          id = static_cast<OC_INDEX>(region_name.size());
          int r = (int)floor(red   * 256); if(r>255) r = 255;
          int g = (int)floor(green * 256); if(g>255) g = 255;
          int b = (int)floor(blue  * 256); if(b>255) b = 255;
          char colorrep[32];
          Oc_Snprintf(colorrep,sizeof(colorrep),"#%02x%02x%02x",r,g,b);
          region_name.push_back(String(colorrep));
          OxsImageAtlasColorData tempcolor;
          tempcolor.region_id = id;
          if(!tempcolor.SetColor(String(colorrep))) {
            char buf[1024];
            Oc_Snprintf(buf,sizeof(buf),
                        "Unrecognized auto-generated color in"
                        " in Specify block: %.32",colorrep);
            throw Oxs_ExtError(this,buf);
          }
          color_data.push_back(tempcolor); // Add color mapping to color_data
        }
      }
      pixel[offset] = static_cast<OxsImageAtlasPixelType>(id);
    }
  }
}

void
Oxs_ImageAtlas::InitializeRegionBBoxes(const Oxs_Box& world)
{ // Uses world, view and pixel data to setup bounding boxes
  // for each region.  NB: view, pixel, image_width,
  // image_height, and row/column_scale/offset variables
  // _must_ be set up *before* calling this function!

  region_bbox.clear(); // Safety
  region_bbox.push_back(world); // Set "universe" region to world

  // Initialize all non-universe region bboxes to empty boxes
  OC_INDEX id;
  Oxs_Box empty_box;
  for(id=1;id<static_cast<OC_INDEX>(region_name.size());id++) {
    region_bbox.push_back(empty_box);
  }

  // For each point in pixel array, expand corresponding region
  // the point in pixel coords.
  OC_INDEX index=0;
  for(OC_INDEX j=0;j<image_height;j++) {
    for(OC_INDEX i=0;i<image_width;i++) {
      OC_INDEX tempid = pixel[index];
      if(tempid>0) { // Skip points in "universe" region.
	region_bbox[tempid].Expand(static_cast<OC_REAL8m>(i),
				   static_cast<OC_REAL8m>(j),
				   0.0);
	// Fix the out-of-plane component to 0 here; it
	// will be expanded to full world size when converting
	// to atlas coordinates below.
      }
      ++index;
    }
  }

  // Now convert from pixel coords to atlas coords.
  OC_REAL8m cmult = 1.0/column_scale;
  OC_REAL8m rmult = 1.0/row_scale;
  for(id=1;id<static_cast<OC_INDEX>(region_bbox.size());id++) {
    OC_REAL8m cmin = region_bbox[id].GetMinX();
    OC_REAL8m cmax = region_bbox[id].GetMaxX()+1.0; // Pad to full pixel
    OC_REAL8m rmin = region_bbox[id].GetMinY();
    OC_REAL8m rmax = region_bbox[id].GetMaxY()+1.0; // Pad to full pixel
    cmin = (cmin*cmult) + column_offset;
    cmax = (cmax*cmult) + column_offset;
    rmin = (rmin*rmult) + row_offset;
    rmax = (rmax*rmult) + row_offset;
    switch(view) {
    case xy:
      region_bbox[id].Set(cmin,cmax,
			  rmin,rmax,
			  world.GetMinZ(),world.GetMaxZ());
      break;
    case zx:
      region_bbox[id].Set(rmin,rmax,
			  world.GetMinY(),world.GetMaxY(),
			  cmin,cmax);
      break;
    case yz:
      region_bbox[id].Set(world.GetMinX(),world.GetMaxX(),
			  cmin,cmax,
			  rmin,rmax);

      break;
    }
  }
}

// Constructor
Oxs_ImageAtlas::Oxs_ImageAtlas
( const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr    // MIF block argument string
  ) : Oxs_Atlas(name,newdtr,argstr),
      pixel(NULL),image_width(0),image_height(0),view(xy),
      row_offset(0.),row_scale(1.),
      column_offset(0.),column_scale(1.)
{
  // Process input string
  vector<OC_REAL8m> xrange; GetRealVectorInitValue("xrange",2,xrange);
  vector<OC_REAL8m> yrange; GetRealVectorInitValue("yrange",2,yrange);
  vector<OC_REAL8m> zrange; GetRealVectorInitValue("zrange",2,zrange);
  Oxs_Box world_box;
  if(!world_box.CheckOrderSet(xrange[0],xrange[1],
			      yrange[0],yrange[1],
			      zrange[0],zrange[1])) {
    throw Oxs_ExtError(this,"One of [xyz]range in Specify block"
			 " is improperly ordered.");
  }
  if(world_box.GetVolume()<=0.0) {
    throw Oxs_ExtError(this,"Atlas bounding box has zero volume.");
  }

  String viewstr = GetStringInitValue("viewplane");
  if(viewstr.compare("xy")==0)      view = xy;
  else if(viewstr.compare("zx")==0) view = zx;
  else if(viewstr.compare("yz")==0) view = yz;
  else {
    String msg = String("Invalid viewplane init string: ")
      + viewstr + String("; should be one of xy, zx or yz.");
    throw Oxs_ExtError(this,msg.c_str());
  }

  OC_REAL8m match_error = GetRealInitValue("matcherror",3.);
  // Default is no limit

  // Parse colormap info from Specify block
  vector<OxsImageAtlasColorData> color_data;
  OC_INDEX default_id=0;
  region_name.push_back("universe"); // Required special region, id 0.
  int autocolor = 0;
  if(!HasInitValue("colorfunction") && !HasInitValue("colormap")) {
    String msg = String("Invalid Specify block; at least one of "
                        "\"colormap\" or \"colorfunction\" must"
                        " be specified.");
    throw Oxs_ExtError(this,msg.c_str());
  }
  if(HasInitValue("colorfunction")) {
    String colorfunction = GetStringInitValue("colorfunction");
    if(colorfunction.compare("auto")!=0) {
      String msg = String("Unrecognized colorfunction: \"")
        + colorfunction + String("\"; should be \"auto\".");
      throw Oxs_ExtError(this,msg.c_str());
    }
    autocolor = 1;
  }
  if(HasInitValue("colormap")) {
    vector<String> colormap;
    FindRequiredInitValue("colormap",colormap);
    if(colormap.size()%2!=0) {
      char buf[1024];
      Oc_Snprintf(buf,sizeof(buf),
                  "Colormap entry in Specify block has"
                  " %u entries, which is not an even number",
                  (unsigned int)colormap.size());
      throw Oxs_ExtError(this,buf);
    }
    for(OC_INDEX i=0;i<static_cast<OC_INDEX>(colormap.size());i+=2) {
      // Is label already defined?
      const String& label = colormap[i+1];
      OC_INDEX id;
      for(id=0;id<static_cast<OC_INDEX>(region_name.size());id++) {
        if(region_name[id].compare(label)==0) break;
      }
      if(id>=static_cast<OC_INDEX>(region_name.size())) {
        region_name.push_back(label); // New label
      }
      // id is the index of the label specified by colormap[i+1]
      if(Nb_StrCaseCmp(colormap[i].c_str(),"default")==0) {
        default_id = id;
      } else {
        OxsImageAtlasColorData tempcolor;
        tempcolor.region_id=id;
        if(!tempcolor.SetColor(colormap[i])) {
          char item[275];  // Safety
          item[250]='\0';
          Oc_Snprintf(item,sizeof(item),"%.252s",colormap[i].c_str());
          if(item[250]!='\0') strcpy(item+250,"..."); // Overflow
          char buf[1024];
          Oc_Snprintf(buf,sizeof(buf),
                      "Unrecognized color in colormap"
                      " in Specify block: %.255s",
                      item);
          throw Oxs_ExtError(this,buf);
        }
        color_data.push_back(tempcolor); // Add color mapping to color_data
      }
    }
    size_t maxsize
      = (( size_t(1) <<(8*sizeof(OxsImageAtlasPixelType)-1))-1)*2+1;
    if(region_name.size()-1 > maxsize) {
      // Note: the "-1" in the previous line accounts for the
      // "universe" region; since this region is generally not
      // explicitly specified by the user, we don't include it
      // in the following error message.
      char buf[1024];
      Oc_Snprintf(buf,sizeof(buf),
                  "Too many regions: %" OC_INDEX_MOD "u "
                  "(max allowed is %" OC_INDEX_MOD "u)",
                  (OC_UINDEX)(region_name.size()-1),
                  (OC_UINDEX)maxsize);
      throw Oxs_ExtError(this,buf);
    }
    DeleteInitValue("colormap");
  }

  String image_file = GetStringInitValue("image");

  VerifyAllInitArgsUsed();

  // Initialize pixel map
  FillPixels(image_file.c_str(),color_data,match_error,default_id,autocolor);

  // Initialize transformation coefficients
  switch(view) {
  case xy:
    column_offset = world_box.GetMinX();
    column_scale  =  static_cast<OC_REAL8m>(image_width)
      /(world_box.GetMaxX()-world_box.GetMinX());
    row_offset = world_box.GetMinY();
    row_scale  = static_cast<OC_REAL8m>(image_height)
      /(world_box.GetMaxY()-world_box.GetMinY());
    break;
  case zx:
    column_offset = world_box.GetMinZ();
    column_scale  = static_cast<OC_REAL8m>(image_width)
      /(world_box.GetMaxZ()-world_box.GetMinZ());
    row_offset = world_box.GetMinX();
    row_scale  = static_cast<OC_REAL8m>(image_height)
      /(world_box.GetMaxX()-world_box.GetMinX());
    break;
  case yz:
    column_offset = world_box.GetMinY();
    column_scale  = static_cast<OC_REAL8m>(image_width)
      /(world_box.GetMaxY()-world_box.GetMinY());
    row_offset = world_box.GetMinZ();
    row_scale  = static_cast<OC_REAL8m>(image_height)
      /(world_box.GetMaxZ()-world_box.GetMinZ());
    break;
  }

  // Iterate through points to find bboxes for each region
  InitializeRegionBBoxes(world_box);
}


// Destructor
Oxs_ImageAtlas::~Oxs_ImageAtlas()
{
  if(pixel!=NULL) delete[] pixel;
}

OC_BOOL Oxs_ImageAtlas::GetRegionExtents(OC_INDEX id,Oxs_Box &mybox) const
{ // If 0<id<GetRegionCount, then sets mybox to bounding box
  // for the specified region, and returns 1.  If id is 0,
  // sets mybox to atlas bounding box, and returns 1.
  // Otherwise, leaves mybox untouched and returns 0.
  if(id>=GetRegionCount()) return 0;
  mybox = region_bbox[id]; // Atlas bbox is stored in region_bbox[0].
  return 1;
}

OC_INDEX Oxs_ImageAtlas::GetRegionId(const ThreeVector& point) const
{ // Returns the id number for the region containing point.
  // The return value is 0 if the point is not contained in
  // the atlas, i.e., belongs to the "universe" region.

  if(!region_bbox[0].IsIn(point)) return 0; // Outside atlas

  OC_REAL8m mapx=0,mapy=0;
  switch(view) {
  case xy: mapx = point.x; mapy = point.y; break;
  case zx: mapx = point.z; mapy = point.x; break;
  case yz: mapx = point.y; mapy = point.z; break;
  }
  
  OC_REAL8m tempc = floor((mapx-column_offset)*column_scale);
  OC_INDEX column_index = 0;
  if(tempc>=image_width) column_index = image_width-1;
  else if(tempc>0) column_index = static_cast<OC_INDEX>(tempc);

  OC_REAL8m tempr = floor((mapy-row_offset)*row_scale);
  OC_INDEX row_index = 0;
  if(tempr>=image_height) row_index = image_height-1;
  else if(tempr>0) row_index = static_cast<OC_INDEX>(tempr);

  OC_INDEX index = row_index*image_width+column_index;

  OC_INDEX id = pixel[index];
  if(id>=GetRegionCount()) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
		"Programming error detected in "
		"Oxs_ImageAtlas::GetRegionId(const ThreeVector&) const."
		"  Region id %u discovered in pixel array;"
		" max value should be %u",id,GetRegionCount()-1);
    throw Oxs_ExtError(this,buf);
  }

  return id;
}

OC_INDEX Oxs_ImageAtlas::GetRegionId(const String& name) const
{ // Given a region id string (name), returns
  // the corresponding region id index.  If
  // "name" is not included in the atlas, then
  // -1 is returned.  Note: If name == "universe",
  // then the return value will be 0.
  OC_INDEX count = GetRegionCount();
  for(OC_INDEX i=0;i<count;i++) {
    if(region_name[i].compare(name)==0) return i;
  }
  return -1;
}

OC_BOOL Oxs_ImageAtlas::GetRegionName(OC_INDEX id,String& name) const
{ // Given an id number, fills in "name" with
  // the corresponding region id string.  Returns
  // 1 on success, 0 if id is invalid.  If id is 0,
  // then name is set to "universe", and the return
  // value is 1.
  name.erase();
  if(id>=GetRegionCount()) return 0;
  name = region_name[id];
  return 1;
}
