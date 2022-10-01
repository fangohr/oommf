/* FILE: oxs.h                 -*-Mode: c++-*-
 *
 * Standard macro definitions.
 *
 */

#ifndef _OXS_H
#define _OXS_H

#include "oc.h"

/* End includes */

/*
 * NOTE: When version number information is changed here, it must
 * also be changed in the following files:
 *    oommf/app/oxs/base/oxs.tcl
 */
 
#define OXS_MAJOR_VERSION    2
#define OXS_MINOR_VERSION    0
#define OXS_RELEASE_LEVEL    "b"
#define OXS_RELEASE_SERIAL   0

/* Set this value when making a snapshot release, example value 20061206 */
/* Also adjust the SnapshotDate method in oommf/pkg/oc/config.tcl        */
#define OXS_SNAPSHOT_DATE    ""

#define OXS_VERSION OC_MAKE_VERSION(OXS)

#endif // _OXS_H
