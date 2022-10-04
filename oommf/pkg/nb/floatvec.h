/* FILE: floatvec.h                 -*-Mode: c++-*-
 *
 * 3D vector C++ template declarations, and associated structures,
 * for use with both OC_REAL4 and OC_REAL8 data types.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2015/08/04 21:55:17 $
 * Last modified by: $Author: donahue $
 */

#ifndef _NB_FLOATVEC
#define _NB_FLOATVEC

#include "evoc.h"
#include "functions.h"

/* End includes */     /* Optional directive to build.tcl */

/////////////////// 3D Vector Template Declaration ////////////////////
// NOTE: "T" should a be floating point type

template<typename T> class Nb_Vec3 {
public:
  T x,y,z;  // Components
  void Set(T nx,T ny,T nz) { x=nx; y=ny; z=nz; }
  int Set(const char* tripletstr); // Sets from string of the form
  /// "# # #" where each "#" is an ascii floating point string, e.g.,
  /// "2.7 -3 1.2e8".
  Nb_Vec3(void) { x=y=z=(T)0.0; } // Default constructor
  Nb_Vec3(T nx,T ny,T nz) { Set(nx,ny,nz); }

  // Copy constructors
  template <class T2>
  Nb_Vec3(const Nb_Vec3<T2> &v)
    : x(static_cast<T>(v.x)),
      y(static_cast<T>(v.y)),
      z(static_cast<T>(v.z)) {}

  // Assignment operators
  template <class T2>
  Nb_Vec3<T>& operator=(const Nb_Vec3<T2> &v) {
    x=static_cast<T>(v.x);
    y=static_cast<T>(v.y);
    z=static_cast<T>(v.z);
    return *this;
  }


  OC_BOOL Equals(const Nb_Vec3<T> &v) const {
    return (x==v.x  && y==v.y && z==v.z);
  }
  OC_BOOL Bigger(const Nb_Vec3<T> &v) const {
    return (x>=v.x  && y>=v.y && z>=v.z);
  }
  T MagSq() const { return x*x+y*y+z*z; }
  T Mag() const {
    return Oc_Hypot(x,y,z);
  }
  Nb_Vec3<T> & operator+=(const Nb_Vec3<T>& v);
  Nb_Vec3<T> & operator-=(const Nb_Vec3<T>& v);
  Nb_Vec3<T> & operator*=(T a);
  Nb_Vec3<T> & operator^=(const Nb_Vec3<T>& v);  // Cross product

  // Define the following friend functions here to avoid instantiation
  // problems.
  friend Nb_Vec3<T> operator+(const Nb_Vec3<T>& v,const Nb_Vec3<T>& w) {
    Nb_Vec3<T> temp;
    temp.x=v.x+w.x;  temp.y=v.y+w.y;  temp.z=v.z+w.z;
    return temp;
  }
  friend Nb_Vec3<T> operator-(const Nb_Vec3<T>& v,const Nb_Vec3<T>& w) {
    Nb_Vec3<T> temp;
    temp.x=v.x-w.x;  temp.y=v.y-w.y;  temp.z=v.z-w.z;
    return temp;
  }

  friend Nb_Vec3<T> operator^(const Nb_Vec3<T>& v,const Nb_Vec3<T>& w) {
    // Cross product
    Nb_Vec3<T> temp;
    temp.z = v.x*w.y;
    temp.y = v.x*w.z;
    temp.x = v.y*w.z;
    temp.z -= v.y*w.x;
    temp.y = v.z*w.x - temp.y;
    temp.x -= v.z*w.y;
    return temp;
  }

  friend Nb_Vec3<T> operator*(T a,const Nb_Vec3<T>& v) {
    Nb_Vec3<T> temp;
    temp.x=a*v.x;  temp.y=a*v.y;  temp.z=a*v.z;
    return temp;
  }
  friend Nb_Vec3<T> operator*(const Nb_Vec3<T>& v,T a) {
    Nb_Vec3<T> temp;
    temp.x=a*v.x;  temp.y=a*v.y;  temp.z=a*v.z;
    return temp;
  }
};

template<class T> int Nb_Vec3<T>::Set(const char* tripletstr)
{
  const char* start = tripletstr;
  char* end;
  double tx=Nb_Strtod(start,&end);
  if(start==end) return 1;
  start=end;
  double ty=Nb_Strtod(start,&end);
  if(start==end) return 2;
  start=end;
  double tz=Nb_Strtod(start,&end);
  if(start==end) return 3;
  x=T(tx); y=T(ty); z=T(tz);
  return 0;
}

