/* FILE: threevector.cc        -*-Mode: c++-*-
 *
 * Simple 3D vector class.  *All* client code should use the
 * ThreeVector typedef in case this class gets moved into an
 * OOMMF extension library.
 *
 */

#include "nb.h"
#include "threevector.h"

OC_USE_STD_NAMESPACE;  // Specify std namespace, if supported.
/// For some compilers this is needed to get "long double"
/// versions of the basic math library functions, e.g.,
/// long double atan(long double);

/* End includes */


void Oxs_ThreeVector::SetMag(OC_REAL8m mag)
{ // Adjusts size to "mag".  We assume MagSq() doesn't over/underflow.
  // This is probably okay for our intended uses, but if we want to
  // bullet-proof we should do some preemptive prescaling.
  // Cf. James L. Blue, "A Portable Fortran Program to Find the
  // Euclidean Norm of a Vector," ACM Transactions on Mathematical
  // Software, 4, 15-23 (1978).
  OC_REAL8m magsq=MagSq();
  if(magsq<=0) {
    Random(mag);
  } else {
    OC_REAL8m mult = mag/sqrt(magsq);
    x*=mult; y*=mult; z*=mult;
  }
}

OC_REAL8m Oxs_ThreeVector::MakeUnit()
{ // Conceptually equivalent to SetMag(1.0), but
  // with enhanced accuracy.
  OC_REAL8m orig_magsq=MagSq();
  OC_REAL8m magsq = orig_magsq;
  if(magsq<=0) {
    Random(1.0);
    magsq=MagSq();
  }
  OC_REAL8m error = 1-magsq;
  if(fabs(error)<=2*OC_REAL8_EPSILON) {
    // In general, we can't do better than this, so we might as
    // well stop.  Moreover, kicking out on this condition helps
    // limit "chatter," i.e., without this check repeated calls
    // into this routine will yield slightly different results,
    // causing (x,y,z) to wander around like a drunk looking for
    // his house keys.
    return orig_magsq;
  }
  else if(fabs(error)>OC_CUBE_ROOT_REAL8_EPSILON) {
    // Big error; fall back to making a sqrt evaluation.
    // OC_REAL8_EPSILON is about 2e-16 with IEEE floating pt,
    // so OC_CUBE_ROOT_REAL8_EPSILON is 6e-6.  See mjd NOTES II,
    // 10-Feb-2001, p 91.
    OC_REAL8m mult = 1.0/sqrt(magsq);
    x*=mult; y*=mult; z*=mult;
    magsq=MagSq();
    error = 1-magsq;
  }

  // Do one Halley step (see mjd's NOTES II, 8-Feb-2001, p82)
  // to improve accuracy to <= 2*OC_REAL8_EPSILON
  OC_REAL8m adj = 2*error/(1+3*magsq);
  x += adj*x;   y += adj*y;   z += adj*z;
  /// v += adj*v produces smaller | ||v||^2 - 1 | than
  /// v *= (1+adj).

  return orig_magsq;
}

void Oxs_ThreeVector::Random(OC_REAL8m mag)
{ // Makes a random vector of size "mag"
  OC_REAL8m r=2*Oc_UnifRand()-1;
  OC_REAL8m theta=r*PI;

  OC_REAL8m costheta=1.0,sintheta=0.0;

  if(fabs(r)>0.25 && fabs(r)<0.75) {
    // cosine is small and accurate
    costheta=cos(theta);
    sintheta=sqrt(OC_MAX(0.,1.-OC_SQ(costheta)));
    if(r<0) sintheta *= -1;
  } else {
    // sine is small and accurate
    sintheta=sin(theta);
    costheta=sqrt(OC_MAX(0.,1.-OC_SQ(sintheta)));
    if(fabs(r)>0.5) costheta *= -1;
  }

  OC_REAL8m cosphi=1-2*Oc_UnifRand();
  OC_REAL8m sinphi=sqrt(OC_MAX(0.,1.-OC_SQ(cosphi)));

  x=mag*sinphi*costheta;
  y=mag*sinphi*sintheta;
  z=mag*cosphi;
}

