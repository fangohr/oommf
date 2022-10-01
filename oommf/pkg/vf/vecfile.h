/* FILE: vecfile.h                 -*-Mode: c++-*-
 *
 * Header file for vector field file I/O.
 *
 * Last modified on: $Date: 2016/04/14 02:20:01 $
 * Last modified by: $Author: donahue $
 */

#ifndef _VF_VECFILE
#define _VF_VECFILE

#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

#include "nb.h"
#include "mesh.h"

/* End includes */

OC_USE_STRING;

class Vf_FileInput;  // Forward declaration

//////////////////////////////////////////////////////////////////////
// Vf_FileInputID
//
class Vf_FileInputID
{
  friend class Vf_FileInput;
private:
  static const ClassDoc class_doc;
  Nb_DString filetype;
  OC_BOOL (*pIsType)(const char *filename,
		  FILE *input_file,
		  Tcl_Channel channel,
		  const char *channelName);
  Vf_FileInput* (*pMakeNew)(const char *new_filename,
                            FILE *new_infile,
                            Tcl_Channel new_channel,
                            const char *new_channelName);
  void Dup(const Vf_FileInputID &rhs); // Copy
public:
  // Constructor *with* registration (for global initialization)
  // (Set last field to NULL to avoid registration.)
  Vf_FileInputID(const char *new_filetype,
                 OC_BOOL (*newpIsType)(const char *filename,
                                    FILE *input_file,
                                    Tcl_Channel channel,
                                    const char *channelName),
                 Vf_FileInput *
                 (*newpNewReader)(const char *filename,
                                  FILE *input_file,
                                  Tcl_Channel channel,
                                  const char *channelName),
                 OC_BOOL (*pRegister)(const Vf_FileInputID &)
                 );

  // Copy constructor
  Vf_FileInputID(const Vf_FileInputID &copyobj);

  Vf_FileInputID(); // Default constructor; Needed by Nb_List<T>
  Vf_FileInputID& operator=(const Vf_FileInputID &rhs);

  // Dummy constructor taking const char*.  This is wanted by
  // compilers (e.g., gcc) that try to instantiate all template
  // member functions regardless of whether or not they are
  // ever called.  (The template in question is Nb_List<T>.)
  Vf_FileInputID(const char*);

  ~Vf_FileInputID();
};

//////////////////////////////////////////////////////////////////////
// Vf_FileInput
//
class Vf_FileInput
{
  // NOTES ON CHILD CLASSES:
  //
  // Every child class should have a static member function defined
  // like this:
  //
  // static Vf_FileInput* MakeNew(const char *new_filename,
  //                              FILE *new_infile,
  //                              Tcl_Channel new_channel,
  //	                          const char *new_channelName);
  //
  // which calls its "internal" constructor.  This function is passed
  // to Vf_FileInput::RegisterChild as part of a Vf_FileInputID.
  //
  // All children classes should have a static member variable:
  //
  //   static Vf_FileInputID id;
  //
  // The initialization of this variable should call
  // Vf_FileInput::RegisterChild(id) so that they can be
  // called as necessary from Vf_FileInput::NewReader().
  //
  // The FILE *infile is closed in the base class destructor.

private:
  static const ClassDoc class_doc;
  static Nb_List<Vf_FileInputID> child_list;

  // The following 3 functions are left undefined on purpose
  Vf_FileInput(); // Default constructor
  Vf_FileInput(const Vf_FileInput &copyobj);  // Copy constructor
  Vf_FileInput& operator=(const Vf_FileInput &rhs); // Assignment op

protected:
  const Nb_DString filename;
  Nb_DString filetitle; // (Long) filetitle
  Nb_DString filedesc;  // File description
  FILE *infile;
  Tcl_Channel channel;     // Channel registered with omfInterp
  const char *channelName; // (omfInterp is a global variable defined
  // in tclcmds.cc, which is a pointer to the current Tcl command
  // interpreter.)
  // NOTE: The file is "registered" with the Tcl interpreter inside
  // "NewReader", and so must be closed *only* by a corresponding
  // call to Tcl_UnregisterChannel(), which occurs in the
  // ~Vf_FileInput destructor, NOT IN ANY OF THE CHILD CLASSES!

  // Internal constructor
  Vf_FileInput(const char *new_filename,
		       FILE *new_infile,
		       Tcl_Channel new_channel,
		       const char *new_channelName);

  static OC_BOOL RegisterChild(const Vf_FileInputID &id);

public:
  // Clients should call NewReader() to get a VFFI instance.  The
  // client is responsible for destroying (via delete) the returned
  // object, i.e., treat this as a "new" operator.
  static Vf_FileInput *NewReader(const char *new_filename,
                                 Nb_DString* errmsg=NULL);
  virtual ~Vf_FileInput();
  const char *GetFileName(void) { return (const char *)filename; }
  virtual const char *FileType(void)=0;
  virtual Vf_Mesh *NewMesh(void)=0;  // Derived classes should return
  /// a *Vf_EmptyMesh on error (not NULL).  Either way, the client
  /// is responsible for destroying (via delete) the returned mesh.
};

//////////////////////////////////////////////////////////////////////
// Ovf File Format specs and utility routines

enum Vf_OvfDataStyle { vf_oinvalid, vf_oascii, vf_obin4, vf_obin8 };
/// Format of data in "Data" block: ASCII (text),
/// 4-byte IEEE binary, 8-byte IEEE binary

enum Vf_OvfFileVersion { vf_ovf_invalid, vf_ovf_10, vf_ovf_20 };
#define vf_ovf_latest vf_ovf_20
/// Two recognized versions: 1.0 and 2.0

class Vf_OvfFileFormatSpecs
{
private:
  static const ClassDoc class_doc;
  static const OC_REAL4 CheckValue4;  // 4-byte real data block check value
  static const OC_REAL8 CheckValue8;  // 8-byte   "                     "

public:

  static Vf_OvfFileVersion GetFileVersion(const char *test);
  /// Checks "Magic" string at top of .ovf files.

  static OC_BOOL CheckValue(OC_REAL4 test) { return test==CheckValue4; }
  static OC_BOOL CheckValue(OC_REAL8 test) { return test==CheckValue8; }

  // The use of the CheckValue functions is encouraged, but
  // the actual values are needed for creating ovf files.
  static void GetCheckValue(OC_REAL4 &check) { check=CheckValue4; }
  static void GetCheckValue(OC_REAL8 &check) { check=CheckValue8; }

  // Binary I/O routines that perform byteswapping as needed.
  static OC_INT4m ReadBinary(FILE *infile,OC_REAL4 *buf,OC_INDEX count);
  static OC_INT4m ReadBinary(FILE *infile,OC_REAL8 *buf,OC_INDEX count);
  static OC_INT4m ReadBinaryFromLSB(FILE *infile,OC_REAL4 *buf,OC_INDEX count);
  static OC_INT4m ReadBinaryFromLSB(FILE *infile,OC_REAL8 *buf,OC_INDEX count);
  static OC_INT4m WriteBinary(FILE *outfile,OC_REAL4 *buf,OC_INDEX count);
  static OC_INT4m WriteBinary(FILE *outfile,OC_REAL8 *buf,OC_INDEX count);
  static OC_INT4m WriteBinary(Tcl_Channel channel,OC_REAL4 *buf,OC_INDEX count);
  static OC_INT4m WriteBinary(Tcl_Channel channel,OC_REAL8 *buf,OC_INDEX count);
  // NOTE: The write routines may modify buf during byteswapping.
  //   If this is unacceptable, then one should add additional
  //   routines that take a const buf, and copy buf into internal
  //   storage.


#if OC_INDEX_CHECKS
  // OC_INDEX import type checks for the above.  We declare but
  // don't define the following, so if they are used, it should
  // raise an error at link time.  Find and fix!
  //   Note: It is rather convenient to allow reads and writes
  // with less than full OC_INDEX width, so we only check for
  // accesses that usea wider type.
# if OC_INDEX_WIDTH < 2 && OC_HAS_INT2
  static OC_INT4m ReadBinary(FILE *infile,OC_REAL4 *buf,OC_INT2 count);
  static OC_INT4m ReadBinary(FILE *infile,OC_REAL8 *buf,OC_INT2 count);
  static OC_INT4m ReadBinaryFromLSB(FILE *infile,OC_REAL4 *buf,OC_INT2 count);
  static OC_INT4m ReadBinaryFromLSB(FILE *infile,OC_REAL8 *buf,OC_INT2 count);
  static OC_INT4m WriteBinary(FILE *outfile,OC_REAL4 *buf,OC_INT2 count);
  static OC_INT4m WriteBinary(FILE *outfile,OC_REAL8 *buf,OC_INT2 count);
  static OC_INT4m WriteBinary(Tcl_Channel channel,OC_REAL4 *buf,OC_INT2 count);
  static OC_INT4m WriteBinary(Tcl_Channel channel,OC_REAL8 *buf,OC_INT2 count);
  static OC_INT4m ReadBinary(FILE *infile,OC_REAL4 *buf,OC_UINT2 count);
  static OC_INT4m ReadBinary(FILE *infile,OC_REAL8 *buf,OC_UINT2 count);
  static OC_INT4m ReadBinaryFromLSB(FILE *infile,OC_REAL4 *buf,OC_UINT2 count);
  static OC_INT4m ReadBinaryFromLSB(FILE *infile,OC_REAL8 *buf,OC_UINT2 count);
  static OC_INT4m WriteBinary(FILE *outfile,OC_REAL4 *buf,OC_UINT2 count);
  static OC_INT4m WriteBinary(FILE *outfile,OC_REAL8 *buf,OC_UINT2 count);
  static OC_INT4m WriteBinary(Tcl_Channel channel,OC_REAL4 *buf,OC_UINT2 count);
  static OC_INT4m WriteBinary(Tcl_Channel channel,OC_REAL8 *buf,OC_UINT2 count);
# endif
# if OC_INDEX_WIDTH < 4 && OC_HAS_INT4
  static OC_INT4m ReadBinary(FILE *infile,OC_REAL4 *buf,OC_INT4 count);
  static OC_INT4m ReadBinary(FILE *infile,OC_REAL8 *buf,OC_INT4 count);
  static OC_INT4m ReadBinaryFromLSB(FILE *infile,OC_REAL4 *buf,OC_INT4 count);
  static OC_INT4m ReadBinaryFromLSB(FILE *infile,OC_REAL8 *buf,OC_INT4 count);
  static OC_INT4m WriteBinary(FILE *outfile,OC_REAL4 *buf,OC_INT4 count);
  static OC_INT4m WriteBinary(FILE *outfile,OC_REAL8 *buf,OC_INT4 count);
  static OC_INT4m WriteBinary(Tcl_Channel channel,OC_REAL4 *buf,OC_INT4 count);
  static OC_INT4m WriteBinary(Tcl_Channel channel,OC_REAL8 *buf,OC_INT4 count);
  static OC_INT4m ReadBinary(FILE *infile,OC_REAL4 *buf,OC_UINT4 count);
  static OC_INT4m ReadBinary(FILE *infile,OC_REAL8 *buf,OC_UINT4 count);
  static OC_INT4m ReadBinaryFromLSB(FILE *infile,OC_REAL4 *buf,OC_UINT4 count);
  static OC_INT4m ReadBinaryFromLSB(FILE *infile,OC_REAL8 *buf,OC_UINT4 count);
  static OC_INT4m WriteBinary(FILE *outfile,OC_REAL4 *buf,OC_UINT4 count);
  static OC_INT4m WriteBinary(FILE *outfile,OC_REAL8 *buf,OC_UINT4 count);
  static OC_INT4m WriteBinary(Tcl_Channel channel,OC_REAL4 *buf,OC_UINT4 count);
  static OC_INT4m WriteBinary(Tcl_Channel channel,OC_REAL8 *buf,OC_UINT4 count);
# endif
# if OC_INDEX_WIDTH < 8 && OC_HAS_INT8
  static OC_INT4m ReadBinary(FILE *infile,OC_REAL4 *buf,OC_INT8 count);
  static OC_INT4m ReadBinary(FILE *infile,OC_REAL8 *buf,OC_INT8 count);
  static OC_INT4m ReadBinaryFromLSB(FILE *infile,OC_REAL4 *buf,OC_INT8 count);
  static OC_INT4m ReadBinaryFromLSB(FILE *infile,OC_REAL8 *buf,OC_INT8 count);
  static OC_INT4m WriteBinary(FILE *outfile,OC_REAL4 *buf,OC_INT8 count);
  static OC_INT4m WriteBinary(FILE *outfile,OC_REAL8 *buf,OC_INT8 count);
  static OC_INT4m WriteBinary(Tcl_Channel channel,OC_REAL4 *buf,OC_INT8 count);
  static OC_INT4m WriteBinary(Tcl_Channel channel,OC_REAL8 *buf,OC_INT8 count);
  static OC_INT4m ReadBinary(FILE *infile,OC_REAL4 *buf,OC_UINT8 count);
  static OC_INT4m ReadBinary(FILE *infile,OC_REAL8 *buf,OC_UINT8 count);
  static OC_INT4m ReadBinaryFromLSB(FILE *infile,OC_REAL4 *buf,OC_UINT8 count);
  static OC_INT4m ReadBinaryFromLSB(FILE *infile,OC_REAL8 *buf,OC_UINT8 count);
  static OC_INT4m WriteBinary(FILE *outfile,OC_REAL4 *buf,OC_UINT8 count);
  static OC_INT4m WriteBinary(FILE *outfile,OC_REAL8 *buf,OC_UINT8 count);
  static OC_INT4m WriteBinary(Tcl_Channel channel,OC_REAL4 *buf,OC_UINT8 count);
  static OC_INT4m WriteBinary(Tcl_Channel channel,OC_REAL8 *buf,OC_UINT8 count);
# endif
#endif // OC_INDEX_CHECKS

};

