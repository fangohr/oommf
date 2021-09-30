/* FILE: vecfile.cc                 -*-Mode: c++-*-
 *
 * Main source file for vector field file I/O.
 *
 * Last modified on: $Date: 2016/03/18 23:14:41 $
 * Last modified by: $Author: donahue $
 */

#include <cassert>
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstring>

#include "oc.h"
#include "nb.h"

#include "fileio.h"  // Obsoleted *.vio input
#include "vecfile.h"

/* End includes */     // This is an optional directive to build.tcl,
                       // that excludes the remainder of the file from
                       // '#include ".*"' dependency building

///////////////////////////////////////////////////////////////////////////
// Vf_FileInputID Class

const ClassDoc Vf_FileInputID::class_doc("Vf_FileInputID",
              "Michael J. Donahue (michael.donahue@nist.gov)",
              "1.0.0","18-Mar-1997");


Vf_FileInputID::Vf_FileInputID(const char *new_filetype,
                               OC_BOOL (*newpIsType)(const char *,FILE *,
                                                  Tcl_Channel,const char *),
                               Vf_FileInput* (*newpMakeNew)(const char *,
                                                            FILE *,
                                                            Tcl_Channel,
                                                            const char *),
                               OC_BOOL (*pRegister)(const Vf_FileInputID &))
  : filetype(new_filetype)
{
#define MEMBERNAME "Vf_FileInputID"
  pIsType=newpIsType;
  pMakeNew=newpMakeNew;
  if(pRegister!=NULL)  (*pRegister)(*this);
#undef MEMBERNAME
}

void Vf_FileInputID::Dup(const Vf_FileInputID &rhs)
{ // Assignment operator
#define MEMBERNAME "Dup"
  filetype=rhs.filetype;
  pIsType=rhs.pIsType;
  pMakeNew=rhs.pMakeNew;
#undef MEMBERNAME
}

Vf_FileInputID& Vf_FileInputID::operator=(
                           const Vf_FileInputID &rhs)
{ // Assignment operator
  Dup(rhs);
  return *this;
}

Vf_FileInputID::Vf_FileInputID
(const Vf_FileInputID& copyobj) // Copy constructor
{
  Dup(copyobj);
}

Vf_FileInputID::Vf_FileInputID()
{ // Default constructor; Needed by Nb_List<T>
#define MEMBERNAME "Vf_FileInputID()"
  pIsType=NULL;
  pMakeNew=NULL;
#undef MEMBERNAME
}

// Dummy constructor taking const char*.  This is wanted by
// compilers (e.g., gcc) that try to instantiate all template
// member functions regardless of whether or not they are
// ever called.  (The template in question is Nb_List<T>.)
Vf_FileInputID::Vf_FileInputID(const char*)
{
#define MEMBERNAME "Vf_FileInputID(const char*)"
    FatalError(-1,STDDOC,"Illegal constructor call.");
#undef MEMBERNAME
}


Vf_FileInputID::~Vf_FileInputID()
{ // Dummy function for now
#define MEMBERNAME "~Vf_FileInputID"
#undef MEMBERNAME
}

#ifdef __GNUC__
// gcc wants explicit instantiation.
template class Nb_List<Vf_FileInputID>;
#endif // __GNUC__

///////////////////////////////////////////////////////////////////////////
// Vf_FileInput Class

const ClassDoc Vf_FileInput::class_doc("Vf_FileInput",
              "Michael J. Donahue (michael.donahue@nist.gov)",
              "1.0.0","17-Mar-1997");

Nb_List<Vf_FileInputID> Vf_FileInput::child_list;

Vf_FileInput::Vf_FileInput(const char *input_filename,
                           FILE *new_infile,
                           Tcl_Channel new_channel,
                           const char *new_channelName)
  : filename(input_filename),infile(new_infile),
    channel(new_channel),channelName(new_channelName)
{
#define MEMBERNAME "Vf_FileInput"
#undef MEMBERNAME
}

Vf_FileInput::~Vf_FileInput()
{
#define MEMBERNAME "~Vf_FileInput"
  if(infile!=NULL)  fclose(infile);
  Tcl_Interp* interp=Oc_GlobalInterpreter();
  if(channel!=NULL && interp!=NULL) {
    Tcl_SavedResult saved;
    Tcl_SaveResult(interp, &saved);
    Tcl_UnregisterChannel(interp, channel);
    Tcl_RestoreResult(interp, &saved);
  // NOTE: "channel" should be registered with the global Tcl
  // interpreter, and should therefore be automatically
  // closed when "unregistered."
  }
#undef MEMBERNAME
}

// All children classes of VFFI should register themselves with
// RegisterChild so that they can be called as necessary from
// NewReader().
OC_BOOL Vf_FileInput::RegisterChild(const Vf_FileInputID &id)
{
#define MEMBERNAME "RegisterChild"
  Vf_FileInputID *id_ptr;
  // Check for repeats
  for(id_ptr=child_list.GetFirst(); id_ptr!=NULL;
      id_ptr=child_list.GetNext()) {
    if(StringCompare(id.filetype,id_ptr->filetype)==0)
      FatalError(-1,STDDOC,"Type %s already registered",
                 (const char*)id_ptr->filetype);
  }
  child_list.Append(id);
  return 0;
#undef MEMBERNAME
}


// Clients should call NewReader() to get a VFFI instance.  The
// client is responsible for destroying (via delete) the returned
// object, i.e., treat this as a "new" operator.
Vf_FileInput *Vf_FileInput::NewReader(const char *new_file,
                                      Nb_DString* errmsg)
{
#define MEMBERNAME "NewReader"
  // Open to both a FILE* and a Tcl_Channel
  FILE *fptr=Nb_FOpen(new_file,"rb");
  if(fptr==NULL) {
    if(errmsg==NULL) {
      ClassMessage(STDDOC,"Unable to open file \"%s\" for reading",new_file);
    } else {
      *errmsg = "Unable to open file \"";
      *errmsg += new_file;
      *errmsg += "\" for reading";
    }
    return NULL;
  }
  Tcl_Interp* interp=Oc_GlobalInterpreter();
  if(interp==NULL)
    FatalError(-1,STDDOC,"Tcl interpretor not initialized");
  Tcl_SavedResult saved;
  Tcl_SaveResult(interp, &saved);
  char *tmpbuf=new char[strlen(new_file)+16];
  strcpy(tmpbuf,new_file);
  Tcl_Channel mychannel=Tcl_OpenFileChannel(interp,tmpbuf,
                                            OC_CONST84_CHAR("r"),0);
  delete[] tmpbuf;
  if(mychannel==NULL) {
    if(errmsg==NULL) {
      ClassMessage(STDDOC,"Unable to open file \"%s\" for Tcl reading",
                   new_file);
    } else {
      *errmsg = "Unable to open file \"";
      *errmsg += new_file;
      *errmsg += "\" for Tcl reading";
    }
    Tcl_RestoreResult(interp, &saved);
    fclose(fptr);
    return NULL;
  }
  // Note: I had intended to just associate "mychannel" with the input
  // stream "fptr" via a call to Tcl_MakeFileChannel, but I ran into several
  // problems: 1) there appear likely to be portability difficulties between
  // Unix and Windows, 2)  the Tcl_MakeFileChannel interface is different in
  // Tcl 8.0 than in 7.x (though this can be handled with a simple #ifdef),
  // 3) Tcl closes the file itself when the file is "unregistered", but
  // Tcl_MakeFileChannel takes a file descriptor, *not* a FILE* stream pointer,
  // so I don't know if the stream gets properly closed (and "fclose"-ing a
  // file more than once has undefined behavior, and in particular crashes
  // on Linux/Axp, and 4) the file descriptor, fileno(fptr), is passed as
  // type ClientData, which is a void*, so you need a call like
  //
  //  int handle=fileno(fptr);
  //  Tcl_Channel mychannel=Tcl_MakeFileChannel((void *)handle,NULL,
  //                                            TCL_READABLE);
  //
  // (_not_ ...((void *)(&handle)).  This causes the "int handle" to be
  // converted to a void*, which then gets converted back to an int on the
  // other end.  I'm a little skittish about doing this on a machines where
  // void* and int are different sizes.
  //   So, I decided it is probably safer to just open the fool file twice.
  // -mjd, 30-Aug-1997.

  Tcl_RegisterChannel(interp, mychannel);
  Tcl_RestoreResult(interp, &saved);
  const char *mychannelName=Tcl_GetChannelName(mychannel);

  // Check that this is not an empty file (special case)
  char empty_check;
  if(Tcl_Read(mychannel,&empty_check,1) != 1) {
    if(errmsg==NULL) {
      ClassMessage(STDDOC,"File \"%s\" is empty or unreadable",new_file);
    } else {
      *errmsg = "File \"";
      *errmsg += new_file;
      *errmsg += "\" is empty or unreadable";
    }
    if(fptr!=NULL)      fclose(fptr);
    if(mychannel!=NULL) Tcl_UnregisterChannel(interp, mychannel);
    return NULL;
  }

  // Determine file type
  Vf_FileInputID *id_ptr;
  try {
    for(id_ptr=child_list.GetFirst(); id_ptr!=NULL;
        id_ptr=child_list.GetNext()) {
      rewind(fptr);  Tcl_Seek(mychannel,0,SEEK_SET);  // Rewind both streams
      if((id_ptr->pIsType)(new_file,fptr,mychannel,mychannelName)) break;
    }
  } catch(...) {
    id_ptr = NULL;
  }

  if(id_ptr==NULL) {
    if(errmsg==NULL) {
      ClassMessage(STDDOC,"File \"%s\" is of unknown type",new_file);
    } else {
      *errmsg = "File \"";
      *errmsg += new_file;
      *errmsg += "\" is of unknown type";
    }
    if(fptr!=NULL)      fclose(fptr);
    if(mychannel!=NULL) Tcl_UnregisterChannel(interp, mychannel);
    return NULL;
  }

  rewind(fptr);  Tcl_Seek(mychannel,0,SEEK_SET);  // Rewind both streams

  return (id_ptr->pMakeNew)(new_file,fptr,mychannel,mychannelName);
  // Note: It is the responsibility of the VFFI destructor to
  //       fclose fptr and unregister mychannel.
#undef MEMBERNAME
}

//////////////////////////////////////////////////////////////////////
// Ovf File Format specs
const ClassDoc Vf_OvfFileFormatSpecs::class_doc("Vf_OvfFileFormatSpecs",
              "Michael J. Donahue (michael.donahue@nist.gov)",
              "1.0.1","5-Oct-1997");

// 4- & 8-byte real data block check values
const OC_REAL4 Vf_OvfFileFormatSpecs::CheckValue4=OC_REAL4(1234567.);
const OC_REAL8 Vf_OvfFileFormatSpecs::CheckValue8=OC_REAL8(123456789012345.);

Vf_OvfFileVersion Vf_OvfFileFormatSpecs::GetFileVersion(const char *test)
{ // Returns version, or invalid if test isn't a valid version string

  Vf_OvfFileVersion result = vf_ovf_invalid;

  char *buf=new char[strlen(test)+1];

  // Strip spaces and convert to lowercase
  char *cptr1=buf;
  const char *cptr2=test;
  while(*cptr2!='\0') {
    if(!isspace(*cptr2)) *(cptr1++) = (char)tolower(*cptr2);
    cptr2++;
  }
  *cptr1='\0';

  // For v2.0 files, the reduced id string should be exactly
  // "#oommfovf2.0".
  const char* idstring20="#oommfovf2.0";
  if(strcmp(buf,idstring20)==0) {
    result = vf_ovf_20;
  } else {
    // Older OVF files, prior to verson 2.0
    // The first part of the string should match "#oommf"
    const char* idstring="#oommf";
    if(strncmp(buf,idstring,strlen(idstring))==0) {
      // Next check that version is 0.0a0 or >0.98.  The version
      // number is at the end of the string, with the form
      // "v#.##" or "v0.0a0".  (v0.0a0 was written into ovf file
      // headers by mmsolve, and v0.99 by mmDisp prior to
      // 15-July-1999.)
      char *cptr=strrchr(buf,'v');
      if(cptr!=NULL) {
        cptr++;
        if(strcmp(cptr,"0.0a0")==0) {
          result=vf_ovf_10;
        } else {
          double version=Nb_Atof(cptr);
          if(version>0.98) result=vf_ovf_10;
        }
      }
    }
  }

  delete[] buf;
  return result;
}

// Binary I/O routines that perform byteswapping as needed.
// Return 0 on success, !=0 on error.
OC_INT4m Vf_OvfFileFormatSpecs::ReadBinary(FILE *infile,OC_REAL4 *buf,
                                         OC_INDEX count)
{
  assert(count>=0);
  if(fread((char *)buf,sizeof(OC_REAL4),size_t(count),infile)
                                   != static_cast<size_t>(count))
    return 1;
#if (OC_BYTEORDER == 4321)  // Flip bytes from MSB to LSB on input
  Oc_SwapBuf(buf,sizeof(OC_REAL4),count);
#endif
  return 0;
}

OC_INT4m Vf_OvfFileFormatSpecs::ReadBinary(FILE *infile,OC_REAL8 *buf,
                                        OC_INDEX count)
{
  assert(count>=0);
  if(fread((char *)buf,sizeof(OC_REAL8),size_t(count),infile)
                                   != static_cast<size_t>(count))
    return 1;
#if (OC_BYTEORDER == 4321)  // Flip bytes from MSB to LSB on input
  Oc_SwapBuf(buf,sizeof(OC_REAL8),count);
#endif
  return 0;
}

OC_INT4m Vf_OvfFileFormatSpecs::ReadBinaryFromLSB(FILE *infile,OC_REAL4 *buf,
                                         OC_INDEX count)
{
  assert(count>=0);
  if(fread((char *)buf,sizeof(OC_REAL4),size_t(count),infile)
                                   != static_cast<size_t>(count))
    return 1;
#if (OC_BYTEORDER == 1234)  // Flip bytes from LSB to MSB on input
  Oc_SwapBuf(buf,sizeof(OC_REAL4),count);
#endif
  return 0;
}

OC_INT4m Vf_OvfFileFormatSpecs::ReadBinaryFromLSB(FILE *infile,OC_REAL8 *buf,
                                        OC_INDEX count)
{
  assert(count>=0);
  if(fread((char *)buf,sizeof(OC_REAL8),size_t(count),infile)
                                   != static_cast<size_t>(count))
    return 1;
#if (OC_BYTEORDER == 1234)  // Flip bytes from LSB to MSB on input
  Oc_SwapBuf(buf,sizeof(OC_REAL8),count);
#endif
  return 0;
}

OC_INT4m Vf_OvfFileFormatSpecs::WriteBinary(FILE *outfile,OC_REAL4 *buf,
                                         OC_INDEX count)
{
  assert(count>=0);
  char *cptr=(char *)buf;
#if (OC_BYTEORDER == 4321)  // Flip bytes from LSB to MSB on output
  Oc_SwapBuf(cptr,sizeof(OC_REAL4),count);
#endif
  if(fwrite(cptr,sizeof(OC_REAL4),size_t(count),outfile)
                             != static_cast<size_t>(count))
    return 1;
  return 0;
}

OC_INT4m Vf_OvfFileFormatSpecs::WriteBinary(FILE *outfile,OC_REAL8 *buf,
                                         OC_INDEX count)
{
  assert(count>=0);
  char *cptr=(char *)buf;
#if (OC_BYTEORDER == 4321)  // Flip bytes from LSB to MSB on output
  Oc_SwapBuf(cptr,sizeof(OC_REAL8),count);
#endif
  if(fwrite(cptr,sizeof(OC_REAL8),size_t(count),outfile)
                             != static_cast<size_t>(count))
    return 1;
  return 0;
}

OC_INT4m Vf_OvfFileFormatSpecs::WriteBinary(Tcl_Channel channel,OC_REAL4 *buf,
                                         OC_INDEX count)
{
  char *cptr=(char *)buf;
#if (OC_BYTEORDER == 4321)  // Flip bytes from LSB to MSB on output
  Oc_SwapBuf(cptr,sizeof(OC_REAL4),count);
#endif
  OC_INDEX byte_count = OC_INDEX(sizeof(OC_REAL4))*count;
  if(Nb_WriteChannel(channel,cptr,byte_count) != byte_count) return 1;
  return 0;
}

OC_INT4m Vf_OvfFileFormatSpecs::WriteBinary(Tcl_Channel channel,OC_REAL8 *buf,
                                         OC_INDEX count)
{
  char *cptr=(char *)buf;
#if (OC_BYTEORDER == 4321)  // Flip bytes from LSB to MSB on output
  Oc_SwapBuf(cptr,sizeof(OC_REAL8),count);
#endif
  OC_INDEX byte_count = OC_INDEX(sizeof(OC_REAL8))*count;
  if(Nb_WriteChannel(channel,cptr,byte_count) != byte_count) return 1;
  return 0;
}


//////////////////////////////////////////////////////////////////////
// Vf_OvfSegmentHeader
//
const ClassDoc Vf_OvfSegmentHeader::class_doc("Vf_OvfSegmentHeader",
              "Michael J. Donahue (michael.donahue@nist.gov)",
              "1.0.0","24-Mar-1997");

Vf_OvfSegmentHeader::Vf_OvfSegmentHeader()
{
#define MEMBERNAME "Vf_OvfSegmentHeader"
  SetBools(0);
  meshtype = vf_oshmt_rectangular;  // Default setting, for backwards
  /// compatibility with some old OVF versions that did not require
  /// a meshtype record.
#undef MEMBERNAME
}

Vf_OvfSegmentHeader::~Vf_OvfSegmentHeader()
{
}

void Vf_OvfSegmentHeader::SetBools(OC_BOOL newval)
{ // Sets all OC_BOOL's to newval
#define MEMBERNAME "SetBools"
  title_bool=desc_bool=
    meshunit_bool=xbase_bool=ybase_bool=zbase_bool=
    xstepsize_bool=ystepsize_bool=zstepsize_bool=
    xnodes_bool=ynodes_bool=znodes_bool=
    xmin_bool=ymin_bool=zmin_bool=
    xmax_bool=ymax_bool=zmax_bool=
    valueunit_bool=valuemultiplier_bool=
    valuerangemaxmag_bool=valuerangeminmag_bool=
    meshtype_bool=pointcount_bool=bdryline_bool=
    valuedim_bool=valueunits_list_bool=valuelabels_list_bool=newval;
#undef MEMBERNAME
}

OC_BOOL Vf_OvfSegmentHeader::ValidSet(void) const
{ // Returns 1 iff a complete and valid set of the OC_BOOLS are set,
  //  and the associated values match some sanity checks
#define MEMBERNAME "ValidSet"
  // Currently 2 + 2 cases: rectangular & irregular grids,
  // and OVF versions 1.0 or 2.0.
  // For v1.0 files, the only fields optional for all files are
  //   desc & bdryline.  For backwards compatibility, meshtype
  //   is also not required, in which case it defaults to a
  //   regular rectangular grid.  The valuedim, valueunits_list,
  //   and valuelabels_list fields are not allowed.
  // For v2.0 files, the only always optional field is desc.
  //   Unallowed fields are: valuemultiplier, boundary,
  //   ValueRangeMaxMag, and ValueRangeMinMag.  Fields valuedim
  //   valueunits_list, and valuelabel_list are required
  //   (and also meshtype).
  //   NB: In v2.0 files, the record label for the valueunits_list
  //   is "valueunits"; the "valueunit" label of v1.0 is not
  //   allowed in v2.0 files.
  // For both versions, fields xstepsize, ystepsize, and zstepsize
  //   are required for rectangular grids, and optional for
  //   irregular grids.

  // The following fields are required for all ovf files
  OC_BOOL set_code = title_bool && meshunit_bool
    && xmin_bool && ymin_bool && zmin_bool
    && xmax_bool && ymax_bool && zmax_bool
    && fileversion_bool;
  if(!set_code) return set_code;

  // Check fields by file version
  if(fileversion==vf_ovf_20) {
    // OVF version 2.0
    set_code = set_code && (valuedim>0)
      && valueunits_list_bool && valuelabels_list_bool
      && meshtype_bool && (!valueunit_bool) && (!valuemultiplier_bool)
      && (!valuerangemaxmag_bool) && (!valuerangeminmag_bool);
  } else if(fileversion==vf_ovf_10) {
    // OVF version 1.0
    if(!meshtype_bool) {
      if(meshtype != vf_oshmt_rectangular) {
        // Check default setting
        ClassMessage(STDDOC,"Coding error: meshtype default not set");
        set_code=0;
      }
    }
    set_code = set_code && valueunit_bool && valuemultiplier_bool
      && valuerangemaxmag_bool && valuerangeminmag_bool
      && (!valuedim_bool) && (!valueunits_list_bool)
      && (!valuelabels_list_bool);
  } else {
    ClassMessage(STDDOC,"Unknown file version: %d\n",(int)fileversion);
    set_code=0;
  }
  if(!set_code) return set_code;

  // Check fields by mesh type
  if(meshtype==vf_oshmt_rectangular) {
    set_code = set_code && xbase_bool && ybase_bool && zbase_bool
       && xstepsize_bool && ystepsize_bool && zstepsize_bool
       && xnodes_bool && ynodes_bool && znodes_bool;
    set_code = set_code && (!pointcount_bool);
  }
  else if(meshtype==vf_oshmt_irregular) {
    set_code = set_code && pointcount_bool;
    set_code = set_code && (!xbase_bool) && (!ybase_bool) && (!zbase_bool)
       && (!xnodes_bool) && (!ynodes_bool) && (!znodes_bool);
  }
  else {
    ClassMessage(STDDOC,"Unknown mesh type: %d\n",(int)meshtype);
    set_code=0;
  }

  // Sanity checks
  if(pointcount_bool && pointcount<1) set_code = 0;
  if(xnodes_bool && xnodes<1)     set_code = 0;
  if(ynodes_bool && ynodes<1)     set_code = 0;
  if(znodes_bool && znodes<1)     set_code = 0;
  if(valuedim_bool && valuedim<1) set_code = 0;
  if(valueunits_list_bool) {
    if(!valuedim_bool) {
      set_code = 0;
    } else {
      const OC_INDEX listlen = valueunits_list.GetSize();
      assert(listlen == OC_INDEX(OC_INT4(listlen)));
      if(static_cast<OC_INT4>(listlen) != valuedim && listlen != 1) {
        set_code = 0;
      }
    }
  }
  if(valuelabels_list_bool && 
     (!valuedim_bool || valuelabels_list.GetSize() != valuedim)) {
    set_code = 0;
  }

  return set_code;
#undef MEMBERNAME
}

