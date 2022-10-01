/* FILE: test-sse.cc               -*-Mode: c++-*-
 *
 * Debug test code for Oc_Duet and SSE processing.  The tests are not
 * comprehensive; extend as needed.
 * Build with a command like:
 *
 *   g++ -std=c++11 -Ilinux-x86_64 test-sse.cc -o test-sse
 *
 * Adjust compiler flags to match pimake.  Then run with
 *
 *   ./test-sse
 *
 * and examine the output closely --- at this time the results are not
 * automatically checked, you need to do that by eye.
 *
 */

#include <cstdio>
#include <cstdlib>

#include "ocsse.h"

int main(int,char**)
{
  Oc_Duet A(1.1,-2.2),B,C,D;
  B.Set(3.3,4.4);
  C.Set(-5.5);

#ifdef OC_USE_SSE
  printf("Control macro OC_USE_SSE=%d\n",OC_USE_SSE);
#else
  printf("Macro OC_USE_SSE not set\n");
#endif

  double* Adata = reinterpret_cast<double*>(&A);
  printf("A data: (%25.16e,%25.16e)\n",Adata[0],Adata[1]);

  double Ebuf[3];
  double *Eptra=0,*Eptru=0;
  OC_UINDEX Eoff = reinterpret_cast<OC_UINDEX>(Ebuf);
  switch(Eoff%16) {
  case 0:
    Eptra = Ebuf;  // Aligned
    Eptru = Ebuf+1;  // Aligned
    break;
  case 8:
    Eptra = Ebuf+1;  // Aligned
    Eptru = Ebuf;    // Unaligned
    break;
  default:
    fprintf(stderr,"Bad double array offset: %lX\n",Eoff);
    break;
  }
  A.StoreUnaligned(*Eptru);
  printf("A unaligned store: (%25.16e,%25.16e)\n",Eptru[0],Eptru[1]);
  A.StoreAligned(*Eptra);
  printf("A  aligned  store: (%25.16e,%25.16e)\n",Eptra[0],Eptra[1]);

  printf("A Get lower: %25.16e\n",A.GetA());
  printf("A Get upper: %25.16e\n",A.GetB());

  
  Eptru[0] =  11.11;
  Eptru[1] = -22.22;
  D.LoadUnaligned(*Eptru);
  double* Ddata = reinterpret_cast<double*>(&D);
  printf("D unaligned load: (%25.16e,%25.16e)\n",Ddata[0],Ddata[1]);
  
  Eptra[0] =  33.33;
  Eptra[1] = -44.44;
  D.LoadAligned(*Eptra);
  printf("D  aligned  load: (%25.16e,%25.16e)\n",Ddata[0],Ddata[1]);


  D = A + B;
  printf("(%25.16e,%25.16e) + (%25.16e,%25.16e) = (%25.16e,%25.16e)\n",
         A.GetA(),A.GetB(),B.GetA(),B.GetB(),D.GetA(),D.GetB());

  D = A + C;
  printf("(%25.16e,%25.16e) + (%25.16e,%25.16e) = (%25.16e,%25.16e)\n",
         A.GetA(),A.GetB(),C.GetA(),C.GetB(),D.GetA(),D.GetB());

  D = A - B;
  printf("(%25.16e,%25.16e) - (%25.16e,%25.16e) = (%25.16e,%25.16e)\n",
         A.GetA(),A.GetB(),B.GetA(),B.GetB(),D.GetA(),D.GetB());

  D = A * B;
  printf("(%25.16e,%25.16e) * (%25.16e,%25.16e) = (%25.16e,%25.16e)\n",
         A.GetA(),A.GetB(),B.GetA(),B.GetB(),D.GetA(),D.GetB());

  D = A / B;
  printf("(%25.16e,%25.16e) / (%25.16e,%25.16e) = (%25.16e,%25.16e)\n",
         A.GetA(),A.GetB(),B.GetA(),B.GetB(),D.GetA(),D.GetB());


  __m128d xa = _mm_set_pd(1.1,2.2);
  __m128d xb = _mm_set_pd(3.3,4.4);
  __m128d xc = _mm_unpackhi_pd(xa,xb);
  __m128d xd = _mm_unpackhi_pd(xa,xa);
  double* xptr = reinterpret_cast<double*>(&xc);
  printf("X test: (%25.16e,%25.16e)\n",xptr[0],xptr[1]);
  printf("Xlo: %25.16e\n",oc_sse_cvtsd_f64(xc));

  double* yptr = reinterpret_cast<double*>(&xd);
  printf("Y test: (%25.16e,%25.16e)\n",yptr[0],yptr[1]);
  printf("Ylo: %25.16e\n",oc_sse_cvtsd_f64(xd));

  return 0;
}
