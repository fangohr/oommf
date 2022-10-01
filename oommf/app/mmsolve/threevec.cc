/* FILE: threevec.cc           -*-Mode: c++-*-
 *
 * This file contains class definitions for vector classes.
 *
 */

#include <cstdio>

#include "constants.h"
#include "threevec.h"
#include "nb.h"

/* End includes */

#undef CODE_CHECKS

/*
 * Cf. notes on vector classes in threevec.h
 */

// Class static variable definitions
double ThreeVector::zeroarr[VECDIM]={0.0,0.0,0.0};
ThreeVector ThreeVector::zero(ThreeVector::zeroarr);

ThreeVector::ThreeVector(void)
{
  arr=NULL;  // Note: With luck, accessing a NULL pointer will cause
             //       a segmentation error.
}

ThreeVector::ThreeVector(double* array)
{
  Init(array);
}

ThreeVector& ThreeVector::Set(double a,double b,double c)
{
  arr[0]=a; arr[1]=b; arr[2]=c;
  return *this;
}

void ThreeVector::Init(double* array)
{
  if(array==NULL)
    PlainError(1,"Error in ThreeVector::Init: %s",ErrBadParam);
  arr=array;
  for(int i=0;i<VECDIM;i++) arr[i]=0.0;
}

void ThreeVector::ReleaseArray(void)
{ // Resets arr to point NULL
  arr=NULL;
}

ThreeVector::~ThreeVector(void)
{
#ifdef CODE_CHECKS
  arr=NULL;  // Just for destructor testing
#endif // CODE_CHECKS
  return;
}

int ThreeVector::Alloc2DArray(int dim1,int dim2,ThreeVector** &v)
{ // IMPORTANT NOTE: This routine allocates everything as one dimensional
  //   arrays, and then sets up pointers into that array.  This memory
  //   *must* be deallocated with ThreeVector::Release2DArray
  int i,j;
  if(dim1<1 || dim2<1 || dim1*dim2*VECDIM<1) // The last is an overflow check
    PlainError(1,"Error in ThreeVector::Alloc2DArray: %s"
	       " (dim1=%d, dim2=%d)",ErrBadParam,dim1,dim2);

  // Allocate one big chunk of memory for the doubles
  double *dptr;
  if((dptr=new double[dim1*dim2*VECDIM])==0)
    PlainError(1,"Error in ThreeVector::Alloc2DArray: %s",ErrNoMem);
  // Allocate one big chunk of memory for the ThreeVectors
  ThreeVector *vptr;
  if((vptr=new ThreeVector[dim1*dim2])==0)
    PlainError(1,"Error in ThreeVector::Alloc2DArray: %s",ErrNoMem);
  // Allocate ThreeVector pointers
  if((v=new ThreeVector*[dim1])==0)
    PlainError(1,"Error in ThreeVector::Alloc2DArray: %s",ErrNoMem);

  // Initialize ThreeVector pointers and ThreeVectors
  for(i=0;i<dim1;i++) {
    v[i]=vptr+i*dim2;
    for(j=0;j<dim2;j++) v[i][j].Init(dptr+((i*dim2+j)*VECDIM));
  }
  return 0;
}

void ThreeVector::Release2DArray(ThreeVector** &v)
{
  if(v==NULL) return;
  double *dptr=v[0][0].arr;
  ThreeVector *vptr=v[0];
  delete[] v;
  delete[] vptr;
  delete[] dptr;
}

void ThreeVector::Dup(const double* array)
{
#ifdef CODE_CHECKS
  if(array==NULL)
    PlainError(1,"Error in ThreeVector::Dup: %s",ErrBadParam);
#endif
  for(int i=0;i<VECDIM;i++) arr[i]=array[i];  
}

ThreeVector& ThreeVector::operator*(double a)
{ // This is actually pretty broken.  Don't use in compound statements
  static double dumarr[VECDIM];
  static ThreeVector dummy(dumarr);
  for(int i=0;i<VECDIM;i++) dummy[i]=arr[i]*a;
  return dummy;
}

double PreciseDot(const ThreeVector& u,const ThreeVector& v)
{ // Extra-precision dot product.
  int i=0; double *uptr=u.arr,*vptr=v.arr;
  Nb_Xpfloat dot(0.0);
  while( i++ < VECDIM ) dot.Accum((*(uptr++))*(*(vptr++)));
  return dot.GetValue();  
}

ThreeVector& ThreeVector::operator^(const ThreeVector& v) // Cross product
{ // We need two dummy arrays, dumarr and resultarr, to hold 
  // the intermediate results and the final results.  These need to
  // be held seperately because compound products will call this routine
  // with v.arr==resultarr, and we don't want these values to change
  // in the middle of the calculation, as would happen if
  // dumarr==resultarr==v.arr. (This is a problem with aliasing.)

  // We return by value rather than reference because otherwise
  // compound products will cause an intermediate &v = &dummy to be
  // produced, which will not be processed properly (the first lines
  // modify for example dummy[0] which is identical to v[0] used in
  // the remaining code lines.
  static double dumarr[VECDIM],resultarr[VECDIM];
  static ThreeVector dummy(dumarr),result(resultarr);

  dumarr[0]=arr[1]*v.arr[2]-arr[2]*v.arr[1];
  dumarr[1]=arr[2]*v.arr[0]-arr[0]*v.arr[2];
  dumarr[2]=arr[0]*v.arr[1]-arr[1]*v.arr[0];

  result=dummy;  // Hmm, be sure compiler optimization doesn't drop the
  return result; // intermediate storage into dummy (aliasing problems).
}

