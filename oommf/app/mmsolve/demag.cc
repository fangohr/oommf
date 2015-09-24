/* demag.8.c  10/30/95 double precision version of fft demag routines */
/* demag.9.c  10/30/95 Four line charge representation of cell charge 
                       change dipole term to pair of monopoles.   */
/*             7/10/96 Rearrangement of fft into multiple 1-d fft's to
                       avoid cache misses */
/*            11/08/96 Added real fft code. -mjd */
/*	       5/08/97 Adapted for new fft  code. -jge*/
/*  Functions Phixy and Upipe have been modified to concentrate */
/*  magnetostatic charge on single line charges at the corners of square */
/*  magnetization elements.  */

#include "oc.h"
#include "nb.h"

#include "demag.h"
#include "fft.h"

/* End includes */

#undef VERBOSE

#define CACHE_FLOP
#ifndef CACHE_SIZE
#define CACHE_SIZE 250000 /* Machine cache size in bytes */
#endif

/* NOTE: To add new demag. calculation functions, append to the following
 * lists, and fill in the DemagIdent array below.  Use the existing
 * functions as a guide to writing your own functions.
 */
static DF_Init     DemagInit;
static DF_Destroy  DemagDestroy;
static DF_Calc     Demag3dSlab,Demag2dSlab,DemagKern,DemagPipe;

extern DF_Init     ConstMagInit;
extern DF_Destroy  ConstMagDestroy;
extern DF_Calc     ConstMagField;

static struct DF_FuncInfo DF_Ident[]={
{ "SlowPipe", 0, NULL, NULL,        NULL,      NULL }, /* Default */
{ "NoDemag",  0, NULL, NULL,        NULL,      NULL }, /* No-demag */
{ "3dSlab",   2, NULL, Demag3dSlab, DemagInit, DemagDestroy },
{ "3dCharge", 2, NULL, DemagKern,   DemagInit, DemagDestroy },
{ "2dSlab",   2, NULL, Demag2dSlab, DemagInit, DemagDestroy },
{ "FastPipe", 1, NULL, DemagPipe,   DemagInit, DemagDestroy },
{ "ConstMag", 2, NULL, ConstMagField, ConstMagInit, ConstMagDestroy },
{ NULL,       0, NULL, NULL,        NULL,      NULL} /* End-of-array mark */
};
/* NOTE: Setting the "calc" field above (the third field after the name)
 *       to "NULL" causes the micromagnetics program to substitute either
 *       its own "internal" (brute-force) demag field calculations, or
 *       to skip the demag calculation altogether (depending on the name
 *       field).
 */

/* The following function performs ASCII name to demag function indexing.
 * The export "fi" is a pointer to a corresponding DF_FuncInfo structure,
 * or NULL if no match is found.  The return value is 0 on success, >0
 * on failure.  Note that "Internal" is included in the list to force
 * use of the built in code (i.e., the brute force approach).  If
 * lookname==NULL then the first entry in DF_Ident is returned.
 */
int DF_LookUpName(char* lookname,struct DF_FuncInfo **fi)
{
    int i;
    if(lookname==NULL) (*fi)=&(DF_Ident[0]);
    else {
	for(i=0,(*fi)=NULL;DF_Ident[i].name!=NULL;i++) {
	    if(Nb_StrCaseCmp(lookname,DF_Ident[i].name)==0) { /* Match! */
		(*fi)=&(DF_Ident[i]);
		break;

	    }
	}
    }
    if((*fi)!=NULL && (*fi)->name!=NULL) return 0; /* Match found */
    return 1;			/* No match found */
}

void DF_List(FILE* output)
{
    /* First determine the number of entries */
    int i,entrycount;
    for(entrycount=0;DF_Ident[entrycount].name!=NULL;entrycount++) ;
    fprintf(output,"%d demag calculation routines available---\n",entrycount);
    for(i=0;i<entrycount;i++) {
	fprintf(output,"%11s with %d arguments",
		DF_Ident[i].name,OC_MAX(DF_Ident[i].paramcount-1,0));
                /* Don't include name in argument count total. */
	if(i%3 == 2) fprintf(output,"\n"); else fprintf(output," ");
    }
    if(i%3 != 3) fprintf(output,"\n");
}
/**********************************************************************/
/***************** Sample demag function realizations *****************/
/**********************************************************************/
static int magx, magy, chargex,chargey, cxdim, cydim, cxdimh,
           c2xdim,c2ydim,c2xdimh;
