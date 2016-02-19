/* FILE: demagcoef.h            -*-Mode: c++-*-
 *
 * Demag coefficients.
 *
 * Constant magnetization demag routines, based on formulae presented in
 * "A Generalization of the Demagnetizing Tensor for Nonuniform
 * Magnetization," by Andrew J. Newell, Wyn Williams, and David
 * J. Dunlop, Journal of Geophysical Research, vol 98, p 9551-9555, June
 * 1993.  This formulae clearly satisfy necessary symmetry and scaling
 * properties, which is not true of the formulae presented in
 * "Magnetostatic Energy Calculations in Two- and Three-Dimensional
 * Arrays of Ferromagnetic Prisms," M. Maicas, E. Lopez, M. CC. Sanchez,
 * C. Aroca and P. Sanchez, IEEE Trans Mag, vol 34, May 1998, p601-607.
 * (Side note: The units in the latter paper are apparently cgs.)  It
 * appears likely that there is an error in the latter paper (attempts
 * to implement the formulae presented there did not produce the proper
 * symmetries), as well as in the older paper, "Magnetostatic
 * Interaction Fields for a Three-Dimensional Array of Ferromagnetic
 * Cubes," Manfred E. Schabes and Amikam Aharoni, IEEE Trans Mag, vol
 * 23, November 1987, p3882-3888.  (Note: The Newell paper deals with
 * uniformly sized rectangular prisms, the Maicas paper allows
 * non-uniformly sized rectangular prisms, and the Schabes paper only
 * considers cubes.)
 *
 *   The kernel here is based on an analytically derived energy, and the
 * effective (discrete) demag field is calculated from the (discrete)
 * energy.
 *
 */

#ifndef _OXS_DEMAGCOEF
#define _OXS_DEMAGCOEF

#if defined(STANDALONE) && STANDALONE==0
# undef STANDALONE
#endif

#ifndef STANDALONE
# include "nb.h"
#endif

/* End includes */

// If OXS_DEMAG_REAL_ASYMP is anything other than OC_REAL8m,
// then OXS_DEMAG_ASYMP_USE_SSE should be forced to 0
typedef OC_REAL8m OXS_DEMAG_REAL_ASYMP;

#ifndef OXS_DEMAG_ASYMP_USE_SSE
# if OC_USE_SSE
#  define OXS_DEMAG_ASYMP_USE_SSE 1
   /// Assumes OXS_DEMAG_REAL_ASYMP is OC_REAL8m!
# else
#  define OXS_DEMAG_ASYMP_USE_SSE 0
# endif
#endif

// Maybe these routines should be wrapped up in a class?

OC_REALWIDE Oxs_Newell_f(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z);
OC_REALWIDE Oxs_Newell_g(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z);

inline OC_REALWIDE Oxs_Newell_f_xx(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z) {
  return Oxs_Newell_f(x,y,z);
}
inline OC_REALWIDE Oxs_Newell_f_yy(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z) {
  return Oxs_Newell_f(y,x,z);
}
inline OC_REALWIDE Oxs_Newell_f_zz(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z) {
  return Oxs_Newell_f(z,y,x);
}
inline OC_REALWIDE Oxs_Newell_g_xy(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z) {
  return Oxs_Newell_g(x,y,z);
}
inline OC_REALWIDE Oxs_Newell_g_xz(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z) {
  return Oxs_Newell_g(x,z,y);
}
inline OC_REALWIDE Oxs_Newell_g_yz(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z) {
  return Oxs_Newell_g(y,z,x);
}


OC_REALWIDE
Oxs_CalculateSDA00(OC_REALWIDE x, OC_REALWIDE y, OC_REALWIDE z,
                   OC_REALWIDE dx,OC_REALWIDE dy,OC_REALWIDE dz);

inline OC_REALWIDE
Oxs_CalculateSDA11(OC_REALWIDE x, OC_REALWIDE y, OC_REALWIDE z,
                   OC_REALWIDE dx,OC_REALWIDE dy,OC_REALWIDE dz)
{ return Oxs_CalculateSDA00(y,x,z,dy,dx,dz); }

inline OC_REALWIDE
Oxs_CalculateSDA22(OC_REALWIDE x, OC_REALWIDE y, OC_REALWIDE z,
                   OC_REALWIDE dx,OC_REALWIDE dy,OC_REALWIDE dz)
{ return Oxs_CalculateSDA00(z,y,x,dz,dy,dx); }


OC_REALWIDE
Oxs_CalculateSDA01(OC_REALWIDE x, OC_REALWIDE y, OC_REALWIDE z,
                   OC_REALWIDE dx,OC_REALWIDE dy,OC_REALWIDE dz);

inline OC_REALWIDE
Oxs_CalculateSDA02(OC_REALWIDE x, OC_REALWIDE y, OC_REALWIDE z,
                   OC_REALWIDE dx,OC_REALWIDE dy,OC_REALWIDE dz)
{ return Oxs_CalculateSDA01(x,z,y,dx,dz,dy); }

inline OC_REALWIDE
Oxs_CalculateSDA12(OC_REALWIDE x, OC_REALWIDE y, OC_REALWIDE z,
                   OC_REALWIDE dx,OC_REALWIDE dy,OC_REALWIDE dz)
{ return Oxs_CalculateSDA01(y,z,x,dy,dz,dx); }


OC_REALWIDE
Oxs_SelfDemagNx(OC_REALWIDE xsize,OC_REALWIDE ysize,OC_REALWIDE zsize);

OC_REALWIDE
Oxs_SelfDemagNy(OC_REALWIDE xsize,OC_REALWIDE ysize,OC_REALWIDE zsize);

OC_REALWIDE
Oxs_SelfDemagNz(OC_REALWIDE xsize,OC_REALWIDE ysize,OC_REALWIDE zsize);

////////////////////////////////////////////////////////////////////////
// Classes for coordinate permutations:
struct OxsDemagAdapter {
public:
  OxsDemagAdapter(const OXS_DEMAG_REAL_ASYMP& ix,
                  const OXS_DEMAG_REAL_ASYMP& iy,
                  const OXS_DEMAG_REAL_ASYMP& iz) : x(ix), y(iy), z(iz) {}
  const OXS_DEMAG_REAL_ASYMP& x;
  const OXS_DEMAG_REAL_ASYMP& y;
  const OXS_DEMAG_REAL_ASYMP& z;

  // Disable the following three (otherwise implicit) functions by
  // declaring but not defining them.  These need to be public rather
  // than private as otherwise g++ complains about them being hidden
  // before it figures out it can't use them.
  OxsDemagAdapter();
  OxsDemagAdapter(const OxsDemagAdapter&);
  OxsDemagAdapter& operator=(const OxsDemagAdapter&);
};
struct OxsDemagXZYAdapter : public OxsDemagAdapter {
public:
  OxsDemagXZYAdapter(const OXS_DEMAG_REAL_ASYMP& ix,
                     const OXS_DEMAG_REAL_ASYMP& iy,
                     const OXS_DEMAG_REAL_ASYMP& iz)
    : OxsDemagAdapter(ix,iz,iy) {}
};
struct OxsDemagYXZAdapter : public OxsDemagAdapter {
public:
  OxsDemagYXZAdapter(const OXS_DEMAG_REAL_ASYMP& ix,
                     const OXS_DEMAG_REAL_ASYMP& iy,
                     const OXS_DEMAG_REAL_ASYMP& iz)
    : OxsDemagAdapter(iy,ix,iz) {}
};
struct OxsDemagYZXAdapter : public OxsDemagAdapter {
public:
  OxsDemagYZXAdapter(const OXS_DEMAG_REAL_ASYMP& ix,
                     const OXS_DEMAG_REAL_ASYMP& iy,
                     const OXS_DEMAG_REAL_ASYMP& iz)
    : OxsDemagAdapter(iy,iz,ix) {}
};
struct OxsDemagZXYAdapter : public OxsDemagAdapter {
public:
  OxsDemagZXYAdapter(const OXS_DEMAG_REAL_ASYMP& ix,
                     const OXS_DEMAG_REAL_ASYMP& iy,
                     const OXS_DEMAG_REAL_ASYMP& iz)
    : OxsDemagAdapter(iz,ix,iy) {}
};
struct OxsDemagZYXAdapter : public OxsDemagAdapter {
public:
  OxsDemagZYXAdapter(const OXS_DEMAG_REAL_ASYMP& ix,
                     const OXS_DEMAG_REAL_ASYMP& iy,
                     const OXS_DEMAG_REAL_ASYMP& iz)
    : OxsDemagAdapter(iz,iy,ix) {}
};


////////////////////////////////////////////////////////////////////////
// A couple of data structures for parameter passing.  Main intent
// is to allow caching of reused quantities such as sqrt(x*x+y*y+z*z).
// Intended for internal use only
struct OxsDemagNabData {
public:
  OXS_DEMAG_REAL_ASYMP x,y,z;
  OXS_DEMAG_REAL_ASYMP tx2,ty2,tz2;
  OXS_DEMAG_REAL_ASYMP R,iR;
  OXS_DEMAG_REAL_ASYMP R2,iR2;

