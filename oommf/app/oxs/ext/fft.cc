/* FILE: fft.cc             -*-Mode: c++-*-
 *
 * C++ code to do 1 and 3 dimensional FFT's.
 *
 */

#include <cstdio> // For some versions of g++ on Windows, if stdio.h is
/// #include'd after a C++-style header, then printf format modifiers
/// like OC_INDEX_MOD and OC_INT4m_MOD aren't handled properly.

#include <string>

#include "nb.h"  // For constants PI and SQRT1_2
#include "oxsexcept.h"
#include "fft.h"

OC_USE_STRING;

OC_USE_STD_NAMESPACE;  // Specify std namespace, if supported.
/// For some compilers this is needed to get "long double"
/// versions of the basic math library functions.

/* End includes */

#define OXS_FFT_SQRT1_2 OXS_FFT_REAL_TYPE(WIDE_SQRT1_2)
/// Get the right width

#undef USE_COMPLEX

#define CMULT(xr,xi,yr,yi,zr,zi) (zr) = (xr)*(yr)-(xi)*(yi), \
                                 (zi) = (xr)*(yi)+(xi)*(yr)

////////////////////////////////////////////////////////////////////////
// 1D Complex FFT
OC_BOOL Oxs_FFT::PackCheck()
{ // Routine to check Oxs_Complex packing restriction.  Much of the
  // FFT code relies on tight packing of two reals as 1 complex.
  // Packing failure has been observed when OXS_FFT_REAL_TYPE is a
  // long double with a compiler (such as Borland C++ 5.5) that stores
  // a long double with a short word (e.g., 10 bytes).  This routine
  // returns 0 on success.
  return (sizeof(Oxs_Complex)!=2*sizeof(OXS_COMPLEX_REAL_TYPE));
}

void Oxs_FFT::ReleaseMemory() const
{
  if(vecsize>0) {
    if(Uforward!=(Oxs_Complex *)NULL)       delete[] Uforward;
    if(Uinverse!=(Oxs_Complex *)NULL)       delete[] Uinverse;
    if(permindex!=(OXS_FFT_INT_TYPE *)NULL) delete[] permindex;
  }
  vecsize=0;
  vecsize_normalization=0.; // Dummy value
  log2vecsize=-1;    // Dummy value
  Uforward=Uinverse=(Oxs_Complex *)NULL;
  permindex=(OXS_FFT_INT_TYPE *)NULL;
  if(realdata_vecsize>0) {
    if(realdata_wforward!=(Oxs_Complex *)NULL) delete[] realdata_wforward;
    if(realdata_winverse!=(Oxs_Complex *)NULL) delete[] realdata_winverse;
  }
  realdata_vecsize=0;
  realdata_wforward=realdata_winverse=(Oxs_Complex *)NULL;
}

Oxs_FFT::Oxs_FFT() : 
  vecsize(0),vecsize_normalization(0.),log2vecsize(-1),
  Uforward(NULL),Uinverse(NULL),permindex(NULL),
  realdata_vecsize(0),realdata_wforward(NULL),realdata_winverse(NULL)
{
  if(PackCheck()!=0) {
    OXS_THROW(Oxs_ProgramLogicError,
              "Oxs_Complex structure packing failure"
              " detected in Oxs_FFT::Oxs_FFT()");
  }
}

Oxs_FFT::~Oxs_FFT()
{
  ReleaseMemory();
}

void Oxs_FFT::InitializeRealDataTransforms(OXS_FFT_INT_TYPE size) const
{ // Import size is the desired value for realdata_vecsize.
  // This function is conceptually const.
  if(realdata_vecsize==size) return;
  if(realdata_vecsize>0) {
    if(realdata_wforward!=(Oxs_Complex *)NULL) delete[] realdata_wforward;
    if(realdata_winverse!=(Oxs_Complex *)NULL) delete[] realdata_winverse;
  }
  realdata_vecsize=0;
  realdata_wforward=realdata_winverse=(Oxs_Complex *)NULL;
  if(size>=4) {
    if((realdata_winverse=new Oxs_Complex[size/4])==0) {
      OXS_THROW(Oxs_NoMem,"Out of memory error in "
	       "Oxs_FFT::InitializeRealDataTransforms(OXS_FFT_INT_TYPE)");
    }
    if((realdata_wforward=new Oxs_Complex[size/4])==0) {
      OXS_THROW(Oxs_NoMem,"Out of memory error in "
	       "Oxs_FFT::InitializeRealDataTransforms(OXS_FFT_INT_TYPE)");
    }
    realdata_wforward[0].Set(0.5,0);
    realdata_winverse[0].Set(1,0);
    double baseang = -2*WIDE_PI/double(size);
    for(OXS_FFT_INT_TYPE k=1;k<size/8;k++) {
      double angle=k*baseang;
      double y=sin(angle);
      double x=cos(angle);
      realdata_winverse[k].Set(x,-y);
      realdata_winverse[size/4-k].Set(-y,x);
      realdata_wforward[k].Set(0.5*x,0.5*y);
      realdata_wforward[size/4-k].Set(-0.5*y,-0.5*x);
    }
    if(size>=8) {
      realdata_winverse[size/8] = Oxs_Complex(OXS_FFT_SQRT1_2,OXS_FFT_SQRT1_2);
      realdata_wforward[size/8] = Oxs_Complex(0.5*OXS_FFT_SQRT1_2,-0.5*OXS_FFT_SQRT1_2);
    }
    realdata_vecsize=size;
  }
}

void Oxs_FFT::FillRootsOfUnity(OXS_FFT_INT_TYPE size,
			       Oxs_Complex* fwdroot,
			       Oxs_Complex* invroot) const
{
  for(OXS_FFT_INT_TYPE k=1;k<size/8;k++) {
    double angle = (double(-2*k)/double(size))*WIDE_PI;
    double y=sin(angle);
    double x=cos(angle);
    fwdroot[k]          = invroot[size-k]     = Oxs_Complex(x,y);
    fwdroot[size/2-k]   = invroot[size/2+k]   = Oxs_Complex(-x,y);
    fwdroot[size/2+k]   = invroot[size/2-k]   = Oxs_Complex(-x,-y);
    fwdroot[size-k]     = invroot[k]          = Oxs_Complex(x,-y);
    fwdroot[size/4-k]   = invroot[3*size/4+k] = Oxs_Complex(-y,-x);
    fwdroot[size/4+k]   = invroot[3*size/4-k] = Oxs_Complex(y,-x);
    fwdroot[3*size/4-k] = invroot[size/4+k]   = Oxs_Complex(y,x);
    fwdroot[3*size/4+k] = invroot[size/4-k]   = Oxs_Complex(-y,x);
  }
  fwdroot[0]=invroot[0]=Oxs_Complex(1,0);
  if(size>1) {
    fwdroot[size/2]=invroot[size/2]=Oxs_Complex(-1,0);
  }
  if(size>3) {
    fwdroot[size/4]  =invroot[3*size/4]=Oxs_Complex(0,-1);
    fwdroot[3*size/4]=invroot[size/4]  =Oxs_Complex(0,1);
  }
  if(size>7) {
    double x=OXS_FFT_SQRT1_2;  // 1/sqrt(2)
    double y=-x;
    fwdroot[size/8]   = invroot[7*size/8] = Oxs_Complex(x,y);
    fwdroot[3*size/8] = invroot[5*size/8] = Oxs_Complex(-x,y);
    fwdroot[5*size/8] = invroot[3*size/8] = Oxs_Complex(-x,-y);
    fwdroot[7*size/8] = invroot[size/8]   = Oxs_Complex(x,-y);
  }
}

void Oxs_FFT::Setup(OXS_FFT_INT_TYPE size) const
{
  if(size<1) {
    char buf[512];
    sprintf(buf,"Error in Oxs_FFT::Setup(OXS_FFT_INT_TYPE): Requested "
		"length (%" OXS_FFT_INT_MOD "d) must be >0",size);
    OXS_THROW(Oxs_BadParameter,buf);
  }
  OXS_FFT_INT_TYPE k;
  OXS_FFT_INT_TYPE power_count=0;
  // Check that size is power of 2
  for(k=size;k>=2;k/=2) {
    if(k%2!=0) {
      char buf[512];
      sprintf(buf,"Error in Oxs_FFT::Setup(OXS_FFT_INT_TYPE): Requested "
	       "length (%" OXS_FFT_INT_MOD "d) is not a power of 2",size);
      OXS_THROW(Oxs_BadParameter,buf);
    }
    power_count++;
  }
  ReleaseMemory();
  vecsize=size;
  vecsize_normalization=1./double(vecsize);
  log2vecsize=power_count;

  // Allocate and setup Oxs_Complex arrays
  if((Uforward=new Oxs_Complex[size])==0)
    OXS_THROW(Oxs_NoMem,"Out of memory error in Oxs_FFT::Setup");
  if((Uinverse=new Oxs_Complex[size])==0)
    OXS_THROW(Oxs_NoMem,"Out of memory error in Oxs_FFT::Setup");
  FillRootsOfUnity(size,Uforward,Uinverse);

  // Allocate and setup (bit-reversal) permutation index
  if((permindex=new OXS_FFT_INT_TYPE[size])==0)
    OXS_THROW(Oxs_NoMem,"Out of memory error in Oxs_FFT::Setup");
  permindex[0]=0;
  OXS_FFT_INT_TYPE m,n; // The following code relies heavily on
                       /// size==2^log2vecsize
  for(k=1,n=size>>1;k<size;k++) {
    // At each step, n is bit-reversed pattern of k
    if(n>k) permindex[k]=n;  // Swap index
    else permindex[k]=0;     // Do nothing: Index already swapped or the same
    // Calculate next n
    m=size>>1;
    while(m>0 && n&m) { n-=m; m>>=1; }
    n+=m;
  }
}

