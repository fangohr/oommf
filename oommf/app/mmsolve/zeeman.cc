/* FILE: zeeman.cc           -*-Mode: c++-*-
 *
 * This file contains function definitions for micromagnetic zeeman fields
 *
 * NOTE: The Zeeman classes work in xyz (as opposed to xzy) coordinates.
 *   The Grid class itself calls ConvertXyzToXzy() to convert applied
 *   fields from user (xyz) to grid (xzy) coordinates.
 */

#include <cstring>

#include "constants.h"

#include "oc.h"
#include "nb.h"
#include "vf.h"

#include "grid.h"
#include "zeeman.h"

/* End includes */

#define CODE_CHECKS

#ifdef __GNUC__
// Older versions of gcc (e.g., 2.7.0) want explicit instantiation.
template class Nb_Vec3< OC_REAL8 >;
template class Nb_Array2D< Nb_Vec3<OC_REAL8> >;
#endif // __GNUC__

// Zeeman field type selection routine
Zeeman* Zeeman::MakeZeeman(int argc,const char* argp[])
{
  if(argc<1)
    PlainError(1,"Error in Zeeman::MakeZeeman: No field type specified");
  Nb_DString temp(argp[0]);
  temp.Trim(); temp.CollapseStr(); temp.ToLower();
  if(strcmp(temp,"uniform")==0)
    return new UniformZeeman(argc,argp);
  else if(strcmp(temp,"ribbon")==0)
    return new RibbonZeeman(argc,argp);
  else if(strcmp(temp,"tie")==0)
    return new TieZeeman(argc,argp);
  else if(strcmp(temp,"onefile")==0)
    return new OneFileZeeman(argc,argp);
  else if(strcmp(temp,"fileseq")==0)
    return new FileSeqZeeman(argc,argp);
  else if(strcmp(temp,"multi")==0)
    return new MultiZeeman(argc,argp);
  else
    PlainError(1,"Error in Zeeman::MakeZeeman:"
	       " Unknown external field type: %s",temp.GetStr());
  return (Zeeman *)NULL;
}


Zeeman::Zeeman(int, const char**)
{
  nomhfield.Init(nomhfieldarr);
  nomhfieldmag=0.;
  nomhfieldstep=0;
  Nx=Ny=0;
  Ms=1.0;
  return;
}

Zeeman::~Zeeman(void) 
{
 return;
}

int Zeeman::SetCoords(Grid2D*,int imax,int jmax)
{
  if((Nx=imax)<0)
    PlainError(1,"Error in Zeeman::SetCoords:"
	       " Illegal mesh width: %d cells",Nx);
  if((Ny=jmax)<0)
    PlainError(1,"Error in Zeeman::SetCoords:"
	       " Illegal mesh height: %d cells",Ny);
  return 0;
}

int Zeeman::SetNomField(double newMs,const ThreeVector& newBfield,
			int fieldstep)
{
  if(newMs<=0.) Ms=1.0; // Safety
  else          Ms=newMs;
  nomhfieldstep=fieldstep;
  nomhfield=newBfield; nomhfield*=1./(mu0*Ms);
  if((nomhfieldmag=nomhfield.MagSq())>0) nomhfieldmag=sqrt(nomhfieldmag);
  else                                   nomhfieldmag=0.0;
  return 0;
}

double Zeeman::GetNomBFieldMag()
{
  return nomhfieldmag*mu0*Ms;
}

Vec3D Zeeman::GetNomBField()
{
  double mult=mu0*Ms;
  return Vec3D(nomhfieldarr[0]*mult,
	       nomhfieldarr[1]*mult,
	       nomhfieldarr[2]*mult);
}


double Zeeman::GetLocalhFieldMag(int i,int j) const
{
  static double arr[VECDIM]; static ThreeVector v(arr);
  GetLocalhField(i,j,v);
  return v.Mag();
}

