/* FILE: mesh.cc                 -*-Mode: c++-*-
 *
 * Abstract Oxs_Mesh class, derives from Oxs_Ext and Oxs_Lock classes.
 *
 */

#include <string>

#include "mesh.h"
#include "meshvalue.h"
#include "oxsexcept.h"
#include "vf.h"

/* End includes */


////////////////////////////////////////////////////////////////////////
// Oxs_Mesh (generic mesh) class

// Constructors
Oxs_Mesh::Oxs_Mesh
( const char* name,     // Child instance id
  Oxs_Director* newdtr  // App director
  ) : Oxs_Ext(name,newdtr)
{}

Oxs_Mesh::Oxs_Mesh
( const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr    // MIF block argument string
  ) : Oxs_Ext(name,newdtr,argstr)
{}


/////////////////////////////////////////////////////////////////
// Default vector field output using generic Oxs_Mesh interface.
// Throws exception on error
void Oxs_Mesh::WriteOvfIrregular
(Tcl_Channel channel,   // Output channel
 OC_BOOL headers,          // If false, then output only raw data
 const char* title,     // Long filename or title
 const char* desc,      // Description to embed in output file
 const char* valueunit, // Field units, such as "A/m".
 const char* datatype,  // Either "binary" or "text"
 const char* precision, // For binary, "4" or "8";
                        ///  for text, a printf-style format
 const Oxs_MeshValue<ThreeVector>* vec,  // Vector array
 const Oxs_MeshValue<OC_REAL8m>* scale,     // Optional scaling for vec
                    /// Set scale to NULL to use vec values directly.
 const ThreeVector* stephints // If non-null, then the values are
                             /// written as stephints to the file.
 ) const
{
  // Check import validity
  enum DataType { BINARY, TEXT };
  DataType dt = BINARY;
  if(strcmp("text",datatype)==0) {
    dt = TEXT;
  } else if(strcmp("binary",datatype)!=0){
    String errmsg = String("Bad datatype: \"") + String(datatype)
      + String("\"  Should be either \"binary\" or \"text\"");
    OXS_EXTTHROW(Oxs_BadParameter,errmsg.c_str(),-1);
  }

  int datawidth = 0;
  String dataformat;
  if(dt == BINARY) {
    datawidth = atoi(precision);
    if(datawidth != 4 && datawidth != 8) {
      String errmsg = String("Bad precision: ") + String(precision)
	+ String("  Should be either 4 or 8.");
      OXS_EXTTHROW(Oxs_BadParameter,errmsg.c_str(),-1);
    }
  } else {
    if(precision==NULL || precision[0]=='\0')
      precision="%# .17g";  // Default format
    String temp = String(precision) + String(" ");
    dataformat = temp + temp + temp + temp + temp +
      String(precision) + String("\n");
  }

  if(!vec->CheckMesh(this)) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),"Size mismatch; input data length=%u,"
		" which is different than mesh size=%u",
		vec->Size(),Size());
    OXS_EXTTHROW(Oxs_BadParameter,buf,-1);
  }

  if(scale!=NULL && !scale->CheckMesh(this)) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),"Size mismatch; input scale data length=%u,"
		" which is different than mesh size=%u",
		scale->Size(),Size());
    OXS_EXTTHROW(Oxs_BadParameter,buf,-1);
  }

  if(headers) {
    try {
      // Write header
      Nb_FprintfChannel(channel,NULL,1024,"# OOMMF: irregular mesh v1.0\n");
      Nb_FprintfChannel(channel,NULL,1024,"# Segment count: 1\n");
      Nb_FprintfChannel(channel,NULL,1024,"# Begin: Segment\n");
      Nb_FprintfChannel(channel,NULL,1024,"# Begin: Header\n");
      if(title==NULL || title[0]=='\0')
        Nb_FprintfChannel(channel,NULL,1024,"# Title: unknown\n");
      else
        Nb_FprintfChannel(channel,NULL,1024,"# Title: %s\n",title);
      if(desc!=NULL && desc[0]!='\0') {
        // Print out description, breaking at newlines as necessary.
        // Note: This block is optional
        const char *cptr1,*cptr2;
        cptr1=desc;
        while((cptr2=strchr(cptr1,'\n'))!=NULL) {
          Nb_FprintfChannel(channel,NULL,1024,
                            "# Desc: %.*s\n",(int)(cptr2-cptr1),cptr1);
          cptr1=cptr2+1;
        }
        if(*cptr1!='\0')
          Nb_FprintfChannel(channel,NULL,1024,"# Desc: %s\n",cptr1);
      }
      Nb_FprintfChannel(channel,NULL,1024,"# meshunit: m\n");
      Nb_FprintfChannel(channel,NULL,1024,"# valueunit: %s\n",valueunit);
      Nb_FprintfChannel(channel,NULL,1024,"# valuemultiplier: 1.0\n");

      Oxs_Box bbox;
      GetBoundingBox(bbox);
      Nb_FprintfChannel(channel,NULL,1024,
                        "# xmin: %.17g\n# ymin: %.17g\n# zmin: %.17g\n"
                        "# xmax: %.17g\n# ymax: %.17g\n# zmax: %.17g\n",
                        static_cast<double>(bbox.GetMinX()),
                        static_cast<double>(bbox.GetMinY()),
                        static_cast<double>(bbox.GetMinZ()),
                        static_cast<double>(bbox.GetMaxX()),
                        static_cast<double>(bbox.GetMaxY()),
                        static_cast<double>(bbox.GetMaxZ()));
      // As of 6/2001, mmDisp supports display of out-of-plane rotations;
      // Representing the boundary under these conditions is awkward with a
      // single polygon.  So don't write the boundary line, and rely on
      // defaults based on the data range and step size.

      OC_REAL8m minmag=0,maxmag=0;
      if(Size()>0) {
        minmag=DBL_MAX;
        for(OC_INDEX i=0;i<Size();i++) {
          OC_REAL8m val=(*vec)[i].MagSq();
          if(scale!=NULL) {
            OC_REAL8m tempscale=(*scale)[i];
            val*=tempscale*tempscale;
          }
          if(val<minmag && val>0) minmag=val; // minmag is smallest non-zero
          if(val>maxmag) maxmag=val;         /// magnitude.
        }
        if(minmag>maxmag) minmag=maxmag;
        maxmag=sqrt(maxmag);
        minmag=sqrt(minmag);
        minmag*=0.999;  // Underestimate lower bound by 0.1% to protect against
        /// rounding errors.
      }

      Nb_FprintfChannel(channel,NULL,1024,
                        "# ValueRangeMaxMag: %.17g\n",
                        static_cast<double>(maxmag));
      Nb_FprintfChannel(channel,NULL,1024,
                        "# ValueRangeMinMag: %.17g\n",
                        static_cast<double>(minmag));

      Nb_FprintfChannel(channel,NULL,1024,
                        "# meshtype: irregular\n");
      Nb_FprintfChannel(channel,NULL,1024,
                        "# pointcount: %lu\n",
                        static_cast<unsigned long>(Size()));

      // Write stephints, if requested
      if(stephints!=NULL) {
        Nb_FprintfChannel(channel,NULL,1024,
                          "# xstepsize: %.17g\n",
                          static_cast<double>(stephints->x));
        Nb_FprintfChannel(channel,NULL,1024,
                          "# ystepsize: %.17g\n",
                          static_cast<double>(stephints->y));
        Nb_FprintfChannel(channel,NULL,1024,
                          "# zstepsize: %.17g\n",
                          static_cast<double>(stephints->z));
      }


      Nb_FprintfChannel(channel,NULL,1024,"# End: Header\n");
    } catch(...) {
      OXS_EXTTHROW(Oxs_DeviceFull,
                   "Error writing OVF file header;"
                   " disk full or buffer overflow?",1);
    }
  }

  // Write data block
  try {
    if(dt == BINARY) {
      if(datawidth==4) {
        OC_REAL4 buf[6];
        if(headers) {
          Nb_FprintfChannel(channel,NULL,1024,"# Begin: Data Binary 4\n");
          buf[0]=1234567.;  // 4-Byte checkvalue
          if(Vf_OvfFileFormatSpecs::WriteBinary(channel,buf,1)) {
            throw "write error";
          }
        }
        const OC_INDEX size=Size();
        for(OC_INDEX i=0 ; i<size ; ++i) {
          ThreeVector loc;   Center(i,loc);
          buf[0] = static_cast<OC_REAL4>(loc.x);
          buf[1] = static_cast<OC_REAL4>(loc.y);
          buf[2] = static_cast<OC_REAL4>(loc.z);
          const ThreeVector& v = (*vec)[i];
          if(scale==NULL) {
            buf[3] = static_cast<OC_REAL4>(v.x);
            buf[4] = static_cast<OC_REAL4>(v.y);
            buf[5] = static_cast<OC_REAL4>(v.z);
          } else {
            OC_REAL8m tempscale=(*scale)[i];
            buf[3] = static_cast<OC_REAL4>(tempscale*v.x);
            buf[4] = static_cast<OC_REAL4>(tempscale*v.y);
            buf[5] = static_cast<OC_REAL4>(tempscale*v.z);
          }
          if(Vf_OvfFileFormatSpecs::WriteBinary(channel,buf,6)) {
            throw "write error";
          }
          // Vf_OvfFileFormatSpecs::WriteBinary performs
          // byte-swapping as needed.
        }
        if(headers) {
          Nb_FprintfChannel(channel,NULL,1024,"\n# End: Data Binary 4\n");
        }
      } else {
        OC_REAL8 buf[6];
        if(headers) {
          Nb_FprintfChannel(channel,NULL,1024,"# Begin: Data Binary 8\n");
          buf[0]=123456789012345.;  // 8-Byte checkvalue
          if(Vf_OvfFileFormatSpecs::WriteBinary(channel,buf,1)) {
            throw "write error";
          }
        }
        const OC_INDEX size=Size();
        for(OC_INDEX i=0 ; i<size ; ++i) {
          ThreeVector loc;   Center(i,loc);
          buf[0] = static_cast<OC_REAL8>(loc.x);
          buf[1] = static_cast<OC_REAL8>(loc.y);
          buf[2] = static_cast<OC_REAL8>(loc.z);
          const ThreeVector& v = (*vec)[i];
          if(scale==NULL) {
            buf[3] = static_cast<OC_REAL8>(v.x);
            buf[4] = static_cast<OC_REAL8>(v.y);
            buf[5] = static_cast<OC_REAL8>(v.z);
          } else {
            OC_REAL8m tempscale=(*scale)[i];
            buf[3] = static_cast<OC_REAL8>(tempscale*v.x);
            buf[4] = static_cast<OC_REAL8>(tempscale*v.y);
            buf[5] = static_cast<OC_REAL8>(tempscale*v.z);
          }
          if(Vf_OvfFileFormatSpecs::WriteBinary(channel,buf,6)) {
            throw "write error";
          }
          // Vf_OvfFileFormatSpecs::WriteBinary performs
          // byte-swapping as needed.
        }
        if(headers) {
          Nb_FprintfChannel(channel,NULL,1024,"\n# End: Data Binary 8\n");
        }
      }
    } else {
      if(headers) {
        Nb_FprintfChannel(channel,NULL,1024,"# Begin: Data Text\n");
      }
      const OC_INDEX size=Size();
      for(OC_INDEX i=0 ; i<size ; ++i) {
	ThreeVector loc;   Center(i,loc);
	const ThreeVector& v = (*vec)[i];
	if(scale==NULL) {
	  Nb_FprintfChannel(channel,NULL,1024,dataformat.c_str(),
                            static_cast<double>(loc.x),
                            static_cast<double>(loc.y),
                            static_cast<double>(loc.z),
                            static_cast<double>(v.x),
                            static_cast<double>(v.y),
                            static_cast<double>(v.z));
	} else {
	  OC_REAL8m tempscale=(*scale)[i];
	  Nb_FprintfChannel(channel,NULL,1024,dataformat.c_str(),
                            static_cast<double>(loc.x),
                            static_cast<double>(loc.y),
                            static_cast<double>(loc.z),
                            static_cast<double>(tempscale*v.x),
                            static_cast<double>(tempscale*v.y),
                            static_cast<double>(tempscale*v.z));
	}
      }
      if(headers) {
        Nb_FprintfChannel(channel,NULL,1024,"# End: Data Text\n");
      }
    }

    if(headers) {
      Nb_FprintfChannel(channel,NULL,1024,"# End: Segment\n");
    }
  } catch(...) {
    OXS_EXTTHROW(Oxs_DeviceFull,
                 "Error writing OVF file data block;"
                 " disk full or buffer overflow?",-1);
  }
}