//////////////////////////////////////////////////////////////////////
// Vf_OvfSegmentHeader
//
enum Vf_OvfSegmentHeadMeshType { vf_oshmt_rectangular, vf_oshmt_irregular  };
struct Vf_OvfSegmentHeader
{
private:
  static const ClassDoc class_doc;
public:
  // HEADER VALUES
  Nb_DString title; // Long file name or title goes here
  Nb_DString desc;  // File description
  Nb_DString meshunit; // Fundamental mesh measurement unit. 
                      /// Treated as a label. Example: nm
  Vf_OvfSegmentHeadMeshType meshtype;

  Vf_OvfFileVersion fileversion;

  OC_REAL8 xbase,ybase,zbase; // (xbase,ybase,zbase) is the position, in
                           // 'meshunit', of the first point in the data
                           //  section (below).

  OC_REAL8 xstepsize,ystepsize,zstepsize;  // Distance between adjacent
  // grid pts.: on the x-axis, 20 nm, etc.  The sign on these values
  // determines the grid orientation relative to (xbase,ybase,zbase).

  OC_INDEX xnodes,ynodes,znodes; // Number of nodes along each axis, regular grids.
  OC_INDEX pointcount;           // Number of points, for irregular grids.

  OC_REAL8 xmin,ymin,zmin;  // Corner points defining mesh boundary.  The
  OC_REAL8 xmax,ymax,zmax;  // bounding box is set to include both these
  /// points and the extent from ?base, ?stepsize and ?nodes.

  Nb_List< Nb_Vec3<OC_REAL8> > bdryline;  // Points defining a polyline, used to
  /// indicate boundary.  Optional; if missing, a bdry will be constructed
  /// from the ?min & ?max boundary points.

  Nb_DString valueunit; // Fundamental field value unit, treated as a
  /// label. Example: kA/m  See also valuedim and valueunits_list
  /// variables defined below for use in v2.0 files.

  OC_REAL8 valuemultiplier; // Multiply file values by this to get
                         // true value in 'valueunits'.

  OC_REAL8 valuerangemaxmag,valuerangeminmag; // These are in file value
  // units, and are used as hints (or defaults) by postprocessing programs.
  //  The mmdisp program ignores any points with magnitude smaller than
  // valuerangeminmag, and uses valuerangemaxmag to scale inputs for display.
  // The use of these values is postprocessor dependent, and should be
  // interactively overrideable, but typically ValueRangeMaxMag should be
  // the maximum vector magnitude in the file data, and ValueRangeMinMag
  // should be a lower bound on non-zero file vector magnitudes.

  // Additional variables for OVF version 2.0 files
  OC_INT4 valuedim;   // Dimension of values
  Nb_List< Nb_DString > valueunits_list;  // Value units.  The length
  /// of this list should either be valuedim, in which case each
  /// element represents the units for the corresponding dimension
  /// index, or otherwise the length should be one, in which case the
  /// one label applies to all indexes.
  Nb_List< Nb_DString > valuelabels_list; // Column labels.  The length
  /// of this list should be exactly valuedim.  There is no corresponding
  /// feature in the 1.0 format, so this field is processed but not used.