void Oxs_FFT::ForwardDecFreqRealDataNoScale(OXS_FFT_INT_TYPE size,
                                            void *vec) const
{ // Performs FFT on a real sequence.  size must be a positive power of
  // two.  The calculation is done in-place, where on return the array
  // should be interpreted as a complex array with real and imaginary
  // parts interleaved, except that ((Oxs_Complex*)vec)[0].re is the
  // 0-th element in the transform space (which is real because the
  // input is real), and ((Oxs_Complex*)vec)[0].im is the size/2-th
  // element in the transform space (which is also real for the same
  // reason).
  //   The physical length on output is size/2 complex elements, but
  // this is sufficient to define the whole transform of length size
  // because in the transform space the array is conjugate symmentric
  // (since the input is real); this means that virtual element
  //
  //   ((Oxs_Complex*)vec)[k] = conj(((Oxs_Complex*)vec)[size-k])
  //
  // for size/2<k<size.

  OXS_FFT_INT_TYPE csize=size/2;
  if(csize!=vecsize) Setup(csize);

  if(size>8) {
    // General case

    Oxs_Complex* cvec = static_cast<Oxs_Complex*>(vec);
    ForwardDecFreqNoScale(csize,cvec); // Perform half-sized FFT

    // Unpack.  See mjd NOTES I, 5-Nov-1996, p86.
    if(realdata_vecsize!=size) InitializeRealDataTransforms(size);
    OXS_FFT_REAL_TYPE a=cvec[0].re;
    OXS_FFT_REAL_TYPE b=cvec[0].im;
    cvec[0].Set(a+b,a-b);
    cvec[csize/2].im *= -1;
    OXS_FFT_INT_TYPE k=csize/2-1;
    do {
      OXS_FFT_REAL_TYPE resum  = 0.5 * (cvec[k].re + cvec[csize-k].re);
      OXS_FFT_REAL_TYPE rediff =       (cvec[k].re - cvec[csize-k].re);
      OXS_FFT_REAL_TYPE imdiff = 0.5 * (cvec[k].im - cvec[csize-k].im);
      OXS_FFT_REAL_TYPE imsum  =       (cvec[k].im + cvec[csize-k].im);

      OXS_FFT_REAL_TYPE wx=realdata_wforward[k].re;
      OXS_FFT_REAL_TYPE wy=realdata_wforward[k].im;

      OXS_FFT_REAL_TYPE temp1 = wx*imsum + wy*rediff;
      OXS_FFT_REAL_TYPE temp2 = wy*imsum - wx*rediff;

      cvec[k].Set(resum+temp1,temp2+imdiff);
      cvec[csize-k].Set(resum-temp1,temp2-imdiff);
    } while((--k)>0);
  } else if(size==8) { // Special case
    Oxs_Complex* cvec = static_cast<Oxs_Complex*>(static_cast<void*>(vec));
    ForwardDecFreqNoScale(4,cvec); // Perform half-sized FFT

    // Unpack.  See mjd NOTES I, 5-Nov-1996, p86.
    OXS_FFT_REAL_TYPE a=cvec[0].re;
    OXS_FFT_REAL_TYPE b=cvec[0].im;
    cvec[0].Set(a+b,a-b);

    OXS_FFT_REAL_TYPE resum  = 0.5 * (cvec[1].re + cvec[3].re);
    OXS_FFT_REAL_TYPE rediff =       (cvec[1].re - cvec[3].re);
    OXS_FFT_REAL_TYPE imdiff = 0.5 * (cvec[1].im - cvec[3].im);
    OXS_FFT_REAL_TYPE imsum  =       (cvec[1].im + cvec[3].im);

    // Note: OXS_FFT_SQRT1_2 = 1./sqrt(2.)
    OXS_FFT_REAL_TYPE temp1 = (imsum  - rediff)*(0.5*OXS_FFT_SQRT1_2);
    OXS_FFT_REAL_TYPE temp2 = (rediff + imsum)*(0.5*OXS_FFT_SQRT1_2);

    cvec[1].Set(resum+temp1,imdiff-temp2);
    cvec[3].Set(resum-temp1,-imdiff-temp2);

    cvec[2].im *= -1;

  } else if(size==4) { // Special case
    OXS_FFT_REAL_TYPE *rvec = static_cast<OXS_FFT_REAL_TYPE *>(vec);
    OXS_FFT_REAL_TYPE s02 = rvec[0]+rvec[2];
    rvec[2] = rvec[0]-rvec[2];
    OXS_FFT_REAL_TYPE s13 = rvec[1]+rvec[3];
    rvec[3] -= rvec[1];
    rvec[0]=s02+s13;
    rvec[1]=s02-s13;
  } else if(size==2) { // Special case
    OXS_FFT_REAL_TYPE *rvec = static_cast<OXS_FFT_REAL_TYPE *>(vec);
    OXS_FFT_REAL_TYPE temp=rvec[0];
    rvec[0]+=rvec[1];
    rvec[1]=temp-rvec[1];
  } else if(size==1) { // NOP
  } else { // size<2
    String msg="Invalid array dimensions in "
      "Oxs_FFT::ForwardDecFreqRealDataNoScale(OXS_FFT_INT_TYPE,void*) const";
    OXS_THROW(Oxs_BadParameter,msg.c_str());
  }
}

void Oxs_FFT::InverseDecTime(OXS_FFT_INT_TYPE size,Oxs_Complex *vec) const
{
  // Do InverseDecTime with default scaling, which is 1.0/size;
  InverseDecTimeNoScale(size,vec);
  for(OXS_FFT_INT_TYPE k=0;k<size;k++) vec[k]*=vecsize_normalization;
}

void Oxs_FFT::InverseDecTimeRealDataNoScale(OXS_FFT_INT_TYPE size,
                                            void *vec) const 
{ // Performs iFFT on a conjugate symmetric complex sequence.  The
  // import size should be the size of then entire sequence, and
  // must be a positive power of two.  It is assumed that vec on
  // entry is a sequence of complex numbers of length size/2, with
  // special packing of vec[0] so that vec[0].im is actually
  // vec[size/2], if the sequence were extended (both vec[0] and
  // vec[size/2] are real, if vec is conj. symm.).  This packing
  // is consistent with the output from
  //    Oxs_FFT::ForwardDecFreq(OXS_FFT_INT_TYPE,void*,OXS_FFT_REAL_TYPE)
  // See the documentation for that function for additional details.
  //   The iFFT calculation is done in-place, and on return the array
  // 'vec' should be interpreted as a real array of length size.
  //   See mjd's NOTES II, 4-Apr-2001, p 105 and NOTES I, 5-Nov-1996,
  // p. 86.

  OXS_FFT_INT_TYPE csize=size/2;
  if(csize!=vecsize) Setup(csize);

  if(size<2) {
    if(size==1) return; // NOP
    String msg="Invalid array dimensions in "
      "Oxs_FFT::InverseDecTimeRealDataNoScale(OXS_FFT_INT_TYPE,void*) const";
    OXS_THROW(Oxs_BadParameter,msg.c_str());
  }

  Oxs_Complex* cvec = static_cast<Oxs_Complex*>(vec);
  OXS_FFT_REAL_TYPE temp=cvec[0].im; // Handle special edge element
  cvec[0].im = (cvec[0].re - temp);  // packing.
  cvec[0].re = (cvec[0].re + temp);

  if(size>8) {
    // General case.
    if(realdata_vecsize!=size) InitializeRealDataTransforms(size);
    OXS_FFT_INT_TYPE k=0;
    while((++k)<csize/2) {
      OXS_FFT_REAL_TYPE resum  = (cvec[k].re + cvec[csize-k].re);
      OXS_FFT_REAL_TYPE rediff = (cvec[k].re - cvec[csize-k].re);
      OXS_FFT_REAL_TYPE imdiff = (cvec[k].im - cvec[csize-k].im);
      OXS_FFT_REAL_TYPE imsum  = (cvec[k].im + cvec[csize-k].im);

      OXS_FFT_REAL_TYPE wx=realdata_winverse[k].re;
      OXS_FFT_REAL_TYPE wy=realdata_winverse[k].im;

      OXS_FFT_REAL_TYPE temp1 = wy*rediff + wx*imsum ;
      OXS_FFT_REAL_TYPE temp2 = wx*rediff - wy*imsum ;

      cvec[k].Set(      resum-temp1,temp2+imdiff);
      cvec[csize-k].Set(resum+temp1,temp2-imdiff);
    }
    cvec[csize/2].re*=2;
    cvec[csize/2].im*=-2;
    InverseDecTimeNoScale(csize,cvec);
  } else if(size==8) {
    OXS_FFT_REAL_TYPE resum  = (cvec[1].re + cvec[3].re);
    OXS_FFT_REAL_TYPE rediff = (cvec[1].re - cvec[3].re);
    OXS_FFT_REAL_TYPE imdiff = (cvec[1].im - cvec[3].im);
    OXS_FFT_REAL_TYPE imsum  = (cvec[1].im + cvec[3].im);
    OXS_FFT_REAL_TYPE temp1 = (rediff + imsum)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE temp2 = (rediff - imsum)*OXS_FFT_SQRT1_2;
    cvec[1].Set(resum-temp1,temp2+imdiff);
    cvec[3].Set(resum+temp1,temp2-imdiff);
    cvec[2].re*=2;
    cvec[2].im*=-2;
    InverseDecTimeNoScale(4,cvec);
  } else if(size==4) {
    OXS_FFT_REAL_TYPE* rvec = static_cast<OXS_FFT_REAL_TYPE*>(vec);
    OXS_FFT_REAL_TYPE temp1=2*rvec[2];
    rvec[2]=rvec[0]-temp1;
    rvec[0]+=temp1;
    OXS_FFT_REAL_TYPE temp2=2*rvec[3];
    rvec[3]=rvec[1]+temp2;
    rvec[1]-=temp2;
  }
  // size==2 involves only edge elements, which are already handled.
}

