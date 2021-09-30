/* FILE: oxsexcept.h                 -*-Mode: c++-*-
 *
 * Exception classes for Oxs.
 *
 */

#ifndef _OXS_EXCEPT
#define _OXS_EXCEPT

#include <string>
#include <exception>

#include "oc.h"

OC_USE_STD_NAMESPACE;  // Specify std namespace, if supported
OC_USE_STRING;

/* End includes */

// Dummy declarations that should move over to oommf/pkg/oc

#define OXS_THROW(except,text)                                          \
  throw except(String("ERROR\n"                                         \
                      "   in file: " __FILE__ "\n"                      \
                      "   at line: " OC_STRINGIFY(__LINE__) "\n")       \
               + text )

#define OXS_TCLTHROW(text,ei,ec)                                        \
  throw Oxs_TclError(String("ERROR\n"                                   \
                            "   in file: " __FILE__ "\n"                \
                            "   at line: " OC_STRINGIFY(__LINE__) "\n") \
                     + text, ei, ec)

#define OXS_EXTTHROW(except,text,count)                                 \
  throw except(text,"",this->InstanceName(),__FILE__,__LINE__,count)


// Oxs exceptions
class Oxs_Exception : public std::exception {
private:
  String msg;

  // Optional extended info
  String subtype;
  String src;
  String file;  // File where exception occurred
  int line;     // Line in "file" where exception occurred
  int suggested_display_count; // Maximum recommended number of times
  /// to show this message; may be used by non-interactive loggers.
  /// Use -1 to indicate no limit.

  mutable String workspace; // Buffer for building compound error string;
  /// Exists for lifetime of object.  This is not mutex locked, so any
  /// instance should only be used in one thread.
public:
  Oxs_Exception(const String& msgtext);

  // Extended version
  Oxs_Exception(const String& msgtext,
                const String& msgsubtype,
                const String& msgsrc,
                const char* msgfile,int msgline,
                int sdcount);

  virtual ~Oxs_Exception() {}

  void SetSubtype(const String& st) { subtype = st; }

  virtual const char* MessageType() const =0;

  String MessageText() const { return msg; }
  void Prepend(const String& prefix) { msg = prefix + msg; }
  void Postpend(const String& suffix) { msg += suffix; }

  // Extended info.  If not available, then string quantities
  // will be empty, and integer quantities will be -1.
  String MessageSrc() const { return src; }
  String MessageSubtype() const { return subtype; }
  String MessageFile() const { return file; }
  int MessageLine() const { return line; }

  String FullType() const;  // MessageType + subtype
  String FullSrc() const;   // src + file + line

  int DisplayCount() const { return suggested_display_count; }
  String DisplayCountAsString() const;

  const char* what() const noexcept; // Virtual member from std::exception
};

// Bad pointer
class Oxs_BadPointer : public Oxs_Exception {
public:
  const char* MessageType() const { return "Bad pointer"; }
  Oxs_BadPointer(const String& text) : Oxs_Exception(text) {}
  Oxs_BadPointer(const String& msgtext,
                 const String& msgsubtype,
                 const String& msgsrc,
                 const char* msgfile,int msgline,int sd_count)
    : Oxs_Exception(msgtext,msgsubtype,msgsrc,msgfile,msgline,sd_count) {}
};

// Bad code
class Oxs_BadCode : public Oxs_Exception {
public:
  const char* MessageType() const { return "Bad code"; }
  Oxs_BadCode(const String& text) : Oxs_Exception(text) {}
  Oxs_BadCode(const String& msgtext,
              const String& msgsubtype,
              const String& msgsrc,
              const char* msgfile,int msgline,int sd_count)
    : Oxs_Exception(msgtext,msgsubtype,msgsrc,msgfile,msgline,sd_count) {}
};

// Data structure lock action failure
class Oxs_BadLock : public Oxs_Exception {
public:
  const char* MessageType() const { return "Bad lock"; }
  Oxs_BadLock(const String& text) : Oxs_Exception(text) {}
  Oxs_BadLock(const String& msgtext,
              const String& msgsubtype,
              const String& msgsrc,
              const char* msgfile,int msgline,int sd_count)
    : Oxs_Exception(msgtext,msgsubtype,msgsrc,msgfile,msgline,sd_count) {}
};