  // OC_BOOLEANS
  // The following boolean values indicate whether each of the above values
  // have been set (1=>set, 0=>not set).
  OC_BOOL title_bool,desc_bool;
  OC_BOOL meshunit_bool,xbase_bool,ybase_bool,zbase_bool;
  OC_BOOL xstepsize_bool,ystepsize_bool,zstepsize_bool;
  OC_BOOL xnodes_bool,ynodes_bool,znodes_bool;
  OC_BOOL xmin_bool,ymin_bool,zmin_bool;
  OC_BOOL xmax_bool,ymax_bool,zmax_bool;
  OC_BOOL valueunit_bool,valuemultiplier_bool;
  OC_BOOL valuerangemaxmag_bool,valuerangeminmag_bool;
  OC_BOOL pointcount_bool,bdryline_bool;
  OC_BOOL valuedim_bool, valueunits_list_bool, valuelabels_list_bool;
  OC_BOOL meshtype_bool,fileversion_bool;

  // Utility routines
  Vf_OvfSegmentHeader();
  ~Vf_OvfSegmentHeader();
  void SetBools(OC_BOOL newval);  // Sets all OC_BOOL's to newval
  OC_BOOL ValidSet() const; // Returns 1 iff a complete and valid
  /// collection of the OC_BOOL's are 1.
  void SetFieldValue(const Nb_DString &field,const Nb_DString &value);
};

//////////////////////////////////////////////////////////////////////
// Vf_OvfFileInput
//
class Vf_OvfFileInput : public Vf_FileInput
{
private:
  static const ClassDoc class_doc;
  static const char *file_type;
  static const Vf_FileInputID id;

  // File read helper routines
  OC_INT4m ReadLine(Nb_DString &buf);
  OC_INT4m ParseContentLine(const Nb_DString &buf,
			 Nb_DString &field,Nb_DString &value);
  OC_INT4m ReadTextVec3f(int Nin,Nb_Vec3<OC_REAL8> &data);
  OC_INT4m ReadVec3f(Vf_OvfDataStyle datastyle,int Nin,
		  Nb_Vec3<OC_REAL8> &data,
		  OC_REAL8m* badvalue=NULL);
  OC_INT4m ReadVec3fFromLSB(Vf_OvfDataStyle datastyle,int Nin,
                         Nb_Vec3<OC_REAL8> &data,
                         OC_REAL8m* badvalue=NULL);

  // Internal constructor
  Vf_OvfFileInput(const char *new_filename,
	       FILE *new_infile,
	       Tcl_Channel new_channel,
	       const char *new_channelName);

  // The next 2 functions & the following "Default constructor"
  // are left undefined on purpose
  Vf_OvfFileInput(const Vf_OvfFileInput &copyobj);  // Copy constructor
  Vf_OvfFileInput& operator=(const Vf_OvfFileInput &rhs); // Assignment op

public:
  Vf_OvfFileInput();  // Default constructor
  // Once upon a time (sometime before Aug-1997), I had trouble
  // with the SGI CC compiler requiring at least 1 public
  // constructor, so I put the dummy "Vf_OvfFileInput();" constructor
  // declaration here (but with no definition).  This restriction
  // appears to have gone away with the newer IRIX 6.2 CC compiler,
  // but I'll leave this note here just in case.  Of course, actual
  // code is required to call "MakeNew" instead of a constructor.

  static Vf_FileInput* MakeNew(const char *new_filename,
				       FILE *new_infile,
				       Tcl_Channel new_channel,
				       const char *new_channelName);
  const char *FileType(void) { return file_type; }
  static OC_BOOL IsType(const char *filename,
		     FILE *input_file,
		     Tcl_Channel channel,
		     const char *channelName);
  Vf_Mesh *NewMesh(void);
};

//////////////////////////////////////////////////////////////////////
// Vf_OvfFileOutput
//
class Vf_OvfFileOutput
{
private:
  static const ClassDoc class_doc;
public:
  Vf_OvfFileOutput() {}
  OC_INT4m WriteMesh(Vf_Mesh *mesh,Tcl_Channel channel,
                  Vf_OvfDataStyle datastyle,
		  OC_BOOL force_irreg=0,
		  const char *title=NULL,const char *desc=NULL);
  // Returns -1 if mesh type not supported,
  //          0 on success,
  //         >0 on other error.
  // If force_irreg is set, the output will be in the Omf irregular
  // grid file format (location & value) even if input data lies
  // on a rectangular grid.
  ~Vf_OvfFileOutput() {}
};

//////////////////////////////////////////////////////////////////////
// Vf_VioFileOutput
//
class Vf_VioFileOutput
{
private:
  static const ClassDoc class_doc;
public:
  Vf_VioFileOutput() {}
  OC_INT4m WriteMesh(Vf_Mesh *mesh,Tcl_Channel channel,const char *desc=NULL);
  // Returns -1 if mesh type not supported,
  //          0 on success,
  //         >0 on other error.
  ~Vf_VioFileOutput() {}
};

//////////////////////////////////////////////////////////////////////
// Vf_VioFileInput
//
class Vf_VioFileInput : public Vf_FileInput
{
private:
  static const ClassDoc class_doc;
  static const char *file_type;
  static const Vf_FileInputID id;

  // The following 3 functions are left undefined on purpose
  Vf_VioFileInput(); // Default constructor
  Vf_VioFileInput(const Vf_VioFileInput &copyobj);  // Copy constructor
  Vf_VioFileInput& operator=(const Vf_VioFileInput &rhs); // Assignment op

public:
  // Internal constructor
  Vf_VioFileInput(const char *new_filename,
	       FILE *new_infile,
	       Tcl_Channel new_channel,
	       const char *new_channelName);

  // Standard constructor
  static Vf_FileInput* MakeNew(const char *new_filename,
				       FILE *new_infile,
				       Tcl_Channel new_channel,
				       const char *new_channelName);
  const char *FileType(void) { return file_type; }
  static OC_BOOL IsType(const char *filename,
		     FILE *input_file,
		     Tcl_Channel channel,
		     const char *channelName);
  Vf_Mesh *NewMesh(void); // Returns *Vf_EmptyMesh on error
  Vf_GridVec3f *NewRectGrid(void);  // Return NULL on error
};

//////////////////////////////////////////////////////////////////////
// Standard problem (or Simple) Vector Format (SVF)
//