static double **wr=NULL, **dat=NULL,**RKern=NULL,**ba=NULL,**dxx=NULL,
	      **dxy=NULL,**dyy=NULL,**mx=NULL,**my=NULL;
static MyComplex **wc=NULL,**CKern=NULL,**CZKern=NULL,**Cdxx=NULL,
		 **Cdxy=NULL,**Cdyy,**w2c=NULL;
static double aspect;  
static void CheckInit(int magx, int magy);
static void GetCharge(DF_Export_m* writem,void* m,int magx,int magy);
static void PutHxy(DF_Import_h* readh,void* h,int magx,int magy);
static void FieldCalc(MyComplex **Uk);
static double KernelZ(double a, int ioff, int joff);
static double KernelXY(double a, int ioff, int joff);
static double KernelPipe(int ioff, int joff);
static void dten(double a);
static void InitSlab(double aspect);
static void InitPipes();
static void InitZcalc(double aspect);
static int SecondPowerTwo(int dim);
static void Field2Calc();
/* initializes FFT*/
FFTReal2D FFTthis;                              

static void DemagInit(const int dimx,const int dimy,
		      int paramc, char** params)
{
/*fuction allocates memory and calculates kernels for demag field*/
int i,j,xp,yp;
if(dimx<1 || dimy<1 || paramc<1 ||
       ((Nb_StrCaseCmp(params[0],"2dSlab")!=0 || paramc!=2) &&
	(Nb_StrCaseCmp(params[0],"3dSlab")!=0 || paramc!=2) &&
	(Nb_StrCaseCmp(params[0],"FastPipe")!=0|| paramc!=1)&& 
	(Nb_StrCaseCmp(params[0],"3dCharge")!=0||paramc!=2) )) {
  PlainError(1,"FATAL ERROR: Invalid argument (->%s<- with paramc=%d)"
	     " to DemagInit()\n",params[0],paramc);
}
/*finds values of global integer constants*/
/*************************************************************
magx=x-dimension magnetization matrix
magy=y-dimension magnetization matrix
chargex=x-dimension of charge matrix: magx+1
chargey=y-dimension of charge matrix: magy+1
cxdim=2nd power of two greater than chargex
cxdimh=x-size of complex charge matrix: 2*cxdim-1
cydim=2nd power of two greater than  magy
**************************************************************/  
    magx = dimx;
    magy = dimy;
    chargex=magx+1;
    chargey=magy+1;
    cxdim = SecondPowerTwo(chargex);
    cxdimh = (cxdim/2)+1;
    cydim = SecondPowerTwo(chargey);
    c2xdim = SecondPowerTwo(magx);
    c2xdimh = (c2xdim/2)+1;
    c2ydim = SecondPowerTwo(magy);
    /*allocates space for real arrays*/
    xp=magx+2;
    yp=magy+2;
    wr=new double*[xp];
    dat=new double*[xp];
    ba=new double*[xp];
    RKern=new double*[cxdim];
    wr[0]=new double[xp*yp];
    ba[0]=new double[xp*yp];
    RKern[0]=new double[cxdim*cydim];
    for(i=1;i<xp;i++) {wr[i]=wr[i-1]+yp;ba[i]=ba[i-1]+yp;}
    /*dat points to same array as ba except dat is shifted by x+1,y+1*/    
    for(i = 0; i < xp-1; i++){dat[i] = ba[i+1] + 1;}
    for(i=1;i<cxdim;i++)  {RKern[i]=RKern[i-1]+cydim;}

    /*allocates space for complex arrays*/
    wc=new MyComplex*[cxdimh];
    CKern= new MyComplex*[cxdimh];
    wc[0]=new MyComplex[cxdimh*cydim];
    CKern[0]=new MyComplex[cxdimh*cydim];
    for(i=1;i<cxdimh;i++) {wc[i]=wc[i-1]+cydim;}
    for(i=1;i<cxdimh;i++) {CKern[i]=CKern[i-1]+cydim;}
    
    /*clear unshifted array ba*/
    for (i=0;i<xp;i++) for (j = 0; j < yp; j++ ){
         ba[i][j] = 0.0;} 
   
    /*Initialize interaction arrays */
    if(paramc>1) aspect =(double) atof(params[1]);
    else         aspect = 1.0;  /* Dummy value */
    /*initialize selected routines*/
    if(Nb_StrCaseCmp(params[0],"2dSlab") == 0 )
        InitSlab(aspect);
    else if(Nb_StrCaseCmp(params[0],"FastPipe") == 0 )
        InitPipes();
    else if(Nb_StrCaseCmp(params[0],"3dSlab") == 0) {
	InitSlab(aspect);
	CZKern=new MyComplex*[cxdimh];
	CZKern[0]=new MyComplex[cxdimh*cydim];
	for(i=1;i<cxdimh;i++) {CZKern[i]=CZKern[i-1]+cydim;}
	InitZcalc(aspect);
    }
    else if(Nb_StrCaseCmp(params[0],"3dCharge") == 0) {
	mx=new double*[xp];
	mx[0]=new double[xp*yp];
	for(i=1;i<xp;i++) {mx[i]=mx[i-1]+yp;}
	my=new double*[xp];
	my[0]=new double[xp*yp];
	for(i=1;i<xp;i++) {my[i]=my[i-1]+yp;}
	
	Cdxx=new MyComplex*[c2xdimh];
        Cdxy= new MyComplex*[c2xdimh];
	Cdyy= new MyComplex*[c2xdimh];
        Cdxy[0]=new MyComplex[c2xdimh*c2ydim];
        Cdxx[0]=new MyComplex[c2xdimh*c2ydim];
	Cdyy[0]=new MyComplex[c2xdimh*c2ydim];
   
	for(i=1;i<c2xdimh;i++) {Cdxx[i]=Cdxx[i-1]+c2ydim;}
        for(i=1;i<c2xdimh;i++) {Cdxy[i]=Cdxy[i-1]+c2ydim;}
	for(i=1;i<c2xdimh;i++) {Cdyy[i]=Cdyy[i-1]+c2ydim;}
	dxx=new double*[c2xdim];
	dxx[0]=new double[c2xdim*c2ydim];
	for(i=1;i<c2xdim;i++)  {dxx[i]=dxx[i-1]+c2ydim;}

	dyy=new double*[c2xdim];
	dyy[0]=new double[c2xdim*c2ydim];
	for(i=1;i<c2xdim;i++)  {dyy[i]=dyy[i-1]+c2ydim;}

	dxy=new double*[c2xdim];
	dxy[0]=new double[c2xdim*c2ydim];
	for(i=1;i<c2xdim;i++)  {dxy[i]=dxy[i-1]+c2ydim;}
	

	w2c=new MyComplex*[c2xdimh];
	w2c[0]=new MyComplex[c2xdimh*c2ydim];
	for(i=1;i<c2xdimh;i++)  {w2c[i]=w2c[i-1]+c2ydim;}

	dten(aspect);

	CZKern=new MyComplex*[cxdimh];
	CZKern[0]=new MyComplex[cxdimh*cydim];
	for(i=1;i<cxdimh;i++) {CZKern[i]=CZKern[i-1]+cydim;}
	InitZcalc(aspect);
	delete[] dyy[0]; delete[] dyy;
	delete[] dxx[0]; delete[] dxx;
	delete[] dxy[0]; delete[] dxy;
    }
    /*free memory used for real part of kernel*/
    delete[] RKern[0]; delete[] RKern;
    
}