void Oxs_FFT::InverseDecTimeRealData(OXS_FFT_INT_TYPE size,void *vec) const
{ // Performs iFFT on a conjugate symmetric complex sequence with
  // default scaling, which is 1.0/size.  To specify a different,
  // explicit scaling, use the companion
  // InverseDecTimeRealData(OXS_FFT_INT_TYPE, void*,
  // OXS_FFT_REAL_TYPE) routine.
  InverseDecTimeRealDataNoScale(size,vec);
  OXS_FFT_REAL_TYPE *rvec=static_cast<OXS_FFT_REAL_TYPE *>(vec);
  if(size==2) {
    for(OXS_FFT_INT_TYPE k=0;k<2;k++) rvec[k]*=0.5;
  } else if(size==4) {
    for(OXS_FFT_INT_TYPE k=0;k<4;k++) rvec[k]*=0.25;
  } else if(size>4) {
    OXS_FFT_REAL_TYPE scale=vecsize_normalization*0.5;
    /// vecsize_normalization = 1.0/vecsize, where vecsize is the
    /// length of the computed iFFT, which is size/2.
    for(OXS_FFT_INT_TYPE k=0;k<size;k++) rvec[k]*=scale;
  }
}

inline static void Swap(Oxs_Complex &a,Oxs_Complex &b)
{ Oxs_Complex c(a); a=b; b=c; }

void Oxs_FFT::Permute(Oxs_Complex *vec) const
{ /* Bit reversal permutation */
  OXS_FFT_INT_TYPE i,j;
  for(i=1;i<vecsize-1;i++) {
    // vec[0] is always fixed, so start at 1.
    // vec[vecsize-1] is also always fixed (at least for
    // power-of-two...don't know about others), so stop
    // at vecsize-2
    if((j=permindex[i])!=0) Swap(vec[i],vec[j]);
  }
}

void Oxs_FFT::BaseDecFreqForward(Oxs_Complex *vec) const
{ // In-place forward FFT using Decimation in Frequency technique,
  // *WITHOUT* resuffling of indices.
  // NOTE 1: This code does not use the Complex class, because
  //         some compilers do not effectively optimize around
  //         Complex operations such as multiplication.  So this
  //         routine just makes use of ordinary type "OXS_FFT_REAL_TYPE"
  //         variables, and assumes each Complex variable is
  //         actually two consecutive "OXS_COMPLEX_REAL_TYPE" variables.
  // NOTE 2: See notes in MJD's micromagnetics notebook, 11-Sep-96
  //         (p. 62) and 29-Sep-96 (p. 69).
  // NOTE 3: This code has been optimized for performance on cascade.cam,
  //         a PentiumPro 200 MHz machine using a stock gcc 2.7.2
  //         compiler.  In particular, the x86 chips suffer from a
  //         shortage of registers.
  // NOTE 4: Some compromise made to RISC architectures on 27-May-1997,
  //         by moving all loads before any stores in main loop.  As
  //         done, it hurts performance on PentiumPro-200 only a couple
  //         of percent. (mjd)

  if(vecsize==1) return; // Nothing to do

  OXS_COMPLEX_REAL_TYPE *const dvec=(OXS_COMPLEX_REAL_TYPE *)vec;
  OXS_COMPLEX_REAL_TYPE const *const U=(OXS_COMPLEX_REAL_TYPE *)Uforward;

  // Blocksize>8
  OXS_FFT_INT_TYPE blocksize,blockcount;
  for(blocksize=vecsize,blockcount=1;blocksize>8;
      blocksize/=4,blockcount*=4) {
    // Loop through double-step matrix multiplications
    OXS_FFT_INT_TYPE halfbs=blocksize/2;
    OXS_FFT_INT_TYPE threehalfbs=blocksize+halfbs;
    OXS_FFT_INT_TYPE block,offset,uoff1;
    OXS_COMPLEX_REAL_TYPE *v;
    for(block=0,v=dvec;block<blockcount;block++,v+=2*blocksize) {
      for(offset=0;offset<halfbs;offset+=2) {
	OXS_FFT_REAL_TYPE m1x,m1y,m2x,m2y,m3x,m3y;
	OXS_FFT_REAL_TYPE x0,y0,x1,y1,x2,y2,x3,y3;
	OXS_FFT_REAL_TYPE xs02,ys02,xd02,yd02,xs13,ys13,xd13,yd13;
	OXS_FFT_REAL_TYPE t1x,t1y,t2x,t2y,t3x,t3y;
	uoff1=offset*blockcount;
	m1x=U[uoff1];	m1y=U[uoff1+1];
	m2x=U[2*uoff1];	m2y=U[2*uoff1+1];
	m3x=U[3*uoff1];	m3y=U[3*uoff1+1];

	x0=v[offset];
        y0=v[offset+1];
	x1=v[halfbs+offset];
	y1=v[halfbs+offset+1];
	x2=v[blocksize+offset];
	y2=v[blocksize+offset+1];
	x3=v[threehalfbs+offset];
	y3=v[threehalfbs+offset+1];

	xs02=x0+x2;	xs13=x1+x3;
	v[offset]=xs02+xs13;
	t1x=xs02-xs13;

	ys02=y0+y2;	ys13=y1+y3;
        v[offset+1]=ys02+ys13;
	t1y=ys02-ys13;

	v[halfbs+offset]    =t1x*m2x-t1y*m2y;
	v[halfbs+offset+1]  =t1y*m2x+t1x*m2y;

	xd02=x0-x2;	yd13=y1-y3;
	t3x=xd02-yd13;  t2x=xd02+yd13;
        yd02=y0-y2;	xd13=x1-x3;
	t3y=yd02+xd13;  t2y=yd02-xd13;
	v[blocksize+offset]  =t2x*m1x-t2y*m1y;
	v[blocksize+offset+1]=t2y*m1x+t2x*m1y;
	v[threehalfbs+offset]  =t3x*m3x-t3y*m3y;
	v[threehalfbs+offset+1]=t3y*m3x+t3x*m3y;
      }
    }
  }
  // If vecsize is an odd power of 2, then blocksize at this
  // point will be 8, otherwise 4.  If 8, then do a single
  // power-of-2 block, and leave the rest to the blocksize==4
  // code block.
  if(blocksize==8) {
    blockcount=vecsize/8;
    OXS_FFT_INT_TYPE block;
    OXS_COMPLEX_REAL_TYPE *v;
    for(block=0,v=dvec;block<blockcount;block++,v+=16) {
      // w = 1
      OXS_FFT_REAL_TYPE xa=v[8]; v[8]=v[0]-xa; v[0]+=xa;
      OXS_FFT_REAL_TYPE ya=v[9]; v[9]=v[1]-ya; v[1]+=ya;

      // w = 1/sqrt(2) - i/sqrt(2)
      OXS_FFT_REAL_TYPE xb=v[2]; v[2]+=v[10]; xb-=v[10];
      OXS_FFT_REAL_TYPE yb=v[3]; v[3]+=v[11]; yb-=v[11];
      v[10]=(yb+xb)*OXS_FFT_SQRT1_2;
      v[11]=(yb-xb)*OXS_FFT_SQRT1_2;

      // w = -i
      OXS_FFT_REAL_TYPE xc=v[4]; v[4]+=v[12]; xc-=v[12];
      OXS_FFT_REAL_TYPE yc=v[5]; v[5]+=v[13]; yc-=v[13];
      v[12]=  yc;
      v[13]= -xc;

      // w = -1/sqrt(2) - i/sqrt(2)
      OXS_FFT_REAL_TYPE xd=v[6]; v[6]+=v[14]; xd-=v[14];
      OXS_FFT_REAL_TYPE yd=v[7]; v[7]+=v[15]; yd-=v[15];
      v[14]=(yd-xd)*OXS_FFT_SQRT1_2;
      v[15]=(yd+xd)*(-1*OXS_FFT_SQRT1_2);
    }
    blocksize=4;
  }
  // Do smallest blocks; size is either 4 or 2
  if(blocksize==4) {
    blockcount=vecsize/4;
    OXS_FFT_INT_TYPE block;
    OXS_COMPLEX_REAL_TYPE *v;
    for(block=0,v=dvec;block<blockcount;block++,v+=8) {
      OXS_FFT_REAL_TYPE x0,y0,x1,y1,x2,y2,x3,y3;
      x0=v[0];      y0=v[1];      x1=v[2];      y1=v[3];
      x2=v[4];      y2=v[5];      x3=v[6];      y3=v[7];

      OXS_FFT_REAL_TYPE xs02=x0+x2;
      OXS_FFT_REAL_TYPE xs13=x1+x3; 
      v[0]=xs02+xs13;
      v[2]=xs02-xs13;

      OXS_FFT_REAL_TYPE ys02=y0+y2;
      OXS_FFT_REAL_TYPE ys13=y1+y3;
      v[1]=ys02+ys13;
      v[3]=ys02-ys13;

      OXS_FFT_REAL_TYPE xd02=x0-x2;
      OXS_FFT_REAL_TYPE yd13=y1-y3;
      v[4]=xd02+yd13;
      v[6]=xd02-yd13;

      OXS_FFT_REAL_TYPE yd02=y0-y2;
      OXS_FFT_REAL_TYPE xd13=x1-x3;
      v[5]=yd02-xd13;
      v[7]=yd02+xd13;
    }
  }
  else { // blocksize==2
    // We should only get here if original vecsize==2.
    if(vecsize!=2) {
      String msg="Programming error in "
	"Oxs_FFT::BaseDecFreqForward(Oxs_Complex *) const: "
	"invalid vecsize handling.";
      OXS_THROW(Oxs_ProgramLogicError,msg.c_str());
    }
    OXS_FFT_REAL_TYPE x0=dvec[0];
    OXS_FFT_REAL_TYPE x1=dvec[2];
    OXS_FFT_REAL_TYPE y0=dvec[1];
    OXS_FFT_REAL_TYPE y1=dvec[3];
    dvec[0]=x0+x1;   dvec[2]=x0-x1;
    dvec[1]=y0+y1;   dvec[3]=y0-y1;
  }
}

