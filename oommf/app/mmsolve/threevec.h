/* FILE: threevec.h           -*-Mode: c++-*-
 *
 * This file contains class declarations for vector classes.
 *
 */

#ifndef _THREEVEC_H
#define _THREEVEC_H

#include "nb.h"

#include <cstdio>

/* End includes */

/*
 * Notes on vector classes:
 *
 * 0) The class ThreeVector is being phased out.  Where possible, use
 * Vec3D instead.
 *
 * 1) Since the number of vectors may be large, and we may want to allow
 * external processing, the array memory is allocated external to the
 * class and a pointer is sent to each realization.  In particular, the
 * array memory is _not_ owned by the vector, but must be allocated and
 * freed by the calling program.  This is dangerous, since the calling
 * program can delete the memory and leave the Vector instance with an
 * invalid memory pointer.  But at this point in time my judgement is
 * that the benefits outweigh the risks...
 *
 * 2) In view of 1), and as a crude check of initial allocation, the
 * Vector/UnitVector initialization sequences initialize the array
 * memory to 0's (Vector) or (1 0 0 ...) (UnitVector), unless the
 * initialization routine is called without array pointer, in which
 * case arr is set to point to worksp, which is then _not_ initialized.
 * (This helps out in the case where worksp is being used as temporary
 * storage for dynamically allocated (usually temporary) vectors.)
 *
 * 3) One is tempted at first to make UnitVector a class derived from
 * Vector, but the member functions have to all be rewritten anyway,
 * and the overhead associated with virtual function calls probably does
 * not justify their expense.  If it appears in the future that a common
 * interface to Vector and UnitVector is worthwhile, I can always define
 * a virtual class Array and derive both Vector and UnitVector from it.
 *
 * 4) The overhead involved in dynamic allocation of arrays of indeterminate
 * size (needed for simple operators like `+') seems to necessitate fixed
 * length vectors (ThreeVector and ThreeVector) rather than more general
 * beasts.
 *
 * 5) The use of a common workspace array can cause all types of aliasing
 * problems, especially in compound calls to vector operators.  See, for
 * example, the cross product (^) operator.  For this reason the workspace
 * array ThreeVector::worksp has been eliminated, and the use of static
 * arrays inside functions must be _closely_ analyzed.
 *
 */
#define VECDIM 3  // This is silly, but may make modifications easier
class Vec3D; // Forward declaration

class ThreeVector {
protected:
  double*  arr;
  static double zeroarr[VECDIM];
  static ThreeVector zero;
public:
  void     Init(double* array); // Inits array to (0,0,0), so array != NULL
  void     Set(double* array) { arr=array; } // Does not access array.
           ThreeVector(void);
           ThreeVector(double* array);
  ThreeVector& Set(double a,double b,double c); // *this=<a,b,c>
  void     ReleaseArray(void); // Resets arr to point to NULL
  virtual ~ThreeVector(void);
  static const ThreeVector& ZeroVector(void) { return zero; }
  static int Alloc2DArray(int dim1,int dim2,ThreeVector** &v);
  static void Release2DArray(ThreeVector** &v);
  void     Dup(const double* array);

  inline double&  operator[](int n);
  inline const double&  operator[](int n) const;
  inline ThreeVector&  operator=(const ThreeVector& u);
  inline ThreeVector&  operator=(double c);  // *this = <c,c,c>
  operator double*() const { return arr; } // Use this sparingly!!!

  ThreeVector&  operator^=(const ThreeVector& v);
  inline ThreeVector&  operator*=(double a);
  inline ThreeVector&  operator+=(const ThreeVector& v);
  inline ThreeVector&  operator-=(const ThreeVector& v);

  friend inline double operator*(const ThreeVector& u,
				 const ThreeVector& v); // Dot product
  friend double PreciseDot(const ThreeVector& u,  // Extra-precision 
			   const ThreeVector& v); // dot product.
  inline double MagSq(void) const; // Magnitude squared
  double   Mag(void) const;   // Magnitude
  void     Random(double magnitude); // Fills arr with "random" vector of
                                     // given magnitude.
  void     Perturb(double max_mag); // Perturbs with a random vector no bigger
                                    // than max_mag.
  void     Scale(double new_mag);
  void     PreciseScale(double new_mag); // Slower but more accurate
  inline void FastAdd(double a,const ThreeVector& v,double b,
		      const ThreeVector& w);
                                                  // *this = a.v+b.w 
  inline void FastAdd(const ThreeVector& v,double b,const ThreeVector& w);
                                                  // *this = v+b.w 
  inline void Accum(double a,const ThreeVector& v);
  inline void Accum(double a,const Vec3D& v);
  void     SignAccum(ThreeVector& accumpos,ThreeVector& accumneg);
                   // Accumulate positive components of *this into
                   // accumpos, negative components into accumneg.
  void     Print(FILE* fout) const;

