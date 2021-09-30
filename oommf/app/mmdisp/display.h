/* FILE: display.h                 -*-Mode: c++-*-
 *
 * C++ classes for display windows
 *
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include "nb.h"
#include "vf.h"

#include "bitmap.h"
#include "colormap.h"

/* End includes */     /* Optional directive to build.tcl */

//////////////////////////////////////////////////////////////////////////
// PlotConfiguration structure
//
class PlotConfiguration {
private:
  static const ClassDoc class_doc;
public:
  PlotConfiguration();
  PlotConfiguration(const PlotConfiguration &);
  PlotConfiguration& operator=(const PlotConfiguration &);
  
  OC_BOOL displaystate; // 1=>display, 0=>don't display

  OC_BOOL autosample;   // If 1, then replace specified subsample
  /// with an automatically determined data-dependent value.
  OC_REAL4m subsample;  // Subsample period, in units of mesh cells.
  ///   NOTE: If subsample=0, then *all* points are selected.
  /// Also if subsample<1, then features will be scaled as
  /// though subsample==1.

  OC_REAL4m size;   // Item magnification, relative to its
  ///  "standard size".  The drawn size depends upon size,
  /// subsample, and the "standard size".

  OC_BOOL antialias;  // 1=> Do antialiasing, 0=> don't.  May be ignored
  /// by some drawing routines.

  OC_REAL8m outlinewidth;  // 0 means no arrow outlines
  OommfPackedRGB outlinecolor;

  const char* stipple; // Stipple pattern.  Use "" for solid.
  // "gray25" is recommended for partial transparency.
  // NOTE: The patterns are intended to be string constants,
  // so this element points at that string; no memory is
  // allocated for the pattern name.  If a client points
  // stipple into free store, it is the responsibility of
  // the client to guarantee that the pointer stays valid
  // through the lifetime of the PlotConfiguration object.

  Nb_DString colorquantity;
  OC_REAL8m phase;
  OC_BOOL invert;

  DisplayColorMap colormap;
  const char* Color(OC_REAL4m shade) const {
    return colormap.GetColor(shade);
  }
  OommfPackedRGB PackedColor(OC_REAL4m shade) const {
    return colormap.GetPackedColor(shade);
  }

};

//////////////////////////////////////////////////////////////////////////
// Display frame class
//
//  Restrictions: Assumes square pixels.
//                2D, ignores z.
//
enum CoordinateSystem { // All are righthanded systems
  CS_ILLEGAL=-1,             // Illegal value
  CS_CALCULATION_STANDARD=0, // x to left,  y up,       z out -  mesh units
  CS_DISPLAY_STANDARD=1,     // x to left,  y down,     z in  - pixel units
  CS_DISPLAY_ROT_90=2,       // x down,     y to right, z in  - pixel units
  CS_DISPLAY_ROT_180=3,      // x to right, y up,       z in  - pixel units
  CS_DISPLAY_ROT_270=4       // x up,       y to left,  z in  - pixel units
};

OC_INT4m CoordinateAngleDifference(CoordinateSystem cs1,CoordinateSystem cs2,
				double &angle);
// Calculates the difference (in degrees, from 0 to 360) of cs1 - cs2, if
// both are CS_DISPLAY_* coordinates, sets the difference in 'angle' and
// returns 0;  If cs1 is not a CS_DISPLAY_* c.s., then the return value
// is 1; if cs2 is not a CS_DISPLAY_* c.s., then the return value is 2.
// In these latter two cases angle is 0.0 on return.

struct SampleInfo {
  OC_REAL4m subsample;
  Nb_DString colorquantity;
  OC_REAL8m phase;
  OC_BOOL invert;
  OC_BOOL trimtiny;
  Nb_List<Vf_DisplayVector> dvlist;
  SampleInfo() : subsample(0.), phase(0.), invert(0), trimtiny(0) {}
};

class DisplayFrame
{
private:
  static const ClassDoc class_doc;
  static const OC_REAL4m default_arrow_cellsize;
  static const OC_REAL4m default_pixel_cellsize; 
  /// Preferred cellsizes, in pixels.

