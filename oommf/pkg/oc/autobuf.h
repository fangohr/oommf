/* FILE: autobuf.h                    -*-Mode: c++-*-
 *
 * This class was initially created to convert string literals into
 * the writable strings needed as arguments to old-versions of many
 * Tcl library calls.  The Oc_AutoBuf class is now used in many areas
 * of the code where a simple dynamic buffer is needed with automatic
 * and exception-safe cleanup.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2012/09/27 23:06:52 $
 * Last modified by: $Author: donahue $
 */

#ifndef _OC_AUTOBUF
#define _OC_AUTOBUF

#include <cstddef>  // size_t definition
#include <cstring>  // strlen declaration

#include <string>

#include "imports.h" // OC_USING_CONST84 definition

/* End includes */     /* Optional directive to pimake */

OC_USE_STRING;

#define OC_STRINGIFY(x) OC_STRINGIFY1(x)
#define OC_STRINGIFY1(x) #x
#define OC_JOIN(a,b) OC_JOIN1(a,b)
#define OC_JOIN1(a,b) a##b

class Oc_AutoBuf
{
private:
  static const size_t reclaim_bound;
  static const size_t reclaim_proportion;
  size_t bufsize; // Length of buffer pointed to by str. ( >= strlen(str)+1.)
  char *str;
public:
  ~Oc_AutoBuf() { delete[] str; }

  Oc_AutoBuf() : bufsize(0), str(NULL) { Dup(""); }
  Oc_AutoBuf(const char* const_str)
    : bufsize(0), str(NULL) { Dup(const_str); }

  Oc_AutoBuf(const Oc_AutoBuf& other);
  // Note: Proper copy constructor is needed for use of Oc_AutoBuf as
  // arguments or as members of arguments to C++ exception "throw"
  // commands.  It doesn't hurt to have a proper assignment operator
  // either.

  Oc_AutoBuf(const String& in_string)
    : bufsize(0), str(NULL) { Dup(in_string.c_str()); }

  // The following constructor is mainly of use when using
  // Oc_AutoBuf as raw scratch space.
  Oc_AutoBuf(size_t initial_length)
    : bufsize(0), str(NULL) { SetLength(initial_length); }


#if (OC_SYSTEM_TYPE == OC_WINDOWS)
  // Specialized conversion function for Windows.  Converts from wide
  // char to char.  This is mainly needed in the case UNICODE is
  // defined, because then TCHAR is a wide char (TCHAR is a normal
  // char otherwise.)
  Oc_AutoBuf(const WCHAR* const_str);
#endif // WINDOWS

  operator char*() const { return str; }
  char* GetStr() const { return str; }
  char* operator()(const char* const_str);
  void Dup(const char* const_str);
  void Append(const char* const_str);
  void Append(const Oc_AutoBuf& other) { Append(other.str); }
  void SetLength(size_t new_length); // bufsize = length + 1
  size_t GetLength() { return bufsize-1; }

  size_t GetStrlen() { return strlen(str); }

  Oc_AutoBuf& operator=(const Oc_AutoBuf& other);
  Oc_AutoBuf& operator+=(const Oc_AutoBuf& other) {
    Append(other); return *this;
  }

  const char& operator[](int i) const {
#ifndef NDEBUG
    if(i<0 || static_cast<size_t>(i) > bufsize) {
#if OC_USING_CONST84
      Tcl_Panic("%s","Out-of-bounds access in"
                " Oc_AutoBuf::operator[] const");
#else
      Tcl_Panic((char*)("%s"),"Out-of-bounds access in"
                " Oc_AutoBuf::operator[] const");
#endif
    }
#endif
    return str[i];
  }
  char& operator[](int i) {
#ifndef NDEBUG
    if(i<0 || static_cast<size_t>(i) > bufsize) {
#if OC_USING_CONST84
      Tcl_Panic("%s","Out-of-bounds access in"
                " Oc_AutoBuf::operator[]");
#else
      Tcl_Panic((char*)("%s"),"Out-of-bounds access in"
                " Oc_AutoBuf::operator[]");
#endif
    }
#endif
    return str[i];
  }

};

inline const Oc_AutoBuf operator+(const Oc_AutoBuf& a,const Oc_AutoBuf& b)
{
  Oc_AutoBuf c(a);
  c.Append(b);
  return c;
}


