/* FILE: magelt.cc           -*-Mode: c++-*-
 *
 * This file contains class definitions for micromagnetic element classes.
 *
 */

/* Revision history:
 *  6-May-00: NOTE: The "field" functions return the average
 *            field in the cell, the "energy" terms return the
 *            "effective *energy*" for the cell, as opposed to the
 *            energy density.  The field is the derivative of
 *            the energy density with respect to m.  The
 *             "effective energy" is the energy density multiplied
 *            by the material thickness at that cell.
 *  5-May-00: Changed "moment" member variable to "thickness"
 *            which is a more accurate description of the new
 *            use of this value.
 *  2-May-00: Changed anisotropy coefficient back to
 *                 anisotropy_coef = K1/(mu0*OC_SQ(Ms));
 *            Storing moment inside anis_coef turns out to
 *            be a bad idea because if moment is set to 0,
 *            then anis_coef gets adjusted to 0, so if later
 *            moment is set to something non-zero we can't
 *            recover the proper value of anis_coef.  This type
 *            of moment changing behavior occurs in the new multi-grey
 *            level geometry mask file routine.  We could outlaw
 *            this behavior, but the initialization code is confusing
 *            enough as it is.  -mjd
 * 27-Apr-00: Added thickness adjustment to exchange term.
 * 18-Apr-00: Renamed anisotropy_coef to anis_coef, and changed it to
 *                 anis_coef = K1*moment/(mu0*OC_SQ(Ms));
 *            instead of
 *                 anisotropy_coef = K1/(mu0*OC_SQ(Ms));
 *            Completed removal of UNIT_MAG code.
 *            Removed energy offsets from anisotropy terms.
 * 21-Nov-97: Added individual link weights, and began removal of
 *            "UNIT_MAG" code.
 *  7-Apr-97: Moved anisotropy vectors from static member to
 *            instantiation specific pointers into external array,
 *            with separate initialization routines.
 *  9-Jul-96: Added support for max angle difference determination
 * 13-May-96: Moved EXCHANGE_COS, EXCHANGE_4NGBR_ANGLE, and
 *            MAGNGBRCNT #defines from magelt.h to magelt.cc
 *  4-May-96: Added support for non-unit magnetizations.
 *  4-May-96: Modified anisotropy_coef to be  
 *                 anisotropy_coef= K1/(mu0*OC_SQ(Ms));
 *            instead of
 *                 anisotropy_coef= -2*K1/(mu0*OC_SQ(Ms));
 *  1-May-96: I decided that *all* the exchange calculations
 *            were too small by a factor of 2. -mjd
 */

#include <cstdio>

#include "magelt.h"

/* End includes */

#undef CODE_CHECKS

// #define exactly one of the following to select the exchange
// field calculation method
#define EXCHANGE_COS
#undef EXCHANGE_4NGBR_ANGLE

// Select neighborhood size for exchange interaction; EXCHANGE_COS
// is setup for MAGNGBRCNT==4 or 8, EXCHANGE_4NGBR_ANGLE only 4.
#define MAGNGBRCNT 8   // Neighborhood size

// #define'd parameters consistency check
#if defined(EXCHANGE_COS) && defined(EXCHANGE_4NGBR_ANGLE)
#error You can only define ONE of EXCHANGE_COS & EXCHANGE_4NGBR_ANGLE
#endif
#if MAGNGBRCNT>MAXMAGNGBRCNT
#error MAGNGBRCNT must not be larger than MAXMAGNGBRCNT
#endif
#if defined(EXCHANGE_COS)
#if MAGNGBRCNT!=4 && MAGNGBRCNT!=8
#error Only currently supported values for MAGNGBRCNT are 4 and 8
#endif
#elif defined(EXCHANGE_4NGBR_ANGLE)
#if MAGNGBRCNT!=4
#error EXCHANGE_4NGBR_ANGLE only supports MAGNGBRCNT==4
#endif
#else
#error You must define one of EXCHANGE_COS & EXCHANGE_4NGBR_ANGLE
#endif