// Version of WriteOvf that takes a filename instead of a Tcl_Channel.
// This is for backwards compatibility and convenience.
void Oxs_Mesh::WriteOvfFile
(const char* filename,  // Name of output file
 OC_BOOL headers,          // If false, then output only raw data
 const char* title,     // Long filename or title
 const char* desc,      // Description to embed in output file
 const char* valueunit, // Field units, such as "A/m".
 const char* meshtype,  // Either "rectangular" or "irregular"
 const char* datatype,  // Either "binary" or "text"
 const char* precision, // For binary, "4" or "8";
                        ///  for text, a printf-style format
 const Oxs_MeshValue<ThreeVector>* vec,  // Vector array
 const Oxs_MeshValue<OC_REAL8m>* scale      // Optional scaling for vec
 /// Set scale to NULL to use vec values directly.
 ) const
{
  if(filename==NULL) {
    // Bad input
    String msg=String("Failure in object ")
      + String(InstanceName())
      + String("; filename not specified.");
    throw Oxs_ExtError(msg.c_str());
  }

  int orig_errno=Tcl_GetErrno();
  Tcl_SetErrno(0);
  Tcl_Channel channel = Tcl_OpenFileChannel(NULL,
	const_cast<char*>(filename),
	const_cast<char*>("w"),0666);
  /// We'll *ASSUME* Tcl isn't really going to modify filename/mode.
  int new_errno=Tcl_GetErrno();
  Tcl_SetErrno(orig_errno);

  if(channel==NULL) {
    // File open failure
    String msg=String("Failure in object ")
      + String(InstanceName())
      + String(" to open file ") + String(filename);
    if(new_errno!=0)
      msg += String(" for writing: ") + String(Tcl_ErrnoMsg(new_errno));
    else
      msg += String(" for writing.");
    throw Oxs_ExtError(msg.c_str());
  }

  try {
    // Set channel options
    Tcl_SetChannelOption(NULL,channel,OC_CONST84_CHAR("-buffering"),
			 OC_CONST84_CHAR("full"));
    Tcl_SetChannelOption(NULL,channel,OC_CONST84_CHAR("-buffersize"),
			 OC_CONST84_CHAR("100000"));
    /// What's a good size???
    if(strcmp("text",datatype)!=0) {
      // Binary mode
      Tcl_SetChannelOption(NULL,channel,OC_CONST84_CHAR("-encoding"),
			   OC_CONST84_CHAR("binary"));
      Tcl_SetChannelOption(NULL,channel,OC_CONST84_CHAR("-translation"),
			   OC_CONST84_CHAR("binary"));
    }

    if(title==NULL || title[0]=='\0') {
      title=filename;
    }
    WriteOvf(channel,headers,title,desc,valueunit,
	     meshtype,datatype,precision,vec,scale);
  } catch (...) {
    Tcl_Close(NULL,channel);
    Nb_Remove(filename); // Delete possibly corrupt or
                        /// empty file on error.
    throw;
  }
  Tcl_Close(NULL,channel);
}

