/* FILE: display.cc                 -*-Mode: c++-*-
 *
 * OOMMF colormap C++ code
 *
 */

#include "colormap.h"

/* End includes */     // This is an optional directive to build.tcl,
                       // that excludes the remainder of the file from
                       // '#include ".*"' dependency building

#ifndef CODE_CHECKS
#define CODE_CHECKS
#endif

//////////////////////////////////////////////////////////////////////////
// Utility Hue-Saturation-Value to Red-Green-Blue conversion routine,
// based on email from John Hagedorn (john.hagedorn@nist.gov), NIST x4339,
// on 3-Oct-1995.  All imports and exports in the range [0,1].
static void HSVtoRGB(double H,double S,double V,
                     double& R,double& G,double& B)
{
  H-=floor(H);  // Take hue input mod 1
  if(S>1.) S=1.; else if (S<0.) S=0.;
  if(V>1.) V=1.; else if (V<0.) V=0.;
  if(V==0) { R=G=B=0; return; }
  if(S==0) { R=G=B=V; return; }
  int sextant;  double fraction,p,q,t;
  sextant=(int)floor(H*6); fraction=H*6-sextant;
  p=V*(1.-S);  q=V*(1.-S*fraction);  t=V*(1.-S*(1.-fraction));
  switch(sextant) {
  default: // Safety
  case 0: // Red     -> Yellow
    R=V; G=t; B=p; break;
  case 1: // Yellow  -> Green
    R=q; G=V; B=p; break;
  case 2: // Green   -> Cyan
    R=p; G=V; B=t; break;
  case 3: // Cyan    -> Blue
    R=p; G=q; B=V; break;
  case 4: // Blue    -> Magenta
    R=t; G=p; B=V; break;
  case 5: // Magenta -> Red
    R=V; G=p; B=q; break;
  }
}

//////////////////////////////////////////////////////////////////////////
// OommfPackedRGB class
void OommfPackedRGB::Blend(OC_REAL8m wgt1,OommfPackedRGB color1,
			   OC_REAL8m wgt2,OommfPackedRGB color2)
{ // Generalization of Set; does wgt1*color1 + wgt2*color2.
  double newred=wgt1*double(color1.red)+wgt2*double(color2.red);
  if(newred<0.5)           red=0;
  else if(newred>=254.5)   red=255;
  else                     red=(unsigned char)OC_ROUND(newred);
  double newgreen=wgt1*double(color1.green)+wgt2*double(color2.green);
  if(newgreen<0.5)         green=0;
  else if(newgreen>=254.5) green=255;
  else                     green=(unsigned char)OC_ROUND(newgreen);
  double newblue=wgt1*double(color1.blue)+wgt2*double(color2.blue);
  if(newblue<0.5)          blue=0;
  else if(newblue>=254.5)  blue=255;
  else                     blue=(unsigned char)OC_ROUND(newblue);
}

OC_BOOL OommfPackedRGB::Set(const char* color_req)
{ // Converts color_req to rgb format, using Nb_GetColor.
  // Note that Nb_GetColor may call into the global Tcl interp.
  OC_REAL8m r,g,b;
  if(!Nb_GetColor(color_req,r,g,b)) return 0;
  if(r>=1.0) red=255;
  else       red=static_cast<unsigned char>(floor(r*256.));
  if(g>=1.0) green=255;
  else       green=static_cast<unsigned char>(floor(g*256.));
  if(b>=1.0) blue=255;
  else       blue=static_cast<unsigned char>(floor(b*256.));
  return 1;
}


//////////////////////////////////////////////////////////////////////////
// Colormap class
const ClassDoc DisplayColorMap::class_doc("DisplayColorMap",
		    "Michael J. Donahue (michael.donahue@nist.gov)",
		    "1.0.0","15-Dec-1996");

#if (TKCOLORLENGTH != 6)
# error "Current DisplayColorMap code only supports TKCOLORLENGTH==6"
#endif