int Zeeman::GetLocalBField(int i,int j,ThreeVector& localfield) const
{ GetLocalhField(i,j,localfield); localfield*=(mu0*Ms); return 0; }

int Zeeman::SetNomFieldVec3D(double newMs,const Vec3D& newBfield,
			     int fieldstep)
{
  double arr[3]; ThreeVector vtemp(arr);
  Convert(vtemp,newBfield);
  return SetNomField(newMs,vtemp,fieldstep);
}

UniformZeeman::UniformZeeman(int argc,const char *argp[]):Zeeman(argc,argp)
{ return; }

UniformZeeman::UniformZeeman(double _Ms,double _Bx,double _By,double _Bz)
  :Zeeman(0,NULL)
{
  double arr[3];
  arr[0]=_Bx; arr[1]=_By; arr[2]=_Bz;
  // HP CC does not implement "general initializer in initializer list".
  SetNomField(_Ms,ThreeVector(arr),0);
}

RibbonZeeman::RibbonZeeman(int argc,const char *argp[]):Zeeman(argc,argp)
{
  if(argc!=7) { 
    PlainError(1,"Error in RibbonZeeman::RibbonZeeman:"
	       " Wrong argument count: %d (should be 7)",argc);
  }
  relcharge=atof(argp[1])/(4*PI);
  ribx0=atof(argp[2]);
  riby0=atof(argp[3]);
  ribx1=atof(argp[4]);
  riby1=atof(argp[5]);
  ribheight=atof(argp[6]);
  h=(ThreeVector **)NULL;
}

RibbonZeeman::~RibbonZeeman(void)
{
  if(Nx>0 && Ny>0 && h!=(ThreeVector **)NULL)
    ThreeVector::Release2DArray(h);
}

void RibbonZeeman::FieldCalc(double dpar,double riblength,double dperp,
			     double &hpar,double &hperp)
{
  // Ribbon vertices are ({dpar,dpar+riblength},dperp,{+/- ribheight/2}).
  //  The z (out-of-plane) coordinate crosses octant boundaries, and so
  // is treated as  two symmetric pieces.  The x range may or may not
  // cross octant boundaries, and is in general not symmetric.
  //  See notes in MJD's Micromagnetic lab book, p 73-74, 7-Oct-96.
  //  NOTE: All calculation are in ratio's of distances, so uniform
  // scaling, by for example "cellsize", won't change output.
  //  The return value hpar is the x-component of the field at the origin,
  // and hperp is the y-component of the field at the origin.  (The
  // calling routine should adjust dpar dperp as necessary to determine
  // fields throughout the plane.)
  double radbot0sq,radbot1sq,temp0,temp1,temp;
  double radtop0,radtop1;

  if(dpar<0 && dpar+riblength>0) { // x coordinate crosses octant bdry
    // Break into two pieces, and call once for each piece
    double hpara,hperpa,hparb,hperpb;
    FieldCalc(dpar,-dpar,dperp,hpara,hperpa);
    FieldCalc(0.,dpar+riblength,dperp,hparb,hperpb);
    hpar=hpara+hparb;
    hperp=hperpa+hperpb;
  }
  else { // hpar coordinate stays in same octant
    radbot0sq=OC_SQ(dpar)+OC_SQ(dperp);
    radbot1sq=OC_SQ(dpar+riblength)+OC_SQ(dperp);
    radtop0=sqrt(radbot0sq+OC_SQ(ribheight/2.));
    radtop1=sqrt(radbot1sq+OC_SQ(ribheight/2.));
    // First do hpar, which is log(y+R), where R=(x^2+y^2+z^2)^(1/2)
    if(radbot0sq<OC_SQ(EPSILON))      hpar=-1/OC_SQ(EPSILON);
    else if(radbot1sq<OC_SQ(EPSILON)) hpar= 1/OC_SQ(EPSILON);
    else {
      temp0=ribheight/2.+radtop0;
      temp1=ribheight/2.+radtop1;
      temp=OC_SQ(temp1)*radbot0sq/(OC_SQ(temp0)*radbot1sq);
      hpar=log(temp); // This is symmetric, so double,
      // which inside log is equivalent to squaring.
    }
    // Now do hperp, which is -arctan(x*y/(z*R)); NOTE: arctan(0)=0.
    if(fabs(dperp)<OC_SQ(EPSILON*EPSILON)
       && fabs(dpar)<OC_SQ(EPSILON*EPSILON))
      hperp=0.0;
    else
      hperp=Oc_Atan2(fabs(dpar)*ribheight,(2*fabs(dperp)*radtop0));
    if(fabs(dperp)<OC_SQ(EPSILON*EPSILON) 
       && fabs(dpar+riblength)<OC_SQ(EPSILON*EPSILON))
      hperp+=0.0;
    else
      hperp-=Oc_Atan2(fabs(dpar+riblength)*ribheight,(2*fabs(dperp)*radtop1));
    hperp*=2;  // Symmetry

    // Adjust for the octant we are in
    if(dpar<0)  hperp*= -1;
    if(dperp<0) hperp*= -1;
    // NOTE: if dperp<0 && dpar<0 then hperp is unchanged

    // Adjust for relcharge
    hpar*=relcharge;
    hperp*=relcharge;
  }
}

