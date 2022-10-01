/* FILE: dstring.h                    -*-Mode: c++-*-
 *
 * Generic C++ Dynamic String class
 * 
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2013/05/22 07:15:32 $
 * Last modified by: $Author: donahue $
 */

#ifndef _NB_DSTRING
#define _NB_DSTRING

#include <cstring>
#include "dlist.h"
#include "evoc.h"

/* End includes */     /* Optional directive to build.tcl */

//////////////////////////////////////////////////////////////////////////
// Dynamic String class
//  NOTE: This implementation grows the char buffer "str" as needed,
//        but only shrinks it if Free() is explicitly called.
class Nb_DString
{
  friend OC_INT4m StringCompare(const Nb_DString &,
			     const Nb_DString &); // Replicates strcmp
  friend OC_INDEX StringSpan(const Nb_DString &,
			   const char *); // Replicates strspn
  friend Nb_DString operator+(const Nb_DString& a,
			      const Nb_DString& b);
private:
  static const ClassDoc class_doc;
  OC_INDEX bufsize;  // Length of buffer pointed to by str.
  char *str;
  void Empty(void);
  void ExtendBuf(OC_INDEX newsize);
public:
  Nb_DString(void);                       // Default constructor
  Nb_DString(const Nb_DString &copyobj);  // Copy constructor
  Nb_DString(const char *cptr);

  Nb_DString(OC_INDEX _bufsize);  // Allocates buffer of length _bufsize,
  /// and puts a null string in it. NOTE: A string of length "n"
  /// requires a buffer of length "n+1".
  Nb_DString& Dup(const char *cptr);
  Nb_DString& operator=(const Nb_DString &rhs);  // Duplicates string
  Nb_DString& operator=(const char *cptr);      // Ditto
  ~Nb_DString();

#if (OC_SYSTEM_TYPE == OC_WINDOWS)
  Nb_DString(const WCHAR *cptr) : str(NULL) {
    Empty();
    operator=(Oc_AutoBuf(cptr));
  }
  Nb_DString& operator=(const WCHAR *cptr) {
    return operator=(Oc_AutoBuf(cptr));
  }
#endif

  const char* GetStr(void) const { return (const char *)str; }

  char& operator[](OC_INDEX offset) {
    if(offset>0 && offset+1>bufsize) ExtendBuf(offset+1);
    return str[offset];
  }
  // The following [] operator can be used on 'const Nb_DString' objects.
  const char& operator[](OC_INDEX offset) const { return str[offset]; }

  operator const char*() const { return (const char *)str; }
  // Note: In the last 3 functions, the returned pointer may become
  //  invalid if later any of the non-const Nb_DString member functions
  //  are called.

  // Length *not* including trailing null character.
  OC_INDEX Length() const {
    size_t size = strlen(str);
    OC_INDEX result = static_cast<OC_INDEX>(size);
    assert(result>=0 && size == size_t(result));
    return result;
  }

  OC_INDEX GetCapacity() const { return bufsize; } // For debugging

  // Append functions; grows str as needed.  The char version appends
  // up to the first '\0' character or maxlen, which ever happens first.
  // (Set maxlen=0 to remove maxlen check.) Either way the last char in
  // this->str is set to '\0'.  Both functions return *this.
  Nb_DString& Append(const Nb_DString& appendstr);
  Nb_DString& Append(const char *appendarr,OC_INDEX maxlen=0);

  Nb_DString& operator+=(const Nb_DString& appendstr) {
    return Append(appendstr);
  }
  Nb_DString& operator+=(const char *appendarr) {
    return Append(appendarr);
  }

  // Appends the contents of argv[0] through argv[argc-1] to
  // *this, inserting a single space between elements.
  Nb_DString& AppendArgs(int argc,const char **argv);
  Nb_DString& AppendArgs(int argc,const Nb_DString *argv);

  // Merges the contents of argv[0] through argv[argc-1],
  // maintaining proper Tcl list structure, and sets the
  // result into *this.  (Based on the Tcl_Merge function.)
  Nb_DString& MergeArgs(int argc,const Nb_DString *argv);
  Nb_DString& MergeArgs(const Nb_List<Nb_DString>& argv);

  // Functions to trim leading & trailing whitespace (as determined by
  // 'isspace' function) from string.
  Nb_DString& TrimLeft();
  Nb_DString& TrimRight();
  Nb_DString& Trim();

  // Function to collapse runs of whitespace into single blanks.
  // Also removes leading and trailing whitespace.
  Nb_DString& CollapseStr();

  // Function to remove all whitespace
  Nb_DString& StripWhite();

  // Interface to C tolower function
  Nb_DString& ToLower();

  void Free(void) { Empty(); }
};

// Normal C char[] functions.  Used by above Nb_DString class, but
// may also be called from outside for operations on non-Nb_DString
// char arrays.
void LowerString(char *str);
void CollapseStr(char *str);

#endif // _NB_DSTRING
