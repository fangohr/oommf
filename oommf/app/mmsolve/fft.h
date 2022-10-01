/* FILE: fft.h                    -*-Mode: c++-*-
 *
 *	C++ header file for 1 and 2 dimensional FFT code.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2008/07/31 23:10:06 $
 * Last modified by: $Author: donahue $
 */

#ifndef _FFT_H
#define _FFT_H

#include <cstdio>

#include "oc.h"

#ifdef OLD_CODE
#ifdef __GNUC__
#include <Complex.h>
#else
#include <complex.h>
class double_complex: public complex {
public:
  double_complex(void):complex() {}
  double_complex(complex &c):complex(c) {}
  double_complex(double x,double y):complex(x,y) {}
  double real(void) const { return ::real(*this); }
  double imag(void) const { return ::imag(*this); }
};
typedef double_complex Complex;
#endif // __GNUC__
#endif // OLD_CODE

/* End includes */

typedef double FFT_REAL_TYPE;

#define USE_MYCOMPLEX

#ifdef USE_MYCOMPLEX
typedef FFT_REAL_TYPE MY_COMPLEX_REAL_TYPE;
class MyComplex {
public:
  MY_COMPLEX_REAL_TYPE re,im;
  MyComplex() { re=im=0.0; }
  MyComplex(MY_COMPLEX_REAL_TYPE x,MY_COMPLEX_REAL_TYPE y) { re=x; im=y; }
  friend inline MyComplex operator+(const MyComplex& a,const MyComplex& b);
  friend inline MyComplex operator-(const MyComplex& a,const MyComplex& b);
  friend inline MyComplex operator*(const MyComplex& a,const MyComplex& b);
  friend inline MyComplex operator*(const MyComplex& a,
				    MY_COMPLEX_REAL_TYPE b);
  friend inline MyComplex operator*(MY_COMPLEX_REAL_TYPE b,MyComplex& a);
  MyComplex& operator+=(const MyComplex& b);
  MyComplex& operator*=(const MyComplex& b);
  MyComplex& operator*=(MY_COMPLEX_REAL_TYPE b);
  inline MY_COMPLEX_REAL_TYPE real(void) const { return re; }
  inline MY_COMPLEX_REAL_TYPE imag(void) const { return im; }
};

inline MyComplex operator*(const MyComplex& a,const MyComplex& b)
{ return MyComplex(a.re*b.re-a.im*b.im,a.re*b.im+a.im*b.re); }

inline MyComplex operator+(const MyComplex& a,const MyComplex& b)
{ return MyComplex(a.re+b.re,a.im+b.im); }

inline MyComplex operator-(const MyComplex& a,const MyComplex& b)
{ return MyComplex(a.re-b.re,a.im-b.im); }

inline MyComplex operator*(const MyComplex& a,MY_COMPLEX_REAL_TYPE b)
{ return MyComplex(a.re*b,a.im*b); }

inline MyComplex operator*(MY_COMPLEX_REAL_TYPE b,MyComplex& a)
{ return MyComplex(a.re*b,a.im*b); }

inline MyComplex& MyComplex::operator+=(const MyComplex& b)
{ re+=b.re; im+=b.im; return *this; }

inline MyComplex& MyComplex::operator*=(const MyComplex& b)
{
  MY_COMPLEX_REAL_TYPE temp;
  re=(temp=re)*b.re-im*b.im; im=im*b.re+temp*b.im;
  return *this;
}

inline MyComplex& MyComplex::operator*=(MY_COMPLEX_REAL_TYPE b)
{ re*=b; im*=b; return *this; }

/* typedef MyComplex Complex; */
/* NOTE: <X11/X.h> has a "#define Complex 0" line
 *    (apparently associated with defining polygon
 *    types) that can interfere with the preceding
 *    typedef.  So presently (28-April-97) I've (mjd)
 *    decided to use MyComplex throughout.  If
 *    nothing else, this should make it easier to
 *    modify Complex type usage later on.  See, for
 *    example, the typedef in the !USE_MYCOMPLEX
 *    #ifdef branch below.
 */
#else  /* !USE_MYCOMPLEX */
typedef Complex MyComplex;
#endif /* USE_MYCOMPLEX */

inline MyComplex conj(const MyComplex& a)
{
  return MyComplex(a.real(),-a.imag());
}

class FFT {
private:
  int vecsize;
  enum Direction { forward, inverse };
  MyComplex *Uforward,*Uinverse;
  int *permindex;
  void Permute(MyComplex *vec);
  void BaseDecFreqForward(MyComplex *vec);
  void BaseDecTimeInverse(MyComplex *vec);
  void Setup(int size);
public:
  FFT(void) {
    vecsize=0;
    Uforward=Uinverse=(MyComplex *)NULL;
    permindex=(int *)NULL;
  }
  ~FFT(void);
  void ForwardDecFreq(int size,MyComplex *vec,FFT_REAL_TYPE divisor=0.);
  void InverseDecTime(int size,MyComplex *vec,FFT_REAL_TYPE divisor=0.);
  void ReleaseMemory(void);
};


class FFTReal2D {
private:
  static const double CRRCspeedratio; // Relative speed of ForwardCR
  // as compared to ForwardRC.  If bigger than 1, then ForwardCR is
  // faster.  This will be machine & compiler dependent...oh well.
  int vecsize1,vecsize2; // Dimensions of full-sized real array.
  int logsize1,logsize2; // Base-2 logs of vecsize1 & vecsize2
                         // (which *must* be powers of 2).
  // The corresponding MyComplex array is only half-height, i.e.,
  // is (vecsize1/2)+1 rows by vecsize2 columns.
  FFT fft1,fft2;
  MyComplex* scratch;
  MyComplex* scratchb; // Secondary scratch space; same size as 'scratch'.
  // The main use of scratchb is to slightly block memory accesses
  // having bad strides.
  MyComplex** workarr; // Used only by inverse FFT routines
  void Setup(int size1,int size2);
  void SetupInverse(int size1,int size2);