typedef double (MagElt::*MEAEFP)(void); // MagElt Anis. Energy Function Ptr
typedef const ThreeVector& (MagElt::*MEAFFP)(void); // ME Anis. field fnc. ptr

// Class static variable definitions
double MagElt::exchange_stiffness=0.0;  //  J/m (say 21e-12)
double MagElt::Ms=1.0;                  //  A/m (say 1.7e6)
double MagElt::mesh_size=0.0;           //  m   (say 10e-9)
double MagElt::exchange_coef=0.0;       // Derived from others. (say 0.0578)
MagEltAnisType MagElt::meat=meat_unknown;
MEAEFP MagElt::AnisotropyEnergy=(MEAEFP)NULL;
MEAFFP MagElt::AnisotropyField=(MEAFFP)NULL;

void MagElt::MagInit(void)
{
  int i;
  thickness=1.0;  // Default is unit thickness.
  Ny_correction=0.0;  // No correction if thickness == 1.0
  ngbrcnt=0;
  anisotropy_coef=0.0;  // Safety initialization.
  for(i=0;i<MAGNGBRCNT;i++) {
    ngbr[i].node=(MagElt*)NULL;
    ngbr[i].weight=0.;
  }
}

MagElt::MagElt(void):ThreeVector(vecarr)
{
  MagInit();
}

MagElt::~MagElt(void)
{
}

int MagElt::GetMaxNgbrcnt(void) { return MAGNGBRCNT; }

void MagElt::SetupConstants(double new_Ms,
			    double new_exchange_stiffness,
			    double new_mesh_size,MagEltAnisType new_meat)
{
  if(new_Ms>0.0)  Ms = new_Ms;
  else            Ms = 1e5;    // Dummy scaling value
  exchange_stiffness = new_exchange_stiffness;
  if(new_mesh_size>0.0)  mesh_size = new_mesh_size;
  else                   mesh_size = 10e-9;  // Dummy scaling value
  exchange_coef=exchange_stiffness/(mu0*OC_SQ(Ms)*OC_SQ(mesh_size));

  switch(meat=new_meat)
    {
    case meat_cubic:
       AnisotropyEnergy=&MagElt::CubicAnisotropyEnergy;
       AnisotropyField=&MagElt::CubicAnisotropyField;
       break;
    case meat_gencubic:
       AnisotropyEnergy=&MagElt::GenCubicAnisotropyEnergy;
       AnisotropyField=&MagElt::GenCubicAnisotropyField;
       break;
    case meat_uniaxial:
       AnisotropyEnergy=&MagElt::UniaxialAnisotropyEnergy;
       AnisotropyField=&MagElt::UniaxialAnisotropyField;
       break;
    default:
      PlainError(1,"Error in MagElt::SetupConstants:"
		 " Unknown MagEltAnisType: %d\n",(int)new_meat);
      break;
    }
}

void MagElt::SetupNeighbors(int new_ngbrcnt,MagEltLink* myngbrs)
{
  if(new_ngbrcnt>MAGNGBRCNT)
    PlainError(1,"Error in MagElt::SetupNeighbors: Too many neighbors");
  ngbrcnt=new_ngbrcnt;
  int i;
  for(i=0;i<ngbrcnt;i++) ngbr[i]=myngbrs[i];
}

void MagElt::QuickInitAnisDirs(double* arr_a,double* arr_b,double* arr_c)
{ // Sets up anisotropy directions without consistency checks.
  // This can be called directly by magelt array copy routines where
  // the consistency check has already been performed once by the
  // original magelt array.
  AnisDirA.Set(arr_a);
  AnisDirB.Set(arr_b);
  AnisDirC.Set(arr_c);
}