/////////////////////////////////////////////////////////////////
// Helper function for Vf_Ovf20_MeshNodes interface function
// DumpGeometry.
void Oxs_Mesh::DumpIrregGeometry(Vf_Ovf20FileHeader& header) const
{
  header.meshtype.Set(vf_ovf20mesh_irregular);
  header.meshunit.Set(String("m"));

  Oxs_Box bbox;
  GetBoundingBox(bbox);
  header.xmin.Set(bbox.GetMinX());
  header.ymin.Set(bbox.GetMinY());
  header.zmin.Set(bbox.GetMinZ());
  header.xmax.Set(bbox.GetMaxX());
  header.ymax.Set(bbox.GetMaxY());
  header.zmax.Set(bbox.GetMaxZ());

  header.pointcount.Set(Size());

  if(!header.IsValidGeom()) {
    throw Oxs_ExtError(this,"Invalid header");
  }
}

////////////////////////////////////////////////////////////////////
// Default geometry string from generic Oxs_Mesh interface.
String Oxs_Mesh::GetGeometryString() const {
  char buf[1024];
  Oc_Snprintf(buf,sizeof(buf),"%ld cells",(long)Size());
  return String(buf);
}

////////////////////////////////////////////////////////////////////
// Default volume summing routines using generic Oxs_Mesh interface.
OC_REAL8m
Oxs_Mesh::VolumeSum(const Oxs_MeshValue<OC_REAL8m>& scalar) const
{
  if(!scalar.CheckMesh(this)) {
    throw Oxs_ExtError(this,
			 "Incompatible scalar array import to"
			 " VolumeSum(const Oxs_MeshValue<OC_REAL8m>&)");
  }
  const OC_INDEX size=Size();
  OC_REAL8m sum=0.0;
  for(OC_INDEX i=0;i<size;i++) {
    sum += scalar[i] * Volume(i);
  }
  return sum;
}

