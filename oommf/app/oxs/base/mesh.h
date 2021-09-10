/* FILE: mesh.h                 -*-Mode: c++-*-
 *
 * Abstract Oxs_Mesh class, derives from Oxs_Ext.
 *
 */

#ifndef _OXS_MESH
#define _OXS_MESH

#include "vf.h"

#include "threevector.h"
#include "util.h"
#include "ext.h"

/* End includes */

template<class T> class Oxs_MeshValue;  // Forward declaration

////////////////////////////////////////////////////////////////////////
// Oxs_Mesh (generic mesh) class
class Oxs_Mesh : public Oxs_Ext, public Vf_Ovf20_MeshNodes {
private:
  // Disable copy constructor and assignment operator by declaring
  // them without defining them.
  Oxs_Mesh(const Oxs_Mesh&);
  Oxs_Mesh& operator=(const Oxs_Mesh&);
protected:
  // Default vector field output using generic Oxs_Mesh interface
  void WriteOvfIrregular
  (Tcl_Channel channel,   // These have same meaning
   OC_BOOL headers,          // as imports to WriteOvf().
   const char* title,
   const char* desc,
   const char* valueunit,
   const char* datatype,
   const char* precision,
   const Oxs_MeshValue<ThreeVector>* vec,
   const Oxs_MeshValue<OC_REAL8m>* scale=NULL,
   const ThreeVector* stephints=NULL // If non-null, then the values are
                                   /// written as stephints to the file.
   ) const;

  Oxs_Mesh(const char* name,Oxs_Director* newdtr);
  Oxs_Mesh(const char* name,Oxs_Director* newdtr,const char* argstr);

  // Helper function for Vf_Ovf20_MeshNodes interface function
  // DumpGeometry.
  void DumpIrregGeometry(Vf_Ovf20FileHeader& header) const;

public:
  virtual ~Oxs_Mesh() {}

  virtual void GetBoundingBox(Oxs_Box& bbox) const =0;
  // Bounding region for mesh

  virtual OC_REAL8m TotalVolume() const =0; // Sum of volumes of all basis elts

  virtual OC_REAL8m Volume(OC_INDEX index) const=0;
  /// Integral of basis elt 'index'
  /// NOTE: This function must be written to be thread-safe, under the
  /// assumption that the underlying mesh object is not changed in
  /// another thread.

  virtual void VolumePair(OC_INDEX index,Oc_Duet& vol0) const=0;
  virtual void VolumeQuad(OC_INDEX index,Oc_Duet& vol0,Oc_Duet& vol2) const=0;
  // VolumePair and VolumeQuad are extensions of Volume() that fill
  // the export variable(s) vol# with the cell volumes at index 'index'
  // and subsequent indices, like so:
  //    vol0.GetA() --> Volume(index);
  //    vol0.GetB() --> Volume(index+1);
  //    vol2.GetA() --> Volume(index+2);
  //    vol2.GetB() --> Volume(index+3);
  // It is the responsibility of the calling routine to insure that the
  // specified index range consists entirely of valid mesh indices.
  //   The point of VolumePair and VolumeQuad is to reduce the number of
  // virtual function calls; this can have a significant impact inside a
  // tight loop.  (The problem being not the "virtual", i.e., indirect,
  // nature of the call but rather that as a virtual function there is
  // no possiblility of inlining what would otherwise be a very quick
  // accessor.)  In priciple it would be simple to provide default
  // implementations for these functions, e.g.,
  //
  //    virtual void VolumeQuad(OC_INDEX index,
  //                            Oc_Duet& vol0,Oc_Duet& vol2) const {
  //        vol0.Set(Volume(index),Volume(index+1));
  //        vol2.Set(Volume(index+2),Volume(index+4));
  //    }
  //
  // but using such would actually increase the number of function
  // calls, so these are left pure virtual functions to be filled in by
  // mesh children classes.
  //   Note that the thread-safe restrictions for Volume apply to these
  // functions as well.

  virtual void Center(OC_INDEX index,ThreeVector& location) const =0;
  /// Center of mass of basis elt 'index'