void Oxs_FFT::BaseDecTimeInverse(Oxs_Complex *vec) const
{ // In-place inverse FFT using Decimation in Time technique,
  // *WITHOUT* resuffling of indices.
  // NOTE 1: This code does not use the Complex class, because
  //         some compilers do not effectively optimize around
  //         Complex operations such as multiplication.  So this
  //         routine just makes use of ordinary type "OXS_FFT_REAL_TYPE"
  //         variables, and assumes each Complex variable is
  //         actually two consecutive "OXS_COMPLEX_REAL_TYPE" variables.
  // NOTE 2: See notes in MJD's micromagnetics notebook, 11-Sep-96
  //         (p. 62) and 29-Sep-96 (p. 69).
  // NOTE 3: This code has been optimized for performance on cascade.cam,
  //         a PentiumPro 200 MHz machine using a stock gcc 2.7.2
  //         compiler.  In particular, the x86 chips suffer from a
  //         shortage of registers.
  // NOTE 4: Some compromise made to RISC architectures on 27-May-1997,
  //         by moving all loads before any stores in main loop.  As
  //         done, it hurts performance on PentiumPro-200 only a couple
  //         of percent. (mjd)
  // NOTE 5: For big vecsize (i.e., too big to fit in cache), it is
  //         important to order array reads and writes as much as
  //         possible to increase cache hits. 6-Apr-2001 (mjd)
  if(vecsize==1) return; // Nothing to do

  OXS_COMPLEX_REAL_TYPE *const dvec=(OXS_COMPLEX_REAL_TYPE *)vec;

  // Do smallest blocks; size is either 4 or 2
  if(vecsize==2) {
    OXS_FFT_REAL_TYPE x0=dvec[0];  OXS_FFT_REAL_TYPE y0=dvec[1];
    OXS_FFT_REAL_TYPE x1=dvec[2];  OXS_FFT_REAL_TYPE y1=dvec[3];
    dvec[0]=x0+x1;
    dvec[2]=x0-x1;
    dvec[1]=y0+y1;
    dvec[3]=y0-y1;
    return;
  }

  // Else vecsize>2
  OXS_FFT_INT_TYPE blocksize=4;
  OXS_FFT_INT_TYPE blockcount=vecsize/4;
  OXS_FFT_INT_TYPE block;
  OXS_COMPLEX_REAL_TYPE *v;
  for(block=0,v=dvec;block<blockcount;++block,v+=8) {
    OXS_FFT_REAL_TYPE x0=v[0];
    OXS_FFT_REAL_TYPE x1=v[2];
    OXS_FFT_REAL_TYPE xs01=x0+x1;
    OXS_FFT_REAL_TYPE xd01=x0-x1;

    OXS_FFT_REAL_TYPE x2=v[4];
    OXS_FFT_REAL_TYPE x3=v[6];
    OXS_FFT_REAL_TYPE xs23=x2+x3;
    OXS_FFT_REAL_TYPE xd23=x2-x3;

    v[0]=xs01+xs23;
    v[4]=xs01-xs23;

    OXS_FFT_REAL_TYPE y2=v[5];
    OXS_FFT_REAL_TYPE y3=v[7];
    OXS_FFT_REAL_TYPE yd23=y2-y3;
    OXS_FFT_REAL_TYPE ys23=y2+y3;
    v[2]=xd01-yd23;
    v[6]=xd01+yd23;

    OXS_FFT_REAL_TYPE y0=v[1];
    OXS_FFT_REAL_TYPE y1=v[3];
    OXS_FFT_REAL_TYPE ys01=y0+y1;
    OXS_FFT_REAL_TYPE yd01=y0-y1;
    v[1]=ys01+ys23;
    v[5]=ys01-ys23;

    v[3]=yd01+xd23;
    v[7]=yd01-xd23;
  }

  if(log2vecsize%2==1) { // vecsize is an odd power-of-2; do a single
    /// block (of size 8) here so we can finish all the rest two blocks
    /// at a time.
    ///   Note also that since vecsize is at least 4, if log2vecsize is
    /// odd, then vecsize must actually be at least 8.    
    blockcount/=2;
    blocksize*=2;   // blocksize==8
    for(block=0,v=dvec;block<blockcount;block++,v+=16) {
      // 1,1 pair
      OXS_FFT_REAL_TYPE xa=v[8];  v[8]=v[0]-xa;  v[0]+=xa;
      OXS_FFT_REAL_TYPE ya=v[9];  v[9]=v[1]-ya;  v[1]+=ya;

      // 1,1/sqrt(2)+i/sqrt(2)) pair
      OXS_FFT_REAL_TYPE xb=v[10];
      OXS_FFT_REAL_TYPE yb=v[11];
      OXS_FFT_REAL_TYPE sum_xyb=(xb+yb)*OXS_FFT_SQRT1_2;
      OXS_FFT_REAL_TYPE dif_xyb=(xb-yb)*OXS_FFT_SQRT1_2;
      v[10]=v[2]-dif_xyb;  v[2]+=dif_xyb;
      v[11]=v[3]-sum_xyb;  v[3]+=sum_xyb;

      // 1,i pair
      OXS_FFT_REAL_TYPE xc=v[12];
      OXS_FFT_REAL_TYPE yc=v[13];
      v[12]=v[4]+yc;  v[4]-=yc;
      v[13]=v[5]-xc;  v[5]+=xc;

      // 1,-1/sqrt(2)+i/sqrt(2)) pair
      OXS_FFT_REAL_TYPE xd=v[14];
      OXS_FFT_REAL_TYPE yd=v[15];
      OXS_FFT_REAL_TYPE sum_xyd=(xd+yd)*OXS_FFT_SQRT1_2;
      OXS_FFT_REAL_TYPE dif_xyd=(xd-yd)*OXS_FFT_SQRT1_2;
      v[14]=v[6]+sum_xyd;  v[6]-=sum_xyd;
      v[15]=v[7]-dif_xyd;  v[7]+=dif_xyd;
    }
  }

  // Blocksize>4
  blocksize*=4;
  blockcount/=4;
  OXS_COMPLEX_REAL_TYPE const *const U=(OXS_COMPLEX_REAL_TYPE *)Uinverse;
  for(;blockcount>0;blocksize*=4,blockcount/=4) {
    // Loop through double-step matrix multiplications
    OXS_FFT_INT_TYPE halfbs=blocksize/2;  // Half blocksize
    OXS_FFT_INT_TYPE threehalfbs=blocksize+halfbs; // 3/2 blocksize
    OXS_FFT_INT_TYPE offset,uoff1;
    for(block=0,v=dvec;block<blockcount;block++,v+=2*blocksize) {
      for(offset=0,uoff1=0;offset<halfbs;offset+=2,uoff1+=2*blockcount) {
	OXS_FFT_REAL_TYPE m1x,m1y,m2x,m2y,m3x,m3y;
	OXS_FFT_REAL_TYPE x0,y0,x1,y1,x2,y2,x3,y3;
	OXS_FFT_REAL_TYPE xs01,ys01,xd01,yd01,xs23,ys23,xd23,yd23;
	OXS_FFT_REAL_TYPE t1x,t1y,t2x,t2y,t3x,t3y;

	x0=v[offset];
	y0=v[offset+1];

	m1x=U[uoff1];
	m1y=U[uoff1+1];

	t1x=v[halfbs+offset];
	t1y=v[halfbs+offset+1];

	m2x=U[2*uoff1];
	m2y=U[2*uoff1+1];

	t2x=v[blocksize+offset];
	t2y=v[blocksize+offset+1];

	m3x=U[3*uoff1];
	m3y=U[3*uoff1+1];

	t3x=v[threehalfbs+offset];
	t3y=v[threehalfbs+offset+1];

	x1=t1x*m2x-t1y*m2y;
	xs01=x0+x1;
	xd01=x0-x1;

	x2=t2x*m1x-t2y*m1y;
	x3=t3x*m3x-t3y*m3y;
	xs23=x2+x3;
	xd23=x2-x3;

	y1=t1y*m2x+t1x*m2y;
	ys01=y0+y1;
	yd01=y0-y1;

	y2=t2y*m1x+t2x*m1y;
	y3=t3y*m3x+t3x*m3y;
	ys23=y2+y3;
	yd23=y2-y3;

	v[offset]               = xs01+xs23;
	v[offset+1]             = ys01+ys23;
	v[offset+halfbs]        = xd01-yd23;
	v[offset+halfbs+1]      = yd01+xd23;
	v[offset+blocksize]     = xs01-xs23;
	v[offset+blocksize+1]   = ys01-ys23;
	v[offset+threehalfbs]   = xd01+yd23;
	v[offset+threehalfbs+1] = yd01-xd23;
      }
    }
  }

#ifdef OLDE_CODE // The earlier check on parity of log2vecsize
  /// ensures that this block is never needed.
  if(blocksize==2*vecsize) {
    // Do one single-step matrix multiplication
    blocksize=vecsize;  v=dvec;
    for(offset=0;offset<blocksize;offset+=2,v+=2) {
      x0=v[0];              y0=v[1];
      t1x=v[vecsize];       t1y=v[vecsize+1];
      m1x=U[offset];        m1y=U[offset+1];
      x1=t1x*m1x-t1y*m1y;   y1=t1y*m1x+t1x*m1y;
      v[0]         = x0+x1; 
      v[vecsize]   = x0-x1;
      v[1]         = y0+y1;
      v[vecsize+1] = y0-y1;
    }
  }
#endif // OLDE_CODE

}

