/* FILE: planepoint.h                 -*-Mode: c++-*-
 *
 * Little utility class used when rendering 2D curves.
 *
 */

#ifndef PLANEPOINT_H
#define PLANEPOINT_H

/* End includes */     /* Optional directive to pimake */

struct PlanePoint {
public:
  OC_REAL8m x,y;
  PlanePoint() { x=y=0.; }
  PlanePoint(OC_REAL8m xval,OC_REAL8m yval) { x=xval; y=yval; }
  OC_REAL8m Mag(void) { return hypot(x,y); }
  OC_REAL8m MagSq(void) { return x*x+y*y; }
};

inline OC_BOOL operator==(const PlanePoint& lhs,const PlanePoint& rhs)
{ return ((lhs.x==rhs.x) && (lhs.y==rhs.y)); }

#endif // PLANEPOINT_H
