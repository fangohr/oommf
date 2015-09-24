/* FILE: maganis.cc	      -*-Mode: c++-*-
 *
 * This file contains function definitions for the mag anisotropy
 *  initialization routines.
 *
 * NOTE: These functions directly access the Grid2D's magnetization
 *       array, which use the xzy coordinate system.  For efficiency,
 *       these routines also use those coordinates, and the anisotropy
 *       directions passed in to these functions are assumed to be
 *       in the xzy coordinate system.  HOWEVER, the label identifiers
 *       for these functions in MIF files need to use the user xyz
 *       coordinates, so there may be an apparent mismatch between
 *       function and label names.  For example, the UniformXZAnisInit
 *       routine uses a MIF file label of "uniformxy."
 *
 */


#include "maganis.h"
#include "nb.h"
#include "vecio.h"

/* End includes */

///////////////////////////////////////////////////////////////////////
/// MagAnisInitListNode
///
void MagAnisInitListNode::AddNode(MagAnisInitListNode* new_node)
{
  if(next_node==NULL) next_node=new_node;
  else                next_node->AddNode(new_node);
}

MagAnisInitListNode::MagAnisInitListNode(ANIS_INIT_CHECK_FUNC* classcheck,
					 MagAnisInitListNode* firstnode)
{
  anischeck=classcheck;
  next_node=NULL;
  if(firstnode!=NULL) {
    firstnode->AddNode(this);
  }
}

ANIS_INIT_FUNC* MagAnisInitListNode::GetAnisInitFunc(const char* name)
{ // Returns NULL if no match found
  ANIS_INIT_FUNC* aifp=anischeck(name);
  if(aifp!=NULL || next_node==NULL) return aifp;
  return next_node->GetAnisInitFunc(name);
}


///////////////////////////////////////////////////////////////////////
/// Static (crystalline) anisotropy initialization classes
///////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////
/// Dummy first element
///
static ANIS_INIT_FUNC* DummyTypeCheck(const char*) { return NULL; }
MagAnisInitListNode AnisInitList(DummyTypeCheck,NULL);

///////////////////////////////////////////////////////////////////////
/// Constant (in space) anisotropy orientation
///
static ANIS_INIT_FUNC ConstantAnisInit;
static ANIS_INIT_CHECK_FUNC ConstantAnisTypeCheck;
static MagAnisInitListNode ConstantAnis(ConstantAnisTypeCheck,&AnisInitList);

ANIS_INIT_FUNC* ConstantAnisTypeCheck(const char* name)
{
  if(Nb_StrCaseCmp(name,"constant")==0) return ConstantAnisInit;
  return NULL;
}

int ConstantAnisInit(/* Imports: */
		     int /* argc */,const char* const* /* argv */,
		     int Nx,int Nz,MagEltAnisType meat,
		     double K1,ThreeVector& dirA,
		     ThreeVector& dirB,ThreeVector& dirC,
		     /* Exports: */
		     MagElt** m,ThreeVector** &anisA,
		     ThreeVector** &anisB,ThreeVector** &anisC)
{
  int i,k;
  // Initialize K1 inside each magelt.
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    m[i][k].SetK1(K1);
  }

  // Setup anisotropy direction arrays (1 or 0 elements!)
  ThreeVector::Alloc2DArray(1,1,anisA);
  anisA[0][0]=dirA; /* Copy data */
  double *aptr=anisA[0][0];
  double *bptr=NULL,*cptr=NULL;
  if(meat==meat_uniaxial) { anisB=anisC=NULL; }
  else {
    ThreeVector::Alloc2DArray(1,1,anisB);
    anisB[0][0]=dirB; /* Copy data */
    bptr=anisB[0][0];
    ThreeVector::Alloc2DArray(1,1,anisC);
    anisC[0][0]=dirC; /* Copy data */
    cptr=anisC[0][0];
  }

  // Point MagElt anisdir pointers into above arrays
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    m[i][k].InitAnisDirs(aptr,bptr,cptr);
  }

  return 0;
}

///////////////////////////////////////////////////////////////////////
/// Uniformly random in XZ-plane anisotropy orientation (i.e., $K\in S^1$)
///
static ANIS_INIT_FUNC UniformXZAnisInit;
static ANIS_INIT_CHECK_FUNC UniformXZAnisTypeCheck;
static MagAnisInitListNode UniformXZAnis(UniformXZAnisTypeCheck,&AnisInitList);

ANIS_INIT_FUNC* UniformXZAnisTypeCheck(const char* name)
{
  // NOTE: The apparent mismatch between the MIF label name (uniformxy)
  //  and these routine names (UniformXZ) is due to the the use of
  //  xzy coords internally, and xyz coords externally.  See the NOTE
  //  at the top of this file.
  if(Nb_StrCaseCmp(name,"uniformxy")==0) return UniformXZAnisInit;
  return NULL;
}