ThreeVector
Oxs_Mesh::VolumeSum(const Oxs_MeshValue<ThreeVector>& vec) const
{
  if(!vec.CheckMesh(this)) {
    throw Oxs_ExtError(this,
			 "Incompatible import array to"
			 " VolumeSum(const Oxs_MeshValue<ThreeVector>&)");
  }
  const OC_INDEX size=Size();
  ThreeVector sum(0.,0.,0.);
  for(OC_INDEX i=0;i<size;i++) {
    sum +=  Volume(i) * vec[i] ;
  }
  return sum;
}

ThreeVector
Oxs_Mesh::VolumeSum(const Oxs_MeshValue<ThreeVector>& vec,
		    const Oxs_MeshValue<OC_REAL8m>& scale) const
{
  if(!vec.CheckMesh(this) || !scale.CheckMesh(this)) {
    throw Oxs_ExtError(this,
			 "Incompatible import array to"
			 " VolumeSum(const Oxs_MeshValue<ThreeVector>&,"
			 " const Oxs_MeshValue<OC_REAL8m>& scale)");
  }
  const OC_INDEX size=Size();
  ThreeVector sum(0.,0.,0.);
  for(OC_INDEX i=0;i<size;i++) {
    sum +=  ( (Volume(i) * scale[i]) * vec[i] );
  }
  return sum;
}

