/* FILE: meshvalue.cc                 -*-Mode: c++-*-
 *
 * Oxs_MeshValue templated class, intended for use with the
 * Oxs_Mesh family.
 *
 * The definitions for the templated Oxs_MeshValue and related class
 * are in the header file meshvalue.h, q.v. The two non-templated
 * overloaded Oxs_MeshValueOutputField functions can be defined either
 * in this file or as inlined objects in meshvalue.h.  Most compilers
 * accept Oxs_MeshValueOutputField definitions in either place, but
 * the Digital Mars version 8.57 compiler fails with an unresolved external
 * error at link time if the definitions are in this file, whereas the
 * Borland C++ 5.82 compiler (2005) fails with the same problem if the
 * Oxs_MeshValueOutputField definitions are inlined in the header.
 *
 * So, to make everybody happy, the definitions are in both places,
 * with macros to select exactly one of the definitions at compiler
 * time.
 *
 * BIG FAT NOTE: Any changes to the Oxs_MeshValueOutputField functions
 * need to be duplicated in *BOTH* places.
 *
 */

#include "meshvalue.h"

/* End includes */

#if !OXS_MESHVALUEOUTPUTFIELD_IN_HEADER

// Routines for Oxs_MeshValue<OC_REAL8m> output
void Oxs_MeshValueOutputField
(Tcl_Channel channel,    // Output channel
 OC_BOOL do_headers,     // If false, then output only raw data
 const char* title,      // Long filename or title
 const char* desc,       // Description to embed in output file
 vector<String>& valuelabels, // Value label, such as "Exchange energy density"
 vector<String>& valueunits,  // Value units, such as "J/m^3".
 Vf_Ovf20_MeshType meshtype,   // Either rectangular or irregular
 Vf_OvfDataStyle datastyle,   // vf_oascii, vf_obin4, or vf_obin8
 const char* textfmt,  // vf_oascii output only, printf-style format
 const Vf_Ovf20_MeshNodes* mesh,    // Mesh
 const Oxs_MeshValue<OC_REAL8m>* val,  // Scalar array
 Vf_OvfFileVersion ovf_version)
{
  Vf_Ovf20FileHeader fileheader;

  // Fill header
  mesh->DumpGeometry(fileheader,meshtype);
  fileheader.title.Set(String(title));
  fileheader.valuedim.Set(1);  // Scalar field
  fileheader.valuelabels.Set(valuelabels);
  fileheader.valueunits.Set(valueunits);
  fileheader.desc.Set(String(desc));
  fileheader.ovfversion = ovf_version;
  if(!fileheader.IsValid()) {
    OXS_THROW(Oxs_ProgramLogicError,"Oxs_MeshValueOutputField(T=OC_REAL8m)"
              " failed to create a valid OVF fileheader.");
  }

  // Write header
  if(do_headers) fileheader.WriteHeader(channel);

  // Write data
  Vf_Ovf20VecArrayConst data_info(1,val->Size(),val->arr);
  fileheader.WriteData(channel,datastyle,textfmt,mesh,
                       data_info,!do_headers);
}

// For Oxs_MeshValue<ThreeVector> output
void Oxs_MeshValueOutputField
(Tcl_Channel channel,    // Output channel
 OC_BOOL do_headers,        // If false, then output only raw data
 const char* title,      // Long filename or title
 const char* desc,       // Description to embed in output file
 vector<String>& valuelabels, // Value label, such as "Exchange energy density"
 vector<String>& valueunits,  // Value units, such as "J/m^3".
 Vf_Ovf20_MeshType meshtype,   // Either rectangular or irregular
 Vf_OvfDataStyle datastyle,   // vf_oascii, vf_obin4, or vf_obin8
 const char* textfmt,  // vf_oascii output only, printf-style format
 const Vf_Ovf20_MeshNodes* mesh,    // Mesh
 const Oxs_MeshValue<ThreeVector>* val,  // Scalar array
 Vf_OvfFileVersion ovf_version)
{
  Vf_Ovf20FileHeader fileheader;

  // Fill header
  mesh->DumpGeometry(fileheader,meshtype);
  fileheader.title.Set(String(title));
  fileheader.valuedim.Set(3);  // Vector field
  fileheader.valuelabels.Set(valuelabels);
  fileheader.valueunits.Set(valueunits);
  fileheader.desc.Set(String(desc));
  fileheader.ovfversion = ovf_version;
  if(!fileheader.IsValid()) {
    OXS_THROW(Oxs_ProgramLogicError,"Oxs_MeshValueOutputField(T=ThreeVector)"
              " failed to create a valid OVF fileheader.");
  }

  // Write header
  if(do_headers) fileheader.WriteHeader(channel);

  const OC_REAL8m* arrptr = NULL;
  Nb_ArrayWrapper<OC_REAL8m> rbuf;
  if(sizeof(ThreeVector) == 3*sizeof(OC_REAL8m)) {
    // Use MeshValue array directly
    arrptr = reinterpret_cast<const OC_REAL8m*>(val->arr);
  } else {
    // Need intermediate buffer space
    const OC_INDEX valsize = val->Size();
    rbuf.SetSize(3*valsize);
    for(OC_INDEX i=0;i<valsize;++i) {
      rbuf[3*i]   = val->arr[i].x;
      rbuf[3*i+1] = val->arr[i].y;
      rbuf[3*i+2] = val->arr[i].z;
    }
    arrptr = rbuf.GetPtr();
  }


  // Write data
  Vf_Ovf20VecArrayConst data_info(3,val->Size(),arrptr);
  fileheader.WriteData(channel,datastyle,textfmt,mesh,
                       data_info,!do_headers);
}
#endif // !OXS_MESHVALUEOUTPUTFIELD_IN_HEADER