  // The following functions are pretty broken.  Avoid using them,
  // and especially don't use in compound statements (you're likely
  // to get aliasing errors on the returned reference).
  ThreeVector&  operator*(double a);
  ThreeVector&  operator^(const ThreeVector& v); // Cross product
  ThreeVector&  operator+(const ThreeVector& v);
  ThreeVector&  operator-(const ThreeVector& v);
};

inline double& ThreeVector::operator[](int n)
{
#ifdef CODE_CHECKS
  if(n<0 || n>=VECDIM)
    PlainError(1,"Error in ThreeVector::operator[]: %s",ErrBadParam);
#endif // CODE_CHECKS
  return arr[n];
}

inline const double& ThreeVector::operator[](int n) const
{
#ifdef CODE_CHECKS
  if(n<0 || n>=VECDIM)
    PlainError(1,"Error in ThreeVector::operator[] const: %s",ErrBadParam);
#endif // CODE_CHECKS
  return arr[n];
}

inline double operator*(const ThreeVector& u,const ThreeVector& v)
{ // Dot product
  int i=0; double dot=0.0,*uptr=u.arr,*vptr=v.arr;
  while( i++ < VECDIM ) dot+=(*(uptr++))*(*(vptr++));
  return dot;
}

inline ThreeVector& ThreeVector::operator=(const ThreeVector& v)
{
  for(int i=0;i<VECDIM;i++) arr[i]=v.arr[i];
  return *this;
}

inline ThreeVector& ThreeVector::operator=(double c)
{ // *this = <c,c,c>
  for(int i=0;i<VECDIM;i++) arr[i]=c;
  return *this;
}

inline double ThreeVector::MagSq(void) const
{
  return OC_SQ(arr[0])+OC_SQ(arr[1])+OC_SQ(arr[2]);
}

inline ThreeVector& ThreeVector::operator*=(double a)
{
  for(int i=0;i<VECDIM;i++) arr[i]*=a;
  return *this;
}

inline ThreeVector& ThreeVector::operator+=(const ThreeVector& v)
{
  for(int i=0;i<VECDIM;i++) arr[i]+=v.arr[i];
  return *this;
}

inline ThreeVector& ThreeVector::operator-=(const ThreeVector& v)
{
  for(int i=0;i<VECDIM;i++) arr[i]-=v.arr[i];
  return *this;
}

inline void ThreeVector::FastAdd(double a,const ThreeVector& v,
				 double b,const ThreeVector& w)
{ // *this = a.v+b.w
  int i;
  for(i=0;i<VECDIM;i++) arr[i]=a*v.arr[i]+b*w.arr[i];
}

inline void ThreeVector::FastAdd(const ThreeVector& v,double b,
				 const ThreeVector& w)
{ // *this = v+b.w
  int i;
  for(i=0;i<VECDIM;i++) arr[i]=v.arr[i]+b*w.arr[i];
}

inline void ThreeVector::Accum(double a,const ThreeVector& v)
{
  int i;
  for(i=0;i<VECDIM;i++) arr[i]+=a*v.arr[i];
}

/////////////////// Replacement Vec3D Declaration ////////////////////
class Vec3D {
protected:
  double x,y,z;
public:
  Vec3D() { x=y=z=0.; }
  Vec3D(double _x,double _y,double _z) { x=_x; y=_y; z=_z; }
  Vec3D(double *arr) { x=arr[0]; y=arr[1]; z=arr[2]; }
  Vec3D(const ThreeVector &v) { x=v[0]; y=v[1]; z=v[2]; }
  Vec3D& operator=(const ThreeVector &v) {
    x=v[0]; y=v[1]; z=v[2]; return *this;
  }
  inline double GetX() const { return x; }
  inline double GetY() const { return y; }
  inline double GetZ() const { return z; }
  inline void FastAdd(double a,const ThreeVector& v,double b,
		      const ThreeVector& w);      // *this = a.v+b.w 
  inline void FastAdd(const ThreeVector& v,double b,const ThreeVector& w);
                                                  // *this = v+b.w 
  inline void FastAdd(double a,const Vec3D& v,double b,
		      const Vec3D& w);      // *this = a.v+b.w 
  inline void FastAdd(const Vec3D& v,double b,const Vec3D& w);
                                            // *this = v+b.w 
  inline void Accum(double a,const Vec3D& v);
  inline void Accum(double a,const ThreeVector& v);
  inline Vec3D& operator^=(const Vec3D& v);
  inline Vec3D& operator*=(double a) { x*=a; y*=a; z*=a; return *this; }
  inline Vec3D& operator+=(const Vec3D& v) { 
    x+=v.x; y+=v.y; z+=v.z; return *this;
  }
  inline Vec3D& operator+=(const ThreeVector& v) { 
    x+=v[0]; y+=v[1]; z+=v[2]; return *this;
  }
  inline Vec3D& operator-=(const Vec3D& v) {
    x-=v.x; y-=v.y; z-=v.z; return *this;
  }
  inline double MagSq(void) const { return x*x+y*y+z*z; }
  // Dot product
  friend inline double operator*(const Vec3D& u,const Vec3D& v);
  // src -> dest
  friend inline void Convert(ThreeVector& dest,const Vec3D& src);
  friend inline void Convert(Vec3D& dest,const ThreeVector& src);
  void Normalize(); // Adjusts size to 1
  void Random();    // Makes a random unit vector

