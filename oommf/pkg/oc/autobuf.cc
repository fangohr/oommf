/* FILE: autobuf.cc                   -*-Mode: c++-*-
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2014/02/22 23:12:30 $
 * Last modified by: $Author: donahue $
 */

/* Standard libraries */
#include <cassert>
#include <climits>
#include <cstring>
#include <cwctype>   // For iswspace

#include <string>
#include <vector>

/* Other classes and functions in this extension */
#include "autobuf.h"
#include "imports.h"

/* End includes */

const size_t Oc_AutoBuf::reclaim_bound=1024;   // See Oc_AutoBuf::Dup()
const size_t Oc_AutoBuf::reclaim_proportion=4;

#if (OC_SYSTEM_TYPE == OC_WINDOWS)
// Code page to use for conversions from multibyte to WCHAR.
// Tcl works with utf-8, so CP_UTF8 is a good guess, but
// another try is CP_ACP, which is the system default Windows
// ANSI code page. See the Windows MultiByteToWideChar function
// documentation for details.
# define OC_WIDECP CP_UTF8
#endif

void Oc_AutoBuf::Dup(const char* const_str)
{
  if(const_str==NULL) const_str = "";
  if(const_str == str) return; // Don't copy self!
  size_t needsize=strlen(const_str)+1;
  if(needsize>bufsize ||
     (bufsize>reclaim_bound && bufsize/reclaim_proportion>needsize)) {
    if(bufsize>0) delete[] str;
    bufsize=needsize;
    str=new char[bufsize];
    if(str==0) Tcl_Panic((char *)"No memory in Oc_AutoBuf::Dup");
  }
  strcpy(str,const_str);
}

void Oc_AutoBuf::Append(const char* const_str)
{
  // NOTE: Be careful to allow for the case const_str == str.
  // In particular, don't delete str before const_str is copied.

  if(const_str==NULL || const_str[0]=='\0') return; // Nothing to append

  size_t needsize = strlen(str) + strlen(const_str) + 1;

  char* oldstr = str;
  if(needsize>bufsize) {
    // Grab a larger buffer and copy current string over.  The code
    // below sizes the new buffer to an exact fit, but we may want to
    // add some padding, to make the case of multiple appends more
    // efficient.
    bufsize=needsize;
    str=new char[bufsize];
    if(str==0) Tcl_Panic((char *)"No memory in Oc_AutoBuf::Append");
    strcpy(str,oldstr);
  }
  strcat(str,const_str);
  if(oldstr != str) delete[] oldstr;
}


Oc_AutoBuf::Oc_AutoBuf(const Oc_AutoBuf& other)
  : bufsize(0), str(NULL)
{ // Make exact copy
  str = new char[other.bufsize];
  if(str==0) Tcl_Panic((char *)"No memory in Oc_AutoBuf copy constructor");
  bufsize = other.bufsize;
  for(size_t i=0;i<bufsize;++i) str[i] = other.str[i];
}

#if (OC_SYSTEM_TYPE == OC_WINDOWS)
  // Specialized conversion function for Windows.  Converts from wide
  // char to char.  This is mainly needed in the case UNICODE is
  // defined, because then TCHAR is a wide char (TCHAR is a normal
  // char otherwise.)
  Oc_AutoBuf::Oc_AutoBuf(const WCHAR* const_str) : bufsize(0), str(NULL)
  {
    size_t widesize = 1;
    if(const_str!=NULL) widesize = wcslen(const_str) + 1;
    if(widesize==1) {
      bufsize = 1;
      str = new char[bufsize];
      str[0] = '\0';
      return;
    }

    // Note: Lengths to/from WideCharToMultiByte are type int
    if(widesize>INT_MAX) {
      Tcl_Panic((char *)
       "Import buffer too big error in Oc_AutoBuf::Oc_AutoBuf(const WCHAR*)");
    }
    bufsize = static_cast<size_t>
      (WideCharToMultiByte(OC_WIDECP,0,const_str,int(widesize),NULL,0,NULL,NULL));
    if(bufsize==0) {
      Tcl_Panic((char *)
       "Conversion error in Oc_AutoBuf::Oc_AutoBuf(const WCHAR*)");
    }
    if(widesize>INT_MAX) {
      Tcl_Panic((char *)
       "Export buffer too big error in Oc_AutoBuf::Oc_AutoBuf(const WCHAR*)");
    }

    str = new char[bufsize];
    if(str==0) {
      Tcl_Panic((char *)"No memory in Oc_AutoBuf::Oc_AutoBuf(const WCHAR*)");
    }
    if(WideCharToMultiByte(OC_WIDECP, 0, const_str,int(widesize),
                           str,int(bufsize),NULL,NULL)==0) {
      Tcl_Panic((char *)"Conversion error in Oc_AutoBuf::Oc_AutoBuf(const WCHAR*)");
    }
  }
#endif // WINDOWS

Oc_AutoBuf& Oc_AutoBuf::operator=(const Oc_AutoBuf& other)
{ // Make exact copy
  if(other.str == str) return *this; // Don't copy self!
  if(bufsize != other.bufsize) {
    if(bufsize>0) delete[] str;
    bufsize = other.bufsize;
    str = new char[bufsize];
    if(str==0) Tcl_Panic((char *)"No memory in Oc_AutoBuf::operator=");
  }
  for(size_t i=0;i<bufsize;++i) str[i] = other.str[i];
  return *this;
}


