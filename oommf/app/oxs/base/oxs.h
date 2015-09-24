/* FILE: oxs.h                 -*-Mode: c++-*-
 *
 * Standard macro definitions.
 *
 */

#ifndef OXS_H
#define OXS_H

#include "oc.h"

/* End includes */

/*
 * NOTE: When version number information is changed here, it must
 * also be changed in the following files:
 *
 */
 
#define OXS_MAJOR_VERSION    1
#define OXS_MINOR_VERSION    2
#define OXS_RELEASE_LEVEL    0
#define OXS_RELEASE_SERIAL   4

/* Set this value when making a snapshot release, example value 20061206 */
#define OXS_SNAPSHOT_DATE    20120928

#define OXS_VERSION OC_MAKE_VERSION(OXS)

#endif // OXS_H
