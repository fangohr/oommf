/* FILE: messagelocker.h                    -*-Mode: c++-*-
 *
 * Some error handling routines that should be phased out
 * and replaced with code in the oc extension.
 * 
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2011/11/09 00:50:03 $
 * Last modified by: $Author: donahue $
 */

#ifndef _NB_MESSAGELOCKER
#define _NB_MESSAGELOCKER

#include "oc.h"
#include "dstring.h"

/* End includes */     /* Optional directive to build.tcl */

//////////////////////////////////////////////////////////////////////////
// Class to store error & warning messages in a global Nb_DString
class MessageLocker
{
private:
  static Nb_DString msg;
public:
  static int Reset() { msg.Free(); return 0; }
  static int Append(const OC_CHAR *fmt,...);
  static int GetMessage(Nb_DString& _msg);   // Dup's msg into _msg.
};

#endif // _NB_MESSAGELOCKER