void Vf_OvfSegmentHeader::SetFieldValue(const Nb_DString &field,
                                     const Nb_DString &value)
{ // Note 1: The import field should be set into lowercase before calling
  //   this routine.  This routine will duplicate and lowercase value
  //   as needed.
  // Note 2: The fileversion field is *not* set by this routine,
  //   but should be extracted from the very first line of the
  //   file.  In particular, fileversion must be set prior to
  //   calling this routine.
#define MEMBERNAME "Vf_OvfSetFieldValue"

  if(!fileversion_bool || fileversion == vf_ovf_invalid) {
    ClassMessage(STDDOC,"Programming error: fileversion not set");
  }

  if(field.Length()==0) return;  // Blank line
  if(strcmp((const char *)field,"title")==0) {
    title=value;    title_bool=1;
  }
  else if(strcmp((const char *)field,"desc")==0) {
    if(desc_bool==0) { desc=value; desc_bool=1; }
    else { desc.Append("\n"); desc.Append(value); } // Separate multiple
    /// Desc input records with newlines.
  }
  else if(strcmp((const char *)field,"meshunit")==0) {
    meshunit=value;    meshunit_bool=1;
  }
  else if(strcmp((const char *)field,"meshtype")==0) {
    Nb_DString value_lower(value);
    value_lower.Trim();
    value_lower.ToLower();
    if(strcmp((const char *)value_lower,"rectangular")==0) {
      meshtype=vf_oshmt_rectangular;   meshtype_bool=1;
    }
    else if(strcmp((const char *)value_lower,"irregular")==0) {
      meshtype=vf_oshmt_irregular;     meshtype_bool=1;
    }
    else {
      ClassMessage(STDDOC,"Unknown ovf mesh type value: %s (skipping)",
                   (const char *)value);
    }
  }
  else if(strcmp((const char *)field,"xbase")==0) {
    xbase=Nb_Atof((const char *)value); xbase_bool=1;
  }
  else if(strcmp((const char *)field,"ybase")==0) {
    ybase=Nb_Atof((const char *)value); ybase_bool=1;
  }
  else if(strcmp((const char *)field,"zbase")==0) {
    zbase=Nb_Atof((const char *)value); zbase_bool=1;
  }
  else if(strcmp((const char *)field,"xstepsize")==0) {
    xstepsize=Nb_Atof((const char *)value); xstepsize_bool=1;
  }
  else if(strcmp((const char *)field,"ystepsize")==0) {
    ystepsize=Nb_Atof((const char *)value); ystepsize_bool=1;
  }
  else if(strcmp((const char *)field,"zstepsize")==0) {
    zstepsize=Nb_Atof((const char *)value); zstepsize_bool=1;
  }
  else if(strcmp((const char *)field,"xnodes")==0) {
    xnodes=atoi((const char *)value); xnodes_bool=1;
  }
  else if(strcmp((const char *)field,"ynodes")==0) {
    ynodes=atoi((const char *)value); ynodes_bool=1;
  }
  else if(strcmp((const char *)field,"znodes")==0) {
    znodes=atoi((const char *)value); znodes_bool=1;
  }
  else if(strcmp((const char *)field,"pointcount")==0) {
    pointcount=atoi((const char *)value); pointcount_bool=1;
  }
  else if(strcmp((const char *)field,"xmin")==0) {
    xmin=Nb_Atof((const char *)value); xmin_bool=1;
  }
  else if(strcmp((const char *)field,"ymin")==0) {
    ymin=Nb_Atof((const char *)value); ymin_bool=1;
  }
  else if(strcmp((const char *)field,"zmin")==0) {
    zmin=Nb_Atof((const char *)value); zmin_bool=1;
  }
  else if(strcmp((const char *)field,"xmax")==0) {
    xmax=Nb_Atof((const char *)value); xmax_bool=1;
  }
  else if(strcmp((const char *)field,"ymax")==0) {
    ymax=Nb_Atof((const char *)value); ymax_bool=1;
  }
  else if(strcmp((const char *)field,"zmax")==0) {
    zmax=Nb_Atof((const char *)value); zmax_bool=1;
  }
  else if(strcmp((const char *)field,"boundary")==0) {
    char *work=new char[static_cast<size_t>(value.Length())+1];
    strcpy(work,(const char *)value);
    Nb_Vec3<OC_REAL8> point;
    char *nexttoken = work;
    char *cptr=Nb_StrSep(&nexttoken," \t");
    while(cptr!=NULL && *cptr!='#') {
      // Load points into boundary list
      point.x=OC_REAL8(Nb_Atof(cptr));
      cptr=Nb_StrSep(&nexttoken," \t");
      if(cptr==NULL || *cptr=='#') {
        ClassMessage(STDDOC,"Incomplete boundary point (skipping)");
        break;
      }
      point.y=OC_REAL8(Nb_Atof(cptr));
      cptr=Nb_StrSep(&nexttoken," \t");
      if(cptr==NULL || *cptr=='#') {
        ClassMessage(STDDOC,"Incomplete boundary point (skipping)");
        break;
      }
      point.z=OC_REAL8(Nb_Atof(cptr));
      bdryline.Append(point);
      cptr=Nb_StrSep(&nexttoken," \t");
    }
    bdryline_bool=1;
    delete[] work;
  }
  else if(fileversion==vf_ovf_10
          && strcmp((const char *)field,"valueunit")==0) {
    valueunit=value;   valueunit_bool=1;
  }
  else if(strcmp((const char *)field,"valuemultiplier")==0) {
    valuemultiplier=Nb_Atof((const char *)value); valuemultiplier_bool=1;
  }
  else if(strcmp((const char *)field,"valuerangemaxmag")==0) {
    valuerangemaxmag=Nb_Atof((const char *)value); valuerangemaxmag_bool=1;
  }
  else if(strcmp((const char *)field,"valuerangeminmag")==0) {
    valuerangeminmag=Nb_Atof((const char *)value); valuerangeminmag_bool=1;
  }
  else if(fileversion==vf_ovf_20
          && strcmp((const char *)field,"valueunits")==0) {
    valueunits_list.TclSplit(value);
    valueunits_list_bool=1;
  }
  else if(strcmp((const char *)field,"valuedim")==0) {
    valuedim=atoi((const char *)value);
    valuedim_bool=1;
  }
  else if(strcmp((const char *)field,"valuelabels")==0) {
    valuelabels_list.TclSplit(value);
    valuelabels_list_bool=1;
  }
  else {
    ClassMessage(STDDOC,"Unknown ovf segment header field: %s (skipping)",
                  (const char *)field);
  }
#undef MEMBERNAME
}


///////////////////////////////////////////////////////////////////////////
// Vf_OvfFileInput Class

const ClassDoc Vf_OvfFileInput::class_doc("Vf_OvfFileInput",
              "Michael J. Donahue (michael.donahue@nist.gov)",
              "1.0.1","25-Jul-1997");

const char* Vf_OvfFileInput::file_type="OOMMF (.ovf) Input File";

const Vf_FileInputID Vf_OvfFileInput::id(Vf_OvfFileInput::file_type,
                                         Vf_OvfFileInput::IsType,
                                         Vf_OvfFileInput::MakeNew,
                                         Vf_FileInput::RegisterChild);

Vf_OvfFileInput::Vf_OvfFileInput(const char *new_filename,
                                 FILE *new_infile,
                                 Tcl_Channel new_channel,
                                 const char *new_channelName)
  : Vf_FileInput(new_filename,new_infile,new_channel,new_channelName)
{
#define MEMBERNAME "Vf_OvfFileInput"
  // Read file header here (i.e., determine # of segments)
#undef MEMBERNAME
}

Vf_FileInput* Vf_OvfFileInput::MakeNew(const char *new_filename,
                                            FILE *new_infile,
                                            Tcl_Channel new_channel,
                                            const char *new_channelName)
{
  return new Vf_OvfFileInput(new_filename,new_infile,
                          new_channel,new_channelName);
}

OC_BOOL Vf_OvfFileInput::IsType(const char *,
                          FILE *test_fptr,
                          Tcl_Channel,
                          const char *)
{
#define MEMBERNAME "IsType"
  char buf[SCRATCH_BUF_SIZE];
  buf[0]='\0'; // Safety
  if(!fgets(buf,sizeof(buf),test_fptr)) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_OvfFileInput","IsType",
                          "Error reading input."));
  }

  // Do string comparison
  return (Vf_OvfFileFormatSpecs::GetFileVersion(buf) != vf_ovf_invalid);
#undef MEMBERNAME
}

OC_INT4m Vf_OvfFileInput::ReadLine(Nb_DString &buf)
{ // Reads one text line from infile and sticks it into buf
  // Returns 0 on success, 1 on EOF or other error
  // NOTE: This routine is not terribly efficient (lots of
  //  unnecessary calls to string allocation/deallocation),
  //  and should probably only be used on ovf file headers
  // (*not* data sections).
#define MEMBERNAME "ReadLine"
  static char readbuf[SCRATCH_BUF_SIZE];
  char *cptr;
  buf="";  // Clear buf;
  do {
    readbuf[SCRATCH_BUF_SIZE-2]='\0';  // Used for line-too-long check
    cptr=fgets(readbuf,SCRATCH_BUF_SIZE,infile);
    if(cptr==NULL) return 1; // Error
    if(readbuf[SCRATCH_BUF_SIZE-2]!='\0') {
      // Line longer than buffer.  Try to detect binary input
      for(OC_INDEX i=0;i<SCRATCH_BUF_SIZE;i++) {
        if(readbuf[i]>0x7E) {
          buf=""; // Clear buf
          return 1;
        }
      }
    }
    buf.Append(readbuf,SCRATCH_BUF_SIZE);
  } while(readbuf[SCRATCH_BUF_SIZE-2]!='\0'
        && readbuf[SCRATCH_BUF_SIZE-2]!='\n');
  return 0;
#undef MEMBERNAME
}

OC_INT4m Vf_OvfFileInput::ParseContentLine(const Nb_DString &buf,
                                     Nb_DString &field,Nb_DString &value)
{ //   Breaks buf (import) down into field & value substrings (exports),
  // with field put into canonical form (strips whitespace & underscores,
  // and converts to lowercase).
  //   Return 0 on success, 1 on error.  The "field" value will be
  // a null string ('\0') if buf is an empty or comment line.
#define MEMBERNAME "ParseContentLine"
  field="";  value="";  // Default return values

  // Copy buf into workspace, trimming leading & trailing whitespace
  Nb_DString workstr(buf);
  workstr.Trim();
  char *work=&(workstr[0]); // This is cheating!  We can't use workstr
  // again from below this point, since if we do *work may become invalid.
  // It has the advantage, though, that the memory allocated by workstr
  // is automatically freed upon function exit.

  // Tokenize workspace
  char *fieldstart=strchr(work,'#');
  if(fieldstart==NULL) return 0;  // Possibly illegal line, but assume blank
  fieldstart++;
  if(*fieldstart=='#') return 0;  // Comment line; treat as blank
  char *valuestart=strchr(fieldstart,':');
  if(valuestart==NULL) return 0;  // Possibly illegal line, but assume blank
  *valuestart='\0'; valuestart++;

  // Collapse field string (remove all non alpha-numeric characters) and
  // convert to lowercase.
  char *cptr1,*cptr2;
  cptr1=cptr2=fieldstart;
  while(*cptr2!='\0') {
    if(isalnum(*cptr2)) *(cptr1++) = (char)tolower(*cptr2);
    cptr2++;
  }
  *cptr1='\0';
  field=fieldstart;

  // Skip leading whitespace in value string
  while(*valuestart!='\0' && isspace(*valuestart)) valuestart++;

  // If field is 'desc', then return as is.
  if(strncmp("desc",fieldstart,strlen("desc"))==0) {
    value=valuestart;
    return 0;
  }

  // Otherwise strip trailing comment (if any).
  cptr1=valuestart;
  while((cptr1=strchr(cptr1,'#'))!=NULL) if(*(++cptr1)=='#') break;
  if(cptr1!=NULL) *(cptr1-1)='\0';

  // Strip trailing whitespace
  cptr1=strchr(valuestart,'\0');
  while(cptr1>valuestart && isspace(*(--cptr1))) (*cptr1)='\0';

  // If field is "begin" or "end", then convert value to lowercase
  // and strip whitespace
  if(strcmp(field,"begin")==0 || strcmp(field,"end")==0) {
    cptr1=cptr2=valuestart;
    while(*cptr2!='\0') {
      if(isalnum(*cptr2)) *(cptr1++) = (char)tolower(*cptr2);
      cptr2++;
    }
    *cptr1='\0';
  }

  value=valuestart;
  return 0;
#undef MEMBERNAME
}

// Read "Nin" ASCII floating point values from infile, skipping any
// leading whitespace and eating the first whitespace character after
// the last value.  Store up to 3 values in the export "data", in the
// order .x, .y, .z.  If Nin<3, then latter components filled with
// zeros.  If Nin>3, then excess values are simply dropped.
//
// Returns 0 on success, 1 on file read error, 2 if input is a nan or
// inf.
//
OC_INT4m Vf_OvfFileInput::ReadTextVec3f(int Nin, Nb_Vec3<OC_REAL8> &data)
{
#define MEMBERNAME "ReadTextData"
  static char buf[SCRATCH_BUF_SIZE];
  int ch=0,bufindex;
  int elt=0;  // Next element to fill; 0->x, 1->y, 2->z
  OC_REAL8 value;
  data.x = data.y = data.z = 0.0;
  while(elt<Nin) {
    // Skip leading white space and comments
    while(1) {
      ch=fgetc(infile);
      if(ch==EOF) return 1;  // Premature end-of-file
      // Check for comment.
      if(ch=='#') {
        ch=fgetc(infile);
        if(ch==EOF) return 1;  // Premature end-of-file
        if(ch=='#') {
          // Comment; read to end of line
          while((ch=fgetc(infile))!='\n') {
            if(ch==EOF) return 1;  // Premature end-of-file
          }
        } else {
          // Blank line; read to end of line
          while ((ch=fgetc(infile))!='\n') {
            if(ch==EOF) return 1;  // Premature end-of-file
            if(!isspace(ch)) {
              ClassMessage(STDDOC,"Field-value pair inside data block,"
                           " or premature end of data in file %s.",
                           (const char *)filename);
              return 1;
            }
          }
        }
        continue;
      }
      // Non-comment white-space check
      if(isspace(ch)) continue;
      // Otherwise, should be data.
      break;
    }

    // Fill buffer with data up to next white space character
    bufindex=0;
    do {
      if(ch=='d' || ch=='D') ch='e';  // Convert FORTRAN 'd' exponent
      buf[bufindex++]=(char)ch;       // designator to C 'e' designator
      ch=fgetc(infile);
      if(ch==EOF) return 1;  // Premature end-of-file
    } while(!isspace(ch) && bufindex<SCRATCH_BUF_SIZE-1);
    if(bufindex>=SCRATCH_BUF_SIZE-1) {
      // Single data elt too big to fit in buffer?!
      ClassMessage(STDDOC,"Bad data in file %s;"
                    " Single value >%d characters long",
                    (const char *)filename,SCRATCH_BUF_SIZE);
      return 1;
    }
    buf[bufindex]='\0';

    // Convert to floating point and store
    char* endptr;
    value=OC_REAL8(Nb_Strtod(buf,&endptr));
    if( (endptr-buf) != bufindex ) {
      ClassMessage(STDDOC,"Non-numeric data ->%s<- in data block of file %s",
                    buf,(const char *)filename);
      return 1;
    }

    if(!Nb_IsFinite(value)) return 2;

    switch(elt) {
      case 0: data.x=value; break;
      case 1: data.y=value; break;
      case 2: data.z=value; break;
      default: break;  // Drop excess values
    }

    elt++;
  }

  return 0;
#undef MEMBERNAME
}

// Read "Nin" floating point values from infile, using "datastyle"
// format.  Store up to 3 values in the export "data", in the order
// .x, .y, .z.  If Nin<3, then latter components are filled with
// zeros.  If Nin>3, then excess values are simply dropped.
//
// Returns 0 on success, 1 on file read error, 2 if input is a nan or
// inf.
//
OC_INT4m Vf_OvfFileInput::ReadVec3f(Vf_OvfDataStyle datastyle,int Nin,
                                 Nb_Vec3<OC_REAL8> &data,
                                 OC_REAL8m* badvalue)
{
#define MEMBERNAME "ReadVec3f"
  if(datastyle == vf_oascii) return ReadTextVec3f(Nin,data);
  if(datastyle == vf_obin4)      {  // 4-byte binary type
    OC_REAL4 databuf[3];
    int count = 3;
    if(Nin<count) {
      count = Nin;
      databuf[0] = databuf[1] = databuf[2] = 0.0;
    }
    if(Vf_OvfFileFormatSpecs::ReadBinary(infile,databuf,OC_INDEX(count))!=0) {
      return 1;
    }
    if(count<Nin) {
      // Skip extra values in input file.
      if(fseek(infile,(Nin-count)*(long int)sizeof(OC_REAL4),SEEK_CUR)!=0) {
        return 1; // Presumably EOF
      }
    }
    if(!Nb_IsFinite(databuf[0]) || !Nb_IsFinite(databuf[1])
       || !Nb_IsFinite(databuf[2])) return 2;
    data.x=(OC_REAL8)databuf[0];
    data.y=(OC_REAL8)databuf[1];
    data.z=(OC_REAL8)databuf[2];
  }
  else if(datastyle == vf_obin8) {  // 8-byte binary type
    OC_REAL8 databuf[3];
    int count = 3;
    if(Nin<count) {
      count = Nin;
      databuf[0] = databuf[1] = databuf[2] = 0.0;
    }
    if(Vf_OvfFileFormatSpecs::ReadBinary(infile,databuf,OC_INDEX(count))!=0) {
      return 1;
    }
    if(count<Nin) {
      // Skip extra values in input file.
      if(fseek(infile,(Nin-count)*(long int)sizeof(OC_REAL8),SEEK_CUR)!=0) {
        return 1; // Presumably EOF
      }
    }
    if(!Nb_IsFinite(databuf[0])) {
      if(badvalue!=NULL) *badvalue=databuf[0];
      return 2;
    }
    if(!Nb_IsFinite(databuf[1])) {
      if(badvalue!=NULL) *badvalue=databuf[1];
      return 2;
    }
    if(!Nb_IsFinite(databuf[2])) {
      if(badvalue!=NULL) *badvalue=databuf[2];
      return 2;
    }
    data.x=databuf[0];
    data.y=databuf[1];
    data.z=databuf[2];
  }
  return 0;
#undef MEMBERNAME
}

OC_INT4m Vf_OvfFileInput::ReadVec3fFromLSB(Vf_OvfDataStyle datastyle,int Nin,
                                        Nb_Vec3<OC_REAL8> &data,
                                        OC_REAL8m* badvalue)
{
#define MEMBERNAME "ReadVec3fFromLSB"
  if(datastyle == vf_oascii) return ReadTextVec3f(Nin,data);
  if(datastyle == vf_obin4)      {  // 4-byte binary type
    OC_REAL4 databuf[3];
    int count = 3;
    if(Nin<count) {
      count = Nin;
      databuf[0] = databuf[1] = databuf[2] = 0.0;
    }
    if(Vf_OvfFileFormatSpecs::ReadBinaryFromLSB(infile,databuf,OC_INDEX(count))!=0) {
      return 1;
    }
    if(count<Nin) {
      // Skip extra values in input file.
      if(fseek(infile,(Nin-count)*(long int)sizeof(OC_REAL4),SEEK_CUR)!=0) {
        return 1; // Presumably EOF
      }
    }
    if(!Nb_IsFinite(databuf[0]) || !Nb_IsFinite(databuf[1])
       || !Nb_IsFinite(databuf[2])) return 2;
    data.x=(OC_REAL8)databuf[0];
    data.y=(OC_REAL8)databuf[1];
    data.z=(OC_REAL8)databuf[2];
  }
  else if(datastyle == vf_obin8) {  // 8-byte binary type
    OC_REAL8 databuf[3];
    int count = 3;
    if(Nin<count) {
      count = Nin;
      databuf[0] = databuf[1] = databuf[2] = 0.0;
    }
    if(Vf_OvfFileFormatSpecs::ReadBinaryFromLSB(infile,databuf,OC_INDEX(count))!=0) {
      return 1;
    }
    if(count<Nin) {
      // Skip extra values in input file.
      if(fseek(infile,(Nin-count)*(long int)sizeof(OC_REAL8),SEEK_CUR)!=0) {
        return 1; // Presumably EOF
      }
    }
    if(!Nb_IsFinite(databuf[0])) {
      if(badvalue!=NULL) *badvalue=databuf[0];
      return 2;
    }
    if(!Nb_IsFinite(databuf[1])) {
      if(badvalue!=NULL) *badvalue=databuf[1];
      return 2;
    }
    if(!Nb_IsFinite(databuf[2])) {
      if(badvalue!=NULL) *badvalue=databuf[2];
      return 2;
    }
    data.x=databuf[0];
    data.y=databuf[1];
    data.z=databuf[2];
  }
  return 0;
#undef MEMBERNAME
}