  // Coordinate transformations.  The 2D grid/solver uses x+z in-plane
  // (width and height), with y pointing into the screen (thickness).
  //  For reference this is called the "xzy" coordinate system.  All
  // outside OOMMF code (including the MIF class) uses x+y in-plane
  // (width and height), with z pointing out of the screen (thickness).
  // This is referred to as the "xyz" coordinate system.  This functions
  // should not be used outside of the Grid2D class.
  void ConvertXyzToXzy() { double temp= y;  y= -z;  z=  temp; }
  void ConvertXzyToXyz() { double temp= y;  y=  z;  z= -temp; }
};

#if VECDIM!=3
#error All lot of this code _assumes_ VECDIM == 3
#endif

inline void Vec3D::FastAdd(double a,const ThreeVector& v,
		    double b,const ThreeVector& w)
{ // *this = a.v+b.w
  x=a*v[0]+b*w[0];  y=a*v[1]+b*w[1];  z=a*v[2]+b*w[2];
}

inline void Vec3D::FastAdd(const ThreeVector& v,
		    double b,const ThreeVector& w)
{ // *this = v+b.w
  x=v[0]+b*w[0];  y=v[1]+b*w[1];  z=v[2]+b*w[2];
}

inline void Vec3D::FastAdd(double a,const Vec3D& v,
		    double b,const Vec3D& w)
{ // *this = a.v+b.w
  x=a*v.x+b*w.x;  y=a*v.y+b*w.y;  z=a*v.z+b*w.z;
}

inline void Vec3D::FastAdd(const Vec3D& v,
		    double b,const Vec3D& w)
{ // *this = v+b.w
  x=v.x+b*w.x;  y=v.y+b*w.y;  z=v.z+b*w.z;
}

inline void Vec3D::Accum(double a,const Vec3D& v)
{ // *this += a.v
  x+=a*v.x; y+=a*v.y; z+=a*v.z;
}

inline void Vec3D::Accum(double a,const ThreeVector& v)
{ // *this += a.v
  x+=a*v[0]; y+=a*v[1]; z+=a*v[2];
}

inline Vec3D& Vec3D::operator^=(const Vec3D& v) // Cross product
{
  double tx,ty,tz;

  tx=x; ty=y; tz=z;  // tz not really needed
  x=ty*v.z-tz*v.y;
  y=tz*v.x-tx*v.z;
  z=tx*v.y-ty*v.x;

  return *this;
}

// Dot product
inline double operator*(const Vec3D& u,const Vec3D& v)
{ return u.x*v.x+u.y*v.y+u.z*v.z; }

inline double operator*(const Vec3D& u,const ThreeVector& v)
{
  return u.GetX()*v[0]+u.GetY()*v[1]+u.GetZ()*v[2];
}

// Definition for ThreeVector that needs Vec3D declaration
inline void ThreeVector::Accum(double a,const Vec3D& v)
{
  arr[0]+=a*v.GetX();
  arr[1]+=a*v.GetY();
  arr[2]+=a*v.GetZ();
}

//////////////// Conversion routines /////////////////////////////
inline void Convert(ThreeVector& dest,const Vec3D& src)
{ // src -> dest
  dest[0]=src.x; dest[1]=src.y; dest[2]=src.z;
}

inline void Convert(Vec3D& dest,const ThreeVector& src)
{ // src -> dest
  dest.x=src[0]; dest.y=src[1]; dest.z=src[2];
}


// Explicit, separate declaration to help HP CC auto-instantiation.
template<class T> void Convert(ThreeVector& dest,const Nb_Vec3<T>& src);
template<class T> void Convert(Nb_Vec3<T>& dest,const ThreeVector& src);

template<class T> void Convert(ThreeVector& dest,
				      const Nb_Vec3<T>& src)
{ // src -> dest
  dest[0]=src.x; dest[1]=src.y; dest[2]=src.z;
}

template<class T> void Convert(Nb_Vec3<T>& dest,
				      const ThreeVector& src)
{ // src -> dest
  dest.x=src[0]; dest.y=src[1]; dest.z=src[2];
}

#endif // _THREEVEC_H
