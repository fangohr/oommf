#include "ext.h"
#include "exterror.h"

////////////////////////////////////////////////////////////////////////
// Oxs_ExtError object.  Don't inline these, because some
// compilers (e.g., egcs 2.91.66 on Linux/AXP) slurp up a
// lot of memory processing the string template operations.
Oxs_ExtError::Oxs_ExtError(const char* errmsg)
  : msg(String("Oxs_Ext ERROR: ") + String(errmsg)) {}
Oxs_ExtError::Oxs_ExtError(const String& errmsg)
  : msg(String("Oxs_Ext ERROR: ") + errmsg) {}
Oxs_ExtError::Oxs_ExtError(const Oxs_Ext* obj,const char* errmsg)
  : msg(String("Oxs_Ext ERROR in object ")
        + String(obj->InstanceName())
        + String(": ") + String(errmsg)) {}
Oxs_ExtError::Oxs_ExtError(const Oxs_Ext* obj,const String& errmsg)
  : msg(String("Oxs_Ext ERROR in object ")
        + String(obj->InstanceName())
        + String(": ") + errmsg) {}
void Oxs_ExtError::Prepend(const String& prefix) { msg = prefix + msg; }
void Oxs_ExtError::Postpend(const String& suffix) { msg += suffix; }