char* Oc_AutoBuf::operator()(const char* const_str)
{
  Dup(const_str);
  return str;
}

void Oc_AutoBuf::SetLength(size_t newlength)
{ // bufsize = length + 1
  size_t newsize=newlength+1;
  if(newsize!=bufsize) {
    char* tempstr = new char[newsize];
    if(tempstr==0)
      Tcl_Panic((char *)"Insufficient memory in Oc_AutoBuf::SetLength");
    if(bufsize>0) {
      size_t minsize = ( bufsize<newsize ? bufsize : newsize );
      strncpy(tempstr,str,minsize-1);
      tempstr[newsize-1]='\0'; // Safety
      delete[] str;
    }
    str = tempstr;
    bufsize = newsize;
  }
}


#if (OC_SYSTEM_TYPE == OC_WINDOWS)
  // Specialized conversion function for Windows.  Converts from char
  // to wide char.  This is mainly needed in the case UNICODE is
  // defined, because then TCHAR is a wide char (TCHAR is a normal
  // char otherwise.)
void Oc_AutoWideBuf::Set(const char* const_str)
{
  size_t chkbufsize = static_cast<size_t>
    (MultiByteToWideChar(OC_WIDECP,0,const_str,-1,nullptr,0));
  if(chkbufsize==0) {
    Tcl_Panic((char *)"Conversion error in Oc_AutoWideBuf::Set");
  }
  if(chkbufsize>bufsize || 8*chkbufsize<bufsize) {
    delete[] wstr;
    bufsize = chkbufsize;
    wstr = new WCHAR[chkbufsize];
  }
  if(wstr==0) Tcl_Panic((char *)"No memory in Oc_AutoWideBuf::Set");
  assert(bufsize<=INT_MAX); // Note: bufsize is unsigned
  if(MultiByteToWideChar(OC_WIDECP,0,const_str,-1,wstr,
                         static_cast<int>(bufsize))==0) {
    Tcl_Panic((char *)"Conversion error in Oc_AutoWideBuf::Set");
  }
}

void Oc_AutoWideBuf::Set(const WCHAR* const_str)
{
  const size_t inlen = wcslen(const_str);
  if(inlen+1>bufsize || 8*(inlen+1)<bufsize) {
    delete[] wstr;
    bufsize = inlen+1;
    wstr = new WCHAR[bufsize];
  }
  for(size_t i=0;i<inlen;++i) {
    wstr[i] = const_str[i];
  }
  wstr[inlen] = static_cast<WCHAR>('\0');
}

WCHAR* Oc_AutoWideBuf::operator()(const char* const_str)
{
  Set(const_str);
  return wstr;
}

size_t Oc_AutoWideBuf::Trim()
{ // Removes whitespace from front and back. Returns new length.
  const WCHAR ZW = static_cast<WCHAR>('\0'); // String terminator (wide zero)
  size_t i=0,ia=0,ib=0;
  while(wstr[i] != ZW) {
    // Find first non-whitespace character
    if(!iswspace(wstr[i])) { break; }
    ++i;
  }
  ia=ib=i;
  while(wstr[++i] != ZW) {
    // Find last non-whitespace character
    if(!iswspace(wstr[i])) { ib=i; }
  }
  ++ib;
  const size_t inlen = i;
  if(ia>0) { // Overwrite leading whitespace with left shift
    for(i=ia;i<inlen;++i) {
      wstr[i-ia] = wstr[i];
    }
    ib -= ia; // Adjust index to end of string.
  }
  wstr[ib]=ZW; // Add end marker
  return ib;
}

size_t Oc_AutoWideBuf::NormalizeEOLs()
{ // Changes \r\n to \n. Returns new length.
  const WCHAR CR = static_cast<WCHAR>('\r');
  const WCHAR NL = static_cast<WCHAR>('\n');
  const WCHAR ZW = static_cast<WCHAR>('\0');
  size_t ia = 1; // Leading index
  size_t ib = 0; // Trailing index
  while(wstr[ia] != ZW) {
    if(wstr[ia] != NL || wstr[ib] != CR) {
      ++ib;
    }
    wstr[ib] = wstr[ia];
    ++ia;
  }
  wstr[++ib] = ZW;
  return ib;
}


std::string Oc_AutoWideBuf::GetUtf8Str() const
{ // Returns multibyte utf-8 character version of string
  int chkbufsize
    = WideCharToMultiByte(CP_UTF8,0,wstr,-1,nullptr,0,NULL,nullptr);
  if(chkbufsize==0) {
    Tcl_Panic((char *)"Conversion error in Oc_AutoWideBuf::GetUtf8Str");
  }
  std::vector<char> outbuf(chkbufsize,'\0');
  WideCharToMultiByte(CP_UTF8,0,wstr,-1,outbuf.data(),chkbufsize,NULL,nullptr);
  return std::string(outbuf.data());
}

#endif // WINDOWS