  inline void Set(OXS_DEMAG_REAL_ASYMP import_x,
                  OXS_DEMAG_REAL_ASYMP import_y,
                  OXS_DEMAG_REAL_ASYMP import_z);

  static void SetPair(OXS_DEMAG_REAL_ASYMP ixa,
                      OXS_DEMAG_REAL_ASYMP iya,
                      OXS_DEMAG_REAL_ASYMP iza,
                      OXS_DEMAG_REAL_ASYMP ixb,
                      OXS_DEMAG_REAL_ASYMP iyb,
                      OXS_DEMAG_REAL_ASYMP izb,
                      OxsDemagNabData& pta,
                      OxsDemagNabData& ptb);
  // Fills two OxsDemagNabData at one time.  The purpose of this routine
  // is to allow efficient SSE code.

  OxsDemagNabData() : x(0), y(0), z(0),
                      tx2(0), ty2(0), tz2(0),
                      R(0), iR(0), R2(0), iR2(0) {}

  OxsDemagNabData(OXS_DEMAG_REAL_ASYMP import_x,
                  OXS_DEMAG_REAL_ASYMP import_y,
                  OXS_DEMAG_REAL_ASYMP import_z) {
    Set(import_x,import_y,import_z);
  }

  OxsDemagNabData(OXS_DEMAG_REAL_ASYMP import_x,
                  OXS_DEMAG_REAL_ASYMP import_y,
                  OXS_DEMAG_REAL_ASYMP import_z,
                  OXS_DEMAG_REAL_ASYMP import_tx2,
                  OXS_DEMAG_REAL_ASYMP import_ty2,
                  OXS_DEMAG_REAL_ASYMP import_tz2,
                  OXS_DEMAG_REAL_ASYMP import_R,
                  OXS_DEMAG_REAL_ASYMP import_iR,
                  OXS_DEMAG_REAL_ASYMP import_R2,
                  OXS_DEMAG_REAL_ASYMP import_iR2)
    : x(import_x), y(import_y), z(import_z),
      tx2(import_tx2), ty2(import_ty2), tz2(import_tz2),
      R(import_R), iR(import_iR), R2(import_R2), iR2(import_iR2) {}
};

template<class Adapter>
class OxsTDemagNabData : public OxsDemagNabData
{
public:
  OxsTDemagNabData(const OxsDemagNabData& data) : OxsDemagNabData(data)
  {
    // NOTE: "Adapter" works with references, so it is important to
    // initialize a and b off of data rather than (x,y,z) and
    // (tx2,ty2,tz2), because if one did that then the subsequent copies
    // would fail by overwriting some data before it got copied, e.g.,
    // "a.z" could actually be a reference to one of x or y.
    Adapter a(data.x,data.y,data.z);          x = a.x;   y = a.y;   z = a.z;
    Adapter b(data.tx2,data.ty2,data.tz2);  tx2 = b.x; ty2 = b.y; tz2 = b.z;
  }

  // Disable the following functions by declaring but not defining them.
  OxsTDemagNabData();
  OxsTDemagNabData(const OxsTDemagNabData<Adapter>&);
  OxsTDemagNabData<Adapter>& operator=(const OxsTDemagNabData<Adapter>&);
};

struct OxsDemagNabPairData {
public:
  OXS_DEMAG_REAL_ASYMP ubase, uoff;
  OxsDemagNabData ptp;  // ubase + uoff
  OxsDemagNabData ptm;  // ubase - uoff

  OxsDemagNabPairData() : ubase(0), uoff(0) {}

  OxsDemagNabPairData(OXS_DEMAG_REAL_ASYMP import_ubase,
                      OXS_DEMAG_REAL_ASYMP import_uoff,
                      const OxsDemagNabData& import_ptp,
                      const OxsDemagNabData& import_ptm)
    : ubase(import_ubase), uoff(import_uoff),
      ptp(import_ptp), ptm(import_ptm) {}
};

template<class Adapter>
class OxsTDemagNabPairData : public OxsDemagNabPairData
{
public:
  OxsTDemagNabPairData(const OxsDemagNabPairData& data)
    : OxsDemagNabPairData()
  {
    ubase = data.ubase;
    uoff  = data.uoff;
    OxsTDemagNabData<Adapter> tptp(data.ptp);  ptp = tptp;
    OxsTDemagNabData<Adapter> tptm(data.ptm);  ptm = tptm;
  }

  // Disable the following functions by declaring but not defining them.
  OxsTDemagNabPairData();
  OxsTDemagNabPairData(const OxsTDemagNabPairData<Adapter>&);
  OxsTDemagNabPairData<Adapter>& operator=(const OxsTDemagNabPairData<Adapter>&);
};

inline void OxsDemagNabData::Set
(OXS_DEMAG_REAL_ASYMP import_x,
 OXS_DEMAG_REAL_ASYMP import_y,
 OXS_DEMAG_REAL_ASYMP import_z)
{
  x = import_x;  y = import_y;  z = import_z;
  const OXS_DEMAG_REAL_ASYMP x2 = x*x;
  const OXS_DEMAG_REAL_ASYMP y2 = y*y;
  R2 = x2 + y2;
  const OXS_DEMAG_REAL_ASYMP z2 = z*z;
  R2 += z2;
  const OXS_DEMAG_REAL_ASYMP R4 = R2*R2;
  R = Oc_Sqrt(R2);
  if(R2 != 0.0) {
    tx2 = x2/R4;  ty2 = y2/R4;  tz2 = z2/R4;
    iR2 = 1.0/R2;
    iR  = 1.0/R;
  } else {
    // I don't think this branch ever happens
    tx2 = ty2 = tz2 = 0.0;
    iR2 = R = iR = 0.0;
  }
}

#if !OXS_DEMAG_ASYMP_USE_SSE
inline void OxsDemagNabData::SetPair
(OXS_DEMAG_REAL_ASYMP ixa,
 OXS_DEMAG_REAL_ASYMP iya,
 OXS_DEMAG_REAL_ASYMP iza,
 OXS_DEMAG_REAL_ASYMP ixb,
 OXS_DEMAG_REAL_ASYMP iyb,
 OXS_DEMAG_REAL_ASYMP izb,
 OxsDemagNabData& pta,
 OxsDemagNabData& ptb)
{ // Fills two OxsDemagNabData at one time.
  // Non-SSE version:
  pta.Set(ixa,iya,iza);
  ptb.Set(ixb,iyb,izb);
}
#else // !OXS_DEMAG_ASYMP_USE_SSE
inline void OxsDemagNabData::SetPair
(OXS_DEMAG_REAL_ASYMP ixa,
 OXS_DEMAG_REAL_ASYMP iya,
 OXS_DEMAG_REAL_ASYMP iza,
 OXS_DEMAG_REAL_ASYMP ixb,
 OXS_DEMAG_REAL_ASYMP iyb,
 OXS_DEMAG_REAL_ASYMP izb,
 OxsDemagNabData& pta,
 OxsDemagNabData& ptb)
{ // Fills two OxsDemagNabData at one time.  The purpose of this routine
  // is to allow efficient SSE code.

  if((ixa==0.0 && iya==0.0 && iza==0.0) ||
     (ixb==0.0 && iyb==0.0 && izb==0.0)   ) {
    // This code insures against the case where pta.R or ptb.R
    // are zero.  As far as I can tell, this never happens, so
    // this check doesn't do anything other than slow things
    // a bit.
    pta.Set(ixa,iya,iza); ptb.Set(ixb,iyb,izb);
    return;
  }
  // At point we are assured that pta.R and ptb.R are both >0.0

  pta.x = ixa;  pta.y = iya;  pta.z = iza;
  ptb.x = ixb;  ptb.y = iyb;  ptb.z = izb;

  // "a" goes into lower half, "b" into upper half
  Oc_Duet wx2(ixa,ixb); wx2 *= wx2;
  Oc_Duet wy2(iya,iyb); wy2 *= wy2;
  Oc_Duet wR2 = wx2 + wy2;
  Oc_Duet wz2(iza,izb); wz2 *= wz2;
  wR2 += wz2;

  wR2.StoreA(pta.R2); wR2.StoreB(ptb.R2);

  Oc_Duet wR4 = wR2*wR2;

  wx2 /= wR4;  wx2.StoreA(pta.tx2);  wx2.StoreB(ptb.tx2);
  wy2 /= wR4;  wy2.StoreA(pta.ty2);  wy2.StoreB(ptb.ty2);
  wz2 /= wR4;  wz2.StoreA(pta.tz2);  wz2.StoreB(ptb.tz2);

  Oc_Duet wone(1.0);
  Oc_Duet wiR2 = wone/wR2;   wiR2.StoreA(pta.iR2); wiR2.StoreB(ptb.iR2);
  Oc_Duet wR = Oc_Sqrt(wR2); wR.StoreA(pta.R);     wR.StoreB(ptb.R);
  Oc_Duet wiR  = wone/wR;    wiR.StoreA(pta.iR);   wiR.StoreB(ptb.iR);
}
#endif // !OXS_DEMAG_ASYMP_USE_SSE