  // Vf_Ovf20_MeshNodes interface
  virtual OC_BOOL SupportsOutputType(Vf_Ovf20_MeshType type) const =0;
  virtual Vf_Ovf20_MeshType NaturalType() const =0;
  virtual OC_INDEX Size() const =0;  // Number of basis elements
  virtual void GetCellCenter(OC_INDEX index,
                             OC_REAL8m& x,OC_REAL8m& y, OC_REAL8m& z) const {
    ThreeVector center;
    Center(index,center);
    x=center.x; y=center.y; z=center.z;
  }
  virtual void DumpGeometry(Vf_Ovf20FileHeader& header,
                            Vf_Ovf20_MeshType type) const =0;

  virtual OC_INDEX FindNearestIndex(const ThreeVector& location) const =0;
  /// Returns index with center of mass closest to location.  In
  /// case of tie, returns any one of the closest indices.

  // Some algorithms require or can be done more efficiently
  // if all cells in the underlying mesh have the same volume.
  virtual OC_BOOL HasUniformCellVolumes() const =0;
  virtual OC_BOOL HasUniformCellVolumes(OC_REAL8m& vol) const =0;

  // Channel output.  Throws an exception on error.
  // Child classes may use WriteOvfIrregular() above to handle
  // irregular grid output.
  //   NOTE: Perhaps "valueunit" should be embedded into the
  // Oxs_MeshValue class???
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
   const Oxs_MeshValue<ThreeVector>* vec,   // Vector array
   const Oxs_MeshValue<OC_REAL8m>* scale=NULL  // Optional scaling for vec
                      /// Set scale to NULL to use vec values directly.
   ) const =0 ;

  // Version of WriteOvf that takes a filename instead of a Tcl_Channel.
  // This is for backwards compatibility and convenience.  (Note: By
  // giving this routine a different name than WriteOvf, as opposed to
  // relying on the signature, we allow it to be directly visible in
  // child classes.)
  void WriteOvfFile
  (const char* filename,  // Name of output file
   OC_BOOL headers,          // If false, then output only raw data
   const char* title,     // Long filename or title
   const char* desc,      // Description to embed in output file
   const char* valueunit, // Field units, such as "A/m".
   const char* meshtype,  // Either "rectangular" or "irregular"
   const char* datatype,  // Either "binary" or "text"
   const char* precision, // For binary, "4" or "8";
                          ///  for text, a printf-style format
   const Oxs_MeshValue<ThreeVector>* vec,   // Vector array
   const Oxs_MeshValue<OC_REAL8m>* scale=NULL  // Optional scaling for vec
                      /// Set scale to NULL to use vec values directly.
   ) const;

  // Deprecated interface to the previous routine.  This can not
  // be called directly from a derived class; instead you must
  // upcast to Oxs_Mesh.
  void WriteOvf
  (const char* filename,  // Name of output file
   OC_BOOL headers,          // If false, then output only raw data
   const char* title,     // Long filename or title
   const char* desc,      // Description to embed in output file
   const char* valueunit, // Field units, such as "A/m".
   const char* meshtype,  // Either "rectangular" or "irregular"
   const char* datatype,  // Either "binary" or "text"
   const char* precision, // For binary, "4" or "8";
                          ///  for text, a printf-style format
   const Oxs_MeshValue<ThreeVector>* vec,   // Vector array
   const Oxs_MeshValue<OC_REAL8m>* scale=NULL  // Optional scaling for vec
                      /// Set scale to NULL to use vec values directly.
   ) const {
    WriteOvfFile(filename,headers,title,desc,valueunit,meshtype,datatype,
                 precision,vec,scale);
  }

  virtual const char* NaturalOutputType() const { return "irregular"; }
  /// Returns a string suitable for use as the "meshtype" field
  /// to the WriteOvf routine.  Should be the most restrictive
  /// or natural type, i.e., "rectangular" if supported, otherwise
  /// "irregular".

  virtual String GetGeometryString() const; // Informational output for end
  /// user.  This routine returns a string that looks like "1000013
  /// cells" (for general meshes) or "383 x 373 x 7 = 1000013 cells"
  /// (from rectangular meshes).

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

  // Max angle routine.  This routine returns the maximum angle between
  // neighboring vectors across the mesh, for all those vectors for
  // which the corresponding entry in zero_check is non-zero.
  // NB: These routines are likely sensitive to boundary conditions.
  virtual OC_REAL8m MaxNeighborAngle
  (const Oxs_MeshValue<ThreeVector>& vec,
   const Oxs_MeshValue<OC_REAL8m>& zero_check,
   OC_INDEX node_start,OC_INDEX node_stop) const =0;
};

#endif // _OXS_MESH
