/* FILE: mesh.h                 -*-Mode: c++-*-
 *
 * Abstract mesh class and children
 *
 * Last modified on: $Date: 2016/02/24 20:34:19 $
 * Last modified by: $Author: donahue $
 */

#ifndef _VF_MESH
#define _VF_MESH

#include "oc.h"
#include "nb.h"

#include "ptsearch.h"

/* End includes */     /* Optional directive to build.tcl */

//////////////////////////////////////////////////////////////////////////
// Extension of LocatedVector class used for display purposes.
// Perhaps both this and LocatedVector should we written up via
// templates?
struct Vf_DisplayVector
{
public:
  Nb_Vec3<OC_REAL4> location;
  Nb_Vec3<OC_REAL4> value;
  OC_REAL4m shade;
  OC_INDEX zslice;
  Vf_DisplayVector() :
    location(0.,0.,0.),value(0.,0.,0.),
    shade(0.),zslice(0) {}
  Vf_DisplayVector(const Nb_Vec3<OC_REAL4>& location_,
                   const Nb_Vec3<OC_REAL4>& value_,
                   OC_REAL4m shade_,OC_INDEX zslice_)
    : location(location_), value(value_),
      shade(shade_), zslice(zslice_) {}
};


//////////////////////////////////////////////////////////////////////////
// Vf_Mesh (abstract) class

class Vf_Mesh_Index
{ // Index class used by Vf_Mesh::GetFirst() and ::GetNext()
  // Children of Vf_Mesh should define a compatible index class
  // as a child of this class.
public:
  virtual ~Vf_Mesh_Index() {}
};

class Vf_Mesh
{
private:
  Oc_AutoBuf Filename;
  Oc_AutoBuf Title;
  Oc_AutoBuf Description;
  Oc_AutoBuf MeshUnit;  // Measurement unit for grid; e.g., "m"
  Oc_AutoBuf ValueUnit;
  OC_REAL8m DisplayValueScale; // Value scaling relative to ValueMultiplier

  // The following functions are left undefined on purpose
  Vf_Mesh();                   // Default constructor
  Vf_Mesh& operator=(const Vf_Mesh &rhs); // Assignment op

protected:
  void SetFilename(const char* _Filename);
  void SetDescription(const char* _Description);
  void SetMeshUnit(const char* _MeshUnit);
  void SetValueUnit(const char* _ValueUnit);
  OC_REAL8m MinMagHint,MaxMagHint;  // Display hint values
  /// for min & max magnitude of all vectors in mesh.
  /// Any vector smaller than MinMagHint is set to 0
  /// length, and any larger than MaxMagHint is
  /// rescaled to have magnitude equal to MaxMagHint.
  /// Action can be effectively disabled by setting
  /// to GetMagValueSpan() results.  These values
  /// are guaranteed non-negative (but may be zero!).

  OC_REAL8m ValueMultiplier;  // Default value scaling

  // For use by generic Vf_Mesh code for manipulation of
  // mesh node values (as opposed to node locations).
  // Returns 0 on success, 1 if the index is invalid
  // or out-of-bounds.
  virtual OC_BOOL SetNodeValue(const Vf_Mesh_Index* key,
                               const Nb_Vec3<OC_REAL8>& value) = 0;

  void FillDataBoundaryList
  (Nb_List< Nb_Vec3<OC_REAL8> > &boundary_list) const;
  /// Uses GetPreciseDataRange to produce a rectangular boundary
  /// about the data.

public:
  const char* GetName() const;
  const char* GetTitle() const;
  const char* GetDescription() const;
  void SetTitle(const char* _Title);
  // NOTE: "Filename", "Title" & "Description" are owned and managed
  // by class Vf_Mesh, and the returned address will become invalid if
  // SetFilename/Title/Descripion is called or if the mesh is
  // destroyed.  Calling routines will want to copy the returned
  // string into local memory if it is needed for an extended time.

  const char* GetMeshUnit() const;
  const char* GetValueUnit() const;
  OC_REAL8m GetValueMultiplier() const { return ValueMultiplier; }
  OC_REAL8m GetDisplayValueScale() const { return DisplayValueScale; }
  OC_BOOL SetDisplayValueScale(OC_REAL8m newscale) {
    if(newscale<=0.0) return 1;  // Bad value
    DisplayValueScale=newscale;
    return 0;
  }