void Oxs_FFT::UnpackRealPairImage(OXS_FFT_INT_TYPE size,
                                  Oxs_Complex *cvec) const
{ // Given complex sequence cvec[] which is the FFT image of
  // f[]+i.g[], where f[] and g[] are real sequences, unpacks
  // in-place to yield half of each of the conjugate symmetric
  // sequences F[] and G[], which are the FFT image of f[] and
  // g[], respectively.  The layout is:
  //
  //      F[0] = cvec[0].re, F[1] = cvec[1], F[2] = cvec[2],
  //                          ... F[size/2] = cvec[size/2].re
  // and
  //      G[0] = cvec[0].im, G[size-1] = cvec[size-1],
  //                         G[size-2] = cvec[size-2],
  //          ...            G[(size/2)+1] = cvec[(size/2)+1]
  //                         G[size/2] = cvec[size/2].im
  //
  // Note that F[0], F[size/2], G[0] and G[size/2] are all real.
  // Also, because of cong. symmetry, for 0<k<size/2 F[size-k]
  // can be computed as F[size-k] = conj(F[k]), and similarly
  // G[k] = conj(G[size-k]).

  // Refer to mjd NOTES I, 4-Nov-1996, p84.
  for(OXS_FFT_INT_TYPE k=1;k<size/2;k++) {
    OXS_FFT_INT_TYPE negk=size-k;
    double x1=cvec[k].re;
    double y1=cvec[k].im;
    double x2=cvec[negk].re;
    double y2=cvec[negk].im;

    cvec[k].re = (x1+x2)/2;
    cvec[k].im = (y1-y2)/2;
    cvec[negk].re = (y1+y2)/2;
    cvec[negk].im = (x1-x2)/2;
  }
  // Note that cvec[0] and cvec[size/2] are correct on entry.
}

void Oxs_FFT::RepackRealPairImage(OXS_FFT_INT_TYPE size,
                                  Oxs_Complex *cvec) const
{ // Reverses the operation of
  // UnpackRealPairImage(OXS_FFT_INT_TYPE,Oxs_Complex).
  for(OXS_FFT_INT_TYPE k=1;k<size/2;k++) {
    OXS_FFT_INT_TYPE negk=size-k;
    double fx=cvec[k].re;
    double fy=cvec[k].im;
    double gx=cvec[negk].re;
    double gy=cvec[negk].im;

    cvec[k].re = fx+gy;
    cvec[k].im = fy+gx;
    cvec[negk].re = fx-gy;
    cvec[negk].im = gx-fy;
  }
}

////////////////////////////////////////////////////////////////////////
// 3D FFT

#define SCRATCH_BLOCK_SIZE 8 // Number of lines to block
#define SCRATCH_BLOCK_PAD 1  // Extra padding per block line

Oxs_FFT3D::Oxs_FFT3D() :
  scratchsize(0), scratch(NULL), scratch_stride(0),
  scratch_index(0), scratch_top_index(0),
  base_ptr(NULL), base_stride(0),
  base_block_index(0), base_top_index(0),
  buffer_input_size(0),buffer_output_size(0),buffer_full_size(0)
{}

void Oxs_FFT3D::ReleaseMemory() const
{
  if(scratchsize>0 && scratch!=NULL) delete[] scratch;
  scratchsize=0;
  scratch=NULL;
  scratch_stride=scratch_index=scratch_top_index=0;

  base_ptr=NULL;
  base_stride=base_block_index=base_top_index=0;
  buffer_input_size=buffer_output_size=buffer_full_size=0;

  xfft.ReleaseMemory();
  yfft.ReleaseMemory();
  zfft.ReleaseMemory();
}

Oxs_FFT3D::~Oxs_FFT3D()
{
  ReleaseMemory();
}

void Oxs_FFT3D::AdjustScratchSize(OC_INDEX xsize,OC_INDEX ysize,
				  OC_INDEX zsize) const
{
  OC_INDEX maxsize=xsize;
  if(ysize>maxsize) maxsize=ysize;
  if(zsize>maxsize) maxsize=zsize;
  maxsize+=SCRATCH_BLOCK_PAD;  // Add padding to avoid cache line conflicts
  maxsize*=SCRATCH_BLOCK_SIZE;
  if(scratchsize<maxsize) {
    if(scratchsize>0 && scratch!=NULL) delete[] scratch;
    scratchsize=maxsize;
    scratch=new Oxs_Complex[scratchsize];
  }
}

Oxs_Complex*
Oxs_FFT3D::SetupBlocking
(Oxs_Complex* base,OC_INDEX insize,OC_INDEX outsize,OC_INDEX fullsize,
 OC_INDEX basestride,OC_INDEX baserows) const
{
  if(fullsize<insize) {
    String msg="Full size must be >= input size; "
      "Error in Oxs_FFT3D::SetupBlocking()";
    OXS_THROW(Oxs_BadParameter,msg.c_str());
  }
  if(fullsize<outsize) {
    String msg="Full size must be >= output size; "
      "Error in Oxs_FFT3D::SetupBlocking()";
    OXS_THROW(Oxs_BadParameter,msg.c_str());
  }

  base_ptr=base;
  base_stride=basestride;
  base_block_index=0;
  base_top_index=baserows;
  buffer_input_size=insize;
  buffer_output_size=outsize;
  buffer_full_size=fullsize;

  scratch_stride=buffer_full_size
    + SCRATCH_BLOCK_PAD; // Add padding to avoid cache line conflicts
  if(scratch_stride*SCRATCH_BLOCK_SIZE>scratchsize) {
    String msg="Array sized error in Oxs_FFT3D::SetupBlocking(): "
      "Scratch buffer array too small.";
    OXS_THROW(Oxs_BadIndex,msg.c_str());
  }

  scratch_index=scratch_top_index=0;
  return FillBlockBuffer(0);
}

Oxs_Complex* Oxs_FFT3D::FillBlockBuffer(OC_INDEX block_index) const
{ // Fills scratch area from base+block_index
  base_block_index=block_index;
  scratch_index=0;
  scratch_top_index=SCRATCH_BLOCK_SIZE;
  if(block_index+scratch_top_index>base_top_index) {
    scratch_top_index=base_top_index-block_index;
    if(scratch_top_index<1) return NULL; // No more data
  }
  Oxs_Complex* bptr=base_ptr+block_index;
  Oxs_Complex* sptr;
  OC_INDEX i,j;
  for(j=0;j<buffer_input_size;j++) {
    sptr=scratch+j;
    for(i=0;i<scratch_top_index;i++) {
      *sptr = bptr[i];
      sptr+=scratch_stride;
    }
    bptr+=base_stride;
  }
  sptr=scratch;
  for(i=0;i<scratch_top_index;i++) { // Zero fill
    for(j=buffer_input_size;j<buffer_full_size;j++) {
      sptr[j]=Oxs_Complex(0.,0.);
    }
    sptr+=scratch_stride;
  }
  return scratch;
}

void Oxs_FFT3D::FlushBlockBuffer() const
{
  Oxs_Complex* bptr=base_ptr+base_block_index;
  Oxs_Complex* sptr;
  OC_INDEX i,j;
  for(j=0;j<buffer_output_size;j++) {
    sptr=scratch+j;
    for(i=0;i<scratch_top_index;i++) {
      bptr[i] = *sptr ;
      sptr+=scratch_stride;
    }
    bptr+=base_stride;
  }
}

Oxs_Complex*
Oxs_FFT3D::GetNextBlockRun() const
{
  // Already loaded?
  ++scratch_index;
  if(scratch_index<scratch_top_index) {
    return scratch+scratch_index*scratch_stride;
  }
  // Otherwise, flush current block and load next block
  FlushBlockBuffer();
  return FillBlockBuffer(base_block_index+scratch_top_index);
}


void
Oxs_FFT3D::Forward(OC_INDEX xsize,OC_INDEX ysize,OC_INDEX zsize,
		   Oxs_Complex* arr) const
{ // Conceptually const
  OC_INDEX totalsize=xsize*ysize*zsize;
  if(xsize==0 || ysize==0 || zsize==0) return; // Nothing to do
  if(totalsize < xsize || totalsize < ysize || totalsize < zsize
     || (totalsize/zsize)/ysize != xsize) {
    // Overflow check
    String msg="OC_INDEX overflow in Oxs_FFT3D::Forward(): "
      "Product xsize*ysize*zsize too big to fit in a OC_INDEX variable";
    OXS_THROW(Oxs_Overflow,msg.c_str());
  }

  AdjustScratchSize(xsize,ysize,zsize);

  OC_INDEX i,j,k,block,block_stop;
  OC_INDEX offset;
  OC_INDEX xysize = xsize*ysize;
  Oxs_Complex* aptr;

  // Do 1D FFT's in x-direction
  if(xsize>1) {
    aptr = arr;
    for(k=0;k<zsize;k++) for(j=0;j<ysize;j++) {
      xfft.ForwardDecFreq(xsize,aptr);
      aptr += xsize;
    }
  }
  
  // Do 1D FFT's in y-direction
  if(ysize>1) {
    for(k=0;k<zsize;k++) {
      aptr = arr + k*xysize;  // Set base z-position
      for(i=0;i<xsize;i+=SCRATCH_BLOCK_SIZE) {
	if(i+SCRATCH_BLOCK_SIZE<=xsize) block_stop=SCRATCH_BLOCK_SIZE;
	else                            block_stop=xsize-i;
	offset=0;
	for(j=0;j<ysize;j++) {
	  OXS_FFT_INT_TYPE j2;
	  for(block=0,j2=j;block<block_stop;block++,j2+=ysize) {
	    // Copy from arr into scratch buffer
	    scratch[j2] = aptr[offset+block];
	  }
	  offset+=xsize;
	}
	for(block=0;block<block_stop;block++) {
	  // Do FFT in scratch buffer
	  yfft.ForwardDecFreq(ysize,scratch+block*ysize);
	}
	offset=0;
	for(j=0;j<ysize;j++) {
	  OXS_FFT_INT_TYPE j2;
	  for(block=0,j2=j;block<block_stop;block++,j2+=ysize) {
	    // Copy from arr into scratch buffer
	    aptr[offset+block] = scratch[j2];
	  }
	  offset+=xsize;
	}
	aptr+=block_stop; // Increment base x-position
      }
    }
  }

  // Do 1D FFT's in z-direction
  if(zsize>1) {
    aptr=arr;
    for(j=0;j<ysize;j++) {
      for(i=0;i<xsize;i+=SCRATCH_BLOCK_SIZE) {
	if(i+SCRATCH_BLOCK_SIZE<=xsize) block_stop=SCRATCH_BLOCK_SIZE;
	else                            block_stop=xsize-i;
	offset=0;
	for(k=0;k<zsize;k++) {
	  OXS_FFT_INT_TYPE k2;
	  for(block=0,k2=k;block<block_stop;block++,k2+=zsize) {
	    // Copy from arr into scratch buffer
	    scratch[k2] = aptr[offset+block];
	  }
	  offset+=xysize;
	}
	for(block=0;block<block_stop;block++) {
	  // Do FFT in scratch buffer
	  zfft.ForwardDecFreq(zsize,scratch+block*zsize);
	}
	offset=0;
	for(k=0;k<zsize;k++) {
	  OXS_FFT_INT_TYPE k2;
	  for(block=0,k2=k;block<block_stop;block++,k2+=zsize) {
	    // Copy from arr into scratch buffer
	    aptr[offset+block] = scratch[k2];
	  }
	  offset+=xysize;
	}
	aptr+=block_stop; // Increment base position
      }
    }
  }

}