Tcl_CmdProc Vf_SvfFileInputTA;

class Vf_SvfFileInput : public Vf_FileInput
{
private:
  static const ClassDoc class_doc;
  static const char *file_type;
  static const Vf_FileInputID id;
  friend int Vf_SvfFileInputTA(ClientData,Tcl_Interp *interp,
                               int argc,CONST84 char** argv);

  // Instance data members
  Vf_GeneralMesh3f *mesh;

  // The following 3 functions are left undefined on purpose
  Vf_SvfFileInput(); // Default constructor
  Vf_SvfFileInput(const Vf_SvfFileInput &copyobj);  // Copy constructor
  Vf_SvfFileInput& operator=(const Vf_SvfFileInput &rhs); // Assignment op

  // Internal constructor
  Vf_SvfFileInput(const char *new_filename,
	       FILE *new_infile,
	       Tcl_Channel new_channel,
	       const char *new_channelName);

public:

  // Standard constructor
  static Vf_FileInput* MakeNew(const char *new_filename,
				       FILE *new_infile,
				       Tcl_Channel new_channel,
				       const char *new_channelName);
  const char *FileType(void) { return file_type; }
  static OC_BOOL IsType(const char *filename,
		     FILE *input_file,
		     Tcl_Channel channel,
		     const char *channelName);
  Vf_Mesh *NewMesh(void); // Returns *Vf_EmptyMesh on error
  Vf_GridVec3f *NewRectGrid(void) { return NULL; } // Return NULL on error
  ~Vf_SvfFileInput();
};

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// Classes for OVF 2.0

enum Vf_Ovf20_MeshType { vf_ovf20mesh_invalid,
                         vf_ovf20mesh_rectangular,
                         vf_ovf20mesh_irregular  };


struct Vf_Ovf20VecArray {
  // Vf_Ovf20VecArray is an adapter class.  The presumption is that data
  // loaded to or from disk will be stored in memory in some managed
  // structure.  Rather than requiring some particular form for that
  // structure, we assume that at the lowest level that structure consists
  // of an array of OC_REAL8m's, in a particular order, namely, from
  // fastest to slowest changing index: vector element, x offset, y
  // offset, z offset.  Use the Vf_Ovf20VecArray to tell the OVF 2.0 file
  // read/write routines the location of that OC_REAL8m array, its vector
  // dimension, and its total length.
  //   Note that Vf_Ovf20VecArray does not assume ownership of the
  // OC_REAL8m array. IOW, it is the resposibility of the client routine
  // to allocate and free the data.
public:
  OC_INDEX vector_dimension;  // Dimension of the data (usually 1 or 3)
  OC_INDEX array_length;      // Length of the array
  OC_REAL8m* data; // storage for vector_dimension*array_length REAL8m values
  Vf_Ovf20VecArray()
    : vector_dimension(0), array_length(0), data(0) {}
  Vf_Ovf20VecArray(OC_INDEX vdim,OC_INDEX alen,OC_REAL8m* dptr)
    : vector_dimension(vdim), array_length(alen), data(dptr) {}
};

struct Vf_Ovf20VecArrayConst {
  // Same as Vf_Ovf20VecArray struct, except data array is
  // const.  This version should be used for write operations,
  // the other for reads.
public:
  OC_INDEX vector_dimension;
  OC_INDEX array_length;
  const OC_REAL8m* data;
  Vf_Ovf20VecArrayConst()
    : vector_dimension(0), array_length(0), data(0) {}
  Vf_Ovf20VecArrayConst(OC_INDEX vdim,OC_INDEX alen,const OC_REAL8m* dptr)
    : vector_dimension(vdim), array_length(alen), data(dptr) {}
};

class Vf_Ovf20FieldWrap {
protected:
  OC_BOOL isset;
  String fieldname;
public:

  // Default constructor, copy constructor, assignment operator;
  // Declare but don't define, to prevent accidental use.
  Vf_Ovf20FieldWrap();
  Vf_Ovf20FieldWrap(const Vf_Ovf20FieldWrap&);
  Vf_Ovf20FieldWrap& operator=(const Vf_Ovf20FieldWrap&);

  // Use only this constructor
  Vf_Ovf20FieldWrap(vector<Vf_Ovf20FieldWrap*>& flist,const char* fname)
    : isset(0), fieldname(fname) {
    flist.push_back(this);
  }

  // Virtual destructor; this is an abstract class
  virtual ~Vf_Ovf20FieldWrap() {}

  void Clear() { isset = 0; }
  OC_BOOL IsSet() const { return isset; }

  const char* GetName() const { return fieldname.c_str(); }
  const String& GetNameString() const { return fieldname; }

  // Set value from string
  virtual void SetFromString(const char* cptr) =0;

  // Convert value to string
  virtual void WriteToString(String& str) const =0;
};

// Concrete child classes
template<class T> class Vf_Ovf20FWT : public Vf_Ovf20FieldWrap {
private:
  T    value;
public:

  Vf_Ovf20FWT(vector<Vf_Ovf20FieldWrap*>& flist,const char* fname)
    : Vf_Ovf20FieldWrap(flist,fname) {}

  void Set(const T& new_value) {
    value = new_value;
    isset = 1;
  }

  const T& Value() const { return value; }

  virtual void SetFromString(const char* cptr) {
    value = cptr;
    isset = 1;
  }
  virtual void WriteToString(String& str) const {
    str = value;
  }

};

// Specializations /////////////////////////////////////////////////////
//
// For the most part, we only need to specialize the SetFromString and
// WriteToString functions in Vf_Ovf20FWT.  Most current (Aug 2008)
// compilers allow specialization of template member functions without
// specializing the full class, and this is how this code was first
// implemented.  However, some older compilers do not grok that, and
// even the ones that do are sometimes a bit finicky around the
// corners, so given that the rest of the Vf_Ovf20FWT class is pretty
// trivial, we just go ahead and do full class specializations.

