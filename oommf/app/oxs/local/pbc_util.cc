#include <math.h>
#include <stdlib.h>   
#include "nb.h"
#include "pbc_util.h"
#include "demagcoef.h"

#define ADD_INF 1
////////////////////////////////////////////////////////////////////////////
// Routines to do accurate summation
extern "C" {
static int AS_Compare(const void* px,const void* py)
{
  // Comparison based on absolute values
  double x=fabs(*((const double *)px));
  double y=fabs(*((const double *)py));
  if(x<y) return 1;
  if(x>y) return -1;
  return 0;
}
}

OC_REALWIDE
AccurateSum(int n,OC_REALWIDE *arr)
{
  // Order by decreasing magnitude
  qsort(arr,n,sizeof(OC_REALWIDE),AS_Compare);

  OC_REALWIDE sum,corr,y,u,t,v,z,x,tmp;

  sum=arr[0]; corr=0;
  for(int i=1;i<n;i++) {
    x=arr[i];
    y=corr+x;
    tmp=y-corr;
    u=x-tmp;
    t=y+sum;
    tmp=t-sum;
    v=y-tmp;
    z=u+v;
    sum=t+z;
    tmp=sum-t;
    corr=z-tmp;
  }
  return sum;
}	


double
DemagTensorNormal(enum TensorComponent comp,double x,double y,double z,double a,double b,double c)
{
	switch(comp){
		case xx:return Oxs_CalculateSDA00(x,y,z,a,b,c);
		case yy:return Oxs_CalculateSDA00(y,x,z,b,a,c);
		case zz:return Oxs_CalculateSDA00(z,y,x,c,b,a);
		case xy:return Oxs_CalculateSDA01(x,y,z,a,b,c);
		case xz:return Oxs_CalculateSDA01(x,z,y,a,c,b);
		case yz:return Oxs_CalculateSDA01(y,z,x,b,c,a);
	}
	return 0; // Dummy return
}


double
DemagTensorAsymptotic(enum TensorComponent comp,double x,double y,double z,double a,double b,double c)
{
	switch(comp){
		case xx:return Oxs_DemagNxxAsymptotic(a,b,c).NxxAsymptotic(x,y,z);
		case yy:return Oxs_DemagNxxAsymptotic(b,a,c).NxxAsymptotic(y,x,z);
		case zz:return Oxs_DemagNxxAsymptotic(c,b,a).NxxAsymptotic(z,y,x);
		case xy:return Oxs_DemagNxyAsymptotic(a,b,c).NxyAsymptotic(x,y,z);
		case xz:return Oxs_DemagNxyAsymptotic(a,c,b).NxyAsymptotic(x,z,y);
		case yz:return Oxs_DemagNxyAsymptotic(b,c,a).NxyAsymptotic(y,z,x);
	}
	return 0; // Dummy return
}

double 
DemagTensorDipolar(enum TensorComponent comp,double x,double y,double z){
switch(comp)
{
	case xx:return DemagNxxDipolar(x,y,z);
	case yy:return DemagNxxDipolar(y,x,z);
	case zz:return DemagNxxDipolar(z,y,x);
	case xy:return DemagNxyDipolar(x,y,z);
	case xz:return DemagNxyDipolar(x,z,y);
	case yz:return DemagNxyDipolar(y,z,x);
}
return 0; // Dummy return
}


double 
DemagTensorInfinite(enum TensorComponent comp,double x,double y,double z,double X0,double Y0)
{
	switch(comp){
		case xx:return Nxxinf(x,y,z,X0,Y0);
		case yy:return Nyyinf(x,y,z,X0,Y0);
		case zz:return Nzzinf(x,y,z,X0,Y0);
		case xy:return Nxyinf(x,y,z,X0,Y0);
		case xz:return Nxzinf(x,y,z,X0,Y0);
		case yz:return Nyzinf(x,y,z,X0,Y0);
	}
	return 0; // Dummy return
}