////////////////////////////////////////////////////////////////////////
// High-order asymptotic approximations
#define OXS_DEMAG_ASYMPTOTIC_DEFAULT_REFINE_RATIO 1.5
struct OxsDemagAsymptoticRefineData {
public:
  OxsDemagAsymptoticRefineData(OXS_DEMAG_REAL_ASYMP in_dx,
                               OXS_DEMAG_REAL_ASYMP in_dy,
                               OXS_DEMAG_REAL_ASYMP in_dz,
                               OXS_DEMAG_REAL_ASYMP in_maxratio);
  OXS_DEMAG_REAL_ASYMP rdx, rdy, rdz;
  OXS_DEMAG_REAL_ASYMP result_scale;
  OC_INT4m xcount,ycount,zcount;
};

class Oxs_DemagNxxAsymptoticBase {
public:
  Oxs_DemagNxxAsymptoticBase(const OxsDemagAsymptoticRefineData& refine_data);

  ~Oxs_DemagNxxAsymptoticBase() {}

  OXS_DEMAG_REAL_ASYMP NxxAsymptotic(const OxsDemagNabData& ptdata) const;

#if OXS_DEMAG_ASYMP_USE_SSE
  OXS_DEMAG_REAL_ASYMP
  NxxAsymptoticPair(const OxsDemagNabData& ptA,const OxsDemagNabData& ptB) const;
#else
  OXS_DEMAG_REAL_ASYMP
  NxxAsymptoticPair(const OxsDemagNabData& ptA,const OxsDemagNabData& ptB) const
  { return NxxAsymptotic(ptA) + NxxAsymptotic(ptB); }
#endif

  OXS_DEMAG_REAL_ASYMP
  NxxAsymptoticPair(const OxsDemagNabPairData& ptdatapair) const {
    return NxxAsymptoticPair(ptdatapair.ptp,ptdatapair.ptm);
  }

  OXS_DEMAG_REAL_ASYMP NxxAsymptotic(OXS_DEMAG_REAL_ASYMP x,
                                     OXS_DEMAG_REAL_ASYMP y,
                                     OXS_DEMAG_REAL_ASYMP z) const {
    OxsDemagNabData ptdata(x,y,z);
    return NxxAsymptotic(ptdata);
  }

private:
  int cubic_cell;
  OXS_DEMAG_REAL_ASYMP self_demag;
  OXS_DEMAG_REAL_ASYMP lead_weight;
  OXS_DEMAG_REAL_ASYMP a1,a2,a3,a4,a5,a6;
  OXS_DEMAG_REAL_ASYMP b1,b2,b3,b4,b5,b6,b7,b8,b9,b10;
  OXS_DEMAG_REAL_ASYMP c1,c2,c3,c4,c5,c6,c7,c8,c9,c10,c11,c12,c13,c14,c15;

  // The following members are declared but not defined:
  Oxs_DemagNxxAsymptoticBase(const Oxs_DemagNxxAsymptoticBase&);
  Oxs_DemagNxxAsymptoticBase& operator=(const Oxs_DemagNxxAsymptoticBase&);
};

class Oxs_DemagNxxAsymptotic {
public:
  Oxs_DemagNxxAsymptotic(OXS_DEMAG_REAL_ASYMP dx,
                         OXS_DEMAG_REAL_ASYMP dy,
                         OXS_DEMAG_REAL_ASYMP dz,
                         OXS_DEMAG_REAL_ASYMP maxratio
                         = OXS_DEMAG_ASYMPTOTIC_DEFAULT_REFINE_RATIO)
    : refine_data(dx,dy,dz,maxratio), Nxx(refine_data) {}
  ~Oxs_DemagNxxAsymptotic() {}

  OXS_DEMAG_REAL_ASYMP NxxAsymptotic(const OxsDemagNabData& ptdata) const;

  OXS_DEMAG_REAL_ASYMP NxxAsymptotic(OXS_DEMAG_REAL_ASYMP x,
                                     OXS_DEMAG_REAL_ASYMP y,
                                     OXS_DEMAG_REAL_ASYMP z) const {
    OxsDemagNabData ptdata(x,y,z);
    return NxxAsymptotic(ptdata);
  }

  OXS_DEMAG_REAL_ASYMP
  NxxAsymptoticPair(const OxsDemagNabPairData& ptdata) const
  { return NxxAsymptotic(ptdata.ptp) +  NxxAsymptotic(ptdata.ptm); }

private:
  OxsDemagAsymptoticRefineData refine_data;
  Oxs_DemagNxxAsymptoticBase Nxx;
};

class Oxs_DemagNxyAsymptoticBase {
public:
  Oxs_DemagNxyAsymptoticBase(const OxsDemagAsymptoticRefineData& refine_data);
  ~Oxs_DemagNxyAsymptoticBase() {}

  OXS_DEMAG_REAL_ASYMP NxyAsymptotic(const OxsDemagNabData& ptdata) const;

  OXS_DEMAG_REAL_ASYMP NxyAsymptotic(OXS_DEMAG_REAL_ASYMP x,
                                     OXS_DEMAG_REAL_ASYMP y,
                                     OXS_DEMAG_REAL_ASYMP z) const {
    OxsDemagNabData ptdata(x,y,z);
    return NxyAsymptotic(ptdata);
  }

  // The NxyAsymptoticPairX routines evaluate asymptotic approximation to
  //    Nxy(x+xoff,y,z) + Nxy(x-xoff,y,z)
  // If |xoff| >> |x|, then this is better than making two calls to
  // NxyAsymptotic because the PairX variant handles cancellations in
  // the lead O(1/R^3) term algebraically.  This cancellation does not
  // occur across the other (y- or z-) axes, so there is no corresponding
  // PairY or PairZ routines --- in those circumstances just make two
  // calls to NxyAsymptotic.
  OXS_DEMAG_REAL_ASYMP NxyAsymptoticPairX(const OxsDemagNabPairData& ptdata) const;

#if OXS_DEMAG_ASYMP_USE_SSE
  OXS_DEMAG_REAL_ASYMP
  NxyAsymptoticPair(const OxsDemagNabData& ptA,const OxsDemagNabData& ptB) const;
#else
  OXS_DEMAG_REAL_ASYMP
  NxyAsymptoticPair(const OxsDemagNabData& ptA,const OxsDemagNabData& ptB) const
  { return NxyAsymptotic(ptA) + NxyAsymptotic(ptB); }
#endif

private:
  int cubic_cell;
  OXS_DEMAG_REAL_ASYMP lead_weight;
  OXS_DEMAG_REAL_ASYMP a1,a2,a3;
  OXS_DEMAG_REAL_ASYMP b1,b2,b3,b4,b5,b6;
  OXS_DEMAG_REAL_ASYMP c1,c2,c3,c4,c5,c6,c7,c8,c9,c10;
  // The following members are declared but not defined:
  Oxs_DemagNxyAsymptoticBase(const Oxs_DemagNxyAsymptoticBase&);
  Oxs_DemagNxyAsymptoticBase& operator=(const Oxs_DemagNxyAsymptoticBase&);
};

class Oxs_DemagNxyAsymptotic {
public:
  Oxs_DemagNxyAsymptotic(OXS_DEMAG_REAL_ASYMP dx,
                         OXS_DEMAG_REAL_ASYMP dy,
                         OXS_DEMAG_REAL_ASYMP dz,
                         OXS_DEMAG_REAL_ASYMP maxratio
                         = OXS_DEMAG_ASYMPTOTIC_DEFAULT_REFINE_RATIO)
    : refine_data(dx,dy,dz,maxratio), Nxy(refine_data) {}
  ~Oxs_DemagNxyAsymptotic() {}

  OXS_DEMAG_REAL_ASYMP NxyAsymptotic(const OxsDemagNabData& ptdata) const;

  OXS_DEMAG_REAL_ASYMP NxyAsymptotic(OXS_DEMAG_REAL_ASYMP x,
                                     OXS_DEMAG_REAL_ASYMP y,
                                     OXS_DEMAG_REAL_ASYMP z) const {
    OxsDemagNabData ptdata(x,y,z);
    return NxyAsymptotic(ptdata);
  }

  OXS_DEMAG_REAL_ASYMP NxyAsymptoticPairX(const OxsDemagNabPairData& ptdata) const;
  OXS_DEMAG_REAL_ASYMP NxyAsymptoticPairZ(const OxsDemagNabPairData& ptdata) const
  {
    return NxyAsymptotic(ptdata.ptp) + NxyAsymptotic(ptdata.ptm);
  }

private:
  OxsDemagAsymptoticRefineData refine_data;
  Oxs_DemagNxyAsymptoticBase Nxy;
};