void
Oxs_FFT3D::Inverse(OC_INDEX xsize,OC_INDEX ysize,OC_INDEX zsize,
		   Oxs_Complex* arr) const
{ // Conceptually const
  OC_INDEX totalsize=xsize*ysize*zsize;
  if(xsize==0 || ysize==0 || zsize==0) return; // Nothing to do
  if(totalsize < xsize || totalsize < ysize || totalsize < zsize
     || (totalsize/zsize)/ysize != xsize) {
    // Overflow check
    String msg="OC_INDEX overflow in Oxs_FFT3D::Inverse(): "
      "Product xsize*ysize*zsize too big to fit in a OC_INDEX variable";
    OXS_THROW(Oxs_Overflow,msg.c_str());
  }

  AdjustScratchSize(xsize,ysize,zsize);

  OC_INDEX i,j,k,block,block_stop;
  OC_INDEX offset;
  OC_INDEX xysize = xsize*ysize;
  Oxs_Complex* aptr;

  // Do 1D iFFT's in z-direction
  if(zsize>1) {
    aptr=arr;
    for(j=0;j<ysize;j++) {
      for(i=0;i<xsize;i+=SCRATCH_BLOCK_SIZE) {
	if(i+SCRATCH_BLOCK_SIZE<=xsize) block_stop=SCRATCH_BLOCK_SIZE;
	else                            block_stop=xsize-i;
	offset=0;
	for(k=0;k<zsize;k++) {
	  OXS_FFT_INT_TYPE k2;
	  for(block=0,k2=k;block<block_stop;block++,k2+=zsize) {
	    // Copy from arr into scratch buffer
	    scratch[k2] = aptr[offset+block];
	  }
	  offset+=xysize;
	}
	for(block=0;block<block_stop;block++) {
	  // Do iFFT in scratch buffer
	  zfft.InverseDecTimeNoScale(zsize,scratch+block*zsize);
	}
	offset=0;
	for(k=0;k<zsize;k++) {
	  OXS_FFT_INT_TYPE k2;
	  for(block=0,k2=k;block<block_stop;block++,k2+=zsize) {
	    // Copy from arr into scratch buffer
	    aptr[offset+block] = scratch[k2];
	  }
	  offset+=xysize;
	}
	aptr+=block_stop; // Increment base position
      }
    }
  }
  
  // Do 1D iFFT's in y-direction
  if(ysize>1) {
    for(k=0;k<zsize;k++) {
      aptr = arr + k*xysize;  // Set base z-position
      for(i=0;i<xsize;i+=SCRATCH_BLOCK_SIZE) {
	if(i+SCRATCH_BLOCK_SIZE<=xsize) block_stop=SCRATCH_BLOCK_SIZE;
	else                            block_stop=xsize-i;
	offset=0;
	for(j=0;j<ysize;j++) {
	  OXS_FFT_INT_TYPE j2;
	  for(block=0,j2=j;block<block_stop;block++,j2+=ysize) {
	    // Copy from arr into scratch buffer
	    scratch[j2] = aptr[offset+block];
	  }
	  offset+=xsize;
	}
	for(block=0;block<block_stop;block++) {
	  // Do FFT in scratch buffer
	  yfft.InverseDecTimeNoScale(ysize,scratch+block*ysize);
	}
	offset=0;
	for(j=0;j<ysize;j++) {
	  OXS_FFT_INT_TYPE j2;
	  for(block=0,j2=j;block<block_stop;block++,j2+=ysize) {
	    // Copy from arr into scratch buffer
	    aptr[offset+block] = scratch[j2];
	  }
	  offset+=xsize;
	}
	aptr+=block_stop; // Increment base x-position
      }
    }
  }

  // Do 1D iFFT's in x-direction
  if(xsize>1) {
    aptr = arr;
    for(k=0;k<zsize;k++) for(j=0;j<ysize;j++) {
      xfft.InverseDecTimeNoScale(xsize,aptr);
      aptr += xsize;
    }
  }

  // Scale
  OXS_FFT_REAL_TYPE scale = 1./static_cast<OXS_FFT_REAL_TYPE>(totalsize);
  for(i=0;i<totalsize;i++) arr[i] *= scale;

}

void
Oxs_FFT3D::ForwardRealDataScrambledNoScale
(void* arr,
 OC_INDEX rsize1_in,OC_INDEX rsize2_in,OC_INDEX rsize3_in,
 OC_INDEX csize1_out,OC_INDEX csize2_out,OC_INDEX csize3_out,
 OC_INDEX cstride2,OC_INDEX cstride3) const
{ // Conceptually const
  if(rsize1_in>2*csize1_out || rsize2_in>csize2_out
     || rsize3_in>csize3_out
     || csize1_out>cstride2 || csize2_out*cstride2>cstride3) {
    String msg="Invalid input to "
      "Oxs_FFT3D::ForwardRealDataScrambledNoScale()";
    OXS_THROW(Oxs_BadParameter,msg.c_str());
  }

  if(csize1_out==0 || csize2_out==0 || csize3_out==0)
    return; // Nothing to do

  OC_INDEX ctotalsize=cstride3*csize3_out;
  /// Total length when interpreted as a Oxs_Complex array.

  if(ctotalsize<csize1_out || ctotalsize<csize2_out
     || ctotalsize<csize3_out
     || (ctotalsize/csize3_out)/csize2_out < csize1_out) {
    // Overflow check
    String msg="OC_INDEX overflow in "
      "Oxs_FFT3D::ForwardRealDataScrambledNoScale(): "
      "Total size too big to fit in a OC_INDEX variable";
    OXS_THROW(Oxs_Overflow,msg.c_str());
  }

  AdjustScratchSize(0,csize2_out,csize3_out); // dim1 transform computed
  /// in-place.

  OC_INDEX i,j,k;

  // Do 1D FFT's in x-direction
  OXS_COMPLEX_REAL_TYPE *rptr;
  OXS_COMPLEX_REAL_TYPE * const rarr
    = static_cast<OXS_COMPLEX_REAL_TYPE *>(arr);
  for(k=0;k<rsize3_in;k++) {
    rptr = rarr + k*cstride3*2; // 1 complex == 2 reals
    for(j=0;j<rsize2_in;j++) {
      for(i=rsize1_in;i<2*csize1_out;i++) rptr[i]=0.;
      xfft.ForwardDecFreqRealDataNoScale(2*csize1_out,rptr);
      rptr += 2*cstride2;
    }
  }

  // Do 1D FFT's in y-direction
  Oxs_Complex *cptr;
  Oxs_Complex * const carr = static_cast<Oxs_Complex *>(arr);
  if(csize2_out>1) {
    for(k=0;k<rsize3_in;k++) {
      cptr = SetupBlocking(carr + k*cstride3,
			   rsize2_in,csize2_out,csize2_out,
			   cstride2,csize1_out);
      // The i==0 plane requires special unpacking.  At this
      // point each entry holds the 0-th and csize1/2-th components
      // of the corresponding dim1 transform packed as the real and
      // imag. parts, respectively.
      // Do FFT, with bit reversal, in scratch buffer
      yfft.ForwardDecFreqNoScale(csize2_out,cptr);
      // 3) Unpack
      yfft.UnpackRealPairImage(csize2_out,cptr);
      
      // Do the remaining planes
      for(i=1;i<csize1_out;i++) {
	cptr = GetNextBlockRun();
	yfft.ForwardDecFreqScrambledNoScale(csize2_out,cptr);
      }

      FlushBlockBuffer();
    }
  }

  // Do 1D FFT's in z-direction
  // The (0,0,k) and (0,csize2_out/2,k) lines requires special
  // unpacking.
  if(csize3_out>1) {
    cptr = SetupBlocking(carr,
			 rsize3_in,csize3_out,csize3_out,
			 cstride3,csize1_out);
    // FFT for i=j=0;
    zfft.ForwardDecFreqNoScale(csize3_out,cptr);
    zfft.UnpackRealPairImage(csize3_out,cptr);

    // FFT for j=0, i>0
    for(i=1;i<csize1_out;i++) {
      cptr = GetNextBlockRun();
      zfft.ForwardDecFreqScrambledNoScale(csize3_out,cptr);
    }
    FlushBlockBuffer();

    if(csize2_out>1) {
      // FFT for 0<j<csize2_out/2
      for(j=1;j<csize2_out/2;j++) {
	cptr = SetupBlocking(carr+j*cstride2,
			     rsize3_in,csize3_out,csize3_out,
			     cstride3,csize1_out);
	zfft.ForwardDecFreqScrambledNoScale(csize3_out,cptr);
	for(i=1;i<csize1_out;i++) {
	  cptr = GetNextBlockRun();
	  zfft.ForwardDecFreqScrambledNoScale(csize3_out,cptr);
	}
	FlushBlockBuffer();
      }

      // FFT for j=csize2_out/2, i=0
      cptr = SetupBlocking(carr+csize2_out*cstride2/2,
			   rsize3_in,csize3_out,csize3_out,
			   cstride3,csize1_out);
      zfft.ForwardDecFreqNoScale(csize3_out,cptr);
      zfft.UnpackRealPairImage(csize3_out,cptr);

      // FFT for j=csize2_out/2, i>0
      for(i=1;i<csize1_out;i++) {
	cptr = GetNextBlockRun();
	zfft.ForwardDecFreqScrambledNoScale(csize3_out,cptr);
      }
      FlushBlockBuffer();

      // FFT for j>csize2_out/2
      for(j=csize2_out/2+1;j<csize2_out;j++) {
	cptr = SetupBlocking(carr+j*cstride2,
			     rsize3_in,csize3_out,csize3_out,
			     cstride3,csize1_out);
	zfft.ForwardDecFreqScrambledNoScale(csize3_out,cptr);
	for(i=1;i<csize1_out;i++) {
	  cptr = GetNextBlockRun();
	  zfft.ForwardDecFreqScrambledNoScale(csize3_out,cptr);
	}
	FlushBlockBuffer();
      }
    }
  }
}