template<class T> Nb_Vec3<T> & Nb_Vec3<T>::operator+=(const Nb_Vec3<T>& v)
{
  x+=v.x; y+=v.y; z+=v.z; return *this;
}

template<class T> Nb_Vec3<T> & Nb_Vec3<T>::operator-=(const Nb_Vec3<T>& v)
{
  x-=v.x; y-=v.y; z-=v.z; return *this;
}

template<class T> Nb_Vec3<T> & Nb_Vec3<T>::operator^=(const Nb_Vec3<T>& v)
{
  T p12 = x*v.y;  T p13 = x*v.z;  T p23 = y*v.z;
  p12 -= y*v.x;
  y = z*v.x - p13;
  x = p23 - z*v.y;
  z = p12;
  return *this;
}

template<class T> Nb_Vec3<T> & Nb_Vec3<T>::operator*=(T a)
{
  x*=a; y*=a; z*=a; return *this;
}

// Newer templated assignment operator can be used in place of
// Convert function
template <class S,class T>
void Convert(const Nb_Vec3<S> &invec,Nb_Vec3<T> &outvec) {
  outvec.x=static_cast<T>(invec.x);
  outvec.y=static_cast<T>(invec.y);
  outvec.z=static_cast<T>(invec.z);
}

//////////////////////////////////////////////////////////////////////////
// Position _and_ Vector structure
template<class T> struct Nb_LocatedVector {
public:
  Nb_Vec3<T> location;
  Nb_Vec3<T> value;
  Nb_LocatedVector() {
    location.Set((T)0.,(T)0.,(T)0.);
    value.Set((T)0.,(T)0.,(T)0.);
  }
  Nb_LocatedVector(const Nb_Vec3<T> &newloc,const Nb_Vec3<T> &newval) {
    location=newloc; value=newval;
  }
};

//////////////////////////////////////////////////////////////////////////
// Bounding Box
template<class T> class Nb_BoundingBox
{ // Note: Empty boxes are well-defined.  See Empty() function below.
private:
  Nb_Vec3<T> bbmin;
  Nb_Vec3<T> bbmax;
public:
  OC_BOOL IsEmpty() const { 
    return (bbmin.x>bbmax.x || bbmin.y>bbmax.y || bbmin.z>bbmax.z);
  }
  void Reset() {
    bbmin.Set(T(0.),T(0.),T(0.)); bbmax.Set(T(-1.),T(-1.),T(-1.));
  }
  void SortAndSet(const Nb_Vec3<T> &corner1,
		  const Nb_Vec3<T> &corner2);
  void Set(const Nb_Vec3<T> &corner1,const Nb_Vec3<T> &corner2);
  void Set(const Nb_BoundingBox<T> &_other) {
    Set(_other.bbmin,_other.bbmax);
  }
  OC_BOOL ExpandWith(const Nb_Vec3<T> &include_point); // Expands box as
  /// necessary to include 'include_point'.  Returns 1 if expansion occurred,
  /// 0 otherwise.
  OC_BOOL ExpandWith(const Nb_BoundingBox<T> &other_box); // Expands box as
  /// necessary to include 'other_box'.  Returns 1 if expansion occurred,
  /// 0 otherwise
  OC_BOOL IntersectWith(const Nb_BoundingBox<T> &other_box); // Contracts
  /// box as necessary to form the intersection of *this with other_box.
  /// Returns 1 if contraction occurs, 0 if *this is contained in
  /// other_box.
  ///   Note: This use to be a friend function taking 2 BB's as input
  /// and returning a the BB intersection, but gcc 2.8.1 and egcs-1.0.2
  /// don't handle friend templates very well. -mjd, 98/08/13

  // Default constructor
  Nb_BoundingBox(void) { Reset(); }

  // Copy constructors
  template <class T2>
  Nb_BoundingBox(const Nb_BoundingBox<T2>& newbb)
    : bbmin(static_cast<Nb_Vec3<T> >(newbb.bbmin)),
      bbmax(static_cast<Nb_Vec3<T> >(newbb.bbmax)) {}
  Nb_BoundingBox(const Nb_BoundingBox<T>& newbb) = default;

  // Construction from two Nb_Vec3<T>'s.
  template <class T2>
  Nb_BoundingBox(const Nb_Vec3<T2>& v1,const Nb_Vec3<T2>& v2)
    : bbmin(static_cast<Nb_Vec3<T> >(v1)),
      bbmax(static_cast<Nb_Vec3<T> >(v2)) {}

  // Assignment operators
  template <class T2>
  Nb_BoundingBox<T>& operator=(const Nb_BoundingBox<T2>& bb) { 
    Nb_Vec3<T2> bb_minpt,bb_maxpt;
    bb.GetExtremes(bb_minpt,bb_maxpt);
    bbmin = static_cast<Nb_Vec3<T> >(bb_minpt);
    bbmax = static_cast<Nb_Vec3<T> >(bb_maxpt);
    return *this;
  }
  Nb_BoundingBox<T>& operator=(const Nb_BoundingBox<T>& bb) = default;
  /// Note: The MSVC++ 6.0 compiler wants the template<class T2>
  /// version of operator= declared before the base <class T>
  /// version.  Otherwise, the compiler complains about the
  /// member function being already defined.

  OC_BOOL IsIn(const Nb_Vec3<T> &location) const;
  /// Returns 1 if location is inside this box.

  OC_BOOL Contains(const Nb_BoundingBox<T> &other_box) const;
  /// Returns 1 if other_box is entirely inside this box.

  OC_BOOL MoveInto(Nb_Vec3<T> &location) const;
  /// Adjusts location minimally to be inside bbox.
  /// Returns 1 if location was already inside bbox (no shift done).
  /// ASSUMES bbox is non-empty!

  template <class T2>
  void GetMinPt(Nb_Vec3<T2> &minpt) const {
    minpt = static_cast<Nb_Vec3<T2> >(bbmin);
  }
  template <class T2>
  void GetMinPt(T2& x,T2& y,T2& z) const {
    x = static_cast<T2>(bbmin.x);
    y = static_cast<T2>(bbmin.y);
    z = static_cast<T2>(bbmin.z);
  }
  template <class T2>
  void GetMaxPt(Nb_Vec3<T2> &maxpt) const {
    maxpt = static_cast<Nb_Vec3<T2> >(bbmax);
  }
  template <class T2>
  void GetMaxPt(T2& x,T2& y,T2& z) const {
    x = static_cast<T2>(bbmax.x);
    y = static_cast<T2>(bbmax.y);
    z = static_cast<T2>(bbmax.z);
  }

  template <class T2>
  void GetExtremes(Nb_Vec3<T2> &minpt,Nb_Vec3<T2> &maxpt) const {
    minpt = static_cast<Nb_Vec3<T2> >(bbmin);
    maxpt = static_cast<Nb_Vec3<T2> >(bbmax);
  }

  T GetWidth(void)  const { return OC_MAX(bbmax.x-bbmin.x,T(0.)); }
  T GetHeight(void) const { return OC_MAX(bbmax.y-bbmin.y,T(0.)); }
  T GetDepth(void)  const { return OC_MAX(bbmax.z-bbmin.z,T(0.)); }
  T GetVolume(void) const { return GetWidth()*GetHeight()*GetDepth(); }
  Nb_Vec3<T> GetDiag(void) const; // Returns max-min if box is not empty,
  /// 0 otherwise.
};