  // EXPLANATION OF VARIOUS SCALING TERMS
  //  Note: This is an after-the-fact (11-Sept-2002) reconstruction
  // of the original intent, but at least it can hopefully provide
  // a consistent ruleset for future work.
  //
  //  ValueUnit, ValueMultiplier: When the values as stored in
  //        the mesh are multiplied by ValueMultiplier, then
  //        the resulting values are in ValueUnit units.  The
  //        ValueMultiplier setting should not be changed except
  //        in association with a corresponding change to the
  //        magnitude of all vectors stored in the mesh.
  //        Incidentally, the file input code in vecfile.cc
  //        typically divides all data vectors on input by
  //        the ValueRangeMaxMag value specified in the file,
  //        and adjusts ValueMultiplier accordingly.
  // DisplayValueScale: This is used *only* by output DisplayList
  //        functions, to scale display output.  It is a relative,
  //        non-dimensional quantity.
  //
  // MinMagHint, MaxMagHint: On display output, any magnitude
  //        larger than MaxMagHint is truncated to MaxMagHint,
  //        and any value smaller than MinMagHint is set to 0.
  //
  // Thus, given a vector v in the mesh, the "real" represented
  // vector is ValueMultiplier*v, which is in units ValueUnits
  // (e.g., A/m).  For display purposes, this may be additionally
  // scaled by the relative DisplayValueScale, i.e., the conceptual
  // display vector is DisplayValueScale*ValueMultiplier*v.  This
  // may be further adjusted by MinMagHint and MaxMagHint, as
  // described above.  The MagHint's are also in ValueUnits.
  //    There is then still an additional scaling when the vectors
  // are rendered onto the screen.  This is the responsiblity of
  // the rendering code (i.e., Not My Problem).

  OC_BOOL SetMagHints(OC_REAL8m minmag,OC_REAL8m maxmag) {
    if(maxmag<minmag || minmag<0.) return 0; // Error: Bad input values
    MinMagHint=minmag; MaxMagHint=maxmag;
    return 1;
  }
  void GetMagHints(OC_REAL8m& minmag,OC_REAL8m& maxmag) const {
    minmag=MinMagHint; maxmag=MaxMagHint;
  }

  void GetValueMagSpan(Nb_LocatedVector<OC_REAL8>& min_export,
                       Nb_LocatedVector<OC_REAL8>& max_export) const;
  void GetValueMagSpan(OC_REAL8m &minmag,OC_REAL8m &maxmag) const;
  /// Returns smallest and largest magnitude across all value vectors in
  /// mesh. This does *not* include DisplayValueScale scaling, but does
  /// include ValueMultiplier.  The Nb_LocatedVector version returns the
  /// (or, in case of duplicates, an example of) vector value and
  /// location having the extremal values.

  void GetNonZeroValueMagSpan(Nb_LocatedVector<OC_REAL8>& min_export,
                              Nb_LocatedVector<OC_REAL8>& max_export) const;
  void GetNonZeroValueMagSpan(OC_REAL8m &minmag,OC_REAL8m &maxmag) const;
  /// Determine smallest and largest magnitude across all vectors in
  /// mesh, *excluding* zero vectors. This does *not* include
  /// DisplayValueScale scaling, but does include ValueMultiplier.  The
  /// Nb_LocatedVector version returns the (or, in case of duplicates,
  /// an example of) vector value and location having the extremal
  /// values.

  void GetValueMean(Nb_Vec3<OC_REAL8>& meanvalue) const;
  /// Compute vector mean value.  Each node is weighted equally.  The
  /// value does *not* include DisplayValueScale scaling, but does
  /// include ValueMultiplier.

  OC_REAL8m GetValueRMS() const;
  /// Compute the Root mean square (RMS) of the norm of the node
  /// vectors.  Each node is weighted equally.  The value does *not*
  /// include DisplayValueScale scaling, but does include
  /// ValueMultiplier.

  OC_REAL8m GetValueL1() const;
  /// Computes \sum_i (|x_i| + |y_i| + |z_i|)/N, where i=1..N.
  /// Each node is weighted equally.  The value does *not* include
  /// DisplayValueScale scaling, but does include ValueMultiplier.

  virtual const char* GetMeshType() const =0;

  void GetRange(Nb_BoundingBox<OC_REAL4> &range) const;
  virtual void GetPreciseRange(Nb_BoundingBox<OC_REAL8> &range) const =0;
  /// Data + boundary range.  Tightness not required, but should
  /// be close.

  void GetDataRange(Nb_BoundingBox<OC_REAL4> &range) const;
  virtual void GetPreciseDataRange(Nb_BoundingBox<OC_REAL8> &range) const =0;
  /// Data range only, no boundary.  Tightness not required, but
  /// should be close.


#if !OC_REAL8m_IS_OC_REAL8
  void GetPreciseRange(Nb_BoundingBox<OC_REAL8m> &range) const {
    Nb_BoundingBox<OC_REAL8> range8;
    GetPreciseRange(range8);
    range = range8;
  }
  void GetPreciseDataRange(Nb_BoundingBox<OC_REAL8m> &range) const {
    Nb_BoundingBox<OC_REAL8> range8;
    GetPreciseDataRange(range8);
    range = range8;
  }
#elif !OC_REAL8m_IS_DOUBLE
  void GetPreciseDataRange(Nb_BoundingBox<double> &range) const {
    Nb_BoundingBox<OC_REAL8> range8;
    GetPreciseDataRange(range8);
    range = range8;
  }
  void GetPreciseRange(Nb_BoundingBox<double> &range) const {
    Nb_BoundingBox<OC_REAL8> range8;
    GetPreciseRange(range8);
    range = range8;
  }
#endif


  void GetBoundaryList(Nb_List< Nb_Vec3<OC_REAL4> > &boundary_list) const;
  virtual void
  GetPreciseBoundaryList(Nb_List< Nb_Vec3<OC_REAL8> > &boundary_list) const =0;
  /// Used to draw outline polygon in display.  Should probably enclose
  /// all data points.

