/* FILE: oxsexcept.cc                 -*-Mode: c++-*-
 *
 * Exception classes for Oxs.
 *
 */

#include <iostream>
#include <stdlib.h>

#include "oxsexcept.h"

/* End includes */


// Note: We put these constructors in a separate file so we
// have a convenient place to put a debugger breakpoint
// from which a backtrace may be obtain.
Oxs_Exception::Oxs_Exception(const String& text)
  : msg(text), line(-1), suggested_display_count(-1)
{
#ifndef NDEBUG
    cerr << "DEBUG EXCEPTION: " <<  text << '\n';
    abort();
#endif
}

Oxs_Exception::Oxs_Exception
(const String& text,
 const String& msgsubtype,
 const String& msgsrc,
 const char* msgfile,
 int msgline,
 int sd_count)
  : msg(text), subtype(msgsubtype),
    src(msgsrc), file(msgfile), line(msgline),
    suggested_display_count(sd_count)
{
#ifndef NDEBUG
    cerr << "DEBUG EXCEPTION: " <<  text << '\n';
    abort();
#endif
}

String Oxs_Exception::FullType() const
{
  String full_type = MessageType();
  if(!subtype.empty()) {
    if(!full_type.empty()) {
      full_type += " : ";
    }
    full_type += subtype;
  }
  return full_type;
}

String Oxs_Exception::FullSrc() const
{
  String full_src = src;
  if(!file.empty()) {
    if(!src.empty()) {
      full_src += "; ";
    }
    full_src += "File: ";
    full_src += file;
    if(line>=0) {
      char numbuf[256];
      Oc_Snprintf(numbuf,sizeof(numbuf),", line %d",line);
      full_src += numbuf;
    }
  }
  return full_src;
}

String Oxs_Exception::DisplayCountAsString() const
{
  char numbuf[256];
  Oc_Snprintf(numbuf,sizeof(numbuf),"%d",DisplayCount());
  return static_cast<String>(numbuf);
}