class Oxs_DemagNyyAsymptotic {
public:
  Oxs_DemagNyyAsymptotic(OXS_DEMAG_REAL_ASYMP dx,
                         OXS_DEMAG_REAL_ASYMP dy,
                         OXS_DEMAG_REAL_ASYMP dz,
                         OXS_DEMAG_REAL_ASYMP maxratio
                         = OXS_DEMAG_ASYMPTOTIC_DEFAULT_REFINE_RATIO)
    : Nxx(dy,dx,dz,maxratio) {}

  ~Oxs_DemagNyyAsymptotic() {}

  OXS_DEMAG_REAL_ASYMP NyyAsymptotic(OXS_DEMAG_REAL_ASYMP x,
                                     OXS_DEMAG_REAL_ASYMP y,
                                     OXS_DEMAG_REAL_ASYMP z) const
  { return Nxx.NxxAsymptotic(y,x,z); }

private:
  const Oxs_DemagNxxAsymptotic Nxx;
  // The following members are declared but not defined:
  Oxs_DemagNyyAsymptotic(const Oxs_DemagNyyAsymptotic&);
  Oxs_DemagNyyAsymptotic& operator=(const Oxs_DemagNyyAsymptotic&);
};

class Oxs_DemagNzzAsymptotic {
public:
  Oxs_DemagNzzAsymptotic(OXS_DEMAG_REAL_ASYMP dx,
                         OXS_DEMAG_REAL_ASYMP dy,
                         OXS_DEMAG_REAL_ASYMP dz,
                         OXS_DEMAG_REAL_ASYMP maxratio
                         = OXS_DEMAG_ASYMPTOTIC_DEFAULT_REFINE_RATIO)
    : Nxx(dz,dy,dx,maxratio) {}

  ~Oxs_DemagNzzAsymptotic() {}

  OXS_DEMAG_REAL_ASYMP NzzAsymptotic(OXS_DEMAG_REAL_ASYMP x,
                                     OXS_DEMAG_REAL_ASYMP y,
                                     OXS_DEMAG_REAL_ASYMP z) const
  { return Nxx.NxxAsymptotic(z,y,x); }

private:
  const Oxs_DemagNxxAsymptotic Nxx;
  // The following members are declared but not defined:
  Oxs_DemagNzzAsymptotic(const Oxs_DemagNzzAsymptotic&);
  Oxs_DemagNzzAsymptotic& operator=(const Oxs_DemagNzzAsymptotic&);
};

class Oxs_DemagNxzAsymptotic {
public:
  Oxs_DemagNxzAsymptotic(OXS_DEMAG_REAL_ASYMP dx,
                         OXS_DEMAG_REAL_ASYMP dy,
                         OXS_DEMAG_REAL_ASYMP dz,
                         OXS_DEMAG_REAL_ASYMP maxratio
                         = OXS_DEMAG_ASYMPTOTIC_DEFAULT_REFINE_RATIO)
    : Nxy(dx,dz,dy,maxratio) {}

  ~Oxs_DemagNxzAsymptotic() {}

  OXS_DEMAG_REAL_ASYMP NxzAsymptotic(OXS_DEMAG_REAL_ASYMP x,
                                     OXS_DEMAG_REAL_ASYMP y,
                                     OXS_DEMAG_REAL_ASYMP z) const
  { return Nxy.NxyAsymptotic(x,z,y); }

private:
  const Oxs_DemagNxyAsymptotic Nxy;
  // The following members are declared but not defined:
  Oxs_DemagNxzAsymptotic(const Oxs_DemagNxzAsymptotic&);
  Oxs_DemagNxzAsymptotic& operator=(const Oxs_DemagNxzAsymptotic&);
};

class Oxs_DemagNyzAsymptotic {
public:
  Oxs_DemagNyzAsymptotic(OXS_DEMAG_REAL_ASYMP dx,
                         OXS_DEMAG_REAL_ASYMP dy,
                         OXS_DEMAG_REAL_ASYMP dz,
                         OXS_DEMAG_REAL_ASYMP maxratio
                         = OXS_DEMAG_ASYMPTOTIC_DEFAULT_REFINE_RATIO)
    : Nxy(dy,dz,dx,maxratio) {}

  ~Oxs_DemagNyzAsymptotic() {}

  OXS_DEMAG_REAL_ASYMP NyzAsymptotic(OXS_DEMAG_REAL_ASYMP x,
                                     OXS_DEMAG_REAL_ASYMP y,
                                     OXS_DEMAG_REAL_ASYMP z) const
  { return Nxy.NxyAsymptotic(y,z,x); }

private:
  const Oxs_DemagNxyAsymptotic Nxy;
  // The following members are declared but not defined:
  Oxs_DemagNyzAsymptotic(const Oxs_DemagNyzAsymptotic&);
  Oxs_DemagNyzAsymptotic& operator=(const Oxs_DemagNyzAsymptotic&);
};

////////////////////////////////////////////////////////////////////////
// Support classes for periodic boundaries

// Code to compute tails for periodic boundaries:
//
//    class OxsDemagNxxIntegralX
//    class OxsDemagNxyIntegralX
//    class OxsDemagNxxIntegralZ
//    class OxsDemagNxyIntegralZ
//
// These four classes suffice to to compute any NabIntegralC for
// a,b,c \in {x,y,z}.
//
// These classes are intended for internal use only.
//
class OxsDemagNxxIntegralXBase {
public:
  OxsDemagNxxIntegralXBase(const OxsDemagAsymptoticRefineData& refine_data,
                           OXS_DEMAG_REAL_ASYMP Wx);
  ~OxsDemagNxxIntegralXBase() {}
  OXS_DEMAG_REAL_ASYMP Compute(const OxsDemagNabPairData& data) const;
  /// ubase, uoff in data are wrt x
private:
  const OXS_DEMAG_REAL_ASYMP scale;
  OXS_DEMAG_REAL_ASYMP a1,a2,a3;
  OXS_DEMAG_REAL_ASYMP b1,b2,b3,b4,b5,b6;
  int cubic_cell;
  // The following members are declared but not defined:
  OxsDemagNxxIntegralXBase(const OxsDemagNxxIntegralXBase&);
  OxsDemagNxxIntegralXBase& operator=(const OxsDemagNxxIntegralXBase&);
};

class OxsDemagNxyIntegralXBase {
public:
  OxsDemagNxyIntegralXBase(const OxsDemagAsymptoticRefineData &refine_data,
                           OXS_DEMAG_REAL_ASYMP Wx);
  ~OxsDemagNxyIntegralXBase() {}
  OXS_DEMAG_REAL_ASYMP Compute(const OxsDemagNabPairData& data) const;
  /// ubase, uoff in data are wrt x
private:
  const OXS_DEMAG_REAL_ASYMP scale;
  OXS_DEMAG_REAL_ASYMP a1,a2,a3;
  OXS_DEMAG_REAL_ASYMP b1,b2,b3,b4,b5,b6;
  int cubic_cell;
  // The following members are declared but not defined:
  OxsDemagNxyIntegralXBase(const OxsDemagNxyIntegralXBase&);
  OxsDemagNxyIntegralXBase& operator=(const OxsDemagNxyIntegralXBase&);
};

// In the following template, class T should be one of
// OxsDemagNxxIntegralXBase or OxsDemagNxyIntegralXBase.
// See typedef's following template definition.
template<typename T>
class OxsDemagNttIntegralX {
public:
  OxsDemagNttIntegralX(OXS_DEMAG_REAL_ASYMP dx,
                       OXS_DEMAG_REAL_ASYMP dy,
                       OXS_DEMAG_REAL_ASYMP dz,
                       OXS_DEMAG_REAL_ASYMP Wx,
                       OXS_DEMAG_REAL_ASYMP maxratio
                       = OXS_DEMAG_ASYMPTOTIC_DEFAULT_REFINE_RATIO)
    : refine_data(dx,dy,dz,maxratio), Ntt(refine_data,Wx) {}
  ~OxsDemagNttIntegralX() {}
  OXS_DEMAG_REAL_ASYMP Compute(const OxsDemagNabPairData& data) const;
  /// ubase, uoff in data are wrt x
private:
  OxsDemagAsymptoticRefineData refine_data;
  const T Ntt;
};