  virtual OC_BOOL IsBoundaryFromData() const =0; // Should return 0
  /// if boundary was explicitly specified, 1 if it was generated
  /// by the mesh from data.

  virtual Nb_Vec3<OC_REAL4>  GetApproximateCellDimensions() const =0;
  /// This should be an "average" distance between nearest
  /// neighbor points.  Used to size arrows in display.

  virtual OC_REAL4m GetSubsampleGrit(void) const =0;
  /// Used to set increment on subsample scale widget in display
  /// control window.  Units are RELATIVE to cellsize.  For regular
  /// grids it doesn't make sense to sample at a rate smaller than the
  /// grid lattice spacing, so this value should be 1 but for
  /// irregular meshes, a smaller value may make perfect sense.
  /// GetSubsampleGrit() should return a value >0 and <=1.

  virtual OC_INDEX ColorQuantityTypes(Nb_List<Nb_DString> &types) const {
    types.Clear();
    return 0;
  }
  /// List of strings detailing color quantities supported.  Return
  /// value is the number of elements in the list.  Non-trivial meshes
  /// should support at least "x", "y", "z", "mag", and "none".

  // The next function is an aid for external routines working with
  // transformations and Vf_Mesh::GetDisplayList.  Return value is 1
  // if a change occurred, 0 othersize.
  virtual OC_BOOL ColorQuantityTransform
  (const Nb_DString /* flipstr */,
   const Nb_DString& quantity_in,OC_REAL8m phase_in,OC_BOOL invert_in,
   Nb_DString& quantity_out,OC_REAL8m& phase_out,OC_BOOL& invert_out) const {
    quantity_out=quantity_in; phase_out=phase_in; invert_out=invert_in;
    return 0;
  }

  virtual OC_REAL4m GetVecShade(const char* /* colorquantity */,
			     OC_REAL8m /* phase */,
			     OC_BOOL /* invert */,
                             const Nb_Vec3<OC_REAL4>& /* v */) const {
    return OC_REAL4m(-1.);
  }
  /// Given a vector and a plotting quantity, returns the "shade" that vector
  /// would be colored.  The return value is between 0. and 1. if the plotting
  /// quantity is computable for a single vector, or -1. if colorquantity is
  /// unrecognized or not computable.  This routine is intended to be used for
  /// coloring coordinate axes, and makes sense if colorquantity is "x", "y"
  /// or "z".  For a quantity like "div" there is no logical coloring of a
  /// single vector, and so -1. will be returned.  "none" should also return
  /// -1.
  /// NOTE: The input vector v is assumed to be pre-scaled, i.e., no
  ///  DisplayValueScale or ValueMultiplier is applied inside this routine.
  ///  MagHints are used, however.
  ///  NB: Unlike GetDisplayList which is required to return a value in [0.,1.]
  /// for each element of the display_list, this routine has the option of
  /// returning -1.  The caller can convert -1. to 0.5 if consistency with
  /// GetDisplayList is required.


  virtual OC_INDEX GetDisplayList(OC_REAL4m &xstep_request,
			       OC_REAL4m &ystep_request,
			       OC_REAL4m &zstep_request,
			       const Nb_BoundingBox<OC_REAL4> &range,
			       const char * colorquantity,
                               OC_REAL8m phase,
                               OC_BOOL invert, OC_BOOL trimtiny,
			       Nb_List<Vf_DisplayVector> &display_list) =0;
  ///   Use ?step_request=0 to remove constraint on that axis, e.g., if
  /// xstep_request=ystep_request=zstep_request=0, then this routine places
  /// all points inside "range" into display_list.
  ///   Return value is number of points in display_list.  This is conceptually
  /// a const function, but isn't if the underlying representation stores the
  /// mesh values internally in a Nb_List<> structure, and uses the Nb_List<>
  /// iterator to access the elements (the iterator has an internal pointer
  /// that gets incremented with each access).  One might want to add a
  /// Nb_List<> "header" copy routine to the Nb_List<> template definition, to
  /// allow for efficient local iterators.  Then this function could be made
  /// const.
  ///   Note: All returned elements of display_list have a shade value in
  /// [0.,1.].  If colorquantity is unknown or can't be computed for some
  /// reason, then shade will be set to 0.5.
  ///   Note 2: The value size and shade values are dependent on the
  /// settings of MinMagHints and MaxMagHints.  If you want full scale
  /// output without truncation, set the hint values to the output of
  /// GetMagValueSpan().

  virtual void GetZslice(OC_REAL8m zlow,OC_REAL8m zhigh,
			 OC_INDEX& islicelow,OC_INDEX& islicehigh) =0;
  // Returns the zslice value that would be associated with the
  // import depth 'z' ranging between zlow and zhigh, in a
  // Vf_DisplayVector structure. The exports should be interpreted
  // as including islicelow, and everything above up to but not
  // including islicehigh.

  virtual OC_INDEX GetZsliceCount() =0;
  // Zslice values range from 0 up through GetZsliceCount()-1