template<> class Vf_Ovf20FWT<OC_REAL8> : public Vf_Ovf20FieldWrap {
private:
  OC_REAL8    value;
public:

  Vf_Ovf20FWT(vector<Vf_Ovf20FieldWrap*>& flist,const char* fname)
    : Vf_Ovf20FieldWrap(flist,fname), value(0.0) {}

  void Set(const OC_REAL8& new_value) {
    value = new_value;
    isset = 1;
  }

  const OC_REAL8& Value() const { return value; }

  virtual void SetFromString(const char* cptr) {
    value = Nb_Atof(cptr);
    isset = 1;
  }
  virtual void WriteToString(String& str) const {
    char buf[64];
    Oc_Snprintf(buf,sizeof(buf),"%.17g",value);
    str = buf;
  }
};

template<> class Vf_Ovf20FWT<OC_INT4> : public Vf_Ovf20FieldWrap {
private:
  OC_INT4    value;
public:

  Vf_Ovf20FWT(vector<Vf_Ovf20FieldWrap*>& flist,const char* fname)
    : Vf_Ovf20FieldWrap(flist,fname), value(0) {}

  void Set(const OC_INT4& new_value) {
    value = new_value;
    isset = 1;
  }

  const OC_INT4& Value() const { return value; }

  virtual void SetFromString(const char* cptr) {
    value = atoi(cptr);
    isset = 1;
  }
  virtual void WriteToString(String& str) const {
    char buf[64];
    Oc_Snprintf(buf,sizeof(buf),"%d",value);
    str = buf;
  }

};

#if (OC_INDEX_WIDTH != 4)
// OC_INDEX is a different type than OC_INT4
template<> class Vf_Ovf20FWT<OC_INDEX> : public Vf_Ovf20FieldWrap {
private:
  OC_INDEX    value;
public:

  Vf_Ovf20FWT(vector<Vf_Ovf20FieldWrap*>& flist,const char* fname)
    : Vf_Ovf20FieldWrap(flist,fname), value(0) {}

  void Set(const OC_INDEX& new_value) {
    value = new_value;
    isset = 1;
  }

  const OC_INDEX& Value() const { return value; }

  virtual void SetFromString(const char* cptr) {
    value = OC_INDEX(strtol(cptr,NULL,10));
    isset = 1;
  }
  virtual void WriteToString(String& str) const {
    char buf[64];
    Oc_Snprintf(buf,sizeof(buf),"%d",value);
    str = buf;
  }

};
#endif // OC_INDEX_WIDTH

template<> class Vf_Ovf20FWT<Vf_Ovf20_MeshType> : public Vf_Ovf20FieldWrap {
private:
  Vf_Ovf20_MeshType    value;
public:

  Vf_Ovf20FWT(vector<Vf_Ovf20FieldWrap*>& flist,const char* fname)
    : Vf_Ovf20FieldWrap(flist,fname), value(vf_ovf20mesh_invalid) {}

  void Set(const Vf_Ovf20_MeshType& new_value) {
    value = new_value;
    isset = 1;
  }

  const Vf_Ovf20_MeshType& Value() const { return value; }

  virtual void SetFromString(const char* cptr) {
    String str = cptr;
    for(String::iterator it = str.begin(); it!= str.end(); ++it) {
      *it = static_cast<char>(tolower(*it));
    }
    if(str.compare("rectangular")==0) {
      value = vf_ovf20mesh_rectangular;
    } else if(str.compare("irregular")==0) {
      value = vf_ovf20mesh_irregular;
    } else {
      value = vf_ovf20mesh_invalid;
    }
    isset = 1;
  }

  virtual void WriteToString(String& str) const {
    switch(value) {
    case vf_ovf20mesh_rectangular:
      str = "rectangular"; break;
    case vf_ovf20mesh_irregular:
      str = "irregular"; break;
    default:
      str = "invalid"; break;
    }
  }

};


template<> class Vf_Ovf20FWT< vector<String> > : public Vf_Ovf20FieldWrap {
private:
  vector<String>    value;
public:

  Vf_Ovf20FWT(vector<Vf_Ovf20FieldWrap*>& flist,const char* fname)
    : Vf_Ovf20FieldWrap(flist,fname) {}

  void Set(const vector<String>& new_value) {
    value = new_value;
    isset = 1;
  }

  const vector<String>& Value() const { return value; }

  virtual void SetFromString(const char* cptr) {
    value.clear();

    int argc;
    CONST84 char ** argv;
    if(Tcl_SplitList(NULL,Oc_AutoBuf(cptr),&argc,&argv)!=TCL_OK) {
      char buf[4096];
      Oc_Snprintf(buf,sizeof(buf),
                  "Ovf 2.0 header input string \"%.3500s\" "
                  "is not a valid Tcl list.",cptr);
      throw buf;
    }
    for(int i=0;i<argc;++i) {
      value.push_back(argv[i]);
    }

    Tcl_Free((char *)argv);

    isset = 1;
  }

  virtual void WriteToString(String& str) const {
    str = Nb_MergeList(value);
  }

};

// Specializations (end) ///////////////////////////////////////////////

class Vf_Ovf20FileHeader;  // Forward decl, needed for Vf_Ovf20_MeshNodes

// Mesh interface spec class
class Vf_Ovf20_MeshNodes {
public:
  // Tell whether or not a particular Vf_Ovf20_MeshType output
  // is supported.
  virtual OC_BOOL SupportsOutputType(Vf_Ovf20_MeshType type) const =0;
  virtual Vf_Ovf20_MeshType NaturalType() const =0;

  virtual OC_INDEX Size() const =0;  // Number of basis elements

  // Returns position of the center of the cell (mesh element) for a
  // given index.  This abstract class (interface) is used for file
  // writes by the Vf_OVF20FileHeader class.  Any object that wants to
  // write through Vf_OVF20FileHeader should inherit from this class.
  virtual void GetCellCenter(OC_INDEX index,
                             OC_REAL8m& x,OC_REAL8m& y, OC_REAL8m& z) const =0;