#if OC_USE_SSE
void Oxs_ThreeVectorPairMakeUnit
(__m128d& tx,__m128d& ty,__m128d& tz,
 OC_REAL8m* magsq0, OC_REAL8m* magsq1)
{
  __m128d txsq = _mm_mul_pd(tx,tx);
  __m128d tysq = _mm_mul_pd(ty,ty);
  __m128d tzsq = _mm_mul_pd(tz,tz);

  __m128d tmagsq = _mm_add_pd(_mm_add_pd(txsq,tysq),tzsq);

  if(magsq0) _mm_storel_pd(magsq0,tmagsq);
  if(magsq1) _mm_storeh_pd(magsq1,tmagsq);

  // cmpZ is a two-bit mask; bit 0 is low term result,
  // bit 1 is high term result.
  int cmpZ = _mm_movemask_pd(_mm_cmple_pd(tmagsq,_mm_setzero_pd()));
  if(cmpZ != 0) { // At least one term is non-positive
    ThreeVector t0,t1;
    if(cmpZ & 0x1) { // Low term <= 0
      t0.Random(1.0);
    } else {
      _mm_storel_pd(&t0.x,tx);
      _mm_storel_pd(&t0.y,ty);
      _mm_storel_pd(&t0.z,tz);
    }
    if(cmpZ & 0x2) { // High term <= 0
      t1.Random(1.0);
    } else {
      _mm_storeh_pd(&t1.x,tx);
      _mm_storeh_pd(&t1.y,ty);
      _mm_storeh_pd(&t1.z,tz);
    }
    tx   = _mm_set_pd(t1.x,t0.x);  // Recompute
    txsq = _mm_mul_pd(tx,tx);
    ty   = _mm_set_pd(t1.y,t0.y);
    tysq = _mm_mul_pd(ty,ty);
    tz   = _mm_set_pd(t1.z,t0.z);
    tzsq = _mm_mul_pd(tz,tz);
    tmagsq = _mm_add_pd(_mm_add_pd(txsq,tysq),tzsq);
  }

  __m128d error = _mm_sub_pd(_mm_set1_pd(1.0),tmagsq);
  // Compute abs(error) by clearing sign bits.
  OC_SSE_MASK absmask;
  absmask.imask = (~(OC_UINT8(0)))>>1; // 0x7FFFFFFFFFFFFFFF
  __m128d abserror = _mm_and_pd(error,_mm_set1_pd(absmask.dval));

  // cmpA and cmpB below are two-bit masks.  Bit 0 records the state
  // of the lower value comparison (vec0) and bit 1 records the state
  // of the upper value comparison (vec1).
  int cmpA = _mm_movemask_pd(_mm_cmple_pd(abserror,
                                         _mm_set1_pd(2*OC_REAL8_EPSILON)));

  if(cmpA == 0x3) {
    // In both cases, error<2*OC_REAL8_EPSILON.  In general, we can't do
    // better than this, so we might as well stop.
    return;
  }

  __m128d gtmask = _mm_cmpgt_pd(abserror,
                                _mm_set1_pd(OC_CUBE_ROOT_REAL8_EPSILON));
  int cmpB = _mm_movemask_pd(gtmask);

  if(cmpB != 0x0) {
    // At least one of vectors has mag far from one.  Fall back to
    // a sqrt evaluation on at least one.

    __m128d ones = _mm_set1_pd(1.0);

    // Use gtmask to select which part or parts of vec need to be
    // renormalized by sqrt.  Don't renormalized except where required,
    // because Halley step below can provide full accuracy and extra
    // renormalization just introduces additional rounding error.

    // If a term has norm close to 1, then change corresponding magsq
    // scaling factor to 1.0, so subsequent scaling below effects no
    // change.  If a term has norm far from 1, then the next three lines
    // leave the magsq scaling unchanged.  Use logical ops here to avoid
    // branches (which are slow if mispredicted).
    __m128d workmagsq = _mm_and_pd(gtmask,tmagsq);
    __m128d workones  = _mm_andnot_pd(gtmask,ones);
    workmagsq = _mm_add_pd(workmagsq,workones);

    __m128d wmag = _mm_sqrt_pd(workmagsq);
    __m128d wimag = _mm_div_pd(ones,wmag);
    tx = _mm_mul_pd(tx,wimag);    txsq = _mm_mul_pd(tx,tx);
    ty = _mm_mul_pd(ty,wimag);    tysq = _mm_mul_pd(ty,ty);
    tz = _mm_mul_pd(tz,wimag);    tzsq = _mm_mul_pd(tz,tz);
    tmagsq = _mm_add_pd(_mm_add_pd(txsq,tysq),tzsq);
    error = _mm_sub_pd(_mm_set1_pd(1.0),tmagsq);
    // Note: abserror not used below
  }

  // Do one Halley step (see mjd's NOTES II, 8-Feb-2001, p82)
  // to improve accuracy to <= 2*OC_REAL8_EPSILON
  __m128d denom = _mm_add_pd(_mm_set1_pd(1.0),
                             _mm_mul_pd(_mm_set1_pd(3.0),tmagsq));
  __m128d adj = _mm_div_pd(_mm_mul_pd(_mm_set1_pd(2.0),error),denom);
  tx = _mm_add_pd(tx,_mm_mul_pd(tx,adj));
  ty = _mm_add_pd(ty,_mm_mul_pd(ty,adj));
  tz = _mm_add_pd(tz,_mm_mul_pd(tz,adj));
  /// v += adj*v produces smaller | ||v||^2 - 1 | than
  /// v *= (1+adj).
}
#endif // OC_USE_SSE

