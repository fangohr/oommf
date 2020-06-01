/* File: fft3v.h
 *
 * Classes for real-to-complex and complex-to-real FFT of three
 * dimensional arrays of three vectors.
 *
 * These classes are:
 *
 *   Oxs_FFT1DThreeVector: FFTs on 1D arrays of three vectors,
 *                         real <-> complex.  Not in-place.
 *
 *         Oxs_FFTStrided: In-place FFTs on complex multi-dimensional
 *                         arrays, with stride>1.
 *
 *   Oxs_FFT3DThreeVector: FFTs on 3D arrays of three vectors,
 *                         real<->complex.  For forward transforms,
 *                         Oxs_FFT1DThreeVector is called to perform
 *                         all the stride=1 (i.e., "x") axis FFTs,
 *                         and Oxs_FFTStrided is used to do the FFTs
 *                         along the other two axes.
 *
 */

#ifndef _OXS_FFT3V_H
#define _OXS_FFT3V_H

#ifndef STANDALONE
#include "oc.h" // OC_REAL8m, OC_INT4m, OC_UINT4m, OC_INDEX declarations
#endif // STANDALONE

/* End includes */     /* Optional directive to pimake */

typedef OC_REAL8m OXS_FFT_REAL_TYPE;
/// Note: Some code in demag-threaded.cc (others?) loads the spin array
/// directly into FFT code.  Spins are arrays of ThreeVectors, which are
/// of type OC_REAL8m, so we need to make OXS_FFT_REAL_TYPE of OC_REAL8m
/// type too.  If you want the FFTs to have the width of type
/// OC_REALWIDE, then it probably makes the most sense all around to do
/// it by declaring OC_REAL8m to be the same width as OC_REALWIDE
/// (presumably "long double"), rather than trying to keep spins and
/// whatnot OC_REAL8m == double and defining OXS_FFT_REAL_TYPE ==
/// OC_REALWIDE == long double because once the FFTs are long double it
/// presumably doesn't proportionally cost that much extra to make all
/// the OC_REAL8m structures long double.

////////////////////////////////////////////////////////////////////////
// Class Oxs_FFT1DThreeVector //////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

class Oxs_FFT1DThreeVector {
public:
  Oxs_FFT1DThreeVector();
  ~Oxs_FFT1DThreeVector();

  // Copy operators.  These are required by STL vector template.
  Oxs_FFT1DThreeVector(const Oxs_FFT1DThreeVector& other):
    ForwardPtr(0),InversePtr(0),         // Dummy initial values
    arrcount(0), rsize(0), rstride(0), fftsize(0),
    log2fftsize(-1),UReals(0),UForwardRadix4(0),
    ptsRadix4(0),bitreverse(0),scratch_size(0),scratch(0),
    workbuffer_size(0),workbuffer(0)
    {
      Dup(other);
    }
  Oxs_FFT1DThreeVector& operator=(const Oxs_FFT1DThreeVector& other) {
    Dup(other); return *this;
  }

  void SetDimensions(OC_INDEX rsize,OC_INDEX csize,OC_INDEX array_count);
  // Import rsize is the size of each sub-array of 3-vectors without
  // zero-padding, i.e., the length in bytes of each run is
  // 3*rsize*sizeof(OXS_FFT_REAL_TYPE).  Import csize is the "nominal" size of
  // the complex 3-vector sub-array, which represents the size of the
  // FFT.  Because the input vector is real, the resulting FFT will be
  // conjugate symmetric, so it is only necessary to store (csize/2)+1
  // complex values.  (Actually, carr[0] and carr[csize/2] will be
  // real values, i.e., carr[0].im = carr[csize/2].im = 0.0, but we do
  // not take advantage of this wrt packing.)  The length in bytes of
  // each run in the transform space is
  // 3*((csize/2)+1)*(2*sizeof(OXS_FFT_REAL_TYPE)).
  //
  // NOTE 1: Conceptually, the input vector is taken to be a real
  // vector of length csize.  If rsize < csize, then the missing
  // elements are treated as though zero filled.
  //
  // NOTE 2: At present, this FFT code is strictly power-of-2, so
  // csize must be a power of 2.  In case this changes in the future,
  // client code should use the member function RecommendSize to get a
  // valid value for csize.  Usually, one has an array of length
  // rsize, which needs to be zero-padded to double length (in order
  // to emulate a linear convolution via the circular convolution
  // effected by FFTs), and then zero-padded up to an supported
  // FFT-size.  So, set csize = RecommendSize(2*rsize).
  //
  // NOTE 3: "Complex" vectors are actually handled as real vectors
  // packed as real, imag, real, imag, ...  No complex type is
  // actually defined or used, as an aid to optimization challenged
  // compilers.  So array imports to the various member functions are
  // always of "real" type, and csize is interpreted internally as
  // 2*csize for access purposes.
  //
  // NOTE 4: The FFT *actually* computed is not of length csize, but
  // rather csize/2, because the real array is first packed with
  // alternating values as real and imaginary parts into a complex
  // array of size csize/2, the full complex FFT is taken of the
  // packed array, and then the packed array is unpacked into the 
  // half-size+1 array that would arise if an FFT of size csize
  // had been taken instead on the original array.  Because of this,
  // the scale factor arising from taking a forward followed by
  // an inverse array is 2/csize rather than csize, as might be
  // expected.  Clients should use the GetScaling() member function
  // to adjust for this.

  void AdjustInputDimensions(OC_INDEX rsize,OC_INDEX array_count);
  // After SetDimensions has been called, this routine may be called to
  // change the (pre-zero-padding) rsize and array_count values.  Of
  // course, the new rsize value must be compatible with the original
  // csize value, as outlined in the SetDimensions discussion above.
  // Changing rsize also changes the stride distance between successive
  // rows of the input array.  However, AdjustInputDimension does not
  // change the logical dimensions of the FFTs, so the roots of unity,
  // scaling, and similar setup properties do not change.  All that is
  // changed is where the zero padding begins in the forward transform
  // (i.e., the extents of the input array), and the extents of the
  // output array in the inverse transform.  This routine throws an
  // exception if the new rsize value is not compatible with the csize
  // value.