// NOTE: OC_USING_CONST84 is #define'd in oc/imports.h
// The OC_CONST84_CHAR macro can be used to wrap "const char*"
// arguments to the Tcl API that changed from char* to const char*
// between Tcl 8.3 and 8.4.  On Tcl 8.4 and later OC_CONST84_CHAR
// translates to a nop.  On Tcl 8.3 and earlier OC_CONST84_CHAR
// copies the const char* into a temporary Oc_AutoBuf, and forwards
// a pointer to the char buf in Oc_AutoBuf.
#if OC_USING_CONST84
# define OC_CONST84_CHAR(x) (x)
#else
# define OC_CONST84_CHAR(x) (Oc_AutoBuf(x).GetStr())
#endif


#if (OC_SYSTEM_TYPE == OC_WINDOWS)
  // Specialized conversion function for Windows.  Converts from char
  // to wide char.  This is mainly needed in the case UNICODE is
  // defined, because then TCHAR is a wide char (TCHAR is a normal
  // char otherwise.)
class Oc_AutoWideBuf
{
private:
  size_t bufsize; // Length of buffer pointed to by str. ( >= strlen(str)+1.)
  WCHAR *wstr;
public:
  ~Oc_AutoWideBuf() { if(bufsize>0) { delete[] wstr; } }
  Oc_AutoWideBuf()
    : bufsize(0), wstr(NULL) { Set(""); }
  Oc_AutoWideBuf(const char* const_str)
    : bufsize(0), wstr(NULL) { Set(const_str); }
  Oc_AutoWideBuf(const WCHAR* const_wstr)
    : bufsize(0), wstr(NULL) { Set(const_wstr); }

  void Set(const char* const_str);
  void Set(const WCHAR* const_str);

  operator WCHAR*() const { return wstr; }
  WCHAR* GetStr() const { return wstr; }
  WCHAR* operator()(const char* const_str);

  // Tweaks. Return value is length of new string.
  size_t Trim(); // Removes whitespace from front and back
  size_t NormalizeEOLs(); // Changes \r\n to \n

  String GetUtf8Str() const; // Returns multibyte
  // utf-8 character version of string

  // The following are (to date) undefined
  Oc_AutoWideBuf(const Oc_AutoWideBuf& other);
  Oc_AutoWideBuf& operator=(const Oc_AutoWideBuf& other);

};

// Code to convert from char* to TCHAR*
// WARNING: If UNICODE is not defined (the usual case for us), then the
//    Oc_AutoTBuf typedef resolves to Oc_AutoBuf.  This is problematic
//    in that the other case (less common for us) the Oc_AutoTBuf
//    typedef resolves to Oc_AutoWideBuf, which has *less* total
//    functionality than the former.  In principle this should not be
//    a problem, because Oc_AutoWideBuf provides all the functionality
//    intended for Oc_AutoTBuf, but the risk is that an unwary
//    programmer may inadvertently use Oc_AutoBuf functionality on an
//    Oc_AutoTBuf variable, and this error won't get caught until
//    someone (perhaps in the field) tries to do a build with UNICODE
//    defined.

#ifdef UNICODE
typedef Oc_AutoWideBuf Oc_AutoTBuf;
#else
typedef Oc_AutoBuf Oc_AutoTBuf;
#endif

#endif // WINDOWS

////////////////////////////////////////////////////////////////////////
// Exception-safe wrapper for Tcl_DString. Note: tcl.h is #included
// via oc/imports.h (q.v.)
class Oc_TclDString {
public:
  Oc_TclDString() {
    Tcl_DStringInit(&buffer);
  }
  Oc_TclDString(const char* cptr) {
    Tcl_DStringInit(&buffer);
    Tcl_DStringAppend(&buffer,cptr,-1);
  }
  ~Oc_TclDString() {
    Tcl_DStringFree(&buffer);
  }
  // Copy operations
  Oc_TclDString(const Oc_TclDString& other) {
    Tcl_DStringInit(&buffer);
    Tcl_DStringAppend(&buffer,Tcl_DStringValue(&(other.buffer)),
                      Tcl_DStringLength(&(other.buffer)));
  }
  Oc_TclDString& operator=(const Oc_TclDString& other) {
    Tcl_DStringSetLength(&buffer,0);
    Tcl_DStringAppend(&buffer,Tcl_DStringValue(&(other.buffer)),
                      Tcl_DStringLength(&(other.buffer)));
    return *this;
  }
  Oc_TclDString& operator=(const char* cptr) {
    Tcl_DStringSetLength(&buffer,0);
    Tcl_DStringAppend(&buffer,cptr,-1);
    return *this;
  }
  // Move operators will default to copies. To do better would
  // involve mucking around inside the Tcl_DString stucture.