void MagElt::InitAnisDirs(double* arr_a,double* arr_b,double* arr_c)
{
  QuickInitAnisDirs(arr_a,arr_b,arr_c);
  if(fabs(AnisDirA.MagSq()-1)>8*EPSILON) {
    fprintf(stderr,"AnisDirA: (%.17g,%.17g,%.17g), Norm Sq=1+%.7g\n",
	    arr_a[0],arr_a[1],arr_a[2],AnisDirA.MagSq()-1);
    PlainError(1,"Error in MagElt::InitAnisDirs: "
	     "Anisotropy direction A (%g,%g,%g) not a unit vector",
	     arr_a[0],arr_a[1],arr_a[2]);
  }
  if(arr_b!=NULL) {
    if(fabs(AnisDirB.MagSq()-1)>8*EPSILON)
      PlainError(1,"Error in MagElt::InitAnisDirs: "
		 "Anisotropy direction B not a unit vector");
  }
  if(arr_c!=NULL) {
    if(fabs(AnisDirC.MagSq()-1)>8*EPSILON) 
      PlainError(1,"Error in MagElt::InitAnisDirs: "
		 "Anisotropy direction C not a unit vector");
  }
  if(arr_a!=NULL && arr_b!=NULL && arr_c!=NULL) {
    if(fabs(AnisDirA*AnisDirB)>4*EPSILON ||
       fabs(AnisDirB*AnisDirC)>4*EPSILON ||
       fabs(AnisDirC*AnisDirA)>4*EPSILON)
      PlainError(1,"Error in MagElt::InitAnisDirs:\n"
		 " Anisotropy directions not mutually orthogonal:\n"
		 " (%g,%g,%g) (%g,%g,%g) (%g,%g,%g)",
		 AnisDirA[0],AnisDirA[1],AnisDirA[2],
		 AnisDirB[0],AnisDirB[1],AnisDirB[2],
		 AnisDirC[0],AnisDirC[1],AnisDirC[2]);
  }
}

void MagElt::GetAnisDirs(double* &anis_dir_a,double* &anis_dir_b,
			 double* &anis_dir_c) const
{
  anis_dir_a=AnisDirA;
  anis_dir_b=AnisDirB;
  anis_dir_c=AnisDirC;
}


void MagElt::CopyData(const MagElt& mother)
{ // Copies vector, thickness, Ny_correction, and K1 info
  int i;
  for(i=0;i<VECDIM;i++) arr[i]=mother.arr[i];
  anisotropy_coef=mother.anisotropy_coef;
  thickness=mother.thickness;
  Ny_correction=mother.Ny_correction;
#ifdef CODE_CHECKS
  double sizesq=0.0;  for(i=0;i<VECDIM;i++) sizesq+=OC_SQ(arr[i]);
  if(fabs(sizesq-1.)>SQRT_EPSILON)
    PlainError(1,"Error in MagElt::CopyData: "
	       "Non-unit magnetization direction detected: (%f,%f,%f)",
	       arr[0],arr[1],arr[2]);
#endif
}

void MagElt::DumpMag(double *outarr)
{ // Fills outarr with magnetization vector (direction x thickness)
  for(int i=0;i<VECDIM;i++) outarr[i]=thickness*arr[i];
}

double MagElt::GetMinNeighborDot(void) const
{ // Returns minimum value of
  // dot product of moment *direction* with that of neighbors.
  // NOTE: 0-thickness elements *should* have no neighbors, and
  //  therefore return 1.0 from this function.
  double dot,mindot;
  mindot=1.0; // *SHOULD* be maximum possible value!
  for(int i=0;i<ngbrcnt;i++) {
    dot = (*this)*(*ngbr[i].node);
    if(dot<mindot) mindot=dot;
  }
  if(fabs(mindot)>1+2*VECDIM*SQRT_EPSILON) { // The "2" shouldn't be needed...
    PlainError(1,"Error in MagElt::GetMinNeighborDot: %s (Mindot=1+%.15g)",
	       ErrProgramming,mindot-1.);
  }
  if(mindot >=  1.0) return  1.0;  // Allow for roundoff error
  if(mindot <= -1.0) return -1.0;  //        ""
  return mindot;
}