  void AdjustArrayCount(OC_INDEX array_count) {
#ifndef NDEBUG
    if(array_count<1) {
      char buf[1024];
      Oc_Snprintf(buf,sizeof(buf),
                  "Illegal import to Oxs_FFT1DThreeVector::AdjustArrayCount:"
                  " array_count=%d",(int)array_count);
      OXS_THROW(Oxs_BadParameter,buf);
    }
#endif
    arrcount = array_count;
  }


  inline void ForwardRealToComplexFFT(const OXS_FFT_REAL_TYPE* rarr_in,
     OXS_FFT_REAL_TYPE* carr_out,const OXS_FFT_REAL_TYPE* mult=NULL) const {
    (this->*ForwardPtr)(rarr_in,carr_out,mult);
  }
  // NOTE: The inverse transform may use the carr_in as workspace during
  // computation; thus, carr_in is *not* const, unlike rarr_in for the
  // forward transform.
  inline void InverseComplexToRealFFT(OXS_FFT_REAL_TYPE* carr_in,
                                      OXS_FFT_REAL_TYPE* rarr_out) const {
    (this->*InversePtr)(carr_in,rarr_out);
  }

  // The following member function returns the multiplicative scale
  // factor s, such that s*Inverse(Forward(...)) = Identity.  This value
  // will be 2/csize, since the size of the complex FFT actually
  // performed is csize/2.  NB: This differs from the corresponding FFTW
  // library routines, which scale by 1/csize.  (It appears that the
  // extra factor of 2 in the FFTW case is built into the inverse routine,
  // because the FFTW forward routine returns the same scaling as the
  // OXS_FFTReal forward routine.)
  OXS_FFT_REAL_TYPE GetScaling() const {
    if(fftsize>0) return 1.0/static_cast<OXS_FFT_REAL_TYPE>(fftsize);
    return 1.0;
  }

  static OC_INDEX RecommendSize(OC_INDEX rsize); // Returns first
  // integer >= rsize that is supported by FFT code.  Currently code
  // is power of two only, so the returned value will be the smallest
  // power of 2 >= rsize.  Client code is encouraged to use this
  // member function for zero-pad sizing, in order to automatically
  // take advantage of reduced carr sizes if this code is ever
  // enhanced to support mixed radix FFT's.  This is especially
  // important for multidimensional FFT's, where unnecessary padding
  // is computationally expensive.  (Note to self: if mixed radix
  // FFT's are introduced, update csize restrictions documented above
  // in the constructor notes.)

  OC_INDEX GetLogicalDimension() const {
    // Logical ("nominal") dimension of transform.
    return (fftsize>0 ? 2*fftsize : 1);
  }

private:
  void (Oxs_FFT1DThreeVector::* ForwardPtr)
    (const OXS_FFT_REAL_TYPE* rarr_in,
     OXS_FFT_REAL_TYPE* carr_out,
     const OXS_FFT_REAL_TYPE* mult) const;
  void (Oxs_FFT1DThreeVector::* InversePtr)
    (OXS_FFT_REAL_TYPE* carr_in,
     OXS_FFT_REAL_TYPE* rarr_out) const;

  void AssignTransformPointers(); // Sets ForwardPtr and InversePtr
  /// to the appropriate routines detailed below.

  // ForwardRealToComplexFFTRadix4() performs a real-to-complex FFT
  // using decimation-in-frequency, with radix 4 blocks, inline
  // roots-of-unity U, pre-order traversal (depth-first walk), with
  // built-in zero padding.  If fftsize is an odd power of 2, then at
  // the block=32 level a single radix 2 pass is made.  The bottom
  // block=16 level algorithm has hard coded roots of unity and
  // embedded bit reversal.  There is an additional pass at the end to
  // unpack the real transform.  The ...ZP form assumes that the import
  // array size, rsize, is not more than half the requested complex
  // FFT size (rsize<=csize/2), or equivalently, rsize<=fftsize.
  void ForwardRealToComplexFFTRadix4(const OXS_FFT_REAL_TYPE* rarr_in,
                                     OXS_FFT_REAL_TYPE* carr_out,
                                     const OXS_FFT_REAL_TYPE* mult) const;
  void ForwardRealToComplexFFTRadix4ZP(const OXS_FFT_REAL_TYPE* rarr_in,
                                       OXS_FFT_REAL_TYPE* carr_out,
                                       const OXS_FFT_REAL_TYPE* mult) const;

  // InverseComplexToRealFFTRadix4 is the complementary process to
  // ForwardRealToComplexFFTRadix4.  In this case the real/complex
  // packing occur at the top (first), and the zero-padding effects are
  // at the bottom.  The ...ZP form assumes that the output array size,
  // rsize, is not more than half the requested complex FFT size
  // (rsize<=csize/2), or equivalently, rsize<=fftsize.
  void InverseComplexToRealFFTRadix4(OXS_FFT_REAL_TYPE* carr_in,
                                     OXS_FFT_REAL_TYPE* rarr_out) const;
  void InverseComplexToRealFFTRadix4ZP(OXS_FFT_REAL_TYPE* carr_in,
                                       OXS_FFT_REAL_TYPE* rarr_out) const;