int UniformXZAnisInit(/* Imports: */
		     int /* argc */,const char* const* /* argv */,
		     int Nx,int Nz,MagEltAnisType meat,
		     double K1,ThreeVector& /* dirA */,
		     ThreeVector& /* dirB */,ThreeVector& /* dirC */,
		     /* Exports: */
		     MagElt** m,ThreeVector** &anisA,
		     ThreeVector** &anisB,ThreeVector** &anisC)
{
  int i,k;
  int anis_count=3;
  if(meat==meat_uniaxial) anis_count=1;

  // Initialize K1 inside each magelt.
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    m[i][k].SetK1(K1);
  }

  // Allocate anis array(s)
  ThreeVector::Alloc2DArray(Nx,Nz,anisA);
  if(anis_count==1) { anisB=anisC=NULL; }
  else {
    ThreeVector::Alloc2DArray(Nx,Nz,anisB);
    ThreeVector::Alloc2DArray(Nx,Nz,anisC);
  }

  // Initialize anis arrays
  double theta,x,z;
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    theta=Oc_UnifRand()*2*PI;
    x=cos(theta);
    z=1-x*x;
    if(z<0.0) z=0.0; else z=sqrt(z); // Safety
    if(theta>PI) z*=-1;
    anisA[i][k].Set(x,0.,z);
    if(anis_count!=1) {
      anisB[i][k].Set(-z,0.,x);
      anisC[i][k].Set(0.,1.,0.);
    }
  }

  // Point MagElt anisdir pointers into above anis arrays
  if(anis_count==1) {
    for(i=0;i<Nx;i++) for(k=0;k<Nz;k++)
      m[i][k].InitAnisDirs(anisA[i][k],NULL,NULL);
  }
  else {
    for(i=0;i<Nx;i++) for(k=0;k<Nz;k++)
      m[i][k].InitAnisDirs(anisA[i][k],anisB[i][k],anisC[i][k]);
  }

  return 0;
}

///////////////////////////////////////////////////////////////////////
/// Uniformly random in 3-space anisotropy orientation (i.e., $K\in S^2$)
///
static ANIS_INIT_FUNC UniformS2AnisInit;
static ANIS_INIT_CHECK_FUNC UniformS2AnisTypeCheck;
static MagAnisInitListNode UniformS2Anis(UniformS2AnisTypeCheck,
					 &AnisInitList);

ANIS_INIT_FUNC* UniformS2AnisTypeCheck(const char* name)
{
  if(Nb_StrCaseCmp(name,"uniforms2")==0) return UniformS2AnisInit;
  return NULL;
}

int UniformS2AnisInit(/* Imports: */
		     int /* argc */,const char* const* /* argv */,
		     int Nx,int Nz,MagEltAnisType meat,
		     double K1,ThreeVector& /* dirA */,
		     ThreeVector& /* dirB */,ThreeVector& /* dirC */,
		     /* Exports: */
		     MagElt** m,ThreeVector** &anisA,
		     ThreeVector** &anisB,ThreeVector** &anisC)
{
  int i,k;
  int anis_count=3;
  if(meat==meat_uniaxial) anis_count=1;

  // Initialize K1 inside each magelt.
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    m[i][k].SetK1(K1);
  }

  // Allocate anis array(s)
  ThreeVector::Alloc2DArray(Nx,Nz,anisA);
  if(anis_count==1) { anisB=anisC=NULL; }
  else {
    ThreeVector::Alloc2DArray(Nx,Nz,anisB);
    ThreeVector::Alloc2DArray(Nx,Nz,anisC);
  }

  // Initialize anis arrays
  double dummy1arr[3],dummy2arr[3],dummy3arr[3];
  ThreeVector dummy1(dummy1arr),dummy2(dummy2arr),dummy3(dummy3arr);
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    dummy1.Random(1.0);  // Get random vector
    dummy1.PreciseScale(1.0);
    anisA[i][k]=dummy1;
    if(anis_count!=1) {
      double dot,dot2;
      do {
	dummy3.Random(1.0);
	dot=PreciseDot(dummy1,dummy3);
      } while(fabs(dot)>0.8);
      dummy2.FastAdd(dummy3,-dot,dummy1);  // Make perp. to dummy1
      dummy2.PreciseScale(1.0);
      dot=PreciseDot(dummy1,dummy2); // For increased precision, redo
      dummy2.Accum(-dot,dummy1);     // "perpendicularization."
      dummy2.PreciseScale(1.0);
      anisB[i][k]=dummy2;
      dummy3=dummy1;
      dummy3^=dummy2;
      dummy3.PreciseScale(1.0);  // Safety
      dot= -PreciseDot(dummy1,dummy3);  // Redo to improve precision
      dot2= -PreciseDot(dummy2,dummy3);
      Nb_Xpfloat x; // Should probably have an extra-prec. vector class
      x.Set(dot*dummy1[0]); x.Accum(dot2*dummy2[0]); x.Accum(dummy3[0]);
      dummy3[0]=x.GetValue();
      x.Set(dot*dummy1[1]); x.Accum(dot2*dummy2[1]); x.Accum(dummy3[1]);
      dummy3[1]=x.GetValue();
      x.Set(dot*dummy1[2]); x.Accum(dot2*dummy2[2]); x.Accum(dummy3[2]);
      dummy3[2]=x.GetValue();
      dummy3.PreciseScale(1.0);
      anisC[i][k]=dummy3;
    }
  }

  // Point MagElt anisdir pointers into above anis arrays
  if(anis_count==1) {
    for(i=0;i<Nx;i++) for(k=0;k<Nz;k++)
      m[i][k].InitAnisDirs(anisA[i][k],NULL,NULL);
  }
  else {
    for(i=0;i<Nx;i++) for(k=0;k<Nz;k++)
      m[i][k].InitAnisDirs(anisA[i][k],anisB[i][k],anisC[i][k]);
  }

  return 0;
}


