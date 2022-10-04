/* FILE: maganis.h           -*-Mode: c++-*-
 *
 * This file contains function declarations for mag element initialization
 *
 */

#ifndef _MAGANIS_H
#define _MAGANIS_H

#include <cstdio>
#include "threevec.h"
#include "magelt.h"

/* End includes */

typedef int ANIS_INIT_FUNC(/* Imports: */
			   int argc,const char* const* argv,
			   int Nx,int Nz,MagEltAnisType meat,
			   double K1,ThreeVector& dirA,
			   ThreeVector& dirB,ThreeVector& dirC,
			   /* Exports: */
			   MagElt** m,ThreeVector** &anisA,
			   ThreeVector** &anisB,ThreeVector** &anisC);

typedef ANIS_INIT_FUNC* ANIS_INIT_CHECK_FUNC(const char*);

struct MagAnisInitListNode
{
public:
  ANIS_INIT_CHECK_FUNC* anischeck;
  MagAnisInitListNode* next_node; // Tail element has next_node==NULL
  void AddNode(MagAnisInitListNode* new_node);
  MagAnisInitListNode(ANIS_INIT_CHECK_FUNC* classcheck,
		      MagAnisInitListNode* firstnode);
  // Call with firstnode==NULL if *this is the first node
  ANIS_INIT_FUNC* GetAnisInitFunc(const char*);  // Returns NULL if no match
};

extern MagAnisInitListNode AnisInitList; // First node

#endif /* _MAGANIS_H */