void DisplayColorMap::Free()
{
  if(TkColor!=NULL) {
    delete[] TkColor[0];
    delete[] TkColor;
    TkColor=NULL;
  }
  if(rgb!=NULL) {
    delete[] rgb;
    rgb=NULL;
  }
  colorcount=0;
  mapname="none";
}

void DisplayColorMap::Alloc(int new_colorcount,const char* new_mapname)
{
  if(new_colorcount<1) new_colorcount = 1;
  if(new_colorcount!=colorcount) {
    Free();
    colorcount=new_colorcount;
    TkColor=new char*[colorcount];
    TkColor[0]=new char[colorcount*(TKCOLORLENGTH+2)];
    /// Leave room for '#' & '\0'
    for(int i=1;i<colorcount;i++) {
      TkColor[i]=TkColor[i-1]+TKCOLORLENGTH+2;
    }
    rgb=new OommfPackedRGB[colorcount];
  }
  mapname=new_mapname;
}

void DisplayColorMap::Setup(int new_colorcount,const char* new_mapname,
			    const char* MakeColor(OC_REAL8m relative_shade))
{
  if(colorcount == new_colorcount
     && strcmp(mapname.GetStr(),new_mapname)==0) {
    return; // Map already set
  }
  if(MakeColor==NULL) {
    Alloc(1,"Black");
    strncpy(TkColor[0],"#000000",TKCOLORLENGTH+2);
    rgb[0].Set(TkColor[0]);
    return;
  }
  Alloc(new_colorcount,new_mapname);
  if(colorcount==1) {
    strncpy(TkColor[0],MakeColor(0.),TKCOLORLENGTH+2);
    rgb[0].Set(TkColor[0]);
  } else {
    for(int i=0;i<colorcount;i++) {
      OC_REAL8m relshade = double(i)/double(colorcount-1);
      strncpy(TkColor[i],MakeColor(relshade),TKCOLORLENGTH+2);
      /// Include room for '#' & '\0'
      rgb[i].Set(TkColor[i]);
    }
  }
}

DisplayColorMap::DisplayColorMap(void)
  : colorcount(0),TkColor(0),rgb(0)
{
#define MEMBERNAME "DisplayColorMap"
  Setup(0,"unknown",NULL);
#undef MEMBERNAME
}

DisplayColorMap::DisplayColorMap(const DisplayColorMap& colormap)
  : colorcount(0),TkColor(0),rgb(0)
{
  *this=colormap;
}

DisplayColorMap& DisplayColorMap::operator=(const DisplayColorMap& colormap)
{
  Alloc(colormap.colorcount,colormap.mapname);
  for(int i=0;i<colorcount;i++) {
    strncpy(TkColor[i],colormap.TkColor[i],TKCOLORLENGTH+2);
    /// Include room for '#' & '\0'
    rgb[i]=colormap.rgb[i];
  }
  return *this;
}

const char* DisplayColorMap::GetColor(OC_REAL4m shade) const
{ // Import shade should in [0.,1.]
  int index=(int)OC_ROUND(shade*OC_REAL4m(colorcount-1));
  if(index<0)                index=0;
  else if(index>=colorcount) index=colorcount-1;
  return TkColor[index];
}

OommfPackedRGB DisplayColorMap::GetPackedColor(OC_REAL4m shade) const
{ // Import shade should in [0.,1.]
  int index=(int)OC_ROUND(shade*OC_REAL4m(colorcount-1));
  if(index<0)                index=0;
  else if(index>=colorcount) index=colorcount-1;
  return rgb[index];
}