  // Special handling for small powers-of-2.  The ...ZP forms assume the
  // last half of the effective input array is zero padding, i.e., that
  // rsize<=csize/2, or equivalently, that rsize<=fftsize.  The ...Size8
  // codes have built-in "ZP" handling.  A ZP variant is provided for
  // the forward Size4 case, but in testing doesn't appear be any faster
  // than the standard one, so no ZP variants are provided for the even
  // smaller transforms.
  void ForwardRealToComplexFFTSize0(const OXS_FFT_REAL_TYPE* rarr_in,
                                    OXS_FFT_REAL_TYPE* carr_out,
                                    const OXS_FFT_REAL_TYPE* mult) const;
  void InverseComplexToRealFFTSize0(OXS_FFT_REAL_TYPE* carr_in,
                                    OXS_FFT_REAL_TYPE* rarr_out) const;
  void ForwardRealToComplexFFTSize1(const OXS_FFT_REAL_TYPE* rarr_in,
                                    OXS_FFT_REAL_TYPE* carr_out,
                                    const OXS_FFT_REAL_TYPE* mult) const;
  void InverseComplexToRealFFTSize1(OXS_FFT_REAL_TYPE* carr_in,
                                    OXS_FFT_REAL_TYPE* rarr_out) const;
  void ForwardRealToComplexFFTSize2(const OXS_FFT_REAL_TYPE* rarr_in,
                                    OXS_FFT_REAL_TYPE* carr_out,
                                    const OXS_FFT_REAL_TYPE* mult) const;
  void InverseComplexToRealFFTSize2(OXS_FFT_REAL_TYPE* carr_in,
                                    OXS_FFT_REAL_TYPE* rarr_out) const;
  void ForwardRealToComplexFFTSize4(const OXS_FFT_REAL_TYPE* rarr_in,
                                    OXS_FFT_REAL_TYPE* carr_out,
                                    const OXS_FFT_REAL_TYPE* mult) const;
  void ForwardRealToComplexFFTSize4ZP(const OXS_FFT_REAL_TYPE* rarr_in,
                                      OXS_FFT_REAL_TYPE* carr_out,
                                      const OXS_FFT_REAL_TYPE* mult) const;
  void InverseComplexToRealFFTSize4(OXS_FFT_REAL_TYPE* carr_in,
                                    OXS_FFT_REAL_TYPE* rarr_out) const;
  void ForwardRealToComplexFFTSize8(const OXS_FFT_REAL_TYPE* rarr_in,
                                    OXS_FFT_REAL_TYPE* carr_out,
                                    const OXS_FFT_REAL_TYPE* mult) const;
  void InverseComplexToRealFFTSize8(OXS_FFT_REAL_TYPE* carr_in,
                                    OXS_FFT_REAL_TYPE* rarr_out) const;
  void ForwardRealToComplexFFTSize16(const OXS_FFT_REAL_TYPE* rarr_in,
                                     OXS_FFT_REAL_TYPE* carr_out,
                                     const OXS_FFT_REAL_TYPE* mult) const;
  void InverseComplexToRealFFTSize16(OXS_FFT_REAL_TYPE* carr_in,
                                     OXS_FFT_REAL_TYPE* rarr_out) const;
  void ForwardRealToComplexFFTSize16ZP(const OXS_FFT_REAL_TYPE* rarr_in,
                                       OXS_FFT_REAL_TYPE* carr_out,
                                       const OXS_FFT_REAL_TYPE* mult) const;
  void InverseComplexToRealFFTSize16ZP(OXS_FFT_REAL_TYPE* carr_in,
                                       OXS_FFT_REAL_TYPE* rarr_out) const;
  void ForwardRealToComplexFFTSize32(const OXS_FFT_REAL_TYPE* rarr_in,
                                     OXS_FFT_REAL_TYPE* carr_out,
                                     const OXS_FFT_REAL_TYPE* mult) const;
  void InverseComplexToRealFFTSize32(OXS_FFT_REAL_TYPE* carr_in,
                                     OXS_FFT_REAL_TYPE* rarr_out) const;
  void ForwardRealToComplexFFTSize32ZP(const OXS_FFT_REAL_TYPE* rarr_in,
                                       OXS_FFT_REAL_TYPE* carr_out,
                                       const OXS_FFT_REAL_TYPE* mult) const;
  void InverseComplexToRealFFTSize32ZP(OXS_FFT_REAL_TYPE* carr_in,
                                       OXS_FFT_REAL_TYPE* rarr_out) const;
  void ForwardRealToComplexFFTSize64(const OXS_FFT_REAL_TYPE* rarr_in,
                                     OXS_FFT_REAL_TYPE* carr_out,
                                     const OXS_FFT_REAL_TYPE* mult) const;
  void InverseComplexToRealFFTSize64(OXS_FFT_REAL_TYPE* carr_in,
                                     OXS_FFT_REAL_TYPE* rarr_out) const;
  void ForwardRealToComplexFFTSize64ZP(const OXS_FFT_REAL_TYPE* rarr_in,
                                       OXS_FFT_REAL_TYPE* carr_out,
                                       const OXS_FFT_REAL_TYPE* mult) const;
  void InverseComplexToRealFFTSize64ZP(OXS_FFT_REAL_TYPE* carr_in,
                                       OXS_FFT_REAL_TYPE* rarr_out) const;

  // Copy operators are required by STL vector template.
  void Dup(const Oxs_FFT1DThreeVector&);


  // Utility functions
  static OC_INDEX GetNextPowerOfTwo(OC_INDEX n,OC_INT4m& logsize);
  void FillRootsOfUnity();
  void FillPreorderTraversalStateArray();
  void FillBitReversalArray();
  void FreeMemory();

  OC_INDEX arrcount; // Number of FFT's to perform on each FFT call.
  OC_INDEX rsize;
  OC_INDEX rstride;  // Stride between start of one rsize stretch
  /// and the next, when arrcount>1.  Standard constructor sets this
  /// to 3*rsize.

  OC_INDEX fftsize;  // Complex FFT size.  This must be a power-of-2.
  /// Real vector is transformed by packing successive pairs of reals
  /// as real+imag parts of a complex vector of half-size.  Thus
  /// fftsize will be exactly half of the constructor import csize.
  /// The expansion of the general complex fftsize transform to the
  /// conjugate symmetric transform of conceptual size csize is handled
  /// in a separate pass through the data inside the forward/inverse
  /// routines.
  OC_INT4m log2fftsize;  // Convenience; log base 2 of fftsize