Vf_Mesh *Vf_OvfFileInput::NewMesh()
{
#define MEMBERNAME "NewMesh"
  Nb_DString buf,field,value;
  Vf_OvfDataStyle datastyle=vf_oascii;
  const char *datastylestring=NULL;

  Vf_OvfSegmentHeader ovfseghead;
  ovfseghead.SetBools(0);  // Safety

  // Extract file version from first line
  if(ReadLine(buf)!=0) {
    ClassMessage(STDDOC,"Premature EOF in file %s "
                 "(file type line missing)",
                 (const char *)filename);
    return new Vf_EmptyMesh();
  }
  ovfseghead.fileversion = Vf_OvfFileFormatSpecs::GetFileVersion(buf);
  if(ovfseghead.fileversion == vf_ovf_20) {
    ovfseghead.fileversion_bool = 1;
  } else if(ovfseghead.fileversion == vf_ovf_10) {
    ovfseghead.fileversion_bool = 1;
  } else {
    ClassMessage(STDDOC,"Invalid file type identifier \"%s\" in file %s",
                 buf.GetStr(),filename.GetStr());
    return new Vf_EmptyMesh();
  }

  // Skip remainder of preamble
  while(1) {
    if(ReadLine(buf)!=0) {
      ClassMessage(STDDOC,"Premature EOF in file %s "
                    "(no segment header found)",
                    (const char *)filename);
      return new Vf_EmptyMesh();
    }
    ParseContentLine(buf,field,value);
    if(strcmp(field,"begin")==0  && strcmp(value,"header")==0) break;
  }

  // Read head and store results into Vf_OvfSegmentHeader struct
  ovfseghead.SetFieldValue("meshtype","rectangular");
  /// Set default meshtype to rectangular, for older ovf files w/o
  /// the meshtype field.
  while(1) {
    if(ReadLine(buf)!=0) {
      ClassMessage(STDDOC,"Premature EOF in file %s "
                    "(missing segment header end mark)",
                    (const char *)filename);
      return new Vf_EmptyMesh();
    }
    ParseContentLine(buf,field,value);
    if(strcmp(field,"end")==0 && strcmp(value,"header")==0) break;
    if(field.Length()>0) ovfseghead.SetFieldValue(field,value);
  }
  if(!ovfseghead.ValidSet()) {
      ClassMessage(STDDOC,
        "Incomplete or invalid segment header in file %s; Skipping",
        (const char *)filename);
      return new Vf_EmptyMesh();
  }

#ifdef OLD_CODE
  // Is this needed???
  if(!ovfseghead.desc_bool)
    ovfseghead.SetFieldValue("desc","");  // Description field is optional
#endif

  // Skip ahead in file to data section
  while(1) {
    if(ReadLine(buf)!=0) {
      ClassMessage(STDDOC,"Premature EOF in file %s (no data section found)",
                    (const char *)filename);
      return new Vf_EmptyMesh();
    }
    ParseContentLine(buf,field,value);
    if(strcmp(field,"begin")==0) {
      if(strcmp(value,"datatext")==0) {
        // ASCII data
        datastyle=vf_oascii;
        datastylestring="datatext";
        break;
      }
      if(strcmp(value,"databinary4")==0) {
        // 4-byte binary
        datastyle=vf_obin4;
        datastylestring="databinary4";
        break;
      }
      if(strcmp(value,"databinary8")==0) {
        // 8-byte binary
        datastyle=vf_obin8;
        datastylestring="databinary8";
        break;
      }
    }
  }

  // Setup total_range bounding box to include the boundary list
  // and the ?min-$max corners.  (Regular grids additionally
  // expand to include all data points.  See "Rectangular grid"
  // initialization section below.)
  Nb_BoundingBox<OC_REAL8> total_range;
  Nb_Vec3<OC_REAL8> corner1(OC_REAL8(ovfseghead.xmin),OC_REAL8(ovfseghead.ymin),
                         OC_REAL8(ovfseghead.zmin));
  Nb_Vec3<OC_REAL8> corner2(OC_REAL8(ovfseghead.xmax),OC_REAL8(ovfseghead.ymax),
                         OC_REAL8(ovfseghead.zmax));
  total_range.SortAndSet(corner1,corner2);

  if(ovfseghead.bdryline_bool) {
    for(Nb_Vec3<OC_REAL8> *vptr=ovfseghead.bdryline.GetFirst();vptr!=NULL;
        vptr=ovfseghead.bdryline.GetNext()) {
      total_range.ExpandWith(*vptr);
    }
  }

  // Determine data normalization value.
  // NOTE: Unless it is really necessary, it seems it is better
  // to not renormalize the data, as that introduces rounding
  // error in the last bit.  So, we disable the renormalization
  // by setting norm to 1.0, but leave the code in place for
  // now in case we find something gets broken. -mjd, 15-Mar-2007
#if 0
  double norm=1.0;
  if(ovfseghead.fileversion == vf_ovf_10) {
    // valuerangemaxmag not supported in OVF 2.0
    norm=ovfseghead.valuerangemaxmag;
    if(norm>OC_SQRT_REAL8_EPSILON) norm=1./norm;
  }
#else
  const double norm = 1.0;
#endif

  // Value units and multiplier.  In OVF 2.0, value units
  // is a list and value multiplier is not supported.  Also,
  // value labels in OVF 2.0 are not supported in OVF 1.0.
  // So, ignore valuelabels_list and kludge the others.
  const char* valunit = "";
  if(ovfseghead.valueunit_bool) {
    valunit = (const char *)ovfseghead.valueunit;
  } else if(ovfseghead.valueunits_list_bool) {
    // Vf_Mesh only supports one valueunits string,
    // so punt and just take the first one.
    Nb_List_Index< Nb_DString > key;
    valunit
      = ovfseghead.valueunits_list.GetFirst(key)->GetStr();
  }
  OC_REAL8 valmult = 1.0;
  if(ovfseghead.valuemultiplier_bool) {
    valmult = ovfseghead.valuemultiplier;
  }
  valmult /= norm;

  // Initialize mesh
  Vf_GridVec3f *mesh_rect=NULL;
  Vf_GeneralMesh3f *mesh_irreg=NULL;
  Vf_Mesh *mesh=NULL;
  if(ovfseghead.meshtype!=vf_oshmt_irregular) {
    // Rectangular grid
    Nb_Vec3<OC_REAL8> basept(OC_REAL8(ovfseghead.xbase),OC_REAL8(ovfseghead.ybase),
                          OC_REAL8(ovfseghead.zbase));
    Nb_Vec3<OC_REAL8> lastpt(OC_REAL8((ovfseghead.xnodes-1)*ovfseghead.xstepsize),
                          OC_REAL8((ovfseghead.ynodes-1)*ovfseghead.ystepsize),
                          OC_REAL8((ovfseghead.znodes-1)*ovfseghead.zstepsize));
    lastpt+=basept;
    total_range.ExpandWith(basept); // Expand to include all datapoints.
    total_range.ExpandWith(lastpt);
    Nb_BoundingBox<OC_REAL8> data_range; data_range.SortAndSet(basept,lastpt);

    Nb_Vec3<OC_REAL8> gridstep(OC_REAL8(ovfseghead.xstepsize),
                            OC_REAL8(ovfseghead.ystepsize),
                            OC_REAL8(ovfseghead.zstepsize));

    Nb_List< Nb_Vec3<OC_REAL8> > *bdryptr=NULL;
    if(ovfseghead.bdryline_bool) {
      bdryptr= &ovfseghead.bdryline;
    }

    mesh_rect=new Vf_GridVec3f((const char *)filename,
                               (const char *)ovfseghead.title,
                               (const char *)ovfseghead.desc,
                               (const char *)ovfseghead.meshunit,
                               valunit,valmult,
                               ovfseghead.xnodes,
                               ovfseghead.ynodes,ovfseghead.znodes,
                               basept,gridstep,
                               data_range,total_range,bdryptr);
    mesh=mesh_rect;
  }
  else {
    // Irregular grid
    mesh_irreg=new Vf_GeneralMesh3f((const char *)filename,
                                    (const char *)ovfseghead.title,
                                    (const char *)ovfseghead.desc,
                                    (const char *)ovfseghead.meshunit,
                                    valunit,valmult);
    if(ovfseghead.bdryline_bool) {
      mesh_irreg->SetBoundaryList(ovfseghead.bdryline);
    }
    mesh_irreg->SetBoundaryRange(total_range);
    if(ovfseghead.xstepsize_bool) {
      // Set step hints
      Nb_Vec3<OC_REAL8> step;
      step.x=OC_REAL8(ovfseghead.xstepsize);
      if(!ovfseghead.ystepsize_bool) step.y=OC_REAL8(ovfseghead.xstepsize);
      else                           step.y=OC_REAL8(ovfseghead.ystepsize);
      if(!ovfseghead.zstepsize_bool) step.z=OC_REAL8(ovfseghead.xstepsize);
      else                           step.z=OC_REAL8(ovfseghead.zstepsize);
      mesh_irreg->SetStepHints(step);
    }
    mesh=mesh_irreg;
  }

  // If binary data, verify check value
  if(datastyle == vf_obin4) {
    OC_REAL4 test=OC_REAL4(0.);
    if(ovfseghead.fileversion == vf_ovf_20) {
      Vf_OvfFileFormatSpecs::ReadBinaryFromLSB(infile,&test,1);
    } else {
      Vf_OvfFileFormatSpecs::ReadBinary(infile,&test,1);
    }
    if(!Vf_OvfFileFormatSpecs::CheckValue(test)) {
      ClassMessage(STDDOC,
            "Invalid data block check value in file %s; Wrong byte order?",
                    (const char *)filename);
      delete mesh;
      return new Vf_EmptyMesh();
    }
  }
  else if(datastyle == vf_obin8) {
    OC_REAL8 test;
    // OVF 2.0 files write binary blocks in LSB (little endian) order.
    // The older formats use MSB (big endian).
    if(ovfseghead.fileversion == vf_ovf_20) {
      Vf_OvfFileFormatSpecs::ReadBinaryFromLSB(infile,&test,1);
    } else {
      Vf_OvfFileFormatSpecs::ReadBinary(infile,&test,1);
    }
    if(!Vf_OvfFileFormatSpecs::CheckValue(test)) {
      ClassMessage(STDDOC,
            "Invalid data block check value in file %s; Wrong byte order?",
                    (const char *)filename);
      delete mesh;
      return new Vf_EmptyMesh();
    }
  }

  // Read data
  int filevaldim = 3;  // Dimension of values in input file.
  if(ovfseghead.valuedim_bool) {
    filevaldim = ovfseghead.valuedim;
  }
  /// Vf_Mesh values are always three dimensional.  Therefore,
  /// support for filevaldim != 3 is a kludge.  If filevaldim>3,
  /// then we fill .x, .y, .z in order with the first three value
  /// indexes from the file, and drop the remainder.  If we wanted,
  /// this could be modified to allow the user to specify which
  /// value indexes to keep.  If filevaldim<3, then we fill the
  /// Vf_Mesh value output in the same .x, .y, .z order, but set
  /// unfilled indexes to 0.0.  This too could be generalized, if
  /// desired.
  if(ovfseghead.meshtype!=vf_oshmt_irregular) {
    // Rectangular grid
    OC_INDEX i,j,k;
    Nb_Vec3<OC_REAL8> data;
    OC_REAL8m baddata; // For debugging
    for(k=0;k<ovfseghead.znodes;k++)
      for(j=0;j<ovfseghead.ynodes;j++)
        for(i=0;i<ovfseghead.xnodes;i++) {
          // Read one entry
          int float_error=0;
          if(ovfseghead.fileversion == vf_ovf_20) {
            float_error = ReadVec3fFromLSB(datastyle,filevaldim,data,&baddata);
          } else {
            float_error = ReadVec3f(datastyle,filevaldim,data,&baddata);
          }
          if(float_error!=0) {
            if(float_error==2) {
              if(datastyle == vf_obin8) {
                unsigned char* cptr=(unsigned char*)(&baddata);
                fprintf(stderr,"Bad data at index (%ld,%ld,%ld)\n",
                        long(i),long(j),long(k));
                fprintf(stderr,"   Bit pattern: "
                        "0x%02X%02X%02X%02X%02X%02X%02X%02X\n",
                        cptr[0],cptr[1],cptr[2],cptr[3],
                        cptr[4],cptr[5],cptr[6],cptr[7]);
                fprintf(stderr,"      FP value: %g\n",
                        static_cast<double>(baddata));
              }
              ClassMessage(STDDOC,
                           "Invalid floating point data detected"
                           " in file %s, at index (%d,%d,%d)",
                           (const char *)filename,i,j,k);
            } else {
              ClassMessage(STDDOC,"Premature EOF in file %s"
                           " in data section",
                           (const char *)filename);
            }
            delete mesh;
            return new Vf_EmptyMesh();
          }
          data*=norm; // Normalize data.
          (*mesh_rect)(i,j,k)=data;
        }
  }
  else {
    // Irregular grid
    OC_INDEX i;
    Nb_LocatedVector<OC_REAL8> lv;
    for(i=0;i<ovfseghead.pointcount;i++) {
      // Read one entry.  Note that location dimension is always 3
      int float_error = 0;
      if(ovfseghead.fileversion == vf_ovf_20) {
        float_error = ReadVec3fFromLSB(datastyle,3,lv.location);
        if(float_error==0) {
          float_error = ReadVec3fFromLSB(datastyle,filevaldim,lv.value);
        }
      } else {
        float_error = ReadVec3f(datastyle,3,lv.location);
        if(float_error==0) {
          float_error = ReadVec3f(datastyle,filevaldim,lv.value);
        }
      }
      if(float_error!=0) {
        if(float_error==2) {
          ClassMessage(STDDOC,
                       "Invalid floating point data detected in file %s",
                       (const char *)filename);
        } else {
          ClassMessage(STDDOC,"Premature EOF in file %s in data section",
                       (const char *)filename);
        }
        delete mesh;
        return new Vf_EmptyMesh();
      }
      lv.value*=norm; // Normalize data.
      mesh_irreg->AddPoint(lv);
    }
    mesh_irreg->SortPoints();
  }

  // Skip over trailing whitespace
  char endmark[2];
  endmark[0]=endmark[1]='\0';
  while(1) {
    int ch=fgetc(infile);
    if(feof(infile)) {
      ClassMessage(STDDOC,"Premature EOF in file %s (no data end marker)",
                   (const char *)filename);
      delete mesh;
      return new Vf_EmptyMesh();
    }
    if(ferror(infile)) {
      ClassMessage(STDDOC,
                   "Error detected in file %s (looking for data end marker)",
                   (const char *)filename);
      delete mesh;
      return new Vf_EmptyMesh();
    }

    if(isspace(ch)) continue; // Skip blank

    // Raise error if we find a binary character
    if(ch<0x20 || ch>0x7e) {
      ClassMessage(STDDOC,"Error in file %s:"
                   " Binary data (%02X) detected past end of data block",
                   (const char *)filename,ch);
      delete mesh;
      return new Vf_EmptyMesh();
    }

    // Otherwise, we have a non-space character.  Save in endmark
    // and break out.
    endmark[0]=(char)ch;
    break;
  }

  // Remainder of current line should be data block end statement
  Nb_DString tempbuf;
  if(ReadLine(tempbuf)!=0) {
    ClassMessage(STDDOC,"Premature EOF in file %s (no data end marker)",
                 (const char *)filename);
    delete mesh;
    return new Vf_EmptyMesh();
  }
  buf=endmark;
  buf.Append(tempbuf);

  ParseContentLine(buf,field,value);
  if(strcmp(field,"end")!=0) {
    ClassMessage(STDDOC,"Error in file %s:"
                 " Data block too long, or end data marker missing.",
                 (const char *)filename);
    delete mesh;
    return new Vf_EmptyMesh();
  }
  if(strcmp(value,datastylestring)!=0) {
    ClassMessage(STDDOC,"Data end marker wrong type in file %s "
                 "(->%s<- instead of ->%s<-)",
                 (const char *)filename,
                 (const char *)value,datastylestring);
    delete mesh;
    return new Vf_EmptyMesh();
  }

  // Skip to segment end
  while(1) {
    if(ReadLine(buf)!=0) {
      ClassMessage(STDDOC,"Premature EOF in file %s (no segment end marker)",
                    (const char *)filename);
      return mesh;
    }
    ParseContentLine(buf,field,value);
    if(strcmp(field,"end")==0  && strcmp(value,"segment")==0) break;
  }

  OC_BOOL hints_set = 0;
  if(ovfseghead.valuerangemaxmag_bool && ovfseghead.valuerangeminmag_bool
     && (ovfseghead.valuerangeminmag != 0.0 ||
         ovfseghead.valuerangemaxmag != 0.0)) {
    // Use hints.  Inside file, valuerange[max|min]mag are scaled
    // relative to the datablock.  In the mesh, the scaling is
    // with respect to the exterior view of the data as
    // "mesh_value*ValueMultiplier".
    OC_REAL8m hint_scale = ovfseghead.valuemultiplier;
    hints_set
      = mesh->SetMagHints(ovfseghead.valuerangeminmag*hint_scale,
                          ovfseghead.valuerangemaxmag*hint_scale);
  }
  if(!hints_set) {
    // Set hint values from data span
    OC_REAL8m minhint,maxhint;
    mesh->GetNonZeroValueMagSpan(minhint,maxhint);
    mesh->SetMagHints(minhint,maxhint);
  }

  // NOTE: *infile is closed by base destructor
  // (Moreover, there may be additional segments in this file.)

  return mesh;
#undef MEMBERNAME
}


///////////////////////////////////////////////////////////////////////////
// Vf_OvfFileOutput Class

const ClassDoc Vf_OvfFileOutput::class_doc("Vf_OvfFileOutput",
              "Michael J. Donahue (michael.donahue@nist.gov)",
              "1.0.1","5-Oct-1997");

OC_INT4m Vf_OvfFileOutput::WriteMesh(Vf_Mesh *mesh,
                                  Tcl_Channel channel,
                                  Vf_OvfDataStyle datastyle,
                                  OC_BOOL force_irreg,
                                  const char *title,
                                  const char *desc)
{ // Returns -1 if mesh type not supported,
  //          0 on success,
  //         >0 on other error.
  // If force_irreg is set, the output will be in the Ovf irregular
  // grid file format (location & value) even if input data lies
  // on a rectangular grid.

  enum { mt_rect, mt_irreg } meshtype=mt_rect;
  OC_INDEX xdim=0,ydim=0,zdim=0,pointcount=0;
  Nb_BoundingBox<OC_REAL8> range;
  Nb_List< Nb_Vec3<OC_REAL8> > bdryline;

  if(strcmp(mesh->GetMeshType(),"Vf_GridVec3f")==0) {
    meshtype=mt_rect;
  } else if(strcmp(mesh->GetMeshType(),"Vf_GeneralMesh3f")==0) {
    meshtype=mt_irreg;
  } else {
    // Unsupported mesh type
    return -1;
  }

  // Downcast mesh
  Vf_GridVec3f *mesh_rect=NULL;
  Vf_GeneralMesh3f *mesh_irreg=NULL;
  try {
    switch(meshtype) {
    case mt_rect:
      mesh_rect=(Vf_GridVec3f *)mesh;
      pointcount=mesh_rect->GetSize();
      if(force_irreg) {
        Nb_FprintfChannel(channel, NULL, 1024,
                          "# OOMMF: irregular mesh v1.0\n");
      } else {
        Nb_FprintfChannel(channel, NULL, 1024,
                          "# OOMMF: rectangular mesh v1.0\n");
      }
      break;
    case mt_irreg:
    default:  // Safety
      mesh_irreg=(Vf_GeneralMesh3f *)mesh;
      pointcount=mesh_irreg->GetSize();
      Nb_FprintfChannel(channel, NULL, 1024,
                        "# OOMMF: irregular mesh v1.0\n");
      break;
    }

    // Write header
    Nb_FprintfChannel(channel, NULL, 1024, "# Segment count: 1\n");
    Nb_FprintfChannel(channel, NULL, 1024, "# Begin: Segment\n");
    Nb_FprintfChannel(channel, NULL, 1024, "# Begin: Header\n");
    if(title==NULL) {
      Nb_FprintfChannel(channel,NULL,1024,"# Title: %s\n",mesh->GetTitle());
    } else {
      Nb_FprintfChannel(channel,NULL,1024,"# Title: %s\n",title);
    }
    if(desc==NULL) desc=mesh->GetDescription();
    if(desc==NULL || desc[0]=='\0') {
      Nb_FprintfChannel(channel, NULL, 1024,
                        "# Desc: Ovf file created by class %s v%s (%s)\n",
                        class_doc.classname,class_doc.revision,
                        class_doc.revdate);
    } else {
      // Print out description, breaking at newlines as necessary.
      const char *cptr1,*cptr2;
      cptr1=desc;
      while((cptr2=strchr(cptr1,'\n'))!=NULL) {
        Nb_FprintfChannel(channel, NULL, 1024,
                          "# Desc: %.*s\n",(int)(cptr2-cptr1),cptr1);
        cptr1=cptr2+1;
      }
      if(*cptr1!='\0') {
        Nb_FprintfChannel(channel, NULL, 1024, "# Desc: %s\n",cptr1);
      }
    }

    if(meshtype==mt_rect && !force_irreg) {
      Nb_Vec3<OC_REAL8> basept=mesh_rect->GetBasePoint();
      Nb_Vec3<OC_REAL8> gridstep=mesh_rect->GetGridStep();
      mesh_rect->GetDimens(xdim,ydim,zdim);
      Nb_FprintfChannel(channel, NULL, 1024, "# meshtype: rectangular\n");
      const char *mu=mesh->GetMeshUnit();
      if(mu!=NULL && mu[0]!='\0') {
        Nb_FprintfChannel(channel, NULL, 1024, "# meshunit: %s\n",mu);
      } else {
        Nb_FprintfChannel(channel, NULL, 1024, "# meshunit: unknown\n");
      }
      Nb_FprintfChannel(channel, NULL, 1024,
                        "# xbase: %.17g\n# ybase: %.17g\n# zbase: %.17g\n",
                        static_cast<double>(basept.x),
                        static_cast<double>(basept.y),
                        static_cast<double>(basept.z));
      Nb_FprintfChannel(channel, NULL, 1024,
                        "# xstepsize: %.17g\n# ystepsize: %.17g\n"
                        "# zstepsize: %.17g\n",
                        static_cast<double>(gridstep.x),
                        static_cast<double>(gridstep.y),
                        static_cast<double>(gridstep.z));
      Nb_FprintfChannel(channel, NULL, 1024,
                        "# xnodes: %ld\n# ynodes: %ld\n# znodes: %ld\n",
                        long(xdim),long(ydim),long(zdim));
    } else {
      Nb_Vec3<OC_REAL8> stephints;
      if(meshtype==mt_irreg)   stephints=mesh_irreg->GetStepHints();
      else if(meshtype==mt_rect) stephints=mesh_rect->GetGridStep();
      Nb_FprintfChannel(channel, NULL, 1024, "# meshtype: irregular\n");
      const char *mu=mesh->GetMeshUnit();
      if(mu!=NULL && mu[0]!='\0') {
        Nb_FprintfChannel(channel, NULL, 1024, "# meshunit: %s\n",mu);
      } else {
        Nb_FprintfChannel(channel, NULL, 1024, "# meshunit: unknown\n");
      }
      Nb_FprintfChannel(channel, NULL, 1024,
                        "# pointcount: %ld\n",long(pointcount));
      Nb_FprintfChannel(channel, NULL, 1024,
                        "# xstepsize: %.17g\n# ystepsize: %.17g\n"
                        "# zstepsize: %.17g\n",
                        static_cast<double>(stephints.x),
                        static_cast<double>(stephints.y),
                        static_cast<double>(stephints.z));
    }

    mesh->GetPreciseRange(range);
    mesh->GetPreciseBoundaryList(bdryline);

    Nb_Vec3<OC_REAL8> minpt,maxpt;
    range.GetExtremes(minpt,maxpt);
    Nb_FprintfChannel(channel, NULL, 1024,
                      "# xmin: %.17g\n# ymin: %.17g\n# zmin: %.17g\n",
                      static_cast<double>(minpt.x),
                      static_cast<double>(minpt.y),
                      static_cast<double>(minpt.z));
    Nb_FprintfChannel(channel, NULL, 1024,
                      "# xmax: %.17g\n# ymax: %.17g\n# zmax: %.17g\n",
                      static_cast<double>(maxpt.x),
                      static_cast<double>(maxpt.y),
                      static_cast<double>(maxpt.z));

    if(!mesh->IsBoundaryFromData()) {
      Nb_FprintfChannel(channel, NULL, 1024, "# boundary:");
      for(Nb_Vec3<OC_REAL8> *v=bdryline.GetFirst();
          v!=NULL;v=bdryline.GetNext()) {
        Nb_FprintfChannel(channel, NULL, 1024,
                          " %.17g %.17g %.17g",
                          static_cast<double>(v->x),
                          static_cast<double>(v->y),
                          static_cast<double>(v->z));
      }
      Nb_FprintfChannel(channel, NULL, 1024, "\n");
    }

    Nb_FprintfChannel(channel, NULL, 1024,
                      "# valueunit: %s\n",mesh->GetValueUnit());
    Nb_FprintfChannel(channel, NULL, 1024,
                      "# valuemultiplier: %.17g\n",
                      static_cast<double>(mesh->GetValueMultiplier()));

    OC_REAL8m minmaghint,maxmaghint;
    mesh->GetMagHints(minmaghint,maxmaghint);
    OC_REAL8m magscaling = mesh->GetValueMultiplier();
    if(magscaling>=1. || maxmaghint<DBL_MAX*magscaling) {
      Nb_FprintfChannel(channel, NULL, 1024,
                        "# ValueRangeMinMag: %.17g\n",
                        static_cast<double>(minmaghint/magscaling));
      Nb_FprintfChannel(channel, NULL, 1024,
                        "# ValueRangeMaxMag: %.17g\n",
                        static_cast<double>(maxmaghint/magscaling));
    } else {
      Nb_FprintfChannel(channel, NULL, 1024,
                        "# ValueRangeMinMag: %.17g\n",1e-8);
      Nb_FprintfChannel(channel, NULL, 1024,
                        "# ValueRangeMaxMag: %.17g\n",1.);
    }

    Nb_FprintfChannel(channel, NULL, 1024, "# End: Header\n");
  } catch(...) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Vf_OvfFileOutput","WriteMesh",
                          "Error writing OVF file header; disk full?"));
  }

  try {
    // Write data header
    if(datastyle==vf_obin4) {
      OC_REAL4 check;
      Vf_OvfFileFormatSpecs::GetCheckValue(check);
      Nb_FprintfChannel(channel, NULL, 1024, "# Begin: Data Binary 4\n");
      if(Vf_OvfFileFormatSpecs::WriteBinary(channel,&check,1)) {
        throw 1; // Dummy throw; real exception thrown in catch(...) block.
      }
    } else if(datastyle==vf_obin8) {
      OC_REAL8 check;
      Vf_OvfFileFormatSpecs::GetCheckValue(check);
      Nb_FprintfChannel(channel, NULL, 1024, "# Begin: Data Binary 8\n");
      if(Vf_OvfFileFormatSpecs::WriteBinary(channel,&check,1)) {
        throw 1; // Dummy throw; real exception thrown in catch(...) block.
      }
    } else if(datastyle==vf_oascii) {
      Nb_FprintfChannel(channel, NULL, 1024, "# Begin: Data Text\n");
    }

    // Write data
    if(meshtype==mt_rect && !force_irreg) {
      // Regular mesh
      OC_INDEX i,j,k;
      Nb_Vec3<OC_REAL8> *v;
      for(k=0;k<zdim;k++) for(j=0;j<ydim;j++) for(i=0;i<xdim;i++) {
        v=&(mesh_rect->GridVec(i,j,k));
        if(datastyle==vf_obin4) {
          OC_REAL4 buf[3];
          buf[0]=static_cast<OC_REAL4>(v->x);
          buf[1]=static_cast<OC_REAL4>(v->y);
          buf[2]=static_cast<OC_REAL4>(v->z);
          if(Vf_OvfFileFormatSpecs::WriteBinary(channel,buf,3)) {
            throw 1; // Dummy throw
          }
        } else if(datastyle==vf_obin8) {
          OC_REAL8 buf[6];
          buf[0]=v->x; buf[1]=v->y; buf[2]=v->z;
          if(Vf_OvfFileFormatSpecs::WriteBinary(channel,buf,3)) {
            throw 1; // Dummy throw
          }
        } else if(datastyle==vf_oascii) {
          Nb_FprintfChannel(channel, NULL, 1024,
                            "%# .17g %# .17g %# .17g\n",
                            static_cast<double>(v->x),
                            static_cast<double>(v->y),
                            static_cast<double>(v->z));
        }
      }
    } else {
      // Irregular mesh
      Nb_LocatedVector<OC_REAL8> lv;
      Vf_Mesh_Index* key;
      for(OC_INDEX index_code=mesh->GetFirstPt(key,lv);index_code;
          index_code=mesh->GetNextPt(key,lv)) {
        if(datastyle==vf_obin4) {
          OC_REAL4 buf[6];
          buf[0] = OC_REAL4(lv.location.x);
          buf[1] = OC_REAL4(lv.location.y);
          buf[2] = OC_REAL4(lv.location.z);
          buf[3] = OC_REAL4(lv.value.x);
          buf[4] = OC_REAL4(lv.value.y);
          buf[5] = OC_REAL4(lv.value.z);
          if(Vf_OvfFileFormatSpecs::WriteBinary(channel,buf,6)) {
            throw 1; // Dummy throw
          }
        } else if(datastyle==vf_obin8) {
          OC_REAL8 buf[6];
          buf[0] = lv.location.x;
          buf[1] = lv.location.y;
          buf[2] = lv.location.z;
          buf[3] = lv.value.x;
          buf[4] = lv.value.y;
          buf[5] = lv.value.z;
          if(Vf_OvfFileFormatSpecs::WriteBinary(channel,buf,6)) {
            throw 1; // Dummy throw
          }
        } else if(datastyle==vf_oascii) {
          Nb_FprintfChannel(channel, NULL, 1024,
             "%# .17g %# .17g %# .17g %# .17g %# .17g %# .17g\n",
             static_cast<double>(lv.location.x),
             static_cast<double>(lv.location.y),
             static_cast<double>(lv.location.z),
             static_cast<double>(lv.value.x),
             static_cast<double>(lv.value.y),
             static_cast<double>(lv.value.z));
        }
      }
      delete key;
    }

    // Write trailer
    if(datastyle==vf_obin4) {
      Nb_FprintfChannel(channel, NULL, 1024, "\n# End: Data Binary 4\n");
    } else if(datastyle==vf_obin8)  {
      Nb_FprintfChannel(channel, NULL, 1024, "\n# End: Data Binary 8\n");
    } else if(datastyle==vf_oascii) {
      Nb_FprintfChannel(channel, NULL, 1024, "# End: Data Text\n");
    }
    Nb_FprintfChannel(channel, NULL, 1024, "# End: Segment\n");
  } catch(...) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Vf_OvfFileOutput","WriteMesh",
                          "Error writing OVF file data block; disk full?"));
  }

  return 0;
}