void dten(double a)
{
int i,j,x,y=0,n,m;
double  xp,yp,zp,xn,yn,zn,c;
double d[8];
#define euclid(x,y,z) (sqrt(x*x+y*y+z*z))
n=c2xdim/2-1;
m=c2ydim/2-1;	
zp=-.5*a;
zn= .5*a;
c=1.0/(4.0*PI);
for(i=-n-1;i<=n;i++) {
    xp=i-.5; xn=i+.5;
    if(i < 0) x = 2*(n+1) +i;
    else      x = i;
    for(j=-m-1;j<=m;j++){
	  yn=j+.5;
	  yp=j-.5;
	  if(j < 0) y = 2*(m+1) +j;
	  else      y = j;  
	  d[0]=euclid(xp,yp,zp);
	  d[1]=euclid(xp,yn,zp);
	  d[2]=euclid(xp,yp,zn);
	  d[3]=euclid(xp,yn,zn);
	  d[4]=euclid(xn,yp,zp);
	  d[5]=euclid(xn,yn,zp);
	  d[6]=euclid(xn,yp,zn);
	  d[7]=euclid(xn,yn,zn);
	  dxx[x][y]=((atan((yp*zp)/(d[0]*xp))-
		     atan((yn*zp)/(d[1]*xp))-
		     atan((yp*zn)/(d[2]*xp))+
		     atan((yn*zn)/(d[3]*xp)))-
		    (atan((yp*zp)/(d[4]*xn))-
		     atan((yn*zp)/(d[5]*xn))-
		     atan((yp*zn)/(d[6]*xn))+
		     atan((yn*zn)/(d[7]*xn))))*c;
	  dyy[x][y]=((atan((xp*zp)/(d[0]*yp))-
                      atan((xp*zp)/(d[1]*yn))-
                      atan((xp*zn)/(d[2]*yp))+
                      atan((xp*zn)/(d[3]*yn)))-
                     (atan((xn*zp)/(d[4]*yp))-
                      atan((xn*zp)/(d[5]*yn))-
                      atan((xn*zn)/(d[6]*yp))+
                      atan((xn*zn)/(d[7]*yn))))*c;
	  dxy[x][y]=(log((((d[2]+zn)*(d[1]+zp))*((d[4]+zp)*(d[7]+zn)))/
			 (((d[0]+zp)*(d[3]+zn))*((d[6]+zn)*(d[5]+zp)))))*c; 
    }
}
FFTthis.Forward(2*n+2,2*m+2,(const double* const*)dxx,c2xdimh,c2ydim,Cdxx);
FFTthis.Forward(2*n+2,2*m+2,(const double* const*)dxy,c2xdimh,c2ydim,Cdxy);
FFTthis.Forward(2*n+2,2*m+2,(const double* const*)dyy,c2xdimh,c2ydim,Cdyy);
}
static void InitSlab(double slab_aspect)
{
    int i,j,x,y;
    x=cxdim/2-1;
    y=cydim/2-1;
    int indx, indy;
    for(i=-x-1;i<x+1;i++) {for(j=-y-1;j<y+1;j++){
	    if(i < 0) indx = 2*(x+1) +i;
	    else indx = i;
	    if(j < 0) indy = 2*(y+1) +j;
	    else indy = j;
		RKern[indx][indy] = KernelXY(slab_aspect,i,j);
	}
    }
  
    FFTthis.Forward(2*x+2,2*y+2,(const double* const*)RKern,
		    cxdimh,cydim,CKern);
}
static void InitPipes()
{
    int i, j,x,y;
    x=cxdim/2-1;
    y=cydim/2-1;
    int indx, indy;
    for(i=-x-1;i<x+1;i++) {for(j=-y-1;j<y+1;j++){
	    if(i < 0) indx = 2*(x+1) +i;
	    else indx = i;
 	    if(j < 0) indy = 2*(y+1) +j;
	    else indy = j;
		RKern[indx][indy] = KernelPipe(i,j);
	
	}
    }
    FFTthis.Forward(2*x+2,2*y+2,(const double* const*)RKern,
		    cxdimh,cydim,CKern);
}
static void InitZcalc(double slab_aspect)
{				
/* Interaction coefficients for z-dipoles */
    int i, j,x,y;
    x=cxdim/2-1;
    y=cydim/2-1;
    int indx, indy;
    for(i=-x-1;i<x+1;i++) for(j=-y-1;j<y+1;j++){
	if(i!=0 || j!=0){
	    if(i < 0) indx = 2*(x+1) +i;
	    else indx = i;
	    if(j < 0) indy = 2*(y+1) +j;
	    else indy = j;
	    RKern[indx][indy] = KernelZ(slab_aspect, i, j);
	}
    }
    RKern[0][0] = -2*asin(1.0/(1.0+OC_SQ(aspect)))/PI; //exact!!! 
    FFTthis.Forward(2*x+2,2*y+2,(const double* const*)RKern,
		    cxdimh,cydim,CZKern);
}
static void GetCharge(DF_Export_m* writem,void* m,int magx_size,int magy_size)
{
    int i, j;
    
    (*writem)(dat,m,magx_size,magy_size,0); /* load x components (0) into dat*/
    /* compute charge density, -d(Mx)/dx */
    for (i=0; i < magx_size+1; i++) for(j=0; j < magy_size+1; j++) {
        wr[i][j] = (ba[i][j] + ba[i][j+1] -ba[i+1][j] -ba[i+1][j+1])*.5;
    }
    (*writem)(dat,m,magx_size,magy_size,1); 
    for (i=0; i < magx_size+1; i++) for(j=0; j < magy_size+1; j++) {
        wr[i][j] += (ba[i][j] + ba[i+1][j] -ba[i][j+1] - ba[i+1][j+1])*.5;
    }
}
static void PutHxy(DF_Import_h* readh,void* h,int magx_size, int magy_size)
{
    int i, j;
    for (i=0;i<magx_size;i++) for(j=0;j<magy_size;j++) { /* x-component */
	dat[i][j] =(wr[i][j] + wr[i][j+1] -wr[i+1][j] - wr[i+1][j+1])*.5;
    }
    (*readh)(dat,h,magx_size,magy_size,0); /* output x-field */
    for (i=0; i<magx_size; i++) for(j=0; j<magy_size; j++) { /* y-component */
	dat[i][j] =(wr[i][j] + wr[i+1][j] -wr[i][j+1] - wr[i+1][j+1])*.5;
    }
    (*readh)(dat,h,magx_size,magy_size,1); /* output y-field */
}
static void FieldCalc(MyComplex **Uk)
{
    int i, j;
    for(i=0; i<cxdimh; i++) for (j=0;j<cydim;j++){
	wc[i][j] *= Uk[i][j];
    }
}
static void Field2Calc()
{
    int i, j;
    MyComplex h1,h2;
    for(i=0; i<c2xdimh; i++) for (j=0;j<c2ydim;j++){
        h1=wc[i][j];
	h2=w2c[i][j];
	wc[i][j] =Cdxx[i][j]*h1+Cdxy[i][j]*h2;
	w2c[i][j]=Cdyy[i][j]*h2+Cdxy[i][j]*h1;
    }
}

