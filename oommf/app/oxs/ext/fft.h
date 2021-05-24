/* FILE: fft.h                    -*-Mode: c++-*-
 *
 *	C++ header file for 1 and 3 dimensional FFT code.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2014/10/29 23:52:16 $
 * Last modified by: $Author: donahue $
 */

#ifndef _OXS_FFT_H
#define _OXS_FFT_H

#include "oc.h" // OC_REALWIDE defn

/* End includes */

typedef OC_REAL8m OXS_FFT_REAL_TYPE;
typedef OC_INDEX  OXS_FFT_INT_TYPE;
#define OXS_FFT_INT_MOD OC_INDEX_MOD

#define USE_OXSCOMPLEX

#ifdef USE_OXSCOMPLEX
typedef OXS_FFT_REAL_TYPE OXS_COMPLEX_REAL_TYPE;
class Oxs_Complex {
public:
  OXS_COMPLEX_REAL_TYPE re,im;
  Oxs_Complex() : re(0), im(0) {}
  Oxs_Complex(OXS_COMPLEX_REAL_TYPE x,OXS_COMPLEX_REAL_TYPE y): re(x), im(y) {}
  void Set(OXS_COMPLEX_REAL_TYPE x,OXS_COMPLEX_REAL_TYPE y) { re=x; im=y; }
  friend inline Oxs_Complex operator+(const Oxs_Complex& a,
					const Oxs_Complex& b);
  friend inline Oxs_Complex operator-(const Oxs_Complex& a,
					const Oxs_Complex& b);
  friend inline Oxs_Complex operator*(const Oxs_Complex& a,
					const Oxs_Complex& b);
  friend inline Oxs_Complex operator*(const Oxs_Complex& a,
				    OXS_COMPLEX_REAL_TYPE b);
  friend inline Oxs_Complex operator*(OXS_COMPLEX_REAL_TYPE b,
					Oxs_Complex& a);
  Oxs_Complex& operator+=(const Oxs_Complex& b);
  Oxs_Complex& operator-=(const Oxs_Complex& b);
  Oxs_Complex& operator*=(const Oxs_Complex& b);
  Oxs_Complex& operator*=(OXS_COMPLEX_REAL_TYPE b);
  inline OXS_COMPLEX_REAL_TYPE real() const { return re; }
  inline OXS_COMPLEX_REAL_TYPE imag() const { return im; }
};

inline Oxs_Complex operator*(const Oxs_Complex& a,const Oxs_Complex& b)
{ return Oxs_Complex(a.re*b.re-a.im*b.im,a.re*b.im+a.im*b.re); }

inline Oxs_Complex operator+(const Oxs_Complex& a,const Oxs_Complex& b)
{ return Oxs_Complex(a.re+b.re,a.im+b.im); }

inline Oxs_Complex operator-(const Oxs_Complex& a,const Oxs_Complex& b)
{ return Oxs_Complex(a.re-b.re,a.im-b.im); }

inline Oxs_Complex operator*(const Oxs_Complex& a,OXS_COMPLEX_REAL_TYPE b)
{ return Oxs_Complex(a.re*b,a.im*b); }

inline Oxs_Complex operator*(OXS_COMPLEX_REAL_TYPE b,Oxs_Complex& a)
{ return Oxs_Complex(a.re*b,a.im*b); }

inline Oxs_Complex& Oxs_Complex::operator+=(const Oxs_Complex& b)
{ re+=b.re; im+=b.im; return *this; }

inline Oxs_Complex& Oxs_Complex::operator-=(const Oxs_Complex& b)
{ re-=b.re; im-=b.im; return *this; }

inline Oxs_Complex& Oxs_Complex::operator*=(const Oxs_Complex& b)
{
  OXS_COMPLEX_REAL_TYPE temp;
  re=(temp=re)*b.re-im*b.im; im=im*b.re+temp*b.im;
  return *this;
}

inline Oxs_Complex& Oxs_Complex::operator*=(OXS_COMPLEX_REAL_TYPE b)
{ re*=b; im*=b; return *this; }

/* typedef Oxs_Complex Complex; */
/* NOTE: <X11/X.h> has a "#define Complex 0" line
 *    (apparently associated with defining polygon
 *    types) that can interfere with the preceding
 *    typedef.  So presently (28-April-97) I've (mjd)
 *    decided to use Oxs_Complex throughout.  If
 *    nothing else, this should make it easier to
 *    modify Complex type usage later on.  See, for
 *    example, the typedef in the !USE_OXSCOMPLEX
 *    #ifdef branch below.
 */