///////////////////////////////////////////////////////////////////////////
// Vf_VioFileOutput Class

const ClassDoc Vf_VioFileOutput::class_doc("Vf_VioFileOutput",
              "Michael J. Donahue (michael.donahue@nist.gov)",
              "1.0.0","25-Feb-1998");

OC_INT4m Vf_VioFileOutput::WriteMesh(Vf_Mesh *mesh,Tcl_Channel channel,
                                  const char *desc)
{ // Returns -1 if mesh type not supported,
  //          0 on success,
  //         >0 on other error.
#define MEMBERNAME "WriteMesh"

  OC_INDEX i,j,xdim,ydim,zdim;

  if(strcmp(mesh->GetMeshType(),"Vf_GridVec3f")!=0) {
    // Unsupported mesh type
    return -1;
  }

  // Downcast mesh
  Vf_GridVec3f& mesh_rect=(Vf_GridVec3f&)(*mesh);
  mesh_rect.GetDimens(xdim,ydim,zdim);
  if(zdim!=1) {
    // Unsupported mesh type
    return -1;
  }

  // Write header
  struct VioFileHeader {
    OC_BYTE    Magic[8];
    OC_REAL8   EndianTest;
    OC_INT4    Nx,Ny,Nz,dimen,dataoffset;
    OC_BYTE    Reserved[36];
    OC_BYTE    Note[448];
  };

  VioFileHeader fh;
  fh.Magic[0]='\0';    // Safety
  fh.Reserved[0]='\0'; // Safety
  fh.Note[0]='\0';     // Safety
  strncpy((char *)fh.Magic,"VecFil\x02",sizeof(fh.Magic));
  fh.EndianTest = 1.234;

  // Overflow check
  const OC_INT4 CHECKMAX = (~OC_UINT4(0))>>1;
  if(xdim>CHECKMAX || ydim>CHECKMAX || zdim>CHECKMAX) {
    ClassMessage(STDDOC,
                 "Overflow: Spatial dimension doesn't fit into OC_INT4");
    return -1;
  }

  fh.Nx=static_cast<OC_INT4>(xdim);
  fh.Nz=static_cast<OC_INT4>(ydim);
  fh.Ny=static_cast<OC_INT4>(zdim);  // Note swap of y & z
  fh.dimen=3;
  fh.dataoffset=sizeof(fh);

  for(i=0;static_cast<size_t>(i)<sizeof(fh.Reserved);i++) fh.Reserved[i]='\0';
  for(i=0;static_cast<size_t>(i)<sizeof(fh.Note);i++)     fh.Note[i]='\0';
  if(desc!=NULL) strncpy((char *)fh.Note,desc,sizeof(fh.Note)-1);

  if(Nb_WriteChannel(channel,(char *)(&fh),OC_INDEX(sizeof(fh))) != sizeof(fh))
    return 1;

  // Write data
  OC_REAL8 arr[3];
  for(i=0;i<xdim;i++) for(j=0;j<ydim;j++) {
    Nb_Vec3<OC_REAL8>& v=mesh_rect(i,j,0);
    OC_INDEX bytecount = 3 * sizeof(arr[0]);
    arr[0]=v.x; arr[1]=-v.z; arr[2]=v.y;  // Change coord system
    if(Nb_WriteChannel(channel, (char *)arr, bytecount) != bytecount)
      return 1;
  }

  return 0;
#undef MEMBERNAME
}

///////////////////////////////////////////////////////////////////////////
// Vf_VioFileInput Class

const ClassDoc Vf_VioFileInput::class_doc("Vf_VioFileInput",
              "Michael J. Donahue (michael.donahue@nist.gov)",
              "1.0.0","19-Mar-1997");

const char* Vf_VioFileInput::file_type="VIO (.vio) Input File";

const Vf_FileInputID Vf_VioFileInput::id(Vf_VioFileInput::file_type,
                                         Vf_VioFileInput::IsType,
                                         Vf_VioFileInput::MakeNew,
                                         Vf_FileInput::RegisterChild);

Vf_VioFileInput::Vf_VioFileInput(const char *new_filename,
                                 FILE *new_infile,
                                 Tcl_Channel new_channel,
                                 const char *new_channelName)
  : Vf_FileInput(new_filename,new_infile,new_channel,new_channelName)
{
#define MEMBERNAME "Vf_VioFileInput"
  // Read header here
#undef MEMBERNAME
}

Vf_FileInput* Vf_VioFileInput::MakeNew(const char *new_filename,
                                       FILE *new_infile,
                                       Tcl_Channel new_channel,
                                       const char *new_channelName)
{
  return new Vf_VioFileInput(new_filename,new_infile,
                             new_channel,new_channelName);
}

OC_BOOL Vf_VioFileInput::IsType(const char *,
                             FILE *test_fptr,
                             Tcl_Channel,
                             const char *)
{
#define MEMBERNAME "IsType"
  const char *VecFileMagic02 = "VecFil\x02";
  char buf[8];
  if(fread(buf,1,8,test_fptr)==8 
     && memcmp(VecFileMagic02,buf,8)==0) return 1;  // Match!
  return 0;
#undef MEMBERNAME
}

Vf_Mesh *Vf_VioFileInput::NewMesh()
{
#define MEMBERNAME "NewMesh"
  Vf_Mesh *newmesh=NewRectGrid();
  if(newmesh==NULL) newmesh=new Vf_EmptyMesh();
  return newmesh;
#undef MEMBERNAME
}

Vf_GridVec3f *Vf_VioFileInput::NewRectGrid()
{
#define MEMBERNAME "NewRectGrid"
  Vf_VioFile viofile;

  // Open file and get grid dimensions
  if(viofile.SetFilePtr(infile)!=0) return NULL;
  OC_INDEX xsize,ysize,zsize;
  viofile.GetDimensions(xsize,ysize,zsize);
  // Set bounding box and boundary list
  // Note: Vio files sample at cell centers
  Nb_BoundingBox<OC_REAL8> data_range,total_range;
  data_range.Set(Nb_Vec3<OC_REAL8>(OC_REAL8(0.),OC_REAL8(0.),OC_REAL8(0.)),
                 Nb_Vec3<OC_REAL8>(OC_REAL8(xsize-1.),
                                   OC_REAL8(ysize-1.),OC_REAL8(zsize-1.)));
  total_range.Set(Nb_Vec3<OC_REAL8>(OC_REAL8(-0.5),
                                    OC_REAL8(-0.5),OC_REAL8(-0.5)),
                  Nb_Vec3<OC_REAL8>(OC_REAL8(xsize-0.5),
                                    OC_REAL8(ysize-0.5),OC_REAL8(zsize-0.5)));

  // Initialize mesh
  Vf_GridVec3f *mesh=new Vf_GridVec3f(filename,filename,"vio file",
                              NULL,NULL,1.0,xsize,ysize,zsize,
                              Nb_Vec3<OC_REAL8>(OC_REAL8(0.),
                                                OC_REAL8(0.),OC_REAL8(0.)),
                              Nb_Vec3<OC_REAL8>(OC_REAL8(1.),
                                                OC_REAL8(1.),OC_REAL8(1.)),
                              data_range,total_range,NULL);

  // Fill grid
  if(viofile.FillArray(*mesh)!=0) {
    delete mesh;
    return NULL;
  }

  // Set hint values from data span
  OC_REAL8m minhint,maxhint;
  mesh->GetValueMagSpan(minhint,maxhint);
  mesh->SetMagHints(minhint,maxhint);

  // NOTE: *infile is closed by base destructor

  return mesh;
#undef MEMBERNAME
}

///////////////////////////////////////////////////////////////////////////
// Vf_SvfFileInput Class

const ClassDoc Vf_SvfFileInput::class_doc("Vf_SvfFileInput",
              "Michael J. Donahue (michael.donahue@nist.gov)",
              "1.0.0","30-Aug-1997");

const char* Vf_SvfFileInput::file_type="Svf (.svf) Input File";

const Vf_FileInputID Vf_SvfFileInput::id(Vf_SvfFileInput::file_type,
                                         Vf_SvfFileInput::IsType,
                                         Vf_SvfFileInput::MakeNew,
                                         Vf_FileInput::RegisterChild);

Vf_SvfFileInput::Vf_SvfFileInput(const char *new_filename,
                                 FILE *new_infile,
                                 Tcl_Channel new_channel,
                                 const char *new_channelName)
  : Vf_FileInput(new_filename,new_infile,
                 new_channel,new_channelName)
{
#define MEMBERNAME "Vf_SvfFileInput"
#undef MEMBERNAME
}

Vf_SvfFileInput::~Vf_SvfFileInput()
{
#define MEMBERNAME "~Vf_SvfFileInput"
#undef MEMBERNAME
}

Vf_FileInput* Vf_SvfFileInput::MakeNew(const char *new_filename,
                                       FILE *new_infile,
                                       Tcl_Channel new_channel,
                                       const char *new_channelName)
{
#define MEMBERNAME "MakeNew"
  return new Vf_SvfFileInput(new_filename,new_infile,
                          new_channel,new_channelName);
#undef MEMBERNAME
}

OC_BOOL Vf_SvfFileInput::IsType(const char * test_filename,
                          FILE *,
                          Tcl_Channel,
                          const char *test_channelName)
{
#define MEMBERNAME "IsType"
  const char* CheckProcName="Vf_SvfIsType";
  char *cmd=new char[strlen(CheckProcName)+strlen(test_filename)
              +strlen(test_channelName)+3+16]; // 16 for safety
  sprintf(cmd,"%s [list %s] [list %s]",
          CheckProcName,test_filename,test_channelName);
  Tcl_Interp* interp=Oc_GlobalInterpreter();
  if(interp==NULL)
    FatalError(-1,STDDOC,"Tcl interpretor not initialized");
  Tcl_SavedResult saved;
  Tcl_SaveResult(interp, &saved);
  int return_code=Tcl_Eval(interp,cmd);
  if(return_code!=TCL_OK) NonFatalError(STDDOC,"Error in Tcl proc %s: %s",
                                        cmd,Tcl_GetStringResult(interp));
  delete[] cmd;
  int rescode = atoi(Tcl_GetStringResult(interp));
  Tcl_RestoreResult(interp, &saved);
  if(rescode != 0) return 1;
  return 0;
#undef MEMBERNAME
}

Vf_Mesh *Vf_SvfFileInput::NewMesh()
{
#define MEMBERNAME "NewMesh"
  // Initialize mesh, using filename as default title
  mesh=new Vf_GeneralMesh3f(filename,filename);
  mesh->ResetMesh();

  Tcl_Interp* interp=Oc_GlobalInterpreter();
  if(interp==NULL)
    FatalError(-1,STDDOC,"Tcl interpretor not initialized");

  // If needed, register Vf_SvfFileInputTA with interpreter.
  Tcl_CmdInfo info;
  Tcl_SavedResult saved;
  Tcl_SaveResult(interp, &saved);
  if(!Tcl_GetCommandInfo(interp,
                         OC_CONST84_CHAR("Vf_SvfFileInputTA"),&info)) {
    // Command not registered
    Oc_RegisterCommand(interp,"Vf_SvfFileInputTA",
                       Vf_SvfFileInputTA);
  }

  // Call Tcl proc to read file and stick data into mesh
  const char* ReadProcName="Vf_SvfRead";

  char *ascii_this=new char[Omf_AsciiPtr::GetAsciiSize()];
  Omf_AsciiPtr::PtrToAscii(this,ascii_this);
  /// ascii_this is an Ascii-ified
  size_t cmdlen=strlen(ReadProcName)+strlen(channelName)
                     +strlen(ascii_this)+3+16;  // 16 for safety
  char *cmd=new char[cmdlen+1];
  sprintf(cmd,"%s %s %s",ReadProcName,channelName,ascii_this);
  delete[] ascii_this;
  cmd[cmdlen]='\0';  // Safety
  int return_code=Tcl_Eval(interp,cmd);
  if(return_code!=TCL_OK) {
    NonFatalError(STDDOC,"Error in Tcl proc \"%s\": %s",
                  cmd,Tcl_GetStringResult(interp));
    Tcl_RestoreResult(interp, &saved);
    delete[] cmd;
    delete mesh;
    return new Vf_EmptyMesh();
  }
  Tcl_RestoreResult(interp, &saved);

  if(mesh->GetSize()<1) {
    ClassMessage(STDDOC,"No valid data in file %s",
                  (const char *)filename);
    delete[] cmd;
    delete mesh;
    return new Vf_EmptyMesh();

  }
  delete[] cmd;

  // With respect to boundary specification, SVF files currently
  // (6-2001) only support Boundary-XY, which leaves the z-coordinate
  // unspecified.  So check to see if a boundary has been set, and if
  // so adjust its z-value to be in the middle of the data range.
  if(!mesh->IsBoundaryFromData()) {
    Nb_BoundingBox<OC_REAL8> datarange;
    mesh->GetPreciseDataRange(datarange);
    Nb_Vec3<OC_REAL8> minpt,maxpt;
    datarange.GetExtremes(minpt,maxpt);
    OC_REAL8m zmid=(minpt.z+maxpt.z)/2.0;

    Nb_List< Nb_Vec3<OC_REAL8> > bdry;
    mesh->GetPreciseBoundaryList(bdry);

    Nb_Vec3<OC_REAL8> *vptr;
    Nb_List_Index< Nb_Vec3<OC_REAL8> > key;
    for(vptr=bdry.GetFirst(key);vptr!=NULL;vptr=bdry.GetNext(key)) {
      vptr->z=zmid;
    }

    mesh->SetBoundaryList(bdry);
  }

  // Do post-import mesh processing
  mesh->SortPoints();

  // Set hint values from data span
  OC_REAL8m minhint,maxhint;
  mesh->GetValueMagSpan(minhint,maxhint);
  mesh->SetMagHints(minhint,maxhint);

  // All done!
  return (Vf_Mesh *)mesh;
#undef MEMBERNAME
}

