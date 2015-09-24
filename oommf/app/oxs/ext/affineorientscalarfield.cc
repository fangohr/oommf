/* FILE: affineorientscalarfield.cc      -*-Mode: c++-*-
 *
 * Scalar field object, derived from Oxs_ScalarField class,
 * that applies an affine orientation (pre-)transformation
 * to another scalar field object.
 *
 */

#include "oc.h"

#include "util.h"
#include "threevector.h"
#include "meshvalue.h"
#include "director.h"
#include "affineorientscalarfield.h"

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_AffineOrientScalarField);

/* End includes */

// Constructor
Oxs_AffineOrientScalarField::Oxs_AffineOrientScalarField(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_ScalarField(name,newdtr,argstr)
{
  // Get scalar field object
  OXS_GET_INIT_EXT_OBJECT("field",Oxs_ScalarField,field);

  // Get offset
  offset = GetThreeVectorInitValue("offset",ThreeVector(0,0,0));

  // Get matrix M elements
  vector<OC_REAL8m> M;
  if(HasInitValue("M")) {
    GetGroupedRealListInitValue("M",M);
  }
  switch(M.size()) {
  case 0: // Identity matrix
    row1.Set(1,0,0);
    row2.Set(0,1,0);
    row3.Set(0,0,1);
    break;
  case 1: // Simple scaling matrix
    row1.Set(M[0],0,0);
    row2.Set(0,M[0],0);
    row3.Set(0,0,M[0]);
    break;
  case 3: // Diagonal matrix
    row1.Set(M[0],0,0);
    row2.Set(0,M[1],0);
    row3.Set(0,0,M[2]);
    break;
  case 6: // Symmetric matrix
    row1.Set(M[0],M[1],M[2]);
    row2.Set(M[1],M[3],M[4]);
    row3.Set(M[2],M[4],M[5]);
    break;
  case 9: // General matrix
    row1.Set(M[0],M[1],M[2]);
    row2.Set(M[3],M[4],M[5]);
    row3.Set(M[6],M[7],M[8]);
    break;
  default:
    char buf[512];
    Oc_Snprintf(buf,sizeof(buf),
		"Invalid M list; length=%lu, should be 1, 3, 6 or 9.",
		static_cast<unsigned long>(M.size()));
    throw Oxs_ExtError(this,buf);
  }

  // Invert?
  OC_BOOL inverse = GetIntInitValue("inverse",0);
  OC_REAL8m checkslack = GetRealInitValue("inverse_slack",128.);
  if(inverse) {
    InvertMatrix(this,row1,row2,row3,checkslack);
    OC_REAL8m offx = -1*(row1*offset);
    OC_REAL8m offy = -1*(row2*offset);
    OC_REAL8m offz = -1*(row3*offset);
    offset.Set(offx,offy,offz);
  }

  VerifyAllInitArgsUsed();
}

Oxs_AffineOrientScalarField::~Oxs_AffineOrientScalarField()
{}

void
Oxs_AffineOrientScalarField::InvertMatrix
(Oxs_Ext* obj,
 Oxs_ThreeVector& A1,
 Oxs_ThreeVector& A2,
 Oxs_ThreeVector& A3,
 OC_REAL8m checkslack)
{ // Ai's are matrix rows
  Oxs_ThreeVector c1,c2,c3;

  c1 = A2 ^ A3;  // Cross product
  c2 = A3 ^ A1;
  c3 = A1 ^ A2;

  OC_REAL8m det = A1*c1;
  if(det==0.0 || (fabs(det)<1.0 && 1.0 >= DBL_MAX*fabs(det)) ) {
    throw Oxs_ExtError(obj,
	 "Input matrix is singular; can't invert.");
  }

  // Overflow protection
  OC_REAL8m maxc = 0.0;
  if(fabs(c1.x)>maxc) maxc = fabs(c1.x);
  if(fabs(c1.y)>maxc) maxc = fabs(c1.y);
  if(fabs(c1.z)>maxc) maxc = fabs(c1.z);
  if(fabs(c2.x)>maxc) maxc = fabs(c2.x);
  if(fabs(c2.y)>maxc) maxc = fabs(c2.y);
  if(fabs(c2.z)>maxc) maxc = fabs(c2.z);
  if(fabs(c3.x)>maxc) maxc = fabs(c3.x);
  if(fabs(c3.y)>maxc) maxc = fabs(c3.y);
  if(fabs(c3.z)>maxc) maxc = fabs(c3.z);
  if(fabs(det)<1.0 && maxc*maxc >= (DBL_MAX/3.)*fabs(det)) {
    throw Oxs_ExtError(obj,
	 "Unable to invert input matrix; numeric overflow detected.");
  }
  OC_REAL8m mult = 1.0/det;
  c1 *= mult;
  c2 *= mult;
  c3 *= mult;

  // Check quality
  OC_REAL8m errmax = 0.0;
  OC_REAL8m check;
  check = fabs(A1*c1-1.0);  if(check>errmax) errmax=check;
  check = fabs(A1*c2);      if(check>errmax) errmax=check;
  check = fabs(A1*c3);      if(check>errmax) errmax=check;
  check = fabs(A2*c1);      if(check>errmax) errmax=check;
  check = fabs(A2*c2-1.0);  if(check>errmax) errmax=check;
  check = fabs(A2*c3);      if(check>errmax) errmax=check;
  check = fabs(A3*c1);      if(check>errmax) errmax=check;
  check = fabs(A3*c2);      if(check>errmax) errmax=check;
  check = fabs(A3*c3-1.0);  if(check>errmax) errmax=check;
  if(errmax>checkslack*OC_REAL8_EPSILON) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
	"Error inverting input matrix; inverse accuracy only %g",
	static_cast<double>(errmax));
    throw Oxs_ExtError(obj,buf);
  }

  // Transpose and store
  A1.Set(c1.x,c2.x,c3.x);
  A2.Set(c1.y,c2.y,c3.y);
  A3.Set(c1.z,c2.z,c3.z);
}

OC_REAL8m
Oxs_AffineOrientScalarField::Value
(const ThreeVector& pt) const
{
  // Apply transform to pt
  ThreeVector newpt = ThreeVector(row1*pt,row2*pt,row3*pt);
  newpt += offset;
  return field->Value(newpt);
}

void
Oxs_AffineOrientScalarField::FillMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& array) const
{
  DefaultFillMeshValue(mesh,array);
}

void
Oxs_AffineOrientScalarField::IncrMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& array) const
{
  DefaultIncrMeshValue(mesh,array);
}

void
Oxs_AffineOrientScalarField::MultMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& array) const
{
  DefaultMultMeshValue(mesh,array);
}