int RibbonZeeman::SetCoords(Grid2D* grid,int imax,int jmax)
{
  // Release previous array, if any
  if(Nx>0 && Ny>0 && h!=(ThreeVector **)NULL)
    ThreeVector::Release2DArray(h);

  // Allocate new array
  if((Nx=imax)<1)
    PlainError(1,"Error in RibbonZeeman::SetCoords:"
	       " Illegal mesh width: %d cells",Nx);
  if((Ny=jmax)<1)
    PlainError(1,"Error in RibbonZeeman::SetCoords:"
	       " Illegal mesh height: %d cells",Ny);
  ThreeVector::Alloc2DArray(Nx,Ny,h);

  // Initialize new array (*ASSUMES* 2D grid)
  int i,j; double x,y,zdummy;
  double vpararr[VECDIM];  ThreeVector vpar(vpararr);   // || to ribbon
  double vperparr[VECDIM]; ThreeVector vperp(vperparr); // Perp. to ribbon
  double warr[VECDIM]; ThreeVector w(warr); // Work space
  double dpar,dperp,hpar,hperp;
  double riblength;  // Length of ribbon
  vpar[0]=ribx1-ribx0; vpar[1]=riby1-riby0; vpar[2]=0.0;
  riblength=vpar.Mag();
  vpar.Scale(1.0);
  vperp[0]= -vpar[1]; vperp[1]=vpar[0]; vperp[2]=vpar[2];
  w[1]=0.0;  // ASSUMES 2D grid (i.e., y is always 0)
  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    grid->GetRealCoords(i,j,x,y,zdummy);
    w[0]=ribx0-x; w[1]=riby0-y;
    dpar=w*vpar;
    dperp=w*vperp;
    FieldCalc(dpar,riblength,dperp,hpar,hperp);
    h[i][j].FastAdd(hpar,vpar,hperp,vperp);
  }
  return 0;
}

int RibbonZeeman::GetLocalhField(int i,int j,ThreeVector& localfield) const
{
#ifdef CODE_CHECKS
  if(i<0 || i>=Nx || j<0 || j>=Ny)
    PlainError(1,"Error in RibbonZeeman::GetLocalhField:"
	       " Index request (%d,%d) is out-of-bounds",i,j);
#endif // CODE_CHECKS
  localfield=h[i][j];
  return 0;
}