// Routine registered with Tcl interpreter for accessing
// Vf_SvfFileInput functions from Tcl scripts.
int Vf_SvfFileInputTA(ClientData,Tcl_Interp *interp,
                       int argc,CONST84 char** argv)
{
  static char buf[SCRATCH_BUF_SIZE];
  Tcl_ResetResult(interp);
  if(argc<3) {
    sprintf(buf,"Vf_SvfFileInputTA must be called with"
            " at least 2 arguments: <subcommand> <sfi_id> [parameters]"
            " (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  Vf_SvfFileInput *sfip=(Vf_SvfFileInput *)Omf_AsciiPtr::AsciiToPtr(argv[2]);
  if(sfip==NULL || strcmp(sfip->class_doc.classname,"Vf_SvfFileInput")!=0) {
    sprintf(buf,"Sfip (%s -> %p) invalid ptr handle",argv[2],sfip);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  // Switch based on sub-command parameter.  Each if-block fills
  // the result as appropriate, and returns TCL_OK from
  // inside the block.  The fall through case at the bottom
  // handles undefined sub-commands (and returns TCL_ERROR).

  if(strcmp(argv[1],"processline")==0) {          // Process line
    // The processline subcommand returns as the result a "1"
    // on a valid data line (and modifies the import string), a "0"
    // on a comment line (and leaves the import string unchanged),
    // and "-1" otherwise.  In the last case the import string will
    // most likely be altered, as in the valid data case.  At the
    // C level, TCL_OK is returned in all 3 cases.
    //  If line is a valid data line, then the input is passed into
    // the mesh, in the same manner as the addpoint subcommand.
    if(argc!=4) {
      sprintf(buf,"processline subcommand requires 2 parameters:"
              " mesh_id line; %d passed",argc-2);
      Tcl_AppendResult(interp,buf,(char *)NULL);
      return TCL_ERROR;
    }

    // See if this is a data line or a comment
    Tcl_DString copy;
    Tcl_DStringInit(&copy);
    Tcl_DStringAppend(&copy, argv[3], -1);
    char* line_ptr=Tcl_DStringValue(&copy);
    char* data_ptr=line_ptr+strspn(line_ptr," \t");
    /// Skip leading whitespace
    if(*data_ptr=='#') { // Comment line
      Tcl_AppendResult(interp,"0",(char *)NULL);
      return TCL_OK;
    }
    // Work under the assumption that this is a valid data line.
    OC_REAL8m data[6];
    int i=0;
    char* nexttoken=data_ptr;
    data_ptr=Nb_StrSep(&nexttoken," \t");
    while(i<6 && data_ptr!=NULL && *data_ptr!='#') {
      data[i++]=Nb_Atof(data_ptr);
      data_ptr=Nb_StrSep(&nexttoken," \t");
    }
    Tcl_DStringFree(&copy);
    if(i<5) {
      // Bad data line
      Tcl_AppendResult(interp,"-1",(char *)NULL);
      return TCL_OK;
    }

    // Apparently a valid line.  Copy the 5 or 6 values into a
    // LocatedVector and stick it into mesh.  These should be in the
    // order "x y z mx my mz", but some older files (version Svf-01)
    // may have only "x y mx my mz".
    Nb_LocatedVector<OC_REAL8> lv;
    if(i==5) {
      lv.location.Set(OC_REAL8(data[0]),OC_REAL8(data[1]),OC_REAL8(0.0));
      lv.value.Set(OC_REAL8(data[2]),OC_REAL8(data[3]),OC_REAL8(data[4]));
    }
    else {
      lv.location.Set(OC_REAL8(data[0]),OC_REAL8(data[1]),OC_REAL8(data[2]));
      lv.value.Set(OC_REAL8(data[3]),OC_REAL8(data[4]),OC_REAL8(data[5]));
    }
    sfip->mesh->AddPoint(lv);
    Tcl_AppendResult(interp,"1",(char *)NULL);
    return TCL_OK;
  }
  else if(strcmp(argv[1],"addpoint")==0) {        // Add Point
    if(argc!=9) {
      sprintf(buf,"addpoint subcommand requires 7 parameters:"
              " mesh_id x y z mx my mz; %d passed",argc-2);
      Tcl_AppendResult(interp,buf,(char *)NULL);
      return TCL_ERROR;
    }

    Nb_LocatedVector<OC_REAL8> lv;
    lv.location.x=OC_REAL8(Nb_Atof(argv[3]));
    lv.location.y=OC_REAL8(Nb_Atof(argv[4]));
    lv.location.z=OC_REAL8(Nb_Atof(argv[5]));
    lv.value.x=OC_REAL8(Nb_Atof(argv[6]));
    lv.value.y=OC_REAL8(Nb_Atof(argv[7]));
    lv.value.z=OC_REAL8(Nb_Atof(argv[8]));

    // Enter data into mesh
    sfip->mesh->AddPoint(lv);
    return TCL_OK;
  }
  else if(strcmp(argv[1],"getpointcount")==0) {   // Get Point Count
    if(argc!=3) {
      sprintf(buf,"getpointcount subcommand requires 1 parameter:"
              " mesh_id ; %d passed",argc-2);
      Tcl_AppendResult(interp,buf,(char *)NULL);
      return TCL_ERROR;
    }
    // Query mesh
    sprintf(buf,"%ld",long(sfip->mesh->GetSize()));
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_OK;
  }
  else if(strcmp(argv[1],"setbdry")==0) {         // Set Boundary
    Nb_List< Nb_Vec3<OC_REAL8> > boundary_list;
    if(argc>=4) { // Non-blank list
      char *pointstr=Tcl_Concat(argc-3,argv+3);
      Nb_Vec3<OC_REAL8> v;  char* cptr;
      char* nexttoken=pointstr;
      cptr=Nb_StrSep(&nexttoken," \t\n\r\f");
      while(cptr!=NULL) {
        v.x=OC_REAL8(Nb_Atof(cptr));
        cptr=Nb_StrSep(&nexttoken," \t\n\r\f");
        if(cptr!=NULL) {
          v.y=OC_REAL8(Nb_Atof(cptr));
          v.z=OC_REAL8(0.0);
          boundary_list.Append(v); // Add point to list
        }
        else {
          Tcl_Free(pointstr);
          sprintf(buf,"setbdry subcommand requires x y pairs;"
                  " %" OC_INDEX_MOD "d values passed.",
                  2*boundary_list.GetSize()+1);
          Tcl_AppendResult(interp,buf,(char *)NULL);
          return TCL_ERROR;
        }
        cptr=Nb_StrSep(&nexttoken," \t\n\r\f");
      }
      Tcl_Free(pointstr);
    }
    sfip->mesh->SetBoundaryList(boundary_list);
    return TCL_OK;
  }
  else if(strcmp(argv[1],"stephints")==0) {       // Grid step hints
    if(argc<4 || argc>6) {
      sprintf(buf,"addpoint subcommand requires 2-4 parameters:"
              " mesh_id xstep [ystep [zstep]];  %d passed",argc-2);
      Tcl_AppendResult(interp,buf,(char *)NULL);
      return TCL_ERROR;
    }
    Nb_List< Nb_Vec3<OC_REAL8> > step_list;
    char *pointstr=Tcl_Concat(argc-3,argv+3);
    Nb_Vec3<OC_REAL8> step(OC_REAL8(0.),OC_REAL8(0.),OC_REAL8(0.));  char* cptr;
    char* nexttoken=pointstr;
    cptr=Nb_StrSep(&nexttoken," \t\n\r\f");
    if(cptr!=NULL) {
      step.x=OC_REAL8(Nb_Atof(cptr));
      cptr=Nb_StrSep(&nexttoken," \t\n\r\f");
      if(cptr==NULL) { step.y=step.z=step.x; }
      else {
        step.y=OC_REAL8(Nb_Atof(cptr));
        cptr=Nb_StrSep(&nexttoken," \t\n\r\f");
        if(cptr!=NULL) {
          step.z=OC_REAL8(Nb_Atof(cptr));
        }
      }
    }
    Tcl_Free(pointstr);
    sfip->mesh->SetStepHints(step);
    return TCL_OK;
  }
  else if(strcmp(argv[1],"settitle")==0) {       // Set title
    if(argc!=4) {
      sprintf(buf,"settitle subcommand requires 2 parameters:"
              " mesh_id title;  %d passed",argc-2);
      Tcl_AppendResult(interp,buf,(char *)NULL);
      return TCL_ERROR;
    }
    sfip->mesh->SetMeshTitle(argv[3]);
    return TCL_OK;
  }

  // If we reach here, we didn't get a valid command!
  sprintf(buf,"Illegal subcommand (%s); Should be"
          " \"processline\", \"addpoint\", \"getpointcount\","
          " \"setbdry\",  or \"stephints\"",
          argv[1]);
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_ERROR;
}

////////////////////////////////////////////////////////////////////////
// Classes for OVF 2.0

////////////////////////////////////////////////////////////////////////
// Vf_Ovf20FileHeader

OC_BOOL Vf_Ovf20FileHeader::IsValid() const
{
  OC_BOOL result = 1;

  // File version set?
  if(ovfversion == vf_ovf_invalid) return 0;

  // Check that all fields required for all files are set
  result = result && title.IsSet() && meshtype.IsSet() && meshunit.IsSet()
     && xmin.IsSet() && ymin.IsSet() && zmin.IsSet()
     && xmax.IsSet() && ymax.IsSet() && zmax.IsSet()
     && valuedim.IsSet() && valuelabels.IsSet() && valueunits.IsSet();
  if(!result) return 0;

  // Check mesh type specific fields
  if(meshtype.Value() == vf_ovf20mesh_rectangular) {
    result = result && xbase.IsSet() && ybase.IsSet() && zbase.IsSet()
      && xnodes.IsSet() && ynodes.IsSet() && znodes.IsSet()
      && xstepsize.IsSet() && ystepsize.IsSet() && zstepsize.IsSet()
      && !pointcount.IsSet();
  } else if(meshtype.Value() == vf_ovf20mesh_irregular) {
    result = result && pointcount.IsSet()
      && !xbase.IsSet() && !ybase.IsSet() && !zbase.IsSet()
      && !xnodes.IsSet() && !ynodes.IsSet() && !znodes.IsSet();
  } else {
    result = 0;
  }
  if(!result) return 0;

  // Sanity checks
  if(valuedim.Value()<1) {
    result = 0;
  } else {
    if(valuelabels.Value().size()!=1 &&
       valuelabels.Value().size() != static_cast<size_t>(valuedim.Value())) {
      result = 0;
    }
    if(valueunits.Value().size()!=1 &&
       valueunits.Value().size() != static_cast<size_t>(valuedim.Value())) {
      result = 0;
    }
  }

  if(pointcount.IsSet() && pointcount.Value()<0) result = 0;
  if(xnodes.IsSet() && xnodes.Value()<0) result = 0;
  if(ynodes.IsSet() && ynodes.Value()<0) result = 0;
  if(znodes.IsSet() && znodes.Value()<0) result = 0;
  if(xstepsize.IsSet() && xstepsize.Value()<0.) result = 0;
  if(ystepsize.IsSet() && ystepsize.Value()<0.) result = 0;
  if(zstepsize.IsSet() && zstepsize.Value()<0.) result = 0;

  return result;
}

OC_BOOL Vf_Ovf20FileHeader::IsValid(String& errmsg) const
{
  errmsg.erase();  // Some older C++ compilers don't know
  /// the newer .clear() member function.

  // File version set?
  if(ovfversion == vf_ovf_invalid) {
    errmsg = String("Bad version");
    return 0;
  }

  // Check that all fields required for all files are set
  if(!title.IsSet()) errmsg += String(" No title :");
  if(!meshtype.IsSet()) errmsg += String(" No mesh type :");
  if(!meshunit.IsSet()) errmsg += String(" No mesh unit :");
  if(!xmin.IsSet()) errmsg += String(" xmin not set :");
  if(!ymin.IsSet()) errmsg += String(" ymin not set :");
  if(!zmin.IsSet()) errmsg += String(" zmin not set :");
  if(!xmax.IsSet()) errmsg += String(" xmax not set :");
  if(!ymax.IsSet()) errmsg += String(" ymax not set :");
  if(!zmax.IsSet()) errmsg += String(" zmax not set :");
  if(!valuedim.IsSet()) errmsg += String(" valuedim not set :");
  if(!valuelabels.IsSet()) String(" valuelabels not set :");
  if(!valueunits.IsSet()) errmsg += String(" valueunits not set :");
  if(errmsg.size()>0) return 0;

  // Check mesh type specific fields
  if(meshtype.Value() == vf_ovf20mesh_rectangular) {
    if(!xbase.IsSet()) errmsg += String(" xbase not set :");
    if(!ybase.IsSet()) errmsg += String(" ybase not set :");
    if(!zbase.IsSet()) errmsg += String(" zbase not set :");
    if(!xnodes.IsSet()) errmsg += String(" xnodes not set :");
    if(!ynodes.IsSet()) errmsg += String(" ynodes not set :");
    if(!znodes.IsSet()) errmsg += String(" znodes not set :");
    if(!xstepsize.IsSet()) errmsg += String(" xstepsize not set :");
    if(!ystepsize.IsSet()) errmsg += String(" ystepsize not set :");
    if(!zstepsize.IsSet()) errmsg += String(" zstepsize not set :");
    if(pointcount.IsSet()) {
      errmsg += String(" pointcount set for rectangular mesh :");
    }
  } else if(meshtype.Value() == vf_ovf20mesh_irregular) {
    if(xbase.IsSet()) errmsg += String(" xbase set for irregular mesh :");
    if(ybase.IsSet()) errmsg += String(" ybase set for irregular mesh :");
    if(zbase.IsSet()) errmsg += String(" zbase set for irregular mesh :");
    if(xnodes.IsSet()) errmsg += String(" xnodes set for irregular mesh :");
    if(ynodes.IsSet()) errmsg += String(" ynodes set for irregular mesh :");
    if(znodes.IsSet()) errmsg += String(" znodes set for irregular mesh :");
    if(!pointcount.IsSet()) errmsg += String(" pointcount not set :");
  } else {
    errmsg += String("Invalid meshtype");
  }
  if(errmsg.size()>0) return 0;

  // Sanity checks
  if(valuedim.Value()<1) {
    errmsg += String(" valuedim<1 :");
  } else {
    if(valuelabels.Value().size()!=1 &&
       valuelabels.Value().size() != static_cast<size_t>(valuedim.Value())) {
      errmsg += String(" valuelabels length doesn't match valuedim :");
    }
    if(valueunits.Value().size()!=1 &&
       valueunits.Value().size() != static_cast<size_t>(valuedim.Value())) {
      errmsg += String(" valueunits length doesn't match valuedim :");
    }
  }

  if(pointcount.IsSet() && pointcount.Value()<0) {
    errmsg += String(" pointcount < 0 :");
  }
  if(xnodes.IsSet() && xnodes.Value()<0) errmsg += String(" xnodes < 0 :");
  if(ynodes.IsSet() && ynodes.Value()<0) errmsg += String(" ynodes < 0 :");
  if(znodes.IsSet() && znodes.Value()<0) errmsg += String(" znodes < 0 :");
  if(xstepsize.IsSet() && xstepsize.Value()<0.) errmsg += String(" xstepsize < 0 :");
  if(ystepsize.IsSet() && ystepsize.Value()<0.) errmsg += String(" ystepsize < 0 :");
  if(zstepsize.IsSet() && zstepsize.Value()<0.) errmsg += String(" zstepsize < 0 :");

  if(errmsg.size()>0) return 0;
  return 1;
}

OC_BOOL Vf_Ovf20FileHeader::IsValidGeom() const
{
  OC_BOOL result = 1;

  // Check that all fields required for all files are set
  result = result && meshtype.IsSet() && meshunit.IsSet()
    && xmin.IsSet() && ymin.IsSet() && zmin.IsSet()
    && xmax.IsSet() && ymax.IsSet() && zmax.IsSet();
  if(!result) return 0;

  // Check mesh type specific fields
  if(meshtype.Value() == vf_ovf20mesh_rectangular) {
    result = result && xbase.IsSet() && ybase.IsSet() && zbase.IsSet()
      && xnodes.IsSet() && ynodes.IsSet() && znodes.IsSet()
      && xstepsize.IsSet() && ystepsize.IsSet() && zstepsize.IsSet()
      && !pointcount.IsSet();
  } else if(meshtype.Value() == vf_ovf20mesh_irregular) {
    result = result && pointcount.IsSet()
      && !xbase.IsSet() && !ybase.IsSet() && !zbase.IsSet()
      && !xnodes.IsSet() && !ynodes.IsSet() && !znodes.IsSet();
  } else {
    result = 0;
  }
  if(!result) return 0;

  // Sanity checks
  if(pointcount.IsSet() && pointcount.Value()<0) result = 0;
  if(xnodes.IsSet() && xnodes.Value()<0) result = 0;
  if(ynodes.IsSet() && ynodes.Value()<0) result = 0;
  if(znodes.IsSet() && znodes.Value()<0) result = 0;
  if(xstepsize.IsSet() && xstepsize.Value()<0.) result = 0;
  if(ystepsize.IsSet() && ystepsize.Value()<0.) result = 0;
  if(zstepsize.IsSet() && zstepsize.Value()<0.) result = 0;

  return result;
}

void Vf_Ovf20FileHeader::CollapseHeaderLine(char* line) const
{ // Strips spaces and convert to lowercase
  char* cptr1;
  const char* cptr2;
  cptr2 = cptr1 = line;
  while(*cptr2 != '\0') {
    if(!isspace(*cptr2)) *(cptr1++) = (char)tolower(*cptr2);
    ++cptr2;
  }
  *cptr1='\0';
}

int
Vf_Ovf20FileHeader::ParseHeaderLine
(char* line,
 String& field,
 String& value) const
{ // Returns 0 on success, 1 on error.  If the line is a comment or
  // empty line, then field.size() will be 0.  "field" value is
  // converted to lowercase, and all non-alphanumeric characters are
  // removed.  Value string has outer whitespace removed.  As a
  // convenience, if field is "begin" or "end", then the value string
  // is converted to lowercase.
  //
  // Note 1: Import line is used as workspace, and will likely
  //    be modified by this routine.
  //
  // Note 2: This code is mostly cribbed from
  //    Vf_OvfFileInput::ParseContentLine
  //                     (const Nb_DString &buf,
  //                      Nb_DString &field,Nb_DString &value);
  //
  field="";  value="";  // Default return values

  // Tokenize workspace
  char *fieldstart=strchr(line,'#');
  if(fieldstart==NULL) return 0;  // Possibly illegal line, but assume blank
  fieldstart++;
  if(*fieldstart=='#') return 0;  // Comment line; treat as blank
  char *valuestart=strchr(fieldstart,':');
  if(valuestart==NULL) return 0;  // Possibly illegal line, but assume blank
  *valuestart='\0'; valuestart++;

  // Collapse field string (remove all non alpha-numeric characters) and
  // convert to lowercase.
  char *cptr1,*cptr2;
  cptr1=cptr2=fieldstart;
  while(*cptr2!='\0') {
    if(isalnum(*cptr2)) *(cptr1++) = (char)tolower(*cptr2);
    cptr2++;
  }
  *cptr1='\0';
  field=fieldstart;

  // Skip leading whitespace in value string
  while(*valuestart!='\0' && isspace(*valuestart)) valuestart++;

  // Strip trailing comment (if any).
  cptr1=valuestart;
  while((cptr1=strchr(cptr1,'#'))!=NULL) if(*(++cptr1)=='#') break;
  if(cptr1!=NULL) *(cptr1-1)='\0';

  // Strip trailing whitespace
  cptr1=strchr(valuestart,'\0');
  while(cptr1>valuestart && isspace(*(--cptr1))) (*cptr1)='\0';

  // If field is "begin" or "end", then convert value to lowercase
  // and strip whitespace
  if(field.compare("begin")==0 || field.compare("end")==0) {
    cptr1=cptr2=valuestart;
    while(*cptr2!='\0') {
      if(isalnum(*cptr2)) *(cptr1++) = (char)tolower(*cptr2);
      cptr2++;
    }
    *cptr1='\0';
  }

  value=valuestart;
  return 0;
}

void Vf_Ovf20FileHeader::ReadTextFloat
(Tcl_Channel inchan,
 OC_REAL8m& value,
 unsigned char& nextbyte)
{ // Reads one floating point value as text from inchan.  Returns the
  // read value in the export val, and returns the first succeeding byte
  // in nextbyte.  Leading whitespace and comments are silently skipped.
  // This routine is based on the older Vf_OvfFileInput::ReadTextVec3f
  // code.  An exception is thrown if a floating point value cannot be
  // read.

  static unsigned char buf[SCRATCH_BUF_SIZE];
  unsigned char ch;
  OC_INDEX bufindex;

  // Skip leading white space and comments
  while(1) {
    if(Tcl_Read(inchan,(char*)&ch,1)<1) {
      OC_THROW(Oc_Exception(__FILE__,__LINE__,
                            "Vf_Ovf20FileHeader","ReadTextFloat",
                            "Premature end-of-file or other read error."));
    }
    if(ch=='#') {
      // Comment; read to end of line
      do {
        if(Tcl_Read(inchan,(char*)&ch,1)<1) {
          OC_THROW(Oc_Exception(__FILE__,__LINE__,
                                "Vf_Ovf20FileHeader","ReadTextFloat",
                                "Premature end-of-file or other read error"
                                " while reading comment line."));
        }
      } while(ch!='\n');
      continue;
    }
    // Non-comment white-space check
    if(isspace(ch)) continue;
    // Otherwise, should be data.
    break;
  }

  // Fill buffer with data up to next white space character
  bufindex=0;
  do {
    if(ch=='d' || ch=='D') ch='e';  // Convert FORTRAN 'd' exponent
    buf[bufindex++]=ch;             // designator to C 'e' designator
    if(Tcl_Read(inchan,(char*)&ch,1)<1) {
      OC_THROW(Oc_Exception(__FILE__,__LINE__,
                            "Vf_Ovf20FileHeader","ReadTextFloat",
                            "Premature end-of-file or other read error "
                            "while reading data value."));
    }
  } while(!isspace(ch) && bufindex<SCRATCH_BUF_SIZE-1);
  if(bufindex>=SCRATCH_BUF_SIZE-1) {
    // Single data elt too big to fit in buffer?!
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_Ovf20FileHeader","ReadTextFloat",1024,
                          "Bad data in file; single floating point"
                          " value >%d characters long.",
                          SCRATCH_BUF_SIZE-1));
  }
  buf[bufindex]='\0';
  nextbyte = ch;

  // Convert to floating point and store
  unsigned char* endptr;
  value = Nb_Strtod(buf,&endptr);
  if( (endptr-buf) != bufindex ) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
             "Vf_Ovf20FileHeader","ReadTextFloat",1024+SCRATCH_BUF_SIZE,
             "Non-numeric data ->%s<- in data block of input file",
             buf));
  }

}

void Vf_Ovf20FileHeader::ReadHeader
(Tcl_Channel inchan)
{ // Fills header fields from file.  Throws an exception
  // if the file has improper structure, reaches EOF
  // before the "# End Header" marker, or if the resulting
  // header is not valid (as determined by IsValid()).
  Tcl_DString linebuf;
  Tcl_DStringInit(&linebuf);
  char* cptr;
  String field,value;
  String origline; // Copy of line as read, for error messages (if needed)

  //////////////////////////////////////////////////////////////////////
  // Read outer wrapper lines.

  // File ID line
  if(Tcl_Gets(inchan,&linebuf)<0) {
    Tcl_DStringFree(&linebuf);
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_Ovf20FileHeader","ReadHeader",
                          "Empty or unreadable file."));
  }
  cptr = Tcl_DStringValue(&linebuf);
  CollapseHeaderLine(cptr);
  if(strcmp(cptr,"#oommfovf2.0")!=0) {
    Tcl_DStringFree(&linebuf);
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_Ovf20FileHeader","ReadHeader",
                          "Bad identifier (first) line."));
  }
  ovfversion = vf_ovf_20;

  // Segment count line
  do {
    Tcl_DStringSetLength(&linebuf,0);
    if(Tcl_Gets(inchan,&linebuf)<0) {
      Tcl_DStringFree(&linebuf);
      OC_THROW(Oc_Exception(__FILE__,__LINE__,
                            "Vf_Ovf20FileHeader","ReadHeader",
                            "Premature EOF reading header."));
    }
    cptr = Tcl_DStringValue(&linebuf);
    CollapseHeaderLine(cptr);
  } while(cptr[0]=='\0' ||
          (cptr[0]=='#' && (cptr[1]=='\0' || cptr[1]=='#')));
  /// Skip empty lines and comment lines
  if(strcmp(cptr,"#segmentcount:1")!=0) {
    Tcl_DStringFree(&linebuf);
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_Ovf20FileHeader","ReadHeader",
                          "Bad Segment count line."));
  }

  // Begin: Segment
  do {
    Tcl_DStringSetLength(&linebuf,0);
    if(Tcl_Gets(inchan,&linebuf)<0) {
      Tcl_DStringFree(&linebuf);
      OC_THROW(Oc_Exception(__FILE__,__LINE__,
                            "Vf_Ovf20FileHeader","ReadHeader",
                            "Premature EOF reading header."));
    }
    ParseHeaderLine(Tcl_DStringValue(&linebuf),field,value);
  } while(field.size()==0); // Skip empty lines and comment lines
  if(field.compare("begin")!=0 || value.compare("segment")!=0) {
    Tcl_DStringFree(&linebuf);
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_Ovf20FileHeader","ReadHeader",
                          "Bad Begin: Segment line."));
  }

  // Begin: Header
  do {
    Tcl_DStringSetLength(&linebuf,0);
    if(Tcl_Gets(inchan,&linebuf)<0) {
      Tcl_DStringFree(&linebuf);
      OC_THROW(Oc_Exception(__FILE__,__LINE__,
                            "Vf_Ovf20FileHeader","ReadHeader",
                            "Premature EOF reading header."));
    }
    ParseHeaderLine(Tcl_DStringValue(&linebuf),field,value);
  } while(field.size()==0); // Skip empty lines and comment lines
  if(field.compare("begin")!=0 || value.compare("header")!=0) {
    Tcl_DStringFree(&linebuf);
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_Ovf20FileHeader","ReadHeader",
                          "Bad Begin: Header line."));
  }

  //////////////////////////////////////////////////////////////////////
  // Read field-value lines.
  while(1) {
    do {
      Tcl_DStringSetLength(&linebuf,0);
      if(Tcl_Gets(inchan,&linebuf)<0) {
        Tcl_DStringFree(&linebuf);
        OC_THROW(Oc_Exception(__FILE__,__LINE__,
                              "Vf_Ovf20FileHeader","ReadHeader",
                              "Premature EOF reading header."));
      }
      cptr = Tcl_DStringValue(&linebuf);
      origline = cptr;
      ParseHeaderLine(cptr,field,value);
    } while(field.size()==0); // Skip empty lines and comment lines

    if(field.compare("end")==0) {
      if(value.compare("header")==0) break;  // End: Header found
      String errmsg = "Invalid header line: ";
      errmsg += origline;
      Tcl_DStringFree(&linebuf);
      OC_THROW(Oc_Exception(__FILE__,__LINE__,
                            "Vf_Ovf20FileHeader","ReadHeader",
                            errmsg.c_str()));
    }

    // Otherwise, we have what appears to be a valid field-value line

    if(field.compare("desc")==0) {
      // Special case handling for "descriptor" lines
      String tmp;
      if(desc.IsSet()) {
        desc.WriteToString(tmp);
        desc.Clear();
        tmp += "\n";
      }
      tmp += value;
      desc.SetFromString(tmp.c_str());
      continue;
    }

    vector<Vf_Ovf20FieldWrap*>::iterator it;
    for(it = field_list.begin() ; it!= field_list.end() ; ++it) {
      if(field.compare((*it)->GetName())!= 0) continue;
      if((*it)->IsSet()) {
        String errmsg = String("Header field ")
          + field + String(" set more than once.");
        Tcl_DStringFree(&linebuf);
        OC_THROW(Oc_Exception(__FILE__,__LINE__,
                              "Vf_Ovf20FileHeader","ReadHeader",
                              errmsg.c_str()));
      }
      (*it)->SetFromString(value.c_str());
      break;
    }
    if( it == field_list.end() ) {
      String errmsg = String("Unrecognized header field: ")
        + field;
      Tcl_DStringFree(&linebuf);
      OC_THROW(Oc_Exception(__FILE__,__LINE__,
                            "Vf_Ovf20FileHeader","ReadHeader",
                            errmsg.c_str()));
    }
  }
  Tcl_DStringFree(&linebuf);

  //////////////////////////////////////////////////////////////////////
  // Header successfully read, up to "# End: Header" line.
  // Check for validity.
  if(!IsValid()) {
    String errmsg = "Invalid segment header.";
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_Ovf20FileHeader","ReadHeader",
                          errmsg.c_str()));
  }
}