template<class T> OC_BOOL
Nb_BoundingBox<T>::IsIn(const Nb_Vec3<T> &location) const
{ // Returns 1 if location is inside this box.
  return (location.Bigger(bbmin) && (bbmax.Bigger(location)));
}

template<class T> OC_BOOL
Nb_BoundingBox<T>::Contains(const Nb_BoundingBox<T>& other_box) const
{ // Returns 1 if other_box is entirely inside this box.
  if(other_box.IsEmpty()) return 1;
  if(IsEmpty()) return 0;
  return (IsIn(other_box.bbmin) && IsIn(other_box.bbmax));
}

template<class T> OC_BOOL
Nb_BoundingBox<T>::MoveInto(Nb_Vec3<T> &location) const
{ // Adjusts location minimally to be inside bbox.
  // Returns 1 if location was already inside bbox (no shift done).
  // ASSUMES bbox is non-empty!
  OC_BOOL is_in=1;
  if(location.x<bbmin.x) {
    location.x=bbmin.x; is_in=0;
  } else if(location.x>bbmax.x) {
    location.x=bbmax.x; is_in=0;
  }
  if(location.y<bbmin.y) {
    location.y=bbmin.y; is_in=0;
  } else if(location.y>bbmax.y) {
    location.y=bbmax.y; is_in=0;
  }
  if(location.z<bbmin.z) {
    location.z=bbmin.z; is_in=0;
  } else if(location.z>bbmax.z) {
    location.z=bbmax.z; is_in=0;
  }
  return is_in;
}