  // Routine to dump mesh geometry data into Ovf20FileHeader.
  // For irregular grids this routine should fill
  //    meshtype
  //    meshunit
  //    xmin, ymin, zmin
  //    xmax, ymax, zmax
  //    pointcount
  // and may optionally fill
  //    xstepsize, ystepsize, zstepsize.
  //
  // For regular, rectangular grids, this routine should fill
  //    meshtype
  //    meshunit
  //    xmin, ymin, zmin
  //    xmax, ymax, zmax
  //    xbase, ybase, zbase
  //    xnodes, ynodes, znodes
  //    xstepsize, ystepsize, zstepsize.
  //
  // The header routine IsValidGeom() should be called to check
  // that all needed fields are filled.
  //
  virtual void DumpGeometry(Vf_Ovf20FileHeader& header,
                            Vf_Ovf20_MeshType type) const =0;

  virtual ~Vf_Ovf20_MeshNodes() {}
};

class Vf_Ovf20FileHeader {
private:
  vector<Vf_Ovf20FieldWrap*> field_list;

  void CollapseHeaderLine(char* line) const;
  // Removes spaces converts to lowercase

  int ParseHeaderLine(char* line,String& field,String& value) const;
  // Returns 0 on success, 1 on error.  If the line is a comment or
  // empty line, then field.size() will be 0.  "field" value is
  // converted to lowercase, and all non-alphanumeric characters are
  // removed.  Value string has outer whitespace removed.  As a
  // convenience, if field is "begin" or "end", then the value string
  // is converted to lowercase.
  // Note: import "line" may be modified by this routine.


  static void ReadTextFloat(Tcl_Channel inchan,OC_REAL8m& value,
                            unsigned char& nextbyte);
  // Reads one floating point value as text from inchan.  Returns the
  // read value in the export val, and returns the first succeeding byte
  // in nextbyte.  Leading whitespace and comments are silently skipped.
  // This routine is based on the older Vf_OvfFileInput::ReadTextVec3f
  // code.  An exception is thrown if a floating point value cannot be
  // read.

public:

  Vf_Ovf20FileHeader() :
    ovfversion(vf_ovf_latest),
    title(field_list,"title"),
    desc(field_list,"desc"),
    meshunit(field_list,"meshunit"),
    meshtype(field_list,"meshtype"),

    xbase(field_list,"xbase"),
    ybase(field_list,"ybase"),
    zbase(field_list,"zbase"),

    xnodes(field_list,"xnodes"),
    ynodes(field_list,"ynodes"),
    znodes(field_list,"znodes"),

    pointcount(field_list,"pointcount"),

    xstepsize(field_list,"xstepsize"),
    ystepsize(field_list,"ystepsize"),
    zstepsize(field_list,"zstepsize"),

    xmin(field_list,"xmin"),
    ymin(field_list,"ymin"),
    zmin(field_list,"zmin"),
    xmax(field_list,"xmax"),
    ymax(field_list,"ymax"),
    zmax(field_list,"zmax"),

    valuedim(field_list,"valuedim"),
    valuelabels(field_list,"valuelabels"),
    valueunits(field_list,"valueunits") {}

  // I/O routines //////////////////////////////////////////////////////

  OC_BOOL IsValid() const; // Returns 1 if header is valid, 0 otherwise.
  OC_BOOL IsValid(String& errmsg) const;

  OC_BOOL IsValidGeom() const; // Returns 1 if geometry portion of header
  /// is valid, 0 otherwise.

  void ReadHeader(Tcl_Channel inchan);
  void ReadData(Tcl_Channel inchan,
                const vector<Vf_Ovf20VecArray>& data_array,
                const Vf_Ovf20VecArray* node_locations=0) const;
  // Interleaves data from a channel into a collection of OC_REAL8m
  // arrays, as indicated by the data_array parameter.  The data_array
  // elements must have their data set sized to compatible values before
  // calling --- call ReadHeader first to get the necessary info.
  // Parameter node_locations must be non-null for irregular grids, but
  // may be either null or non-null for rectangular grids.
  void ReadData(Tcl_Channel inchan,const Vf_Ovf20VecArray& data_info,
                const Vf_Ovf20VecArray* node_locations=0) const;
  // Special case version of ReadData, where the destination buffer
  // is a single array rather than a collection of several.

  // In both WriteHeader and WriteData, if version is not specified
  // then the default version is chosen; the intent is for the default
  // version to track the latest available.
  void WriteHeader(Tcl_Channel outchan) const;

  void WriteData(Tcl_Channel outchan,
                 Vf_OvfDataStyle datastyle,
                 const char* textfmt, // Active iff datastyle==vf_oascii
                 const Vf_Ovf20_MeshNodes* mesh,
                 const Vf_Ovf20VecArrayConst& data_info,
                 OC_BOOL bare=0) const;
  // The mesh import is only needed for writing irregular grids.  For
  // rectangular grids this may be set to NULL.

  void WriteData(Tcl_Channel outchan,
                 Vf_OvfDataStyle datastyle,
                 const char* textfmt, // Active iff datastyle==vf_oascii
                 const Vf_Ovf20_MeshNodes* mesh,
                 const vector<Vf_Ovf20VecArrayConst>& data_array,
                 OC_BOOL bare=0) const;
  // Same as preceding WriteData member, except that rather than
  // a single dumping out of a single OC_REAL8m*, a collection of
  // OC_REAL8m pointed to by Vf_Ovf20VecArray elements are interleaved
  // on output.  Note: The array_length field for each element of
  // the data_array must be the same, and must match either the header
  // pointcount (for irregular grids) or xnodes*ynodes*znodes (for
  // regular grids).  Also, the sum of the vector_dimension fields
  // must equal the header valuedim value.