///////////////////////////////////////////////////////////////////////
/// Example user-defined anisotropy initialization routine
///
/// This routine is specified in the input .mif file by a line of
/// the form
///           Anisotropy Init: MyAnis Kl L1 L2 L3 Ku U1 U2 U3
///
/// where Kl and Ku are the anisotropy constants for the lower and
/// upper half of the sample, and L[], U[] are the uniaxial anisotropy
/// axis for the same lower and upper regions.
///   You *must* specify "Anisotropy Type: uniaxial" when using this
/// routine.  Extension to the cubic type is left as an exercise for
/// the reader. ;^)
///

static ANIS_INIT_FUNC MyAnisInit;
static ANIS_INIT_CHECK_FUNC MyAnisTypeCheck;

// Register MyAnis with MagAnisInitListNode::GetAnisInitFunc()
// during file static initializations.
static MagAnisInitListNode MyAnis(MyAnisTypeCheck,&AnisInitList);

// Routine that does .mif anis init routine name checking.
ANIS_INIT_FUNC* MyAnisTypeCheck(const char* name)
{
  if(Nb_StrCaseCmp(name,"myanis")==0) return MyAnisInit;
  return NULL;
}

// MyAnis anisotropy initialization function.
int MyAnisInit(/* Imports: */
	       int argc,const char* const* argv,
	       int Nx,int Nz,MagEltAnisType meat,
	       double /* K1 */,
	       ThreeVector& /* dirA */,
	       ThreeVector& /* dirB */,
	       ThreeVector& /* dirC */,
	       /* Exports: */
	       MagElt** m,ThreeVector** &anisA,
	       ThreeVector** &anisB,ThreeVector** &anisC)
{ // Example anisotropy initialization function.  We
  // ignore imports K1, dirA, ..., and grab needed parameters
  // off the .mif input line (as stored in argv).

  // Consistency checks
  if(meat!=meat_uniaxial) {
    PlainError(1,"Error in MyAnisInit(): This anisotropy "
	       "initialization function only supports "
	       "uniaxial type anisotropy");
  }
  // NB: The first parameter, argv[0], is the 'Anistropy Init'
  //     name, which in this case is "myanis".
  if(argc!=9) {
    PlainError(1,"Error in MyAnisInit(): Wrong number of .mif"
	       " input line parameters (%d); should be 8:\n"
	       " Klower L1 L2 L3 Kupper U1 U2 U3",argc-1);
  }
  double Klower=Nb_Atof(argv[1]);
  double Kupper=Nb_Atof(argv[5]);
  double L_arr[3],U_arr[3];
  for(int comp=0;comp<3;comp++) {
    L_arr[comp]=Nb_Atof(argv[2+comp]);
    U_arr[comp]=Nb_Atof(argv[6+comp]);
  }

  // The solver works with xzy coordinates (x to the left, z up,
  // and y into the plane), while the input .mif file specifies
  // xyz coords (x to the left, y up, and z out of the plane).
  // Convert from xyz to xzy:
  double temp;
  temp=L_arr[1]; L_arr[1]=-L_arr[2]; L_arr[2]=temp;
  temp=U_arr[1]; U_arr[1]=-U_arr[2]; U_arr[2]=temp;

  // Initialize K inside each magelt.
  int i,k;
  for(i=0;i<Nx;i++) {
    for(k=0;k<Nz/2;k++) m[i][k].SetK1(Klower);
    for(;k<Nz;k++)      m[i][k].SetK1(Kupper);
  }

  // Setup anisotropy direction arrays.  The anisA array could be
  // dimensioned the same as the magelt arrays, but we only need
  // to store two directions, so we allocate only 2 elements and
  // point all magelts to one or the other.  (Refer to file
  // threevec.h for details on the ThreeVector class.)
  ThreeVector::Alloc2DArray(1,2,anisA);
  anisA[0][0].Dup(L_arr); // Copy data
  anisA[0][1].Dup(U_arr);
  anisA[0][0].Scale(1.0); // Make sure these are unit vectors
  anisA[0][1].Scale(1.0);
  anisB=anisC=NULL;       // anisB & anisC not used for uniaxial anis.
  for(i=0;i<Nx;i++) {
    // Point MagElt anisdir pointers to above vectors
    for(k=0;k<Nz/2;k++)
      m[i][k].InitAnisDirs((double *)anisA[0][0],NULL,NULL);
    for(;k<Nz;k++)
      m[i][k].InitAnisDirs((double *)anisA[0][1],NULL,NULL);
  }

  return 0;
}