template<class T> void
Nb_BoundingBox<T>::SortAndSet(const Nb_Vec3<T> &corner1,
                              const Nb_Vec3<T> &corner2) {
  if(corner1.x<corner2.x) { bbmin.x=corner1.x; bbmax.x=corner2.x; }
  else                    { bbmin.x=corner2.x; bbmax.x=corner1.x; }
  if(corner1.y<corner2.y) { bbmin.y=corner1.y; bbmax.y=corner2.y; }
  else                    { bbmin.y=corner2.y; bbmax.y=corner1.y; }
  if(corner1.z<corner2.z) { bbmin.z=corner1.z; bbmax.z=corner2.z; }
  else                    { bbmin.z=corner2.z; bbmax.z=corner1.z; }
}

template<class T> void
Nb_BoundingBox<T>::Set(const Nb_Vec3<T> &corner1,
                       const Nb_Vec3<T> &corner2) {
  bbmin.x=corner1.x; bbmax.x=corner2.x;
  bbmin.y=corner1.y; bbmax.y=corner2.y;
  bbmin.z=corner1.z; bbmax.z=corner2.z;
}

template<class T> OC_BOOL
Nb_BoundingBox<T>::ExpandWith(const Nb_Vec3<T> &include_point)
{ // Expands box as necessary to include 'include_point'. Returns
  // 1 if expansion occurred, 0 otherwise.
  if(IsEmpty()) {
    bbmin=bbmax=include_point;
    return 1;
  }
  OC_BOOL expand=0;
  if(include_point.x<bbmin.x) { bbmin.x=include_point.x; expand=1; }
  if(include_point.y<bbmin.y) { bbmin.y=include_point.y; expand=1; }
  if(include_point.z<bbmin.z) { bbmin.z=include_point.z; expand=1; }
  if(include_point.x>bbmax.x) { bbmax.x=include_point.x; expand=1; }
  if(include_point.y>bbmax.y) { bbmax.y=include_point.y; expand=1; }
  if(include_point.z>bbmax.z) { bbmax.z=include_point.z; expand=1; }
  return expand;
}

template<class T> OC_BOOL
Nb_BoundingBox<T>::ExpandWith(const Nb_BoundingBox<T>& other_box)
{ // Expands box as necessary to include 'other_box'.  Returns 1 if
  // expansion occurred, 0 otherwise
  if(other_box.IsEmpty()) return 0;
  if(IsEmpty()) {
    Set(other_box);
    return 1;
  }
  OC_BOOL expand=0;
  if(other_box.bbmin.x<bbmin.x) { bbmin.x=other_box.bbmin.x; expand=1; }
  if(other_box.bbmin.y<bbmin.y) { bbmin.y=other_box.bbmin.y; expand=1; }
  if(other_box.bbmin.z<bbmin.z) { bbmin.z=other_box.bbmin.z; expand=1; }
  if(other_box.bbmax.x>bbmax.x) { bbmax.x=other_box.bbmax.x; expand=1; }
  if(other_box.bbmax.y>bbmax.y) { bbmax.y=other_box.bbmax.y; expand=1; }
  if(other_box.bbmax.z>bbmax.z) { bbmax.z=other_box.bbmax.z; expand=1; }
  return expand;
}

template<class T> OC_BOOL
Nb_BoundingBox<T>::IntersectWith(Nb_BoundingBox<T> const & other)
{
  // Intersects *this with other.
  // If the intersection is empty, then the resultant
  // *this box has .bbmin=(0,0,0), .bbmax=(-1,-1,-1)
  if(IsEmpty() || other.IsEmpty()) {
    Reset();
    return 0;
  }
  OC_BOOL change=0;
  if(other.bbmin.x>bbmin.x) { bbmin.x=other.bbmin.x; change=1; }
  if(other.bbmin.y>bbmin.y) { bbmin.y=other.bbmin.y; change=1; }
  if(other.bbmin.z>bbmin.z) { bbmin.z=other.bbmin.z; change=1; }
  if(other.bbmax.x<bbmax.x) { bbmax.x=other.bbmax.x; change=1; }
  if(other.bbmax.y<bbmax.y) { bbmax.y=other.bbmax.y; change=1; }
  if(other.bbmax.z<bbmax.z) { bbmax.z=other.bbmax.z; change=1; }
  if(IsEmpty()) Reset();
  return change;
}

template<class T> Nb_Vec3<T> Nb_BoundingBox<T>::GetDiag(void) const { 
  if(IsEmpty()) return Nb_Vec3<T>(0.,0.,0.);
  return bbmax-bbmin;
}

// Newer templated assignment operator can be used in place of
// Convert function
template <class S,class T>
void Convert(const Nb_BoundingBox<S> &inbox,
             Nb_BoundingBox<T> &outbox) {
  outbox = inbox;
}

#endif // _NB_FLOATVEC
