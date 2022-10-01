/* FILE: fft.cc             -*-Mode: c++-*-
 *
 * C++ code to do 1 and 2 dimensional FFT's.
 *
 */

#include <cstring>

#include "nb.h"
#include "fft.h"

#ifdef USE_MPI
#include "mmsmpi.h"
#endif /* USE_MPI */

/* End includes */

#undef USE_COMPLEX

#ifndef OLD_CODE
#define CMULT(xr,xi,yr,yi,zr,zi) (zr) = (xr)*(yr)-(xi)*(yi), \
                                 (zi) = (xr)*(yi)+(xi)*(yr)
#else
extern inline void CMULT(const MY_COMPLEX_REAL_TYPE &xr,
			 const MY_COMPLEX_REAL_TYPE &xi,
			 const MY_COMPLEX_REAL_TYPE &yr,
			 const MY_COMPLEX_REAL_TYPE &yi,
			 MY_COMPLEX_REAL_TYPE &zr,
			 MY_COMPLEX_REAL_TYPE &zi)
{
  zr = xr*yr-xi*yi;
  zi = xr*yi+xi*yr;
}
#endif // OLD_CODE

void FFT::ReleaseMemory(void)
{
  if(vecsize>0) {
    if(Uforward!=(MyComplex *)NULL)  delete[] Uforward;
    if(Uinverse!=(MyComplex *)NULL)  delete[] Uinverse;
    if(permindex!=(int *)NULL)     delete[] permindex;
  }
  vecsize=0;
  Uforward=Uinverse=(MyComplex *)NULL;
  permindex=(int *)NULL;
}

FFT::~FFT(void)
{
  ReleaseMemory();
}