  static const OC_REAL4m default_arrow_ratio;
  static const OC_REAL4m default_pixel_ratio;
  /// Default (base) size of objects, relative to the cell size.

  static const OC_REAL4m max_display_object_dimension;
  /// Don't produce arrows or pixels with x or y dimension
  /// larger than this number of pixels.  This may produce
  /// unexpected artifacts with arrows, as this normalization
  /// is with respect to the sup norm rather than the L2 norm,
  /// but the main purpose of this constraint is to protect
  /// against overflow, not to provide pretty pictures.

  Tcl_Interp* tcl_interp;
  Vf_Mesh *mesh;

  void ClearFrame();

  void GetBoundingBoxes(Nb_BoundingBox<OC_REAL4> &bdry,
			Nb_BoundingBox<OC_REAL4> &data,OC_REAL8m &data_pad) const;
  /// NB: Either of the returning boxes, in particular bdry, may
  ///  be an "empty" box, in which case max<min.

  OC_INT4m GetDisplayList(SampleInfo &si);
  OC_INT4m GetDisplayList(const Nb_BoundingBox<OC_REAL4>& select_box,
		       SampleInfo &si);

  OC_REAL4m mesh_per_pixel;  // * pixels => mesh units;   This is
  ///  guaranteed>0.  User zoom is given by
  ///    "mesh cell width"/mesh_per_pixel;

  OC_REAL4m preferred_arrow_cellsize,preferred_pixel_cellsize;

  CoordinateSystem coords; // Current coordinates
  void ChangeCoordinates(CoordinateSystem old_coords,
			 CoordinateSystem new_coords,
			 Nb_List<Vf_DisplayVector> &display_list) const;
  void ChangeCoordinates(CoordinateSystem old_coords,
			 CoordinateSystem new_coords,
			 Nb_List< Nb_Vec3<OC_REAL4> > &pt_list) const;

  PlotConfiguration arrow_config,pixel_config;

  SampleInfo arrow_sample,pixel_sample;
  /// Current mesh sample list and support information.

  OC_BOOL arrow_valid,pixel_valid,boundary_valid;
  /// If true, then the corresponding mesh sample lists are up-to-date.
  /// In general, display_lists are only updated when a Render request
  /// is made.  Exceptions are scaling and coordinate change requests,
  /// for which valid lists will be adjusted at once.

  void MarkAllSampleListsInvalid() {
    arrow_valid=pixel_valid=boundary_valid=0;
  }

  struct DrawPixelData {
  public:
    enum { SCRATCHSIZE = 256 };
    char slicetag[SCRATCHSIZE];
    OC_REAL4m xsize;
    OC_REAL4m ysize;
    Nb_TclCommand pixelcmd;
  };
  void SetDrawPixelBaseCmd(const char* canvas_name,
                           const char* stipple,
                           OC_BOOL hide,
                           OC_REAL4m xsize,
                           OC_REAL4m ysize,
                           DrawPixelData& draw_data) const;
  void DrawPixel(const Vf_DisplayVector& dspvec,
                 DrawPixelData& draw_data) const;


  struct DrawArrowData {
  public:
    enum { SCRATCHSIZE = 256 };
    char slicetag[SCRATCHSIZE];
    Nb_TclCommand arrowcmd;
    Nb_TclCommand inarrowcmdA;
    Nb_TclCommand inarrowcmdB;
    Nb_TclCommand outarrowcmdA;
    Nb_TclCommand outarrowcmdB;
#if !NB_TCL_COMMAND_USE_STRING_INTERFACE
    Nb_TclObjArray arrowshape;
    DrawArrowData() : arrowshape(3) {}
#endif
  };
  void SetDrawArrowBaseCmd(const char* canvas_name,
                           OC_BOOL hide,
                           DrawArrowData& draw_data) const;
  void DrawArrow(const Vf_DisplayVector& dspvec,
                 OC_REAL4m arrowsize,
                 DrawArrowData& draw_data) const;