template<typename T>
OXS_DEMAG_REAL_ASYMP
OxsDemagNttIntegralX<T>::Compute(const OxsDemagNabPairData& ptdata) const
{ // Integral of asymptotic approximation to Ntt term, from xp to
  // infinity and -infinity to xm (xp must be > 0, and xm must be < 0),
  // with window x dimension Wx.  This routine is used internally for
  // periodic demag computations.

  // Presumably at least one of xcount, ycount, or zcount is 1, but this
  // fact is not used in following code.
  assert(ptdata.ptp.y == ptdata.ptm.y &&
         ptdata.ptp.z == ptdata.ptm.z);

  // Presumably at least one of xcount, ycount, or zcount is 1, but this
  // fact is not used in following code.

  // Alias data from refine_data structure.
  const OC_INT4m& xcount = refine_data.xcount;
  const OC_INT4m& ycount = refine_data.ycount;
  const OC_INT4m& zcount = refine_data.zcount;
  const OXS_DEMAG_REAL_ASYMP& rdx = refine_data.rdx;
  const OXS_DEMAG_REAL_ASYMP& rdy = refine_data.rdy;
  const OXS_DEMAG_REAL_ASYMP& rdz = refine_data.rdz;
  const OXS_DEMAG_REAL_ASYMP& result_scale = refine_data.result_scale;

  OxsDemagNabPairData work;
  work.ubase = ptdata.ubase;
  OXS_DEMAG_REAL_ASYMP zsum = 0.0;
  for(OC_INT4m k=1-zcount;k<zcount;++k) {
    OXS_DEMAG_REAL_ASYMP zoff = ptdata.ptp.z+k*rdz; // .ptm.z == .ptp.z
    OXS_DEMAG_REAL_ASYMP ysum = 0.0;
    for(OC_INT4m j=1-ycount;j<ycount;++j) {
      // Compute interactions for x-strip
      OXS_DEMAG_REAL_ASYMP yoff = ptdata.ptp.y+j*rdy; // .ptm.y == .ptp.y
      work.uoff = ptdata.uoff;
      OxsDemagNabData::SetPair(work.ubase + work.uoff,yoff,zoff,
                               work.ubase - work.uoff,yoff,zoff,
                               work.ptp, work.ptm);
      OXS_DEMAG_REAL_ASYMP xsum = xcount * Ntt.Compute(work);
      for(OC_INT4m i=1;i<xcount;++i) {
        work.uoff = ptdata.uoff + i*rdx;
        OxsDemagNabData::SetPair(work.ubase + work.uoff,yoff,zoff,
                                 work.ubase - work.uoff,yoff,zoff,
                                 work.ptp, work.ptm);
        OXS_DEMAG_REAL_ASYMP tmpsum = Ntt.Compute(work);
        work.uoff = ptdata.uoff - i*rdx;
        OxsDemagNabData::SetPair(work.ubase + work.uoff,yoff,zoff,
                                 work.ubase - work.uoff,yoff,zoff,
                                 work.ptp, work.ptm);
        tmpsum += Ntt.Compute(work);
        xsum += (xcount-i) * tmpsum;
      }
      // Weight x-strip interactions into xy-plate
      ysum += (ycount - abs(j))*xsum;
    }
    // Weight xy-plate interactions into total sum
    zsum += (zcount - abs(k))*ysum;
  }
  return zsum*result_scale;
}

typedef OxsDemagNttIntegralX<OxsDemagNxxIntegralXBase> OxsDemagNxxIntegralX;
typedef OxsDemagNttIntegralX<OxsDemagNxyIntegralXBase> OxsDemagNxyIntegralX;

class OxsDemagNxxIntegralZBase {
public:
  OxsDemagNxxIntegralZBase(const OxsDemagAsymptoticRefineData &refine_data,
                           OXS_DEMAG_REAL_ASYMP Wz);
  ~OxsDemagNxxIntegralZBase() {}
  OXS_DEMAG_REAL_ASYMP Compute(const OxsDemagNabPairData& data) const;
  /// ubase, uoff in data are wrt z
private:
  const OXS_DEMAG_REAL_ASYMP scale;
  OXS_DEMAG_REAL_ASYMP a1,a2,a3,a4,a5,a6;
  OXS_DEMAG_REAL_ASYMP b1,b2,b3,b4,b5,b6,b7,b8,b9,b10;
  int cubic_cell;
  // The following members are declared but not defined:
  OxsDemagNxxIntegralZBase(const OxsDemagNxxIntegralZBase&);
  OxsDemagNxxIntegralZBase& operator=(const OxsDemagNxxIntegralZBase&);
};

class OxsDemagNxyIntegralZBase {
public:
  OxsDemagNxyIntegralZBase(const OxsDemagAsymptoticRefineData &refine_data,
                           OXS_DEMAG_REAL_ASYMP Wz);
  ~OxsDemagNxyIntegralZBase() {}
  OXS_DEMAG_REAL_ASYMP Compute(const OxsDemagNabPairData& data) const;
  /// ubase, uoff in data are wrt z
private:
  const OXS_DEMAG_REAL_ASYMP scale;
  OXS_DEMAG_REAL_ASYMP a1,a2,a3;
  OXS_DEMAG_REAL_ASYMP b1,b2,b3,b4,b5,b6;
  int cubic_cell;
  // The following members are declared but not defined:
  OxsDemagNxyIntegralZBase(const OxsDemagNxyIntegralZBase&);
  OxsDemagNxyIntegralZBase& operator=(const OxsDemagNxyIntegralZBase&);
};

// In the following template, class T should be one of
// OxsDemagNxxIntegralZBase or OxsDemagNxyIntegralZBase.
// See typedef's following template definition.
template<typename T>
class OxsDemagNttIntegralZ {
public:
  OxsDemagNttIntegralZ(OXS_DEMAG_REAL_ASYMP dx,
                       OXS_DEMAG_REAL_ASYMP dy,
                       OXS_DEMAG_REAL_ASYMP dz,
                       OXS_DEMAG_REAL_ASYMP Wz,
                       OXS_DEMAG_REAL_ASYMP maxratio
                       = OXS_DEMAG_ASYMPTOTIC_DEFAULT_REFINE_RATIO)
    : refine_data(dx,dy,dz,maxratio), Ntt(refine_data,Wz) {}
  ~OxsDemagNttIntegralZ() {}
  OXS_DEMAG_REAL_ASYMP Compute(const OxsDemagNabPairData& data) const;
  /// ubase, uoff in data are wrt z
private:
  OxsDemagAsymptoticRefineData refine_data;
  const T Ntt;
};

template<typename T>
OXS_DEMAG_REAL_ASYMP
OxsDemagNttIntegralZ<T>::Compute(const OxsDemagNabPairData& ptdata) const
{ // Integral of asymptotic approximation to Ntt term, from zp to
  // infinity and -infinity to zm (zp must be > 0, and zm must be < 0),
  // with window z dimension Wz.  This routine is used internally for
  // periodic demag computations.

  // Presumably at least one of xcount, ycount, or zcount is 1, but this
  // fact is not used in following code.
  assert(ptdata.ptp.x == ptdata.ptm.x &&
         ptdata.ptp.y == ptdata.ptm.y);

  // Presumably at least one of xcount, ycount, or zcount is 1, but this
  // fact is not used in following code.

  // Alias data from refine_data structure.
  const OC_INT4m& xcount = refine_data.xcount;
  const OC_INT4m& ycount = refine_data.ycount;
  const OC_INT4m& zcount = refine_data.zcount;
  const OXS_DEMAG_REAL_ASYMP& rdx = refine_data.rdx;
  const OXS_DEMAG_REAL_ASYMP& rdy = refine_data.rdy;
  const OXS_DEMAG_REAL_ASYMP& rdz = refine_data.rdz;
  const OXS_DEMAG_REAL_ASYMP& result_scale = refine_data.result_scale;

  OxsDemagNabPairData work;
  work.uoff = ptdata.uoff;
  OXS_DEMAG_REAL_ASYMP zsum = 0.0;
  for(OC_INT4m k=1-zcount;k<zcount;++k) {
    work.ubase = ptdata.ubase + k*rdz;
    OXS_DEMAG_REAL_ASYMP za = work.ubase + work.uoff;
    OXS_DEMAG_REAL_ASYMP zs = work.ubase - work.uoff;
    OXS_DEMAG_REAL_ASYMP ysum = 0.0;
    for(OC_INT4m j=1-ycount;j<ycount;++j) {
      // Compute interactions for x-strip
      OXS_DEMAG_REAL_ASYMP yoff = ptdata.ptp.y+j*rdy; // .ptm.y == .ptp.y
      OxsDemagNabData::SetPair(ptdata.ptp.x,yoff,za,
                               ptdata.ptp.x,yoff,zs,
                               work.ptp, work.ptm);  // .ptm.x == .ptp.x
      OXS_DEMAG_REAL_ASYMP xsum = xcount * Ntt.Compute(work);
      for(OC_INT4m i=1;i<xcount;++i) {
        OXS_DEMAG_REAL_ASYMP xoffa = ptdata.ptp.x+i*rdx; // .ptm.x == .ptp.x
        OxsDemagNabData::SetPair(xoffa,yoff,za,
                                 xoffa,yoff,zs,
                                 work.ptp, work.ptm);
        OXS_DEMAG_REAL_ASYMP tmpsum = Ntt.Compute(work);
        OXS_DEMAG_REAL_ASYMP xoffs = ptdata.ptp.x-i*rdx; // .ptm.x == .ptp.x
        OxsDemagNabData::SetPair(xoffs,yoff,za,
                                 xoffs,yoff,zs,
                                 work.ptp, work.ptm);
        tmpsum += Ntt.Compute(work);
        xsum += (xcount-i) * tmpsum;
      }
      // Weight x-strip interactions into xy-plate
      ysum += (ycount - abs(j))*xsum;
    }
    // Weight xy-plate interactions into total sum
    zsum += (zcount - abs(k))*ysum;
  }
  return zsum*result_scale;
}