TieZeeman::TieZeeman(int argc,const char *argp[]):Zeeman(argc,argp)
{
  if(argc!=9) { 
    PlainError(1,"Error in TieZeeman::TieZeeman:"
	       " Wrong argument count: %d (should be 9)",argc);
  }
  nomhfield[0]=atof(argp[1]);
  nomhfield[1]=atof(argp[2]);
  nomhfield[2]=atof(argp[3]);
  ribx0=atof(argp[4]);
  riby0=atof(argp[5]);
  ribx1=atof(argp[6]);
  riby1=atof(argp[7]);
  ribwidth=atof(argp[8]);
  h=(ThreeVector **)NULL;
}

TieZeeman::~TieZeeman(void)
{
  if(Nx>0 && Ny>0 && h!=(ThreeVector **)NULL)
    ThreeVector::Release2DArray(h);
}

int TieZeeman::SetCoords(Grid2D* grid,int imax,int jmax)
{
  // Release previous array, if any
  if(Nx>0 && Ny>0 && h!=(ThreeVector **)NULL)
    ThreeVector::Release2DArray(h);

  // Allocate new array
  if((Nx=imax)<1)
    PlainError(1,"Error in TieZeeman::SetCoords:"
	       " Illegal mesh width: %d cells",Nx);
  if((Ny=jmax)<1)
    PlainError(1,"Error in TieZeeman::SetCoords:"
	       " Illegal mesh height: %d cells",Ny);
  ThreeVector::Alloc2DArray(Nx,Ny,h);

  // Initialize new array (*ASSUMES* 2D grid)
  int i,j; double x,y,zdummy;
  double vperparr[VECDIM]; ThreeVector vperp(vperparr); // Perp. component
  double vpararr[VECDIM];  ThreeVector vpar(vpararr);   // || component
  double warr[VECDIM];     ThreeVector w(warr);
  double riblength;
  vpar[0]=ribx1-ribx0; vpar[1]=0.0;  vpar[2]=riby1-riby0;
  riblength=vpar.Mag();
  vpar.Scale(1.0);
  vperp[0]=-vpar[2];   vperp[1]=0.0; vperp[2]=vpar[0];
  w[1]=0.0; // *ASSUMES* 2D grid
  double pardist,perpdist,cellsize;
  if((cellsize=grid->GetCellSize())<EPSILON)
    PlainError(1,"Error in TieZeeman::SetCoords:"
	       " Cellsize (%g) smaller than EPSILON",cellsize);
  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    grid->GetRealCoords(i,j,x,y,zdummy); w[0]=x-ribx0; w[2]=y-riby0;
    pardist=vpar*w;    perpdist=fabs(vperp*w);
    if(2*perpdist>ribwidth || pardist<0 || pardist>riblength)
      h[i][j]=0.;          // outside ribbon
    else
      h[i][j]=nomhfield;   // inside ribbon
  }

  return 0;
}

int TieZeeman::GetLocalhField(int i,int j,ThreeVector& localfield) const
{
#ifdef CODE_CHECKS
  if(i<0 || i>=Nx || j<0 || j>=Ny)
    PlainError(1,"Error in TieZeeman::GetLocalhField:"
	       " Index request (%d,%d) is out-of-bounds",i,j);
#endif // CODE_CHECKS
  localfield=h[i][j];
  return 0;
}

FileZeeman::FileZeeman(int argc,const char* argp[]):Zeeman(argc,argp)
{}