int MagElt::IsBoundary(void) const
{
  if(ngbrcnt<MAGNGBRCNT) return 1;
  for(int i=0;i<ngbrcnt;i++)
    if(fabs(thickness-ngbr[i].node->thickness) >
       1e-8*(thickness+ngbr[i].node->thickness)) return 1;
  return 0;
}

ThreeVector& MagElt::PerpComp(const ThreeVector& v) const
{ // Returns component of v perpendicular to m;  This equation used is
  // v-(1+(v-m)*m)m, which is more accurate for v||m than v-(v*m)m
  // NOTE: ASSUMES |(*this)|==1.0.
  static double temparr[VECDIM],tempcoef;
  static ThreeVector tempvec(temparr);
  tempvec=v;  tempvec-=(*this); tempcoef=-1*(1+tempvec*(*this));
  tempvec.FastAdd(v,tempcoef,*this);
  return tempvec;
}

double MagElt::AngleBetween(const MagElt& other) const
{ // Returns angle between *this and other, in the range [0,PI].
  // NOTE: This routine _ASSUMES_ the vector components are UNIT VECTORS.
  //       If this is not the case then this routine must be modified.
  //       This routine is *not* affected by non-unit this->thickness,
  //       however.
  // NOTE2: This routine uses the arcsin to calculate the angle, which
  //        should be more accurate for small angles than the arccos.
  double angle;
#ifdef CODE_CHECKS
  if(fabs(other.MagSq()-1.)>SQRT_EPSILON || fabs(MagSq()-1.)>SQRT_EPSILON)
    PlainError(1,"Error in MagElt::AngleBetween(MagElt&): "
	       "Non-unit magnetization vector detected; |*this|^2=1+%g, |other|^2=1+%g",
	       other.MagSq()-1.,MagSq()-1.);
#endif // CODE_CHECKS
  double delta=0;
  for(int i=0;i<VECDIM;i++) delta+=OC_SQ(other.arr[i]-arr[i]);
  delta=sqrt(delta)/2.0;
#ifdef CODE_CHECKS
  if(delta<0.0 || delta>1.0)
    PlainError(1,"Error in MagElt::AngleBetween(MagElt&): "
	       "Vectors too far apart (not unit vectors?)");
#endif // CODE_CHECKS
  angle=2.0*asin(delta);
  return angle;
}

#ifdef EXCHANGE_COS
ThreeVector& MagElt::CalculateExchange(void) const
{
  // NOTE: This routine relies on the ngbr[i].weight values being
  // properly set.
  static double resultarr[VECDIM];
  static ThreeVector result(resultarr);
  MagElt *node;
  double x,y,z,x0,y0,z0,temp;
  x0=arr[0]; y0=arr[1]; z0=arr[2];
  x=0.0; y=0.0; z=0.0;
  for(int i=0;i<ngbrcnt;i++) {
    node=ngbr[i].node;
    temp=ngbr[i].weight;
    x+=temp*((*node)[0]-x0);
    y+=temp*((*node)[1]-y0);
    z+=temp*((*node)[2]-z0);
    /// Remove good guess at m component.  This will give 0 field
    /// if neighbors are all aligned, and also improves numerical
    /// precision.
  }

  // Scale appropriately.
#if MAGNGBRCNT==8
  temp = exchange_coef*(2./3.);
#else // MAGNGBRCNT==4
  temp = 2*exchange_coef;
#endif
  resultarr[0]=temp*x;
  resultarr[1]=temp*y;
  resultarr[2]=temp*z;

  return result;
}