#else  /* !USE_OXSCOMPLEX */
typedef Complex Oxs_Complex;
#endif /* USE_OXSCOMPLEX */

inline Oxs_Complex conj(const Oxs_Complex& a)
{
  return Oxs_Complex(a.real(),-a.imag());
}

inline OXS_FFT_REAL_TYPE Oxs_Conjugate(OXS_FFT_REAL_TYPE x) { return x; }
inline Oxs_Complex Oxs_Conjugate(Oxs_Complex x) { return conj(x); }

class Oxs_FFT {
  // NOTE: All member functions except for constructor and
  // destructor are "conceptually const"
private:
  static OC_BOOL PackCheck();  // Checks Oxs_Complex packing restriction.
  mutable OXS_FFT_INT_TYPE vecsize;
  mutable OXS_FFT_REAL_TYPE vecsize_normalization; // ==1/vecsize.
  mutable OXS_FFT_INT_TYPE log2vecsize; // 2^log2vecsize == vecsize
  enum Direction { forward, inverse };
  mutable Oxs_Complex *Uforward;
  mutable Oxs_Complex *Uinverse;
  // Note: Digital Mars compiler interprets
  //     mutable int *a,*b;
  //  as
  //     mutable int *a;  int *b;
  // i.e., in the case of multiple variables being declared, the
  // "mutable" keyword is applied to only the first element.  I
  // don't know what the relevant C++ specs say (or don't say)
  // on this issue, but doing multiple declares like this is
  // arguably bad practice, so just break them up.

  mutable OXS_FFT_INT_TYPE *permindex;

  void Permute(Oxs_Complex *vec) const;

  void BaseDecFreqForward(Oxs_Complex *vec) const;
  void BaseDecTimeInverse(Oxs_Complex *vec) const;

  void FillRootsOfUnity(OXS_FFT_INT_TYPE size,
			Oxs_Complex* forward_roots,
			Oxs_Complex* inverse_roots) const;
  void Setup(OXS_FFT_INT_TYPE size) const;


  // The "realdata" variables are used by the real FFT code.
  // When initialized, the wforward (winverse) array holds
  // the first (last) quarter of the roots of unity of order
  // 2*vecsize.  Use realdata_vecsize to tell if the wforward
  // and winverse arrays are initialized: if realdata_vecsize
  // == 2*vecsize, then they are.  Otherwise call
  // InitializeRealDataTransforms() to perform the
  // initialization.
  //   NOTE 1: The length of the wforward and winverse arrays
  // is realdata_vecsize/4, *NOT* realdata_vecsize.
  //   NOTE 2: For reasons of computational efficiency,
  // realdata_wforward actually holds 0.5*(roots of unity);
  // but realdata_winverse holds the unscaled roots.
  mutable OXS_FFT_INT_TYPE realdata_vecsize;
  mutable Oxs_Complex *realdata_wforward;
  mutable Oxs_Complex *realdata_winverse;
  // See note following Uforward and Uinverse declarations
  // concerning declaring multiple "mutable" variables with
  // a single statement.  (Executive summary: don't do it.)
  void InitializeRealDataTransforms(OXS_FFT_INT_TYPE size) const;
  /// Import size is the desired value for realdata_vecsize.  This
  /// routine is conceptually const.
public:
  Oxs_FFT();
  ~Oxs_FFT();
  void ReleaseMemory() const; // Conceptually const!?

  void ForwardDecFreqScrambledNoScale(OXS_FFT_INT_TYPE size,
                                      Oxs_Complex *vec) const {
    if(size!=vecsize) Setup(size);
    BaseDecFreqForward(vec);
  }
  void ForwardDecFreqNoScale(OXS_FFT_INT_TYPE size,Oxs_Complex *vec) const {
    if(size==2) {
      OXS_FFT_REAL_TYPE x0=vec[0].re;
      OXS_FFT_REAL_TYPE y0=vec[0].im;
      OXS_FFT_REAL_TYPE x1=vec[1].re;
      OXS_FFT_REAL_TYPE y1=vec[1].im;
      vec[0].Set(x0+x1,y0+y1);
      vec[1].Set(x0-x1,y0-y1);
    } else {
      ForwardDecFreqScrambledNoScale(size,vec);
      Permute(vec);
    }
  }
  void ForwardDecFreq(OXS_FFT_INT_TYPE size,Oxs_Complex *vec) const {
    // Default scaling, which is 1.0
    ForwardDecFreqNoScale(size,vec);
  }