// Vf_Ovf20FileHeader::ReadData interleaves data from a channel into a
// collection of OC_REAL8m arrays, as indicated by the data_array
// parameter.  The data_array elements must have their data set to
// compatible values before calling --- call ReadHeader first to get
// the necessary info.
void
Vf_Ovf20FileHeader::ReadData
(Tcl_Channel inchan,
 const vector<Vf_Ovf20VecArray>& data_array,
 const Vf_Ovf20VecArray* node_locations) const
{
  // node_locations must be non-null for irregular grids

  // Check that data_array structure is consistent with file header
  const OC_INDEX data_array_count = static_cast<OC_INDEX>(data_array.size());
  const OC_INDEX data_array_length = data_array[0].array_length;
  if(data_array_count<1 || data_array_length<1) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_Ovf20FileHeader","ReadData",
                          "No memory allocated to hold data."));
  }
  OC_INDEX data_array_dimension = 0;
  for(OC_INDEX i=0;i<data_array_count;++i) {
    data_array_dimension += data_array[i].vector_dimension;
    if(data_array_length != data_array[i].array_length) {
      OC_THROW(Oc_Exception(__FILE__,__LINE__,
                            "Vf_Ovf20FileHeader","ReadData",
                            "Inconsistent allocated data array lengths"));
    }
  }

  const OC_INDEX veclen = valuedim.Value();
  if(veclen != data_array_dimension) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
       "Vf_Ovf20FileHeader","ReadData",
       "Allocated data dimension doesn't match"
       " dimension specified in header"));
  }


  OC_INDEX nodecount = 0;
  const Vf_Ovf20_MeshType input_meshtype = meshtype.Value();
  if(input_meshtype == vf_ovf20mesh_rectangular) {
    nodecount = xnodes.Value() * ynodes.Value() * znodes.Value();
  } else if(input_meshtype == vf_ovf20mesh_irregular) {
    nodecount = pointcount.Value();
  } else {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_Ovf20FileHeader","ReadData",
                          "Unrecognized mesh type."));
  }
  if(nodecount != data_array_length) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
       "Vf_Ovf20FileHeader","ReadData",
       "Allocated data length doesn't match node count specified in header"));
  }

  if(!node_locations && input_meshtype == vf_ovf20mesh_irregular) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_Ovf20FileHeader","ReadData",
                          "No node_location storage specified."));
  }
  if(node_locations) {
    if(node_locations->vector_dimension != 3
       || node_locations->array_length != nodecount
       || node_locations->data == 0) {
      OC_THROW(Oc_Exception(__FILE__,__LINE__,
                            "Vf_Ovf20FileHeader","ReadData",
                            "Invalid node_location import."));
    }
  }

  // If node_location fill is requested (i.e., node_locations != NULL),
  // and if the mesh type is regular, then fill node_location with
  // locations computed out of the header information --- otherwise,
  // node_location is filled farther below from file data.
  if(node_locations && input_meshtype == vf_ovf20mesh_rectangular) {
    OC_INDEX istop = xnodes.Value();
    OC_INDEX jstop = ynodes.Value();
    OC_INDEX kstop = znodes.Value();
    const OC_REAL8m x0 = static_cast<OC_REAL8m>(xbase.Value());
    const OC_REAL8m y0 = static_cast<OC_REAL8m>(ybase.Value());
    const OC_REAL8m z0 = static_cast<OC_REAL8m>(zbase.Value());
    const OC_REAL8m xstep = static_cast<OC_REAL8m>(xstepsize.Value());
    const OC_REAL8m ystep = static_cast<OC_REAL8m>(ystepsize.Value());
    const OC_REAL8m zstep = static_cast<OC_REAL8m>(zstepsize.Value());
    OC_REAL8m* loc = node_locations->data;
    for(OC_INDEX k=0;k<kstop;++k) {
      const OC_REAL8m z = z0 + k*zstep;
      for(OC_INDEX j=0;j<jstop;++j) {
        const OC_REAL8m y = y0 + j*ystep;
        for(OC_INDEX i=0;i<istop;++i) {
          const OC_REAL8m x = x0 + i*xstep;
          loc[0] = x; loc[1] = y; loc[2] = z;
          loc += 3;
        }
      }
    }
  }

  // Read data header
  Vf_OvfDataStyle datastyle = vf_oinvalid;
  Tcl_DString linebuf;
  String field,value;
  Tcl_DStringInit(&linebuf);
  do {
    Tcl_DStringSetLength(&linebuf,0);
    if(Tcl_Gets(inchan,&linebuf)<0) {
      Tcl_DStringFree(&linebuf);
      OC_THROW(Oc_Exception(__FILE__,__LINE__,
                            "Vf_Ovf20FileHeader","ReadData",
                            "Premature EOF reading data header."));
    }
    ParseHeaderLine(Tcl_DStringValue(&linebuf),field,value);
  } while(field.size()==0); // Skip empty lines and comment lines
  Tcl_DStringFree(&linebuf);
  if(field.compare("begin")!=0 || value.compare(0,4,"data")!=0) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_Ovf20FileHeader","ReadData",
                          "Bad \"Begin: data\" line."));
  }
  if(value.compare("datatext")==0) {
    datastyle = vf_oascii;
  } else if(value.compare("databinary4")==0) {
    datastyle = vf_obin4;
  } else if(value.compare("databinary8")==0) {
    datastyle = vf_obin8;
  } else {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
              "Vf_Ovf20FileHeader","ReadData",
              "Bad \"Begin: data\" line: invalid data style."));
  }


  // Read data
  if(datastyle == vf_oascii) {
    // Text-style input /////////////////////////////////////////////
    unsigned char dummy;
    for(OC_INDEX node=0;node<nodecount;++node) {
      if(input_meshtype == vf_ovf20mesh_irregular) {
        OC_REAL8m x,y,z;
        ReadTextFloat(inchan,x,dummy);
        ReadTextFloat(inchan,y,dummy);
        ReadTextFloat(inchan,z,dummy);
        node_locations->data[3*node]   = x;
        node_locations->data[3*node+1] = y;
        node_locations->data[3*node+2] = z;
      }
      for(OC_INDEX ida=0;ida<data_array_count;++ida) {
        const OC_INDEX ddim = data_array[ida].vector_dimension;
        OC_REAL8m* data = data_array[ida].data + node*ddim;
        for(OC_INDEX elt=0;elt<ddim;++elt) {
          ReadTextFloat(inchan,data[elt],dummy);
        }
      }
    }
  } else if(datastyle == vf_obin4) {
    // 4-byte binary output (little endian order) ////////////////////
    OC_REAL4 check;
    if(Tcl_Read(inchan,(char*)&check,sizeof(check))
       < static_cast<int>(sizeof(check))) {
      OC_THROW(Oc_Exception(__FILE__,__LINE__,
                            "Vf_Ovf20FileHeader","ReadData",
                            "Premature end-of-file or other read error "
                            "while reading 4-byte data check value."));
    }
    // OVF 1.0 uses MSB order, 2.0 uses LSB order
#if (OC_BYTEORDER != 4321)  // Input is MSB
    if(ovfversion != vf_ovf_10) Oc_Flip4(&check);
#else                       // Input is LSB
    if(ovfversion == vf_ovf_10) Oc_Flip4(&check);
#endif
    if(!Vf_OvfFileFormatSpecs::CheckValue(check)) {
      OC_THROW(Oc_Exception(__FILE__,__LINE__,
                            "Vf_Ovf20FileHeader","ReadData",
                            "Invalid data block check value in input"
                            " file; Wrong byte order?"));
    }

    const OC_INDEX stride = 1024; // Number of nodes blocked together for input
    OC_INDEX record_count = stride;
    const OC_INDEX reclen = veclen
      + (input_meshtype == vf_ovf20mesh_irregular ? 3 : 0);
    OC_INDEX inwords = reclen*stride;
    OC_INDEX inbytes = inwords*OC_INDEX(sizeof(OC_REAL4));

    Oc_AutoBuf workbuf; // Workspace.  Tweak to insure good alignment
    workbuf.SetLength(inbytes+sizeof(OC_REAL4)-1 + 10000);
    char* foo_align = workbuf.GetStr();
    int foo_offset = static_cast<int>((foo_align-(char*)0)% sizeof(OC_REAL4));
    if(foo_offset>0) foo_align += sizeof(OC_REAL4) - foo_offset;
    OC_REAL4* const d4buf = reinterpret_cast<OC_REAL4*>(foo_align);

    for(OC_INDEX node=0;node<nodecount;node+=stride) {
      if(node+stride>nodecount) {
        record_count = nodecount-node;
        inwords = reclen * record_count;
        inbytes = inwords * OC_INDEX(sizeof(OC_REAL4));
      }
      if(inbytes>INT_MAX) {
        OC_THROW(Oc_Exception(__FILE__,__LINE__,
                              "Vf_Ovf20FileHeader","ReadData",
                              "Data (d4buf) read length request too big"
                              " for Tcl_Read."));
      }
      if(Tcl_Read(inchan,(char*)d4buf,static_cast<int>(inbytes))<inbytes) {
        OC_THROW(Oc_Exception(__FILE__,__LINE__,
                              "Vf_Ovf20FileHeader","ReadData",
                              "Premature end-of-file or other read error "
                              "while reading data block."));
      }
#if (OC_BYTEORDER != 4321)  // Input is MSB
      if(ovfversion != vf_ovf_10) Oc_SwapWords4(d4buf,inwords);
#else                    // Input is LSB
      if(ovfversion == vf_ovf_10) Oc_SwapWords4(d4buf,inwords);
#endif

      if(input_meshtype == vf_ovf20mesh_irregular) {
        OC_REAL4* din = d4buf - 1;
        for(OC_INDEX j=0;j<record_count;++j) {
          node_locations->data[3*(node+j)]   = static_cast<OC_REAL8m>(*(++din));
          node_locations->data[3*(node+j)+1] = static_cast<OC_REAL8m>(*(++din));
          node_locations->data[3*(node+j)+2] = static_cast<OC_REAL8m>(*(++din));
          for(OC_INDEX ida=0;ida<data_array_count;++ida) {
            const OC_INDEX ddim = data_array[ida].vector_dimension;
            OC_REAL8m* da = data_array[ida].data + (node+j)*ddim;
            for(OC_INDEX elt=0;elt<ddim;++elt) {
              da[elt] = static_cast<OC_REAL8m>(*(++din));
            }
          }
        }
      } else {
        OC_REAL4* din = d4buf - 1;
        for(OC_INDEX j=0;j<record_count;++j) {
          for(OC_INDEX ida=0;ida<data_array_count;++ida) {
            const OC_INDEX ddim = data_array[ida].vector_dimension;
            OC_REAL8m* da = data_array[ida].data + (node+j)*ddim;
            for(OC_INDEX elt=0;elt<ddim;++elt) {
              da[elt] = static_cast<OC_REAL8m>(*(++din));
            }
          }
        }
      }
    }
  } else if(datastyle == vf_obin8) {
    // 8-byte binary output (little endian order) ////////////////////
    // NOTE: An optimization is possible here for the case where
    // data_array_dimension == 1 and OC_REAL8m == OC_REAL8 --- the
    // data could be read directly into data_array[0] without using
    // using an intermediate buffer.  We don't mess with that now,
    // but may add it in the future.
    OC_REAL8 check;
    if(Tcl_Read(inchan,(char*)&check,sizeof(check))
       < static_cast<int>(sizeof(check))) {
      OC_THROW(Oc_Exception(__FILE__,__LINE__,
                            "Vf_Ovf20FileHeader","ReadData",
                            "Premature end-of-file or other read error "
                            "while reading 8-byte data check value."));
    }
    // OVF 1.0 uses MSB order, 2.0 uses LSB order
#if (OC_BYTEORDER != 4321)  // Input is MSB
    if(ovfversion != vf_ovf_10) Oc_Flip8(&check);
#else                       // Input is LSB
    if(ovfversion == vf_ovf_10) Oc_Flip8(&check);
#endif
    if(!Vf_OvfFileFormatSpecs::CheckValue(check)) {
      OC_THROW(Oc_Exception(__FILE__,__LINE__,
                            "Vf_Ovf20FileHeader","ReadData",
                            "Invalid data block check value in input"
                            " file; Wrong byte order?"));
    }

    const OC_INDEX stride = 1024; // Number of nodes blocked together for input
    OC_INDEX record_count = stride;
    const OC_INDEX reclen = veclen
      + (input_meshtype == vf_ovf20mesh_irregular ? 3 : 0);
    OC_INDEX inwords = reclen*stride;
    OC_INDEX inbytes = inwords*OC_INDEX(sizeof(OC_REAL8));

    Oc_AutoBuf workbuf; // Workspace.  Tweak to insure good alignment
    workbuf.SetLength(inbytes+sizeof(OC_REAL8)-1 + 10000);
    char* foo_align = workbuf.GetStr();
    int foo_offset = static_cast<int>((foo_align-(char*)0)% sizeof(OC_REAL8));
    if(foo_offset>0) foo_align += sizeof(OC_REAL8) - foo_offset;
    OC_REAL8* const d8buf = reinterpret_cast<OC_REAL8*>(foo_align);

    for(OC_INDEX node=0;node<nodecount;node+=stride) {
      if(node+stride>nodecount) {
        record_count = nodecount-node;
        inwords = reclen * record_count;
        inbytes = inwords * OC_INDEX(sizeof(OC_REAL8));
      }
      if(inbytes>INT_MAX) {
        OC_THROW(Oc_Exception(__FILE__,__LINE__,
                              "Vf_Ovf20FileHeader","ReadData",
                              "Data (d8buf) read length request too big"
                              " for Tcl_Read."));
      }
      if(Tcl_Read(inchan,(char*)d8buf,static_cast<int>(inbytes))<inbytes) {
        OC_THROW(Oc_Exception(__FILE__,__LINE__,
                              "Vf_Ovf20FileHeader","ReadData",
                              "Premature end-of-file or other read error "
                              "while reading data block."));
      }
#if (OC_BYTEORDER != 4321)  // Input is MSB
      if(ovfversion != vf_ovf_10) Oc_SwapWords8(d8buf,inwords);
#else                    // Input is LSB
      if(ovfversion == vf_ovf_10) Oc_SwapWords8(d8buf,inwords);
#endif

      if(input_meshtype == vf_ovf20mesh_irregular) {
        OC_REAL8* din = d8buf - 1;
        for(OC_INDEX j=0;j<record_count;++j) {
          node_locations->data[3*(node+j)]   = static_cast<OC_REAL8m>(*(++din));
          node_locations->data[3*(node+j)+1] = static_cast<OC_REAL8m>(*(++din));
          node_locations->data[3*(node+j)+2] = static_cast<OC_REAL8m>(*(++din));
          for(OC_INDEX ida=0;ida<data_array_count;++ida) {
            const OC_INDEX ddim = data_array[ida].vector_dimension;
            OC_REAL8m* da = data_array[ida].data + (node+j)*ddim;
            for(OC_INDEX elt=0;elt<ddim;++elt) {
              da[elt] = static_cast<OC_REAL8m>(*(++din));
            }
          }
        }
      } else {
        OC_REAL8* din = d8buf - 1;
        for(OC_INDEX j=0;j<record_count;++j) {
          for(OC_INDEX ida=0;ida<data_array_count;++ida) {
            const OC_INDEX ddim = data_array[ida].vector_dimension;
            OC_REAL8m* da = data_array[ida].data + (node+j)*ddim;
            for(OC_INDEX elt=0;elt<ddim;++elt) {
              da[elt] = static_cast<OC_REAL8m>(*(++din));
            }
          }
        }
      }
    }
  }

  // Read data trailer
  Tcl_DStringInit(&linebuf);
  do {
    Tcl_DStringSetLength(&linebuf,0);
    if(Tcl_Gets(inchan,&linebuf)<0) {
      Tcl_DStringFree(&linebuf);
      OC_THROW(Oc_Exception(__FILE__,__LINE__,
                            "Vf_Ovf20FileHeader","ReadData",
                            "Premature EOF reading data trailer."));
    }
    ParseHeaderLine(Tcl_DStringValue(&linebuf),field,value);
  } while(field.size()==0); // Skip empty lines and comment lines
  Tcl_DStringFree(&linebuf);
  if(field.compare("end")!=0 || value.compare(0,4,"data")!=0) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_Ovf20FileHeader","ReadData",
                          "Bad \"End: data\" line."));
  }

  // Read segment trailer
  Tcl_DStringInit(&linebuf);
  do {
    Tcl_DStringSetLength(&linebuf,0);
    if(Tcl_Gets(inchan,&linebuf)<0) {
      Tcl_DStringFree(&linebuf);
      OC_THROW(Oc_Exception(__FILE__,__LINE__,
                            "Vf_Ovf20FileHeader","ReadData",
                            "Premature EOF reading segment trailer."));
    }
    ParseHeaderLine(Tcl_DStringValue(&linebuf),field,value);
  } while(field.size()==0); // Skip empty lines and comment lines
  Tcl_DStringFree(&linebuf);
  if(field.compare("end")!=0 || value.compare("segment")!=0) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_Ovf20FileHeader","ReadData",
                          "Bad \"End: segment\" line."));
  }
}


// The following Vf_Ovf20FileHeader::ReadData routine is a special
// case version of ReadData, where the destination buffer is a single
// array rather than a collection of several.
void
Vf_Ovf20FileHeader::ReadData
(Tcl_Channel inchan,
 const Vf_Ovf20VecArray& data_info,
 const Vf_Ovf20VecArray* node_locations) const
{
  vector<Vf_Ovf20VecArray> data_array;
  data_array.push_back(data_info);
  ReadData(inchan,data_array,node_locations);
}


void
Vf_Ovf20FileHeader::WriteHeader
(Tcl_Channel outchan) const
{
  if(!IsValid()) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_Ovf20FileHeader","WriteHeader",
                          "Invalid header"));
  }

  if(ovfversion == vf_ovf_invalid) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_Ovf20FileHeader","WriteHeader",
                          "Invalid file version request"));
  }

  if(ovfversion != vf_ovf_10 && ovfversion != vf_ovf_20) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_Ovf20FileHeader","WriteHeader",
                          "Unsupported file version request"));
  }

  if(ovfversion == vf_ovf_10 && valuedim.Value() != 3) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
             "Vf_Ovf20FileHeader","WriteHeader",
             "OVF 1.0 format only supports output of 3-vectors"));
  }

  if(ovfversion == vf_ovf_10) {
    if(meshtype.Value() == vf_ovf20mesh_rectangular) {
      Nb_FprintfChannel(outchan, NULL, 1024,
                        "# OOMMF: rectangular mesh v1.0\n");
    } else {
      Nb_FprintfChannel(outchan, NULL, 1024,
                        "# OOMMF: irregular mesh v1.0\n");
    }
  } else {
    Nb_FprintfChannel(outchan, NULL, 1024, "# OOMMF OVF 2.0\n");
  }
  Nb_FprintfChannel(outchan, NULL, 1024, "#\n");
  Nb_FprintfChannel(outchan, NULL, 1024, "# Segment count: 1\n");
  Nb_FprintfChannel(outchan, NULL, 1024, "#\n");
  Nb_FprintfChannel(outchan, NULL, 1024, "# Begin: Segment\n");
  Nb_FprintfChannel(outchan, NULL, 1024, "# Begin: Header\n");
  Nb_FprintfChannel(outchan, NULL, 1024, "#\n");

  // Dump segment header
  String valuebuf;
  for(vector<Vf_Ovf20FieldWrap*>::const_iterator it = field_list.begin();
      it!= field_list.end(); ++it) {
    if(!(*it)->IsSet()) continue;
    (*it)->WriteToString(valuebuf);
    if((*it)->GetNameString().compare("desc")==0) {
      // Special case handling for "descriptor" field:
      // newlines are changed to multiple Desc records.
      // Note special case handling of "desc" field label
      // in Nb_FprintfChannel call below.
      const char* record_leader = "# Desc: ";
      String::size_type offset=0;
      while((offset=valuebuf.find("\n",offset))!=String::npos) {
        if( offset+1 == valuebuf.size() ) {
          valuebuf.erase(offset);
          break;
        } else {
          valuebuf.insert(offset+1,record_leader);
          offset += strlen(record_leader) + 1;
        }
      }
    } else {
      // In all other fields, replace any embedded newlines
      // with spaces.
      String::size_type offset=0;
      while((offset=valuebuf.find("\n",offset))!=String::npos) {
        valuebuf[offset] = ' ';
      }
    }
    if(ovfversion == vf_ovf_10) {
      if((*it)->GetNameString().compare("valuedim")==0 ||
         (*it)->GetNameString().compare("valuelabels")==0) {
        // These records don't exist in OVF 1.0
        continue;
      }
      if((*it)->GetNameString().compare("valueunits")==0) {
        const vector<String>& vu = valueunits.Value();
        size_t namelen = strlen("valueunit");
        size_t valuelen = vu[0].size();
        Nb_FprintfChannel(outchan, NULL,
                          static_cast<OC_INDEX>(namelen+valuelen+32),
                          "# valueunit: %s\n",vu[0].c_str());
        continue;
      }
    }
      
    size_t namelen = strlen((*it)->GetName());
    size_t valuelen = valuebuf.size();
    size_t nbufsize = namelen + valuelen + 32;
    assert(OC_INDEX(nbufsize)>=0 && nbufsize == size_t(OC_INDEX(nbufsize)));
    if((*it)->GetNameString().compare("title")==0) {
      // Special case handling for "prettier" output that
      // helps backwards compatibility with broken parsers.
      Nb_FprintfChannel(outchan, NULL, OC_INDEX(nbufsize),
                        "# %s: %s\n","Title",valuebuf.c_str());
    } else if((*it)->GetNameString().compare("desc")==0) {
      // Special case handling for "prettier" output that
      // helps backwards compatibility with broken parsers.
      // Note that this should agree with the "record_leader"
      // value above for multiple-line desc records.
      Nb_FprintfChannel(outchan, NULL, OC_INDEX(nbufsize),
                        "# %s: %s\n","Desc",valuebuf.c_str());
    } else {
      // General case --- field names are written all lowercase
      Nb_FprintfChannel(outchan, NULL, OC_INDEX(nbufsize),
                        "# %s: %s\n",(*it)->GetName(),valuebuf.c_str());
    }
  }

  if(ovfversion == vf_ovf_10) {
    // Include fields required in OVF 1.0 that don't exist in OVF 2.0
    // NOTE: Max/min values aren't available at this point in the code,
    //       and it goes against the spirit of the OVF 2.0 code to muddy
    //       its design trying to support obsolete features.  So instead,
    //       just dump dummy values for ValueRangeMin(Max)Mag, and live
    //       with the resulting incompatibility. -mjd, 25-Aug-2008
    const char* vmultline = "# valuemultiplier: 1.0\n";
    const char* vminmagline = "# ValueRangeMinMag: 0.0\n";
    const char* vmaxmagline = "# ValueRangeMaxMag: 0.0\n";
    Nb_FprintfChannel(outchan, NULL, sizeof(vmultline)+32,vmultline);
    Nb_FprintfChannel(outchan, NULL, sizeof(vminmagline)+32,vminmagline);
    Nb_FprintfChannel(outchan, NULL, sizeof(vmaxmagline)+32,vmaxmagline);
  }

  Nb_FprintfChannel(outchan, NULL, 1024, "#\n");
  Nb_FprintfChannel(outchan, NULL, 1024, "# End: Header\n");
}