void
Oxs_FFT3D::ForwardRealDataNoScale
(void* arr,
 OC_INDEX rsize1_in,OC_INDEX rsize2_in,OC_INDEX rsize3_in,
 OC_INDEX csize1_out,OC_INDEX csize2_out,OC_INDEX csize3_out,
 OC_INDEX cstride2,OC_INDEX cstride3) const
{ // Conceptually const
  if(rsize1_in>2*csize1_out || rsize2_in>csize2_out
     || rsize3_in>csize3_out
     || csize1_out>cstride2 || csize2_out*cstride2>cstride3) {
    String msg="Invalid input to "
      "Oxs_FFT3D::ForwardRealDataNoScale()";
    OXS_THROW(Oxs_BadParameter,msg.c_str());
  }

  if(csize1_out==0 || csize2_out==0 || csize3_out==0)
    return; // Nothing to do

  OC_INDEX ctotalsize=cstride3*csize3_out;
  /// Total length when interpreted as a Oxs_Complex array.

  if(ctotalsize<csize1_out || ctotalsize<csize2_out
     || ctotalsize<csize3_out
     || (ctotalsize/csize3_out)/csize2_out < csize1_out) {
    // Overflow check
    String msg="OC_INDEX overflow in "
      "Oxs_FFT3D::ForwardRealDataNoScale(): "
      "Total size too big to fit in a OC_INDEX variable";
    OXS_THROW(Oxs_Overflow,msg.c_str());
  }

  AdjustScratchSize(0,csize2_out,csize3_out); // dim1 transform computed
  /// in-place.

  OC_INDEX i,j,k;

  // Do 1D FFT's in x-direction
  OXS_COMPLEX_REAL_TYPE *rptr;
  OXS_COMPLEX_REAL_TYPE * const rarr
    = static_cast<OXS_COMPLEX_REAL_TYPE *>(arr);
  for(k=0;k<rsize3_in;k++) {
    rptr = rarr + k*cstride3*2; // 1 complex == 2 reals
    for(j=0;j<rsize2_in;j++) {
      for(i=rsize1_in;i<2*csize1_out;i++) rptr[i]=0.;
      xfft.ForwardDecFreqRealDataNoScale(2*csize1_out,rptr);
      rptr += 2*cstride2;
    }
  }

  // Do 1D FFT's in y-direction
  Oxs_Complex *cptr;
  Oxs_Complex * const carr = static_cast<Oxs_Complex *>(arr);
  if(csize2_out>1) {
    for(k=0;k<rsize3_in;k++) {
      cptr = SetupBlocking(carr + k*cstride3,
			   rsize2_in,csize2_out,csize2_out,
			   cstride2,csize1_out);
      // The i==0 plane requires special unpacking.  At this
      // point each entry holds the 0-th and csize1/2-th components
      // of the corresponding dim1 transform packed as the real and
      // imag. parts, respectively.
      // Do FFT, with bit reversal, in scratch buffer
      yfft.ForwardDecFreqNoScale(csize2_out,cptr);
      // 3) Unpack
      yfft.UnpackRealPairImage(csize2_out,cptr);
      
      // Do the remaining planes
      for(i=1;i<csize1_out;i++) {
	cptr = GetNextBlockRun();
	yfft.ForwardDecFreqNoScale(csize2_out,cptr);
      }

      FlushBlockBuffer();
    }
  }

  // Do 1D FFT's in z-direction
  // The (0,0,k) and (0,csize2_out/2,k) lines requires special
  // unpacking.
  if(csize3_out>1) {
    cptr = SetupBlocking(carr,
			 rsize3_in,csize3_out,csize3_out,
			 cstride3,csize1_out);
    // FFT for i=j=0;
    zfft.ForwardDecFreqNoScale(csize3_out,cptr);
    zfft.UnpackRealPairImage(csize3_out,cptr);

    // FFT for j=0, i>0
    for(i=1;i<csize1_out;i++) {
      cptr = GetNextBlockRun();
      zfft.ForwardDecFreqNoScale(csize3_out,cptr);
    }
    FlushBlockBuffer();

    if(csize2_out>1) {
      // FFT for 0<j<csize2_out/2
      for(j=1;j<csize2_out/2;j++) {
	cptr = SetupBlocking(carr+j*cstride2,
			     rsize3_in,csize3_out,csize3_out,
			     cstride3,csize1_out);
	zfft.ForwardDecFreqNoScale(csize3_out,cptr);
	for(i=1;i<csize1_out;i++) {
	  cptr = GetNextBlockRun();
	  zfft.ForwardDecFreqNoScale(csize3_out,cptr);
	}
	FlushBlockBuffer();
      }

      // FFT for j=csize2_out/2, i=0
      cptr = SetupBlocking(carr+csize2_out*cstride2/2,
			   rsize3_in,csize3_out,csize3_out,
			   cstride3,csize1_out);
      zfft.ForwardDecFreqNoScale(csize3_out,cptr);
      zfft.UnpackRealPairImage(csize3_out,cptr);

      // FFT for j=csize2_out/2, i>0
      for(i=1;i<csize1_out;i++) {
	cptr = GetNextBlockRun();
	zfft.ForwardDecFreqNoScale(csize3_out,cptr);
      }
      FlushBlockBuffer();

      // FFT for j>csize2_out/2
      for(j=csize2_out/2+1;j<csize2_out;j++) {
	cptr = SetupBlocking(carr+j*cstride2,
			     rsize3_in,csize3_out,csize3_out,
			     cstride3,csize1_out);
	zfft.ForwardDecFreqNoScale(csize3_out,cptr);
	for(i=1;i<csize1_out;i++) {
	  cptr = GetNextBlockRun();
	  zfft.ForwardDecFreqNoScale(csize3_out,cptr);
	}
	FlushBlockBuffer();
      }
    }
  }
}

void
Oxs_FFT3D::InverseRealDataScrambledNoScale
(void* arr,
 OC_INDEX rsize1_out,OC_INDEX rsize2_out,OC_INDEX rsize3_out,
 OC_INDEX csize1_in,OC_INDEX csize2_in,OC_INDEX csize3_in,
 OC_INDEX cstride2,OC_INDEX cstride3) const
{ // Conceptually const
  if(rsize1_out>2*csize1_in || rsize2_out>csize2_in
     || rsize3_out>csize3_in
     || csize1_in>cstride2 || csize2_in*cstride2>cstride3) {
    String msg="Invalid input to "
      "Oxs_FFT3D::InverseRealDataScrambledNoScale()";
    OXS_THROW(Oxs_BadParameter,msg.c_str());
  }

  if(csize1_in==0 || csize2_in==0 || csize3_in==0)
    return; // Nothing to do

  OC_INDEX ctotalsize=cstride3*csize3_in;
  /// Total length when interpreted as a Oxs_Complex array.

  if(ctotalsize<csize1_in || ctotalsize<csize2_in
     || ctotalsize<csize3_in
     || (ctotalsize/csize3_in)/csize2_in < csize1_in) {
    // Overflow check
    String msg="OC_INDEX overflow in "
      "Oxs_FFT3D::InverseRealDataScrambledNoScale(): "
      "Total size too big to fit in a OC_INDEX variable";
    OXS_THROW(Oxs_Overflow,msg.c_str());
  }

  AdjustScratchSize(0,csize2_in,csize3_in); // dim1 transform computed
  /// in-place.

  OC_INDEX i,j,k;

  Oxs_Complex *cptr;
  Oxs_Complex * const carr = static_cast<Oxs_Complex *>(arr);

  // Do 1D iFFT's in z-direction
  // The (0,0,k) and (0,csize2_in/2,k) lines requires special
  // packing.

  if(csize3_in>1) {
    // iFFT for i=j=0;
    cptr = SetupBlocking(carr,csize3_in,rsize3_out,csize3_in,
			 cstride3,csize1_in);
    zfft.RepackRealPairImage(csize3_in,cptr);
    zfft.InverseDecTimeNoScale(csize3_in,cptr);

    // iFFT for j=0, i>0
    for(i=1;i<csize1_in;i++) {
      cptr = GetNextBlockRun();
      zfft.InverseDecTimeScrambledNoScale(csize3_in,cptr);
    }
    FlushBlockBuffer();

    if(csize2_in>1) {
      // iFFT for 0<j<csize2_in/2
      for(j=1;j<csize2_in/2;j++) {
	cptr = SetupBlocking(carr+j*cstride2,
			     csize3_in,rsize3_out,csize3_in,
			     cstride3,csize1_in);
	zfft.InverseDecTimeScrambledNoScale(csize3_in,cptr);
	for(i=1;i<csize1_in;i++) {
	  cptr = GetNextBlockRun();
	  zfft.InverseDecTimeScrambledNoScale(csize3_in,cptr);
	}
	FlushBlockBuffer();
      }

      // iFFT for j=csize2_in/2, i=0
      cptr = SetupBlocking(carr+csize2_in*cstride2/2,
			   csize3_in,rsize3_out,csize3_in,
			   cstride3,csize1_in);
      zfft.RepackRealPairImage(csize3_in,cptr);
      zfft.InverseDecTimeNoScale(csize3_in,cptr);

      // iFFT for j=csize2_in/2, i>0
      for(i=1;i<csize1_in;i++) {
	cptr = GetNextBlockRun();
	zfft.InverseDecTimeScrambledNoScale(csize3_in,cptr);
      }
      FlushBlockBuffer();

      // iFFT for j>csize2_in/2
      for(j=csize2_in/2+1;j<csize2_in;j++) {
	cptr = SetupBlocking(carr+j*cstride2,
			     csize3_in,rsize3_out,csize3_in,
			     cstride3,csize1_in);
	zfft.InverseDecTimeScrambledNoScale(csize3_in,cptr);
	for(i=1;i<csize1_in;i++) {
	  cptr = GetNextBlockRun();
	  zfft.InverseDecTimeScrambledNoScale(csize3_in,cptr);
	}
	FlushBlockBuffer();
      }
    }
  }

  // Do 1D iFFT's in y-direction
  if(csize2_in>1) {
    for(k=0;k<rsize3_out;k++) {
      cptr = SetupBlocking(carr+k*cstride3,
			   csize2_in,rsize2_out,csize2_in,
			   cstride2,csize1_in);

      // The i==0 plane requires special repacking.
      yfft.RepackRealPairImage(csize2_in,cptr);
      yfft.InverseDecTimeNoScale(csize2_in,cptr);

      for(i=1;i<csize1_in;i++) { // Do the remaining lines at this k
	cptr = GetNextBlockRun();
	yfft.InverseDecTimeScrambledNoScale(csize2_in,cptr);
      }
      FlushBlockBuffer();
    }
  }

  // Do 1D iFFT's in x-direction
  OXS_COMPLEX_REAL_TYPE *rptr;
  OXS_COMPLEX_REAL_TYPE * const rarr
    = static_cast<OXS_COMPLEX_REAL_TYPE *>(arr);
  for(k=0;k<rsize3_out;k++) {
    rptr = rarr + k*cstride3*2;
    for(j=0;j<rsize2_out;j++) {
      xfft.InverseDecTimeRealDataNoScale(2*csize1_in,rptr);
      rptr += 2*cstride2;
    }
  }
}