/* Interface functions writem and readh must be provided by the user.
   writem writes an magx*magy array of magnetization components into the
   arrays pointed to by dat.  dat is a pointer to the first element of an
   array of magx pointers.  Each of these magx pointers points to an array
   of magy floating point numbers.  writem writes the x, y, and
   z-components for xyz = 0, 1, 2 respectively.  readh reads demag field
   vector components from the dat array and writes them into the users data
   structure: x, y, z components for xyz = 0, 1, 2 respectively.  aspect is
   the ratio of (film thickness)/(2-D cell size).  The cells are assumed
   to be square.
   */

static void Demag3dSlab(DF_Export_m* writem,DF_Import_h* readh,
			void* m,void* h)
{   
    CheckInit(magx,magy);	/* insure the workspace has been allocated */
    GetCharge(writem,m,magx,magy);/* load Mx, My, & compute charge density */
    FFTthis.Forward(chargex,chargey,(const double* const*)wr,
		    cxdimh,cydim,wc);/*ffts charge matrix*/
    FieldCalc(CKern);		/* calculate pot'l from charge density*/
    FFTthis.Inverse(cxdimh,cydim,(const MyComplex* const*)wc,
		    chargex,chargey,wr);
    PutHxy(readh,h,magx,magy);	/* compute and write out x-y fields */

    (*writem)(wr,m,magx,magy,2); /* mz is equivalent to surface charge */
    FFTthis.Forward(magx,magy,(const double* const*)wr,
		    cxdimh,cydim,wc);	/*ffts charge matrix*/
    FieldCalc(CZKern);	/* calculate Hz(k) from Mz(k) */
    FFTthis.Inverse(cxdimh,cydim,(const MyComplex* const*)wc,
		    magx,magy,wr);/*inverse fft of potential*/
    (*readh)(wr,h,magx,magy,2); /* write out Hz */
}
static void DemagKern(DF_Export_m* writem,DF_Import_h* readh,
			void* m,void* h)
{   
    CheckInit(magx,magy);	/* insure the workspace has been allocated */
    (*writem)(mx,m,magx,magy,0); 
    (*writem)(my,m,magx,magy,1); 
    FFTthis.Forward(magx,magy,(const double* const*)mx,
		    c2xdimh,c2ydim,wc);
    FFTthis.Forward(magx,magy,(const double* const*)my,
		    c2xdimh,c2ydim,w2c);	
    Field2Calc();	
    FFTthis.Inverse(c2xdimh,c2ydim,(const MyComplex* const*)wc,
		    magx,magy,mx);
    FFTthis.Inverse(c2xdimh,c2ydim,(const MyComplex* const*)w2c,
		    magx,magy,my);
    (*readh)(mx,h,magx,magy,0); 
    (*readh)(my,h,magx,magy,1); 

    (*writem)(wr,m,magx,magy,2); /* mz is equivalent to surface charge */
    FFTthis.Forward(magx,magy,(const double* const*)wr,
		    cxdimh,cydim,wc);	/*ffts charge matrix*/
    FieldCalc(CZKern);	/* calculate Hz(k) from Mz(k) */
    FFTthis.Inverse(cxdimh,cydim,(const MyComplex* const*)wc,
		    magx,magy,wr);/*inverse fft of potential*/
    (*readh)(wr,h,magx,magy,2); /* write out Hz */
}
static void Demag2dSlab(DF_Export_m* writem,DF_Import_h* readh,
			void* m,void* h)