void FFT::Setup(int size)
{
  if(size==vecsize) return;
  if(size<1)  PlainError(1,"Error in FFT::Setup(int): "
		       "Requested length (%d) must be >0",size);
  int k;
  // Check that size is power of 2
  for(k=size;k>2;k/=2)
    if(k%2!=0) PlainError(1,"Error in FFT::Setup(int): "
			"Requested length (%d) is not a power of 2",size);
  ReleaseMemory();
  vecsize=size;

  // Allocate and setup MyComplex arrays
  if((Uforward=new MyComplex[size])==0)
    PlainError(1,"Error in FFT::Setup %s",ErrNoMem);
  if((Uinverse=new MyComplex[size])==0)
    PlainError(1,"Error in FFT::Setup %s",ErrNoMem);
#ifdef ORIG_CODE
  double baseang= -2*PI/double(size);
  Uforward[0]=Uinverse[0]=MyComplex(1,0);
  double x,y;
  for(k=1;k<size/2;k++) {
    x = cos(baseang*k);
    y = -sqrt(1-x*x);
    y += (1-(x*x+y*y))/(2*y);  // Tiny error correction
    x += (1-(x*x+y*y))/(2*x);
    Uforward[k]=Uinverse[size-k]=MyComplex(x,y);
    Uforward[size-k]=Uinverse[k]=MyComplex(x,-y);
  }
  if(size>1) {
    Uforward[size/2]=Uinverse[size/2]=MyComplex(-1,0);
  }
#else
  double baseang = -2*PI/double(size);
  for(k=1;k<size/8;k++) {
    double angle=k*baseang;
    double y=sin(angle);
    double x=cos(angle);

    Uforward[k]=Uinverse[size-k]=MyComplex(x,y);
    Uforward[size/2-k]=Uinverse[size/2+k]=MyComplex(-x,y);
    Uforward[size/2+k]=Uinverse[size/2-k]=MyComplex(-x,-y);
    Uforward[size-k]=Uinverse[k]=MyComplex(x,-y);

    Uforward[size/4-k]=Uinverse[3*size/4+k]=MyComplex(-y,-x);
    Uforward[size/4+k]=Uinverse[3*size/4-k]=MyComplex(y,-x);
    Uforward[3*size/4-k]=Uinverse[size/4+k]=MyComplex(y,x);
    Uforward[3*size/4+k]=Uinverse[size/4-k]=MyComplex(-y,x);
  }
  Uforward[0]=Uinverse[0]=MyComplex(1,0);
  if(size>1) {
    Uforward[size/2]=Uinverse[size/2]=MyComplex(-1,0);
  }
  if(size>3) {
    Uforward[size/4]=Uinverse[3*size/4]=MyComplex(0,-1);
    Uforward[3*size/4]=Uinverse[size/4]=MyComplex(0,1);
  }
  if(size>7) {
    double x=SQRT1_2;  // 1/sqrt(2)
    double y=-x;
    Uforward[size/8]=Uinverse[7*size/8]=MyComplex(x,y);
    Uforward[3*size/8]=Uinverse[5*size/8]=MyComplex(-x,y);
    Uforward[5*size/8]=Uinverse[3*size/8]=MyComplex(-x,-y);
    Uforward[7*size/8]=Uinverse[size/8]=MyComplex(x,-y);
  }
#endif // ORIG_CODE

  // Allocate and setup (bit-reversal) permutation index
  if((permindex=new int[size])==0)
    PlainError(1,"Error in FFT::Setup %s",ErrNoMem);
  permindex[0]=0;
  int m,n;    // The following code relies heavily on size==2^log2vecsize
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

void FFT::ForwardDecFreq(int size,MyComplex *vec,FFT_REAL_TYPE divisor)
{
  if(divisor==0) divisor=1.0;  // Default is no normalization on forward FFT
  Setup(size);
  BaseDecFreqForward(vec);
  Permute(vec);
  if(divisor!=0. && divisor!=1.) {
    MY_COMPLEX_REAL_TYPE mult=1./divisor;
    for(int k=0;k<size;k++) vec[k]*=mult;
  }
}

void FFT::InverseDecTime(int size,MyComplex *vec,FFT_REAL_TYPE divisor)
{
  if(divisor==0) divisor=(FFT_REAL_TYPE)size;
  /// Default divisor on iFFT is 'size'
  Setup(size);
  Permute(vec);
  BaseDecTimeInverse(vec);
  if(divisor!=0. && divisor!=1.) {
    MY_COMPLEX_REAL_TYPE mult=1./divisor;
    for(int k=0;k<size;k++) vec[k]*=mult;
  }

}

inline void Swap(MyComplex &a,MyComplex &b)
{ MyComplex c(a); a=b; b=c; }

void FFT::Permute(MyComplex *vec)
{ /* Bit reversal permutation */
  int i,j;
  for(i=0;i<vecsize;i++) {
    if((j=permindex[i])!=0) Swap(vec[i],vec[j]);
  }
}

void FFT::BaseDecFreqForward(MyComplex *vec)
{ // In-place forward FFT using Decimation in Frequency technique,
  // *WITHOUT* resuffling of indices.
  // NOTE 1: This code does not use the Complex class, because
  //         some compilers do not effectively optimize around
  //         Complex operations such as multiplication.  So this
  //         routine just makes use of ordinary type "FFT_REAL_TYPE"
  //         variables, and assumes each Complex variable is
  //         actually two consecutive "MY_COMPLEX_REAL_TYPE" variables.
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

  MY_COMPLEX_REAL_TYPE *v;

  MY_COMPLEX_REAL_TYPE const *const U=(MY_COMPLEX_REAL_TYPE *)Uforward;
  MY_COMPLEX_REAL_TYPE *const dvec=(MY_COMPLEX_REAL_TYPE *)vec;

  int block,blocksize,blockcount,offset,uoff1;
  int halfbs,threehalfbs; // Half blocksize,3/2 blocksize
  FFT_REAL_TYPE m1x,m1y,m2x,m2y,m3x,m3y;
  FFT_REAL_TYPE x0,y0,x1,y1,x2,y2,x3,y3;
  FFT_REAL_TYPE xs02,ys02,xd02,yd02,xs13,ys13,xd13,yd13;
  FFT_REAL_TYPE t1x,t1y,t2x,t2y,t3x,t3y;


  // Blocksize>4
  for(blocksize=vecsize,blockcount=1;blocksize>4;
      blocksize/=4,blockcount*=4) {
    // Loop through double-step matrix multiplications
    halfbs=blocksize/2; threehalfbs=blocksize+halfbs;
    for(block=0,v=dvec;block<blockcount;block++,v+=2*blocksize) {
      for(offset=0;offset<halfbs;offset+=2) {
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

  // Do smallest blocks; size is either 4 or 2
  if(blocksize==4) {
    blockcount=vecsize/4;
    for(block=0,v=dvec;block<blockcount;block++,v+=8) {
      x0=v[0];      y0=v[1];      x1=v[2];      y1=v[3];
      x2=v[4];      y2=v[5];      x3=v[6];      y3=v[7];

      xs02=x0+x2;
      xs13=x1+x3; 
      v[0]=xs02+xs13;
      v[2]=xs02-xs13;

      ys02=y0+y2;
      ys13=y1+y3;
      v[1]=ys02+ys13;
      v[3]=ys02-ys13;

      xd02=x0-x2;
      yd13=y1-y3;
      v[4]=xd02+yd13;
      v[6]=xd02-yd13;

      yd02=y0-y2;
      xd13=x1-x3;
      v[5]=yd02-xd13;
      v[7]=yd02+xd13;
    }
  }
  else { // blocksize==2
    blockcount=vecsize/2;
    for(block=0,v=dvec;block<blockcount;block++,v+=4) {
      x0=v[0];      y0=v[1];
      x1=v[2];      y1=v[3];
      v[0]=x0+x1;   v[2]=x0-x1;
      v[1]=y0+y1;   v[3]=y0-y1;
    }
  }
}

void FFT::BaseDecTimeInverse(MyComplex *vec)
{ // In-place inverse FFT using Decimation in Time technique,
  // *WITHOUT* resuffling of indices.
  // NOTE 1: This code does not use the Complex class, because
  //         some compilers do not effectively optimize around
  //         Complex operations such as multiplication.  So this
  //         routine just makes use of ordinary type "FFT_REAL_TYPE"
  //         variables, and assumes each Complex variable is
  //         actually two consecutive "MY_COMPLEX_REAL_TYPE" variables.
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

  MY_COMPLEX_REAL_TYPE *v;

  MY_COMPLEX_REAL_TYPE const *const U=(MY_COMPLEX_REAL_TYPE *)Uinverse;
  MY_COMPLEX_REAL_TYPE *const dvec=(MY_COMPLEX_REAL_TYPE *)vec;

  int block,blocksize,blockcount,offset,uoff1;
  int halfbs,threehalfbs; // Half blocksize,3/2 blocksize
  FFT_REAL_TYPE m1x,m1y,m2x,m2y,m3x,m3y;
  FFT_REAL_TYPE x0,y0,x1,y1,x2,y2,x3,y3;
  FFT_REAL_TYPE xs01,ys01,xd01,yd01,xs23,ys23,xd23,yd23;
  FFT_REAL_TYPE t1x,t1y,t2x,t2y,t3x,t3y;

  // Do smallest blocks; size is either 4 or 2
  if(vecsize>2) {
    blockcount=vecsize/4;
    for(block=0,v=dvec;block<blockcount;block++,v+=8) {
      x0=v[0];      y0=v[1];
      x1=v[2];      y1=v[3];
      x2=v[4];      y2=v[5];
      x3=v[6];      y3=v[7];
      xs01=x0+x1;      xs23=x2+x3;   // See NOTE 3 above
      v[0]=xs01+xs23;      v[4]=xs01-xs23;
      ys01=y0+y1;      ys23=y2+y3;
      v[1]=ys01+ys23;      v[5]=ys01-ys23;
      xd01=x0-x1;      yd23=y2-y3;
      v[2]=xd01-yd23;      v[6]=xd01+yd23;
      yd01=y0-y1;      xd23=x2-x3;
      v[3]=yd01+xd23;      v[7]=yd01-xd23;
    }
  }
  else { // vecsize==2
    x0=dvec[0];    y0=dvec[1];
    x1=dvec[2];    y1=dvec[3];
    dvec[0]=x0+x1;
    dvec[2]=x0-x1;
    dvec[1]=y0+y1;
    dvec[3]=y0-y1;
    return;
  }

  // Blocksize>4
  for(blocksize=16,blockcount=vecsize/16;blocksize<=vecsize;
      blocksize*=4,blockcount/=4) {
    // Loop through double-step matric multiplications
    halfbs=blocksize/2; threehalfbs=blocksize+halfbs;
    for(block=0,v=dvec;block<blockcount;block++,v+=2*blocksize) {
      for(offset=0;offset<blocksize/2;offset+=2) {
	x0=v[offset];
	y0=v[offset+1];
	t2x=v[blocksize+offset];
	t2y=v[blocksize+offset+1];
	uoff1=offset*blockcount;
	m1x=U[uoff1];  m1y=U[uoff1+1];
	x2=t2x*m1x-t2y*m1y;
	y2=t2y*m1x+t2x*m1y;

	m2x=U[2*uoff1];  m2y=U[2*uoff1+1];
	t1x=v[halfbs+offset];
	t1y=v[halfbs+offset+1];
	x1=t1x*m2x-t1y*m2y;
	y1=t1y*m2x+t1x*m2y;

	t3x=v[threehalfbs+offset];
	t3y=v[threehalfbs+offset+1];
	m3x=U[3*uoff1];  m3y=U[3*uoff1+1];
	x3=t3x*m3x-t3y*m3y;
	y3=t3y*m3x+t3x*m3y;

	xs01=x0+x1;	xs23=x2+x3;
	v[            offset  ] = xs01+xs23;
	v[  blocksize+offset  ] = xs01-xs23;

	ys01=y0+y1;     ys23=y2+y3;
        v[            offset+1] = ys01+ys23;
	v[  blocksize+offset+1] = ys01-ys23;

	yd01=y0-y1;	xd23=x2-x3;
	v[  halfbs   +offset+1] = yd01+xd23;
	v[threehalfbs+offset+1] = yd01-xd23;

	xd01=x0-x1;	yd23=y2-y3;
	v[  halfbs   +offset  ] = xd01-yd23;
	v[threehalfbs+offset  ] = xd01+yd23;
      }
    }
  }

  if(blocksize==2*vecsize) {
    // We still have to do one single-step matrix multiplication
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

}

const double FFTReal2D::CRRCspeedratio=1.10;
/// Relative speed of ForwardCR as compared to ForwardRC.
/// If bigger than 1, then ForwardCR is faster.  This will
/// be machine & compiler dependent...oh well.

void FFTReal2D::Setup(int size1,int size2)
{ // Note: This routine is also called by FFTReal2D::SetupInverse()
  if(size1==vecsize1 && size2==vecsize2) return;  // Nothing to do

  // Check that sizes are powers of 2, and >= 1.  Also extract
  // base-2 log of sizes
  if(size1<1) PlainError(1,"Error in FFTReal2D::Setup(int): "
			 "Requested size1 (%d) must be >=1",size1);
  if(size2<1) PlainError(1,"Error in FFTReal2D::Setup(int): "
			 "Requested size2 (%d) must be >=1",size2);

  int k;
  if(size1==1) {
    logsize1=0;
  } else {
    for(k=size1,logsize1=1;k>2;k/=2,logsize1++)
      if(k%2!=0) PlainError(1,"Error in FFTReal2D::Setup(int): "
			    "Requested size1 (%d) is not a power of 2",size1);
  }
  if(size2==1) {
    logsize2=0;
  } else {
    for(k=size2,logsize2=1;k>2;k/=2,logsize2++)
      if(k%2!=0) PlainError(1,"Error in FFTReal2D::Setup(int): "
			    "Requested size2 (%d) is not a power of 2",size2);
  }

  // Allocate new space
  ReleaseMemory();
  scratch=new MyComplex[OC_MAX(size1,size2)];
  scratchb=new MyComplex[OC_MAX(size1,size2)];
  vecsize1=size1; vecsize2=size2;
}

void FFTReal2D::SetupInverse(int size1,int size2)
{
  if(size1==vecsize1 && size2==vecsize2 && workarr!=NULL)
    return;  // Nothing to do
  Setup(size1,size2);
  if(workarr!=NULL) { delete[] workarr[0]; delete[] workarr; } // Safety
  int rowcount=(vecsize1/2)+1;
  workarr=new MyComplex*[rowcount];
  workarr[0]=new MyComplex[rowcount*vecsize2];
  for(int i=1;i<rowcount;i++) workarr[i]=workarr[i-1]+vecsize2;
}

void FFTReal2D::ReleaseMemory()
{
  if(vecsize1==0 || vecsize2==0) return;
  delete[] scratch;   scratch=NULL;  
  delete[] scratchb;  scratchb=NULL;  
  if(workarr!=NULL) {
    delete[] workarr[0];
    delete[] workarr;
    workarr=NULL;
  }
  vecsize1=0; vecsize2=0;
  fft1.ReleaseMemory(); fft2.ReleaseMemory();
}

void FFTReal2D::FillOut(int csize1,int csize2,MyComplex** carr)
{ // This routine assumes carr is a top-half-filled DFT of
  // a real function, and fills in the bottom half using the
  // relation
  //      carr[csize1-i][csize2-j]=conj(carr[i][j])
  // for i>csize1/2, with the second indices interpreted 'mod csize2'.
  int i,j;
  for(i=1;i<csize1/2;i++) {
    carr[csize1-i][0]=conj(carr[i][0]);
    for(j=1;j<csize2;j++) {
      carr[csize1-i][j]=conj(carr[i][csize2-j]);
    }
  }

}

void FFTReal2D::ForwardCR(int rsize1,int rsize2,
			  const double* const* rarr,
			  int csize1,int csize2,MyComplex** carr)
{
  Setup(OC_MAX(1,2*(csize1-1)),csize2); // Safety
  if(vecsize1<2 || vecsize2<2) 
    PlainError(1,"Error in FFTReal2D::ForwardCR(...): "
	       "Full array dimensions (%dx%d) must be both >=2",
	       vecsize1,vecsize2);

  int i,j;
  FFT_REAL_TYPE x1,x2,y1,y2;
  FFT_REAL_TYPE xb1,xb2,yb1,yb2;

  // Do FFT on columns, 2 at a time
  for(j=0;j+3<rsize2;j+=4) {
    // Pack into MyComplex scratch array
    for(i=0;i<rsize1;i++) {
      scratch[i]=MyComplex(rarr[i][j],rarr[i][j+1]);
      scratchb[i]=MyComplex(rarr[i][j+2],rarr[i][j+3]);
    }
    // Zero pad scratch space
    for(i=rsize1;i<vecsize1;i++) scratch[i]=MyComplex(0.,0.);
    for(i=rsize1;i<vecsize1;i++) scratchb[i]=MyComplex(0.,0.);
    // Do complex FFT
    fft1.ForwardDecFreq(vecsize1,scratchb);
    fft1.ForwardDecFreq(vecsize1,scratch);
    // Unpack into top half of 2D complex array
    // Rows 0 & vecsize1/2 are real-valued, so pack them together
    // into row 0 (row 0 as real part, row vecsize1/2 as imag. part).
    carr[0][j]  =MyComplex(scratch[0].real(),scratch[vecsize1/2].real());
    carr[0][j+1]=MyComplex(scratch[0].imag(),scratch[vecsize1/2].imag());
    carr[0][j+2]=MyComplex(scratchb[0].real(),scratchb[vecsize1/2].real());
    carr[0][j+3]=MyComplex(scratchb[0].imag(),scratchb[vecsize1/2].imag());
    for(i=1;i<vecsize1/2;i++) { // ASSUMES vecsize1 is even!
      x1=scratch[i].real()/2;            y1=scratch[i].imag()/2;
      x2=scratch[vecsize1-i].real()/2;   y2=scratch[vecsize1-i].imag()/2;
      xb1=scratchb[i].real()/2;          yb1=scratchb[i].imag()/2;
      xb2=scratchb[vecsize1-i].real()/2; yb2=scratchb[vecsize1-i].imag()/2;
      carr[i][j]   =MyComplex(x1+x2,y1-y2);
      carr[i][j+1] =MyComplex(y1+y2,x2-x1);
      carr[i][j+2] =MyComplex(xb1+xb2,yb1-yb2);
      carr[i][j+3] =MyComplex(yb1+yb2,xb2-xb1);
    }
  }
  // Case rsize2 not divisible by 4
  for(;j<rsize2;j+=2) {
    // Pack into complex scratch array
    if(j+1<rsize2) {
      for(i=0;i<rsize1;i++) scratch[i]=MyComplex(rarr[i][j],rarr[i][j+1]);
    }
    else { // rsize2 == 1 mod 2.
      for(i=0;i<rsize1;i++) scratch[i]=MyComplex(rarr[i][j],0.);
    }
    for(i=rsize1;i<vecsize1;i++) scratch[i]=MyComplex(0.,0.);
    fft1.ForwardDecFreq(vecsize1,scratch);
    carr[0][j]  =MyComplex(scratch[0].real(),scratch[vecsize1/2].real());
    carr[0][j+1]=MyComplex(scratch[0].imag(),scratch[vecsize1/2].imag());
    for(i=1;i<vecsize1/2;i++) { // ASSUMES vecsize1 is even!
      x1=scratch[i].real()/2;          y1=scratch[i].imag()/2;
      x2=scratch[vecsize1-i].real()/2; y2=scratch[vecsize1-i].imag()/2;
      carr[i][j]   =MyComplex(x1+x2,y1-y2);
      carr[i][j+1] =MyComplex(y1+y2,x2-x1);
    }
  }
  // Zero-pad remaining columns
  if(rsize2<csize2) {
    for(i=0;i<csize1;i++) for(j=rsize2;j<csize2;j++)
      carr[i][j]=MyComplex(0.,0.);
    // Note: One _may_ be able to gain a few percent speedup
    //       by using the 'memcpy' C-library routine.
  }

  // Do FFT on top half of rows
  for(i=0;i<csize1-1;i++) fft2.ForwardDecFreq(csize2,carr[i]);

  // Pull out row 0 & row csize1-1 from (packed) row 0
  carr[csize1-1][0] = MyComplex(carr[0][0].imag(),0.);
  carr[0][0]        = MyComplex(carr[0][0].real(),0.);
  for(j=1;j<csize2/2;j++) {
      x1=carr[0][j].real()/2;        y1=carr[0][j].imag()/2;
      x2=carr[0][csize2-j].real()/2; y2=carr[0][csize2-j].imag()/2;
      MyComplex temp1(x1+x2,y1-y2);
      MyComplex temp2(y1+y2,x2-x1);
      carr[0][j]                = temp1;
      carr[0][csize2-j]         = conj(temp1);
      carr[csize1-1][j]         = temp2;
      carr[csize1-1][csize2-j]  = conj(temp2);
  }
  carr[csize1-1][csize2/2] = MyComplex(carr[0][csize2/2].imag(),0.);
  carr[0][csize2/2]        = MyComplex(carr[0][csize2/2].real(),0.);
}

void FFTReal2D::ForwardRC(int rsize1,int rsize2,
			  const double* const* rarr,
			  int csize1,int csize2,MyComplex** carr)
{
  Setup(OC_MAX(1,2*(csize1-1)),csize2); // Safety
  if(vecsize1<2 || vecsize2<2) 
    PlainError(1,"Error in FFTReal2D::ForwardRC(...): "
	       "Full array dimensions (%dx%d) must be both >=2",
	       vecsize1,vecsize2);

  int i,j;
  FFT_REAL_TYPE x1,x2,y1,y2;
  FFT_REAL_TYPE xb1,xb2,yb1,yb2;

  // Do row FFT's
  for(i=0;i+1<rsize1;i+=2) {
    // Pack 'MyComplex' row
    for(j=0;j<rsize2;j++)
      carr[i/2][j]=MyComplex(rarr[i][j],rarr[i+1][j]);
    for(j=rsize2;j<csize2;j++) carr[i/2][j]=MyComplex(0.,0.); // Zero pad
    // Do FFT
    fft2.ForwardDecFreq(csize2,carr[i/2]);
  }
  for(;i<rsize1;i+=2) { // In case rsize1 == 1 mod 2
    for(j=0;j<rsize2;j++)
      carr[i/2][j]=MyComplex(rarr[i][j],0.);
    for(j=rsize2;j<csize2;j++) carr[i/2][j]=MyComplex(0.,0.); // Zero pad
    // Do FFT
    fft2.ForwardDecFreq(csize2,carr[i/2]);
  }
  // Any remaining rows are zero padding on the fly during
  // the column FFT's (see below).

  // Do column FFT's
  // Do column 0 and csize2/2, making use of the fact that
  // these 2 columns are 'real'.
  for(i=0;i<(rsize1+1)/2;i++) {
    x1=carr[i][0].real();         x2=carr[i][0].imag();
    y1=carr[i][csize2/2].real();  y2=carr[i][csize2/2].imag();
    scratch[2*i]     = MyComplex(x1,y1);
    scratch[(2*i)+1] = MyComplex(x2,y2);
  }
  for(i*=2;i<vecsize1;i++) scratch[i]=MyComplex(0.,0.); // Zero pad
  fft1.ForwardDecFreq(vecsize1,scratch);
  carr[0][0]        = MyComplex(scratch[0].real(),0.);
  carr[0][csize2/2] = MyComplex(scratch[0].imag(),0.);
  for(i=1;i<csize1-1;i++) {
    x1=scratch[i].real()/2;           y1=scratch[i].imag()/2;
    x2=scratch[vecsize1-i].real()/2;  y2=scratch[vecsize1-i].imag()/2;
    carr[i][0]        = MyComplex(x1+x2,y1-y2);
    carr[i][csize2/2] = MyComplex(y1+y2,x2-x1);
  }
  carr[csize1-1][0]        = MyComplex(scratch[csize1-1].real(),0.);
  carr[csize1-1][csize2/2] = MyComplex(scratch[csize1-1].imag(),0.);

  // Do remaining columns
  for(j=1;j+1<csize2/2;j+=2) {
    for(i=0;i<(rsize1+1)/2;i++) {
      x1 =carr[i][j].real()/2;           y1 =carr[i][j].imag()/2;
      xb1=carr[i][j+1].real()/2;         yb1=carr[i][j+1].imag()/2;
      xb2=carr[i][csize2-1-j].real()/2;  yb2=carr[i][csize2-1-j].imag()/2;
      x2 =carr[i][csize2-j].real()/2;    y2 =carr[i][csize2-j].imag()/2;
      scratch[2*i]     = MyComplex(x1+x2,y1-y2);
      scratch[(2*i)+1] = MyComplex(y1+y2,x2-x1);
      scratchb[2*i]     = MyComplex(xb1+xb2,yb1-yb2);
      scratchb[(2*i)+1] = MyComplex(yb1+yb2,xb2-xb1);
    }
    for(i*=2;i<vecsize1;i++)
      scratch[i]= scratchb[i]=MyComplex(0.,0.);  // Zero pad
    fft1.ForwardDecFreq(vecsize1,scratchb);
    fft1.ForwardDecFreq(vecsize1,scratch);
    carr[0][j]=scratch[0];     carr[0][csize2-j]=conj(scratch[0]);
    carr[0][j+1]=scratchb[0];  carr[0][csize2-1-j]=conj(scratchb[0]);
    for(i=1;i<csize1;i++) {
      carr[i][j]=scratch[i];
      carr[i][j+1]=scratchb[i];
      carr[i][csize2-1-j]=conj(scratchb[vecsize1-i]);
      carr[i][csize2-j]=conj(scratch[vecsize1-i]);
    }
  }
  // There should be 1 column left over
  if(j<csize2/2) {
    for(i=0;i<(rsize1+1)/2;i++) {
      x1=carr[i][j].real()/2;         y1=carr[i][j].imag()/2;
      x2=carr[i][csize2-j].real()/2;  y2=carr[i][csize2-j].imag()/2;
      scratch[2*i]     = MyComplex(x1+x2,y1-y2);
      scratch[(2*i)+1] = MyComplex(y1+y2,x2-x1);
    }
    for(i*=2;i<vecsize1;i++) scratch[i]=MyComplex(0.,0.); // Zero pad
    fft1.ForwardDecFreq(vecsize1,scratch);
    carr[0][j]=scratch[0];   carr[0][csize2-j]=conj(scratch[0]);
    for(i=1;i<csize1;i++) {
      carr[i][j]=scratch[i];
      carr[i][csize2-j]=conj(scratch[vecsize1-i]);
    }
  }
}

void FFTReal2D::InverseRC(int csize1,int csize2,
			  const MyComplex* const* carr,
			  int rsize1,int rsize2,double** rarr)
{
  SetupInverse(OC_MAX(1,2*(csize1-1)),csize2); // Safety.
  if(vecsize1<2 || vecsize2<2)
    PlainError(1,"Error in FFTReal2D::InverseRC(...): "
	       "Full array dimensions (%dx%d) must be both >=2",
	       vecsize1,vecsize2);

  int i,j;
  FFT_REAL_TYPE x1,y1,x2,y2;
  FFT_REAL_TYPE xb1,yb1,xb2,yb2;

  // Do row inverse FFT's
  // Handle the first & csize1'th row specially.  These rows are
  // the DFT's of real sequences, so they each satisfy the conjugate
  // symmetry condition
  workarr[0][0]=MyComplex(carr[0][0].real(),carr[csize1-1][0].real());
  for(j=1;j<csize2/2;j++) {
    x1=carr[0][j].real();         y1=carr[0][j].imag();
    x2=carr[csize1-1][j].real();  y2=carr[csize1-1][j].imag();
    workarr[0][j]        = MyComplex(x1-y2,x2+y1);
    workarr[0][csize2-j] = MyComplex(x1+y2,x2-y1);
  }
  workarr[0][csize2/2]=MyComplex(carr[0][csize2/2].real(),
			       carr[csize1-1][csize2/2].real());
  fft2.InverseDecTime(csize2,workarr[0],1.);

  // iFFT the remaining rows
  for(i=1;i<csize1-1;i++) {
    for(j=0;j<csize2;j++) workarr[i][j]=carr[i][j];
    fft2.InverseDecTime(csize2,workarr[i],1.);
  }

  // Now do iFFT's on columns.  These are conj. symmetric, so we
  // process them 2 at a time.  Also, recall the 1st row of workarr
  // contains the iFFT's of the 1st and csize1'th row of the given carr.
  for(j=0;j+3<rsize2;j+=4) {
    scratch[0]=
      MyComplex(workarr[0][j].real(),workarr[0][j+1].real());
    scratch[csize1-1]=
      MyComplex(workarr[0][j].imag(),workarr[0][j+1].imag());
    scratchb[0]=
      MyComplex(workarr[0][j+2].real(),workarr[0][j+3].real());
    scratchb[csize1-1]=
      MyComplex(workarr[0][j+2].imag(),workarr[0][j+3].imag());
    for(i=1;i<csize1-1;i++) {
      x1 =workarr[i][j].real();    y1 =workarr[i][j].imag();
      x2 =workarr[i][j+1].real();  y2 =workarr[i][j+1].imag();
      xb1=workarr[i][j+2].real();  yb1=workarr[i][j+2].imag();
      xb2=workarr[i][j+3].real();  yb2=workarr[i][j+3].imag();
      scratch[i]          = MyComplex(x1-y2,x2+y1);
      scratch[vecsize1-i] = MyComplex(x1+y2,x2-y1);
      scratchb[i]          = MyComplex(xb1-yb2,xb2+yb1);
      scratchb[vecsize1-i] = MyComplex(xb1+yb2,xb2-yb1);
    }
    fft1.InverseDecTime(vecsize1,scratchb,FFT_REAL_TYPE(vecsize1*vecsize2));
    fft1.InverseDecTime(vecsize1,scratch,FFT_REAL_TYPE(vecsize1*vecsize2));
    for(i=0;i<rsize1;i++) {
      rarr[i][j]  =scratch[i].real();
      rarr[i][j+1]=scratch[i].imag();
      rarr[i][j+2]=scratchb[i].real();
      rarr[i][j+3]=scratchb[i].imag();
    }
  }
  // Remaining columns if rsize2 is not divisible by 4.  OTOH, csize2
  // *is* divisible by 2, so we can assume workarr[i][j+1] exists.
  for(;j<rsize2;j+=2) {
    scratch[0]=
      MyComplex(workarr[0][j].real(),workarr[0][j+1].real());
    scratch[csize1-1]=
      MyComplex(workarr[0][j].imag(),workarr[0][j+1].imag());
    for(i=1;i<csize1-1;i++) {
      x1 =workarr[i][j].real();    y1 =workarr[i][j].imag();
      x2 =workarr[i][j+1].real();  y2 =workarr[i][j+1].imag();
      scratch[i]          = MyComplex(x1-y2,x2+y1);
      scratch[vecsize1-i] = MyComplex(x1+y2,x2-y1);
    }
    fft1.InverseDecTime(vecsize1,scratch,FFT_REAL_TYPE(vecsize1*vecsize2));
    for(i=0;i<rsize1;i++) {
      rarr[i][j]  =scratch[i].real();
      if(j+1<rsize2) rarr[i][j+1]=scratch[i].imag();
    }
  }
}

void FFTReal2D::InverseCR(int csize1,int csize2,
			  const MyComplex* const* carr,
			  int rsize1,int rsize2,double** rarr)
{
  SetupInverse(OC_MAX(1,2*(csize1-1)),csize2); // Safety
  if(vecsize1<2 || vecsize2<2)
    PlainError(1,"Error in FFTReal2D::InverseCR(...): "
	       "Full array dimensions (%dx%d) must be both >=2",
	       vecsize1,vecsize2);

  int i,j;
  FFT_REAL_TYPE x1,y1,x2,y2,xb1,yb1,xb2,yb2;

  // Column iFFT's
  // Handle the first & csize2/2'th column specially.  These cols are
  // the DFT's of real sequences, so they each satisfy the conjugate
  // symmetry condition
  scratch[0]=MyComplex(carr[0][0].real(),carr[0][csize2/2].real());
  for(i=1;i<csize1-1;i++) {
    x1=carr[i][0].real();         y1=carr[i][0].imag();
    x2=carr[i][csize2/2].real();  y2=carr[i][csize2/2].imag();
    scratch[i]          = MyComplex(x1-y2,x2+y1);
    scratch[vecsize1-i] = MyComplex(x1+y2,x2-y1);
  }
  scratch[csize1-1]=MyComplex(carr[csize1-1][0].real(),
			    carr[csize1-1][csize2/2].real());
  fft1.InverseDecTime(vecsize1,scratch,1);
  for(i=0;i<vecsize1;i+=2) { // ASSUMES vecsize1 is even
    // See packing note below.
    workarr[i/2][0]        = MyComplex(scratch[i].real(),scratch[i+1].real());
    workarr[i/2][csize2/2] = MyComplex(scratch[i].imag(),scratch[i+1].imag());
  }
  //
  // Do remaining column iFFT's, two at a time for better memory
  // access locality.
  for(j=1;j+1<csize2/2;j+=2) {
    scratch[0]=carr[0][j];
    scratchb[0]=carr[0][j+1];
    for(i=1;i<csize1-1;i++) {
      scratch[i]=carr[i][j];
      scratchb[i]=carr[i][j+1];
      scratchb[vecsize1-i]=conj(carr[i][csize2-1-j]);
      scratch[vecsize1-i]=conj(carr[i][csize2-j]);
    }
    scratch[csize1-1]=carr[csize1-1][j];
    scratchb[csize1-1]=carr[csize1-1][j+1];
    fft1.InverseDecTime(vecsize1,scratchb,1.);
    fft1.InverseDecTime(vecsize1,scratch,1.);
    // Pack into workarr.  Rows will be conjugate symmetric, so we
    // can pack two rows into 1 via r[k]+i.r[k+1] -> workarr[k/2].
    for(i=0;i<rsize1;i+=2) {
      // CAREFUL! The above 'rsize1' bound may depend on how the
      // iFFT's are calculated in the 'Row iFFT's' code section,
      // and how 'i' is initialized.
      x1=scratch[i].real();      y1=scratch[i].imag();
      x2=scratch[i+1].real();    y2=scratch[i+1].imag();
      xb1=scratchb[i].real();    yb1=scratchb[i].imag();
      xb2=scratchb[i+1].real();  yb2=scratchb[i+1].imag();
      workarr[i/2][j]          = MyComplex(x1-y2,x2+y1);
      workarr[i/2][j+1]        = MyComplex(xb1-yb2,xb2+yb1);
      workarr[i/2][csize2-j-1] = MyComplex(xb1+yb2,xb2-yb1);
      workarr[i/2][csize2-j]   = MyComplex(x1+y2,x2-y1);
    }
  }
  // There should be 1 column left over
  if((j=(csize2/2)-1)%2==1) {
    // Column (csize2/2)-1 *not* processed above
    scratch[0]=carr[0][j];
    for(i=1;i<csize1-1;i++) {
      scratch[i]=carr[i][j];
      scratch[vecsize1-i]=conj(carr[i][csize2-j]);
    }
    scratch[csize1-1]=carr[csize1-1][j];
    fft1.InverseDecTime(vecsize1,scratch,1);
    for(i=0;i<rsize1;i+=2) {
      // CAREFUL! The above 'rsize1' bound may depend on how the
      // iFFT's are calculated in the 'Row iFFT's' code section,
      // and how 'i' is initialized.
      x1=scratch[i].real();    y1=scratch[i].imag();
      x2=scratch[i+1].real();  y2=scratch[i+1].imag();
      workarr[i/2][j]        = MyComplex(x1-y2,x2+y1);
      workarr[i/2][csize2-j] = MyComplex(x1+y2,x2-y1);
    }
  }

  // Row iFFT's
  for(i=0;i<rsize1;i+=2) {
    fft2.InverseDecTime(vecsize2,workarr[i/2],
			FFT_REAL_TYPE(vecsize1*vecsize2));
    for(j=0;j<rsize2;j++) rarr[i][j]   = workarr[i/2][j].real();
    if(i+1<rsize1) {
      for(j=0;j<rsize2;j++) rarr[i+1][j] = workarr[i/2][j].imag();
    }
  }
}

void FFTReal2D::Forward1D(int rsize1,int rsize2,
			  const double* const* rarr,
			  int csize1,int csize2,MyComplex** carr)
{
  // The ForwardRC/CR routines assume full array dimensions >1.
  // This routine handles the special case where (at least) one of
  // the dimension is 1, which degenerates into a simple 1D FFT.
  if(csize1==1) { // Single row FFT
    int j;
    for(j=0;j<rsize2;j++)
      carr[0][j]=MyComplex(rarr[0][j],0.);
    for(;j<csize2;j++)
      carr[0][j]=MyComplex(0.,0.);
    fft2.ForwardDecFreq(csize2,carr[0]);
  } else if(csize2==1) { // Single column FFT
    int i;
    for(i=0;i<rsize1;i++)
      scratch[i]=MyComplex(rarr[i][0],0.0);
    for(;i<vecsize1;i++)
      scratch[i]=MyComplex(0.0,0.0);
    fft1.ForwardDecFreq(vecsize1,scratch);
    for(i=0;i<csize1;i++)
      carr[i][0]=scratch[i]; // Last half, from csize1 to vecsize1
                            /// is conj. sym. since input is real.
  } else {
    PlainError(1,"Error in FFTReal2D::Forward1D(...): "
	       "One array dimension (of %dx%d) must be==1",
	       vecsize1,vecsize2);
  }
}

void FFTReal2D::Inverse1D(int csize1,int csize2,
			  const MyComplex* const* carr,
			  int rsize1,int rsize2,double** rarr)
{
  // The InverseRC/CR routines assume full array dimensions >1.
  // This routine handles the special case where (at least) one of
  // the dimension is 1, which degenerates into a simple 1D FFT.
  if(csize1==1) { // Single row iFFT
    int j;
    for(j=0;j<csize2;j++)
      scratch[j]=carr[0][j];
    fft2.InverseDecTime(csize2,scratch);
    for(j=0;j<rsize2;j++)
      rarr[0][j]=scratch[j].real();
  } else if(csize2==1) { // Single column iFFT
    int i;
    scratch[0]=carr[0][0];
    for(i=1;i<csize1-1;i++) {
      scratch[i]=carr[i][0];
      scratch[vecsize1-i]=conj(carr[i][0]); // Last half obtained
      /// by using conjugate symmetry of real data FFT.
    }
    scratch[csize1-1]=carr[csize1-1][0];
    fft2.InverseDecTime(vecsize1,scratch);
    for(i=0;i<rsize1;i++)
      rarr[i][0]=scratch[i].real();
  } else {
    PlainError(1,"Error in FFTReal2D::Inverse1D(...): "
	       "One array dimension (of %dx%d) must be==1",
	       vecsize1,vecsize2);
  }
}

void FFTReal2D::Forward(int rsize1,int rsize2,
			const double* const* rarr,
			int csize1,int csize2,MyComplex** carr)
{
  if(csize2<rsize2) 
    PlainError(1,"Error in FFTRealD::Forward(int,int,OC_REAL8,**,int,int,"
	       "MyComplex**): csize2 (=%d) *must* be >= rsize2 (=%d)\n",
	       csize2,rsize2);

  if(csize1<(rsize1/2)+1)
    PlainError(1,"Error in FFTRealD::Forward(int,int,double**,int,int,"
	       "MyComplex**): csize1 (=%d) *must* be >= (rsize1/2)+1"
	       " (=%d)\n",csize1,(rsize1/2)+1);

  Setup(OC_MAX(1,2*(csize1-1)),csize2);
  if(vecsize1<1 || vecsize2<1) return; // Nothing to do

  // Check for 1D degenerate cases
  if(vecsize1==1 || vecsize2==1) {
    Forward1D(rsize1,rsize2,rarr,csize1,csize2,carr);
  } else {
    // Determine which Forward routine to call (ForwardCR or ForwardRC)
    // (Use double arithmetic to protect against integer overflow.  If
    // the two times are very close, then the choice doesn't really
    // matter.)
    // 1) Estimated (proportional) time for ForwardCR
    double crtime=double(vecsize1*vecsize2)*double(logsize2)
      + double(vecsize1*rsize2)*double(logsize1);
    // 2) Estimated (proportional) time for ForwardRC
    double rctime=double(rsize1*vecsize2)*double(logsize2)
      + double(vecsize1*vecsize2)*double(logsize1);
    // Introduce empirical adjustment factor
    rctime*=CRRCspeedratio;
    if(crtime<=rctime) ForwardCR(rsize1,rsize2,rarr,csize1,csize2,carr);
    else               ForwardRC(rsize1,rsize2,rarr,csize1,csize2,carr);
  }
}

void FFTReal2D::Inverse(int csize1,int csize2,
			const MyComplex* const* carr,
			int rsize1,int rsize2,double** rarr)
{
  if(csize2<rsize2) 
    PlainError(1,"Error in FFTRealD::Inverse(int,int,double**,"
	       "int,int,MyComplex**): csize2 (=%d) *must* be >="
	       " rsize2 (=%d)\n",csize2,rsize2);

  if(csize1<(rsize1/2)+1) 
    PlainError(1,"Error in FFTRealD::Inverse(int,int,double**,"
	       "int,int,MyComplex**): csize1 (=%d) *must* be >="
	       " (rsize1/2)+1 (=%d)\n",csize1,(rsize1/2)+1);

  SetupInverse(OC_MAX(1,2*(csize1-1)),csize2);
  if(vecsize1<1 || vecsize2<1) return; // Nothing to do

  // Check for 1D degenerate cases
  if(vecsize1==1 || vecsize2==1) {
    Inverse1D(csize1,csize2,carr,rsize1,rsize2,rarr);
  } else {
    // Determine which Inverse routine to call (InverseRC or InverseCR)
    // (Use double arithmetic to protect against integer overflow.
    // If the two times are very close, then the choice doesn't really
    // matter.)
    // 1) Estimated (proportional) time for InverseRC (==ForwardCR)
    double irctime=double(vecsize1*vecsize2)*double(logsize2)
      + double(vecsize1*rsize2)*double(logsize1);
    // 2) Estimated (proportional) time for InverseCR (==ForwardRC)
    double icrtime=double(rsize1*vecsize2)*double(logsize2)
      + double(vecsize1*vecsize2)*double(logsize1);
    // Introduce empirical adjustment factor
    icrtime*=CRRCspeedratio;
    if(irctime<=icrtime) InverseRC(csize1,csize2,carr,rsize1,rsize2,rarr);
    else                 InverseCR(csize1,csize2,carr,rsize1,rsize2,rarr);
  }
}


#ifdef USE_MPI

static FFT fft1_mpi,fft2_mpi;
static int vecsize1_mpi(0),vecsize2_mpi(0);
static MyComplex* scratch_mpi(NULL);
static MyComplex** workarr_mpi(NULL);

FFTReal2D_mpi::FFTReal2D_mpi()
{
}

void SetupMemory_mpi(int size1,int size2)
{
  if(size1!=vecsize1_mpi) {
    if(scratch_mpi!=NULL) delete[] scratch_mpi;
    vecsize1_mpi=size1;
    scratch_mpi=new MyComplex[vecsize1_mpi];
  }
  if(size2!=vecsize2_mpi) {
    if(workarr_mpi!=NULL) {
      delete[] workarr_mpi[0];
      delete[] workarr_mpi;
    }
    vecsize2_mpi=size2;
    int rowcount=(vecsize1_mpi/2)+1;
    workarr_mpi=new MyComplex*[rowcount];
    workarr_mpi[0]=new MyComplex[rowcount*vecsize2_mpi];
    for(int i=1;i<rowcount;i++)
      workarr_mpi[i]=workarr_mpi[i-1]+vecsize2_mpi;
  }
}

void ReleaseMemory_mpi()
{
  fft1_mpi.ReleaseMemory();
  fft2_mpi.ReleaseMemory();
  if(scratch_mpi!=NULL) delete[] scratch_mpi;
  scratch_mpi=NULL;
  vecsize1_mpi=0;
  if(workarr_mpi!=NULL) {
    delete[] workarr_mpi[0];
    delete[] workarr_mpi;
  }
  workarr_mpi=NULL;
  vecsize2_mpi=0;
}

void FFTReal2D_mpi::ReleaseMemory()
{
  Mmsolve_MpiWakeUp(ReleaseMemory_mpi);  // On slaves
  ReleaseMemory_mpi();                   // On master
}

static int vecsize1_mpi_b(0),vecsize2_mpi_b(0);
static MyComplex **work1_mpi(NULL),**work2_mpi(NULL);

static void SetupMemory_mpi_base_b(int size1,int size2)
{
  int i;
  if(size1!=vecsize1_mpi_b || size2!=vecsize2_mpi_b) {
    if(work1_mpi!=NULL) {
      delete[] work1_mpi[0];
      delete[] work1_mpi;
    }
    if(work2_mpi!=NULL) {
      delete[] work2_mpi[0];
      delete[] work2_mpi;
    }
    vecsize1_mpi_b=size1;
    vecsize2_mpi_b=size2;
    work1_mpi=new MyComplex*[vecsize1_mpi_b];
    work1_mpi[0]=new MyComplex[vecsize1_mpi_b*vecsize2_mpi_b];
    for(i=1;i<vecsize1_mpi_b;i++)
      work1_mpi[i]=work1_mpi[i-1]+vecsize2_mpi_b;
    int rowcount=vecsize2_mpi_b/2;
    work2_mpi=new MyComplex*[rowcount];
    work2_mpi[0]=new MyComplex[rowcount*vecsize1_mpi_b];
    for(i=1;i<rowcount;i++)
      work2_mpi[i]=work2_mpi[i-1]+vecsize1_mpi_b;
  }
}

static void SetupMemory_mpi_slave_b()
{
  int size1,size2;
  MPI_Bcast(&size1,1,MPI_INT,0,MPI_COMM_WORLD);
  MPI_Bcast(&size2,1,MPI_INT,0,MPI_COMM_WORLD);
  if(size1<1 || size2<1)
    PlainError(1,"Error propagating array size info in "
	       "SetupMemory_mpi_slave_b(): size1=%d, size2=%d",
	       size1,size2);
  SetupMemory_mpi_base_b(size1,size2);
}

static void SetupMemory_mpi_master_b(int size1,int size2)
{
  Mmsolve_MpiWakeUp(SetupMemory_mpi_slave_b);
  MPI_Bcast(&size1,1,MPI_INT,0,MPI_COMM_WORLD);
  MPI_Bcast(&size2,1,MPI_INT,0,MPI_COMM_WORLD);
  SetupMemory_mpi_base_b(size1,size2);
}

void ForwardFFT1_mpi_slave_b()
{
  // Get data
  int rowcount,colcount;
  colcount=vecsize2_mpi_b;
  MPI_Request request[1];
  MPI_Status status[1];
  MPI_Irecv(&rowcount,1,MPI_INT,0,1,MPI_COMM_WORLD,request);
  MPI_Waitall(1,request,status);
  MPI_Irecv(work1_mpi[0],rowcount*colcount,MMS_COMPLEX,0,2,
	   MPI_COMM_WORLD,request);
  MPI_Waitall(1,request,status);

  // Transform rows
  for(int i=0;i<rowcount;i++) {
    fft1_mpi.ForwardDecFreq(colcount,work1_mpi[i]);
  }

  // Return results
  MPI_Isend(work1_mpi[0],rowcount*colcount,MMS_COMPLEX,0,3,
	   MPI_COMM_WORLD,request);
  MPI_Waitall(1,request,status);
}

void ForwardFFT2_mpi_slave_b()
{
  // Get data
  int rowcount,colcount;
  colcount=vecsize1_mpi_b;
  MPI_Request request[1];
  MPI_Status status[1];
  MPI_Irecv(&rowcount,1,MPI_INT,0,1,MPI_COMM_WORLD,request);
  MPI_Waitall(1,request,status);
  MPI_Irecv(work2_mpi[0],rowcount*colcount,MMS_COMPLEX,0,2,
	   MPI_COMM_WORLD,request);
  MPI_Waitall(1,request,status);

  // Transform rows
  for(int i=0;i<rowcount;i++) {
    fft2_mpi.ForwardDecFreq(colcount,work2_mpi[i]);
  }

  // Return results
  MPI_Isend(work2_mpi[0],rowcount*colcount,MMS_COMPLEX,0,3,
	   MPI_COMM_WORLD,request);
  MPI_Waitall(1,request,status);
}

void FFTReal2D_mpi::Forward(int rsize1,int rsize2,
			    const double* const* rarr,
			    int csize1,int csize2,MyComplex** carr)
{ // Computes FFT of rsize1 x rsize2 double** rarr, leaving result in
  // csize1 x csize2 MyComplex** carr.  rsize2 *must* be <= csize2, and
  // rsize1 *must* be <= 2*(csize1-1).  This routine returns only the
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

  int i,j;
  FFT_REAL_TYPE x1,y1,x2,y2;
  int vecsize1=OC_MAX(1,2*(csize1-1));
  int vecsize2=csize2;
  for(i=vecsize1;i>2;i/=2)
    if(i%2!=0) PlainError(1,"Error in FFTReal2D_mpi::Forward(int): "
			  "Requested csize1 - 1 (%d - 1) is not a power of 2",
			  csize1);
  for(j=vecsize2;j>2;j/=2)
    if(j%2!=0) PlainError(1,"Error in FFTReal2D_mpi::Forward(int): "
			  "Requested csize2 (%d) is not a power of 2",
			  csize2);
  if(vecsize1==0 || vecsize2==0) return; // Nothing to do
  SetupMemory_mpi_master_b(vecsize1,vecsize2);

  // Copy input data into packed complex array
  for(i=0;i<rsize1/2;i++) {
    for(j=0;j<rsize2;j++)
      work1_mpi[i][j]=MyComplex(rarr[2*i][j],rarr[2*i+1][j]);
    for(;j<vecsize2;j++)
      work1_mpi[i][j]=MyComplex(0,0);  // Zero pad
  }
  if(2*i<rsize1) {
    // Odd number of rows.  Pack last with zeros.
    for(j=0;j<rsize2;j++)
      work1_mpi[i][j]=MyComplex(rarr[2*i][j],0.0);
    for(;j<vecsize2;j++)
      work1_mpi[i][j]=MyComplex(0,0);  // Zero pad
  }

  // Do FFT across rows
  int proc,proc_count,base_size,base_orphan,whole_size,chunk_size;
  proc_count=mms_mpi_size;
  Mmsolve_MpiWakeUp(ForwardFFT1_mpi_slave_b);
  whole_size=(rsize1+1)/2;
  base_size=whole_size/proc_count;
  base_orphan=whole_size%proc_count;
  i=0;
  chunk_size=base_size+(0<base_orphan?1:0);
  int msg_count=3*(proc_count-1);
  MPI_Request *request=new MPI_Request[msg_count];
  MPI_Status  *status=new MPI_Status[msg_count];
  for(proc=1;proc<proc_count;proc++) {
    i+=chunk_size;
    chunk_size=base_size+(proc<base_orphan?1:0);
    MPI_Isend(&chunk_size,1,MPI_INT,proc,1,MPI_COMM_WORLD,
	      request+3*(proc-1));
    MPI_Isend(work1_mpi[i],chunk_size*vecsize2,MMS_COMPLEX,proc,2,
	     MPI_COMM_WORLD,request+3*(proc-1)+1);
    MPI_Irecv(work1_mpi[i],chunk_size*vecsize2,MMS_COMPLEX,proc,3,
	     MPI_COMM_WORLD,request+3*(proc-1)+2);
  }
  chunk_size=base_size+(0<base_orphan?1:0);
  for(i=0;i<chunk_size;i++) {
    fft1_mpi.ForwardDecFreq(vecsize2,work1_mpi[i]);
  }
  MPI_Waitall(msg_count,request,status);

  // Unpack and transpose to prepare for FFT in cross dimension.
  //   We fill the first row of work2_mpi with the first and middle
  // columns from unpacked work1_mpi, because these are real-valued.
  for(i=0;i<(rsize1+1)/2;i++) {
    work2_mpi[0][2*i]
      =MyComplex(work1_mpi[i][0].real(),work1_mpi[i][vecsize2/2].real());
    work2_mpi[0][2*i+1]
      =MyComplex(work1_mpi[i][0].imag(),work1_mpi[i][vecsize2/2].imag());
  }
  for(i=2*i;i<vecsize1;i++) work2_mpi[0][i]=MyComplex(0.,0.); // Zero pad
  // Process rest of the rows.
  for(j=1;j<vecsize2/2;j++) {
    for(i=0;i<(rsize1+1)/2;i++) {
      x1=work1_mpi[i][j].real()/2.;
      y1=work1_mpi[i][j].imag()/2.;
      x2=work1_mpi[i][vecsize2-j].real()/2.;
      y2=work1_mpi[i][vecsize2-j].imag()/2.;
      work2_mpi[j][2*i]=MyComplex(x1+x2,y1-y2);
      work2_mpi[j][2*i+1]=MyComplex(y1+y2,x2-x1);
    }
    for(i=2*i;i<vecsize1;i++) work2_mpi[j][i]=MyComplex(0.,0.); // Zero pad
  }


  // Do FFT's on transposed matrix
  Mmsolve_MpiWakeUp(ForwardFFT2_mpi_slave_b);
  whole_size=vecsize2/2;
  base_size=whole_size/proc_count;
  base_orphan=whole_size%proc_count;
  i=0;
  chunk_size=base_size+(0<base_orphan?1:0);
  msg_count=3*(proc_count-1);
  for(proc=1;proc<proc_count;proc++) {
    i+=chunk_size;
    chunk_size=base_size+(proc<base_orphan?1:0);
    MPI_Isend(&chunk_size,1,MPI_INT,proc,1,MPI_COMM_WORLD,
	      request+3*(proc-1));
    MPI_Isend(work2_mpi[i],chunk_size*vecsize1,MMS_COMPLEX,proc,2,
	     MPI_COMM_WORLD,request+3*(proc-1)+1);
    MPI_Irecv(work2_mpi[i],chunk_size*vecsize1,MMS_COMPLEX,proc,3,
	     MPI_COMM_WORLD,request+3*(proc-1)+2);
  }
  chunk_size=base_size+(0<base_orphan?1:0);
  for(i=0;i<chunk_size;i++) {
    fft2_mpi.ForwardDecFreq(vecsize1,work2_mpi[i]);
  }
  MPI_Waitall(msg_count,request,status);
  delete[] request;
  delete[] status;


  // Un-transpose and pack into carr.
  //  First row of work2_mpi needs to be handled separately, because
  // of unique packing described above.
  carr[0][0]=MyComplex(work2_mpi[0][0].real(),0.);
  carr[0][vecsize2/2]=MyComplex(work2_mpi[0][0].imag(),0.);
  carr[vecsize1/2][0]=MyComplex(work2_mpi[0][vecsize1/2].real(),0.);
  carr[vecsize1/2][vecsize2/2]=MyComplex(work2_mpi[0][vecsize1/2].imag(),0.);
  for(i=1;i<vecsize1/2;i++) {
    x1=work2_mpi[0][i].real()/2.;
    y1=work2_mpi[0][i].imag()/2.;
    x2=work2_mpi[0][vecsize1-i].real()/2.;
    y2=work2_mpi[0][vecsize1-i].imag()/2.;
    carr[i][0]=MyComplex(x1+x2,y1-y2);
    carr[i][vecsize2/2]=MyComplex(y1+y2,x2-x1);
  }
  // Process remaining rows
  for(j=1;j<vecsize2/2;j++) {
    carr[0][j]=work2_mpi[j][0];
    carr[0][csize2-j]=conj(work2_mpi[j][0]);
    for(i=1;i<vecsize1/2;i++)
      carr[i][j]=work2_mpi[j][i];
    carr[i][j]=work2_mpi[j][i];
    carr[i][csize2-j]=conj(work2_mpi[j][i]);
    for(i=csize1;i<vecsize1;i++)
      carr[vecsize1-i][csize2-j]=conj(work2_mpi[j][i]);
  }
}

void FFTReal2D_mpi::Inverse(int csize1,int csize2,
			    const MyComplex* const* carr,
			    int rsize1,int rsize2,double** rarr)
{
  // Initialization
  int i,j;
  int vecsize1=OC_MAX(1,2*(csize1-1));
  int vecsize2=csize2;
  FFT_REAL_TYPE x1,y1,x2,y2; // FFT unpacking scratch vars
  for(i=vecsize1;i>2;i/=2)
    if(i%2!=0) PlainError(1,"Error in FFTReal2D_mpi::Inverse(int): "
			  "Requested csize1 - 1 (%d - 1) is not a power of 2",
			  csize1);
  for(j=vecsize2;j>2;j/=2)
    if(j%2!=0) PlainError(1,"Error in FFTReal2D_mpi::Inverse(int): "
			  "Requested csize2 (%d) is not a power of 2",
			  csize2);
  if(vecsize1==0 || vecsize2==0) return; // Nothing to do
  SetupMemory_mpi(vecsize1,vecsize2);


  // Do row inverse FFT's
  // Handle the first & csize1'th row specially.  These rows are
  // the DFT's of real sequences, so they each satisfy the conjugate
  // symmetry condition
  workarr_mpi[0][0]=MyComplex(carr[0][0].real(),carr[csize1-1][0].real());
  for(j=1;j<csize2/2;j++) {
    x1=carr[0][j].real();         y1=carr[0][j].imag();
    x2=carr[csize1-1][j].real();  y2=carr[csize1-1][j].imag();
    workarr_mpi[0][j]        = MyComplex(x1-y2,x2+y1);
    workarr_mpi[0][csize2-j] = MyComplex(x1+y2,x2-y1);
  }
  workarr_mpi[0][csize2/2]=MyComplex(carr[0][csize2/2].real(),
				     carr[csize1-1][csize2/2].real());
  fft2_mpi.InverseDecTime(csize2,workarr_mpi[0],1.);

  // iFFT the remaining rows
  for(i=1;i<csize1-1;i++) {
    for(j=0;j<csize2;j++) workarr_mpi[i][j]=carr[i][j];
    fft2_mpi.InverseDecTime(csize2,workarr_mpi[i],1.);
  }

  // Now do iFFT's on columns.  These are conj. symmetric, so we
  // process them 2 at a time.  Also, recall the 1st row of workarr
  // contains the iFFT's of the 1st and csize1'th row of the given carr.
  //   Note that csize is guaranteed divisible by 2, so if rsize2 is odd
  // then rsize2+1<=csize2.
  for(j=0;j<rsize2;j+=2) {
    scratch_mpi[0]=
      MyComplex(workarr_mpi[0][j].real(),workarr_mpi[0][j+1].real());
    scratch_mpi[csize1-1]=
      MyComplex(workarr_mpi[0][j].imag(),workarr_mpi[0][j+1].imag());
    for(i=1;i<csize1-1;i++) {
      x1 =workarr_mpi[i][j].real();
      y1 =workarr_mpi[i][j].imag();
      x2 =workarr_mpi[i][j+1].real();
      y2 =workarr_mpi[i][j+1].imag();
      scratch_mpi[i]          = MyComplex(x1-y2,x2+y1);
      scratch_mpi[vecsize1-i] = MyComplex(x1+y2,x2-y1);
    }
    fft1_mpi.InverseDecTime(vecsize1,scratch_mpi,
			    FFT_REAL_TYPE(vecsize1*vecsize2));
    if(j+1<rsize2) {
      for(i=0;i<rsize1;i++) {
	rarr[i][j]=scratch_mpi[i].real();
	rarr[i][j+1]=scratch_mpi[i].imag();
      }
    } else {
      for(i=0;i<rsize1;i++) rarr[i][j]=scratch_mpi[i].real();
    }
  }
}


#endif /* USE_MPI */