void
Vf_Ovf20FileHeader::WriteData
(Tcl_Channel outchan,
 Vf_OvfDataStyle datastyle,
 const char* textfmt, // Only active if datastyle == vf_oascii
 const Vf_Ovf20_MeshNodes* mesh, // Only needed for irregular grids.
 const Vf_Ovf20VecArrayConst& data_info,
 OC_BOOL bare) const
{
  // NOTE: For maintenance ease, we might want to consider using a light
  // wrapper to roll this member into the WriteData member using
  // vector<Vf_Ovf20VecArray>& data_array; we keep them separate at
  // present for historical reasons, but also because this case has
  // slightly lower overhead.


  // Dump data
  OC_INDEX veclen = valuedim.Value();
  OC_INDEX nodecount = 0;

  const Vf_Ovf20_MeshType output_meshtype = meshtype.Value();
  if(output_meshtype == vf_ovf20mesh_rectangular) {
    nodecount = xnodes.Value() * ynodes.Value() * znodes.Value();
  } else if(output_meshtype == vf_ovf20mesh_irregular) {
    nodecount = pointcount.Value();
  } else {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_Ovf20FileHeader","WriteData",
                          "Unrecognized mesh type."));
  }

  // Check that parameters in data_info agree with that in the header
  if(data_info.array_length != nodecount ||
     data_info.vector_dimension != veclen) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_Ovf20FileHeader","WriteData",
                          "data size doesn't match header info."));
  }
  const OC_REAL8m* data = data_info.data;

  if(!bare) Nb_FprintfChannel(outchan, NULL, 1024, "#\n");
  if(datastyle == vf_oascii) {
    // Text-style output /////////////////////////////////////////////
    String outfmt;
    if(textfmt == NULL || textfmt[0] == '\0') {
      outfmt = " %# .17g";
    } else {
      if(textfmt[0]!=' ' && textfmt[0]!='\t') {
        outfmt = " "; // Make sure we have at least some whitespace
        /// between items.
      }
      outfmt += textfmt;
    }
    String location_fmt = outfmt + outfmt + outfmt;
    char databuf[1024];
    if(!bare) {
      Nb_FprintfChannel(outchan, NULL, 1024, "# Begin: Data Text\n");
    }
    for(OC_INDEX node=0;node<nodecount;++node) {
      if(output_meshtype == vf_ovf20mesh_irregular) {
        OC_REAL8m x,y,z;
        mesh->GetCellCenter(static_cast<OC_INDEX>(node),x,y,z);
        Nb_FprintfChannel(outchan,databuf,sizeof(databuf),
                          location_fmt.c_str(),
                          static_cast<double>(x),
                          static_cast<double>(y),
                          static_cast<double>(z));
      }
      for(OC_INDEX elt=0;elt<veclen;++elt) {
        Nb_FprintfChannel(outchan,databuf,sizeof(databuf),
                          outfmt.c_str(),
                          static_cast<double>(*data));
        ++data;
      }
      Nb_FprintfChannel(outchan,databuf,sizeof(databuf),"\n");
    }
    if(!bare) {
      Nb_FprintfChannel(outchan, NULL, 1024, "# End: Data Text\n");
    }
  } else if(datastyle == vf_obin4) {
    // 4-byte binary output (little endian order) ////////////////////
    if(!bare) {
      Nb_FprintfChannel(outchan, NULL, 1024, "# Begin: Data Binary 4\n");
      OC_REAL4 check;
      Vf_OvfFileFormatSpecs::GetCheckValue(check);

      // OVF 1.0 uses MSB order, 2.0 uses LSB order
#if (OC_BYTEORDER != 4321)  // Input is MSB
      if(ovfversion != vf_ovf_10) Oc_Flip4(&check);
#else                    // Input is LSB
      if(ovfversion == vf_ovf_10) Oc_Flip4(&check);
#endif
      if(Nb_WriteChannel(outchan,reinterpret_cast<const char*>(&check),
                         sizeof(check)) != sizeof(check)) {
        OC_THROW(Oc_Exception(__FILE__,__LINE__,
                              "Vf_Ovf20FileHeader","WriteData",
                              "Write error (device full?)"));
      }
    }

    const OC_INDEX stride = 1024; // Number of nodes blocked together for output
    OC_INDEX record_count = stride;
    const OC_INDEX reclen = veclen
      + (output_meshtype == vf_ovf20mesh_irregular ? 3 : 0);
    OC_INDEX outwords = reclen*stride;
    OC_INDEX outbytes = outwords*OC_INDEX(sizeof(OC_REAL4));

    Oc_AutoBuf workbuf; // Workspace.  Tweak to insure good alignment
    workbuf.SetLength(outbytes+sizeof(OC_REAL4)-1 + 10000);
    char* foo_align = workbuf.GetStr();
    int foo_offset = static_cast<int>((foo_align-(char*)0)% sizeof(OC_REAL4));
    if(foo_offset>0) foo_align += sizeof(OC_REAL4) - foo_offset;
    OC_REAL4* const d4buf = reinterpret_cast<OC_REAL4*>(foo_align);

    const OC_REAL8m* d8ptr = data;
    for(OC_INDEX node=0;node<nodecount;node+=stride) {
      if(node+stride>nodecount) {
        record_count = nodecount-node;
        outwords = reclen * record_count;
        outbytes = outwords * OC_INDEX(sizeof(OC_REAL4));
      }

      if(output_meshtype == vf_ovf20mesh_irregular) {
        for(OC_INDEX j=0;j<record_count;++j) {
          OC_REAL8m x,y,z;
          mesh->GetCellCenter(static_cast<OC_INDEX>(node+j),x,y,z);
          d4buf[j*reclen]   = static_cast<OC_REAL4>(x);
          d4buf[j*reclen+1] = static_cast<OC_REAL4>(y);
          d4buf[j*reclen+2] = static_cast<OC_REAL4>(z);
          for(OC_INDEX k=0;k<veclen;++k) {
            d4buf[j*reclen+3+k] = static_cast<OC_REAL4>(d8ptr[j*veclen+k]);
          }
        }
      } else {
        for(OC_INDEX j=0;j<outwords;++j) {
          d4buf[j] = static_cast<OC_REAL4>(d8ptr[j]);
        }
      }
#if (OC_BYTEORDER != 4321)  // Input is MSB
      if(ovfversion != vf_ovf_10) Oc_SwapWords4(d4buf,outwords);
#else                    // Input is LSB
      if(ovfversion == vf_ovf_10) Oc_SwapWords4(d4buf,outwords);
#endif
      if(Nb_WriteChannel(outchan,reinterpret_cast<char*>(d4buf),
                         outbytes)!= outbytes) {
        OC_THROW(Oc_Exception(__FILE__,__LINE__,
                              "Vf_Ovf20FileHeader","WriteData",
                              "Write error (device full?)"));
      }
      d8ptr += veclen*record_count;
    }

    if(!bare) {
      Nb_FprintfChannel(outchan, NULL, 1024, "\n# End: Data Binary 4\n");
    }

  } else if(datastyle == vf_obin8) {
    // 8-byte binary output (little endian order) ////////////////////
    if(!bare) {
      Nb_FprintfChannel(outchan, NULL, 1024, "# Begin: Data Binary 8\n");
      OC_REAL8 check;
      Vf_OvfFileFormatSpecs::GetCheckValue(check);
#if (OC_BYTEORDER != 4321)  // Input is MSB
      if(ovfversion != vf_ovf_10) Oc_Flip8(&check);
#else                    // Input is LSB
      if(ovfversion == vf_ovf_10) Oc_Flip8(&check);
#endif

      if(Nb_WriteChannel(outchan,reinterpret_cast<const char*>(&check),
                         sizeof(check)) != sizeof(check)) {
        OC_THROW(Oc_Exception(__FILE__,__LINE__,
                              "Vf_Ovf20FileHeader","WriteData",
                              "Write error (device full?)"));
      }
    }

    const OC_INDEX stride = 1024; // Number of nodes blocked together for output
    OC_INDEX record_count = stride;
    const OC_INDEX reclen = veclen
      + (output_meshtype == vf_ovf20mesh_irregular ? 3 : 0);
    OC_INDEX outwords = reclen*stride;
    OC_INDEX outbytes = outwords*OC_INDEX(sizeof(OC_REAL8));

#if (OC_BYTEORDER == 4321)  // Input is LSB; flip may not be required
    if(output_meshtype != vf_ovf20mesh_irregular
       && sizeof(OC_REAL8m) == sizeof(OC_REAL8)
       && ovfversion != vf_ovf_10) {
      // We can do a direct dump from input data buffer
      const char *cptr=reinterpret_cast<const char *>(data);
      for(OC_INDEX node=0;node<nodecount;node+=stride) {
        if(node+stride>nodecount) {
          outbytes = reclen * (nodecount-node) * OC_INDEX(sizeof(OC_REAL8));
        }
        if(Nb_WriteChannel(outchan,cptr,outbytes) != outbytes) {
          OC_THROW(Oc_Exception(__FILE__,__LINE__,
                                "Vf_Ovf20FileHeader","WriteData",
                                "Write error (device full?)"));
        }
        cptr += outbytes;
      }

    } else {
#endif // OC_BYTEORDER
      // Otherwise we must copy input data buffer into an intermediate
      // buffer before we can write it.
      Oc_AutoBuf workbuf; // Workspace.  Tweak to insure good alignment
      workbuf.SetLength(outbytes+sizeof(OC_REAL8)-1);
      char* foo_align = workbuf.GetStr();
      int foo_offset = static_cast<int>((foo_align-(char*)0) % sizeof(OC_REAL8));
      if(foo_offset>0) foo_align += sizeof(OC_REAL8) - foo_offset;
      OC_REAL8* const dout = reinterpret_cast<OC_REAL8*>(foo_align);
      const OC_REAL8m* din = data;
      for(OC_INDEX node=0;node<nodecount;node+=stride) {
        if(node+stride>nodecount) {
          record_count = nodecount-node;
          outwords = reclen * record_count;
          outbytes = outwords * OC_INDEX(sizeof(OC_REAL8));
        }

        if(output_meshtype == vf_ovf20mesh_irregular) {
          for(OC_INDEX j=0;j<record_count;++j) {
            OC_REAL8m x,y,z;
            mesh->GetCellCenter(static_cast<OC_INDEX>(node+j),x,y,z);
            dout[j*reclen]   = static_cast<OC_REAL8>(x);
            dout[j*reclen+1] = static_cast<OC_REAL8>(y);
            dout[j*reclen+2] = static_cast<OC_REAL8>(z);
            for(OC_INDEX k=0;k<veclen;++k) {
              dout[j*reclen+3+k] = static_cast<OC_REAL8>(din[j*veclen+k]);
            }
          }
        } else {
          for(OC_INDEX j=0;j<outwords;++j) {
            dout[j] = static_cast<OC_REAL8>(din[j]);
          }
        }
#if (OC_BYTEORDER != 4321)  // Input is MSB
        if(ovfversion != vf_ovf_10) Oc_SwapWords8(dout,outwords);
#else                    // Input is LSB
        if(ovfversion == vf_ovf_10) Oc_SwapWords8(dout,outwords);
#endif
        if(Nb_WriteChannel(outchan,reinterpret_cast<const char*>(dout),
                           outbytes) != outbytes) {
          OC_THROW(Oc_Exception(__FILE__,__LINE__,
                                "Vf_Ovf20FileHeader","WriteData",
                                "Write error (device full?)"));
        }
        din += veclen*record_count;
      }
#if (OC_BYTEORDER == 4321)  // Input is LSB; flip may not be required
    }
#endif // OC_BYTEORDER
    if(!bare) {
      Nb_FprintfChannel(outchan, NULL, 1024, "\n# End: Data Binary 8\n");
    }
  }

  if(!bare) {
    Nb_FprintfChannel(outchan, NULL, 1024, "# End: Segment\n");
  }

}

void
Vf_Ovf20FileHeader::WriteData
(Tcl_Channel outchan,
 Vf_OvfDataStyle datastyle,
 const char* textfmt, // Only active if datastyle == vf_oascii
 const Vf_Ovf20_MeshNodes* mesh, // Only needed for irregular grids.
 const vector<Vf_Ovf20VecArrayConst>& data_array,
 OC_BOOL bare) const
{
  // Check that data_array structure is consistent with file header
  const OC_INDEX data_array_count
    = static_cast<OC_INDEX>(data_array.size());
  const OC_INDEX data_array_length
    = static_cast<OC_INDEX>(data_array[0].array_length);
  if(data_array_count<1 || data_array_length<1) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_Ovf20FileHeader","WriteData",
                          "No data."));
  }
  OC_INDEX data_array_dimension = 0;
  for(OC_INDEX i=0;i<data_array_count;++i) {
    data_array_dimension += data_array[i].vector_dimension;
    if(data_array_length != data_array[i].array_length) {
      OC_THROW(Oc_Exception(__FILE__,__LINE__,
                            "Vf_Ovf20FileHeader","WriteData",
                            "Inconsistent data array lengths"));
    }
  }

  const OC_INDEX veclen = valuedim.Value();
  if(veclen != data_array_dimension) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
       "Vf_Ovf20FileHeader","WriteData",
       "Data dimension doesn't match dimension specified in header"));
  }


  OC_INDEX nodecount = 0;
  const Vf_Ovf20_MeshType output_meshtype = meshtype.Value();
  if(output_meshtype == vf_ovf20mesh_rectangular) {
    nodecount = xnodes.Value() * ynodes.Value() * znodes.Value();
  } else if(output_meshtype == vf_ovf20mesh_irregular) {
    nodecount = pointcount.Value();
  } else {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_Ovf20FileHeader","WriteData",
                          "Unrecognized mesh type."));
  }
  if(nodecount != data_array_length) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
       "Vf_Ovf20FileHeader","WriteData",
       "Data length doesn't match node count specified in header"));
  }

  // Dump data
  if(!bare) Nb_FprintfChannel(outchan, NULL, 1024, "#\n");
  if(datastyle == vf_oascii) {
    // Text-style output /////////////////////////////////////////////
    String outfmt;
    if(textfmt == NULL || textfmt[0] == '\0') {
      outfmt = " %# .17g";
    } else {
      if(textfmt[0]!=' ' && textfmt[0]!='\t') {
        outfmt = " "; // Make sure we have at least some whitespace
        /// between items.
      }
      outfmt += textfmt;
    }
    String location_fmt = outfmt + outfmt + outfmt;
    char databuf[1024];
    if(!bare) {
      Nb_FprintfChannel(outchan, NULL, 1024, "# Begin: Data Text\n");
    }
    for(OC_INDEX node=0;node<nodecount;++node) {
      if(output_meshtype == vf_ovf20mesh_irregular) {
        OC_REAL8m x,y,z;
        mesh->GetCellCenter(static_cast<OC_INDEX>(node),x,y,z);
        Nb_FprintfChannel(outchan,databuf,sizeof(databuf),
                          location_fmt.c_str(),
                          static_cast<double>(x),
                          static_cast<double>(y),
                          static_cast<double>(z));
      }
      for(OC_INDEX ida=0;ida<data_array_count;++ida) {
        const OC_INDEX ddim = data_array[ida].vector_dimension;
        const OC_REAL8m* data = data_array[ida].data + node*ddim;
        for(OC_INDEX elt=0;elt<ddim;++elt) {
          Nb_FprintfChannel(outchan,databuf,sizeof(databuf),
                            outfmt.c_str(),
                            static_cast<double>(data[elt]));
        }
      }
      Nb_FprintfChannel(outchan,databuf,sizeof(databuf),"\n");
    }
    if(!bare) {
      Nb_FprintfChannel(outchan, NULL, 1024, "# End: Data Text\n");
    }
  } else if(datastyle == vf_obin4) {
    // 4-byte binary output (little endian order) ////////////////////
    if(!bare) {
      Nb_FprintfChannel(outchan, NULL, 1024, "# Begin: Data Binary 4\n");
      OC_REAL4 check;
      Vf_OvfFileFormatSpecs::GetCheckValue(check);

      // OVF 1.0 uses MSB order, 2.0 uses LSB order
#if (OC_BYTEORDER != 4321)  // Input is MSB
      if(ovfversion != vf_ovf_10) Oc_Flip4(&check);
#else                    // Input is LSB
      if(ovfversion == vf_ovf_10) Oc_Flip4(&check);
#endif
      if(Nb_WriteChannel(outchan,reinterpret_cast<const char*>(&check),
                         sizeof(check)) != sizeof(check)) {
        OC_THROW(Oc_Exception(__FILE__,__LINE__,
                              "Vf_Ovf20FileHeader","WriteData",
                              "Write error (device full?)"));
      }
    }

    const OC_INDEX stride = 1024; // Number of nodes blocked together for output
    OC_INDEX record_count = stride;
    const OC_INDEX reclen = veclen
      + (output_meshtype == vf_ovf20mesh_irregular ? 3 : 0);
    OC_INDEX outwords = reclen*stride;
    OC_INDEX outbytes = outwords*OC_INDEX(sizeof(OC_REAL4));

    Oc_AutoBuf workbuf; // Workspace.  Tweak to insure good alignment
    workbuf.SetLength(outbytes+sizeof(OC_REAL4)-1 + 10000);
    char* foo_align = workbuf.GetStr();
    int foo_offset = static_cast<int>((foo_align-(char*)0)% sizeof(OC_REAL4));
    if(foo_offset>0) foo_align += sizeof(OC_REAL4) - foo_offset;
    OC_REAL4* const d4buf = reinterpret_cast<OC_REAL4*>(foo_align);

    for(OC_INDEX node=0;node<nodecount;node+=stride) {
      if(node+stride>nodecount) {
        record_count = nodecount-node;
        outwords = reclen * record_count;
        outbytes = outwords * OC_INDEX(sizeof(OC_REAL4));
      }
      if(output_meshtype == vf_ovf20mesh_irregular) {
        OC_REAL4* dout = d4buf - 1;
        for(OC_INDEX j=0;j<record_count;++j) {
          OC_REAL8m x,y,z;
          mesh->GetCellCenter(static_cast<OC_INDEX>(node+j),x,y,z);
          *(++dout) = static_cast<OC_REAL4>(x);
          *(++dout) = static_cast<OC_REAL4>(y);
          *(++dout) = static_cast<OC_REAL4>(z);
          for(OC_INDEX ida=0;ida<data_array_count;++ida) {
            const OC_INDEX ddim = data_array[ida].vector_dimension;
            const OC_REAL8m* din = data_array[ida].data + (node+j)*ddim;
            for(OC_INDEX elt=0;elt<ddim;++elt) {
              *(++dout) = static_cast<OC_REAL4>(din[elt]);
            }
          }
        }
      } else {
        OC_REAL4* dout = d4buf - 1;
        for(OC_INDEX j=0;j<record_count;++j) {
          for(OC_INDEX ida=0;ida<data_array_count;++ida) {
            const OC_INDEX ddim = data_array[ida].vector_dimension;
            const OC_REAL8m* din = data_array[ida].data + (node+j)*ddim;
            for(OC_INDEX elt=0;elt<ddim;++elt) {
              *(++dout) = static_cast<OC_REAL4>(din[elt]);
            }
          }
        }
      }
#if (OC_BYTEORDER != 4321)  // Input is MSB
      if(ovfversion != vf_ovf_10) Oc_SwapWords4(d4buf,outwords);
#else                    // Input is LSB
      if(ovfversion == vf_ovf_10) Oc_SwapWords4(d4buf,outwords);
#endif
      if(Nb_WriteChannel(outchan,reinterpret_cast<char*>(d4buf),
                         outbytes)!= outbytes) {
        OC_THROW(Oc_Exception(__FILE__,__LINE__,
                              "Vf_Ovf20FileHeader","WriteData",
                              "Write error (device full?)"));
      }
    }

    if(!bare) {
      Nb_FprintfChannel(outchan, NULL, 1024, "\n# End: Data Binary 4\n");
    }

  } else if(datastyle == vf_obin8) {
    // 8-byte binary output (little endian order) ////////////////////
    if(!bare) {
      Nb_FprintfChannel(outchan, NULL, 1024, "# Begin: Data Binary 8\n");
      OC_REAL8 check;
      Vf_OvfFileFormatSpecs::GetCheckValue(check);
#if (OC_BYTEORDER != 4321)  // Input is MSB
      if(ovfversion != vf_ovf_10) Oc_Flip8(&check);
#else                    // Input is LSB
      if(ovfversion == vf_ovf_10) Oc_Flip8(&check);
#endif

      if(Nb_WriteChannel(outchan,reinterpret_cast<const char*>(&check),
                         sizeof(check)) != sizeof(check)) {
        OC_THROW(Oc_Exception(__FILE__,__LINE__,
                              "Vf_Ovf20FileHeader","WriteData",
                              "Write error (device full?)"));
      }
    }

    const OC_INDEX stride = 1024; // Number of nodes blocked together for output
    OC_INDEX record_count = stride;
    const OC_INDEX reclen = veclen
      + (output_meshtype == vf_ovf20mesh_irregular ? 3 : 0);
    OC_INDEX outwords = reclen*stride;
    OC_INDEX outbytes = outwords*OC_INDEX(sizeof(OC_REAL8));

    Oc_AutoBuf workbuf; // Workspace.  Tweak to insure good alignment
    workbuf.SetLength(outbytes+sizeof(OC_REAL8)-1 + 10000);
    char* foo_align = workbuf.GetStr();
    int foo_offset = static_cast<int>((foo_align-(char*)0)% sizeof(OC_REAL8));
    if(foo_offset>0) foo_align += sizeof(OC_REAL8) - foo_offset;
    OC_REAL8* const d8buf = reinterpret_cast<OC_REAL8*>(foo_align);

    for(OC_INDEX node=0;node<nodecount;node+=stride) {
      if(node+stride>nodecount) {
        record_count = nodecount-node;
        outwords = reclen * record_count;
        outbytes = outwords * OC_INDEX(sizeof(OC_REAL8));
      }
      if(output_meshtype == vf_ovf20mesh_irregular) {
        OC_REAL8* dout = d8buf - 1;
        for(OC_INDEX j=0;j<record_count;++j) {
          OC_REAL8m x,y,z;
          mesh->GetCellCenter(static_cast<OC_INDEX>(node+j),x,y,z);
          *(++dout) = static_cast<OC_REAL8>(x);
          *(++dout) = static_cast<OC_REAL8>(y);
          *(++dout) = static_cast<OC_REAL8>(z);
          for(OC_INDEX ida=0;ida<data_array_count;++ida) {
            const OC_INDEX ddim = data_array[ida].vector_dimension;
            const OC_REAL8m* din = data_array[ida].data + (node+j)*ddim;
            for(OC_INDEX elt=0;elt<ddim;++elt) {
              *(++dout) = static_cast<OC_REAL8>(din[elt]);
            }
          }
        }
      } else {
        OC_REAL8* dout = d8buf - 1;
        for(OC_INDEX j=0;j<record_count;++j) {
          for(OC_INDEX ida=0;ida<data_array_count;++ida) {
            const OC_INDEX ddim = data_array[ida].vector_dimension;
            const OC_REAL8m* din = data_array[ida].data + (node+j)*ddim;
            for(OC_INDEX elt=0;elt<ddim;++elt) {
              *(++dout) = static_cast<OC_REAL8>(din[elt]);
            }
          }
        }
      }
#if (OC_BYTEORDER != 4321)  // Input is MSB
      if(ovfversion != vf_ovf_10) Oc_SwapWords8(d8buf,outwords);
#else                    // Input is LSB
      if(ovfversion == vf_ovf_10) Oc_SwapWords8(d8buf,outwords);
#endif
      if(Nb_WriteChannel(outchan,reinterpret_cast<char*>(d8buf),
                         outbytes)!= outbytes) {
        OC_THROW(Oc_Exception(__FILE__,__LINE__,
                              "Vf_Ovf20FileHeader","WriteData",
                              "Write error (device full?)"));
      }
    }
    if(!bare) {
      Nb_FprintfChannel(outchan, NULL, 1024, "\n# End: Data Binary 8\n");
    }
  }

  if(!bare) {
    Nb_FprintfChannel(outchan, NULL, 1024, "# End: Segment\n");
  }

}