  // Routines for writing Python Numpy NPY version 1.0 output (Export support)
  void WriteNPY(Tcl_Channel outchan,
                Vf_OvfDataStyle datastyle,
                const Vf_Ovf20VecArrayConst& data_info,
                const Vf_Ovf20_MeshNodes* mesh=nullptr, // For irreg grids only
                const int textwidth=24, // Only for datastyle == vf_oascii
                const char* textfmt="%24.16e" // Only for datastyle == vf_oascii
                ) const;

  void WriteNPY(Tcl_Channel outchan,
                Vf_OvfDataStyle datastyle,
                const vector<Vf_Ovf20VecArrayConst>& data_array,
                const Vf_Ovf20_MeshNodes* mesh=nullptr, // For irreg grids only
                const int textwidth=24, // Only for datastyle == vf_oascii
                const char* textfmt="%24.16e" // Only for datastyle == vf_oascii
                ) const;


  // Header data ///////////////////////////////////////////////////////

  // Required for all files
  Vf_OvfFileVersion ovfversion;  // Defaults to latest
  Vf_Ovf20FWT<String> title;

  // Optional for all files
  Vf_Ovf20FWT<String> desc;

  Vf_Ovf20FWT<String> meshunit;

  Vf_Ovf20FWT<Vf_Ovf20_MeshType> meshtype;

  // Required for rectangular grids.  Not allowed for irregular grids.
  Vf_Ovf20FWT<OC_REAL8> xbase;
  Vf_Ovf20FWT<OC_REAL8> ybase;
  Vf_Ovf20FWT<OC_REAL8> zbase;
  Vf_Ovf20FWT<OC_INDEX> xnodes;
  Vf_Ovf20FWT<OC_INDEX> ynodes;
  Vf_Ovf20FWT<OC_INDEX> znodes;

  // Required for irregular grids.  Not allowed for regular grids.
  Vf_Ovf20FWT<OC_INDEX> pointcount;

  // Required for rectangular grids, optional for irregular grids.
  Vf_Ovf20FWT<OC_REAL8> xstepsize;
  Vf_Ovf20FWT<OC_REAL8> ystepsize;
  Vf_Ovf20FWT<OC_REAL8> zstepsize;

  Vf_Ovf20FWT<OC_REAL8> xmin;
  Vf_Ovf20FWT<OC_REAL8> ymin;
  Vf_Ovf20FWT<OC_REAL8> zmin;
  Vf_Ovf20FWT<OC_REAL8> xmax;
  Vf_Ovf20FWT<OC_REAL8> ymax;
  Vf_Ovf20FWT<OC_REAL8> zmax;

  Vf_Ovf20FWT<OC_INT4>  valuedim;
  Vf_Ovf20FWT< vector<String> > valuelabels;
  Vf_Ovf20FWT< vector<String> > valueunits;
};

////////////////////////////////////////////////////////////////////////
//
// Vf_Ovf20_MeshNodes wrapper for Vf_Mesh.
//
// NOTE: The "valueunits" and "valuelabels" fields of the Ovf 2.0 format
//       aren't completely supported by the Vf_Mesh, and anyway client
//       code may wish to use Vf_Mesh_MeshNodes to set only the geometry
//       in an output Ovf 2.0 file, and handle the data values (of
//       whatever dimension) by itself.  Therefore Vf_Mesh_MeshNodes
//       does not set the valueunits or valuelabels fields in the
//       DumpGeometry call.
//
class Vf_Mesh_MeshNodes : public Vf_Ovf20_MeshNodes {
private:
  Vf_Ovf20_MeshType type;

  // Header data
  String meshunit;
  Nb_BoundingBox<OC_REAL8> meshrange;

  OC_INDEX size;
  Nb_ArrayWrapper<Nb_Vec3<OC_REAL8m> > values;

  // For irregular meshes
  Nb_ArrayWrapper<Nb_Vec3<OC_REAL8m> > centers;

  // For rectangular meshes
  Nb_Vec3<OC_INDEX>  dimens;
  Nb_Vec3<OC_REAL8m> basept;

  // Used for both irregular and rectangular meshes
  Nb_Vec3<OC_REAL8m> cellsize;

  // Disable the following by declaring but not defining
  Vf_Mesh_MeshNodes& operator=(const Vf_Mesh_MeshNodes&);
  Vf_Mesh_MeshNodes(const Vf_Mesh_MeshNodes&);

public:

  Vf_Mesh_MeshNodes(const Vf_Mesh* mesh);
  ~Vf_Mesh_MeshNodes() {}

  OC_BOOL SupportsOutputType(Vf_Ovf20_MeshType testtype) const {
    if(testtype == type) return 1;
    if(testtype == vf_ovf20mesh_irregular) return 1;
    return 0;
  }

  Vf_Ovf20_MeshType NaturalType() const { return type; }

  OC_INDEX Size() const { return size; }

  void GetCellCenter(OC_INDEX index,
                     OC_REAL8m& x,OC_REAL8m& y, OC_REAL8m& z) const {
    if(type != vf_ovf20mesh_rectangular) {
      x = centers[index].x;
      y = centers[index].y;
      z = centers[index].z;
      return;
    }
    OC_INDEX k = index/(dimens.x*dimens.y); index -= k*dimens.x*dimens.y;
    OC_INDEX j = index/dimens.x;            index -= j*dimens.x;
    OC_INDEX i = index;
    z = basept.z + k * cellsize.z;
    y = basept.y + j * cellsize.y;
    x = basept.x + i * cellsize.x;
  }

  void DumpGeometry(Vf_Ovf20FileHeader& header,
                    Vf_Ovf20_MeshType type) const;

  // Extensions
  OC_INDEX GetSize() const { return size; }
  const Nb_Vec3<OC_REAL8m>& GetValue(OC_INDEX index) const {
    return values[index];
  }
};

////////////////////////////////////////////////////////////////////////

#endif // _VF_VECFILE