  // Root of unity arrays.  These are complex values stored as
  // side-by-side reals, real part followed by imaginary part.  The
  // UForward array traces through the lower half of the complex
  // plane.  The UForward sequence starts with
  //   1, a, a^2, a^3, ..., a^(fftsize-1)
  // where a = exp(-2*pi*i/(2*fftsize)), so the last entry in the
  // first part of this sequence is just shy of -1.  These are the
  // roots needed to take an FFT of length 2*fftsize, which is the
  // effective length of the real transform.  The actual complex
  // transform uses every other entry in this sequence, i.e., the
  // roots of a^2, but this full sequence is needed in the final
  // unpacking phase to polish off the real transform.  Note that
  // the length of this sequence is fftsize.
  //
  // Root-of-unity arrays, with inlined preorder-traversal (depth
  // first travel) ordering of roots, for Radix 4 algorithm.  These
  // are built up from UForward, but using b=a^2 as the base root, as
  // these arrays are used by the complex transforms of length
  // fftsize.
  //   The first part of UForwardRadix4 looks like
  //        b, b^2, b^3,  b^2, b^4, b^6, ...,  b^k, b^2k, b^3k
  //
  // where k = (fftsize/4)-1.  This allows the first pass of the radix
  // 4 decimation-in-frequency algorithm to access the needed roots of
  // unity sequentially from memory.  Note that the length of this
  // part of the sequence is 3*k = 3*fftsize/4 - 3.
  //
  //    The second level pass of the algorithm is based off of b^4,
  // and its sequence looks like
  //
  //        b^4, b^8, b^12,  b^8, b^16, b^24,
  //        ...,  b^(k-3), b^(2(k-3)), b^(3(k-3))
  //
  // These elements appear also in the first part of the
  // UForwardRadix4 sequence, as every fourth group of three elements,
  // but rather than require the algorithm to peck through the first
  // sequence, this new sequence is appended unto the first.  The
  // length of this sequence is 3*fftsize/16-3.
  //   Appending onto these is the subsequence based on b^16, then
  // b^64, etc., until b^m is the 32th or 64nd root of unity,
  // i.e.,
  //       b^m = e^(m.(-2.pi.i)/fftsize) = e^((-2.pi.i)/(64|128))
  //
  //   The transform for the 16th roots is hard coded, so in the case
  // where fftsize is a power of 4, say fftsize = 2^n with n even, the
  // total length of the UForwardRadix4 array is
  //
  //     array size = fftsize - 3*n/2 - 10,  if 2|n = log_2(fftsize).
  //
  // and the last roots stored correspond to the 64th roots of unity.
  //
  //   OTOH, if n is odd, then the sequence includes the 128th roots,
  // but at the 32nd roots level, rather than performing a radix 4
  // pass, the algorithm preforms a specially coded radix 2 pass,
  // before finishing up with the 16th root passes.  In this case the
  // last bit of the UForwardRadix4 (resp. UinverseRadix4) arrays are
  // just a direct listing of the 15 of the 32nd roots of unity with
  // imaginary part < (resp. >) 0.  So, in this case
  //
  //     array size = [fftsize - 3*(n-1)/2 - 26] + 15
  //                = fftsize - 3*(n-1)/2 - 11, 
  //                                    if n = log_2(fftsize) is odd.

  OXS_FFT_REAL_TYPE* UReals; // Roots of unity for unpacking
  /// real transform.
  OXS_FFT_REAL_TYPE* UForwardRadix4; // Radix 4, inline,
  /// preorder-traversal, with perhaps appended the radix-2 roots at the
  /// 32 level, depending on whether log_2(fftsize) is even or odd.  The
  /// corresponding inverse array is not needed, because each entry in
  /// it would be just the complex conjugate of the corresponding entry
  /// in the forward array.  Since we have separate routines for forward
  /// and inverse transforms, this difference can be handled in the
  /// code.  (Separate forward and inverse transform routines are needed
  /// in any case because the bottom-level compute blocks, using roots
  /// of unity of 16 and smaller, use hard coded roots.)

  // Preorder-traversal stride state, embedded with offsets into
  // roots-of-unity array.
  struct PreorderTraversalState {
    OC_INDEX stride;
    OC_INDEX uoff;
  };
  PreorderTraversalState* ptsRadix4;

  // Bit-flip swap index array.  0 indicates no swap.  A positive value
  // is the index of the value that needs to be swapped with the lookup
  // index.
  //   Two fine points: 1) the lookup index (e.g., i in "bitreverse[i]")
  // is based on the complex-value index, so that the length of
  // bitreverse[] is fftsize.  But the retrieved value (e.g.,
  // "bitreverse[i]") is the real-value array index, i.e., twice the
  // complex value.  For example, if fftsize is 4, then the contents of
  // bitreverse[] are: 0 0 2 0.  Here the values at offsets 0 and 3 are
  // 0 because they don't bitreverse (00 and 11 are self-reversed).  The
  // value at offset 1 is 0 as explained in point 2.  The value at
  // offset 2 is 2, because 10 -> 01, so complex index 2 swaps to
  // complex 1, and complex index 1 == real index 2.  Point 2) Only the
  // larger index in a swap pair is stored in bitreverse[].  The value
  // at the lower index is set to 0.
  OC_INDEX* bitreverse;
  void FillBitReverseArray();

  // Scratch space.  "scratch" is used for some of the bottom level
  // (radix 16 and smaller) passes, and is relatively small.
  // "workbuffer" is used by some routines to lessen memory write
  // traffic by reducing dirty locations in working memory; it is set to
  // hold one working row for the complex FFT:
  // OFTV_VECSIZE*((csize/2)+1)*OFTV_COMPLEXSIZE*sizeof(OXS_FFT_REAL_TYPE)
  // or, equivalently,
  // OFTV_VECSIZE*(fftsize+1)*OFTV_COMPLEXSIZE*sizeof(OXS_FFT_REAL_TYPE)
  // Both scratch and workbuffer are allocated in the
  // ::AssignTransformPointers() member function.
  OC_INDEX scratch_size;
  OXS_FFT_REAL_TYPE* scratch;
  void AllocScratchSpace(OC_INDEX size);
  OC_INDEX workbuffer_size;
  OXS_FFT_REAL_TYPE* workbuffer;
  void AllocBufferSpace();
};



////////////////////////////////////////////////////////////////////////
// Class Oxs_FFTStrided ////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

/* Class Oxs_FFTStrided
 *
 * Class for in-place, complex-to-complex FFT, where consecutive complex
 * values are separated by a fixed stride.  An array of these sequences
 * can be transformed with a single call, provided that the same index
 * entries are adjacent in memory, i.e.,
 *
 *         -- memory index increases by 1 -->
 *   
 *   |     a0_x  a0_y  b0_x  b0_y  c0_x c0_y ...
 *  mem    a1_x  a1_y  b1_x  b1_y  c1_x c1_y ...
 * index   ...
 *  +K     aj_x  aj_y  bj_x  bj_y  cj_x cj_y ...
 *   |     ...
 *   v
 *
 * Here the complex FFT is taken of the sequence (a), and also (b), (c),
 * ..., where the stride between a0 and a1 (and likewise (b0 and b1, c0
 * and c1, ak and ak+1, etc.) is fixed size K.  (The stride is actually
 * measured in OXS_FFT_REAL_TYPE units, which is half the size of a
 * complex item.)
 *
 * Implicit zero padding is also supported.
 *
 * This class is primarily intended for compound use to construct
 * multi-dimensional FFT's.
 */