double MagElt::CalculateExchangeEnergy(void) const
{
  // NOTE: This routine relies on the ngbr[i].weight values being
  // properly set.
  MagElt *node;
  double x,y,z,x0,y0,z0,temp;
  x=0.0; y=0.0; z=0.0;
  x0=arr[0]; y0=arr[1]; z0=arr[2];
  for(int i=0;i<ngbrcnt;i++) {
    node=ngbr[i].node;
    temp=ngbr[i].weight;
    x += temp * (x0 - (*node)[0]);
    y += temp * (y0 - (*node)[1]);
    z += temp * (z0 - (*node)[2]);
  }
  double energy= x0 * x + y0 * y + z0 * z;
  /// NOTE: The above code block has been carefully tuned
  ///   to work well with the exchange field calculation
  ///   in the case where the spins are all nearly aligned,
  ///   which is problematic due to round-off errors.  (This
  ///   is less of a problem on machines with extra hardware
  ///   guard digits, e.g., those with x86 cpu's.)

  // Scale appropriately.   The multiplication by "thickness" converts from
  // energy density (which is related to exchange field) to representative
  // cell energy.
#if MAGNGBRCNT==8
  energy*= (1./3.) * exchange_coef * thickness;
#else // MAGNGBRCNT==4
  energy*= exchange_coef * thickness;
#endif

  return energy;
}
#endif // EXCHANGE_COS

#ifdef EXCHANGE_4NGBR_ANGLE
// A beefed-up exchange field calculation.  It varies the most from
// the simple \sum m[i]-m[0] approximation at large angles
// NOTE 1: ASSUMES MagElt's vector entries form unit vectors.  It works
//         okay with non-unit this->thickness, however.
//
// NOTE 2: This code predates and does not use the ngbr[i].weight
// values.  If someone wants to use this code with non-trivial
// geometries, then this should be corrected.
#error EXCHANGE_4NGBR_ANGLE routines require updating to work with ngbr[i].weight code.
ThreeVector& MagElt::CalculateExchange(void) const
{
  static double resultposarr[VECDIM],resultnegarr[VECDIM];
  static double temparr[VECDIM],tempcoef,angle;
  static ThreeVector resultpos(resultposarr),resultneg(resultnegarr);
  static ThreeVector tempvec(temparr),*w;
  int i;
  for(i=0;i<VECDIM;i++) resultpos[i]=resultneg[i]=0.0;
  for(i=0;i<ngbrcnt;i++) {
    if(this==ngbr[i].node) continue;
    w=ngbr[i].node;
    // Calculate the component of ngbr[i].node-(*this) perpendicular
    // to (*this).  The equation below is better for small angles
    // than ngbr[i].node-(ngbr[i].node-(*this))(*this).
    tempvec = *w; tempvec-= *this; tempcoef= -1*(1+(tempvec*(*this)));
    tempvec.FastAdd(*w,tempcoef,*this);

    // Size tempvec by angle difference; For very small angles sin(angle)
    // and angle are nearly identical, so resizing is not necessary.  Moreover,
    // tempvec.Scale will return random vector if |tempvec|<EPSILON, so we
    // skip the scaling step if angle is very small; we use 2*EPSILON since
    // sin(angle)<angle.
    if((angle=AngleBetween(*(ngbr[i].node)))>2*EPSILON)
      tempvec.Scale(angle * (ngbr[i].node)->thickness);
    tempvec.SignAccum(resultpos,resultneg);
  }
  resultpos+=resultneg;
  resultpos *= 2*exchange_coef;
  return resultpos;
}

double MagElt::CalculateExchangeEnergy(void) const
{
  MagElt *node;
  double energy=0.0,angle;
  for(int i=0;i<ngbrcnt;i++) {
    node=ngbr[i].node;
    angle=AngleBetween(*node);
    energy+= node->thickness * OC_SQ(angle);
  }
  return energy*exchange_coef*0.5*thickness;
  // NOTE: I hope this is the right value. (4-May-96, mjd)
}
#endif // EXCHANGE_4NGBR_ANGLE


void MagElt::Perturb(double max_mag)
{ // Perturbs magnetization, by at most max_mag (relative to
  // the unit vector component of *this.
  ThreeVector::Perturb(max_mag);
  Scale(1.0);  // This would need to be changed if (*this)[]
               // is not a unit vector.
}