typedef OxsDemagNttIntegralZ<OxsDemagNxxIntegralZBase> OxsDemagNxxIntegralZ;
typedef OxsDemagNttIntegralZ<OxsDemagNxyIntegralZBase> OxsDemagNxyIntegralZ;

#undef OXS_DEMAG_ASYMPTOTIC_DEFAULT_REFINE_RATIO

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
//
// Classes to compute periodic Nab kernels:
//
//     OxsDemagNabPeriodic  <-- abstract base class
//
//     OxsDemagNxxPeriodicX
//     OxsDemagNxyPeriodicX
//     OxsDemagNxxPeriodicZ
//     OxsDemagNxyPeriodicZ
//
// These four classes suffice to to compute any NabPeriodicC tensor for
// a,b,C \in {x,y,z}.  The wrapper classes Oxs_DemagPeriodicC, c \in
// {X,Y,Z}, provide the necessary coordinate permutations to use the
// above four classes to produce all 18 NabPeriodicC tensors.
//
// The OxsDemagNabPeriodic* classes are intended for internal use only.
// External code should use the Oxs_DemagPeriodicC wrappers.


class OxsDemagNabPeriodic {  // Interface class
public:
  virtual OXS_DEMAG_REAL_ASYMP NearField(const OxsDemagNabData& data) const = 0;

  virtual OXS_DEMAG_REAL_ASYMP FarField(const OxsDemagNabData& data) const = 0;

  virtual OXS_DEMAG_REAL_ASYMP
  FarFieldPair(const OxsDemagNabPairData& data) const = 0;

  virtual OXS_DEMAG_REAL_ASYMP
  FarIntegralPair(const OxsDemagNabPairData& data) const = 0;


protected:
  OxsDemagNabPeriodic(OXS_DEMAG_REAL_ASYMP idx,
                      OXS_DEMAG_REAL_ASYMP idy,
                      OXS_DEMAG_REAL_ASYMP idz)
    : dx(idx), dy(idy), dz(idz) {}
  OxsDemagNabPeriodic(const OxsDemagAdapter& ad)
    : dx(ad.x), dy(ad.y), dz(ad.z) {}

  virtual ~OxsDemagNabPeriodic() {}

  const OXS_DEMAG_REAL_ASYMP dx;
  const OXS_DEMAG_REAL_ASYMP dy;
  const OXS_DEMAG_REAL_ASYMP dz;
private:
  // Declare but don't define:
  OxsDemagNabPeriodic();
  OxsDemagNabPeriodic(const OxsDemagNabPeriodic&);
  OxsDemagNabPeriodic& operator=(const OxsDemagNabPeriodic&);

};


class OxsDemagNxxPeriodicX : public OxsDemagNabPeriodic {
public:
  OxsDemagNxxPeriodicX(OXS_DEMAG_REAL_ASYMP idx,
                       OXS_DEMAG_REAL_ASYMP idy,
                       OXS_DEMAG_REAL_ASYMP idz,
                       OXS_DEMAG_REAL_ASYMP Wx)
    : OxsDemagNabPeriodic(idx,idy,idz),
      ANxx(idx,idy,idz), NxxIx(idx,idy,idz,Wx)
  {}
  OxsDemagNxxPeriodicX(const OxsDemagAdapter& ad,OXS_DEMAG_REAL_ASYMP Wx)
    : OxsDemagNabPeriodic(ad),
      ANxx(ad.x,ad.y,ad.z),  NxxIx(ad.x,ad.y,ad.z,Wx)
  {}

  virtual ~OxsDemagNxxPeriodicX() {}

  virtual OXS_DEMAG_REAL_ASYMP NearField(const OxsDemagNabData& data) const
  {
    return Oxs_CalculateSDA00(data.x,data.y,data.z,dx,dy,dz);
  } 

  virtual OXS_DEMAG_REAL_ASYMP FarField(const OxsDemagNabData& data) const
  {
    return ANxx.NxxAsymptotic(data);
  }

  virtual OXS_DEMAG_REAL_ASYMP
  FarFieldPair(const OxsDemagNabPairData& pairdata) const
  { 
    return ANxx.NxxAsymptoticPair(pairdata);
  }

  virtual OXS_DEMAG_REAL_ASYMP
  FarIntegralPair(const OxsDemagNabPairData& pairdata) const
  {
    assert(pairdata.ptp.y == pairdata.ptm.y &&
           pairdata.ptp.z == pairdata.ptm.z);
    return NxxIx.Compute(pairdata);
  }

private:
  const Oxs_DemagNxxAsymptotic ANxx;
  const OxsDemagNxxIntegralX NxxIx;
};

class OxsDemagNxyPeriodicX : public OxsDemagNabPeriodic {
public:
  OxsDemagNxyPeriodicX(OXS_DEMAG_REAL_ASYMP idx,
                       OXS_DEMAG_REAL_ASYMP idy,
                       OXS_DEMAG_REAL_ASYMP idz,
                       OXS_DEMAG_REAL_ASYMP Wx)
  : OxsDemagNabPeriodic(idx,idy,idz),
    ANxy(idx,idy,idz), NxyIx(idx,idy,idz,Wx)
  {}
  OxsDemagNxyPeriodicX(const OxsDemagAdapter& ad,OXS_DEMAG_REAL_ASYMP Wx)
    : OxsDemagNabPeriodic(ad),
      ANxy(ad.x,ad.y,ad.z), NxyIx(ad.x,ad.y,ad.z,Wx)
  {}

  virtual ~OxsDemagNxyPeriodicX() {}

  virtual OXS_DEMAG_REAL_ASYMP NearField(const OxsDemagNabData& data) const
  {
    return Oxs_CalculateSDA01(data.x,data.y,data.z,dx,dy,dz);
  } 

  virtual OXS_DEMAG_REAL_ASYMP FarField(const OxsDemagNabData& data) const
  {
    return ANxy.NxyAsymptotic(data);
  }

  virtual OXS_DEMAG_REAL_ASYMP
  FarFieldPair(const OxsDemagNabPairData& pairdata) const
  { // Nxy offers an AsymptoticPairX routine
    return ANxy.NxyAsymptoticPairX(pairdata);
  }

  virtual OXS_DEMAG_REAL_ASYMP
  FarIntegralPair(const OxsDemagNabPairData& pairdata) const
  {
    assert(pairdata.ptp.y == pairdata.ptm.y &&
           pairdata.ptp.z == pairdata.ptm.z);
    return NxyIx.Compute(pairdata);
  }

private:
  const Oxs_DemagNxyAsymptotic ANxy;
  const OxsDemagNxyIntegralX NxyIx;
};

class OxsDemagNxxPeriodicZ : public OxsDemagNabPeriodic {
public:
  OxsDemagNxxPeriodicZ(OXS_DEMAG_REAL_ASYMP idx,
                       OXS_DEMAG_REAL_ASYMP idy,
                       OXS_DEMAG_REAL_ASYMP idz,
                       OXS_DEMAG_REAL_ASYMP Wz)
    : OxsDemagNabPeriodic(idx,idy,idz),
      ANxx(idx,idy,idz), NxxIz(idx,idy,idz,Wz)
  {}
  OxsDemagNxxPeriodicZ(const OxsDemagAdapter& ad,OXS_DEMAG_REAL_ASYMP Wz)
    : OxsDemagNabPeriodic(ad),
      ANxx(ad.x,ad.y,ad.z), NxxIz(ad.x,ad.y,ad.z,Wz)
  {}

  virtual ~OxsDemagNxxPeriodicZ() {}

  virtual OXS_DEMAG_REAL_ASYMP NearField(const OxsDemagNabData& data) const
  {
    return Oxs_CalculateSDA00(data.x,data.y,data.z,dx,dy,dz);
  } 

  virtual OXS_DEMAG_REAL_ASYMP FarField(const OxsDemagNabData& data) const
  {
    return ANxx.NxxAsymptotic(data);
  }

  virtual OXS_DEMAG_REAL_ASYMP
  FarFieldPair(const OxsDemagNabPairData& pairdata) const
  {
    return ANxx.NxxAsymptoticPair(pairdata);

  }

  virtual OXS_DEMAG_REAL_ASYMP
  FarIntegralPair(const OxsDemagNabPairData& pairdata) const
  {
    assert(pairdata.ptp.x == pairdata.ptm.x &&
           pairdata.ptp.y == pairdata.ptm.y);
    return NxxIz.Compute(pairdata);
  }

private:
  const Oxs_DemagNxxAsymptotic ANxx;
  const OxsDemagNxxIntegralZ NxxIz;
};