class Oxs_FFTStrided {
public:
  Oxs_FFTStrided();
  ~Oxs_FFTStrided();

  // Copy operators.  These are required by STL vector template.
  Oxs_FFTStrided(const Oxs_FFTStrided& other) :
    ForwardPtr(0),InversePtr(0),         // Dummy initial values
    arrcount(0),csize_base(0),rstride(0),
    fftsize(0),log2fftsize(-1),
    UForwardRadix4(0),ptsRadix4(0),bitreverse(0),
    scratch_size(0),scratch(0)
    {
      Dup(other);
    }
  Oxs_FFTStrided& operator=(const Oxs_FFTStrided& other) {
    Dup(other); return *this;
  }

  void SetDimensions(OC_INDEX csize_base,OC_INDEX csize_zp,
		     OC_INDEX rstride,OC_INDEX array_count);
  // Import csize_base is the size of each sequence without
  // zero-padding, in complex units (one complex unit equals two
  // OXS_FFT_REAL_TYPE units).  Import csize_zp is the dimension of the
  // transform, including zero-padding, again in complex units.
  // csize_in must be <= csize_out.  In effect, for the forward
  // transform each sequence is padded out to size csize_zp with zeros
  // before the transform is applied.  For the inverse transform, the
  // values up to csize_zp are assumed set, but the values are only
  // computed for indices 0 through csize_base-1.  The values at the
  // remaining indices are not specified.
  //
  // Only in-place transforms are supported.  For the forward transform,
  // only the csize_base x array_count corner of the array needs to be
  // initialized, but the full csize_zp x array_count array must be
  // previously allocated.
  //
  // csize_zp must be "compatible" with supported FFT dimensions.  At
  // present, this mean csize_zp must be a power-of-2, but this may
  // change in the future.  Clients should use the RecommendSize member
  // function to select a valid csize_zp value.  If one is zero-padding
  // to minimally twice the base size, then csize_zp should be set to
  // RecommendSize(2*csize_base).
  //
  // The results are not scaled.  Use the GetScaling member funtion to
  // find the proper scaling, which the client is responsible for
  // applying.

  void AdjustInputDimensions(OC_INDEX csize_base,OC_INDEX rstride,
			     OC_INDEX array_count);
  // After SetDimensions has been called, this routine may be called to
  // change the (pre-zero-padding) csize_base, rstride and array_count
  // values.  Of course, the new csize_base must not larger than the
  // original csize_zp value.  The row stride and array_count values may
  // also be changed.  However, AdjustInputDimension does not change the
  // logical dimensions of the FFTs, so the roots of unity, scaling, and
  // similar setup properties do not change.  All that is changed is
  // where the zero padding begins in the forward transform (i.e., the
  // extents of the input array), and the extents of the output array in
  // the inverse transform.  This routine throws an exception if the
  // import parameters aren't compatible with the setup (in particular,
  // if the new csize_base is bigger than the existing csize_zp).
  //   Technical note: If the new rstride value is different than the
  // previous rstride value, then the PreorderTraversalState array and
  // BitReversalArray are updated.

  void AdjustArrayCount(OC_INDEX array_count) {
#ifndef NDEBUG
    if(array_count<1 || rstride<2*array_count) {
      char buf[1024];
      Oc_Snprintf(buf,sizeof(buf),
                  "Illegal import to Oxs_FFTStrided::AdjustArrayCount:"
                  " array_count=%d, rstride=%d",
                  (int)array_count,(int)rstride);
      OXS_THROW(Oxs_BadParameter,buf);
    }
#endif
    arrcount = array_count;
  }

  inline void ForwardFFT(OXS_FFT_REAL_TYPE* arr) const {
    (this->*ForwardPtr)(arr);
  }
  inline void InverseFFT(OXS_FFT_REAL_TYPE* arr) const {
    (this->*InversePtr)(arr);
  }

  // The following member function returns the multiplicative scale
  // factor s, such that s*Inverse(Forward(...)) = Identity.  This value
  // will be 1/csize_zp.
  OXS_FFT_REAL_TYPE GetScaling() const {
    if(fftsize>0) return 1.0/static_cast<OXS_FFT_REAL_TYPE>(fftsize);
    return 1.0;
  }

  static OC_INDEX RecommendSize(OC_INDEX size); // Returns first
  // integer >= size that is supported by FFT code.  Currently code
  // is power of two only, so the returned value will be the smallest
  // power of 2 >= size.  Client code is encouraged to use this
  // member function for zero-pad sizing, in order to automatically
  // take advantage of reduced array sizes if this code is ever
  // enhanced to support mixed radix FFT's.  This is especially
  // important for multidimensional FFT's, where unnecessary padding
  // is computationally expensive.  (Note to self: if mixed radix
  // FFT's are introduced, update size restrictions documented above
  // in the constructor notes.)

  OC_INDEX GetLogicalDimension() const {
    // Logical ("nominal") dimension of transform.
    return fftsize;
  }
private:
  void (Oxs_FFTStrided::* ForwardPtr)(OXS_FFT_REAL_TYPE* arr) const;
  void (Oxs_FFTStrided::* InversePtr)(OXS_FFT_REAL_TYPE* arr) const;

  void AssignTransformPointers(); // Sets ForwardPtr and InversePtr
  /// to the appropriate routines detailed below.

  // ForwardFFTRadix4() performs an in-place, complex-to-complex FFT
  // using decimation-in-frequency, with radix 4 blocks, inline
  // roots-of-unity U, pre-order traversal (depth-first walk), with
  // built-in zero padding.  If fftsize is an odd power of 2, then at
  // the block=32 level a single radix 2 pass is made.  The bottom
  // block=16 level algorithm has hard coded roots of unity and embedded
  // bit reversal.  The ...ZP form assumes that the import array size,
  // csize_base, is not more than half the requested complex FFT size
  // (csize_baes<=csize_zp/2), or equivalently, csize_base<=fftsize.
  void ForwardFFTRadix4(OXS_FFT_REAL_TYPE* arr) const;
  void ForwardFFTRadix4ZP(OXS_FFT_REAL_TYPE* arr) const;

