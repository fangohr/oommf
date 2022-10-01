#include <cmath>
#include <cstdlib>   
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
        case xx:return static_cast<double>(Oxs_CalculateNxx(x,y,z,a,b,c).Hi());
        case yy:return static_cast<double>(Oxs_CalculateNxx(y,x,z,b,a,c).Hi());
        case zz:return static_cast<double>(Oxs_CalculateNxx(z,y,x,c,b,a).Hi());
        case xy:return static_cast<double>(Oxs_CalculateNxy(x,y,z,a,b,c).Hi());
        case xz:return static_cast<double>(Oxs_CalculateNxy(x,z,y,a,c,b).Hi());
        case yz:return static_cast<double>(Oxs_CalculateNxy(y,z,x,b,c,a).Hi());
	}
	return 0; // Dummy return
}


double
DemagTensorAsymptotic(enum TensorComponent comp,double x,double y,double z,double a,double b,double c)
{
	switch(comp){
		case xx:return Oxs_DemagNxxAsymptotic(a,b,c).Asymptotic(x,y,z);
		case yy:return Oxs_DemagNxxAsymptotic(b,a,c).Asymptotic(y,x,z);
		case zz:return Oxs_DemagNxxAsymptotic(c,b,a).Asymptotic(z,y,x);
		case xy:return Oxs_DemagNxyAsymptotic(a,b,c).Asymptotic(x,y,z);
		case xz:return Oxs_DemagNxyAsymptotic(a,c,b).Asymptotic(x,z,y);
		case yz:return Oxs_DemagNxyAsymptotic(b,c,a).Asymptotic(y,z,x);
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