class OxsDemagNxyPeriodicZ : public OxsDemagNabPeriodic {
public:
  OxsDemagNxyPeriodicZ(OXS_DEMAG_REAL_ASYMP idx,
                       OXS_DEMAG_REAL_ASYMP idy,
                       OXS_DEMAG_REAL_ASYMP idz,
                       OXS_DEMAG_REAL_ASYMP Wz)
    : OxsDemagNabPeriodic(idx,idy,idz),
      ANxy(idx,idy,idz), NxyIz(idx,idy,idz,Wz)
  {}
  OxsDemagNxyPeriodicZ(const OxsDemagAdapter& ad,OXS_DEMAG_REAL_ASYMP Wz)
    : OxsDemagNabPeriodic(ad),
      ANxy(ad.x,ad.y,ad.z), NxyIz(ad.x,ad.y,ad.z,Wz)
  {}

  virtual ~OxsDemagNxyPeriodicZ() {}

  virtual OXS_DEMAG_REAL_ASYMP NearField(const OxsDemagNabData& data) const
  {
    return Oxs_CalculateSDA01(data.x,data.y,data.z,dx,dy,dz);
  } 

  virtual OXS_DEMAG_REAL_ASYMP FarField(const OxsDemagNabData& data) const
  {
    return ANxy.NxyAsymptotic(data);
  }

  virtual OXS_DEMAG_REAL_ASYMP
  FarFieldPair(const OxsDemagNabPairData& pairdata) const
  {
    return ANxy.NxyAsymptoticPairZ(pairdata);
  }

  virtual OXS_DEMAG_REAL_ASYMP
  FarIntegralPair(const OxsDemagNabPairData& pairdata) const
  {
    assert(pairdata.ptp.x == pairdata.ptm.x &&
           pairdata.ptp.y == pairdata.ptm.y);
    return NxyIz.Compute(pairdata);
  }

private:
  const Oxs_DemagNxyAsymptotic ANxy;
  const OxsDemagNxyIntegralZ NxyIz;
};

// Extension template
template<class NabPeriodicC,class Adapter>
class OxsTDemagNabPeriodicC : public OxsDemagNabPeriodic {
public:
  OxsTDemagNabPeriodicC(OXS_DEMAG_REAL_ASYMP idx,
                        OXS_DEMAG_REAL_ASYMP idy,
                        OXS_DEMAG_REAL_ASYMP idz,
                        OXS_DEMAG_REAL_ASYMP W)
    : OxsDemagNabPeriodic(Adapter(idx,idy,idz)),
      Nab(Adapter(idx,idy,idz),W) {}

  virtual ~OxsTDemagNabPeriodicC() {}

  virtual OXS_DEMAG_REAL_ASYMP NearField(const OxsDemagNabData& data) const
  {
    OxsTDemagNabData<Adapter> newdata(data);
    return Nab.NearField(newdata);
  } 

  virtual OXS_DEMAG_REAL_ASYMP FarField(const OxsDemagNabData& data) const
  {
    OxsTDemagNabData<Adapter> newdata(data);
    return Nab.FarField(newdata);
  }

  virtual OXS_DEMAG_REAL_ASYMP
  FarFieldPair(const OxsDemagNabPairData& pairdata) const
  {
    OxsTDemagNabPairData<Adapter> newdata(pairdata);
    return Nab.FarFieldPair(newdata);
  }

  virtual OXS_DEMAG_REAL_ASYMP
  FarIntegralPair(const OxsDemagNabPairData& pairdata) const
  {
    OxsTDemagNabPairData<Adapter> newdata(pairdata);
    return Nab.FarIntegralPair(newdata);
  }

private:
  const NabPeriodicC Nab;
};


////////////////////////////////////////////////////////////////////////
// PBC Demag Tensor Relations
//
// There are 18 periodic demag tensor terms Nab^d, where ab is in {xx,
// xy, xz, yy, yz, zz} and direction of periodicity d is in {x, y, z}.
// However, using symmetries these 18 terms can be represented via the
// four terms defined above, Nxx^x, Nxy^x, Nxx^z and Nxy^z, by simple
// interchange of coordinates.  These relations are:
//
// |      |            Direction of Periodicity           |
// | Term |     x         |     y         |     z         |
// +------+---------------+---------------+---------------+
// |  Nxx |     -         |  Nxx^z(x,z,y) |     -         |
// |  Nyy |  Nxx^z(y,z,x) |  Nxx^x(y,x,z) |  Nxx^z(y,x,z) |
// |  Nzz |  Nxx^z(z,y,x) |  Nxx^z(z,x,y) |  Nxx^x(z,y,x) |
// +------+---------------+---------------+---------------+
// |  Nxy |     -         |  Nxy^x(y,x,z) |     -         |
// |  Nxz |  Nxy^x(x,z,y) |  Nxy^z(x,z,y) |  Nxy^x(z,x,y) |
// |  Nyz |  Nxy^z(y,z,x) |  Nxy^x(y,z,x) |  Nxy^x(z,y,x) |
// +------+---------------+---------------+---------------+
//
// To save space, the above table lists only the (x,y,z) triples,
// but the Nab values depend on the cell dimension (dx,dy,dz) as
// well.  The (dx,dy,dz) are permuted in the same manner as the
// (x,y,z).
//
// See NOTES V, 2-Mar-2011, p162.
//
// However, rather that write wrappers for all 18 calls, instead we
// just write the six interface functions actually called by the
// OOMMF code, and embed the above relations into those interface
// functions.
//
// Initialization imports dx, dy, and dz are typically in problem
// units (i.e., meters), but clients are free to scale (x,y,z) and
// (dx,dy,dz) if desired.
//
// Import W is the width of the simulation window along the periodic
// direction, in other words, the length of the period.  This is
// measured in the same units as (x,y,z) and (dx,dy,dz), e.g., meters.
//
// The import AsymptoticRadius is radius beyond which asymptotics are
// used to replace the closed-form analytic representations for the
// demag tensor.  This is quantity should be scaled with (dx,dy,dz).  In
// the non-periodic demag code setting AsymptoticRadius to zero has the
// effect of using the closed-form analytic representations at all
// points.  This is not possible for the periodic code, because the
// image tail computation is based on the asymptotic formulas.  If the
// AsymptoticRadius import is <=0, then it is reset to match W/2.
// WARNING: If W is very short, then this is probably *not* what one
// wants.
//
// IMPORTANT NOTE: The following routines DO include the 
//                 Nab component from the base window.



class OxsDemagPeriodic
{
protected:
  OxsDemagPeriodic(OXS_DEMAG_REAL_ASYMP dx,
                   OXS_DEMAG_REAL_ASYMP dy,
                   OXS_DEMAG_REAL_ASYMP dz,
                   OXS_DEMAG_REAL_ASYMP iW,
                   OXS_DEMAG_REAL_ASYMP AsymptoticRadius);

  virtual ~OxsDemagPeriodic() {} // Ensure child destructors get called.

  void ComputeAsymptoticLimits
  (OXS_DEMAG_REAL_ASYMP u, // base offset along periodic direction
   OXS_DEMAG_REAL_ASYMP v, OXS_DEMAG_REAL_ASYMP w, // Other coordinates
   OC_INDEX& k1, OC_INDEX& k2,   // Near field terms
   OC_INDEX& k1a, OC_INDEX& k2a, // Asymmetric far field
   OXS_DEMAG_REAL_ASYMP& newu,   // Symmetric far field
   OXS_DEMAG_REAL_ASYMP& newoffset
   ) const;

  enum { TAIL_TWEAK_COUNT = 8 };
  static const OXS_DEMAG_REAL_ASYMP D[TAIL_TWEAK_COUNT];

  const OXS_DEMAG_REAL_ASYMP W; // Window size in x direction
  const OXS_DEMAG_REAL_ASYMP SDA_scaling;
  const OXS_DEMAG_REAL_ASYMP AsymptoticStart;
  const OC_INDEX ktail;
private:
  // The following members are declared but not defined:
  OxsDemagPeriodic(const OxsDemagPeriodic&);
  OxsDemagPeriodic& operator=(const OxsDemagPeriodic&);
};

class Oxs_DemagPeriodicX : public OxsDemagPeriodic
{
public:
  Oxs_DemagPeriodicX(OXS_DEMAG_REAL_ASYMP dx,
                     OXS_DEMAG_REAL_ASYMP dy,
                     OXS_DEMAG_REAL_ASYMP dz,
                     OXS_DEMAG_REAL_ASYMP Wx,
                     OXS_DEMAG_REAL_ASYMP AsymptoticRadius)
    : OxsDemagPeriodic(dx,dy,dz,Wx,AsymptoticRadius),
      Nxx(dx,dy,dz,Wx), Nxy(dx,dy,dz,Wx), Nxz(dx,dy,dz,Wx),
      Nyy(dx,dy,dz,Wx), Nyz(dx,dy,dz,Wx), Nzz(dx,dy,dz,Wx) {}


  void NxxNxyNxz(OXS_DEMAG_REAL_ASYMP x,
                 OXS_DEMAG_REAL_ASYMP y,
                 OXS_DEMAG_REAL_ASYMP z,
                 OXS_DEMAG_REAL_ASYMP &pbcNxx,
                 OXS_DEMAG_REAL_ASYMP &pbcNxy,
                 OXS_DEMAG_REAL_ASYMP &pbcNxz) {
    ComputeTensor(Nxx,Nxy,Nxz,x,y,z,pbcNxx,pbcNxy,pbcNxz);
  }
  
