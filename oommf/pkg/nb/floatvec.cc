/* FILE: floatvec.cc                 -*-Mode: c++-*-
 * At minimum, this file must exist to quiet HP's CC pre-linker.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2010-07-16 22:33:55 $
 * Last modified by: $Author: donahue $
 */
#include "floatvec.h"

void Convert(const Nb_BoundingBox<OC_REAL4> &inbox,
	     Nb_BoundingBox<OC_REAL8> &outbox)
{
  OC_REAL4 xmin,ymin,zmin;
  inbox.GetMinPt(xmin,ymin,zmin);
  Nb_Vec3<OC_REAL8> wmin(static_cast<OC_REAL8>(xmin),
		      static_cast<OC_REAL8>(ymin),
		      static_cast<OC_REAL8>(zmin));
  OC_REAL4 xmax,ymax,zmax;
  inbox.GetMaxPt(xmax,ymax,zmax);
  Nb_Vec3<OC_REAL8> wmax(static_cast<OC_REAL8>(xmax),
		      static_cast<OC_REAL8>(ymax),
		      static_cast<OC_REAL8>(zmax));
  outbox.Set(wmin,wmax);
}

void Convert(const Nb_BoundingBox<OC_REAL8> &inbox,
	     Nb_BoundingBox<OC_REAL4> &outbox)
{
  OC_REAL8 xmin,ymin,zmin;
  inbox.GetMinPt(xmin,ymin,zmin);
  Nb_Vec3<OC_REAL4> nmin(static_cast<OC_REAL4>(xmin),
		      static_cast<OC_REAL4>(ymin),
		      static_cast<OC_REAL4>(zmin));
  OC_REAL8 xmax,ymax,zmax;
  inbox.GetMaxPt(xmax,ymax,zmax);
  Nb_Vec3<OC_REAL4> nmax(static_cast<OC_REAL4>(xmax),
		      static_cast<OC_REAL4>(ymax),
		      static_cast<OC_REAL4>(zmax));
  outbox.Set(nmin,nmax);
}