void DisplayColorMap::Setup(int _colorcount,const char* _mapname)
{ // A setup routine using the predefined MakeColor routines below.
#define MEMBERNAME "Setup(int,const char*)"
  if(strcmp(_mapname,"Red-Black-Blue")==0) {
    Setup(_colorcount,_mapname,MakeColorRedBlackBlue);
  } else if(strcmp(_mapname,"Blue-White-Red")==0) {
    Setup(_colorcount,_mapname,MakeColorBlueWhiteRed);
  } else if(strcmp(_mapname,"Green-White-Orange")==0) {
    Setup(_colorcount,_mapname,MakeColorGreenWhiteOrange);
  } else if(strcmp(_mapname,"Teal-White-Red")==0) {
    Setup(_colorcount,_mapname,MakeColorTealWhiteRed);
  } else if(strcmp(_mapname,"Black-Gray-White")==0) {
    Setup(_colorcount,_mapname,MakeColorBlackGrayWhite);
  } else if(strcmp(_mapname,"HighContrastGray")==0) {
    Setup(_colorcount,_mapname,MakeColorHighContrastGray);
  } else if(strcmp(_mapname,"Black-White-Black")==0) {
    Setup(_colorcount,_mapname,MakeColorBlackWhiteBlack);
  } else if(strcmp(_mapname,"White-Black-White")==0) {
    Setup(_colorcount,_mapname,MakeColorWhiteBlackWhite);
  } else if(strcmp(_mapname,"Black-White-White")==0) {
    Setup(_colorcount,_mapname,MakeColorBlackWhiteWhite);
  } else if(strcmp(_mapname,"Black-Black-White")==0) {
    Setup(_colorcount,_mapname,MakeColorBlackBlackWhite);
  } else if(strcmp(_mapname,"White-Green-Black")==0) {
    Setup(_colorcount,_mapname,MakeColorWhiteGreenBlack);
  } else if(strcmp(_mapname,"Red-Green-Blue-Red")==0) {
    Setup(_colorcount,_mapname,MakeColorRedGreenBlueRed);
  } else if(strcmp(_mapname,"Cyan-Magenta-Yellow-Cyan")==0) {
    Setup(_colorcount,_mapname,MakeColorCyanMagentaYellowCyan);
  } else {
    FatalError(-1,STDDOC,"Unknown mapname: %s",_mapname);
  }
#undef MEMBERNAME
}

void DisplayColorMap::DefaultColorMapList(Nb_List<const char*>& maps) {
  maps.SetFirst("Red-Black-Blue");
  maps.Append("Blue-White-Red");
  maps.Append("Green-White-Orange");
  maps.Append("Teal-White-Red");
  maps.Append("Black-Gray-White");
  maps.Append("HighContrastGray");
  maps.Append("Black-White-Black");
  maps.Append("White-Black-White");
  maps.Append("Black-Black-White");
  maps.Append("Black-White-White");
  maps.Append("White-Green-Black");
  maps.Append("Red-Green-Blue-Red");
  maps.Append("Cyan-Magenta-Yellow-Cyan");
}

// A few sample MakeColor functions.  These are mapped through
// an arccos, which will display a component map as linear in
// angle.
const char* MakeColorRedBlackBlue(OC_REAL8m relative_shade)
{
  static char color[TKCOLORLENGTH+3]; // 1 for safety
  int value=0;
  if(relative_shade>=1.0) value = 255;
  else if(relative_shade>0.0) {
    double x=acos(1-2*relative_shade);
    value=(int)floor(256*x/PI);
    if(value<0)   value=0;
    if(value>255) value=255;
  }
  int red,green,blue;
  red=blue=0; // To keep dumb compilers quiet
  green=0;
  if(value<128) {
    blue=0;
    red=255-2*value;
  } else if(value>=128) {
    red=0;
    blue=2*(value-128)+1;
  }
  sprintf(color,"#%02X%02X%02X",red,green,blue);
  color[TKCOLORLENGTH+2]='\0'; // Safety
  return color;
}