  void GetZslice(OC_REAL8m zlow,OC_REAL8m zhigh,
                 OC_REAL4m& zslicelow,OC_REAL4m& zslicehigh);
  // Calls (virtual) islice-version of GetZslice, but converts
  // from integer back to mesh coordinates.  Useful for emulating
  // slice-based display in non-slice-based code.  Note that
  // this function in non-virtual; it is implemented in the base
  // class using other, virtual function calls.

  virtual void GetZsliceParameters(OC_REAL8m& zslicebase,
                                   OC_REAL8m& zstep) =0;

  virtual OC_BOOL FindPreciseClosest(const Nb_Vec3<OC_REAL8> &pos,
                                     Nb_LocatedVector<OC_REAL8> &lv) =0;
  // Finds closest vector in mesh to location loc;  The return value
  // is non-zero on error, which probably means the point is outside
  // the range of the mesh.  NOTE: This routine is likely not optimized
  // for repeated access.  For tight, time-critical loops, try to
  // access the underlying mesh using a mesh-specific lower level method.
  // NOTE 2: The return lv.value is the value as stored in the mesh.
  // To get the "true" value in ValueUnit's, multipy by ValueMultiplier.

  OC_BOOL FindClosest(const Nb_Vec3<OC_REAL4> &pos,
                      Nb_LocatedVector<OC_REAL4> &lv);
  // Low precision version of the previous.

  // Base constructor
  Vf_Mesh(const char *Filename_,
	  const char *Title_=NULL,const char* Description_=NULL,
	  const char *MeshUnit_=NULL,const char *ValueUnit_=NULL,
	  OC_REAL8m ValueMultiplier_=1.)
    : Filename(Filename_),
      Title(Title_),Description(Description_),
      MeshUnit(MeshUnit_),ValueUnit(ValueUnit_),
      DisplayValueScale(1.0),
      MinMagHint(0.),MaxMagHint(0.),
      ValueMultiplier(ValueMultiplier_) {}

  // Effective copy constructor
  Vf_Mesh(const Vf_Mesh& other)
    : DisplayValueScale(1.0),
      MinMagHint(other.MinMagHint),MaxMagHint(other.MaxMagHint),
      ValueMultiplier(other.GetValueMultiplier()) {
    SetFilename(other.GetName());
    SetTitle(other.GetTitle());
    SetDescription(other.GetDescription());
    SetMeshUnit(other.GetMeshUnit());
    SetValueUnit(other.GetValueUnit());
    SetDisplayValueScale(other.GetDisplayValueScale());
  }

  virtual OC_INDEX GetSize() const =0;

  virtual ~Vf_Mesh() {}

  // Use these to iterate through entire mesh.  The GetFirstPt() routine
  // constructs an appropriate Vf_Mesh_Index pointer, and returns the
  // address in 'key'.  Subsequent calls use this key to get the next
  // element from the mesh.  The return value of GetFirstPt and GetNextPt
  // is 0 if there are no more elements, 1 othersize.  The calling routine
  // is responsible for calling the destructor on 'key' when finished.
  // NOTE: Unlike some other generic Vf_Mesh functions, these return
  //       OC_REAL8 vectors.
  virtual OC_BOOL
  GetFirstPt(Vf_Mesh_Index* &index,Nb_LocatedVector<OC_REAL8> &) const {
    index=NULL;
    return 0;
  }
  virtual OC_BOOL
  GetNextPt(Vf_Mesh_Index* &index,Nb_LocatedVector<OC_REAL8> &) const {
    index=NULL;
    return 0;
  }

  // Subtracts the values in specified mesh from *this.  Returns 0 on
  // success. Returns 1 if meshes aren't compatible.
  virtual OC_BOOL SubtractMesh(const Vf_Mesh& other);

  // Computes *this x other -> *this, where 'x' is the pointwise vector
  // (cross) product.  Returns 0 on success. Returns 1 if meshes aren't
  // compatible.
  virtual OC_BOOL CrossProductMesh(const Vf_Mesh& other);

};

//////////////////////////////////////////////////////////////////////////
// Empty mesh
class Vf_EmptyMesh:public Vf_Mesh
{
private:
  Nb_BoundingBox<OC_REAL8> empty_range;
  Nb_List< Nb_Vec3<OC_REAL8> > empty_boundary_list;

  // The following functions are left undefined on purpose
  Vf_EmptyMesh& operator=(const Vf_EmptyMesh &rhs); // Assignment op
  Vf_EmptyMesh(const Vf_EmptyMesh&);                // Copy constructor

protected:
  // For use by generic Vf_Mesh code for manipulation of
  // mesh node values (as opposed to node locations).
  // Returns 0 on success, 1 if the index is invalid
  // or out-of-bounds.
  virtual OC_BOOL SetNodeValue(const Vf_Mesh_Index*,
			   const Nb_Vec3<OC_REAL8>&) {
    return 1; // All indexes are out-of-bounds
  }

public:
  virtual const char* GetMeshType() const { return "Vf_EmptyMesh"; }

  Vf_EmptyMesh():Vf_Mesh("<empty>") {}

