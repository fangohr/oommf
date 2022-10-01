/* FILE: maginit.cc	      -*-Mode: c++-*-
 *
 * This file contains function definitions for the mag element
 *  initialization routines.
 */

#include <cstring>
#include <cstdio>

#include "grid.h"  // Only needed for PERTURBATION_SIZE
#include "magelt.h"
#include "maginit.h"
#include "vecio.h"
#include "vf.h"

/* End includes */

// Uncomment the next block to disable random perturbations in
// the maginit routines.
/*********************************************/
#ifdef PERTURBATION_SIZE
#undef PERTURBATION_SIZE
#endif
#define PERTURBATION_SIZE 0.
/**********************************************/

static void MagPerturbAndScale(int Nx,int Ny,MagElt** m)
{ // Adjust with random perturbations
  int i,j;
  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    m[i][j].Perturb(PERTURBATION_SIZE);
    // m[i][j].Scale(1.); // NOTE: The MagElt::Perturb
    /// function itself scales to 1.0.
  }
}

// NOTE: To add new magnetization initialization functions, add the name
// to the following list, and fill in the MagInit::Ident structure below.
// Then use the existing functions as a guide to writing your own function.

MI_Function MI_InUpOut,MI_InLeftOut,MI_InLeftOutRot,MI_InWideUpOut;
MI_Function MI_InOut,MI_InOutRot,MI_InUpOutRot,MI_CRot;
MI_Function MI_In,MI_Up;
MI_Function MI_UpDownHeadToHead;
MI_Function MI_UpOutDownHeadToHead,MI_UpRightDownHeadToHead;
MI_Function MI_InOuts,MI_UpDowns;
MI_Function MI_UpDownsRot,MI_UpDownShift;
MI_Function MI_Uniform,MI_Random,MI_Bloch,MI_Neel,MI_Spiral;
MI_Function MI_4domain,MI_1domain,MI_7domain;
MI_Function MI_Vortex,MI_Exvort;
MI_Function MI_Sphere,MI_Source;
MI_Function MI_RightLeft,MI_RightUpLeft;
MI_Function MI_vioFile,MI_AvfFile;

MI_FuncInfo MagInit::Ident[]={
  { MI_Random,	    "Random",	    0}, // Default; MUST require no parameters
  { MI_InUpOut,	    "InUpOut",	    0},
  { MI_InLeftOut,   "InLeftOut",    0},
  { MI_InLeftOutRot,"InLeftOutRot", 1},
  { MI_InWideUpOut, "InWideUpOut",  0},
  { MI_InOut,	    "InOut",	    0},
  { MI_InOutRot,    "InOutRot",	    1},
  { MI_InUpOutRot,  "InUpOutRot",   1},
  { MI_CRot,        "CRot",         1},
  { MI_In,	    "In",	    0},
  { MI_Up,	    "Up",	    0},
  { MI_UpDownHeadToHead,    "UpDownHeadToHead",    0},
  { MI_UpOutDownHeadToHead, "UpOutDownHeadToHead", 1},
  { MI_UpRightDownHeadToHead, "UpRightDownHeadToHead", 1},
  { MI_InOuts,	    "InOuts",	    1},
  { MI_UpDowns,	    "UpDowns",	    1},
  { MI_UpDownsRot,  "UpDownsRot",   2},
  { MI_UpDownShift, "UpDownShift",  1},
  { MI_Bloch,	    "Bloch",	    1},
  { MI_Neel,	    "Neel",	    2},
  { MI_Spiral,	    "Spiral",	    2},
  { MI_Uniform,	    "Uniform",	    2},
  { MI_Vortex,	    "Vortex",	    0},
  { MI_Exvort,	    "Exvort",	    0},
  { MI_Sphere,	    "Sphere",	    0},
  { MI_Source,	    "Source",	    0},
  { MI_1domain,	    "1domain",	    0},
  { MI_4domain,	    "4domain",	    0},
  { MI_7domain,	    "7domain",	    0},
  { MI_RightLeft,   "RightLeft",    0},
  { MI_RightUpLeft, "RightUpLeft",  0},
  { MI_vioFile,     "vioFile",      1},
  { MI_AvfFile,     "AvfFile",      1}
};

int MagInit::IdentCount=0;
static MagInit maginit_dummy; // Dummy variable to run initializer

const char* MagInit::NameCheck(char *index)
{ // Returns pointer to routine name, or NULL if no match found
  int i;
  if(index==NULL || index[0]=='\0') return Ident[0].name;
  for(i=0;i<IdentCount;i++) {
    if(Nb_StrCaseCmp(Ident[i].name,index)==0) break;
  }
  if(i>=IdentCount) return (char*)NULL;
  return Ident[i].name;
}

MI_Function* MagInit::LookUp(char *index)
{
  int i;
  if(index==NULL || index[0]=='\0') return Ident[0].func;
  for(i=0;i<IdentCount;i++)
    if(Nb_StrCaseCmp(Ident[i].name,index)==0) break;
  if(i>=IdentCount)
    PlainError(1,"Error in MagInit::Lookup: Magnetization index string not matched");
  return Ident[i].func;
}

const char* MagInit::LookUp(MI_Function* func)
{
  int i;
  for(i=0;i<IdentCount;i++) if(Ident[i].func==func) break;
  if(i>=IdentCount) return (char*)NULL;
  return Ident[i].name;
}

int MagInit::GetParamCount(char *index)
{
  int i;
  if(index==NULL || index[0]=='\0') return Ident[0].paramcount;
  for(i=0;i<IdentCount;i++)
    if(Nb_StrCaseCmp(Ident[i].name,index)==0) break;
  if(i>=IdentCount)
    PlainError(1,"Error in MagInit::GetParamCount: "
	       "Magnetization index string ->%s<- not matched",index);
  return Ident[i].paramcount;
}

