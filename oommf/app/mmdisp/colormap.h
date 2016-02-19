/* FILE: colormap.h                 -*-Mode: c++-*-
 *
 * OOMMF colormap C++ class
 *
 */

#ifndef COLORMAP_H
#define COLORMAP_H

#include "nb.h"

/* End includes */     /* Optional directive to build.tcl */
//////////////////////////////////////////////////////////////////////////
// Color specification
//
class OommfPackedRGB {
private:
  unsigned char red,green,blue,pad;
public:
  OommfPackedRGB() : red(0), green(0), blue(0), pad(0) {}
  OommfPackedRGB(const char* tkcolorstr) {
    Set(tkcolorstr);
  }
  OommfPackedRGB(int r,int g,int b)
    : red((unsigned char)r), green((unsigned char)g),
      blue((unsigned char)b), pad(0) {}

  void Get(unsigned int &myred,unsigned int &mygreen,
	   unsigned int &myblue) const {
    myred=red; mygreen=green; myblue=blue;
  }

  OC_BOOL Set(const char* color_req);
  /// Uses Nb_GetColor to convert color_req into an rgb triplet.  If
  /// color_req is in Tk numeric format, e.g. #FFFFFF, then Nb_GetColor
  /// does the conversion directly.  Otherwise, Nb_GetColor uses the
  /// global Tcl interpreter to access the nbcolordatabase array defined
  /// in oommf/config/colors.config.

  OC_BOOL Set(unsigned int new_red,unsigned int new_green,
	   unsigned int new_blue) {
    red   = (unsigned char)new_red;
    green = (unsigned char)new_green;
    blue  = (unsigned char)new_blue;
    return 1;
  }

  void Blend(OC_REAL8m wgt1,OommfPackedRGB color1,
	     OC_REAL8m wgt2,OommfPackedRGB color2);
  /// Generalization of Set, using wgt1*color1 + wgt2*color2.

  void Invert() {
    red   = (unsigned char)(255-red);
    green = (unsigned char)(255-green);
    blue  = (unsigned char)(255-blue);
  }

};

//////////////////////////////////////////////////////////////////////////
// Colormap class
//
#define TKCOLORLENGTH 6
class DisplayColorMap {
private:
  static const ClassDoc class_doc;
  int colorcount;  // Number of colors in colormap
  Nb_DString mapname;
  char **TkColor;       // For rapid access, store as both Tk color
  OommfPackedRGB *rgb;  // strings, and packed bytes.
  void Alloc(int new_colorcount,const char* new_mapname);
  void Free();
public:
  void Setup(int _colorcount,const char* _mapname,
	     const char* MakeColor(OC_REAL8m relative_shade));
  DisplayColorMap(void);
  DisplayColorMap(int _colorcount,const char* _mapname,
		  const char* MakeColor(OC_REAL8m relative_shade))
    : colorcount(0),TkColor(0),rgb(0) {
    Setup(_colorcount,_mapname,MakeColor);
  }
  DisplayColorMap(const DisplayColorMap&);
  DisplayColorMap& operator=(const DisplayColorMap&);
  ~DisplayColorMap() { Free(); }

  // Setup routines using predefined maps
  void Setup(int _colorcount,const char* _mapname);
  DisplayColorMap(int _colorcount,const char* _mapname) {
    Setup(_colorcount,_mapname);
  }

  // Routine to get list of predefined maps
  static void DefaultColorMapList(Nb_List<const char*>& maps);

  const char* GetMapName() { return (const char*)mapname; }
  int GetColorCount() { return colorcount; }

  const char* GetColor(OC_REAL4m relative_shade) const; // Given a floating
  /// point number between 0. and 1., returns a char* to the closest
  /// corresponding value in the colormap.  The returned pointer will be
  /// valid until the colormap is changed.

  // OommfPackedRGB access version of the last.  This one returns
  // a value instead of a pointer, however.
  OommfPackedRGB GetPackedColor(OC_REAL4m relative_shade) const;
};

// The predefined MakeColor colormaps
const char* MakeColorRedBlackBlue(OC_REAL8m relative_shade);
const char* MakeColorBlueWhiteRed(OC_REAL8m relative_shade);
const char* MakeColorGreenWhiteOrange(OC_REAL8m relative_shade);
const char* MakeColorTealWhiteRed(OC_REAL8m relative_shade);
const char* MakeColorBlackGrayWhite(OC_REAL8m relative_shade);
const char* MakeColorHighContrastGray(OC_REAL8m relative_shade);
const char* MakeColorBlackWhiteBlack(OC_REAL8m relative_shade);
const char* MakeColorWhiteBlackWhite(OC_REAL8m relative_shade);
const char* MakeColorBlackBlackWhite(OC_REAL8m relative_shade);
const char* MakeColorBlackWhiteWhite(OC_REAL8m relative_shade);
const char* MakeColorWhiteGreenBlack(OC_REAL8m relative_shade);
const char* MakeColorRedGreenBlueRed(OC_REAL8m relative_shade);
const char* MakeColorCyanMagentaYellowCyan(OC_REAL8m relative_shade);

#endif /* COLORMAP_H */