  void GetPreciseRange(Nb_BoundingBox<OC_REAL8> &range) const {
    range=empty_range;
  }
  void GetPreciseDataRange(Nb_BoundingBox<OC_REAL8> &range) const {
    GetPreciseRange(range);
  }
  void
  GetPreciseBoundaryList(Nb_List< Nb_Vec3<OC_REAL8> > &boundary_list) const {
    boundary_list=empty_boundary_list;
  }

  OC_BOOL IsBoundaryFromData() const { return 0; }

  Nb_Vec3<OC_REAL4>  GetApproximateCellDimensions() const {
    return Nb_Vec3<OC_REAL4>(OC_REAL4(1.),OC_REAL4(1.),OC_REAL4(1.));
  }
  OC_REAL4m GetSubsampleGrit(void) const { return OC_REAL4m(1.); }
  OC_INDEX GetDisplayList(OC_REAL4m &,OC_REAL4m &,OC_REAL4m &,
		       const Nb_BoundingBox<OC_REAL4>&,
		       const char *,
                       OC_REAL8m,OC_BOOL,OC_BOOL,
		       Nb_List<Vf_DisplayVector> &display_list) {
    display_list.Clear();
    return 0;
  }

  virtual void GetZslice(OC_REAL8m /* zlow */,OC_REAL8m /* zhigh */,
			 OC_INDEX& islicelow,OC_INDEX& islicehigh)
  { islicelow=0; islicehigh=1; }

  virtual OC_INDEX GetZsliceCount() { return 1; }

  virtual void GetZsliceParameters(OC_REAL8m& zslicebase,OC_REAL8m& zstep) {
    zslicebase=0.0; zstep=1.0;
  }

  virtual OC_BOOL FindPreciseClosest(const Nb_Vec3<OC_REAL8> &,
                                     Nb_LocatedVector<OC_REAL8> &) {
    return 1;  // Not supported.
  }
  virtual OC_INDEX GetSize() const { return 0; }
};

//////////////////////////////////////////////////////////////////////////
// Regular 3D grid of Nb_Vec3<OC_REAL8>'s
class Vf_GridVec3f_Index:public Vf_Mesh_Index
{
public:
  OC_INDEX i,j,k;
  Vf_GridVec3f_Index() : i(0),j(0),k(0) {}
};

class Vf_GridVec3f:public Vf_Mesh
{ // NOTE: dim1,dim2,dim3 of grid should be thought of in the order
  // z,y,x (or k,j,i), because in applications typically z-dim (k)
  // will be smallest, x-dim (i) will be largest, so the data is
  // stored arr[k][j][i].  This is why we overload the () operator
  // to map (i,j,k) -> [k][j][i].
  //   IN PARTICULAR, efficient loops will increment the innermost loop
  // on i. SO, KEEP THIS STRAIGHT: (i,j,k) coming in to member functions
  // should access grid[k][j][i], or (better) use GridVec(i,j,k).

private:
  static const ClassDoc class_doc;
  Nb_Array3D< Nb_Vec3<OC_REAL8> > grid;   // Magnetization vector array
  inline OC_INDEX SetSize(OC_INDEX newi,OC_INDEX newj,OC_INDEX newk) {
    return grid.Allocate(newk,newj,newi);
  }
  Nb_BoundingBox<OC_REAL8> range;
  Nb_BoundingBox<OC_REAL8> data_range;
  Nb_List< Nb_Vec3<OC_REAL8> > boundary;
  OC_BOOL boundary_from_data; // If false, the "boundary" came as input,
  /// either from the input file specifying the vector field, or else
  /// from another grid via the copy constructor.  If true, then
  /// the boundary was generated by this current instance based on
  /// the range data.
  Nb_Vec3<OC_REAL8> coords_base,coords_step;
  Nb_Vec3<OC_REAL8>
  NodalToProblemCoords(Nb_Vec3<OC_REAL8> const &w_nodal) const;
  Nb_Vec3<OC_REAL8>
  ProblemToNodalCoords(Nb_Vec3<OC_REAL8> const &v_problem) const;
  /// The conversion from nodes to problem coordinates is
  ///    (i,j,k) -> base+(i*step.x,j*step.y,k*step.z).
  /// NOTE 1: The grid vector access functions, e.g., operator(), access
  ///   via the integral "nodal" coordinates, *not* in terms of the
  ///   (real valued) problem coordinates.
  /// NOTE 2: The values in coords_step are guaranteed to be !=0,
  ///   but may be negative!  However, GetStdStep() (below), returns
  ///   a value >0.
  /// NOTE 3: The coords conversion routines don't preserve order
  ///   if step.? is negative, e.g., say (min.x,min.y) < (max.x,max.y)
  ///   going in, but step.x is negative.  Then after conversion,
  ///   min.y~ will still be < max.y~, but now min.x~>max.x~.

  OC_REAL8m GetStdStep() const;

  // The following function is left undefined on purpose
  Vf_GridVec3f& operator=(const Vf_GridVec3f& rhs); // Assignment op

protected:
  // For use by generic Vf_Mesh code for manipulation of
  // mesh node values (as opposed to node locations).
  // Returns 0 on success, 1 if the index is invalid
  // or out-of-bounds.
  virtual OC_BOOL SetNodeValue(const Vf_Mesh_Index* key,
			   const Nb_Vec3<OC_REAL8>& value);

public:

  OC_INDEX GetDimens(OC_INDEX &isize,OC_INDEX &jsize,OC_INDEX &ksize) const {
    return grid.GetSize(ksize,jsize,isize);
  }
  virtual OC_INDEX GetSize() const { return grid.GetSize(); }

  // Default (empty) constructor
  Vf_GridVec3f():Vf_Mesh("Vf_GridVec3f",NULL,NULL) {}

  // The following constructor should be called from file input functions.
  Vf_GridVec3f(const char *filename,const char *title,const char *desc,
	       const char *meshunit,
	       const char *valueunit,OC_REAL8 valuemultiplier,
               OC_INDEX newi,OC_INDEX newj,OC_INDEX newk,
               const Nb_Vec3<OC_REAL8> &basept,
               const Nb_Vec3<OC_REAL8> &gridstep,
               const Nb_BoundingBox<OC_REAL8> &newdatarange,
               const Nb_BoundingBox<OC_REAL8> &newrange,
               const Nb_List< Nb_Vec3<OC_REAL8> > *newboundary);
  /// Note: If newboundary==NULL, then default rectangular boundary is
  /// constructed based on range data.

  Nb_Vec3<OC_REAL8> GetBasePoint() const { return coords_base; }
  Nb_Vec3<OC_REAL8> GetGridStep() const { return coords_step; }

  virtual const char* GetMeshType() const { return class_doc.classname; }

  Vf_GridVec3f(const Vf_GridVec3f &import_mesh):Vf_Mesh(import_mesh) {
    // Copy constructor
    grid.Copy(import_mesh.grid);
    data_range=import_mesh.data_range;
    range=import_mesh.range;
    boundary=import_mesh.boundary;
    coords_base=import_mesh.coords_base;
    coords_step=import_mesh.coords_step;
  }

  // Copy constructor with transform.
  // NOTE: At present, only integer values for "subsample" are
  // supported.
  Vf_GridVec3f(const Vf_GridVec3f& import_mesh,
	       OC_REAL8m subsample,const char* flipstr,
	       Nb_BoundingBox<OC_REAL8>& clipbox,
	       OC_BOOL cliprange);

  // Copy constructor with periodic translation.
  Vf_GridVec3f(const Vf_GridVec3f& import_mesh,
               OC_INDEX ioff,OC_INDEX joff,OC_INDEX koff);

  // Copy with resampling from another mesh.  The new mesh has extent
  // described by import newrange.  The number of cells along the x, y,
  // and z axes are icount, jcount, and kcount, respectively.  The
  // method order specifies the interpolation method; it should be
  // either 0 (nearest), 1 (trilinear) or 3 (tricubic).
  void ResampleCopy(const Vf_GridVec3f& import_mesh,
                    const Nb_BoundingBox<OC_REAL8> &newrange,
                    OC_INDEX icount,OC_INDEX jcount,OC_INDEX kcount,
                    int method_order);

  // ResampleCopyAverage is similar to ResampleCopy, except rather than
  // resampling through samples on a fit curve, instead each resampled
  // value is the average value from the cells in source mesh lying in
  // the footprint of the resample cell.
  void ResampleCopyAverage(const Vf_GridVec3f& import_mesh,
                    const Nb_BoundingBox<OC_REAL8> &newrange,
                    OC_INDEX icount,OC_INDEX jcount,OC_INDEX kcount);


  inline Nb_Vec3<OC_REAL8>& GridVec(OC_INDEX i,OC_INDEX j,OC_INDEX k) {
    return grid(k,j,i);
  }
  inline Nb_Vec3<OC_REAL8>& operator()(OC_INDEX i,OC_INDEX j,OC_INDEX k) {
    return GridVec(i,j,k);
  }
  inline const Nb_Vec3<OC_REAL8>&
  GridVec(OC_INDEX i,OC_INDEX j,OC_INDEX k) const {
    return grid(k,j,i);
  }
  inline const Nb_Vec3<OC_REAL8>&
  operator()(OC_INDEX i,OC_INDEX j,OC_INDEX k) const {
    return GridVec(i,j,k);
  }
  Nb_Vec3<OC_REAL8> Position(OC_INDEX i,OC_INDEX j,OC_INDEX k) const {
    return NodalToProblemCoords(Nb_Vec3<OC_REAL8>(OC_REAL8(i),
                                                  OC_REAL8(j),OC_REAL8(k)));
  }
  void GetPreciseRange(Nb_BoundingBox<OC_REAL8> &myrange) const;
  void GetPreciseDataRange(Nb_BoundingBox<OC_REAL8> &myrange) const;
  virtual void GetPreciseBoundaryList(Nb_List< Nb_Vec3<OC_REAL8> >
                                      &boundary_list) const {
    boundary_list=boundary;
  }

  OC_BOOL IsBoundaryFromData() const { return boundary_from_data; }