  // Routine ForwardCR does an FFT first on the columns, then on the
  // rows, conversely, ForwardRC does an FFT first on the rows, and
  // then on the columns.  The subsequent 2 'Inverse FFT' functions are
  // analogous.  It is natural to pair ForwardCR with InverseRC, and
  // ForwardRC with InverseCR (especially if the order has been chosen
  // to get maximum speed-up benefit from zero-padding structure.
  // The routine choice is made automatically (based on speed estimates)
  // by the routines ::Forward & ::Inverse (below in the "public"
  // access block).
  void ForwardCR(int rsize1,int rsize2,
		 const double* const* rarr,
		 int csize1,int csize2,MyComplex** carr);
  void ForwardRC(int rsize1,int rsize2,
		 const double* const* rarr,
		 int csize1,int csize2,MyComplex** carr);
  void InverseRC(int csize1,int csize2,
		 const MyComplex* const* carr,
		 int rsize1,int rsize2,double** rarr);
  void InverseCR(int csize1,int csize2,
		 const MyComplex* const* carr,
		 int rsize1,int rsize2,double** rarr);

  // If either (full) dimension is 1, the 2D FFT degenerates into
  // a 1D FFT.  The above routines assume vecsize1/2 is >=2, and
  // the 1D cases are handled specially:
  void Forward1D(int rsize1,int rsize2,
		 const double* const* rarr,
		 int csize1,int csize2,MyComplex** carr);
  void Inverse1D(int csize1,int csize2,
		 const MyComplex* const* carr,
		 int rsize1,int rsize2,double** rarr);
public:
  FFTReal2D() { vecsize1=vecsize2=0; scratch=NULL; workarr=NULL; }
  ~FFTReal2D() { ReleaseMemory(); }

  // Forward 2D-FFT.
  void Forward(int rsize1,int rsize2,
	       const double* const* rarr,
	       int csize1,int csize2,MyComplex** carr);
  // Computes FFT of rsize1 x rsize2 double** rarr, leaving result in
  // csize1 x csize2 MyComplex** carr.  rsize2 *must* be <= csize2, and
  // (rsize1/2)+1 *must* be <= csize1.  This routine returns only the
  // top half +1 of the transform.  The bottom half is given by
  //      carr[2*(csize1-1)-i][csize2-j]=conj(carr[i][j])
  // for i>=csize1, with the second indices interpreted 'mod csize2'.
  // The client can pass a full-sized top-half-filled MyComplex** carr
  // arr to the FFTReal2D member function "FillOut" to perform
  // this operation...
  //    The dimensions csize2 and 2*(csize1-1) *must* be powers of 2,
  // but rsize1 and rsize2 do not.  On import, the rarr will be
  // implicitly zero-padded as necessary to fit into the specified
  // output array carr.  To get a non-periodic transform, set
  //
  //    2*(csize1-1) to 2*(first power of 2 >= rsize1), and
  //       csize2    to 2*(first power of 2 >= rsize2).
  //
  //  NOTE: The Microsoft Visual C++ compiler apparently doesn't want to
  // automatically convert from (double**) to (const double* const*), so
  // you will need to put in an explicit cast for that compiler (and it
  // does no harm in any case).

  void FillOut(int csize1,int csize2,MyComplex** carr);
  // See previous note.  Here csize1 is the full array height, and the
  // top (csize1/2)+1 rows are assumed already filled.

  // Inverse 2D-FFT.  See notes to '::Forward', including the
  // const* note for MVC++.
  void Inverse(int csize1,int csize2,
	       const MyComplex* const* carr,
	       int rsize1,int rsize2,double** rarr);

  void ReleaseMemory();
};


#ifdef USE_MPI
class FFTReal2D_mpi {
private:
public:
  FFTReal2D_mpi();
  ~FFTReal2D_mpi() { ReleaseMemory(); }

  // Forward 2D-FFT.
  void Forward(int rsize1,int rsize2,
	       const double* const* rarr,
	       int csize1,int csize2,MyComplex** carr);
  // Computes FFT of rsize1 x rsize2 double** rarr, leaving result in
  // csize1 x csize2 MyComplex** carr.  rsize2 *must* be <= csize2, and
  // (rsize1/2)+1 *must* be <= csize1.  This routine returns only the
  // top half +1 of the transform.  The bottom half is given by
  //      carr[2*(csize1-1)-i][csize2-j]=conj(carr[i][j])
  // for i>=csize1, with the second indices interpreted 'mod csize2'.
  //    The dimensions csize2 and 2*(csize1-1) *must* be powers of 2,
  // but rsize1 and rsize2 do not.  On import, the rarr will be
  // implicitly zero-padded as necessary to fit into the specified
  // output array carr.  To get a non-periodic transform, set
  //
  //    2*(csize1-1) to 2*(first power of 2 >= rsize1), and
  //       csize2    to 2*(first power of 2 >= rsize2).

  // Inverse 2D-FFT.  See notes to '::Forward'.
  void Inverse(int csize1,int csize2,
	       const MyComplex* const* carr,
	       int rsize1,int rsize2,double** rarr);

  void ReleaseMemory();
};
#endif /* USE_MPI */

#endif /* _FFT_H */
