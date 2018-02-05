/* FILE: rectangularmesh.h            -*-Mode: c++-*-
 *
 * Cell rectangular meshes, derived from Oxs_Mesh class.
 *
 * There are two types of concrete rectangular meshes,
 * Oxs_PeriodicRectangularMesh, which is periodic in at least one
 * dimension, and Oxs_RectangularMesh (so named historically) which is
 * not periodic.  These are both children of the pure abstract class
 * Oxs_CommonRectangularMesh, which encapsulates the common interface
 * (and implementation) between the two children.
 *
 */

#ifndef _OXS_RECTANGULARMESH
#define _OXS_RECTANGULARMESH

#include <string>
#include <vector>

#include "atlas.h"
#include "scalarfield.h"
#include "mesh.h"
#include "vf.h"
#include "oxsexcept.h"

OC_USE_STD_NAMESPACE;
OC_USE_STRING;

/* End includes */

/* Oxs_CommonRectangularMesh provides the common interface (and
 * implementation) between the non-periodic Oxs_RectangularMesh and the
 * periodic Oxs_PeriodicRectangularMesh classes.
 */
class Oxs_CommonRectangularMesh : public Oxs_Mesh {
private:
  ThreeVector base,cellsize;
  OC_REAL8m cellvolume; // Convenience value
  OC_INDEX xdim,ydim,zdim;
  OC_INDEX xydim; // Convenience value: xdim*ydim
  OC_INDEX elementcount;  // Convenience value: xdim*ydim*zdim

  OC_BOOL GetNeighborPoint(const ThreeVector& base_pt,
			OC_INDEX ngbr_index,
			ThreeVector& ngbr_pt) const;
  // Fills ngbr_pt with location of neighbor element indexed
  // by "ngbr_index", relative to base_pt.  Returns 1 if
  // ngbr_pt < number of neighbors (currently 6); otherwise
  // 0 is returned, in which case ngbr_pt is unchanged.
  // NB: ngbr_index is 0-based.

  // Helper function for constructors
  void InitScaling(const Oxs_Box& box);

  // Dummy class for throwing exceptions; this is intended for
  // internal use only
  class Oxs_CommonRectangularMesh_WBError {};

protected:
  // Oxs_Ext interface
  Oxs_CommonRectangularMesh
  (const char* name,     // Child instance id
   Oxs_Director* newdtr, // App director
   const char* argstr);  // MIF input block parameters

  // Secondary constructor; provides a function-level API
  // for use by other Oxs_Ext objects.
  Oxs_CommonRectangularMesh
  (const char* name,     // Child instance id
   Oxs_Director* newdtr, // App director
   const char* argstr,   // MIF input block parameters, for parent use
   const ThreeVector& in_cellsize,
   const Oxs_Box& range_box);

  // Constructor for private use by child MakeRefinedMesh member functions.
  Oxs_CommonRectangularMesh(const char* name,
		      Oxs_Director* newdtr,
		      const ThreeVector& in_base,
		      const ThreeVector& in_cellsize,
		      OC_INDEX in_xdim,OC_INDEX in_ydim,OC_INDEX in_zdim);

  // Read-only access for children
  const ThreeVector& GetBase() const { return base; }
  const ThreeVector& GetCellsize() const { return cellsize; }

public:
  virtual ~Oxs_CommonRectangularMesh() {}

  // Mesh interface
  virtual void GetBoundingBox(Oxs_Box& bbox) const;
  virtual OC_INDEX Size() const { return elementcount; }
  virtual OC_REAL8m TotalVolume() const { return cellvolume*elementcount; }
  virtual OC_REAL8m Volume(OC_INDEX) const { return cellvolume; }
  virtual void VolumePair(OC_INDEX /* index */,Oc_Duet& vol) const {
    vol.Set(cellvolume);
  }
  virtual void VolumeQuad(OC_INDEX /* index */,
                          Oc_Duet& vol0,Oc_Duet& vol2) const {
    vol0.Set(cellvolume);  vol2.Set(cellvolume);
  }
  /// NOTE: Volume, VolumePair, and VolumeQuad are thread-safe, provided
  /// that the underlying mesh object is not changed in another thread.

  virtual void Center(OC_INDEX index,ThreeVector& location) const;
  virtual OC_INDEX FindNearestIndex(const ThreeVector& location) const;
  virtual OC_BOOL HasUniformCellVolumes() const { return 1; }
  virtual OC_BOOL HasUniformCellVolumes(OC_REAL8m& vol) const {
    vol = cellvolume;
    return 1;
  }

  // Vf_Ovf20_MeshNodes interface
  virtual OC_BOOL SupportsOutputType(Vf_Ovf20_MeshType type) const {
    OC_BOOL i_do = 0;
    switch(type) {
    case vf_ovf20mesh_irregular:
    case vf_ovf20mesh_rectangular:
      i_do = 1;
      break;
    default:
      i_do = 0;  // Safety
      break;
    }
    return i_do;
  }