const char* MakeColorBlueWhiteRed(OC_REAL8m relative_shade)
{
  static char color[TKCOLORLENGTH+3]; // 1 for safety
  int value=0;
  if(relative_shade>=1.0) value = 255;
  else if(relative_shade>0.0) {
    double x=acos(1-2*relative_shade);
    value=(int)floor(256*x/PI);
    if(value<0)   value=0;
    if(value>255) value=255;
  }
  int red,green,blue;
  red=green=blue=0; // To keep dumb compilers quiet
  if(value<128) {
    blue=255;
    red=green=2*value;
  } else if(value>=128) {
    red=255;
    blue=green=510-2*value;
  }
  sprintf(color,"#%02X%02X%02X",red,green,blue);
  color[TKCOLORLENGTH+2]='\0'; // Safety
  return color;
}

const char* MakeColorTealWhiteRed(OC_REAL8m relative_shade)
{ // For color prints and transparencies, fade blue.
  static char color[TKCOLORLENGTH+3]; // 1 for safety
  int value=0;
  if(relative_shade>=1.0) value = 255;
  else if(relative_shade>0.0) {
    double x=acos(1-2*relative_shade);
    value=(int)floor(256*x/PI);
    if(value<0)   value=0;
    if(value>255) value=255;
  }
  int red,green,blue;
  red=green=blue=0; // To keep dumb compilers quiet
  if(value<128) {
    blue=255;
    green=128+value;
    red=2*value;
  } else if(value>=128) {
    red=255;
    blue=green=510-2*value;
  }
  sprintf(color,"#%02X%02X%02X",red,green,blue);
  color[TKCOLORLENGTH+2]='\0'; // Safety
  return color;
}

const char* MakeColorGreenWhiteOrange(OC_REAL8m relative_shade)
{ // Erin Go Braugh
  static char color[TKCOLORLENGTH+3]; // 1 for safety
  int value=0;
  if(relative_shade>=1.0) value = 255;
  else if(relative_shade>0.0) {
    double x=acos(1-2*relative_shade);
    value=(int)floor(256*x/PI);
    if(value<0)   value=0;
    if(value>255) value=255;
  }
  int red,green,blue;
  red=green=blue=0; // To keep dumb compilers quiet
  if(value<128) {
    green=191+value/2;
    red=blue=2*value;
  } else if(value>=128) {
    red=255;
    blue=510-2*value;
    green=345-(45*value)/64;
  }
  sprintf(color,"#%02X%02X%02X",red,green,blue);
  color[TKCOLORLENGTH+2]='\0'; // Safety
  return color;
}

const char* MakeColorBlackGrayWhite(OC_REAL8m relative_shade)
{
  static char color[TKCOLORLENGTH+3]; // 1 for safety
  int value=0;
  if(relative_shade>=1.0) value = 255;
  else if(relative_shade>0.0) {
    double x=acos(1-2*relative_shade);
    value=(int)floor(256*x/PI);
    if(value<0)   value=0;
    if(value>255) value=255;
  }
  sprintf(color,"#%02X%02X%02X",value,value,value);
  color[TKCOLORLENGTH+2]='\0'; // Safety
  return color;
}

const char* MakeColorHighContrastGray(OC_REAL8m relative_shade)
{
  static char color[TKCOLORLENGTH+3]; // 1 for safety
  const double contrast=2.0;
  int value=0;
  if(relative_shade>=1.0) value = 255;
  else if(relative_shade>0.0) {
    double x = relative_shade;
    x = atan(contrast*(2*x-1)/(x*(1-x)))/PI+0.5;
    value=(int)floor(256*x);
    if(value<0)   value=0;
    if(value>255) value=255;
  }
  sprintf(color,"#%02X%02X%02X",value,value,value);
  color[TKCOLORLENGTH+2]='\0'; // Safety
  return color;
}