{
    CheckInit(magx,magy);	/*insure the workspace has been allocated */
    GetCharge(writem,m,magx,magy);/* load Mx, My, & compute charge density*/
    FFTthis.Forward(chargex,chargey,(const double* const*)wr,
		    cxdimh,cydim,wc);/*ffts charge matrix*/
    FieldCalc(CKern);	/* calculate pot'l from charge density*/
    FFTthis.Inverse(cxdimh,cydim,(const MyComplex* const*)wc,
		    chargex,chargey,wr);	/*inverse fft*/
    PutHxy(readh,h,magx,magy);	/* compute and write out x-y fields */
}

static void DemagPipe(DF_Export_m* writem,DF_Import_h* readh,
		      void* m,void* h)
{
    CheckInit(magx,magy);	/* insure the workspace has been allocated */
    GetCharge(writem,m,magx,magy);/* load Mx, My, & compute charge density */
    FFTthis.Forward(chargex,chargey,(const double* const*)wr,
		    cxdimh,cydim,wc);/*ffts charge matrix*/
    FieldCalc(CKern);		/* calculate pot'l from charge density*/
    FFTthis.Inverse(cxdimh,cydim,(const MyComplex* const*)wc,
		    chargex,chargey,wr);/*inverse fft*/
    PutHxy(readh,h,magx,magy);	/* compute and write out x-y fields */  
}
/*************************************************************************/
/*************************************************************************/
 