ThreeVector& ThreeVector::operator^=(const ThreeVector& v) // Cross product
{
  double t0,t1,t2;

  t0=arr[0];
  arr[0]=(t1=arr[1])*v.arr[2]-(t2=arr[2])*v.arr[1];  // t2 not really needed
  arr[1]=t2*v.arr[0]-t0*v.arr[2];
  arr[2]=t0*v.arr[1]-t1*v.arr[0];

  return *this;
}

ThreeVector& ThreeVector::operator+(const ThreeVector& v)
{
  static double dumarr[VECDIM];
  static ThreeVector dummy(dumarr);
  for(int i=0;i<VECDIM;i++) dummy.arr[i]=arr[i]+v.arr[i];
  return dummy;
}

ThreeVector& ThreeVector::operator-(const ThreeVector& v)
{
  static double dumarr[VECDIM];
  static ThreeVector dummy(dumarr);
  for(int i=0;i<VECDIM;i++) dummy.arr[i]=arr[i]-v.arr[i];
  return dummy;
}

double ThreeVector::Mag(void) const
{
  double magsq=MagSq();
  if(magsq<0.0) return 0.0;
  else          return sqrt(magsq);
}

void ThreeVector::Random(double magnitude)
{
  double theta,costheta,sintheta,cosphi,sinphi;
  theta=Oc_UnifRand()*2*PI;
  costheta=cos(theta);
  sintheta=sqrt(OC_MAX(0.,1.-OC_SQ(costheta)));
  if(theta>PI) sintheta*=-1;
  cosphi=1-2*Oc_UnifRand();
  sinphi=sqrt(OC_MAX(0.,1.-OC_SQ(cosphi)));
  arr[0]=sinphi*costheta;
  arr[1]=sinphi*sintheta;
  arr[2]=cosphi;
  Scale(magnitude); // Rounding errors in the above may make arr[]
  // not *quite* a unit vector (I've seen errors as large as 3e-7),
  // so we take the longer but more accurate renormalization route.
}

void ThreeVector::Perturb(double max_mag)
{
  if(max_mag==0.0) return; // No perturbation
  double theta,phi,mag;
  theta=Oc_UnifRand()*2*PI;
  phi=acos(1-2*Oc_UnifRand());
  mag=Oc_UnifRand()*max_mag;
  arr[0]+=mag*sin(phi)*cos(theta);
  arr[1]+=mag*sin(phi)*sin(theta);
  arr[2]+=mag*cos(phi);
}

void ThreeVector::Scale(double new_mag)
{
  double sumsq,mag;
  int i;
  for(i=0,sumsq=0.0;i<VECDIM;i++) sumsq+=OC_SQ(arr[i]);
  if(sumsq<OC_SQ(EPSILON)) Random(new_mag);
  else {
    mag=new_mag/sqrt(sumsq);
    for(i=0;i<VECDIM;i++) arr[i]*=mag;
  }  
}

void ThreeVector::PreciseScale(double new_mag)
{ // Slower than ::Scale, but with improved precision
  double mag;
  Nb_Xpfloat sumsq(0.0);
  int i;
  for(i=0;i<VECDIM;i++) sumsq.Accum(OC_SQ(arr[i]));
  if(sumsq.GetValue()<OC_SQ(EPSILON)) Random(new_mag);
  else {
    mag=1.0/sqrt(sumsq.GetValue());
    sumsq.Set(0.0);
    for(i=0;i<VECDIM;i++) arr[i]*=mag;
    for(i=0;i<VECDIM;i++) sumsq.Accum(OC_SQ(arr[i]));
    sumsq.Accum(-3.);
    mag= -new_mag*sumsq.GetValue()/2;
    /// new_mag*(Correction term)
    /// This correction reduces error from about 6e-16 to 4e-16.
    /// Not clear how much this is worth, but it doesn't cost
    /// *that* much to do...
    for(i=0;i<VECDIM;i++) arr[i]*=mag;
  }  
}

void ThreeVector::SignAccum(ThreeVector& accumpos,ThreeVector& accumneg)
{
  int i;
  for(i=0;i<VECDIM;i++) {
    if(arr[i]<0) accumneg[i]+=arr[i]; else accumpos[i]+=arr[i];
  }
}

void ThreeVector::Print(FILE* fout) const
{
  fprintf(fout,"(%f,%f,%f)",arr[0],arr[1],arr[2]);
}

/////////////////// Vec3D Function Definitions ////////////////////
void Vec3D::Normalize()
{ // Adjusts size to 1
  double magsq=MagSq();
  if(magsq<OC_SQ(EPSILON)) {
    Random();
    return;
  }
  double maginv=1./sqrt(magsq);
  x*=maginv; y*=maginv; z*=maginv;
}

void Vec3D::Random()
{ // Makes a random unit vector
  double theta,costheta,sintheta,cosphi,sinphi;
  theta=Oc_UnifRand()*2*PI;
  costheta=cos(theta);
  sintheta=sqrt(OC_MAX(0.,1.-OC_SQ(costheta)));
  if(theta>PI) sintheta*=-1;
  cosphi=1-2*Oc_UnifRand();
  sinphi=sqrt(OC_MAX(0.,1.-OC_SQ(cosphi)));
  x=sinphi*costheta;
  y=sinphi*sintheta;
  z=cosphi;
  // Rounding errors in the above may make <x,y,z>
  // not *quite* a unit vector (I've seen errors as large as 3e-7),
  // so we take the longer but more accurate renormalization route.
  double maginv=1./sqrt(MagSq());
  x*=maginv; y*=maginv; z*=maginv;
}