// Data structure index invalid
class Oxs_BadIndex : public Oxs_Exception {
public:
  const char* MessageType() const { return "Bad data index"; }
  Oxs_BadIndex(const String& text) : Oxs_Exception(text) {}
  Oxs_BadIndex(const String& msgtext,
               const String& msgsubtype,
               const String& msgsrc,
               const char* msgfile,int msgline,int sd_count)
    : Oxs_Exception(msgtext,msgsubtype,msgsrc,msgfile,msgline,sd_count) {}
};

// Resource allocation failure
class Oxs_BadResourceAlloc : public Oxs_Exception {
public:
  const char* MessageType() const { return "Resource allocation failure"; }
  Oxs_BadResourceAlloc(const String& text) : Oxs_Exception(text) {}
  Oxs_BadResourceAlloc(const String& msgtext,
                       const String& msgsubtype,
                       const String& msgsrc,
                       const char* msgfile,int msgline,int sd_count)
    : Oxs_Exception(msgtext,msgsubtype,msgsrc,msgfile,msgline,sd_count) {}
};

// Resource deallocation failure
class Oxs_BadResourceDealloc : public Oxs_Exception {
public:
  const char* MessageType() const { return "Resource deallocation failure"; }
  Oxs_BadResourceDealloc(const String& text) : Oxs_Exception(text) {}
  Oxs_BadResourceDealloc(const String& msgtext,
                         const String& msgsubtype,
                         const String& msgsrc,
                         const char* msgfile,int msgline,int sd_count)
    : Oxs_Exception(msgtext,msgsubtype,msgsrc,msgfile,msgline,sd_count) {}
};

// Overflow detected
class Oxs_Overflow : public Oxs_Exception {
public:
  const char* MessageType() const { return "Overflow"; }
  Oxs_Overflow(const String& text) : Oxs_Exception(text) {}
  Oxs_Overflow(const String& msgtext,
               const String& msgsubtype,
               const String& msgsrc,
               const char* msgfile,int msgline,int sd_count)
    : Oxs_Exception(msgtext,msgsubtype,msgsrc,msgfile,msgline,sd_count) {}
};

// Device full
class Oxs_DeviceFull : public Oxs_Exception {
public:
  const char* MessageType() const { return "Device full"; }
  Oxs_DeviceFull(const String& text) : Oxs_Exception(text) {}
  Oxs_DeviceFull(const String& msgtext,
                 const String& msgsubtype,
                 const String& msgsrc,
                 const char* msgfile,int msgline,int sd_count)
    : Oxs_Exception(msgtext,msgsubtype,msgsrc,msgfile,msgline,sd_count) {}
};

// Bad parameter
class Oxs_BadParameter : public Oxs_Exception {
public:
  const char* MessageType() const { return "Bad parameter"; }
  Oxs_BadParameter(const String& text) : Oxs_Exception(text) {}
  Oxs_BadParameter(const String& msgtext,
                   const String& msgsubtype,
                   const String& msgsrc,
                   const char* msgfile,int msgline,int sd_count)
    : Oxs_Exception(msgtext,msgsubtype,msgsrc,msgfile,msgline,sd_count) {}
};

// Bad user input
class Oxs_BadUserInput : public Oxs_Exception {
public:
  const char* MessageType() const { return "Bad user input"; }
  Oxs_BadUserInput(const String& text) : Oxs_Exception(text) {}
  Oxs_BadUserInput(const String& msgtext,
                   const String& msgsubtype,
                   const String& msgsrc,
                   const char* msgfile,int msgline,int sd_count)
    : Oxs_Exception(msgtext,msgsubtype,msgsrc,msgfile,msgline,sd_count) {}
};