static double KernelPipe(int ioff, int joff)
{
	double sum;
	int i, j;
	double r,k;
	k=1.0/(32.0*PI);
	sum = 0.0;
	for (i = -3; i < 4; i+=2) for (j = -3; j< 4; j +=2){
		r = hypot(double(ioff)+double(i)/8.0,
			  double(joff)+double(j)/8.0);
		sum -= log(r)*k;
	}
	return sum;

}
static double KernelXY(double a,int ioff, int joff)
{
	double sum;
	int i, j;
	double r;

	sum = 0.0;
	for (i = -3; i < 4; i+=2) for (j = -3; j< 4; j +=2){
		r = hypot(double(ioff)+double(i)/8.0,
			  double(joff)+double(j)/8.0);
		sum += log( (sqrt(1+4*r*r/(a*a))+1) / r)/(32*PI);
	}
	/*printf("%i %i %f\n",ioff,joff,sum);*/
	return sum;
}   
static double KernelZ(double a, int ioff, int joff)
{
	double sum;
	int i, j;
	double r;
	sum = 0.0;
	for (i = -1; i < 2; i+=2) for (j = -1; j< 2; j +=2){
	    r = sqrt(OC_SQ(double(ioff)+double(i)/4.0)
		     + OC_SQ(double(joff)+double(j)/4.0) + a*a/4);
	    sum -= a/(r*r*r*16*PI);
	}
	return sum; 
} 
/* calculation of workspace dimensions */
static int SecondPowerTwo(int dim)
{
//returns second power of two greater than a given number
    int m;
    if(dim<1) {
      PlainError(1,"Error in SecondPowerTwo(): dimension must be greater than 1");
    }
    for(m=1;m<dim;m*=2) {}
    return 2*m;
}
static void CheckInit(int, int)
{
    if ( wr == NULL){
	/* assume the workspace has not been allocated */
      PlainError(1,"Workspace has not been allocated. \n"
		 "DemagInit routine must be called before demag"
		 "calculation.\n");
    }
}

static void DemagDestroy(void)
{
  delete[] wr[0]; delete[] wr;
  delete[] dat;
  delete[] wc[0]; delete[]  wc;
  delete[] CKern[0]; delete[] CKern;
  delete[] ba[0];delete[] ba;
  if(CZKern!=NULL) {
    delete[] CZKern[0];  delete[] CZKern;  CZKern=NULL;
  }
  if(Cdxx!=NULL) {
    delete[] Cdxx[0];  delete[] Cdxx;  Cdxx=NULL;
    delete[] Cdyy[0];  delete[] Cdyy;  Cdyy=NULL;
    delete[] Cdxy[0];  delete[] Cdxy;  Cdxy=NULL;
  }
}