const char* MakeColorBlackWhiteBlack(OC_REAL8m relative_shade)
{
  static char color[TKCOLORLENGTH+3]; // 1 for safety
  int value=0;
  if(relative_shade>=1.0) value = 0;
  else if(relative_shade>0.0) {
    double x=acos(fabs(1-2*relative_shade));
    value=(int)floor(512*x/PI);
    if(value<0)   value=0;
    if(value>255) value=255;
  }
  sprintf(color,"#%02X%02X%02X",value,value,value);
  color[TKCOLORLENGTH+2]='\0'; // Safety
  return color;
}

const char* MakeColorWhiteBlackWhite(OC_REAL8m relative_shade)
{
  static char color[TKCOLORLENGTH+3]; // 1 for safety
  int value=0;
  if(relative_shade>=1.0) value = 0;
  else if(relative_shade>0.0) {
    double x=acos(fabs(1-2*relative_shade));
    value=(int)floor(512*x/PI);
    if(value<0)   value=0;
    if(value>255) value=255;
  }
  value = 255 - value;
  sprintf(color,"#%02X%02X%02X",value,value,value);
  color[TKCOLORLENGTH+2]='\0'; // Safety
  return color;
}

const char* MakeColorBlackWhiteWhite(OC_REAL8m relative_shade)
{
  static char color[TKCOLORLENGTH+3]; // 1 for safety
  int value=0;
  if(relative_shade>=1.0) value = 255;
  else if(relative_shade>0.0) {
    double x=acos(1-2*relative_shade);
    value=(int)floor(512*x/PI);
    if(value<0)   value=0;
    if(value>255) value=255;
  }
  sprintf(color,"#%02X%02X%02X",value,value,value);
  color[TKCOLORLENGTH+2]='\0'; // Safety
  return color;
}

const char* MakeColorBlackBlackWhite(OC_REAL8m relative_shade)
{
  static char color[TKCOLORLENGTH+3]; // 1 for safety
  int value=0;
  if(relative_shade>=1.0) value = 255;
  else if(relative_shade>0.0) {
    double x=acos(1-2*relative_shade);
    value=(int)floor(512*x/PI)-256;
    if(value<0)   value=0;
    if(value>255) value=255;
  }
  sprintf(color,"#%02X%02X%02X",value,value,value);
  color[TKCOLORLENGTH+2]='\0'; // Safety
  return color;
}

const char* MakeColorWhiteGreenBlack(OC_REAL8m relative_shade)
{
  static char color[TKCOLORLENGTH+3]; // 1 for safety
  int value=0;
  if(relative_shade>=1.0) value = 255;
  else if(relative_shade>0.0) {
    double x=acos(1-2*relative_shade);
    value=(int)floor(256*x/PI);
    if(value<0)   value=0;
    if(value>255) value=255;
  }
  int red,green,blue;
  red=blue=255-value;
  green=255;
  if(value>127) green=510-2*value;
  sprintf(color,"#%02X%02X%02X",red,green,blue);
  color[TKCOLORLENGTH+2]='\0'; // Safety
  return color;
}

const char* MakeColorRedGreenBlueRed(OC_REAL8m relative_shade)
{
  static char color[TKCOLORLENGTH+3]; // 1 for safety
  double R,G,B;
  HSVtoRGB(relative_shade,1.,1.,R,G,B);
  int   red = int(OC_ROUND(R*256.));  if(  red>255)   red=255;
  int green = int(OC_ROUND(G*256.));  if(green>255) green=255;
  int  blue = int(OC_ROUND(B*256.));  if( blue>255)  blue=255;
  sprintf(color,"#%02X%02X%02X",red,green,blue);
  color[TKCOLORLENGTH+2]='\0'; // Safety
  return color;
}

const char* MakeColorCyanMagentaYellowCyan(OC_REAL8m relative_shade)
{ // Phase shift relative to Red-Green-Blue-Red map.
  relative_shade -= 0.5;
  if(relative_shade<0) relative_shade += 1.0;
  return MakeColorRedGreenBlueRed(relative_shade);
}
