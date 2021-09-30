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
/// versions of the basic math library functions.

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
  OC_REAL8m error = 1.0 - x*x;  error -= y*y;  error -= z*z;
  OC_REAL8m orig_magsq = 1.0 - error;
  if(error >= 1.0) {
    Random(1.0);
    error = 1.0 - x*x;    error -= y*y;    error -= z*z;
  }
  if(fabs(error)<=2*OC_REAL8_EPSILON) {
    // In general, we can't do better than this, so we might as
    // well stop.  Moreover, kicking out on this condition helps
    // limit "chatter," i.e., without this check repeated calls
    // into this routine will yield slightly different results,
    // causing (x,y,z) to wander around like a drunk looking for
    // his house keys.
    return orig_magsq;
  } else if(fabs(error)>OC_CUBE_ROOT_REAL8_EPSILON) {
    // Big error; fall back to making a sqrt evaluation.
    // OC_REAL8_EPSILON is about 2e-16 with IEEE floating pt,
    // so OC_CUBE_ROOT_REAL8_EPSILON is 6e-6.  See mjd NOTES II,
    // 10-Feb-2001, p 91.
    OC_REAL8m mult = 1.0/sqrt(1.0-error);
    x*=mult; y*=mult; z*=mult;
    error = 1.0 - x*x;    error -= y*y;    error -= z*z;
  }

  // Several terms of 1/sqrt(1-error) - 1 Taylor series
  OC_REAL8m adj = ((0.3125*error+0.375)*error+0.5)*error;

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

void Oxs_ThreeVector::PerturbDirection(OC_REAL8m eps)
{ // Perturb the direction of *this by a random amount.  The magnitude
  // of the perturbation is no more than eps (>=0).  The norm of *this
  // is unchanged.

  // Note: This code assumes that |*this|^2 and |*this + eps_offset|^2
  // don't overflow.

  // Dummy implementation.  When possible, redo with proper probability
  // distribution.
  OC_REAL8m magsq = MagSq();
  x += eps*(2*Oc_UnifRand()-1);
  y += eps*(2*Oc_UnifRand()-1);
  z += eps*(2*Oc_UnifRand()-1);

  OC_REAL8m newmagsq = MagSq();

  if(newmagsq>=1.0 || OC_REAL8m_MAX*newmagsq>magsq) {
    OC_REAL8m mult = sqrt(magsq/newmagsq);
    x*=mult; y*=mult; z*=mult;
  } else {
    Random(sqrt(magsq));
  }
}