void
Vf_Ovf20FileHeader::WriteNPY
(Tcl_Channel outchan,
 Vf_OvfDataStyle datastyle,
 const Vf_Ovf20VecArrayConst& data_info,
 const Vf_Ovf20_MeshNodes* mesh, // Only needed for irregular grids.
 const int textwidth, // Only active if datastyle == vf_oascii
 const char* textfmt  // Only active if datastyle == vf_oascii
) const
{ // Simple wrapper around version that takes a vector of
  // Vf_Ovf20VecArrayConst objects.
  vector<Vf_Ovf20VecArrayConst> data_array;
  data_array.push_back(data_info);
  WriteNPY(outchan,datastyle,data_array,mesh,textwidth,textfmt);
}

void
Vf_Ovf20FileHeader::WriteNPY
(Tcl_Channel outchan,
 Vf_OvfDataStyle datastyle,
 const vector<Vf_Ovf20VecArrayConst>& data_array,
 const Vf_Ovf20_MeshNodes* mesh, // Only needed for irregular grids.
 const int textwidth, // Only active if datastyle == vf_oascii
 const char* textfmt  // Only active if datastyle == vf_oascii
) const
{
  // Check that data_array structure is consistent with file header
  const OC_INDEX data_array_count
    = static_cast<OC_INDEX>(data_array.size());
  const OC_INDEX data_array_length
    = static_cast<OC_INDEX>(data_array[0].array_length);
  if(data_array_count<1 || data_array_length<1) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_Ovf20FileHeader","WriteNPY",
                          "No data."));
  }
  OC_INDEX data_array_dimension = 0;
  for(OC_INDEX i=0;i<data_array_count;++i) {
    data_array_dimension += data_array[i].vector_dimension;
    if(data_array_length != data_array[i].array_length) {
      OC_THROW(Oc_Exception(__FILE__,__LINE__,
                            "Vf_Ovf20FileHeader","WriteNPY",
                            "Inconsistent data array lengths"));
    }
  }

  const OC_INDEX veclen = valuedim.Value();
  if(veclen != data_array_dimension) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
       "Vf_Ovf20FileHeader","WriteNPY",
       "Data dimension doesn't match dimension specified in header"));
  }


  OC_INDEX nodecount = 0;
  const Vf_Ovf20_MeshType output_meshtype = meshtype.Value();
  if(output_meshtype == vf_ovf20mesh_rectangular) {
    nodecount = xnodes.Value() * ynodes.Value() * znodes.Value();
  } else if(output_meshtype == vf_ovf20mesh_irregular) {
    nodecount = pointcount.Value();
  } else {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_Ovf20FileHeader","WriteNPY",
                          "Unrecognized mesh type."));
  }
  if(nodecount != data_array_length) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
       "Vf_Ovf20FileHeader","WriteNPY",
       "Data length doesn't match node count specified in header"));
  }

  // Write NPY header.
  //
  // For NPY version 1.0:
  // The first 6 bytes are the magic string "\x93NUMPY".
  // Bytes 7 and 8 (index 6 and 7) are the major and minor version
  //    numbers, e.g., "\x01\x00" for version 1.0.
  // Bytes at index 8 and 9 are a little-endian two-byte short reporting
  //    the length of the remaining header (aka HEADER_LEN). HEADER_LEN
  //    is padded upward as necessary so that the total header length is
  //    divisible by 64.
  // The remainder is a string consisting of an ASCII string that is a
  //    literal expression of a Python dictionary with the keys:
  //             descr  : dtype.descr
  //      fortran_order : bool
  //              shape : tuple of int
  // The dict string is terminated by a newline (not \x00) and spaces
  //    are appended as necessary to match the length recorded by
  //    HEADER_LEN. (In practice numpy.save() appears to put the spaces
  //    first and the newline at the end.)
  // The dtype.descr field is a string that can be used to construct an
  //    NumPy dtype. The simplest form is three concatenated fields
  //    consisting of and endian code, type character, and byte
  //    count. The endian code is one of '<', '>', or '=' denoting
  //    little-endian, big-endian, or hardware-native (default). There
  //    is a long list of type characters, but notable here are 'f' for
  //    floating-point, 'i' and 'u' for signed and unsigned integer,
  //    respectively, 'S' or 'a' for zero-terminated bytes, and 'U' for
  //    a Unicode string. My reference says 'S' and 'a' are "not
  //    recommended"; Python 3 uses unicode bytes. However, in NPY
  //    version 1.0 and 2.0 the dictionary string in the header is
  //    restricted to ASCII, so we should probably use 'S' unless we are
  //    writing a NPY 3.0 file. The byte count is base-ten ASCII,
  //    although note that only certain floating point lengths are
  //    supported by NumPy: 2, 4, and 8. The 'f16' type is supported if
  //    the build compiler long double type is wider than 8 bytes.
  //    Notably, f16 provides the x86 80-bit float.
  //       
  // The NPY version 2.0 format allows the total size of the header to
  //    be up to 4 GiB, and so the HEADER_LEN field grows from 2 to 4
  //    little endian bytes.
  //
  // The NPY version 3.0 format replaces the ASCII string (which in
  //    practice was really latin1) with a utf-8 encoded string.
  String header;
  header.push_back('\x93');
  header += "NUMPY";
  header.push_back('\x01');  header.push_back('\x00');
  /// Currently support only NPY 1.0. NB: Don't try to use
  /// constructs like
  ///   header += "\x01\x00";
  /// because the RHS is treated as a null-terminated string.
  header.push_back('\x00');  header.push_back('\x00');
  /// Placeholder for HEADER_LEN
  if(datastyle == vf_oascii) {
    if(textwidth<1) {
      OC_THROW(Oc_Exception(__FILE__,__LINE__,
                            "Vf_Ovf20FileHeader","WriteNPY",
                            "Invalid text width request"));
    }
    header += "{'descr': 'S" + std::to_string(textwidth) + "',";
  } else if(datastyle == vf_obin4) {
    header += "{'descr': '<f4',";
  } else if(datastyle == vf_obin8) {
    header += "{'descr': '<f8',";
  } else {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_Ovf20FileHeader","WriteNPY",
                          "Unrecognized output format"));
  }
  // Note: Use C (not Fortran) order to avoid confusion
  header += " 'fortran_order': False, 'shape': ";

  if(output_meshtype == vf_ovf20mesh_rectangular) {
    header += "(" + std::to_string(znodes.Value())
      + "," + std::to_string(ynodes.Value())
      + "," + std::to_string(xnodes.Value())
      + "," + std::to_string(veclen)
      + ") }";
  } else if(output_meshtype == vf_ovf20mesh_irregular) {
    header += "(" + std::to_string(nodecount)
      + "," + std::to_string(veclen + 3)
      + ") }";
  } else {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_Ovf20FileHeader","WriteNPY",
                          "Unrecognized mesh type."));
  }

  // Pad with spaces to 64 byte boundary
  size_t header_size = header.size();
  size_t full_header_size = 64 * ((header_size + 1 + 63)/64);
  /// "+ 1" includes space for newline
  for(size_t i=header_size;i<full_header_size-1;++i) {
    header += " "; // space pad
  }
  header += "\n";
  
  // Back fill HEADER_LEN and write header
  size_t HEADER_LEN = full_header_size - 10; // Here '10' is the prologue
  assert(HEADER_LEN<65536);
  header[8] = static_cast<unsigned char>(HEADER_LEN % 0xFF);
  header[9] = static_cast<unsigned char>((HEADER_LEN>>8) % 0xFF);
  if(Nb_WriteChannel(outchan,header.data(),full_header_size) 
     != OC_INDEX(full_header_size)) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_Ovf20FileHeader","WriteNPY",
                          "Write error (device full?)"));
  }

  // Dump data
  if(datastyle == vf_oascii) {
    // Text-style output /////////////////////////////////////////////
    const OC_INDEX reclen = veclen
      + (output_meshtype == vf_ovf20mesh_irregular ? 3 : 0);
    const OC_INDEX recbytes = reclen * textwidth;
    vector<char> workbuf(recbytes+1);
    vector<char> tmpbuf(textwidth+1);
    if(output_meshtype == vf_ovf20mesh_irregular) {
      OC_REAL8m x,y,z;
      for(OC_INDEX node=0;node<nodecount;++node) { 
        char* wptr = workbuf.data();
        mesh->GetCellCenter(node,x,y,z);
        snprintf(tmpbuf.data(),textwidth+1,textfmt,x);
        snprintf(wptr,textwidth+1,"%-*s",textwidth,
                 tmpbuf.data()); // Right pad with spaces
        wptr += textwidth;
        snprintf(tmpbuf.data(),textwidth+1,textfmt,y);
        snprintf(wptr,textwidth+1,"%-*s",textwidth,
                 tmpbuf.data()); // Right pad with spaces
        wptr += textwidth;
        snprintf(tmpbuf.data(),textwidth+1,textfmt,z);
        snprintf(wptr,textwidth+1,"%-*s",textwidth,
                 tmpbuf.data()); // Right pad with spaces
        wptr += textwidth;
        for(OC_INDEX ida=0;ida<data_array_count;++ida) {
          const OC_INDEX ddim = data_array[ida].vector_dimension;
          const OC_REAL8m* din = data_array[ida].data + node*ddim;
          for(OC_INDEX elt=0;elt<ddim;++elt) {
            snprintf(tmpbuf.data(),textwidth+1,textfmt,din[elt]);
            snprintf(wptr,textwidth+1,"%-*s",textwidth,
                     tmpbuf.data()); // Right pad with spaces
            wptr += textwidth;
          }
        }
        if(Nb_WriteChannel(outchan,workbuf.data(),recbytes)!= recbytes) {
          OC_THROW(Oc_Exception(__FILE__,__LINE__,
                                "Vf_Ovf20FileHeader","WriteNPY",
                                "Write error (device full?)"));
        }
      }
    } else { // Rectangular mesh output
      const OC_INDEX xc = xnodes.Value();
      const OC_INDEX yc = ynodes.Value();
      for(OC_INDEX k=0;k<znodes.Value();++k) { // Use C-order
        for(OC_INDEX j=0;j<ynodes.Value();++j) {
          for(OC_INDEX i=0;i<xnodes.Value();++i) {
            OC_INDEX node = k*yc*xc + j*xc + i;
            char* wptr = workbuf.data();
            for(OC_INDEX ida=0;ida<data_array_count;++ida) {
              const OC_INDEX ddim = data_array[ida].vector_dimension;
              const OC_REAL8m* din = data_array[ida].data + node*ddim;
              for(OC_INDEX elt=0;elt<ddim;++elt) {
                snprintf(tmpbuf.data(),textwidth+1,textfmt,din[elt]);
                snprintf(wptr,textwidth+1,"%-*s",textwidth,
                         tmpbuf.data()); // Right pad with spaces
                wptr += textwidth;
              }
            }
            if(Nb_WriteChannel(outchan,workbuf.data(),recbytes)!= recbytes) {
              OC_THROW(Oc_Exception(__FILE__,__LINE__,
                                    "Vf_Ovf20FileHeader","WriteNPY",
                                    "Write error (device full?)"));
            }
          } // for-i
        } // for-j
      } // for-k
    } // if(output_meshtype == ...)
  } else if(datastyle == vf_obin4) {
    // 4-byte binary output (little endian order) ////////////////////
    const OC_INDEX reclen = veclen
      + (output_meshtype == vf_ovf20mesh_irregular ? 3 : 0);
    const OC_INDEX recbytes = reclen * sizeof(OC_REAL4);
    vector<OC_REAL4> workbuf(reclen);
    if(output_meshtype == vf_ovf20mesh_irregular) {
      OC_REAL8m x,y,z;
      for(OC_INDEX node=0;node<nodecount;++node) {
        mesh->GetCellCenter(node,x,y,z);
        workbuf[0]=static_cast<OC_REAL4>(x);
        workbuf[1]=static_cast<OC_REAL4>(y);
        workbuf[2]=static_cast<OC_REAL4>(z);
        OC_REAL4* wptr = workbuf.data()+2;
        for(OC_INDEX ida=0;ida<data_array_count;++ida) {
          const OC_INDEX ddim = data_array[ida].vector_dimension;
          const OC_REAL8m* din = data_array[ida].data + node*ddim;
          for(OC_INDEX elt=0;elt<ddim;++elt) {
            *(++wptr) = static_cast<OC_REAL4>(din[elt]);
          }
        }
#if (OC_BYTEORDER != 4321)  // Input is MSB
        Oc_SwapWords4(workbuf.data(),workbuf.size());
#endif
        if(Nb_WriteChannel(outchan,reinterpret_cast<char*>(workbuf.data()),
                           recbytes)!= recbytes) {
          OC_THROW(Oc_Exception(__FILE__,__LINE__,
                                "Vf_Ovf20FileHeader","WriteNPY",
                                "Write error (device full?)"));
        }
      }
    } else { // Rectangular mesh output
      const OC_INDEX xc = xnodes.Value();
      const OC_INDEX yc = ynodes.Value();
      for(OC_INDEX k=0;k<znodes.Value();++k) { // Use C-order
        for(OC_INDEX j=0;j<ynodes.Value();++j) {
          for(OC_INDEX i=0;i<xnodes.Value();++i) {
            OC_INDEX node = k*yc*xc + j*xc + i;
            OC_REAL4* wptr = workbuf.data();
            for(OC_INDEX ida=0;ida<data_array_count;++ida) {
              const OC_INDEX ddim = data_array[ida].vector_dimension;
              const OC_REAL8m* din = data_array[ida].data + node*ddim;
              for(OC_INDEX elt=0;elt<ddim;++elt) {
                *(wptr++) = static_cast<OC_REAL4>(din[elt]);
              }
            }
#if (OC_BYTEORDER != 4321)  // Input is MSB
            Oc_SwapWords4(workbuf.data(),workbuf.size());
#endif
            if(Nb_WriteChannel(outchan,reinterpret_cast<char*>(workbuf.data()),
                               recbytes)!= recbytes) {
              OC_THROW(Oc_Exception(__FILE__,__LINE__,
                                    "Vf_Ovf20FileHeader","WriteNPY",
                                    "Write error (device full?)"));
            }
          } // for-i
        } // for-j
      } // for-k
    } // if(output_meshtype == ...)
  } else if(datastyle == vf_obin8) {
    // 8-byte binary output (little endian order) ////////////////////
    const OC_INDEX reclen = veclen
      + (output_meshtype == vf_ovf20mesh_irregular ? 3 : 0);
    const OC_INDEX recbytes = reclen * sizeof(OC_REAL8);
    vector<OC_REAL8> workbuf(reclen);
    if(output_meshtype == vf_ovf20mesh_irregular) {
      OC_REAL8m x,y,z;
      for(OC_INDEX node=0;node<nodecount;++node) {
        mesh->GetCellCenter(node,x,y,z);
        workbuf[0]=x; workbuf[1]=y; workbuf[2]=z;
        OC_REAL8* wptr = workbuf.data()+2;
        for(OC_INDEX ida=0;ida<data_array_count;++ida) {
          const OC_INDEX ddim = data_array[ida].vector_dimension;
          const OC_REAL8m* din = data_array[ida].data + node*ddim;
          for(OC_INDEX elt=0;elt<ddim;++elt) {
            *(++wptr) = static_cast<OC_REAL8>(din[elt]);
          }
        }
#if (OC_BYTEORDER != 4321)  // Input is MSB
        Oc_SwapWords8(workbuf.data(),workbuf.size());
#endif
        if(Nb_WriteChannel(outchan,reinterpret_cast<char*>(workbuf.data()),
                           recbytes)!= recbytes) {
          OC_THROW(Oc_Exception(__FILE__,__LINE__,
                                "Vf_Ovf20FileHeader","WriteNPY",
                                "Write error (device full?)"));
        }
      }
    } else { // Rectangular mesh output
      const OC_INDEX xc = xnodes.Value();
      const OC_INDEX yc = ynodes.Value();
      for(OC_INDEX k=0;k<znodes.Value();++k) { // Use C-order
        for(OC_INDEX j=0;j<ynodes.Value();++j) {
          for(OC_INDEX i=0;i<xnodes.Value();++i) {
            OC_INDEX node = k*yc*xc + j*xc + i;
            OC_REAL8* wptr = workbuf.data();
            for(OC_INDEX ida=0;ida<data_array_count;++ida) {
              const OC_INDEX ddim = data_array[ida].vector_dimension;
              const OC_REAL8m* din = data_array[ida].data + node*ddim;
              for(OC_INDEX elt=0;elt<ddim;++elt) {
                *(wptr++) = static_cast<OC_REAL8>(din[elt]);
              }
            }
#if (OC_BYTEORDER != 4321)  // Input is MSB
            Oc_SwapWords8(workbuf.data(),workbuf.size());
#endif
            if(Nb_WriteChannel(outchan,reinterpret_cast<char*>(workbuf.data()),
                               recbytes)!= recbytes) {
              OC_THROW(Oc_Exception(__FILE__,__LINE__,
                                    "Vf_Ovf20FileHeader","WriteNPY",
                                    "Write error (device full?)"));
            }
          } // for-i
        } // for-j
      } // for-k
    } // if(output_meshtype == ...)
  } // if(datastyle == ...)
}

////////////////////////////////////////////////////////////////////////

// Vf_Ovf20_MeshNodes wrapper for Vf_Mesh.

Vf_Mesh_MeshNodes::Vf_Mesh_MeshNodes(const Vf_Mesh* mesh)
  : type(vf_ovf20mesh_irregular), size(0),
    dimens(0,0,0), basept(0.,0.,0.), cellsize(0.,0.,0.)
{
  const Vf_GridVec3f* rectgrid = dynamic_cast<const Vf_GridVec3f*>(mesh);
  if(rectgrid) {
    type = vf_ovf20mesh_rectangular;
  }
 
  meshunit = String(mesh->GetMeshUnit());
  mesh->GetPreciseRange(meshrange);

  size = mesh->GetSize();
  values.SetSize(size);

  OC_REAL8m multiplier = mesh->GetValueMultiplier();

  if(!rectgrid) {
    Nb_Vec3<OC_REAL4> shortcellsize = mesh->GetApproximateCellDimensions();
    cellsize.x = shortcellsize.x; 
    cellsize.y = shortcellsize.y;
    cellsize.z = shortcellsize.z;

    centers.SetSize(size);

    Nb_LocatedVector<OC_REAL8> lv;
    Vf_Mesh_Index* key;
    OC_INDEX offset = 0;
    for(OC_INDEX index_code=mesh->GetFirstPt(key,lv);index_code;
        index_code=mesh->GetNextPt(key,lv)) {
      centers[offset] = lv.location;
      values[offset]  = multiplier*static_cast< Nb_Vec3<OC_REAL8m> >(lv.value);
      ++offset;
    }
    delete key;
  } else {
    OC_INDEX xdim,ydim,zdim;
    rectgrid->GetDimens(xdim,ydim,zdim);
    dimens.x = xdim; dimens.y = ydim; dimens.z = zdim;
    basept = rectgrid->GetBasePoint();
    cellsize = rectgrid->GetGridStep();

    Nb_LocatedVector<OC_REAL8> lv;
    Vf_Mesh_Index* key;
    OC_INDEX offset = 0;
    for(OC_INDEX index_code=mesh->GetFirstPt(key,lv);index_code;
        index_code=mesh->GetNextPt(key,lv)) {
      values[offset] = multiplier*static_cast< Nb_Vec3<OC_REAL8m> >(lv.value);
      ++offset;
    }
    delete key;
  }

}

void
Vf_Mesh_MeshNodes::DumpGeometry
(Vf_Ovf20FileHeader& header,
 Vf_Ovf20_MeshType reqtype) const
{
  // Mesh type
  if(reqtype != type &&
     reqtype != vf_ovf20mesh_irregular) {
    OC_THROW("Invalid output mesh type request;"
             " can't convert an irregular mesh to a rectangular mesh");
  }
  header.meshtype.Set(reqtype);

  // Mesh unit
  header.meshunit.Set(meshunit);

  // Mesh extents
  Nb_Vec3<OC_REAL8> mesh_minpt;
  Nb_Vec3<OC_REAL8> mesh_maxpt;
  meshrange.GetExtremes(mesh_minpt,mesh_maxpt);
  header.xmin.Set(mesh_minpt.x);
  header.ymin.Set(mesh_minpt.y);
  header.zmin.Set(mesh_minpt.z);
  header.xmax.Set(mesh_maxpt.x);
  header.ymax.Set(mesh_maxpt.y);
  header.zmax.Set(mesh_maxpt.z);

  header.xstepsize.Set(cellsize.x);
  header.ystepsize.Set(cellsize.y);
  header.zstepsize.Set(cellsize.z);

  if(reqtype == vf_ovf20mesh_irregular) {
    header.pointcount.Set(size);
  } else { // Rectangular grid
    header.xbase.Set(basept.x);
    header.ybase.Set(basept.y);
    header.zbase.Set(basept.z);
    header.xnodes.Set(dimens.x);
    header.ynodes.Set(dimens.y);
    header.znodes.Set(dimens.z);
  }
}

////////////////////////////////////////////////////////////////////////