  virtual Vf_Ovf20_MeshType NaturalType() const {
    return vf_ovf20mesh_rectangular;
  }

  virtual String GetGeometryString() const; // Informational output
  /// for end user.  This routine returns a string that looks like
  /// "383 x 373 x 7 = 1000013 cells".

  virtual void DumpGeometry(Vf_Ovf20FileHeader& header,
                            Vf_Ovf20_MeshType type) const;


  // File (channel) output routines.  These throw an exception on error.
  virtual void WriteOvf
  (Tcl_Channel channel,   // Output channel
   OC_BOOL headers,          // If false, then output only raw data
   const char* title,     // Long filename or title
   const char* desc,      // Description to embed in output file
   const char* valueunit, // Field units, such as "A/m".
   const char* meshtype,  // Either "rectangular" or "irregular"
   const char* datatype,  // Either "binary" or "text"
   const char* precision, // For binary, "4" or "8";
                          ///  for text, a printf-style format
   const Oxs_MeshValue<ThreeVector>* vec,  // Vector array
   const Oxs_MeshValue<OC_REAL8m>* scale=NULL // Optional scaling for vec
   /// Set scale to NULL to use vec values directly.
   ) const;

  // Conversion routines from Vf_Mesh to Oxs_MeshValue<ThreeVector>.
  // IsCompatible returns true iff vfmesh is a Vf_GridVec3f with
  //   dimensions identical to those of *this.
  // NB: IsCompatible only compares the mesh dimensions, not the
  //   underlying physical scale, or the cell aspect ratios.  Do
  //   we want to include such a check?, or is it more flexible
  //   to leave it out?
  // FillMeshValueExact copies the vector field held in vfmesh to the
  //   export Oxs_MeshValue<ThreeVector> vec.  This routine throws
  //   an exception on error, the primary cause of which is that
  //   vfmesh is not compatible with *this.  In other words, if you
  //   don't want to catch the exception, call IsCompatible first.
  //   The "Exact" in the name refers to the requirement that the
  //   dimensions on vfmesh exactly match thos of *this.
  OC_BOOL IsCompatible(const Vf_Mesh* vfmesh) const;
  void FillMeshValueExact(const Vf_Mesh* vfmesh,
                          Oxs_MeshValue<ThreeVector>& vec) const;

  virtual const char* NaturalOutputType() const { return "rectangular"; }

  // Summing routines.  The terms are weighted by the basis element volumes.
  virtual OC_REAL8m VolumeSum(const Oxs_MeshValue<OC_REAL8m>& scalar) const;
  virtual ThreeVector VolumeSum(const Oxs_MeshValue<ThreeVector>& vec) const;
  virtual ThreeVector VolumeSum(const Oxs_MeshValue<ThreeVector>& vec,
				const Oxs_MeshValue<OC_REAL8m>& scale) const;
  /// "scale" is elementwise scaling.

  // Extra precision summing routines.  These are the same as above,
  // except that intermediate sums are accumulated into Nb_Xpfloat
  // variables.
  virtual OC_REAL8m VolumeSumXp(const Oxs_MeshValue<OC_REAL8m>& scalar) const;
  virtual ThreeVector VolumeSumXp(const Oxs_MeshValue<ThreeVector>& vec) const;
  virtual ThreeVector VolumeSumXp(const Oxs_MeshValue<ThreeVector>& vec,
				  const Oxs_MeshValue<OC_REAL8m>& scale) const;

  // Rectangular mesh specific functions.
  // IMPORTANT NOTE: Client routines can assume that the storage
  // order is first on x, second on y, third on z (Fortran order),
  // i.e., (0,0,0) (1,0,0) ... (xdim-1,0,0) (0,1,0) (1,1,0) ...
  // (xdim-1,ydim-1,0) (0,0,1) (1,0,1) ... (xdim-1,ydim-1,zdim-1)
  // This locks down the representation in this function, but
  // allows improved access in the clients.  If we need to change
  // representations, we can always introduce a new mesh class. ;^)
  OC_REAL8m EdgeLengthX() const { return cellsize.x; }
  OC_REAL8m EdgeLengthY() const { return cellsize.y; }
  OC_REAL8m EdgeLengthZ() const { return cellsize.z; }
  OC_INDEX DimX() const { return xdim; }
  OC_INDEX DimY() const { return ydim; }
  OC_INDEX DimZ() const { return zdim; }

  // Convenience function for clients that use multiple translations
  // of a single mesh.
  void TranslateBasePt(const ThreeVector& newbasept) { base = newbasept; }

