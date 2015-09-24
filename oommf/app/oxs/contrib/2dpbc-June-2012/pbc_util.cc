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

double
AccurateSum(int n,double *arr)
{
  // Order by decreasing magnitude
  qsort(arr,n,sizeof(double),AS_Compare);

  double sum,corr,y,u,t,v,z,x,tmp;

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
		case xx:return CalculateSDA00(x,y,z,a,b,c);
		case yy:return CalculateSDA00(y,x,z,b,a,c);
		case zz:return CalculateSDA00(z,y,x,c,b,a);
		case xy:return CalculateSDA01(x,y,z,a,b,c);
		case xz:return CalculateSDA01(x,z,y,a,c,b);
		case yz:return CalculateSDA01(y,z,x,b,c,a);
	}
}


double
DemagTensorAsymptotic(enum TensorComponent comp,double x,double y,double z,double a,double b,double c)
{
	switch(comp){
		case xx:return DemagNxxAsymptotic(x,y,z,a,b,c);
		case yy:return DemagNxxAsymptotic(y,x,z,b,a,c);
		case zz:return DemagNxxAsymptotic(z,y,x,c,b,a);
		case xy:return DemagNxyAsymptotic(x,y,z,a,b,c);
		case xz:return DemagNxyAsymptotic(x,z,y,a,c,b);
		case yz:return DemagNxyAsymptotic(y,z,x,b,c,a);
	}
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

}