  void NyyNyzNzz(OXS_DEMAG_REAL_ASYMP x,
                 OXS_DEMAG_REAL_ASYMP y,
                 OXS_DEMAG_REAL_ASYMP z,
                 OXS_DEMAG_REAL_ASYMP &pbcNyy,
                 OXS_DEMAG_REAL_ASYMP &pbcNyz,
                 OXS_DEMAG_REAL_ASYMP &pbcNzz) {
    ComputeTensor(Nyy,Nyz,Nzz,x,y,z,pbcNyy,pbcNyz,pbcNzz);
  }

private:
  // Introducing an adapter into ComputeTensor would allow it to be
  // moved up into the parent OxsDemagPeriodic class, but the code
  // is somewhat easier to follow and develop with separate
  // ComputeTensor members for each periodic direction.  We may change
  // this in the future, but since these are private members such a
  // change shouldn't affect any clients.
  void ComputeTensor(const OxsDemagNabPeriodic& Nab,
                     const OxsDemagNabPeriodic& Ncd,
                     const OxsDemagNabPeriodic& Nef,
                     OXS_DEMAG_REAL_ASYMP x,
                     OXS_DEMAG_REAL_ASYMP y,
                     OXS_DEMAG_REAL_ASYMP z,
                     OXS_DEMAG_REAL_ASYMP& pbcNab,
                     OXS_DEMAG_REAL_ASYMP& pbcNcd,
                     OXS_DEMAG_REAL_ASYMP& pbcNef) const;

  OxsDemagNxxPeriodicX Nxx;
  OxsDemagNxyPeriodicX Nxy;
  OxsTDemagNabPeriodicC<OxsDemagNxyPeriodicX,OxsDemagXZYAdapter> Nxz;

  OxsTDemagNabPeriodicC<OxsDemagNxxPeriodicZ,OxsDemagYZXAdapter> Nyy;
  OxsTDemagNabPeriodicC<OxsDemagNxyPeriodicZ,OxsDemagYZXAdapter> Nyz;
  OxsTDemagNabPeriodicC<OxsDemagNxxPeriodicZ,OxsDemagZYXAdapter> Nzz;
};


class Oxs_DemagPeriodicY : public OxsDemagPeriodic
{
public:
  Oxs_DemagPeriodicY(OXS_DEMAG_REAL_ASYMP dx,
                     OXS_DEMAG_REAL_ASYMP dy,
                     OXS_DEMAG_REAL_ASYMP dz,
                     OXS_DEMAG_REAL_ASYMP Wy,
                     OXS_DEMAG_REAL_ASYMP AsymptoticRadius)
    : OxsDemagPeriodic(dx,dy,dz,Wy,AsymptoticRadius),
      Nxx(dx,dy,dz,Wy), Nxy(dx,dy,dz,Wy), Nxz(dx,dy,dz,Wy),
      Nyy(dx,dy,dz,Wy), Nyz(dx,dy,dz,Wy), Nzz(dx,dy,dz,Wy) {}


  void NxxNxyNxz(OXS_DEMAG_REAL_ASYMP x,
                 OXS_DEMAG_REAL_ASYMP y,
                 OXS_DEMAG_REAL_ASYMP z,
                 OXS_DEMAG_REAL_ASYMP &pbcNxx,
                 OXS_DEMAG_REAL_ASYMP &pbcNxy,
                 OXS_DEMAG_REAL_ASYMP &pbcNxz) {
    ComputeTensor(Nxx,Nxy,Nxz,x,y,z,pbcNxx,pbcNxy,pbcNxz);
  }
  
  void NyyNyzNzz(OXS_DEMAG_REAL_ASYMP x,
                 OXS_DEMAG_REAL_ASYMP y,
                 OXS_DEMAG_REAL_ASYMP z,
                 OXS_DEMAG_REAL_ASYMP &pbcNyy,
                 OXS_DEMAG_REAL_ASYMP &pbcNyz,
                 OXS_DEMAG_REAL_ASYMP &pbcNzz) {
    ComputeTensor(Nyy,Nyz,Nzz,x,y,z,pbcNyy,pbcNyz,pbcNzz);
  }

private:
  void ComputeTensor(const OxsDemagNabPeriodic& Nab,
                     const OxsDemagNabPeriodic& Ncd,
                     const OxsDemagNabPeriodic& Nef,
                     OXS_DEMAG_REAL_ASYMP x,
                     OXS_DEMAG_REAL_ASYMP y,
                     OXS_DEMAG_REAL_ASYMP z,
                     OXS_DEMAG_REAL_ASYMP& pbcNab,
                     OXS_DEMAG_REAL_ASYMP& pbcNcd,
                     OXS_DEMAG_REAL_ASYMP& pbcNef) const;

  OxsTDemagNabPeriodicC<OxsDemagNxxPeriodicZ,OxsDemagXZYAdapter> Nxx;
  OxsTDemagNabPeriodicC<OxsDemagNxyPeriodicX,OxsDemagYXZAdapter> Nxy;
  OxsTDemagNabPeriodicC<OxsDemagNxyPeriodicZ,OxsDemagXZYAdapter> Nxz;

  OxsTDemagNabPeriodicC<OxsDemagNxxPeriodicX,OxsDemagYXZAdapter> Nyy;
  OxsTDemagNabPeriodicC<OxsDemagNxyPeriodicX,OxsDemagYZXAdapter> Nyz;
  OxsTDemagNabPeriodicC<OxsDemagNxxPeriodicZ,OxsDemagZXYAdapter> Nzz;
};

class Oxs_DemagPeriodicZ : public OxsDemagPeriodic
{
public:
  Oxs_DemagPeriodicZ(OXS_DEMAG_REAL_ASYMP dx,
                     OXS_DEMAG_REAL_ASYMP dy,
                     OXS_DEMAG_REAL_ASYMP dz,
                     OXS_DEMAG_REAL_ASYMP Wz,
                     OXS_DEMAG_REAL_ASYMP AsymptoticRadius)
    : OxsDemagPeriodic(dx,dy,dz,Wz,AsymptoticRadius),
      Nxx(dx,dy,dz,Wz), Nxy(dx,dy,dz,Wz), Nxz(dx,dy,dz,Wz),
      Nyy(dx,dy,dz,Wz), Nyz(dx,dy,dz,Wz), Nzz(dx,dy,dz,Wz) {}


  void NxxNxyNxz(OXS_DEMAG_REAL_ASYMP x,
                 OXS_DEMAG_REAL_ASYMP y,
                 OXS_DEMAG_REAL_ASYMP z,
                 OXS_DEMAG_REAL_ASYMP &pbcNxx,
                 OXS_DEMAG_REAL_ASYMP &pbcNxy,
                 OXS_DEMAG_REAL_ASYMP &pbcNxz) {
    ComputeTensor(Nxx,Nxy,Nxz,x,y,z,pbcNxx,pbcNxy,pbcNxz);
  }
  
  void NyyNyzNzz(OXS_DEMAG_REAL_ASYMP x,
                 OXS_DEMAG_REAL_ASYMP y,
                 OXS_DEMAG_REAL_ASYMP z,
                 OXS_DEMAG_REAL_ASYMP &pbcNyy,
                 OXS_DEMAG_REAL_ASYMP &pbcNyz,
                 OXS_DEMAG_REAL_ASYMP &pbcNzz) {
    ComputeTensor(Nyy,Nyz,Nzz,x,y,z,pbcNyy,pbcNyz,pbcNzz);
  }

private:
  void ComputeTensor(const OxsDemagNabPeriodic& Nab,
                     const OxsDemagNabPeriodic& Ncd,
                     const OxsDemagNabPeriodic& Nef,
                     OXS_DEMAG_REAL_ASYMP x,
                     OXS_DEMAG_REAL_ASYMP y,
                     OXS_DEMAG_REAL_ASYMP z,
                     OXS_DEMAG_REAL_ASYMP& pbcNab,
                     OXS_DEMAG_REAL_ASYMP& pbcNcd,
                     OXS_DEMAG_REAL_ASYMP& pbcNef) const;

  OxsDemagNxxPeriodicZ Nxx;
  OxsDemagNxyPeriodicZ Nxy;
  OxsTDemagNabPeriodicC<OxsDemagNxyPeriodicX,OxsDemagZXYAdapter> Nxz;

  OxsTDemagNabPeriodicC<OxsDemagNxxPeriodicZ,OxsDemagYXZAdapter> Nyy;
  OxsTDemagNabPeriodicC<OxsDemagNxyPeriodicX,OxsDemagZYXAdapter> Nyz;
  OxsTDemagNabPeriodicC<OxsDemagNxxPeriodicX,OxsDemagZYXAdapter> Nzz;
};

#endif // _OXS_DEMAGCOEF