OC_REAL8m
Oxs_Mesh::VolumeSumXp(const Oxs_MeshValue<OC_REAL8m>& scalar) const
{
  if(!scalar.CheckMesh(this)) {
    throw Oxs_ExtError(this,
			 "Incompatible scalar array import to"
			 " VolumeSumXp(const Oxs_MeshValue<OC_REAL8m>&)");
  }

  Nb_Xpfloat sum=0.;
  const OC_INDEX size=Size();
  for(OC_INDEX i=0;i<size;i++) {
    sum += scalar[i] * Volume(i);
  }

  return sum.GetValue();
}

ThreeVector
Oxs_Mesh::VolumeSumXp(const Oxs_MeshValue<ThreeVector>& vec) const
{
  if(!vec.CheckMesh(this)) {
    throw Oxs_ExtError(this,
			 "Incompatible import array to"
			 " VolumeSumXp(const Oxs_MeshValue<ThreeVector>&)");
  }

  Nb_Xpfloat sum_x=0.,sum_y=0.,sum_z=0.;
  const OC_INDEX size=Size();
  for(OC_INDEX i=0;i<size;i++) {
    OC_REAL8m vol = Volume(i);
    sum_x += vol*vec[i].x;
    sum_y += vol*vec[i].y;
    sum_z += vol*vec[i].z;
  }

  return ThreeVector(sum_x.GetValue(),sum_y.GetValue(),sum_z.GetValue());
}

ThreeVector
Oxs_Mesh::VolumeSumXp(const Oxs_MeshValue<ThreeVector>& vec,
		      const Oxs_MeshValue<OC_REAL8m>& scale) const
{
  if(!vec.CheckMesh(this) || !scale.CheckMesh(this)) {
    throw Oxs_ExtError(this,
			 "Incompatible import array to"
			 " VolumeSumXp(const Oxs_MeshValue<ThreeVector>&,"
			 " const Oxs_MeshValue<OC_REAL8m>& scale)");
  }

  Nb_Xpfloat sum_x=0.,sum_y=0.,sum_z=0.;
  const OC_INDEX size=Size();
  for(OC_INDEX i=0;i<size;i++) {
    OC_REAL8m mult = Volume(i) * scale[i];
    sum_x += mult*vec[i].x;
    sum_y += mult*vec[i].y;
    sum_z += mult*vec[i].z;
  }

  return ThreeVector(sum_x.GetValue(),sum_y.GetValue(),sum_z.GetValue());
}