  void ForwardDecFreqRealDataNoScale(OXS_FFT_INT_TYPE size,void *vec) const;
  /// Performs FFT on a real sequence.  size must be a positive power of
  /// two.  The calculation is done in-place, where on return the array
  /// should be interpreted as a complex array with real and imaginary
  /// parts interleaved, except that ((Oxs_Complex*)vec)[0].re is the
  /// 0-th element in the transform space (which is real because the
  /// input is real), and ((Oxs_Complex*)vec)[0].im is the size/2-th
  /// element in the transform space (which is also real for the same
  /// reason).
  ///   The physical length on output is size/2 complex elements, but
  /// this is sufficient to define the whole transform of length size
  /// because in the transform space the array is conjugate symmentric
  /// (since the input is real); this means that virtual element
  ///
  ///   ((Oxs_Complex*)vec)[k] = conj(((Oxs_Complex*)vec)[size-k])
  ///
  /// for size/2<k<size.
  ///   NB: This code *ASSUMES* that the Oxs_Complex type packs tightly
  /// as two OXS_FFT_REAL_TYPE elements.

  void ForwardDecFreqRealData(OXS_FFT_INT_TYPE size,void *vec) const {
    // Default scaling, which is 1.0
    ForwardDecFreqRealDataNoScale(size,vec);
  }

  void InverseDecTimeScrambledNoScale(OXS_FFT_INT_TYPE size,
                                      Oxs_Complex *vec) const {
    if(size!=vecsize) Setup(size);
    BaseDecTimeInverse(vec);
  }
  void InverseDecTimeNoScale(OXS_FFT_INT_TYPE size,Oxs_Complex *vec) const {
    if(size==2) {
      OXS_FFT_REAL_TYPE x0=vec[0].re;
      OXS_FFT_REAL_TYPE y0=vec[0].im;
      OXS_FFT_REAL_TYPE x1=vec[1].re;
      OXS_FFT_REAL_TYPE y1=vec[1].im;
      vec[0].Set(x0+x1,y0+y1);
      vec[1].Set(x0-x1,y0-y1);
    } else {
      if(size!=vecsize) Setup(size);
      Permute(vec);
      BaseDecTimeInverse(vec);
    }
  }
  void InverseDecTime(OXS_FFT_INT_TYPE size,Oxs_Complex *vec) const;
  /// Default scaling, which is 1./size.

  void InverseDecTimeRealDataNoScale(OXS_FFT_INT_TYPE size,void *vec) const;
  void InverseDecTimeRealData(OXS_FFT_INT_TYPE size,void *vec) const;
  /// Default scaling, which is 2./size.


  void UnpackRealPairImage(OXS_FFT_INT_TYPE size, Oxs_Complex *cvec) const;
  /// Given complex sequence cvec[] which is the FFT image of
  /// f[]+i.g[], where f[] and g[] are real sequences, unpacks
  /// in-place to yield half of each of the conjugate symmetric
  /// sequences F[] and G[], which are the FFT image of f[] and
  /// g[], respectively.  The layout is:
  ///
  ///      F[0] = cvec[0].re, F[1] = cvec[1], F[2] = cvec[2],
  ///                          ... F[size/2] = cvec[size/2].re
  /// and
  ///      G[0] = cvec[0].im, G[size-1] = cvec[size-1],
  ///                         G[size-2] = cvec[size-2],
  ///          ...            G[(size/2)+1] = cvec[(size/2)+1]
  ///                         G[size/2] = cvec[size/2].im
  ///
  /// Note that F[0], F[size/2], G[0] and G[size/2] are all real.
  /// Also, because of cong. symmetry, for 0<k<size/2 F[size-k]
  /// can be computed as F[size-k] = conj(F[k]), and similarly
  /// G[k] = conj(G[size-k]).

  void RepackRealPairImage(OXS_FFT_INT_TYPE size, Oxs_Complex *cvec) const;
  /// Reverses the operation of
  /// UnpackRealPairImage(OXS_FFT_INT_TYPE,Oxs_Complex).

};


// Class for in-place 3D FFT's
class Oxs_FFT3D {
  // NOTE: All member functions except for constructor and
  // destructor are "conceptually const"

