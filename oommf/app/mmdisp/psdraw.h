/* FILE: psdraw.h                 -*-Mode: c++-*-
 *
 * OOMMF C++ classes for PostScript rendering
 *
 */

#ifndef PSDRAW_H
#define PSDRAW_H

#include "colormap.h"
#include "planepoint.h"

/* End includes */     /* Optional directive to build.tcl */
//////////////////////////////////////////////////////////////////////////
// OOMMF PostScript drawing class
class OommfPSDraw {
public:
  OommfPSDraw(Tcl_Channel channel,
	      OC_REAL8m pxoff,OC_REAL8m pyoff,
	      OC_REAL8m printwidth,OC_REAL8m printheight,
	      const char* page_orientation,
	      OC_REAL8m xmin,OC_REAL8m ymin,
	      OC_REAL8m xmax,OC_REAL8m ymax,
	      OommfPackedRGB background_color,
              OC_REAL8m arrow_outline_width,
              OommfPackedRGB arrow_outline_color);
  /// Write header.  Offsets and print sizes are in points, x/y
  /// range values in pixels.  px/yoff are to lower lefthand
  /// corner of image.  Pixel coordinates run from upper lefthand
  /// corner of image.
  ~OommfPSDraw();                   // Write trailer

  // Draws (potentially fat) line segments between each successive pair
  // of points in vlist.  If joinstyle == 0, then successive segments
  // are connected with a mitered join, == 1 gives a rounded joint, and
  // == 2 gives a beveled join.  In general, this is an unclosed figure,
  // unless the last and first point are equal, in which case those ends
  // will be joined using joinstyle.
  void DrawPolyLine(const Nb_List<PlanePoint>& vlist,
		    OC_REAL8m linescale,OommfPackedRGB color,
                    int joinstyle);

  void DrawFilledRectangle(OC_REAL8m x1,OC_REAL8m y1,
			   OC_REAL8m x2,OC_REAL8m y2,
			   OommfPackedRGB color,
			   const char* transparency_stipple);
  /// Opposing vertices (x1,y1) and (x2,y2) are included.
  /// Specify transparency_stipple to NULL or "" to get no stipple.

  void DrawFilledArrow(OC_REAL8m xc,OC_REAL8m yc,OC_REAL8m size,
		       OC_REAL8m xcos,OC_REAL8m ycos,OC_REAL8m zcos,
		       OommfPackedRGB color);
  /// Draws arrow centered at point (xc,yc) in bitmap, of specified size,
  /// with directional cosines xcos,ycos,zcos, in the specified color.

  void DrawDiamondWithCross(OC_REAL8m xc,OC_REAL8m yc,OC_REAL8m size,
			    OommfPackedRGB outercolor,
			    OommfPackedRGB innercolor);
  /// Symbol representing out-of-plane vectors

  void DrawSquareWithDot(OC_REAL8m xc,OC_REAL8m yc,OC_REAL8m size,
			 OommfPackedRGB outercolor,
			 OommfPackedRGB innercolor);
  /// Symbol representing in-to-plane vectors.

  void AddMat(OC_REAL8m width,OommfPackedRGB color);
  /// Lays flat mat around border of specified width (in pixels)
  /// and color.

private:
  static const ClassDoc class_doc;
  Tcl_Channel channel;
  const OC_REAL8m LineWidth;
  const OC_REAL8m ArrowLength;
  const OC_REAL8m ArrowHeadRatio;
  const OC_REAL8m ArrowOutlineWidth;
  const OommfPackedRGB ArrowOutlineColor;
  const OC_REAL8m InOutTipRadius;
  const OC_REAL8m plot_xmin,plot_ymin,plot_xmax,plot_ymax;

  // Disable copy constructor and assignment operator by declaring
  // them without defining them.
  OommfPSDraw(const OommfPSDraw&);
  OommfPSDraw& operator=(const OommfPSDraw&);
};


#endif // PSDRAW_H