// Standard Cubic anisotropy: crystal axes along coordinate axes
double MagElt::CubicAnisotropyEnergy(void)
{
  double masq=OC_SQ(arr[0]);
  double mbsq=OC_SQ(arr[1]);
  double mcsq=OC_SQ(arr[2]);
  double energy=masq*(mbsq+mcsq)+mbsq*mcsq;
  if(anisotropy_coef<0) energy -= 1./3.;  // Offset to make
  /// energy non-negative.  This should slightly improve
  /// numerical behavior near equilibrium.
  return energy*anisotropy_coef*thickness;
}

const ThreeVector& MagElt::CubicAnisotropyField(void)
{
  static double resultarr[VECDIM];
  static ThreeVector result(resultarr);
  double coef = -2*anisotropy_coef;
  double masq=OC_SQ(arr[0]);
  double mbsq=OC_SQ(arr[1]);
  double mcsq=OC_SQ(arr[2]);
  result[0]= coef*arr[0]*(mbsq+mcsq);
  result[1]= coef*arr[1]*(mcsq+masq);
  result[2]= coef*arr[2]*(masq+mbsq);
  return result;
}

// General Cubic anisotropy: crystal axes given by AnisDir<A,B,B>
double MagElt::GenCubicAnisotropyEnergy(void)
{
  double masq=(*this)*AnisDirA; masq*=masq;
  double mbsq=(*this)*AnisDirB; mbsq*=mbsq;
  double mcsq=(*this)*AnisDirC; mcsq*=mcsq;

  double energy=masq*mbsq+masq*mcsq+mbsq*mcsq;
  if(anisotropy_coef<0) energy -= 1./3.;  // Offset to make
  /// energy non-negative.  This should slightly improve
  /// numerical behavior near equilibrium.

  return energy*anisotropy_coef*thickness;
}

const ThreeVector& MagElt::GenCubicAnisotropyField(void)
{
  static double resultarr[VECDIM];
  static ThreeVector result(resultarr);
  // Convert m to <a,b,c> coordinates
  double ma=(*this)*AnisDirA;    double masq=OC_SQ(ma);
  double mb=(*this)*AnisDirB;    double mbsq=OC_SQ(mb);
  double mc=(*this)*AnisDirC;    double mcsq=OC_SQ(mc);

  // Calculate field in <a,b,c> coordinates
  double coef = -2*anisotropy_coef;
  double ha= coef*ma*(mbsq+mcsq);
  double hb= coef*mb*(masq+mcsq);
  double hc= coef*mc*(masq+mbsq);

  // Convert to <x,y,z> coordinates
  for(int i=0;i<VECDIM;i++)
    result[i]=ha*AnisDirA[i]+hb*AnisDirB[i]+hc*AnisDirC[i];

  return result;
}


// Uniaxial anisotropy
double MagElt::UniaxialAnisotropyEnergy(void)
{
  double dot=(*this)*AnisDirA;
  double energy= -dot*dot;
  if(anisotropy_coef>=0.0) {
    energy += 1.0;  // Makes minimum energy value = 0.0.  If
    /// the equilibrium position is near the minimum (i.e.,
    /// m aligned with AnisDirA), then it is numerically
    /// preferable to have this offset.
  }
  return energy*anisotropy_coef*thickness;
}

const ThreeVector& MagElt::UniaxialAnisotropyField(void)
{
  static double resultarr[VECDIM];
  static ThreeVector result(resultarr);
#if VECDIM!=3
#error This code _assumes_ VECDIM == 3
#endif
  double x=AnisDirA[0];
  double y=AnisDirA[1];
  double z=AnisDirA[2];
  double dot = 2*anisotropy_coef*(arr[0]*x+arr[1]*y+arr[2]*z);
  resultarr[0]=x*dot;  resultarr[1]=y*dot;  resultarr[2]=z*dot;
  return result;
}