  // Boundary extraction.  Returns list (technically, an STL vector) of
  // indices for those elements inside base_region that have a neighbor
  // (in the 6 nearest ngbr sense) such that the first element lies on
  // one side (the "inside") of the surface specified by the
  // Oxs_ScalarField bdry_surface + bdry_value, and the neighbor lies on
  // the other (the "outside").  If the bdry_side argument is "-", then
  // the "inside" of the surface is the set of those points x for which
  // bdry_surface.Value(x)<bdry_value.  If bdry_side is "+", then the
  // "inside" of the surface is the set of those points x for which
  // bdry_surface.Value(x)>bdry_value.
  // Return value is the number of entries in the export list.
  // NB: The tested neighbors may lie outside the mesh proper, allowing
  // elements on the edge of the mesh to be specified.
  OC_INDEX BoundaryList(const Oxs_Atlas& atlas,
		      const String& region_name,
		      const Oxs_ScalarField& bdry_surface,
		      OC_REAL8m bdry_value,
		      const String& bdry_side,
		      vector<OC_INDEX> &BoundaryIndexList) const;

  // Use the following function to convert a triplet index (x,y,z)
  // to the corresponding address used to access linear MeshValue
  // arrays.
  inline OC_INDEX Index(OC_INDEX x,OC_INDEX y,OC_INDEX z) const {
    return x+y*xdim+z*xydim;
  }

  inline void GetCoords(OC_INDEX index,
                        OC_INDEX& x,OC_INDEX& y,OC_INDEX& z) const {
    z = index/xydim;
    y = (index - z*xydim)/xdim;
    x = index - z*xydim - y*xdim;
  }

  friend struct Oxs_RectangularMeshCoords;
};

// Support structure that may be used by code that wants evolving
// coordinate info without accessing mesh object directly.  This is
// intended primarily for use by threads running across multiple
// processors, so that each thread can maintain a local copy of its
// access point inside a mesh.  Sample usage:
//
//   const Oxs_CommonRectangularMesh* mesh;
//   Oxs_MeshValue<OC_REAL8m> invalue;
//   Oxs_MeshValue<OC_REAL8m> outvalue;
//
//   void ProcessChunk(OC_INT4m /* thread_number */,
//                     OC_INDEX jstart,OC_INDEX jstop) {
//      Oxs_RectangularMeshCoords coords(mesh);
//      coords.SetIndex(jstart);
//      do { // Note: jstart<jstop is guaranteed
//          OC_INDEX j=coords.index;
//          if(0<coords.z && coords.z+1<coords.zdim) {
//              outval[j] = inval[j+coords.xydim] - inval[j-coords.xydim];
//          } else {
//              outval[j] = 0;
//          }
//      } while(coords.AdvanceIndex()<jstop);
//   }


struct Oxs_RectangularMeshCoords {
public:
  OC_INDEX index;
  OC_INDEX x;
  OC_INDEX y;
  OC_INDEX z;
  OC_INDEX xdim;
  OC_INDEX ydim;
  OC_INDEX zdim;
  OC_INDEX xydim;
  OC_INDEX xyzdim;

  inline OC_INDEX AdvanceIndex() {
    // Return value is new index value.
    // NB: There is no check that new index value < xyzdim.
    if((++x)>=xdim) {
      x=0;
      if((++y)>=ydim) {
        y=0;
        ++z;
      }
    }
    return (++index);
  }

  OC_BOOL SetIndex(OC_INDEX new_index) {
    // Note: This function involves two integer division operations,
    // which are probably slow, so don't call more often than necessary.
    if(new_index >= xyzdim || new_index<0) return 0;
    index = new_index;
    z = index/xydim;
    y = (index - z*xydim)/xdim;
    x = index - z*xydim - y*xdim;
    return 1;
  }

  OC_INDEX operator++() { return AdvanceIndex(); } // Prefix increment
  OC_INDEX operator++(int n) { // Postfix increment
    OC_INDEX itmp = index;
    if(n==0) {
      AdvanceIndex();
    } else {
      SetIndex(itmp + n);
    }
    return itmp;
  }
  OC_INDEX operator--() { // Prefix increment
    if( (--index) < 0) {
      index = x = y = z = 0;
    } else {
      if((--x)<0) {
        x=xdim-1;
        if((--y)<0) {
          y=ydim-1;
          --z;
        }
      }
    }
    return index;
  }
  OC_INDEX operator--(int n) { // Postfix increment
    OC_INDEX itmp = index;
    if(n==0) {
      operator--();
    } else {
      SetIndex(itmp - n);
    }
    return itmp;
  }