int FileZeeman::FillArray()
{
  // Initialize to zero.
  int i,j;
  for(j=0;j<Ny;j++) for(i=0;i<Nx;i++) h[i][j].Set(0.,0.,0.);

  if(filename.Length()==0) return 2;  // File not specified.  Leave zero.

  // Load mesh from file.
  Vf_FileInput* vffi=Vf_FileInput::NewReader(filename.GetStr());
  if(vffi==NULL) return 3;
  Vf_Mesh* mesh=vffi->NewMesh();
  if(mesh==NULL) return 4;

  // Get mesh extents.
  Nb_BoundingBox<OC_REAL8> range;  mesh->GetPreciseDataRange(range);
  Nb_Vec3<OC_REAL8> base;          range.GetMinPt(base);
  double xstep=range.GetWidth()/double(Nx);
  double ystep=range.GetHeight()/double(Ny);
  double zstep=range.GetDepth();  // Only 1 z-step in output h array
  // Adjust base to half-cell position
  base.x+=xstep/2.;
  base.y+=ystep/2.;
  base.z+=zstep/2.;

  // Step through range and sample mesh, fitting h array
  // with zeroth order fit.
  Nb_Vec3<OC_REAL8> pos;  Nb_LocatedVector<OC_REAL8> lv;
  lv.value.Set(1.,0.,0.); // Safety
  pos.z=base.z;
  OC_REAL8 scale=fieldmult*(mesh->GetValueMultiplier()/Ms)/mu0;
  /// Scales input, including conversion from T to A/m (B field -> H field).
  /// NOTE: This assumes that mesh->GetValueUnit and Ms are in the
  ///       same units!!!
  for(j=0;j<Ny;j++) {
    pos.y=base.y+j*ystep;
    for(i=0;i<Nx;i++) {
      pos.x=base.x+i*xstep;
      mesh->FindPreciseClosest(pos,lv);
      lv.value*=scale;
      h[i][j]=lv.value;
    }
  }

  // Clean up
  delete mesh;
  delete vffi;

  return 0;
}

int FileZeeman::SetCoords(Grid2D* grid,int imax,int jmax)
{
  // Check inputs
  if(imax<1) {
    PlainError(1,"Error in FileZeeman::SetCoords:"
		 " Illegal mesh width: %d cells",imax);
    return 1;
  }
  if(jmax<1) {
    PlainError(1,"Error in FileZeeman::SetCoords:"
	         " Illegal mesh height: %d cells",jmax);
    return 1;
  }

  // Resize h array
  h.Allocate(imax,jmax);
  Nx=imax; Ny=jmax;
  Ms=grid->GetMs();

  // Fill
  int errorcode=FillArray();

  return errorcode;
}

int FileZeeman::GetLocalhField(int i,int j,ThreeVector& localfield) const
{
#ifdef CODE_CHECKS
  if(i<0 || i>=Nx || j<0 || j>=Ny)
    PlainError(1,"Error in FileZeeman::GetLocalhField:"
	       " Index request (%d,%d) is out-of-bounds",i,j);
#endif // CODE_CHECKS
  Convert(localfield,h[i][j]);
  return 0;
}

OneFileZeeman::OneFileZeeman(int argc,const char* argp[])
  : FileZeeman(argc,argp)
{
  if(argc!=3) { 
    PlainError(1,"Error in OneFileZeeman::OneFileZeeman:"
	       " Wrong argument count: %d"
	       " (Usage: filename fieldmult)",argc);
  }
  filename=argp[1];
  fieldmult=atof(argp[2]);
}

FileSeqZeeman::FileSeqZeeman(int argc,const char* argp[])
  : FileZeeman(argc,argp)
{
  if(argc!=4) { 
    PlainError(1,"Error in FileSeqZeeman::FileSeqZeeman: Wrong argument count:"
	       " %d (Usage: scriptfile procname fieldmult)",argc);
  }
  scriptfile=argp[1];
  procname=argp[2];
  fieldmult=atof(argp[3]);
  filename="";

  // Source script file
  Oc_AutoBuf ab;
  Tcl_Interp *interp = Oc_GlobalInterpreter();
  Tcl_SavedResult saved;
  Tcl_SaveResult(interp, &saved);
  int result=Tcl_EvalFile(interp,ab(scriptfile.GetStr()));
  if(result!=TCL_OK) {
    Message("Warning in FileSeqZeeman::FileSeqZeeman: "
	    "Error trying to source scriptfile %s.",scriptfile.GetStr());
    procname="";
  }
  Tcl_RestoreResult(interp, &saved);
}

FileSeqZeeman::~FileSeqZeeman()
{ /* Nothing to do? */ }

