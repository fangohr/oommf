/* FILE: oxsexcept.cc                 -*-Mode: c++-*-
 *
 * Exception classes for Oxs.
 *
 */

#include <iostream>
#include <cstdlib>

#include "oxsexcept.h"

/* End includes */


// Note: We put these constructors in a separate file so we
// have a convenient place to put a debugger breakpoint
// from which a backtrace may be obtained.
Oxs_Exception::Oxs_Exception(const String& text)
  : msg(text), line(-1), suggested_display_count(-1)
{
#ifndef NDEBUG
  std::cerr << "\nDEBUG EXCEPTION---\n" <<  text << "\n---" << std::endl;
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
  std::cerr << "\nDEBUG EXCEPTION---\n" <<  text << "\n---" << std::endl;
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

const char* Oxs_Exception::what() const noexcept
{
  String wrkmsg;
  String wrksrc = FullSrc();
  String wrktype = FullType();
  if(wrksrc.size()>0)  wrkmsg += wrksrc  + String("\n");
  if(wrktype.size()>0) wrkmsg += wrktype + String("\n");
  wrkmsg += MessageText();
  workspace = wrkmsg;
  return workspace.c_str();
}