  OC_BOOL MakeTkColor(const char* req_color,Nb_DString& color) const;
  /// Converts "color" to Tk 8-bit format, "#XXXXXX", and stores
  /// result in color.  Returns 1 on success, 0 on failure to
  /// interpret req_color

  OC_BOOL draw_boundary;        // 1=>draw boundary, 0=>don't draw boundary
  OC_BOOL boundary_on_top;    // 0=>boundary in back, 1=>boundary on top
  /// Currently this is only used by the DrawBitmap routine, because
  /// the standard Render routine tags all objects on the canvas,
  /// and it is up to the calling routine to adjust the relative
  /// position of objects via canvas raise+lower commands.
  OC_REAL8m boundary_width;
  Nb_DString boundary_color; // Color in Tk 8-bit format, "#XXXXXX".
  Nb_List< Nb_Vec3<OC_REAL4> > boundary_list; // List of boundary vertices
  OC_INT4m DrawLine(const char *canvas_name,
		 const Nb_List< Nb_Vec3<OC_REAL4> >& pts,
		 OC_REAL8m width,
		 const char* color,
		 const char* tags);

  Nb_DString background_color; // Color in Tk 8-bit format, "#XXXXXX".

  DisplayFrame(const DisplayFrame&);   // Disable copy constructor and
  DisplayFrame& operator=(const DisplayFrame&); // assignment operator

public:
  DisplayFrame();

  void SetMesh(Vf_Mesh *newmesh);

  void SetTclInterp(Tcl_Interp *newinterp) { tcl_interp=newinterp; }

  void GetPlotConfiguration(PlotConfiguration& _arrow_config,
			    PlotConfiguration& _pixel_config) const {
    _arrow_config=arrow_config;
    _pixel_config=pixel_config;
  }
  void SetPlotConfiguration(const PlotConfiguration& _arrow_config,
			    const PlotConfiguration& _pixel_config);

  // GetPixelDimensions() and GetArrowSize() return the size of
  // a full size pixel and arrow, respectively, in pixels.
  Nb_Vec3<OC_REAL4> GetPixelDimensions() const;
  OC_REAL8m GetArrowSize() const;

  void SetPreferredArrowCellsize(OC_REAL4m pixels);
  void SetPreferredPixelCellsize(OC_REAL4m pixels);
  OC_REAL4m GetPreferredArrowCellsize() const {
    return preferred_arrow_cellsize;
  }
  OC_REAL4m GetPreferredPixelCellsize() const {
    return preferred_pixel_cellsize;
  }
  OC_REAL4m GetAutoSampleRate(OC_REAL4m cellsize) const;
  /// Returns recommended subsample rate, for the current mesh
  /// at current zoom.

  CoordinateSystem GetCoordinates() const { return coords; }
  OC_BOOL SetCoordinates(CoordinateSystem new_coords);
  void CoordinatePointTransform(CoordinateSystem old_coords,
				CoordinateSystem new_coords,
				Nb_Vec3<OC_REAL4> &pt) const;

  const char* GetBackgroundColor() { return background_color; }
  OC_BOOL SetBackgroundColor(const char* color);
  /// GetBackgroundColor returns a Tk 8-bit color spec, of the form
  /// "#XXXXXX".  SetBackgroundColor accepts any valid Tk color string,
  /// and converts it to the "#XXXXXX" format for internal use.  Returns
  /// 1 on success, 0 if unable to interpret color

  OC_BOOL GetDrawBoundary() const { return draw_boundary; }
  void SetDrawBoundary(OC_BOOL newstate) { draw_boundary=newstate; }
  OC_BOOL IsBoundaryOnTop() const { return boundary_on_top; }
  void SetBoundaryOnTop(OC_BOOL top) { boundary_on_top = top; }
  OC_REAL8m GetBoundaryWidth() const { return boundary_width; }
  void SetBoundaryWidth(OC_REAL8m newwidth) { boundary_width = newwidth; }
  const char* GetBoundaryColor() { return boundary_color; }
  OC_BOOL SetBoundaryColor(const char* color);
  /// BoundaryColor is set and read are analogous to BackgroundColor

