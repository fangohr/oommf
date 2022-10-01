/* FILE: version.h                    -*-Mode: c++-*-
 *
 *	Version macros for the OOMMF Vector Field extension.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2015/03/25 16:45:21 $
 * Last modified by: $Author: dgp $
 */

#ifndef _XP_VERSION
#define _XP_VERSION

#include "oc.h"

/* End includes */

/*
 * NOTE: When version number information is changed here, it must
 * also be changed in the following files:
 */

#define XP_MAJOR_VERSION	2
#define XP_MINOR_VERSION	0
#define XP_RELEASE_LEVEL	"b"
#define XP_RELEASE_SERIAL	0

#define XP_VERSION OC_MAKE_VERSION(XP)

#endif /* _XP_VERSION */