void Oxs_ThreeVectorPairMakeUnit
(Oxs_ThreeVector& vec0,
 Oxs_ThreeVector& vec1,
 OC_REAL8m* magsq0, OC_REAL8m* magsq1)
{
#if !OC_USE_SSE
  OC_REAL8m dum0 = vec0.MakeUnit();
  OC_REAL8m dum1 = vec1.MakeUnit();
  if(magsq0) *magsq0 = dum0;
  if(magsq1) *magsq1 = dum1;
#else
  __m128d tx   = _mm_set_pd(vec1.x,vec0.x);
  __m128d ty   = _mm_set_pd(vec1.y,vec0.y);
  __m128d tz   = _mm_set_pd(vec1.z,vec0.z);
  Oxs_ThreeVectorPairMakeUnit(tx,ty,tz,vec0,vec1,magsq0,magsq1);
#endif
}

#if !OC_USE_SSE
// Note: The SSE version of this code is inlined in threevector.h
void Oxs_ThreeVectorPairMakeUnit
(Oc_Duet& tx,Oc_Duet& ty,Oc_Duet& tz,
 OC_REAL8m* magsq0, OC_REAL8m* magsq1)
{ // Conceptually equivalent to SetMag(1.0), but
  // with enhanced accuracy.
  Oc_Duet orig_magsq = tx*tx + ty*ty + tz*tz;
  Oc_Duet magsq = orig_magsq;

  if(magsq0) orig_magsq.StoreA(*magsq0);
  if(magsq1) orig_magsq.StoreB(*magsq1);

  if(magsq.GetA()<=0.0) {
    ThreeVector tmp;   tmp.Random(1.0);
    tx.Set(tmp.x,tx.GetB()); ty.Set(tmp.y,ty.GetB()); tz.Set(tmp.z,tz.GetB());
    magsq.Set(tmp.MagSq(),magsq.GetB());
  }
  if(magsq.GetB()<=0.0) {
    ThreeVector tmp;   tmp.Random(1.0);
    tx.Set(tx.GetA(),tmp.x); ty.Set(ty.GetA(),tmp.y); tz.Set(tz.GetA(),tmp.z);
    magsq.Set(magsq.GetA(),tmp.MagSq());
  }

  Oc_Duet error = Oc_Duet(1.0) - magsq;
  if(Oc_Fabs(error.GetA())<=2*OC_REAL8_EPSILON &&
     Oc_Fabs(error.GetB())<=2*OC_REAL8_EPSILON) {
    // In general, it is not possible to do better than 2*eps.
    return;
  }
  if(Oc_Fabs(error.GetA())>OC_CUBE_ROOT_REAL8_EPSILON ||
     Oc_Fabs(error.GetB())>OC_CUBE_ROOT_REAL8_EPSILON) {
    // Big error.  Make a sqrt evaluation.  Since this is a non-sse code
    // section, we might want to check the A and B terms individually,
    // and only take sqrt termwise as needed.
    Oc_Duet mult = Oc_Invert(Oc_Sqrt(magsq));
    tx *= mult; ty *= mult; tz *= mult;
    magsq = tx*tx + ty*ty + tz*tz;
    error = Oc_Duet(1.0) - magsq;
  }

  // Do one Halley step (see mjd's NOTES II, 8-Feb-2001, p82)
  // to improve accuracy to <= 2*OC_REAL8_EPSILON
  Oc_Duet adj = 2*error/(Oc_Duet(1.0)+3*magsq);
  tx += adj*tx;   ty += adj*ty;   tz += adj*tz;
  /// v += adj*v produces smaller | ||v||^2 - 1 | than
  /// v *= (1+adj).
  return;
}
#endif // !OC_USE_SSE