  OC_INDEX GetColorQuantityTypes(Nb_List<Nb_DString> &types) const {
    return mesh->ColorQuantityTypes(types);
  }

  const char* GetVecColor(const Nb_Vec3<OC_REAL4>& v) const;
  /// Access to the underlying Vf_Mesh::GetVecShade function.  See that
  /// routine for details on shade determination.  The mapping from
  /// shade to color depends on the state of arrow_config.displaystate
  /// and pixel_config.displaystate.  Pixel config colorquantity and
  /// colormap are used, unless pixels are turned off and arrows are
  /// turned on, in which case arrrow config colorquantity and colormap
  /// are used.

  OC_INT4m Render(const char *canvas_name,OC_BOOL hide=0);
  // If hide is true, then pixel and arrow objects are created
  // with the '-state hidden' switch.  Note that this is only
  // available on Tk 8.3 and later.  If using an older version,
  // the default hide=0 should be selected.

  OC_INT4m FillBitmap(OC_BOOL reverse_layers,OommfBitmap& bitmap);
  OC_INT4m FillBitmap(const Nb_BoundingBox<OC_REAL4>& arrow_box,
		   const Nb_BoundingBox<OC_REAL4>& pixel_box,
		   OC_BOOL reverse_layers,OommfBitmap& bitmap);
  /// Writes output into bitmap.  Imports arrow_box and pixel_box are in
  /// mesh coordinates.  Empty boxes are interpreted as no restraint,
  /// i.e., fill all points.  If reverse_layers is true, then
  /// the rendering order of the layers is reversed, with the "top"
  /// (i.e., largest z) first, and the "bottom" last.

  OC_INT4m PSDump(Tcl_Channel channel,
	       OC_REAL8m pxoff,OC_REAL8m pyoff,
	       OC_REAL8m printwidth,OC_REAL8m printheight,
	       const char* page_orientation,
	       OC_REAL8m xmin,OC_REAL8m ymin,
	       OC_REAL8m xmax,OC_REAL8m ymax,
	       const Nb_BoundingBox<OC_REAL4>& arrow_box,
	       const Nb_BoundingBox<OC_REAL4>& pixel_box,
	       OC_BOOL reverse_layers,
	       OC_REAL8m mat_width,Nb_DString mat_color,
	       OC_REAL8m arrow_outline_width,
	       Nb_DString arrow_outline_color);
  /// Writes display to channel, as a PostScript file.

  OC_REAL4m GetZoom(void) const;
  OC_REAL4m SetZoom(OC_REAL4m zoom);
  OC_REAL4m SetZoom(OC_REAL4m width,OC_REAL4m height,
		 OC_REAL4m margin,OC_REAL4m scrollbar_cross_dimension);
  /// NOTE: "zoom" is in units of "pixels per approx cell size".

  OC_REAL4m GetMeshPerPixel() const { return mesh_per_pixel; }
  /// * pixels => mesh units;   This is guaranteed>0.  User zoom
  /// is given by  "mesh cell width"/mesh_per_pixel;

  void GetRequestedSubsampleRates(OC_REAL4m &arrow_samplerate,
				  OC_REAL4m &pixel_samplerate) const {
    arrow_samplerate=arrow_config.subsample;
    pixel_samplerate=pixel_config.subsample;
  }
  void GetActualSubsampleRates(OC_REAL4m &arrow_samplerate,
			       OC_REAL4m &pixel_samplerate) const {
    arrow_samplerate=arrow_sample.subsample;
    pixel_samplerate=pixel_sample.subsample;
  }

  Nb_BoundingBox<OC_REAL4> GetDisplayBox() const;
  /// Returns extents of mesh, in pixels (at current zoom).

  const char* GetValueUnit() const { return mesh->GetValueUnit(); }

  OC_REAL8m GetValueScaling() const;
  OC_INT4m SetValueScaling(OC_REAL8m newscaling);

};

#endif // DISPLAY_H