  // InverseFFTRadix4 is the complementary process to ForwardFFTRadix4.
  // In this case the zero-padding effects are at the end, where the
  // upper half of the transform is not computed.  The ...ZP form
  // assumes that the import array size, csize_base, is not more than
  // half the requested complex FFT size (csize_base<=csize_zp/2), or
  // equivalently, csize_base<=fftsize.
  void InverseFFTRadix4(OXS_FFT_REAL_TYPE* arr) const;
  void InverseFFTRadix4ZP(OXS_FFT_REAL_TYPE* arr) const;


  // Special handling for small powers-of-2.  The ...ZP forms assume the
  // last half of the effective input array is zero padding.

  void FFTNop(OXS_FFT_REAL_TYPE* arr) const; // No operation; Used
  /// for size 1 FFT.

  void FFTSize2(OXS_FFT_REAL_TYPE* arr) const; // forward == inverse
  void ForwardFFTSize2ZP(OXS_FFT_REAL_TYPE* arr) const;
  void InverseFFTSize2ZP(OXS_FFT_REAL_TYPE* arr) const;

  void ForwardFFTSize4(OXS_FFT_REAL_TYPE* arr) const;
  void InverseFFTSize4(OXS_FFT_REAL_TYPE* arr) const;
  void ForwardFFTSize4ZP(OXS_FFT_REAL_TYPE* arr) const;
  void InverseFFTSize4ZP(OXS_FFT_REAL_TYPE* arr) const;

  void ForwardFFTSize8(OXS_FFT_REAL_TYPE* arr) const;
  void InverseFFTSize8(OXS_FFT_REAL_TYPE* arr) const;
  void ForwardFFTSize8ZP(OXS_FFT_REAL_TYPE* arr) const;
  void InverseFFTSize8ZP(OXS_FFT_REAL_TYPE* arr) const;

  void ForwardFFTSize16(OXS_FFT_REAL_TYPE* arr) const;
  void InverseFFTSize16(OXS_FFT_REAL_TYPE* arr) const;
  void ForwardFFTSize16ZP(OXS_FFT_REAL_TYPE* arr) const;
  void InverseFFTSize16ZP(OXS_FFT_REAL_TYPE* arr) const;

  void ForwardFFTSize32(OXS_FFT_REAL_TYPE* arr) const;
  void InverseFFTSize32(OXS_FFT_REAL_TYPE* arr) const;
  void ForwardFFTSize32ZP(OXS_FFT_REAL_TYPE* arr) const;
  void InverseFFTSize32ZP(OXS_FFT_REAL_TYPE* arr) const;

  void ForwardFFTSize64(OXS_FFT_REAL_TYPE* arr) const;
  void InverseFFTSize64(OXS_FFT_REAL_TYPE* arr) const;
  void ForwardFFTSize64ZP(OXS_FFT_REAL_TYPE* arr) const;
  void InverseFFTSize64ZP(OXS_FFT_REAL_TYPE* arr) const;

  // Copy operators are required by STL vector template.
  void Dup(const Oxs_FFTStrided& other);


  // Utility functions
  static OC_INDEX GetNextPowerOfTwo(OC_INDEX n,OC_INT4m& logsize);
  void FillRootsOfUnity();
  void FillPreorderTraversalStateArray();
  void FillBitReversalArray();
  void FreeMemory();

  OC_INDEX arrcount; // Number of FFT's to perform on each FFT call.
  OC_INDEX csize_base;
  OC_INDEX rstride;  // Stride between successive elements in one
  /// FFT sequence, in OXS_FFT_REAL_TYPE units.

  OC_INDEX fftsize;  // Complex FFT size.  This must be a power-of-2.
  OC_INT4m log2fftsize;    // Convenience; log base 2 of fftsize

  // Root of unity arrays.  These are complex values stored as
  // side-by-side reals, real part followed by imaginary part.  The
  // UForward array traces through the lower half of the complex
  // plane.  The UForward sequence starts with
  //   1, a, a^2, a^3, ..., a^(fftsize-1)
  // where a = exp(-2*pi*i/(2*fftsize)), so the last entry in the
  // first part of this sequence is just shy of -1.  These are the
  // roots needed to take an FFT of length 2*fftsize, which is the
  // effective length of the real transform.  The actual complex
  // transform uses every other entry in this sequence, i.e., the
  // roots of a^2, but this full sequence is needed in the final
  // unpacking phase to polish off the real transform.  Note that
  // the length of this sequence is fftsize.
  //
  // Root-of-unity arrays, with inlined preorder-traversal (depth
  // first travel) ordering of roots, for Radix 4 algorithm.  These
  // are built up from UForward, but using b=a^2 as the base root, as
  // these arrays are used by the complex transforms of length
  // fftsize.
  //   The first part of UForwardRadix4 looks like
  //        b, b^2, b^3,  b^2, b^4, b^6, ...,  b^k, b^2k, b^3k
  //
  // where k = (fftsize/4)-1.  This allows the first pass of the radix
  // 4 decimation-in-frequency algorithm to access the needed roots of
  // unity sequentially from memory.  Note that the length of this
  // part of the sequence is 3*k = 3*fftsize/4 - 3.
  //
  //    The second level pass of the algorithm is based off of b^4,
  // and its sequence looks like
  //
  //        b^4, b^8, b^12,  b^8, b^16, b^24,
  //        ...,  b^(k-3), b^(2(k-3)), b^(3(k-3))
  //
  // These elements appear also in the first part of the
  // UForwardRadix4 sequence, as every fourth group of three elements,
  // but rather than require the algorithm to peck through the first
  // sequence, this new sequence is appended unto the first.  The
  // length of this sequence is 3*fftsize/16-3.
  //   Appending onto these is the subsequence based on b^16, then
  // b^64, etc., until b^m is the 32th or 64nd root of unity,
  // i.e.,
  //       b^m = e^(m.(-2.pi.i)/fftsize) = e^((-2.pi.i)/(64|128))
  //
  //   The transform for the 16th roots is hard coded, so in the case
  // where fftsize is a power of 4, say fftsize = 2^n with n even, the
  // total length of the UForwardRadix4 array is
  //
  //     array size = fftsize - 3*n/2 - 10,  if 2|n = log_2(fftsize).
  //
  // and the last roots stored correspond to the 64th roots of unity.
  //
  //   OTOH, if n is odd, then the sequence includes the 128th roots,
  // but at the 32nd roots level, rather than performing a radix 4
  // pass, the algorithm preforms a specially coded radix 2 pass,
  // before finishing up with the 16th root passes.  In this case the
  // last bit of the UForwardRadix4 (resp. UinverseRadix4) arrays are
  // just a direct listing of the 15 of the 32nd roots of unity with
  // imaginary part < (resp. >) 0.  So, in this case
  //
  //     array size = [fftsize - 3*(n-1)/2 - 26] + 15
  //                = fftsize - 3*(n-1)/2 - 11, 
  //                                    if n = log_2(fftsize) is odd.

