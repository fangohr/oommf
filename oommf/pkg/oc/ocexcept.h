/* FILE: except.h                    -*-Mode: c++-*-
 *
 *   Simple exception object for C++ exceptions.
 * 
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2015/05/13 02:03:00 $
 * Last modified by: $Author: donahue $
 */

#ifndef _OC_EXCEPT
#define _OC_EXCEPT

#include <stdarg.h>
#include <string>

#include "autobuf.h"
/* End includes */     /* Optional directive to pimake */

////////////////////////////////////////////////////////////////////////
// A simple throw wrapper
#define OC_THROWTEXT(text)                                       \
  OC_THROW(Oc_AutoBuf("ERROR\n"                                  \
               "   in file: " __FILE__ "\n"                      \
               "   at line: " OC_STRINGIFY(__LINE__) "\n")       \
           + Oc_AutoBuf(text) )

////////////////////////////////////////////////////////////////////////
// Simple exception object.  Sample usage is:
//
//   OC_THROW(Oc_Exception(__FILE__,__LINE__,"My_Class","My_Function",
//            4096,"Too many fubars (%d) for input string %.3500s",
//            fubar_count,fubar_input);
//
// The fifth argument to the Oc_Exception constructor, errmsg_size (here
// 4096), must be large enough to hold the result of evaluating the
// format string errfmt with the appended arguments.  It is strongly
// recommended that any %s format directives include a precision
// setting (".3500" in the above example) to protect against overflow
// in filling the extended error message buffer.
//
class Oc_Exception {
public:
  Oc_Exception
  (const char* file_in,      // File from which exception is thrown
   int lineno_in,            // Line number from which exception is thrown
   const char* classname_in, // Name of class throwing exception
   const char* funcname_in,  // Name of function throwing exception
   size_t errmsg_size,       // Buffer size needed to hold extd. error msg
   const char* errfmt,       // Format string for extended error message
   ...);                     // arguments for errfmt
  // Any of the char* may be set to NULL, in which case an empty string
  // will be substituted.

  // Version of the previous where the error message is a pure string
  // not requiring any formatting.
  Oc_Exception
  (const char* file_in,      // File from which exception is thrown
   int lineno_in,            // Line number from which exception is thrown
   const char* classname_in, // Name of class throwing exception
   const char* funcname_in,  // Name of function throwing exception
   const char* errmsg_in);   // Error message

  virtual ~Oc_Exception() {} // Make sure all child classes get
  /// their destructor called on destruction of base class ptr.

  // ConstructMessage is intended to build a generic error message
  // out of the data stored in this object.  We may add interfaces
  // to the private data if there arises a need for specially
  // tailored error messages.  As a convenience, the return value is
  // a pointer to the import msg buffer.
  const char* ConstructMessage(Oc_AutoBuf& msg) const;

  // Routines to modify errmsg.  These are intended for use
  // by routines that catch and rethrow the exception, in
  // order to add additional info.
  void PrependMessage(const char* prefix);
  void PostpendMessage(const char* suffix);

protected:
  void SetErrMsg(size_t errmsg_size,const char* errfmt,va_list ap);

private:
  Oc_AutoBuf file;      // File from which exception is thrown
  int        lineno;    // Line number from which exception is thrown
  Oc_AutoBuf classname; // Name of class throwing exception, or
  /// empty if exception is being thrown from outside a class.
  Oc_AutoBuf funcname;  // Name of function throwing exception
  Oc_AutoBuf errmsg;    // Extended error information
};

// Error reported by Tcl interpreter
class Oc_TclError : public Oc_Exception {
private:
  Oc_AutoBuf error_info;
  Oc_AutoBuf error_code;
public:
  const char* ErrorInfo() { return error_info.GetStr(); }
  const char* ErrorCode() { return error_code.GetStr(); }
  const char* MessageType() const { return "Tcl error"; }
  Oc_TclError
  (const char* file,      // File from which exception is thrown
   int lineno,            // Line number from which exception is thrown
   const char* classname, // Name of class throwing exception
   const char* funcname,  // Name of function throwing exception
   const char* ei,        // Tcl error info
   const char* ec,        // Tcl error code
   size_t errmsg_size,       // Buffer size needed to hold extd. error msg
   const char* errfmt,    // Format string for extended error message
   ...)                   // arguments for errfmt
    : Oc_Exception(file,lineno,classname,funcname,0,0),
      error_info(ei), error_code(ec)
  {
    // Finish initialization by setting formatted error message
    va_list arg_ptr;
    va_start(arg_ptr,errfmt);
    SetErrMsg(errmsg_size,errfmt,arg_ptr);
    va_end(arg_ptr);
  }
};

#endif // _OC_EXCEPT