void MagInit::List(FILE* output)
{
  fprintf(output,"%d magnetization initialization routines available---\n",
	  IdentCount);
  int i;
  for(i=0;i<IdentCount;i++) {
    if(Ident[i].name[0]!='\0')
      fprintf(output,"%11s with %d arguments",
	      Ident[i].name,Ident[i].paramcount);
    if(i%3 == 2) fprintf(output,"\n"); else fprintf(output," ");
  }
  if(i%3 != 3) fprintf(output,"\n");
}


MagInit::MagInit(void)
{
  IdentCount=sizeof(Ident)/sizeof(MI_FuncInfo);
}

int MI_Random(int Nx,int Ny,MagElt** m,char** /*params*/)
{
  int i,j;
  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    m[i][j].Random(1.0);
  }
  return 0;
}

int MI_InOut(int Nx,int Ny,MagElt** m,char** /*params*/)
{
  int i,j;

  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    m[i][j][0]=m[i][j][1]=0.0;
    if(i<Nx/2) m[i][j][2]= -1.0;
    else       m[i][j][2]=  1.0;
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_InOutRot(int Nx,int Ny,MagElt** m,char** params)
{ // Same as InOut, but rotated params[0] degrees ccw in xy-plane
  const int ParamCount=1; // This routine requires 1 argument: 
  double phi;  // Parameters
  int i,j;

  // Do a parameter list length check
  if(params==NULL || params[0][0]=='\0'
     || params[0][0]=='?' || params[0][1]=='?')
    PlainError(1,"Error in MI_InOutRot: This magnetization initialization"
	       " function requires %d argument: Phi (in degrees);"
	       " None passed", ParamCount);

  // Read parameter
  phi=atof(params[0]);

  double cosphi,sinphi,offset;
  degcossin(phi,cosphi,sinphi);
  offset=double(Nx-1)*cosphi/2.+double(Ny-1)*sinphi/2.;

  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    m[i][j][0]=m[i][j][1]=0.0;
    if(double(i)*cosphi+double(j)*sinphi<offset) m[i][j][2]= -1.0;
    else                                         m[i][j][2]=  1.0;
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_InUpOutRot(int Nx,int Ny,MagElt** m,char** params)
{ // Same as InUpOut, but rotated params[0] degrees ccw in xy-plane
  const int ParamCount=1; // This routine requires 1 argument: Phi
  double phi;  // Parameters
  int i,j;

  // Do a parameter list length check
  if(params==NULL || params[0][0]=='\0'
     || params[0][0]=='?' || params[0][1]=='?')
    PlainError(1,"Error in MI_InOutRot: This magnetization initialization"
	       " function requires %d argument: Phi (in degrees);"
	       " None passed", ParamCount);

  // Read parameter
  phi=atof(params[0]);  // Angle in degrees

  double cosphi,sinphi,offset,center;
  degcossin(phi,cosphi,sinphi);
  center=double(Nx-1)*cosphi/2.+double(Ny-1)*sinphi/2.;

  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    m[i][j][0]=m[i][j][1]=0.0;
    offset=double(i)*cosphi+double(j)*sinphi;
    if(offset<center-0.5)      m[i][j][2]= -1.0;
    else if(offset>center+0.5) m[i][j][2]=  1.0;
    else { // Center region
      m[i][j][0]= -sinphi;  m[i][j][1]=cosphi;  m[i][j][2]=0.0;
    }
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_CRot(int Nx,int Ny,MagElt** m,char** params)
{ // Same as InUpOutRot, but top and bottom spins rotated to form a "C",
  const int ParamCount=1; // This routine requires 1 argument: Phi
  double phi;  // Parameters
  int i,j;

  // Do a parameter list length check
  if(params==NULL || params[0][0]=='\0'
     || params[0][0]=='?' || params[0][1]=='?')
    PlainError(1,"Error in MI_CRot: This magnetization initialization"
	       " function requires %d argument: Phi (in degrees);"
	       " None passed", ParamCount);

  if(Nx<2 || Ny<2)
    PlainError(1,"Error in MI_CRot: This magnetization initialization"
	       " function requires both Nx (=%d) and Ny (=%d) >1.",Nx,Ny);

  // Read parameter
  phi=atof(params[0]);  // Angle in degrees

  double cosphi,sinphi,offset,center,height,centerheight,maxheight,voff;
  degcossin(phi,cosphi,sinphi);
  center=double(Nx-1)*cosphi/2.+double(Ny-1)*sinphi/2.;
  centerheight=double(Nx-1)*sinphi/2.-double(Ny-1)*cosphi/2.;
  if(fabs((Ny-1)*sinphi)<fabs((Nx-1)*cosphi))
    maxheight=fabs(double(Ny-1)/(2*cosphi));
  else
    maxheight=fabs(double(Nx-1)/(2*sinphi));

  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    m[i][j][0]=m[i][j][1]=0.0;
    offset=double(i)*cosphi+double(j)*sinphi;
    height=double(i)*sinphi-double(j)*cosphi-centerheight;
    voff=maxheight-fabs(height);
    if(offset<center-0.5) m[i][j][2]= -1.0;
    else if((offset>center+1.5) ||
	    (offset>center+0.5 && voff>1)) m[i][j][2]= 1.0;
    else { // Center region
      m[i][j][2]=0.0;
      if(voff>1) { // middle
	m[i][j][0]= -sinphi;  m[i][j][1]=  cosphi;
      }
      else if(height>0) { // top
	m[i][j][0]= -cosphi;  m[i][j][1]= -sinphi;
      }
      else { // bottom
	m[i][j][0]=  cosphi;  m[i][j][1]=  sinphi;
      }
    }
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_InUpOut(int Nx,int Ny,MagElt** m,char** /*params*/)
{
  int i,j;

  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    m[i][j][0]=m[i][j][1]=0.0;
    if(i<Nx/2)	           m[i][j][2]= -1.0;
    else if(i>Nx/2)        m[i][j][2]=  1.0;
    else { m[i][j][2]=0.0; m[i][j][1]=  1.0; }
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_InLeftOut(int Nx,int Ny,MagElt** m,char** /*params*/)
{
  int i,j;

  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    m[i][j][0]=m[i][j][1]=0.0;
    if(i<Nx/2)	           m[i][j][2]= -1.0;
    else if(i>Nx/2)        m[i][j][2]=  1.0;
    else { m[i][j][2]=0.0; m[i][j][0]= -1.0; }
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_InLeftOutRot(int Nx,int Ny,MagElt** m,char** params)
{ // Same as InLeftOut, but rotated params[0] degrees ccw in xz-plane
  const int ParamCount=1; // This routine requires 1 argument: Phi
  double phi;  // Parameters
  int i,j;

  // Do a parameter list length check
  if(params==NULL || params[0][0]=='\0'
     || params[0][0]=='?' || params[0][1]=='?')
    PlainError(1,"Error in MI_InLeftOutRot: This magnetization"
	       " initialization function requires %d argument:"
	       " Phi (in degrees); None passed",ParamCount);

  // Read parameter
  phi=atof(params[0]);  // Angle in degrees

  double cosphi,sinphi,offset,center;
  degcossin(phi,cosphi,sinphi);
  center=double(Nx-1)*cosphi/2.+double(Ny-1)*sinphi/2.;

  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    m[i][j][0]=m[i][j][1]=0.0;
    offset=double(i)*cosphi+double(j)*sinphi;
    if(offset<center-0.5)      m[i][j][2]= -1.0;
    else if(offset>center+0.5) m[i][j][2]=  1.0;
    else { // Center region
      m[i][j][0]= -cosphi;  m[i][j][1]= -sinphi; m[i][j][2]=0.0;
    }
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_InWideUpOut(int Nx,int Ny,MagElt** m,char** /*params*/)
{
  int i,j;

  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    m[i][j][0]=m[i][j][1]=0.0;
    if(i<(2*Nx)/5)	   m[i][j][2]= -1.0;
    else if(i>Nx-(2*Nx)/5) m[i][j][2]=  1.0;
    else {
      double theta;
      theta= PI/2-PI*double(i-(2*Nx)/5)/double(Nx-2*((2*Nx)/5));
      m[i][j][1]=cos(theta); m[i][j][2]=-sin(theta);
    }
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_In(int Nx,int Ny,MagElt** m,char** /*params*/)
{
  int i,j;

  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    m[i][j][0]=m[i][j][1]=0.0;
    m[i][j][2]= -1.0;
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_Up(int Nx,int Ny,MagElt** m,char** /*params*/)
{
  int i,j;

  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    m[i][j][0]=m[i][j][2]=0.0;
    m[i][j][1]=1.0;
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_UpDownHeadToHead(int Nx,int Ny,MagElt** m,char** /*params*/)
{
  int i,j;

  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    m[i][j][0]=m[i][j][2]=0.0;
    if(j<Ny/2) m[i][j][1]=  1.0;
    else       m[i][j][1]= -1.0;
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_UpOutDownHeadToHead(int Nx,int Ny,MagElt** m,char** params)
{
  const int ParamCount=1; // This routine requires 1 argument: Out proportion
  double out_proportion;
  // Do a parameter list length check
  if(params==NULL || params[0][0]=='\0'
     || params[0][0]=='?' || params[0][1]=='?')
    PlainError(1,"Error in MI_UpOutDownHeadToHead: "
	       "This magnetization initialization function"
	       " requires %d argument: Out proportion; None passed",
	       ParamCount);
  // Read parameter
  out_proportion=atof(params[0]);
  if(out_proportion<0 || out_proportion>1)
    PlainError(1,"Error in MI_UpOutDownHeadToHead: "
	 "This magnetization initialization function requires %d argument:"
	 " Out proportion; Illegal value (%d) passed (should be in [0,1])",
	       ParamCount,out_proportion);
  int outstart,outstop;
  outstart=(int)OC_ROUND(Ny*(1.-out_proportion)/2.);
  outstop=Ny-outstart;

  int i,j;
  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    m[i][j][0]=m[i][j][1]=m[i][j][2]=0.0;
    if(j<outstart)      m[i][j][1]=  1.0;
    else if(j<outstop)  m[i][j][2]=  1.0;
    else                m[i][j][1]= -1.0;
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_UpRightDownHeadToHead(int Nx,int Ny,MagElt** m,char** params)
{
  const int ParamCount=1; // This routine requires 1 argument: Right proportion
  double right_proportion;
  // Do a parameter list length check
  if(params==NULL || params[0][0]=='\0'
     || params[0][0]=='?' || params[0][1]=='?')
    PlainError(1,"Error in MI_UpRightDownHeadToHead: "
	       "This magnetization initialization function"
	       " requires %d argument: Right proportion; None passed",
	       ParamCount);
  // Read parameter
  right_proportion=atof(params[0]);
  if(right_proportion<0 || right_proportion>1)
    PlainError(1,"Error in MI_UpRightDownHeadToHead: "
	 "This magnetization initialization function requires %d argument:"
	 " Right proportion; Illegal value (%d) passed"
	 " (should be in [0,1])",ParamCount,right_proportion);
  int rightstart,rightstop;
  rightstart=(int)OC_ROUND(Ny*(1.-right_proportion)/2.);
  rightstop=Ny-rightstart;

  int i,j;
  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    m[i][j][0]=m[i][j][1]=m[i][j][2]=0.0;
    if(j<rightstart)      m[i][j][1]=  1.0;
    else if(j<rightstop)  m[i][j][0]=  1.0;
    else                  m[i][j][1]= -1.0;
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_UpDowns(int Nx,int Ny,MagElt** m,char** params)
{
  const int ParamCount=1; // This routine requires 1 argument: Domain width
  int domain_width;	  // Parameters
  // Do a parameter list length check
  if(params==NULL || params[0][0]=='\0'
     || params[0][0]=='?' || params[0][1]=='?')
    PlainError(1,"Error in MI_UpDowns: This magnetization initialization"
	       " function requires %d argument:"
	       " Domain width (in cells); None passed",
	       ParamCount);

  // Read parameter
  domain_width=atoi(params[0]);
  if(domain_width<1)
    PlainError(1,"Error in MI_UpDowns: This magnetization initialization"
	       " function requires %d argument:"
	       " Domain width (in cells); Illegal value (%d) passed",
	       ParamCount,domain_width);
  int i,j;
  double mag;

  mag=-1.0;
  for(i=0;i<Nx;i++) {
    if(i%domain_width==0) mag*=-1;
    for(j=0;j<Ny;j++) {
      m[i][j][0]=m[i][j][2]=0.0;
      m[i][j][1]=mag;
    }
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_UpDownsRot(int Nx,int Ny,MagElt** m,char** params)
{ // Similar to MI_UpDowns, but rotated.
  const int ParamCount=2; // This routine requires 2 arguments:
  int domain_width;	  //   domain width and orientation angle
  double orient;
  // Do a parameter list length check
  if(params==NULL || params[0][0]=='\0'
     || params[0][0]=='?' || params[0][1]=='?')
    PlainError(1,"Error in MI_UpDowns: This magnetization initialization"
	       " function requires %d arguments:\n"
	       " Domain width (in cells) and orientation angle (in degrees);"
	       " None passed", ParamCount);

  // Read parameters
  domain_width=atoi(params[0]);
  if(domain_width<1)
    PlainError(1,"Error in MI_UpDownsRot: "
	       "Illegal domain width value (%d cells) requested.\n",
	       domain_width);
  orient=atof(params[1]);  // Angle in degrees

  int i,j,ioffset;
  double xc,yc,vx,vy,magx,magy,offx,offset,wall;

  xc=(Nx-1)/2.;  yc=(Ny-1)/2.;
  degcossin(orient,vx,vy);
  magx= -vy; magy=vx;

  for(i=0;i<Nx;i++) {
    offx=(double(i)-xc)*vx;
    for(j=0;j<Ny;j++) {
      m[i][j][2]=0.0; // All moments in x-y plane
      offset=(offx+(double(j)-yc)*vy)/domain_width+0.5;
      ioffset=int(floor(offset));
      if(ioffset%2==0) { m[i][j][0]=  magx; m[i][j][1]=  magy; }
      else             { m[i][j][0]= -magx; m[i][j][1]= -magy; }
      wall=(offset-ioffset); if (wall>0.5) wall--;
      wall*=domain_width;
      if(-0.9<wall && wall<=0.35) { // At wall center;
	m[i][j][0]=magy; m[i][j][1]=-magx;      // Rotate horizontally
      }
    }
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_UpDownShift(int Nx,int Ny,MagElt** m,char** params)
{
  const int ParamCount=1; // This routine requires 1 argument: Wall shift,
  double wall_shift;	  // in [-.5,.5], with 0 => wall center at Nx/2.
  // Do a parameter list length check
  if(params==NULL || params[0][0]=='\0'
     || params[0][0]=='?' || params[0][1]=='?')
    PlainError(1,"Error in MI_UpDownShift: "
	       "This magnetization initialization function"
	       " requires %d argument: Wall shift (proportion;"
	       " 0==centered); None passed",ParamCount);

  // Read parameter
  wall_shift=atof(params[0]);
  if(wall_shift<-0.5 || 0.5<wall_shift)
    PlainError(1,"Error in MI_UpDownShift: "
	       "This magnetization initialization function"
	       " requires %d argument: Wall shift ([-.5,.5], "
	       " 0==centered); Passed value: %f",ParamCount,wall_shift);

  int i,j;
  double mag,mx,my,mz;
  int midbreak=int(OC_ROUND(double(Nx)*(0.5+wall_shift)));
  if(midbreak<0) midbreak=0;
  else if(midbreak>Nx) midbreak=Nx;

  mag=1.0;
  mx=0.0; my=mag; mz=0.0;
  for(i=0;i<Nx;i++) {
    if(i==midbreak) {
      mx=0.0; my=0.0; mz=mag;
    }
    else if(i==midbreak+1) {
      mag*=-1;
      mx=0.0; my=mag; mz=0.0;
    }
    for(j=0;j<Ny;j++) {
      m[i][j][0]=mx;
      m[i][j][1]=my;
      m[i][j][2]=mz;
    }
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_InOuts(int Nx,int Ny,MagElt** m,char** params)
{
  const int ParamCount=1; // This routine requires 1 argument: Domain width
  int domain_width;	  // Parameters
  // Do a parameter list length check
  if(params==NULL || params[0][0]=='\0'
     || params[0][0]=='?' || params[0][1]=='?')
    PlainError(1,"Error in MI_InOuts: This magnetization initialization"
	       " function requires %d argument: Domain width (in cells);"
	       " None passed", ParamCount);
  // Read parameter
  domain_width=atoi(params[0]);
  if(domain_width==0)
    PlainError(1,"Error in MI_InOuts: "
	       "This magnetization initialization function requires"
	       " %d argument: Domain width (in cells); Illegal value"
	       " (%d) passed",ParamCount,domain_width);

  int i,j;
  double mag;

  mag=1.0;
  if(domain_width>0) { // Iterate domains from left to right
    for(i=0;i<Nx;i++) {
      if(i%domain_width==0) mag*=-1;
      for(j=0;j<Ny;j++) {
	m[i][j][0]=m[i][j][1]=0.0;
	m[i][j][2]=mag;
	// Adjust with random perturbations
	m[i][j].Perturb(PERTURBATION_SIZE);
	m[i][j].Scale(1.);
      }
    }
  }
  else { // Iterate domains from top to bottom
    domain_width*=-1;
    for(j=0;j<Ny;j++) {
      if(j%domain_width==0) mag*=-1;
      for(i=0;i<Nx;i++) {
	m[i][j][0]=m[i][j][1]=0.0;
	m[i][j][2]=mag;
      }
    }
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_Bloch(int Nx,int Ny,MagElt** m,char** params)
{ // InUpOut rotated about the x axis
  const int ParamCount=1; // This routine requires 1 argument: Phi
  double theta;  // Parameters
  int i,j;

  // Do a parameter list length check
  if(params==NULL || params[0][0]=='\0'
     || params[0][0]=='?' || params[0][1]=='?')
    PlainError(1,"Error in MI_Bloch: "
	       "This magnetization initialization function requires"
	       " %d argument: Phi (in degrees); None passed", ParamCount);

  // Read parameter
  theta=atof(params[0]); // Angle in degrees

  double x,y,z;
  x=   0.0;
  degcossin(theta,y,z);
  y*=-1; z*=-1;

  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    m[i][j][0]=x;
    if(i<Nx/2)	     { m[i][j][1]=  y;  m[i][j][2]= z; }
    else if(i==Nx/2) { m[i][j][1]= -z;  m[i][j][2]= y; }
    else	     { m[i][j][1]= -y;  m[i][j][2]=-z; }
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_Neel(int Nx,int Ny,MagElt** m,char** params)
{ // DownRightUp rotated about the z axis, with an atan-based
  // transition region with width
  //        (cross-particle dimension)*(width proportion)
  // Here "cross-particle dimension" is based on the width across
  // the chosen direction of an ellipse inscribed in Nx x Ny.
  const int ParamCount=2;
  /// This routine requires 2 arguments: Theta and wall width proportion
  double theta,width_proportion;  // Parameters

  // Do a parameter list length check
  if(params==NULL || params[0][0]=='\0'
     || params[0][0]=='?' || params[0][1]=='?')
    PlainError(1,"Error in MI_Neel: "
	       "This magnetization initialization function requires"
	       " %d argument: Theta (in degrees), width (proportion);"
	       " None passed", ParamCount);

  // Read parameters
  theta=atof(params[0]);  // In degrees
  width_proportion=atof(params[1]);

  double vx,vy,dotref;
  degcossin(theta,vx,vy);
  dotref=(Nx/2.)*vx+(Ny/2.)*vy;

  double part_width,wall_width,scale; // Working variables
  if(Nx<1 || Ny<1) return 1; // Safety
  double tempx=vx/double(Nx),tempy=vy/double(Ny);
  part_width=1./sqrt(tempx*tempx+tempy*tempy);
  wall_width=(part_width*width_proportion);
  if(wall_width==0.) wall_width=OC_REAL8_EPSILON; // Safety
  scale=PI/wall_width;

  int i,j; double offset,xproj,yproj;
  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    offset=scale*(i*vx+j*vy-dotref);
    xproj=1./sqrt(1+offset*offset);
    yproj=offset*xproj;
    m[i][j][0]=vx*xproj-vy*yproj;
    m[i][j][1]=vy*xproj+vx*yproj;
    m[i][j][2]=0.0;
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_Spiral(int Nx,int Ny,MagElt** m,char** params)
{ // Linear rotation in plane rotated about the z axis with period
  //        (cross-particle dimension)*(period proportion)
  // Here "cross-particle dimension" is based on the width across
  // the chosen direction of an ellipse inscribed in Nx x Ny.
  const int ParamCount=2;
  /// This routine requires 2 arguments: Theta and period width proportion
  double theta,period_proportion;  // Parameters

  // Do a parameter list length check
  if(params==NULL || params[0][0]=='\0'
     || params[0][0]=='?' || params[0][1]=='?')
    PlainError(1,"Error in MI_Spiral: "
	       "This magnetization initialization function requires"
	       " %d argument: Theta (in degrees), period (proportion);"
	       " None passed", ParamCount);

  // Read parameters
  theta=atof(params[0]);  // In degrees
  period_proportion=atof(params[1]);

  double vx,vy,dotref;
  degcossin(theta,vx,vy);
  dotref=(Nx/2.)*vx+(Ny/2.)*vy;

  double part_width,period,scale; // Working variables
  if(Nx<1 || Ny<1) return 1; // Safety
  double tempx=vx/double(Nx),tempy=vy/double(Ny);
  part_width=1./sqrt(tempx*tempx+tempy*tempy);
  period=(part_width*period_proportion);
  if(period==0.) period=OC_REAL8_EPSILON; // Safety
  scale=2.*PI/period;

  int i,j; double offset,xproj,yproj;
  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    offset=scale*(i*vx+j*vy-dotref);
    xproj=cos(offset);
    yproj=sin(offset);
    m[i][j][0]=vx*xproj-vy*yproj;
    m[i][j][1]=vy*xproj+vx*yproj;
    m[i][j][2]=0.0;
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_Uniform(int Nx,int Ny,MagElt** m,char** params)
{
  const int ParamCount=2; // This routine requires 2 arguments: Theta and Phi
  double theta,phi;  // Parameters
  int i,j;

  // Do a parameter list length check
  if(params==NULL || params[0][0]=='\0'
     || params[0][0]=='?' || params[0][1]=='?')
    PlainError(1,"Error in MI_Uniform: "
	       "This magnetization initialization function requires %d"
	       " arguments: Theta and Phi (in degrees); None passed",
	       ParamCount);
  for(i=0;i<ParamCount;i++) if(params[i][0]=='\0')
    PlainError(1,"Error in MI_Uniform: "
	       "This magnetization initialization function requires %d"
	       " arguments: Phi and Theta (in degrees); Only %d passed",
	       ParamCount,i);

  // Read parameters
  theta=atof(params[0]); phi=atof(params[1]);
  double x,y,z,u;
  degcossin(phi,x,y);
  degcossin(theta,z,u); // z=cos(theta);
  x*=u;                 // x=cos(phi)*sin(theta);
  y*=u;                 // y=sin(phi)*sin(theta);

  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    m[i][j][0]=x; m[i][j][1]=y; m[i][j][2]=z;
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_1domain(int Nx,int Ny,MagElt** m,char** /*params*/)
{
  int i,j;

  if ( Ny > Nx ) {
    for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
      m[i][j][0]=m[i][j][2]=0.0; m[i][j][1]=1.0;
      if( j<Nx-i || j>Ny-i ) { // Cant edges
	m[i][j][0] = 0.707; m[i][j][1] = 0.707;
      }
      // Adjust with random perturbations
      m[i][j].Perturb(PERTURBATION_SIZE);
      m[i][j].Scale(1);
    }
  } else {
    for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
      m[i][j][0]=1.0; m[i][j][1]=m[i][j][2]=0.0;
      if( i<Ny-j || i>Nx-j ) {  // Cant edges
	m[i][j][0] = 0.707; m[i][j][1] = 0.707;
      }
    }
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_4domain(int Nx,int Ny,MagElt** m,char** /*params*/)
{
  int i,j;

  if ( Ny > Nx ) {
    for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
      m[i][j][0]=m[i][j][2]=  0.0;
      if(i<Nx/2) m[i][j][1]= -1.0;
      else	 m[i][j][1]=  1.0;
      if( j<i && j<Nx-1-i )
	{ m[i][j][0] =  1.0; m[i][j][1] =  0.0;}
      if( j > Ny-1-i && j > Ny-Nx+i)
	{ m[i][j][0] = -1.0; m[i][j][1] =  0.0;}
      if( j == i && i < Nx/2 )
	{ m[i][j][0] =  0.7; m[i][j][1] = -0.7;}
      if( j == Nx-1-i && i >= Nx/2 )
	{ m[i][j][0] =  0.7; m[i][j][1] =  0.7;}
      if( j == Ny-1-i && i < Nx/2 )
	{ m[i][j][0] = -0.7; m[i][j][1] = -0.7;}
      if( j == Ny-Nx+i && i >= Nx/2 )
	{ m[i][j][0] = -0.7; m[i][j][1] =  0.7;}
    }
  }
  else {
    for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
      m[i][j][1]=m[i][j][2]=  0.0;
      if(j<Ny/2) m[i][j][0]=  1.0;
      else	 m[i][j][0]= -1.0;
      if( i<j && i<Ny-1-j )
	{ m[i][j][1] = -1.0; m[i][j][0] =  0.0;}
      if( i > Nx-1-j && i > Nx-Ny+j)
	{ m[i][j][1] =  1.0; m[i][j][0] =  0.0;}
      if( i == j && j < Ny/2 )
	{ m[i][j][1] = -0.7; m[i][j][0] =  0.7;}
      if( i == Ny-1-j && j >= Ny/2 )
	{ m[i][j][1] = -0.7; m[i][j][0] = -0.7;}
      if( i == Nx-1-j && j < Ny/2 )
	{ m[i][j][1] =  0.7; m[i][j][0] =  0.7;}
      if( i == Nx-Ny+j && j >= Ny/2 )
	{ m[i][j][1] =  0.7; m[i][j][0] = -0.7;}
    }
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_7domain(int Nx,int Ny,MagElt** m,char** /*params*/)
{
  int i,j;
  int test1,test2,test3,test4;
  if (Ny > Nx) {
    double slope=double(Ny-1)/double(2*(Nx-1));
    double half=double(Ny-1)/2.;
    double adj=0;
    if(Nx%2 == 0) adj=0.5;
    for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
      m[i][j][2]=0.0;
      test1=test2=test3=test4= 0;
      if(j<half-i*slope-adj)  test1 = 1;
      if(j<i*slope-adj)       test2 = 1;
      if(j>half+i*slope+adj)  test3 = 1;
      if(j>Ny-1-i*slope+adj)  test4 = 1;
      if((test1 && test2) || (test3 && test4)) {
        m[i][j][0] = -1.0; m[i][j][1]=  0.0;
      } else if(test1 || test4) {
        m[i][j][0] =  0.0; m[i][j][1]=  1.0;
      } else if(test2 || test3) {
        m[i][j][0] =  0.0; m[i][j][1]= -1.0;
      } else {
        m[i][j][0] =  1.0; m[i][j][1]=  0.0;
      }
    }
  } else {
    double slope=double(Nx-1)/double(2*(Ny-1));
    double half=double(Nx-1)/2.;
    double adj=0;
    if(Ny%2 == 0) adj=0.5;
    for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
      m[i][j][2]=0.0;
      test1=test2=test3=test4= 0;
      if(i<half-j*slope-adj)  test1 = 1;
      if(i<j*slope-adj)       test2 = 1;
      if(i>half+j*slope+adj)  test3 = 1;
      if(i>Nx-1-j*slope+adj)  test4 = 1;
      if((test1 && test2) || (test3 && test4)) {
        m[i][j][0] =  0.0; m[i][j][1]= -1.0;
      } else if(test1 || test4) {
        m[i][j][0] =  1.0; m[i][j][1]=  0.0;
      } else if(test2 || test3) {
        m[i][j][0] = -1.0; m[i][j][1]=  0.0;
      } else {
        m[i][j][0] =  0.0; m[i][j][1]=  1.0;
      }
    }
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_Vortex(int Nx,int Ny,MagElt** m,char** /*params*/)
{
  if(Nx<1 || Ny<1 || m==(MagElt**)NULL)
    PlainError(1,"Error in MI_Vortex: %s",ErrBadParam);
  int i,j;
  double x,y;
  double midx, midy;

  midx = (Nx-1)/2.0; midy = (Ny-1)/2.0;

  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    if ( midx != (double) i || midy != (double) j ) {
      x = i-midx; y = j-midy;
      m[i][j][0] = -y/hypot(x,y);
      m[i][j][1] =  x/hypot(x,y);
      m[i][j][2] =  0.0;
    }
    else
      { m[i][j][0]=m[i][j][1]=0.0; m[i][j][2] = 1.0;}
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_Exvort(int Nx,int Ny,MagElt** m,char** /*params*/)
{
  int i,j;
  double x,y;
  double midx, midy;

  midx = (Nx-1)/2.0; midy = (Ny-1)/2.0;

  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    if ( midx != (double) i || midy != (double) j ) {
      x = i-midx; y = j-midy;
      m[i][j][0] =  y/hypot(x,y);
      m[i][j][1] =  x/hypot(x,y);
      m[i][j][2] =  0.0;
    }
    else
      { m[i][j][0]=m[i][j][1]=0.0; m[i][j][2] = 1.0;}
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_Sphere(int Nx,int Ny,MagElt** m,char** /*params*/)
{
  if(Nx<1 || Ny<1 || m==(MagElt**)NULL)
    PlainError(1,"Error in MI_Sphere: %s",ErrBadParam);
  int i,j;
  double x,y,z,proj_len;
  double midx, midy, radius, radius_sq;

  midx = (Nx-1)/2.0; midy = (Ny-1)/2.0;
  radius = OC_MAX(OC_MIN(midx,midy),1.0);
  radius_sq=OC_SQ(radius);

  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    x = i-midx; y = j-midy;
    proj_len=hypot(x,y);
    if(proj_len >= radius) { // Outside sphere
      m[i][j][0] =  x/proj_len;
      m[i][j][1] =  y/proj_len;
      m[i][j][2] =  0.0;
    }
    else {
      z=sqrt(radius_sq-OC_SQ(proj_len));
      m[i][j][0] =  x/radius;
      m[i][j][1] =  y/radius;
      m[i][j][2] =  z/radius;
    }
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_Source(int Nx,int Ny,MagElt** m,char** /*params*/)
{
  if(Nx<1 || Ny<1 || m==(MagElt**)NULL)
    PlainError(1,"Error in MI_Source: %s",ErrBadParam);
  int i,j;
  double x,y;
  double midx, midy;

  midx = (Nx-1)/2.0; midy = (Ny-1)/2.0;

  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    if ( midx != (double) i || midy != (double) j ) {
      x = i-midx; y = j-midy;
      m[i][j][0] =  x/hypot(x,y);
      m[i][j][1] =  y/hypot(x,y);
      m[i][j][2] =  0.0;
    }
    else
      { m[i][j][0]=m[i][j][1]=0.0; m[i][j][2] = 1.0;}
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_RightLeft(int Nx,int Ny,MagElt** m,char** /*params*/)
{
  int i,j;

  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    m[i][j][1]=m[i][j][2]= 0.0;
    if(i<Nx/2) m[i][j][0]= 1.0;
    else       m[i][j][0]=-1.0;
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_RightUpLeft(int Nx,int Ny,MagElt** m,char** /*params*/)
{
  int i,j;
  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    m[i][j][1]=m[i][j][2]=0.0;
    if(i<(Nx/2-j))      m[i][j][0]=  1.0;
    else if(i>(Nx/2-j)) m[i][j][0]= -1.0;
    else {
      m[i][j][0]=  0.0;
      if (j>Ny/2) m[i][j][1]=  -1.0;  // ?????
      else        m[i][j][1]=   1.0;
    }
  }
  MagPerturbAndScale(Nx,Ny,m);
  return 0;
}

int MI_vioFile(int Nx,int Ny,MagElt** m,char** params)
{ // Initializes magnetization from *.vio file.  If dimensions of
  // file are not the same as the grid (i.e., Nx & Ny), then stretch
  // and/or shrink the input file and interpolate using 0th order fit.

  const int ParamCount=1; // This routine requires 1 argument: filename
  // Do a parameter list length check
  if(params==NULL || params[0][0]=='\0'
     || params[0][0]=='?' || params[0][1]=='?')
    PlainError(1,"Error in MI_vioFile: "
	       "This magnetization initialization function requires %d"
	       " argument: filename.vio; None passed", ParamCount);

  int xfiledim,yfiledim,zfiledim,vd;
  FileVector fv;
  // Read file header and get sizing information.  Note coords change, from
  // vio file's xz+y to OOMMF's xy+z.
  if(fv.ReadHeader(params[0],xfiledim,zfiledim,yfiledim,vd)!=0 || zfiledim!=1)
    PlainError(1,"Error in MI_vioFile: "
	       "Bad or nonexistent *.vio input file: \"%s\".",params[0]);

  // Check that spin dimensions agree with those from input file
  if(vd!=VECDIM)
    PlainError(1,"Error in MI_vioFile: "
	       "Magnetization dimension (%d) incompatible with that of"
	       " input file %s (%d)",VECDIM,params[0],vd);

  // See if grid dimensions agree with those from input file
  double xoff,xscale,yoff,yscale; // Linear parameters to convert from
                                  // mesh coordinates to file coordinates.
  if(xfiledim!=Nx || yfiledim!=Ny) {
    PlainWarning("Error in MI_vioFile: \n"
		 "  Grid dimensions (%dx%d) are different with those"
		 " from input file %s (%dx%d)\n  Will stretch and/or"
		 " shrink input as necessary.  Results may be"
		 " sub-optimal!\n",Nx,Ny,params[0],xfiledim,yfiledim);
    xoff=double(xfiledim-Nx)/double(2*Nx);
    xscale=double(xfiledim)/double(Nx);
    yoff=double(yfiledim-Ny)/double(2*Ny);
    yscale=double(yfiledim)/double(Ny);
  }
  else {
    xoff=yoff=0.0;
    xscale=yscale=1.0;
  }

  // Allocate workspace memory
  double *arr;
  if((arr=new double[yfiledim*vd])==0)
    PlainError(1,"Error in MI_vioFile: %s",ErrNoMem);

  // Fill array
  int itemp,imesh,jmesh,ifile,jfile;
  for(ifile=imesh=0;ifile<xfiledim && imesh<Nx;ifile++) {
    fv.ReadVecs(yfiledim,arr); // Read in 1 row from file
    while(imesh<Nx) { // Fill all appropriate mesh rows from this file row
      itemp=(int)OC_ROUND(xoff+xscale*imesh);
      if(itemp<0)         itemp=0;          // Safety
      if(itemp>=xfiledim) itemp=xfiledim-1; // Safety
      if(itemp!=ifile) break;
      for(jmesh=0;jmesh<Ny;jmesh++) {
	jfile=(int)OC_ROUND(yoff+yscale*jmesh);
	if(jfile<0)         jfile=0;          // Safety
	if(jfile>=yfiledim) jfile=yfiledim-1; // Safety
	// Copy vector.  Note coords change, from vio file's xz+y
	// to OOMMF's xy+z.
	m[imesh][jmesh][0]=  arr[jfile*VECDIM+0];
	m[imesh][jmesh][1]=  arr[jfile*VECDIM+2];
	m[imesh][jmesh][2]= -arr[jfile*VECDIM+1];
	m[imesh][jmesh].Scale(1.0);  // Make sure this is a unit vector!
      }
      imesh++;
    }
  }

  // Clean up and exit
  delete arr;
  fv.Close();
  return 0;
}

int MI_AvfFile(int Nx,int Ny,MagElt** m,char** params)
{ // Initializes magnetization from vector field files.  This routine
  // uses the Vf_Mesh display list calls to fit the input data to
  // the grid, which is likely to involve a 0th order fit.
  int Nz=1;  // Current only supports 2D grids.

  const int ParamCount=1; // This routine requires 1 argument: filename
  // Do a parameter list length check
  if(params==NULL || params[0][0]=='\0'
     || params[0][0]=='?' || params[0][1]=='?') {
    PlainWarning("Error in MI_vioFile: "
	       "This magnetization initialization function requires %d"
	       " argument: filename.vio; None passed", ParamCount);
    return 1;
  }

  // Open file and load mesh.
  Vf_FileInput* vffi=Vf_FileInput::NewReader(params[0]);
  if(vffi==NULL) return 2;
  Vf_Mesh* mesh=vffi->NewMesh();
  if(mesh==NULL) return 3;

  // Get mesh extents
  Nb_BoundingBox<OC_REAL8> range;  mesh->GetPreciseDataRange(range);
  Nb_Vec3<OC_REAL8> base;          range.GetMinPt(base);
  OC_REAL8 xstep=range.GetWidth()/OC_REAL8(Nx);
  OC_REAL8 ystep=range.GetHeight()/OC_REAL8(Ny);
  OC_REAL8 zstep=range.GetDepth()/OC_REAL8(Nz);
  // Adjust base to half-cell position
  base.x+=xstep/2.;
  base.y+=ystep/2.;
  base.z+=zstep/2.;

  // Step through range and sample mesh, fitting grid
  // with zeroth order fit.
  Nb_Vec3<OC_REAL8> pos;  Nb_LocatedVector<OC_REAL8> lv;
  lv.value.Set(1.,0.,0.); // Safety
  for(int k=0;k<Nz;k++) {  // NOTE: Code below assumes Nz=1.
    pos.z=base.z+k*zstep;
    for(int j=0;j<Ny;j++) {
      pos.y=base.y+j*ystep;
      for(int i=0;i<Nx;i++) {
	pos.x=base.x+i*xstep;
	mesh->FindPreciseClosest(pos,lv);
	m[i][j][0]=lv.value.x;
	m[i][j][1]=lv.value.y;
	m[i][j][2]=lv.value.z;
	m[i][j].Scale(1.0);  // Make sure this is a unit vector!
      }
    }
  }

  // Clean up
  delete mesh;
  delete vffi;
  return 0;
}