  OXS_FFT_REAL_TYPE* UForwardRadix4; // Radix 4, inline,
  /// preorder-traversal, with perhaps appended the radix-2 roots at the
  /// 32 level, depending on whether log_2(fftsize) is even or odd.  The
  /// corresponding inverse array is not needed, because each entry in
  /// it would be just the complex conjugate of the corresponding entry
  /// in the forward array.  Since we have separate routines for forward
  /// and inverse transforms, this difference can be handled in the
  /// code.  (Separate forward and inverse transform routines are needed
  /// in any case because the bottom-level compute blocks, using roots
  /// of unity of 16 and smaller, use hard coded roots.)

  // Preorder-traversal stride state, embedded with offsets into
  // roots-of-unity array.
  struct PreorderTraversalState {
    OC_INDEX stride;
    OC_INDEX uoff;
  };
  PreorderTraversalState* ptsRadix4;

  // Bit-flip swap index array.  0 indicates no swap.  A positive value
  // is the index of the value that needs to be swapped with the lookup
  // index.
  //   Two fine points: 1) the lookup index (e.g., i in "bitreverse[i]")
  // is based on the complex-value index, so that the length of
  // bitreverse[] is fftsize.  But the retrieved value (e.g.,
  // "bitreverse[i]") is the real-value array index, i.e., twice the
  // complex value.  For example, if fftsize is 4, then the contents of
  // bitreverse[] are: 0 0 2 0.  Here the values at offsets 0 and 3 are
  // 0 because they don't bitreverse (00 and 11 are self-reversed).  The
  // value at offset 1 is 0 as explained in point 2.  The value at
  // offset 2 is 2, because 10 -> 01, so complex index 2 swaps to
  // complex 1, and complex index 1 == real index 2.  Point 2) Only the
  // larger index in a swap pair is stored in bitreverse[].  The value
  // at the lower index is set to 0.
  OC_INDEX* bitreverse;
  void FillBitReverseArray();

  // Scratch space
  OC_INDEX scratch_size;
  OXS_FFT_REAL_TYPE* scratch;
  void AllocScratchSpace(OC_INDEX size);

};


////////////////////////////////////////////////////////////////////////
// Class Oxs_FFT3DThreeVector //////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

/* Class Oxs_FFT3DThreeVector
 *
 * Class for real-to-complex and complex-to-real FFT of a 3D array
 * of three vectors.
 *
 */

class Oxs_FFT3DThreeVector {
public:
  Oxs_FFT3DThreeVector();
  ~Oxs_FFT3DThreeVector();

  static void RecommendDimensions(OC_INDEX rdim1,OC_INDEX rdim2,OC_INDEX rdim3,
                            OC_INDEX& cdim1,OC_INDEX& cdim2,OC_INDEX& cdim3);
  // Imports are rdim1, rdim2, rdim3.  Exports are cdim1, cdim2, cdim3.
  // Use the values cdim1 x cdim2 x cdim3 to allocate an appropriately
  // sized array to hold the complexed-valued FFT-transformed results,
  // e.g.,
  //       carr = new OXS_FFT_REAL_TYPE[2*cdim1*cdim2*cdim3];
  // where the "2" comes because 1 complex value is 2 REAL values.
  //
  // Each corresponding csize value is the smallest integer >= the
  // corresponding rsize that is supported by FFT code.  As discussed
  // above, cdim2 = csize2, cdim3 = csize3, but cdim1 = (csize1/2)+1.
  // Currently code is power of two only, so the computed csize value
  // will be the smallest power of 2 >= rsize.  Client code is
  // encouraged to use this member function for zero-pad sizing, in
  // order to automatically take advantage of reduced csize values if
  // this code is ever enhanced to support mixed radix FFT's.  This is
  // especially important for multidimensional FFT's, where unnecessary
  // padding is computationally expensive.  (Note to self: if mixed
  // radix FFT's are introduced, update csize restrictions documented
  // above in the constructor notes.)

#if !OC_INDEX_CHECKS && OC_INT4m_WIDTH != OC_INDEX_WIDTH
  // Wrapper for backward compatibility
  static void RecommendDimensions(OC_INT4m rdim1,OC_INT4m rdim2,OC_INT4m rdim3,
                              OC_INT4m& cdim1,OC_INT4m& cdim2,OC_INT4m& cdim3);
#endif