#if OC_USE_SSE
void Oxs_ThreeVectorPairMakeUnit
(__m128d& tx,__m128d& ty,__m128d& tz,
 OC_REAL8m* magsq0, OC_REAL8m* magsq1)
{
  __m128d txsq = _mm_mul_pd(tx,tx);
  __m128d error = _mm_sub_pd(_mm_set1_pd(1.0),txsq);
  __m128d tysq = _mm_mul_pd(ty,ty);
  error = _mm_sub_pd(error,tysq);
  __m128d tzsq = _mm_mul_pd(tz,tz);
  error = _mm_sub_pd(error,tzsq);

  __m128d tmagsq = _mm_sub_pd(_mm_set1_pd(1.0),error);

  if(magsq0) _mm_store_sd(magsq0,tmagsq);
  if(magsq1) _mm_storeh_pd(magsq1,tmagsq);

  // cmpZ is a two-bit mask; bit 0 is low term result,
  // bit 1 is high term result.
  int cmpZ = _mm_movemask_pd(_mm_cmple_pd(tmagsq,_mm_setzero_pd()));
  if(cmpZ != 0) { // At least one term is non-positive
    ThreeVector t0,t1;
    if(cmpZ & 0x1) { // Low term <= 0
      t0.Random(1.0);
    } else {
      _mm_store_sd(&t0.x,tx);
      _mm_store_sd(&t0.y,ty);
      _mm_store_sd(&t0.z,tz);
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
    error = _mm_sub_pd(_mm_set1_pd(1.0),txsq);
    ty   = _mm_set_pd(t1.y,t0.y);
    tysq = _mm_mul_pd(ty,ty);
    error = _mm_sub_pd(error,tysq);
    tz   = _mm_set_pd(t1.z,t0.z);
    tzsq = _mm_mul_pd(tz,tz);
    error = _mm_sub_pd(error,tzsq);
    tmagsq = _mm_sub_pd(_mm_set1_pd(1.0),error);
  }

  // Compute abs(error) by clearing sign bits.
  OC_SSE_MASK absmask;
  absmask.imask = (~(OC_UINT8(0)))>>1; // 0x7FFFFFFFFFFFFFFF
  __m128d abserror = _mm_and_pd(error,_mm_set1_pd(absmask.dval));

  // cmpA and cmpB below are two-bit masks.  Bit 0 records the state
  // of the lower value comparison (vec0) and bit 1 records the state
  // of the upper value comparison (vec1).
  __m128d epsmask = _mm_cmple_pd(abserror,_mm_set1_pd(2*OC_REAL8_EPSILON));
  int cmpA = _mm_movemask_pd(epsmask);

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
    error = _mm_sub_pd(ones,txsq);
    ty = _mm_mul_pd(ty,wimag);    tysq = _mm_mul_pd(ty,ty);
    error = _mm_sub_pd(error,tysq);
    tz = _mm_mul_pd(tz,wimag);    tzsq = _mm_mul_pd(tz,tz);
    error = _mm_sub_pd(error,tzsq);
    tmagsq = _mm_sub_pd(ones,error);
    // Note: abserror not used below
  }

  // Zero-out error on term (if any) that has initial error
  // smaller than 2*OC_REAL8_EPSILON.  This reduces chatter
  error = _mm_andnot_pd(epsmask,error);

  // Several terms of 1/sqrt(1-error) - 1 Taylor series:
  //    ((0.3125*error+0.375)*error+0.5)*error;
  __m128d adj = _mm_mul_pd(_mm_set1_pd(0.3125),error);
  adj = _mm_mul_pd(_mm_add_pd(adj,_mm_set1_pd(0.375)),error);
  adj = _mm_mul_pd(_mm_add_pd(adj,_mm_set1_pd(0.5)),error);

#if OC_FMA_TYPE == 3
  tx = _mm_fmadd_pd(adj,tx,tx);
  ty = _mm_fmadd_pd(adj,ty,ty);
  tz = _mm_fmadd_pd(adj,tz,tz);
#elif OC_FMA_TYPE == 4
  tx = _mm_macc_pd(adj,tx,tx);
  ty = _mm_macc_pd(adj,ty,ty);
  tz = _mm_macc_pd(adj,tz,tz);
#else
  tx = _mm_add_pd(tx,_mm_mul_pd(tx,adj));
  ty = _mm_add_pd(ty,_mm_mul_pd(ty,adj));
  tz = _mm_add_pd(tz,_mm_mul_pd(tz,adj));
#endif
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
  Oc_Duet error = Oc_Duet(1.0) - tx*tx; error -= ty*ty; error -= tz*tz;
  Oc_Duet orig_magsq = Oc_Duet(1.0) - error;

  if(magsq0) orig_magsq.StoreA(*magsq0);
  if(magsq1) orig_magsq.StoreB(*magsq1);

  if(error.GetA()>=1.0) {
    ThreeVector tmp;   tmp.Random(1.0);
    tx.Set(tmp.x,tx.GetB()); ty.Set(tmp.y,ty.GetB()); tz.Set(tmp.z,tz.GetB());
    OC_REAL8m terr = 1.0 - tmp.x*tmp.x;
    terr -= tmp.y*tmp.y;  terr -= tmp.z*tmp.z;
    error.Set(terr,error.GetB());
  }
  if(error.GetB()>=1.0) {
    ThreeVector tmp;   tmp.Random(1.0);
    tx.Set(tx.GetA(),tmp.x); ty.Set(ty.GetA(),tmp.y); tz.Set(tz.GetA(),tmp.z);
    OC_REAL8m terr = 1.0 - tmp.x*tmp.x;
    terr -= tmp.y*tmp.y;  terr -= tmp.z*tmp.z;
    error.Set(error.GetA(),terr);
  }

  if(fabs(error.GetA())<=2*OC_REAL8_EPSILON &&
     fabs(error.GetB())<=2*OC_REAL8_EPSILON) {
    // In general, it is not possible to do better than 2*eps.
    return;
  }
  if(fabs(error.GetA())<=2*OC_REAL8_EPSILON) {
    // Short-circuit correction; this reduces chatter
    error.Set(0.0,error.GetB());
  } else if(fabs(error.GetA())>OC_CUBE_ROOT_REAL8_EPSILON) {
    OC_REAL8m mult = 1.0/sqrt(1.0-error.GetA());
    OC_REAL8m ax = tx.GetA() * mult;    tx.Set(ax,tx.GetB());
    OC_REAL8m ay = ty.GetA() * mult;    ty.Set(ay,ty.GetB());
    OC_REAL8m az = tz.GetA() * mult;    tz.Set(az,tz.GetB());
    OC_REAL8m aerror = 1.0 - ax*ax;   aerror -= ay*ay;   aerror -= az*az;
    error.Set(aerror,error.GetB());
  }
  if(fabs(error.GetB())<=2*OC_REAL8_EPSILON) {
    // Short-circuit Halley step; this reduces chatter
    error.Set(error.GetA(),0.0);
  } else if(fabs(error.GetB())>OC_CUBE_ROOT_REAL8_EPSILON) {
    OC_REAL8m mult = 1.0/sqrt(1.0-error.GetB());
    OC_REAL8m bx = tx.GetB() * mult;    tx.Set(tx.GetA(),bx); 
    OC_REAL8m by = ty.GetB() * mult;    ty.Set(ty.GetA(),by);
    OC_REAL8m bz = tz.GetB() * mult;    tz.Set(tz.GetA(),bz);
    OC_REAL8m berror = 1.0 - bx*bx;   berror -= by*by;   berror -= bz*bz;
    error.Set(error.GetA(),berror);
  }

  // Several terms of 1/sqrt(1-error) - 1 Taylor series
  Oc_Duet adj = ((Oc_Duet(0.3125)*error
                  +Oc_Duet(0.375))*error
                 +Oc_Duet(0.5))*error;
  tx += adj*tx;   ty += adj*ty;   tz += adj*tz;
  /// v += adj*v produces smaller | ||v||^2 - 1 | than
  /// v *= (1+adj).
  return;
}
#endif // !OC_USE_SSE
