/* FILE: exterror.h                 -*-Mode: c++-*-
 *
 * Error handling class for base OXS extension class.
 *
 */

#ifndef _OXS_EXTERROR
#define _OXS_EXTERROR

#include <string>

#include "oc.h"
OC_USE_STD_NAMESPACE;  // Specify std namespace, if supported
OC_USE_STRING;

class Oxs_Ext; // Forward declaration

/* End includes */

struct Oxs_ExtError {
  protected:
    String msg;
  public:
    Oxs_ExtError(const char* errmsg);
    Oxs_ExtError(const String& errmsg);
    Oxs_ExtError(const Oxs_Ext* obj,const char* errmsg);
    Oxs_ExtError(const Oxs_Ext* obj,const String& errmsg);
    virtual ~Oxs_ExtError() {}
    operator String() const { return msg; }
    const char* c_str() const { return msg.c_str(); }
    void Prepend(const String& prefix);
    void Postpend(const String& suffix);
  };

  // A couple specializations of Oxs_ExtError for MakeNew().  We
  // don't need Oxs_ExtError(const Oxs_Ext*, const char*) constructor
  // support, because there is no *this available inside static
  // MakeNew().
  struct Oxs_ExtErrorUnregisteredType : public Oxs_ExtError {
    Oxs_ExtErrorUnregisteredType(const char* errmsg)
      : Oxs_ExtError(errmsg) {}
  };
  struct Oxs_ExtErrorConstructFailure : public Oxs_ExtError {
    Oxs_ExtErrorConstructFailure(const char* errmsg)
      : Oxs_ExtError("") {
      msg = errmsg; // Pass errmsg up without prefixing
      /// with an "Oxs_Ext ERROR" tag.  To see why this
      /// is desirable, check the MakeNew() source---the
      /// only place ConstructFailure occurs it is as a
      /// rethrow of an Oxs_ExtError.
    }
  };

#endif // _OXS_EXTERROR