// Bad data
class Oxs_BadData : public Oxs_Exception {
public:
  const char* MessageType() const { return "Bad data"; }
  Oxs_BadData(const String& text) : Oxs_Exception(text) {}
  Oxs_BadData(const String& msgtext,
              const String& msgsubtype,
              const String& msgsrc,
              const char* msgfile,int msgline,int sd_count)
    : Oxs_Exception(msgtext,msgsubtype,msgsrc,msgfile,msgline,sd_count) {}
};

// Incomplete initialization
class Oxs_IncompleteInitialization : public Oxs_Exception {
public:
  const char* MessageType() const {
    return "Incomplete or incorrect initialization";
  }
  Oxs_IncompleteInitialization(const String& text)
    : Oxs_Exception(text) {}
  Oxs_IncompleteInitialization(const String& msgtext,
                               const String& msgsubtype,
                               const String& msgsrc,
                               const char* msgfile,int msgline,
                               int sd_count)
    : Oxs_Exception(msgtext,msgsubtype,msgsrc,msgfile,msgline,sd_count) {}
};

// Program logic error
class Oxs_ProgramLogicError : public Oxs_Exception {
public:
  const char* MessageType() const { return "Program logic error"; }
  Oxs_ProgramLogicError(const String& text) : Oxs_Exception(text) {}
  Oxs_ProgramLogicError(const String& msgtext,
                        const String& msgsubtype,
                        const String& msgsrc,
                        const char* msgfile,int msgline,int sd_count)
    : Oxs_Exception(msgtext,msgsubtype,msgsrc,msgfile,msgline,sd_count) {}
};

// No memory.  This exists for those broken C++ compilers (you
// know who you are) that don't throw a bad_alloc from new
class Oxs_NoMem : public Oxs_Exception {
public:
  const char* MessageType() const { return "Insufficient memory"; }
  Oxs_NoMem(const String& text) : Oxs_Exception(text) {}
  Oxs_NoMem(const String& msgtext,
            const String& msgsubtype,
            const String& msgsrc,
            const char* msgfile,int msgline,int sd_count)
    : Oxs_Exception(msgtext,msgsubtype,msgsrc,
                    msgfile,msgline,sd_count) {}
};

// Error inside child thread
class Oxs_BadThread : public Oxs_Exception {
public:
  const char* MessageType() const { return "Error running child thread"; }
  Oxs_BadThread(const String& text) : Oxs_Exception(text) {}
  Oxs_BadThread(const String& msgtext,
                const String& msgsubtype,
                const String& msgsrc,
                const char* msgfile,int msgline,int sd_count)
    : Oxs_Exception(msgtext,msgsubtype,msgsrc,msgfile,msgline,sd_count) {}
};

// Bad Tcl return type
class Oxs_TclBadReturnType : public Oxs_Exception {
public:
  const char* MessageType() const { return "Wrong return type from Tcl script"; }
  Oxs_TclBadReturnType(const String& text) : Oxs_Exception(text) {}
  Oxs_TclBadReturnType(const String& msgtext,
                       const String& msgsubtype,
                       const String& msgsrc,
                       const char* msgfile,int msgline,int sd_count)
    : Oxs_Exception(msgtext,msgsubtype,msgsrc,msgfile,msgline,sd_count) {}
};

// Error reported by Tcl interpreter
class Oxs_TclError : public Oxs_Exception {
private:
  String error_info;
  String error_code;
public:
  const char* ErrorInfo() { return error_info.c_str(); }
  const char* ErrorCode() { return error_code.c_str(); }
  const char* MessageType() const { return "Tcl error"; }
  Oxs_TclError(const String& text,const String& ei,const String& ec)
    : Oxs_Exception(text), error_info(ei), error_code(ec) {}
  Oxs_TclError(const String& msgtext,const String& ei,const String& ec,
               const String& msgsubtype,
               const String& msgsrc,
               const char* msgfile,int msgline,int sd_count)
    : Oxs_Exception(msgtext,msgsubtype,msgsrc,msgfile,msgline,sd_count),
                    error_info(ei), error_code(ec) {}

};

#endif // _OXS_EXCEPT