  // Blocking routines and data.  These provide for automated
  // filling and flushing (i.e., pasting) of data from a base
  // memory store into the scratch buffer area.  Conceptually,
  // the base area is viewed as a 2D array with hard direction
  // stride size of base_stride, easy direction stride of 1.
  // This is mirrored into the scratch area with a reversal
  // of the easy and hard stride directions.  In the scratch
  // area the hard direction stride size is scratch_stride,
  // with again an easy directin stride of 1.
  mutable OC_INDEX scratchsize; // Size of scratch area (as an
  /// Oxs_Complex array).
  mutable Oxs_Complex* scratch;  // Scratch buffer area
  mutable OC_INDEX scratch_stride;
  mutable OC_INDEX scratch_index; // Working row in scratch area
  mutable OC_INDEX scratch_top_index; // Number of currently buffered rows
  mutable Oxs_Complex* base_ptr; // Start of base area
  mutable OC_INDEX base_stride;
  mutable OC_INDEX base_block_index; // Buffered region start
  mutable OC_INDEX base_top_index; // Number of rows in base area
  mutable OC_INDEX buffer_input_size;  // Input row length
  mutable OC_INDEX buffer_output_size; // Output row length
  mutable OC_INDEX buffer_full_size;   // Full row size in buffer
  Oxs_Complex* SetupBlocking(Oxs_Complex* base,
			     OC_INDEX insize,OC_INDEX outsize,
			     OC_INDEX fullsize,OC_INDEX basestride,
			     OC_INDEX baserows) const;
  Oxs_Complex* FillBlockBuffer(OC_INDEX blockoffset) const;
  void FlushBlockBuffer() const;
  Oxs_Complex* GetNextBlockRun() const;

  Oxs_FFT xfft,yfft,zfft;
  void ReleaseMemory() const;
  void AdjustScratchSize(OC_INDEX xsize,OC_INDEX ysize,OC_INDEX zsize) const;

public:
  Oxs_FFT3D();
  ~Oxs_FFT3D();
  // This code assumes arr is in fortran order, i.e.,
  // increment first x, then y, and last z.
  void Forward(OC_INDEX xsize,OC_INDEX ysize,OC_INDEX zsize,
	       Oxs_Complex* arr) const;
  void Inverse(OC_INDEX xsize,OC_INDEX ysize,OC_INDEX zsize,
	       Oxs_Complex* arr) const;

  // Routines for real space-domain data.
  // Packing issues in frequency-domain:  Since the data are
  //  real in the space-domain, in the frequency-domain the
  //  data will be conjugate symmetric, i.e., the relation
  //     F(i,j,k) = conjugate(F(N1-i,N2-j,N3-k))
  //  holds.  Therefore, only half the frequency-domain data
  //  need be stored.  The "RealData" routines do this primarily
  //  by storing index i up to only N1/2.  For i>N1/2, use the
  //  aforementioned conjugate relation.  However, i==0 and i==N1/2
  //  are special cases, which are packed together in the i==0
  //  plane.  This packing is difficult to describe; for details
  //  refer either directly to the FFT code, or else the utility
  //  routine Oxs_FFT3D::RetrievePackedIndex.  See also mjd's
  //  NOTES II, 27-Apr-2001, p 110-112.
  void ForwardRealDataScrambledNoScale(void* arr,
             OC_INDEX rsize1_in,OC_INDEX rsize2_in,OC_INDEX rsize3_in,
             OC_INDEX csize1_out,OC_INDEX csize2_out,OC_INDEX csize3_out,
             OC_INDEX cstride2,OC_INDEX cstride3) const;

  void ForwardRealDataNoScale(void* arr,
             OC_INDEX rsize1_in,OC_INDEX rsize2_in,OC_INDEX rsize3_in,
             OC_INDEX csize1_out,OC_INDEX csize2_out,OC_INDEX csize3_out,
             OC_INDEX cstride2,OC_INDEX cstride3) const;

  void InverseRealDataScrambledNoScale(void* arr,
             OC_INDEX rsize1_out,OC_INDEX rsize2_out,OC_INDEX rsize3_out,
             OC_INDEX csize1_in,OC_INDEX csize2_in,OC_INDEX csize3_in,
             OC_INDEX cstride2,OC_INDEX cstride3) const;

  void InverseRealDataNoScale(void* arr,
             OC_INDEX rsize1_out,OC_INDEX rsize2_out,OC_INDEX rsize3_out,
             OC_INDEX csize1_in,OC_INDEX csize2_in,OC_INDEX csize3_in,
             OC_INDEX cstride2,OC_INDEX cstride3) const;

  static Oxs_Complex RetrievePackedIndex(Oxs_Complex* carr,
  	     OC_INDEX csize1,OC_INDEX csize2,OC_INDEX csize3,
             OC_INDEX cstride2,OC_INDEX cstride3,
	     OC_INDEX index1,OC_INDEX index2,OC_INDEX index3);

};

#endif /* _OXS_FFT_H */