  void SetDimensions(OC_INDEX rdim1,OC_INDEX rdim2,OC_INDEX rdim3,
		     OC_INDEX cdim1,OC_INDEX cdim2,OC_INDEX cdim3);
  // Imports rdim1, rdim2, rdim3 are the dimensions of the input
  // real-valued ThreeVector array.  Imports cdim1, cdim2, cdim3 are
  // physical dimensions of the output complex-valued ThreeVector
  // array.  The nominal (i.e., logical) sizes of the complex-valued
  // output array are csize1 x csize2 x csize3 where csize1 =
  // 2*(cdim1-1), csize2 = cdim2, csize3 = cdim3.  This is a result of
  // the input vector being real, so that the FFT-transformed results
  // will be conjugate-symmetric along the first axis; thus the *true*
  // dimensions of the complex array are ((csize1/2))+1 x csize2 x
  // csize3.  (Actually, carr[0][j][k] and carr[cdim1-1][j][k] will be
  // real values, i.e., carr[0][j][k].im = carr[cdim1-1][j][k].im =
  // 0.0, and so if one wishes the real parts could be packed together
  // as a single complex value and the first dimension reduced from
  // csize1/2+1 to csize1/2.  However, this class does not do this.
  // Such packing was done in the older Oxs_FFT3D class, but doing so
  // increases the code complexity and increases the possibility of
  // cache thrashing, since then &carr[i][j][k] and &carr[i][j+1][k]
  // differ by exactly csize1/2, which for power-of-2 transforms will
  // be a power of 2, and hence may map to the same cache line.)  The
  // length in bytes of the real input array is thus
  // 3*rdim1*rdim2*rdim3*sizeof(OXS_FFT_REAL_TYPE), and the length in
  // bytes of the complex output array is
  // 3*cdim1*cdim2*cdim3*(2*sizeof(OXS_FFT_REAL_TYPE)).  Note that
  // csize? >= rdim?  for ?=1,2,3, so in particular 2*(cdim1-1)>=rdim1
  // ==> cdim1 >= (rdim1/2)+1.  In any event, it is best to use the
  // RecommendDimensions member function to select appropriate values
  // for cdim?, because there are other dimension restrictions (in
  // particular power-of-2 requirements) that must also be adhered to.
  //
  // The relation csize1 = 2*(cdim1-1) only holds for cdim1>1.  For
  // cdim1=1 we have the special case csize1=1 iff cdim=1.
  //
  // The results are not scaled.  Use the GetScaling member funtion to
  // find the proper scaling, which the client is responsible for
  // applying.
  //
  // NOTE 1: Conceptually, the input array is taken to be a real array
  // of dimensions csize1 x csize2 x csize3.  If rsize? < csize?, then
  // the missing elements are treated as though zero filled, i.e., this
  // class supports implicit zero-padding.
  //
  // NOTE 2: At present, this FFT code is strictly power-of-2, so each
  // csize? must be a power of 2.  In case this changes in the future,
  // client code should use the member function RecommendDimensions to get
  // valid values for cdim?.  Usually, one has an array of dimensions
  // rdim1 x rdim2 x rdim3, which needs to be zero-padded to double
  // length (in order to emulate a linear convolution via the circular
  // convolution effected by FFTs), and then zero-padded up to an
  // supported FFT-size.  So, set cdim1 x cdim2 x cdim3 =
  // RecommendSize(2*rdim1,2*rdim2,2*rdim3).
  //
  // NOTE 3: "Complex" vectors are actually handled as real vectors
  // packed as real, imag, real, imag, ...  No complex type is
  // actually defined or used, as an aid to optimization challenged
  // compilers.  So array imports to the various member functions are
  // always of "real" type, and csize is interpreted internally as
  // 2*csize for access purposes.
  //
  // NOTE 4: The FFT *actually* computed along the first dimension is
  // not of length csize1, but rather csize1/2, because the real array
  // is first packed with alternating values as real and imaginary
  // parts into a complex array of size csize1/2, the full complex FFT
  // is taken of the packed array, and then the packed array is
  // unpacked into the half-size+1 array that would arise if an FFT of
  // size csize had been taken instead on the original array.  Because
  // of this, the scale factor arising from taking a forward followed
  // by an inverse array is 2/csize1 rather than csize1, as might be
  // expected.  Clients should use the GetScaling() member function to
  // adjust for this.

  void AdjustInputDimensions(OC_INDEX rdim1,OC_INDEX rdim2,OC_INDEX rdim3);
  // After SetDimensions has been called, this routine may be called
  // to change the size of the (pre-zero-padding) rdim# values.  Of
  // course, the new rdim values must be compatible with the original
  // cdim values, as outlined in the SetDimensions discussion above.
  // AdjustInputDimensions does not change the logical dimensions of
  // the FFTs, so the roots of unity, scaling, and similar setup
  // properties do not change.  All that is changed is where the zero
  // padding begins in the forward transform (i.e., the extents of the
  // input array), and the extents of the output array in the inverse
  // transform.  This routine throws an exception if the new rdim
  // values are not compatible with the original cdim values.

  // The following member function exports the values of csize1,
  // csize2, and csize3 as defined above in the discussion following
  // the SetDimensions declaration.
  void GetLogicalDimensions(OC_INDEX& ldim1,OC_INDEX& ldim2, OC_INDEX& ldim3) {
    ldim1 = fftx.GetLogicalDimension();
    ldim2 = ffty.GetLogicalDimension();
    ldim3 = fftz.GetLogicalDimension();
  }

  // The following member function returns the multiplicative scale
  // factor s, such that s*Inverse(Forward(...)) = Identity.  This
  // value will be (2/csize1)*(1/csize2)*(1/csize3); the 2 on the
  // first term arises because the size of the complex FFT along the
  // first dimension is actually csize1/2.  NB: This differs from the
  // corresponding FFTW library routines, which scale by 1/csize1.
  // (It appears that the extra factor of 2 in the FFTW case is built
  // into the inverse routine, because the FFTW forward routine
  // returns the same scaling as the OXS_FFTReal forward routine.)
  OXS_FFT_REAL_TYPE GetScaling() const {
    return fftx.GetScaling()*ffty.GetScaling()*fftz.GetScaling();
  }

  void ForwardRealToComplexFFT(const OXS_FFT_REAL_TYPE* rarr_in,
                               OXS_FFT_REAL_TYPE* carr_out) const;

  // NOTE: The inverse transform may use the carr_in as workspace during
  // computation; thus, carr_in is *not* const, unlike rarr_in for the
  // forward transform.
  void InverseComplexToRealFFT(OXS_FFT_REAL_TYPE* carr_in,
                               OXS_FFT_REAL_TYPE* rarr_out) const;

private:

  // Disable copy constructors by declaring but not defining:
  Oxs_FFT3DThreeVector(const Oxs_FFT3DThreeVector&);
  Oxs_FFT3DThreeVector& operator=(const Oxs_FFT3DThreeVector&);

  // Dimensions
  OC_INDEX rdim1; // Saved values from constructor
  OC_INDEX rdim2;
  OC_INDEX rdim3;
  OC_INDEX cdim1;
  OC_INDEX cdim2;
  OC_INDEX cdim3;
  OC_INDEX rxydim; // Convenience. rxydim = 3 * rdim1 * rdim2;
  OC_INDEX cxydim_rs; // Convenience. cxydim = 2 * 3 * cdim1 * cdim2,
  /// where the "2" converts from complex stride to real stride,
  /// and "3" accounts for the inputs being three-vectors.

  // Component FFT classes
  Oxs_FFT1DThreeVector fftx;
  Oxs_FFTStrided ffty;
  Oxs_FFTStrided fftz;

};

#endif /* _OXS_FFT3V_H */