  Nb_Vec3<OC_REAL4>  GetApproximateCellDimensions() const {
    return Nb_Vec3<OC_REAL4>(OC_REAL4(fabs(coords_step.x)),
			  OC_REAL4(fabs(coords_step.y)),
			  OC_REAL4(fabs(coords_step.z)));
  }
  OC_REAL4m GetSubsampleGrit(void) const { return 1.0; }
  virtual OC_INDEX ColorQuantityTypes(Nb_List<Nb_DString> &types) const;
  virtual OC_BOOL ColorQuantityTransform
  (const Nb_DString flipstr,
   const Nb_DString& quantity_in,OC_REAL8m phase_in,OC_BOOL invert_in,
   Nb_DString& quantity_out,OC_REAL8m& phase_out,OC_BOOL& invert_out) const;
  virtual OC_REAL4m GetVecShade(const char* colorquantity,
			     OC_REAL8m phase,OC_BOOL invert,
			     const Nb_Vec3<OC_REAL4>& v) const;
  virtual OC_INDEX GetDisplayList(OC_REAL4m &xstep_request,
			       OC_REAL4m &ystep_request,
			       OC_REAL4m &zstep_request,
			       const Nb_BoundingBox<OC_REAL4> &dsprange,
			       const char *colorquantity,
			       OC_REAL8m phase,
                               OC_BOOL invert, OC_BOOL trimtiny,
			       Nb_List<Vf_DisplayVector> &display_list);

  virtual void GetZslice(OC_REAL8m zlow,OC_REAL8m zhigh,
			 OC_INDEX& islicelow,OC_INDEX& islicehigh);

  virtual OC_INDEX GetZsliceCount() {
    return grid.GetDim1(); // NB: grid dim1 is z-axis
  }

  virtual void GetZsliceParameters(OC_REAL8m& zslicebase,OC_REAL8m& zstep);

  virtual OC_BOOL FindPreciseClosest(const Nb_Vec3<OC_REAL8> &pos,
                                     Nb_LocatedVector<OC_REAL8> &lv);
  virtual ~Vf_GridVec3f(void) {}

  virtual OC_BOOL
  GetFirstPt(Vf_Mesh_Index* &key,Nb_LocatedVector<OC_REAL8> &lv) const;

  virtual OC_BOOL
  GetNextPt(Vf_Mesh_Index* &key,Nb_LocatedVector<OC_REAL8> &lv) const;

};

//////////////////////////////////////////////////////////////////////////
// General (non-regular) 3D mesh of Nb_Vec3<OC_REAL8>'s
class Vf_GeneralMesh3f_Index:public Vf_Mesh_Index
{
public:
  Vf_BoxList_Index key;
};

class Vf_GeneralMesh3f:public Vf_Mesh
{ 
private:
  static const ClassDoc class_doc;
  static const OC_INDEX zslice_count;  // Number of zslices.
  Vf_BoxList mesh;
  Nb_BoundingBox<OC_REAL8> data_range;  // Minimal box containing data pts
  Nb_BoundingBox<OC_REAL8> range;       // Total range, data + bdry.
  void UpdateRange();  // Should be called by any member function
  /// that affects effective total range, e.g., data, boundary,
  /// or step hints (which determine the margin).  See UpdateRange()
  /// function body for complete list.

  // If boundary_from_data is false, then boundary holds vertex list
  // that was externally specified, and boundary_range has been set
  // to properly enclose that list.  If boundary_from_data is true,
  // then the boundary list and boundary_range change each time a new
  // point is added.  Therefore, both boundary_range and boundary
  // should be ignored, and associated data should be generated from
  // data_range and step_hints as needed.
  OC_BOOL boundary_from_data;
  Nb_BoundingBox<OC_REAL8> boundary_range;
  Nb_List< Nb_Vec3<OC_REAL8> > boundary;    // 2D boundary vertices

  Nb_Vec3<OC_REAL8> step_hints;        // Hints on grid structure
  /// Also affects boundary if boundary_from_data is true.

  // The following functions are left undefined on purpose
  Vf_GeneralMesh3f& operator=(const Vf_GeneralMesh3f& rhs); // Assignment op
  Vf_GeneralMesh3f(const Vf_GeneralMesh3f&);  // Copy constructor

  // Provided for convenience for mesh arrays.
  Vf_GeneralMesh3f(void):Vf_Mesh("Vf_GeneralMesh3f",NULL) {}

protected:
  // For use by generic Vf_Mesh code for manipulation of
  // mesh node values (as opposed to node locations).
  // Returns 0 on success, 1 if the index is invalid
  // or out-of-bounds.
  virtual OC_BOOL SetNodeValue(const Vf_Mesh_Index* key,
                               const Nb_Vec3<OC_REAL8>& value);

public:

  ////////////// Generic Mesh functions ///////////////////

  virtual const char* GetMeshType() const { return class_doc.classname; }

  Vf_GeneralMesh3f(const char* filename,const char* title=NULL,
                   const char* desc=NULL,
		   const char* meshunit=NULL,
		   const char* valueunit=NULL,
		   OC_REAL8 valuemultiplier=1.0)
    :Vf_Mesh(filename,title,desc,
	     meshunit,valueunit,valuemultiplier),
    boundary_from_data(1) {}
  /// The preceding constructor should be called from file input
  /// functions.

