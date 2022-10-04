/* FILE: maginit.h           -*-Mode: c++-*-
 *
 * This file contains function declarations for mag element initialization
 *
 */

#ifndef _MAGINIT_H
#define _MAGINIT_H

#include <cstdio>
#include "magelt.h"

/* End includes */

// Prototype for magnetization initialization functions---
typedef int MI_Function(int width,int height,MagElt** arr,
			char** params);

struct MI_FuncInfo
{
 public:
  MI_Function* func;  // Pointer to actual function
  const char*  name;  // Name of function (null terminated ASCII string)
  int          paramcount; // Number of parameters
};

class MagInit
{
 private:
  static MI_FuncInfo Ident[];
  static int IdentCount;
 public:
  MagInit(void);
  static MI_Function* LookUp(char *index); // Given name, return func. ptr.
  static const char* LookUp(MI_Function* func);  // Given func., return name
  static const char* NameCheck(char *index);     // Check that name is valid
  static int GetParamCount(char *index);   // Param. length for func. "index"
  static void List(FILE* output);          // List all known MI_Functions
};

#endif /* _MAGINIT_H */