  // NB: This class is responsible for initializing and freeing
  // the Tcl_DString buffer. Clients that use the following
  // functions to gain access to the underlying buffer MUST
  // NOT initialize or free the Tcl-managed memory!
  Tcl_DString* GetDString() { return &buffer; }
  const Tcl_DString* GetDString() const { return &buffer; }
  const char* GetStr() const { return Tcl_DStringValue(&buffer); }
  void Clear() { // Sets buffer to an empty string.
    Tcl_DStringSetLength(&buffer,0);
  }

  // Convenience functions. Note that ReadLine, in contrast to Tcl_Gets,
  // overwrites any existing string value. Use AppendLine to mimic
  // Tcl_Gets behavior.
  int ReadLine(Tcl_Channel inchan) {
    // Return value is the number of bytes read. -1 indicates an error,
    // which can be retrieved via Tcl_GetErrno() or
    // Tcl_ErrnoMsg(Tcl_GetErrno()).
    Tcl_DStringSetLength(&buffer,0);
    return Tcl_Gets(inchan,&buffer);
  }
  int AppendLine(Tcl_Channel inchan) {
    // Return value is the number of bytes read. -1 indicates an error,
    // which can be retrieved via Tcl_GetErrno() or
    // Tcl_ErrnoMsg(Tcl_GetErrno()).
    return Tcl_Gets(inchan,&buffer);
  }
  char* AppendString(const char* str) {
    Tcl_DStringAppend(&buffer,str,-1);
    return Tcl_DStringValue(&buffer);
  }
  char* AppendElement(const char* str) {
    Tcl_DStringAppendElement(&buffer,str);
    return Tcl_DStringValue(&buffer);
  }
  Oc_TclDString& operator+=(const Oc_TclDString& rhs) {
    AppendString(Tcl_DStringValue(&(rhs.buffer)));
    return *this;
  }
  Oc_TclDString& operator+=(const char* rhs) {
    AppendString(rhs);
    return *this;
  }
private:
  Tcl_DString buffer;
};

////////////////////////////////////////////////////////////////////////
// Exception-safe wrapper for Tcl_Obj. In this iterations, each
// Oc_TclObj object has its own Tcl_Obj copy, i.e., there is no sharing
// of the underlying Tcl_Obj's via Tcl's reference counting mechanism.
// Note: tcl.h is #included via oc/imports.h (q.v.)
class Oc_TclObj {
public:
  // Default constructor
  Oc_TclObj() : objPtr(Tcl_NewObj()) {
    Tcl_IncrRefCount(objPtr);
  }
  ~Oc_TclObj() {
    Tcl_DecrRefCount(objPtr);
  }

  // Initializing constructors. Add more as needed.
  Oc_TclObj(const Oc_TclObj& other) : objPtr(Tcl_DuplicateObj(other.objPtr))
  { Tcl_IncrRefCount(objPtr); }
  Oc_TclObj(const char* cptr) : objPtr(Tcl_NewStringObj(cptr,-1))
  { Tcl_IncrRefCount(objPtr); }
  Oc_TclObj(Oc_TclDString& dstr) : objPtr(Tcl_NewStringObj(dstr.GetStr(),-1))
  { Tcl_IncrRefCount(objPtr); }


  // Copy operators. Add more as needed.
  Oc_TclObj& operator=(const Oc_TclObj& other) {
    Tcl_DecrRefCount(objPtr);
    objPtr = Tcl_DuplicateObj(other.objPtr);
    Tcl_IncrRefCount(objPtr);
    return *this;
  }
  Oc_TclObj& operator=(const char* cptr) {
    Tcl_DecrRefCount(objPtr);
    objPtr = Tcl_NewStringObj(cptr,-1);
    Tcl_IncrRefCount(objPtr);
    return *this;
  }
  Oc_TclObj& operator=(const Oc_TclDString& dstr) {
    Tcl_DecrRefCount(objPtr);
    objPtr = Tcl_NewStringObj(dstr.GetStr(),-1);
    Tcl_IncrRefCount(objPtr);
    return *this;
  }

  // Move operators default to copies.

  // External access
  Tcl_Obj* GetObj() { return objPtr; }
  const Tcl_Obj* GetObj() const { return objPtr; }
  const char* GetString() {
    // Note: Non-const as GetString may need to generate the string
    // representation. objPtr could be made mutable is a const version
    // is needed, but this should be given some thought...
    return Tcl_GetString(objPtr);
  }

private:
  Tcl_Obj* objPtr;
};

#endif /* _OC_AUTOBUF */