int FileSeqZeeman::SetNomField(double newMs,const ThreeVector& newBfield,
			       int fieldstep)
{
  if(procname.Length()==0) return 0;  // procname not set.
  int errorcode;
  Tcl_Interp *interp=Oc_GlobalInterpreter();
  Tcl_SavedResult saved;
  Tcl_SaveResult(interp, &saved);
  Zeeman::SetNomField(newMs,newBfield,fieldstep);
  char *buf=new char[procname.Length()+512];  // Should be big enough
  sprintf(buf,"%s %.17g %.17g %.17g %d",procname.GetStr(),
	  newBfield[0],newBfield[1],newBfield[2],fieldstep);
  errorcode=Tcl_Eval(interp,buf);
  delete[] buf;
  if(errorcode!=TCL_OK) {
    Message("Warning from FileSeqZeeman::SetNomField: %s",
	    Tcl_GetStringResult(interp));
  } else {
    filename=Tcl_GetStringResult(interp);
    errorcode=FillArray();
  }
  Tcl_RestoreResult(interp, &saved);
  return errorcode;
}


MultiZeeman::MultiZeeman(int argc,const char* argp[]):Zeeman(argc,argp)
{
  Fcount=0; z=(Zeeman**)NULL;
  if(argc<4) { 
    PlainError(1,"Error in MultiZeeman::MultiZeeman:"
	       " Wrong argument count: %d (should be at least 4)",argc);
  }

  if((Fcount=atoi(argp[1]))<1) {
    PlainError(1,"Error in MultiZeeman::MultiZeeman:"
	       " Number of sub-fields (%d) must be positive",Fcount);
  }

  z=new Zeeman*[Fcount];

  int n,tempc; const char** tempp;
  for(n=0,tempp=argp+2;n<Fcount;n++) {
    if(tempp-argp>=argc) {
      PlainError(1,"Error in MultiZeeman::MultiZeeman:"
		 " %d field subtypes specified; Only %d found.",Fcount,n);
    }

    if((tempc=atoi(*tempp))<1) {
      PlainError(1,"Error in MultiZeeman::MultiZeeman:"
		 " Sub-field parameter count (%d) must be positive",tempc);
    }
    if((tempp-argp)+tempc>=argc) {
      PlainError(1,"Error in MultiZeeman::MultiZeeman:"
		 " Insufficent (%d) parameters",argc);
    }
    z[n]=Zeeman::MakeZeeman(tempc,tempp+1);
    tempp+=tempc+1;
  }
  if((tempp-argp)!=argc) {
    PlainError(1,"Error in MultiZeeman::MultiZeeman:"
	       " %d unused parameters",argc-(tempp-argp));
  }


}

MultiZeeman::~MultiZeeman(void)
{
  for(int n=0;n<Fcount;n++) {
    // Hmm, I *thought* delete[] z should delete all of these,    /**/
    // but it doesn't, at least using gcc 2.7.2.                  /**/
    delete z[n];
  }
  delete z;
}


int MultiZeeman::SetCoords(Grid2D* grid,int imax,int jmax)
{
  int n,errcode;
  for(n=errcode=0;n<Fcount;n++)
    errcode+=z[n]->SetCoords(grid,imax,jmax);
  return errcode;
}

int MultiZeeman::SetNomField(double newMs,const ThreeVector& newBfield,
			     int fieldstep)
{
  int n,errcode;
  errcode=Zeeman::SetNomField(newMs,newBfield,fieldstep);
  for(n=0;n<Fcount;n++)
    errcode+=z[n]->SetNomField(newMs,newBfield,fieldstep);
  return errcode;
}

int MultiZeeman::GetLocalhField(int i,int j,ThreeVector& localfield) const
{
  static double varr[VECDIM]; static ThreeVector v(varr);
  int n,errcode;
  localfield=0.; n=errcode=0;
  while(n<Fcount) {
    errcode+=z[n++]->GetLocalhField(i,j,v);
    localfield+=v;
  }
  return errcode;
}