  // Copy constructor with transform.
  // NOTE: At present, "subsample" not supported.
  Vf_GeneralMesh3f(const Vf_Mesh& import_mesh,
		   OC_REAL8m subsample,const char* flipstr,
		   Nb_BoundingBox<OC_REAL8>& clipbox,
		   OC_BOOL cliprange);

  void SetMeshTitle(const char* title) { SetTitle(title); }
  void SetMeshDescription(const char* desc) { SetDescription(desc); }

  virtual ~Vf_GeneralMesh3f(void) {}

  OC_INDEX GetSize() const { return mesh.GetSize();  }
  void GetPreciseRange(Nb_BoundingBox<OC_REAL8> &myrange) const;
  void GetPreciseDataRange(Nb_BoundingBox<OC_REAL8> &myrange) const;
  virtual void GetPreciseBoundaryList(Nb_List< Nb_Vec3<OC_REAL8> >
                                      &boundary_list) const;
  OC_BOOL IsBoundaryFromData() const { return boundary_from_data; }

  Nb_Vec3<OC_REAL4>  GetApproximateCellDimensions() const;

  OC_REAL4m GetSubsampleGrit(void) const { return OC_REAL4m(0.1); }
  /// This is suppose to be a sensible lower bound on the sampling
  /// period relative to cellsize.  This should probably be set to the
  /// smallest neighbor distance in the mesh, but for now we'll just
  /// use 0.1.
  virtual OC_INDEX ColorQuantityTypes(Nb_List<Nb_DString> &types) const;
  virtual OC_BOOL ColorQuantityTransform
  (const Nb_DString flipstr,
   const Nb_DString& quantity_in,OC_REAL8m phase_in,OC_BOOL invert_in,
   Nb_DString& quantity_out,OC_REAL8m& phase_out,OC_BOOL& invert_out) const;
  virtual OC_REAL4m GetVecShade(const char* colorquantity,
			     OC_REAL8m phase,OC_BOOL invert,
			     const Nb_Vec3<OC_REAL4>& v) const;
  virtual OC_INDEX GetDisplayList(OC_REAL4m &xstep_request,
			       OC_REAL4m & ystep_request,
			       OC_REAL4m & zstep_request,
			       const Nb_BoundingBox<OC_REAL4> &range_request,
			       const char *colorquantity,
			       OC_REAL8m phase,
                               OC_BOOL invert, OC_BOOL trimtiny,
			       Nb_List<Vf_DisplayVector> &display_list);

  virtual void GetZslice(OC_REAL8m zlow,OC_REAL8m zhigh,
			 OC_INDEX& islicelow,OC_INDEX& islicehigh);

  virtual OC_INDEX GetZsliceCount() { return zslice_count; }

  virtual void GetZsliceParameters(OC_REAL8m& zslicebase,OC_REAL8m& zstep);

  virtual OC_BOOL FindPreciseClosest(const Nb_Vec3<OC_REAL8> &pos,
                                     Nb_LocatedVector<OC_REAL8> &lv);

  ////////////// Vf_GeneralMesh3f specific functions ///////////////////
  void ResetMesh(); // Resets data w/o changing filename, title or desc.
  void ResetMesh(const char* newFilename); // Resets data & filename
  void ResetMesh(const char* newFilename,const char* newTitle,
		 const char* newDescription);
  /// Resets data, filename, title and description.

  void AddPoint(const Nb_LocatedVector<OC_REAL8>& lv);
  void SortPoints();   // Post point import processing, for efficient
  // accessing.  Will be called automatically by GetDisplayList() if needed,
  // but client access provided to allow this processing to be scheduled,
  // since this may take more than a few CPU cycles.

  // There are two SetBoundaryList calls.  The first explicitly
  // sets the boundary list, and sets boundary_from_data false.
  // The second sets boundary_from_data true, and subsequent requests
  // for boundary or range data are generated on the fly; this allows
  // them to stay up-to-date with data_range and step_hints.
  void SetBoundaryList(const Nb_List< Nb_Vec3<OC_REAL8> > &boundary_list);
  void SetBoundaryList();


  // SetBoundaryRange sets boundary_range without adjusting either
  // boundary_list or boundary_from_data.  This is intended for use
  // by file input functions, and should otherwise be used only with
  // extreme caution.
  void SetBoundaryRange(const Nb_BoundingBox<OC_REAL8> &newrange) {
    boundary_range = newrange;
    UpdateRange();
  }

  void SetApproximateStepHints();
  void SetStepHints(const Nb_Vec3<OC_REAL8> &_step);
  Nb_Vec3<OC_REAL8> GetStepHints() const { return step_hints; }
  virtual OC_BOOL
  GetFirstPt(Vf_Mesh_Index* &key,Nb_LocatedVector<OC_REAL8> &lv) const;

  virtual OC_BOOL
  GetNextPt(Vf_Mesh_Index* &key,Nb_LocatedVector<OC_REAL8> &lv) const;

};

#endif  // _VF_MESH