  Oxs_RectangularMeshCoords(const Oxs_CommonRectangularMesh* mesh)
    : index(0), x(0), y(0), z(0),
      xdim(mesh->xdim), ydim(mesh->ydim), zdim(mesh->zdim),
      xydim(mesh->xydim), xyzdim(mesh->elementcount) {}
  void Reset(const Oxs_CommonRectangularMesh* mesh) {
    index = x = y = z = 0;
    xdim = mesh->xdim;
    ydim = mesh->ydim;
    zdim = mesh->zdim;
    xydim = mesh->xydim;
    xyzdim = mesh->elementcount;
  }
  Oxs_RectangularMeshCoords(const Oxs_Mesh* mesh) {
    const Oxs_CommonRectangularMesh* rectmesh
      = dynamic_cast<const Oxs_CommonRectangularMesh*>(mesh);
    if(!rectmesh) {
      OXS_THROW(Oxs_BadParameter,"Import mesh is not a rectangular mesh.");
    }
    Reset(rectmesh);
  }
};

////////////////////////////////////////////////////////////////////////
class Oxs_RectangularMesh : public Oxs_CommonRectangularMesh {
  // Non-periodic rectangular mesh
private:
  // Constructor for private use by MakeRefinedMesh member function.
  Oxs_RectangularMesh(const char* name,
		      Oxs_Director* newdtr,
		      const ThreeVector& in_base,
		      const ThreeVector& in_cellsize,
		      OC_INDEX in_xdim,OC_INDEX in_ydim,OC_INDEX in_zdim)
    : Oxs_CommonRectangularMesh(name,newdtr,in_base,in_cellsize,
                              in_xdim,in_ydim,in_zdim) {}

public:
  // Oxs_Ext interface
  Oxs_RectangularMesh
  (const char* name,     // Child instance id
   Oxs_Director* newdtr, // App director
   const char* argstr);  // MIF input block parameters
  virtual ~Oxs_RectangularMesh() {}

  virtual const char* ClassName() const; // ClassName() is
  /// automatically generated by the OXS_EXT_REGISTER macro.

  // Secondary constructor; provides a function-level API for use by
  // other Oxs_Ext objects (namely, Oxs_EdgeCorrAnisotropy).
  Oxs_RectangularMesh
  (const char* name,     // Child instance id
   Oxs_Director* newdtr, // App director
   const char* argstr,   // MIF input block parameters
   const ThreeVector& in_cellsize,
   const Oxs_Box& range_box)
    : Oxs_CommonRectangularMesh(name,newdtr,argstr,in_cellsize,range_box)
  {
    VerifyAllInitArgsUsed();
  }

  // Mesh refinement, via generation of a new mesh.
  void MakeRefinedMesh(const char* name,
	       OC_INDEX refine_x,OC_INDEX refine_y,OC_INDEX refine_z,
	       Oxs_OwnedPointer<Oxs_RectangularMesh>& out_mesh) const;

  // Max angle routine.  This routine returns the maximum angle between
  // neighboring vectors across the mesh, for all those vectors for
  // which the corresponding entry in zero_check is non-zero.
  // NB: This routine is boundary condition sensitive.
  virtual OC_REAL8m MaxNeighborAngle
  (const Oxs_MeshValue<ThreeVector>& vec,
   const Oxs_MeshValue<OC_REAL8m>& zero_check,
   OC_INDEX node_start,OC_INDEX node_stop) const;
};

////////////////////////////////////////////////////////////////////////
class Oxs_PeriodicRectangularMesh : public Oxs_CommonRectangularMesh {
  // Periodic rectangular mesh
private:
  int xperiodic;  // If 0, then not periodic.  Otherwise,
  int yperiodic;  // periodic in indicated direction.
  int zperiodic;

  // Note: Currently no MakeRefinedMesh support in this class.
  //       MakeRefinedMesh is used by the Oxs_EdgeCorrAnisotropy class.
public:
  // Oxs_Ext interface
  Oxs_PeriodicRectangularMesh
  (const char* name,     // Child instance id
   Oxs_Director* newdtr, // App director
   const char* argstr);  // MIF input block parameters
  virtual ~Oxs_PeriodicRectangularMesh() {}

  int IsPeriodicX() const { return xperiodic; }
  int IsPeriodicY() const { return yperiodic; }
  int IsPeriodicZ() const { return zperiodic; }

  virtual const char* ClassName() const; // ClassName() is
  /// automatically generated by the OXS_EXT_REGISTER macro.

  // Max angle routine.  This routine returns the maximum angle between
  // neighboring vectors across the mesh, for all those vectors for
  // which the corresponding entry in zero_check is non-zero.
  // NB: This routine is boundary condition sensitive.
  virtual OC_REAL8m MaxNeighborAngle
  (const Oxs_MeshValue<ThreeVector>& vec,
   const Oxs_MeshValue<OC_REAL8m>& zero_check,
   OC_INDEX node_start,OC_INDEX node_stop) const;
};

#endif // _OXS_RECTANGULARMESH