void
Oxs_FFT3D::InverseRealDataNoScale
(void* arr,
 OC_INDEX rsize1_out,OC_INDEX rsize2_out,OC_INDEX rsize3_out,
 OC_INDEX csize1_in,OC_INDEX csize2_in,OC_INDEX csize3_in,
 OC_INDEX cstride2,OC_INDEX cstride3) const
{ // Conceptually const
  if(rsize1_out>2*csize1_in || rsize2_out>csize2_in
     || rsize3_out>csize3_in
     || csize1_in>cstride2 || csize2_in*cstride2>cstride3) {
    String msg="Invalid input to "
      "Oxs_FFT3D::InverseRealDataNoScale()";
    OXS_THROW(Oxs_BadParameter,msg.c_str());
  }

  if(csize1_in==0 || csize2_in==0 || csize3_in==0)
    return; // Nothing to do

  OC_INDEX ctotalsize=cstride3*csize3_in;
  /// Total length when interpreted as a Oxs_Complex array.

  if(ctotalsize<csize1_in || ctotalsize<csize2_in
     || ctotalsize<csize3_in
     || (ctotalsize/csize3_in)/csize2_in < csize1_in) {
    // Overflow check
    String msg="OC_INDEX overflow in "
      "Oxs_FFT3D::InverseRealDataNoScale(): "
      "Total size too big to fit in a OC_INDEX variable";
    OXS_THROW(Oxs_Overflow,msg.c_str());
  }

  AdjustScratchSize(0,csize2_in,csize3_in); // dim1 transform computed
  /// in-place.

  OC_INDEX i,j,k;

  Oxs_Complex *cptr;
  Oxs_Complex * const carr = static_cast<Oxs_Complex *>(arr);

  // Do 1D iFFT's in z-direction
  // The (0,0,k) and (0,csize2_in/2,k) lines requires special
  // packing.

  if(csize3_in>1) {
    // iFFT for i=j=0;
    cptr = SetupBlocking(carr,csize3_in,rsize3_out,csize3_in,
			 cstride3,csize1_in);
    zfft.RepackRealPairImage(csize3_in,cptr);
    zfft.InverseDecTimeNoScale(csize3_in,cptr);

    // iFFT for j=0, i>0
    for(i=1;i<csize1_in;i++) {
      cptr = GetNextBlockRun();
      zfft.InverseDecTimeNoScale(csize3_in,cptr);
    }
    FlushBlockBuffer();

    if(csize2_in>1) {
      // iFFT for 0<j<csize2_in/2
      for(j=1;j<csize2_in/2;j++) {
	cptr = SetupBlocking(carr+j*cstride2,
			     csize3_in,rsize3_out,csize3_in,
			     cstride3,csize1_in);
	zfft.InverseDecTimeNoScale(csize3_in,cptr);
	for(i=1;i<csize1_in;i++) {
	  cptr = GetNextBlockRun();
	  zfft.InverseDecTimeNoScale(csize3_in,cptr);
	}
	FlushBlockBuffer();
      }

      // iFFT for j=csize2_in/2, i=0
      cptr = SetupBlocking(carr+csize2_in*cstride2/2,
			   csize3_in,rsize3_out,csize3_in,
			   cstride3,csize1_in);
      zfft.RepackRealPairImage(csize3_in,cptr);
      zfft.InverseDecTimeNoScale(csize3_in,cptr);

      // iFFT for j=csize2_in/2, i>0
      for(i=1;i<csize1_in;i++) {
	cptr = GetNextBlockRun();
	zfft.InverseDecTimeNoScale(csize3_in,cptr);
      }
      FlushBlockBuffer();

      // iFFT for j>csize2_in/2
      for(j=csize2_in/2+1;j<csize2_in;j++) {
	cptr = SetupBlocking(carr+j*cstride2,
			     csize3_in,rsize3_out,csize3_in,
			     cstride3,csize1_in);
	zfft.InverseDecTimeNoScale(csize3_in,cptr);
	for(i=1;i<csize1_in;i++) {
	  cptr = GetNextBlockRun();
	  zfft.InverseDecTimeNoScale(csize3_in,cptr);
	}
	FlushBlockBuffer();
      }
    }
  }

  // Do 1D iFFT's in y-direction
  if(csize2_in>1) {
    for(k=0;k<rsize3_out;k++) {
      cptr = SetupBlocking(carr+k*cstride3,
			   csize2_in,rsize2_out,csize2_in,
			   cstride2,csize1_in);

      // The i==0 plane requires special repacking.
      yfft.RepackRealPairImage(csize2_in,cptr);
      yfft.InverseDecTimeNoScale(csize2_in,cptr);

      for(i=1;i<csize1_in;i++) { // Do the remaining lines at this k
	cptr = GetNextBlockRun();
	yfft.InverseDecTimeNoScale(csize2_in,cptr);
      }
      FlushBlockBuffer();
    }
  }

  // Do 1D iFFT's in x-direction
  OXS_COMPLEX_REAL_TYPE *rptr;
  OXS_COMPLEX_REAL_TYPE * const rarr
    = static_cast<OXS_COMPLEX_REAL_TYPE *>(arr);
  for(k=0;k<rsize3_out;k++) {
    rptr = rarr + k*cstride3*2;
    for(j=0;j<rsize2_out;j++) {
      xfft.InverseDecTimeRealDataNoScale(2*csize1_in,rptr);
      rptr += 2*cstride2;
    }
  }
}

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
//  routine Oxs_FFT3D::RetrievePackedIndex below, and also mjd's
//  NOTES II, 27-Apr-2001, p 110-112.
Oxs_Complex
Oxs_FFT3D::RetrievePackedIndex
(Oxs_Complex* carr,
 OC_INDEX csize1,OC_INDEX csize2,OC_INDEX csize3,
 OC_INDEX cstride2,OC_INDEX cstride3,
 OC_INDEX index1,OC_INDEX index2,OC_INDEX index3)
{
#ifndef NDEBUG
  if(index1>=2*csize1 || index2>=csize2 || index3>=csize3) {
    String msg="Index out of range in"
      " Oxs_FFT3D::RetrievePackedIndex";
    OXS_THROW(Oxs_BadIndex,msg.c_str());
  }
#endif
  OC_BOOL take_conjugate=0;
  if(0<index1 && index1<csize1) {
    return carr[index1 + index2*cstride2 + index3*cstride3];
  } else {
    if(index1>csize1) {
      take_conjugate=1;
      index1=2*csize1-index1;
      if(index2>0) index2=csize2-index2;
      if(index3>0) index3=csize3-index3;
    } else if(index1==csize1) {
      index1=0;
      if(index2==0) {
	if(index3==0 || 2*index3==csize3) {
	  return Oxs_Complex(carr[index3*cstride3].im,0.);
	} else if(2*index3<csize3) {
	  index3=csize3-index3;
	  take_conjugate=1;
	}
      } else if(2*index2<csize2) {
	index2=csize2-index2;
	if(index3>0) index3=csize3-index3;
	take_conjugate=1;
      } else if(2*index2==csize2) {
	if(index3==0 || 2*index3==csize3) {
	  return Oxs_Complex(carr[index2*cstride2+index3*cstride3].im,0.);
	} else if(2*index3<csize3) {
	  index3=csize3-index3;
	  take_conjugate=1;
	}
      }
    } else { // index1==0
      if(index2==0) {
	if(index3==0 || 2*index3==csize3) {
	  return Oxs_Complex(carr[index3*cstride3].re,0.);
	} else if(2*index3>csize3) {
	  index3=csize3-index3;
	  take_conjugate=1;
	}
      } else if(2*index2>csize2) {
	index2=csize2-index2;
	if(index3>0) index3=csize3-index3;
	take_conjugate=1;
      } else if(2*index2==csize2) {
	if(index3==0 || 2*index3==csize3) {
	  return Oxs_Complex(carr[index2*cstride2+index3*cstride3].re,0.);
	} else if(2*index3>csize3) {
	  index3=csize3-index3;
	  take_conjugate=1;
	}
      }
    }
  }

  Oxs_Complex data = carr[index1 + index2*cstride2 + index3*cstride3];
  if(take_conjugate) data.im *= -1;
  return data;
}
