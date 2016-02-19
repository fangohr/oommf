/* FILE: bitmap.h                 -*-Mode: c++-*-
 *
 * OOMMF C++ classes for bitmap rendering
 *
 */

#ifndef OBITMAP_H
#define OBITMAP_H

#include "oc.h"
#include "nb.h"
#include "vf.h"

#include "colormap.h"
#include "planepoint.h"

/* End includes */     /* Optional directive to build.tcl */

//////////////////////////////////////////////////////////////////////////
// OOMMF Polygon class
// This class defines a closed (not necessarily convex) polygon, lying
// in the x-y plane.  It is used by the bitmap drawing routines.
class OommfPolygon {
private:
  static const ClassDoc class_doc;
  Nb_List<PlanePoint> vertex;
public:
  OommfPolygon() {}
  OommfPolygon(const OommfPolygon& poly); // Implicit copy constructor
  ~OommfPolygon() {}
  OommfPolygon& operator=(OommfPolygon& poly);
  void Clear(); // Removes all vertices from polygon (does not free memory)
  OC_INDEX GetVertexCount();
  void AddVertex(const PlanePoint& pt); // Adds a new vertex to polygon
  void GetBounds(OC_REAL8m& xmin,OC_REAL8m& ymin,
                 OC_REAL8m& xmax,OC_REAL8m& ymax) const;
  int IsInside(const PlanePoint& pt) const;
  /// Returns 1 if pt is inside the polygon
  void Scale(OC_REAL8m xscale,OC_REAL8m yscale);    // Coordinate
  void Shift(OC_REAL8m xoffset,OC_REAL8m yoffset);  //    transformations
  void Rotate(OC_REAL8m angcos,OC_REAL8m angsin);   //  "  "
  void AdjustFirstToInteger(); // Rotates and rescales bitmap as
  /// necessary to shift first vertex point to the nearest integral
  /// coordinate
  void AdjustAllToIntegers(); // Rounds position of each vertex
  /// individually to nearest integer.

  void MakeVertexList(Nb_List<PlanePoint>& vlist) const;
  /// Copies vertex into vlist, repeating the first and last vertex to
  /// make a closed polyhedron.

  void DumpVertices(FILE* fout) const;  // Debugging tool
};

//////////////////////////////////////////////////////////////////////////
// OOMMF Bitmap class
class OommfBitmap {
private:
  static const ClassDoc class_doc;
  int xmin,ymin,xmax,ymax;  // Inclusive
  int xsize,ysize,totalsize; // xsize=xmax-xmin+1, etc.
  OommfPackedRGB* bitmap; // Rectangular display region
  void Alloc(int new_xmin,int new_ymin,
	     int new_xmax,int new_ymax);
  OommfPolygon default_arrow,default_diamond,default_cross;
  OommfPolygon work_poly; // Class scratch space.  Used by
  /// the following routines: DrawFilledArrow, DrawDiamondWithCross.
  /// Must not be used by any routine calling either of these.
  void Free();
  OommfPackedRGB GetPixel(int x,int y) const;
  void SetPixel(int x,int y,OommfPackedRGB color);
  /// Both GetPixel and SetPixel ASSUME x,y is in-bounds
public:
  OommfBitmap();
  void Setup(int new_xmin,int new_ymin,int new_xmax,int new_ymax);
  ~OommfBitmap() { Free(); }

  void GetExtents(int& myxmin,int& myymin,int& myxmax,int& myymax) const {
    myxmin=xmin; myymin=ymin; myxmax=xmax; myymax=ymax;
  }

  int WritePPMFile(const char* filename,int ppm_type);
  /// Use filename == NULL or "-" to write to stdout.
  /// "type" corresponds to the PPM magic number.  It should be either
  ///   3 (ASCII text) or 6 (binary).
  /// Return value is
  ///     0 => No error
  ///     1 => Bad parameter
  ///     2 => Unable to open filename for output
  ///     4 => Write error

  int WritePPMChannel(Tcl_Channel chan,int ppm_type);
  /// Analogous to WritePPMFile, but writes to a previously
  /// opened Tcl_Channel.  Returns 0 on success, >0 on error.

  int WriteBMPChannel(Tcl_Channel chan,int bmp_type);
  // Same as WritePPMChannel, but for Microsoft BMP files.
  // The 'bmp_type' is bits-per-pixel; currently only
  // bmp_type==24 is supported.

  // Drawing routines
  void Erase(OommfPackedRGB color); // Writes 'color' on each pixel

  void DrawFilledRectangle(OC_REAL8m x1,OC_REAL8m y1,OC_REAL8m x2,OC_REAL8m y2,
			  OommfPackedRGB color,
			  const char* transparency_stipple);
  /// Opposing vertices (x1,y1) and (x2,y2) are included.
  /// Specify transparency_stipple to NULL or "" to get no stipple.

  // Draws frame outside specified rectangle.
  void DrawRectangleFrame(OC_REAL8m x1,OC_REAL8m y1,
			  OC_REAL8m x2,OC_REAL8m y2,
			  OC_REAL8m borderwidth,
			  OommfPackedRGB color);

  // Filled, closed polygon.
  void DrawFilledPoly(const OommfPolygon& poly,
		      OommfPackedRGB color,OC_BOOL antialias);

  // Draws (potentially fat) line segments between each successive pair
  // of points in vlist.  If the joinstyle>0, then the segment ends are
  // connected with bevel joins.  In general, this is an unclosed
  // figure, unless the last and first point are equal, in which case
  // those ends will be joined if joinstyle>0.
  void DrawPolyLine(const Nb_List<PlanePoint>& vlist,
		    OC_REAL8m linewidth,OommfPackedRGB color,
		    int joinstyle,int antialias);

  /////// The next 3 drawing routines are specialized methods for
  /////// rendering vector fields, realized in terms of the
  /////// preceding general drawing routines, DrawFilledRectangle
  /////// and DrawFilledPoly.  It can be argued that these specialized
  /////// routines don't properly belong inside the OommfBitmap class.
  ///////
  void DrawFilledArrow(OC_REAL8m xc,OC_REAL8m yc,OC_REAL8m size,
                      OC_REAL8m xcos,OC_REAL8m ycos,OC_REAL8m zcos,
                      OommfPackedRGB color,OC_BOOL antialias,
                      OC_REAL8m outline_width,
                      OommfPackedRGB outline_color);
  /// Draws arrow centered at point (xc,yc) in bitmap, of specified size,
  /// with directional cosines xcos,ycos,zcos, in the specified color.
  /// If outline_width is not zero, then an outline is drawn of the
  /// specified relative width and color.

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

};

#endif // OBITMAP_H
