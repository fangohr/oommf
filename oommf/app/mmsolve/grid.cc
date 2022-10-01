/* FILE: grid.cc           -*-Mode: c++-*-
 *
 * This file contains class definitions for micromagnetic grid classes.
 *
 */

/* Revision history:
 *  7-Apr-96: Removed last vestiges of g2d file format.
 *  9-Jul-96: Added support for maximum angle difference determination.
 *  8-Jul-96: Revamped ODE solver.  Introduced 4th order Runge-Kutta
 *            solver with adaptive stepsize.
 * 30-Jun-96: Added Array2D<MagElt> support.
 */

#include <cassert>
#include <cctype>
#include <cstring>

#include "oc.h"
#include "nb.h"

#include "demag.h"
#include "grid.h"
#include "maganis.h"
#include "magelt.h"
#include "vecio.h"

/* End includes */

#undef BAD_LOCAL_DEMAG_HACK // Define this to get cellwise self-demag
/// thickness adjustment.  This adjustment is *not* balanced with global
/// demag field.  Not recommended.

#undef WEAK_BOUNDARY // 0.5  // Set boundary element reduced thickness to
// this. #undef or set to 0.0 to get sharp boundaries.
#undef EDGE_ADJUST // 1.0 // Alternate form of WEAK_BOUNDARY; Affects demag
                    // only, and only works with RECTANGULAR sample shapes!!!
#undef ROUGH_BOUNDARY
    // If #defined, then each boundary thickness is independently multiplied
    // by an amount drawn from a uniform distribution on [0,1].

// ODE parameters: GyRatio and DampCoef.  The default value for GyRatio
// (gyromagnetic ratio) given by MIF, is 2.21e5 m/A.s.  This is used in
// conjuction with DampCoef (damping coefficient), which is
// dimensionless (default from MIF is 0.5). The damping coefficient may
// be obtained via FMR studies as
//
//    DampCoef=(Delta H).GyRatio/(2.(fmr_driving_frequency)).
//
// 'Delta H' here is the FMR line width.  In the literature,
// GyRatio is denoted by \gamma, fmr_driving_frequency is \omega_0,
// and DampCoef is often written \alpha or \alpha/(1+\alpha).
//   Realistic values for DampCoef are in the range [.004,.15], but for
// fast convergence of the ODE, DampCoef=0.5 is generally pretty good.
//   LLG (Landau-Lifschitz-Gilbert):
//
//     dm/dt = (-GyRatio.Ms)(mxh)-(DampCoef.GyRatio.Ms)(mx(mxh))
//
// (This is really the Landau-Lifschitz formulation.  Divide the
// RHS by (1+DampCoef^2) for the Landau-Lifschitz-Gilbert formulation.)
// This program actually solves RHS=(-1/DampCoef)(mxh)-mx(mxh).
// The routine Grid2D::GetTimeStep() does the conversion from StepSize
// to actual time step in seconds, via
//             time_step = StepSize/(DampCoef.GyRatio.Ms)
// See also PRECESSION_RATIO below.
//
// BIG NOTE: *Everywhere* in this code, "torque" refers to the RHS
// of the solved ODE, i.e., (-1/DampCoef)(mxh)-mx(mxh).  The public
// interface provides "|mxh|," which should be free of confusion.
// If DoPrecess is false, then DampCoef is effectively infinite,
// i.e., the (mxh) term is ignored.

// #define exactly one of the following to select the coefficient
// to use with the internal demag routine.
#undef CYL_COEFS      // This is the one you get if you don't #define any
#undef SQUARE_COEFS
#define PAPER_COEFS

// Miscellaneous #define's
#undef OLD_CODE
#undef VERBOSE
#define PRECESSION_RATIO (-1./DampCoef)
          // Relative size of precession term. See above discussion
          // about DampCoef and GyRatio.

#undef CODE_CHECKS

// ODE Solvers; Define exactly one of the following
#undef EULER_ODE       // 1st order Euler
#define PREDICTOR_2_ODE // 2nd order predictor/corrector
#undef RUNGE_KUTTA_ODE  // 4th order Runge-Kutta
                        // (cf. MaxTorqueStep note below)
#undef RUNGE_KUTTA_FAST // Define this *in addition to RUNGE_KUTTA_ODE*
                         // to use approximate field values (i.e.,
                         // hFastUpdate) for internal ODE steps.

// Error control
#undef SMALL_STEPS  // Define this to get slower, but more accurate results
#ifndef RUNGE_KUTTA_ODE
#ifdef SMALL_STEPS
const double MaxTorqueStep  =  0.0175; // StepSize*maxtorque restricted to
     // being less than this amount.  This will limit magnetization shifts
     // to roughly < atan(0.0175) --- about 1 degree --- per ODE step.
#else
const double MaxTorqueStep  =  0.0875; // StepSize*maxtorque restricted to
     // being less than this amount.  This will limit magnetization shifts
     // to roughly < atan(0.0875) --- about 5 degrees --- per ODE step.
#endif // SMALL_STEPS
#else  // Current Runge-Kutta solver takes double step
#ifdef SMALL_STEPS
const double MaxTorqueStep  =  0.035;
#else
const double MaxTorqueStep  =  0.175;
#endif // SMALL_STEPS
#endif
#ifdef SMALL_STEPS
const double AllowedSpinError =  0.000175;  // Allowed error per spin
/// Affects ODE solvers; atan(0.000175) is about 0.01 degree.
#else
const double AllowedSpinError =  0.00175;  // Allowed error per spin
/// Affects ODE solvers; atan(0.00175) is about 0.1 degree.
#endif // SMALL_STEPS

#ifdef STOPWATCH_ON
#define STOPWATCH_START(watch) watch.Start();
#define STOPWATCH_STOP(watch)  watch.Stop();
#else // !STOPWATCH_ON
#define STOPWATCH_START(watch)
#define STOPWATCH_STOP(watch)
#endif // STOPWATCH_ON

// This is a temporary hack, until we can better control "Step too small"
// errors.
#define MAX_TOO_SMALL_MSGS 5         // Count on number of "Step too small"
static int too_small_msg_count=0;    // messages to send out.
void SendSmallStepMessage(const char *src,double stepsize,double maxtorque,
                          const char *addendum=NULL)
{
  ++too_small_msg_count;
  if(too_small_msg_count>MAX_TOO_SMALL_MSGS) return;  // Skip
  if(too_small_msg_count<MAX_TOO_SMALL_MSGS) {
    if(addendum==NULL) 
      Message("Warning in %s: Step (%g) too small (maxtorque=%g)",
              src,stepsize,maxtorque);
    else
      Message("Warning in %s: Step (%g) too small (maxtorque=%g)\n%s",
              src,stepsize,maxtorque,addendum);

  } else {
    if(addendum==NULL) 
      Message("Warning in %s: Step (%g) too small (maxtorque=%g)"
              " (All further messages of this type will be suppressed.)",
              src,stepsize,maxtorque);
    else
      Message("Warning in %s: Step (%g) too small (maxtorque=%g)\n%s\n"
              " (All further messages of this type will be suppressed.)",
              src,stepsize,maxtorque,addendum);
  }
}

// Grid (mesh) file magic cookie string
// const char GridFileMagic03[8] = "G2DFil\x03";   OBSOLETE
// const double GridFileEndianTest = 1.234;        OBSOLETE
// The bit pattern for 1.234 on a little endian machine (e.g.,
// Intel's x86 series) should be 58 39 B4 C8 76 BE F3 3F, while
// on a big endian machine it is 3F F3 BE 76 C8 B4 39 58.  Examples
// of big endian machines include ones using the MIPS R4000 and R8000
// series chips, for example SGI's.


#ifdef BDRY_CODE
// Boundary code support.
static const int MagNbhdInterior    = 0;
static const int MagNbhdLeft        = 0x07;
static const int MagNbhdBottomLeft  = 0x2F;
static const int MagNbhdBottom      = 0x29;
static const int MagNbhdBottomRight = 0xE9;
static const int MagNbhdRight       = 0xE0;
static const int MagNbhdTopRight    = 0xF4;
static const int MagNbhdTop         = 0x94;
static const int MagNbhdTopLeft     = 0x97;
// The above codes specify which neighbors have a thickness different
// from the current cell, where each neighbor is associated with one bit
// in the code via the following schematic:
//
//                                2 4 7
//                                1 x 6
//                                0 3 5
//
// NOTE: There are 256 possible bit patterns, but only 9 codes above.  The
// other patterns are harder to classify, representing islands, isthmus and
// the like.
//
#endif // BDRY_CODE

/////////////////////////// Grid2D Code /////////////////////////////////

// Coordinate transformations.  The 2D grid/solver uses x+z in-plane
// (width and height), with y pointing into the screen (thickness).
//  For reference this is called the "xzy" coordinate system.  All
// outside OOMMF code (including the MIF class) uses x+y in-plane
// (width and height), with z pointing out of the screen (thickness).
// This is referred to as the "xyz" coordinate system.
void Grid2D::ConvertXyzToXzy(ThreeVector &vec)
{
  double temp=vec[1];  vec[1]=-vec[2];  vec[2]=temp;
}

void Grid2D::ConvertXzyToXyz(ThreeVector &vec)
{
  double temp=vec[1];  vec[1]=vec[2];  vec[2]=-temp;
}


double Grid2D::GetTimeStep() const
{ // Returns size of last step, in seconds
  return StepSize/(GyRatio*DampCoef*Ms);
}

double Grid2D::ConvertTimeToStepSize(double time) const
{ // Given import time in seconds, returns the corresponding StepSize.
  return time*GyRatio*DampCoef*Ms;
  
}

void Grid2D::Allocate(int xcount,int zcount)
{ // NOTE: This routine requires that Grid2d::ExternalDemag be initialized,
  //       since *ExternalDemag->init is called.
  int i;

  if(xcount<1 || zcount<1)
    PlainError(1,"Bad Parameter Error in Grid2D::Allocate:"
               " xcount=%d, zcount=%d",xcount,zcount);

  Nx=xcount; Nz=zcount;
  if(DoDemag) ThreeVector::Alloc2DArray(Nx,Nz,hdemag);
  else        hdemag=(ThreeVector**)NULL;
  ThreeVector::Alloc2DArray(Nx,Nz,h);
  ThreeVector::Alloc2DArray(Nx,Nz,torque);
  ThreeVector::Alloc2DArray(Nx,Nz,torque0);
  ThreeVector::Alloc2DArray(Nx,Nz,torque1);
  ThreeVector::Alloc2DArray(Nx,Nz,h0);
  ThreeVector::Alloc2DArray(Nx,Nz,h1);
  
  // Allocate MagElt arrays
  m0.Allocate(Nx,Nz);
  m1.Allocate(Nx,Nz);
  m2.Allocate(Nx,Nz);
  m.Allocate(Nx,Nz);
  thickness_sum=Nx*Nz;  // Note: This will be recalculated after
  /// the sample geometry is set.

  // Allocate energy density arrays
  energy_density.Allocate(Nx,Nz);
  energy_density1.Allocate(Nx,Nz);

  // Allocate demag calculation arrays
  if(DoDemag) {
    if(ExternalDemag->calc==NULL) { // Default to internal routine
      // Allocate demagcoef matrices
      if((ADemagCoef=new double*[Nx])==0)
        PlainError(1,"Error in Grid2D::Allocate: %s",ErrNoMem);
      if((CDemagCoef=new double*[Nx])==0)
        PlainError(1,"Error in Grid2D::Allocate: %s",ErrNoMem);
      for(i=0;i<Nx;i++) {
        if((ADemagCoef[i]=new double[Nz])==0)
          PlainError(1,"Error in Grid2D::Allocate: %s",ErrNoMem);
        if((CDemagCoef[i]=new double[Nz])==0)
          PlainError(1,"Error in Grid2D::Allocate: %s",ErrNoMem);
      }
    }
    else { // Use external routine
      ADemagCoef=CDemagCoef=NULL;
      (*ExternalDemag->init)(Nx,Nz,ExternalDemag->paramcount,
                             ExternalDemag->params);
    }
  } // fi(DoDemag)
}


int Grid2D::FindNeighbors(int nsize,Nb_Array2D<MagElt> &me,
                          MagEltLink men[],int i,int k)
{ // NOTE: Currently does only 4 and 8 neighborhoods.
  // NOTE 2: Enforces Neuman bdry conditions along edges of gridded
  //       region.  Hmm, you may get different results if the sample edge
  //       is not the same as the gridded region edge (e.g., by setting
  //       |m|=0 inside gridded region to, for example, round corners.)
  //       -mjd 5-Dec-1996.
#ifdef CODE_CHECKS
  if(nsize!=4 && nsize!=8)
    PlainError(1,"Error in Grid2D::FindNeighbors: "
             "Unsupported neighborhood size (%d) request",nsize);
  if(me.GetSize()<1 || men==(MagEltLink*)NULL) 
    PlainError(1,"Error in Grid2D::FindNeighbors: Null pointer received");
  if(i<0 || i>=Nx || k<0 || k>=Nz)
    PlainError(1,"Error in Grid2D::FindNeighbors: "
             "Requested index elt (%d,%d) out-of-bounds (0-%d,0-%d)",
             i,k,Nx,Nz);
#endif
  hValid=torqueValid=energyValid=0;
  double my_thick=me[i][k].GetThickness();
  if(my_thick<=0.) return 0; // Zero thickness elements don't get connected.
  double my_thin=1.0/my_thick;

  static int ioff[8]={ 1, 1, 0,-1,-1,-1, 0, 1};
  static int koff[8]={ 0, 1, 1, 1, 0,-1,-1,-1};
  int n,offset,offstep,it,kt,new_ngbrcnt;
  if(nsize==4) offstep=2;  else offstep=1;
  for(n=offset=new_ngbrcnt=0;n<nsize;n++,offset+=offstep) {
    it=i+ioff[offset];
    kt=k+koff[offset];

    // Enforce Neumann bdry conds.,  \part(m)/\part(n)=0,
    // at grid edges
    if(it<0)        it=0;
    else if(it>=Nx) it=Nx-1;
    if(kt<0)        kt=0;
    else if(kt>=Nz) kt=Nz-1;
    if(it==i && kt==k) continue;

    // Handle irregular boundaries
    if(me[it][kt].GetThickness()<=0.) {
      if(it==i || kt==k) continue;  // Set up alternate connection
      /// only for diagonal links

      // See which of the nearest neighbors is non-zero
      int itt=it;  int ktt=kt;
      if(me[i][kt].GetThickness()>0.) itt=i;
      if(me[it][k].GetThickness()>0.) ktt=k;

      if(itt==it && ktt==kt) continue; // Neither; Skip link
      if(itt==i  && ktt==k)  continue; // Both; Skip link

      it=itt; kt=ktt;
    }

#ifdef CODE_CHECKS
    if((it==i && kt==k) || me[it][kt].GetThickness()<=0.)
      PlainError(1,"Error in Grid2D::FindNeighbors: "
               "Programming error: Bad neighbor link");
#endif // CODE_CHECKS

    // Add neighbor
    double ngbr_thick=me[it][kt].GetThickness();
    men[new_ngbrcnt].node=&(me[it][kt]);
    men[new_ngbrcnt].weight=EXCHANGE_THICKNESS_ADJ(my_thick,ngbr_thick)*my_thin;
    /// EXCHANGE_THICKNESS_ADJ is #define'd in magelt.h.  We divide by my_thick
    /// for calculation of energy density & field (cf. MagElt::CalculateExchange()).
    /// The MagElt::CalculateExchangeEnergy() routine multiplies the energy
    /// density by my_thick, to get representative energy for the cell.
    new_ngbrcnt++;
  }
  return new_ngbrcnt;
}

void Grid2D::InitRectangle(Nb_DString &)
{ // Default part geometry
  return; // Nothing to do
}

void Grid2D::InitEllipse(Nb_DString &)
{ // Zero's thicknesses outside ellipse defined by Nx,Nz
  if(Nx<2 || Nz<2) return; // Nothing to do
  int i,k; double x,z;
  for(i=0;i<Nx;i++) {
    x= double(2*i+1)/double(Nx)-1.0;
    for(k=0;k<Nz;k++) {
      z= double(2*k+1)/double(Nz)-1.0;
      if(OC_SQ(x)+OC_SQ(z)>1+OC_SQRT_REAL8_EPSILON)
        m[i][k].SetThickness(0.0);
    }
  }
  hValid=torqueValid=energyValid=0;
}

void Grid2D::InitEllipsoid(Nb_DString &)
{ // Simulates an inscribed ellipsoid (Nx x Nz x thickness)
  if(Nx<2 || Nz<2) return; // Nothing to do
  int i,k; double x,z,radsq;
  for(i=0;i<Nx;i++) {
    x= double(2*i+1)/double(Nx)-1.0;
    for(k=0;k<Nz;k++) {
      z= double(2*k+1)/double(Nz)-1.0;
      radsq=OC_SQ(x)+OC_SQ(z);
      if(radsq>=1.0)  m[i][k].SetThickness(0.0);
      else            m[i][k].SetThickness(float(sqrt(1.0-radsq)));
    }
  }
  hValid=torqueValid=energyValid=0;
}

void Grid2D::Mask(Nb_DString &filepoint)
{ // Adjusts thicknesses; black->full (nominal) thickness, others
  //   scaled linearly by brightest value (where bright=R+G+B)
  //   with white (defined as R=G=B=maximum color-component
  //   value; cf. ppm format specification) mapping to 0 thickness.
  if(Nz<1 || Nx<1) return; // Nothing to fill (empty grid).

  // Load image into an Nb_ImgObj object
  Nb_ImgObj image;
  image.LoadFile(filepoint.GetStr());

  // Get header info
  OC_INT4m picwidth  = image.Width();
  OC_INT4m picheight = image.Height();
  OC_INT4m maxval = image.MaxValue();
  /// maxval is the maximum color-component pixel value;
  /// This value determines scaling, where R+G+B=maxval is taken
  /// to be white and is assigned zero thickness.

  OC_INT4m maxbright=3*maxval;  // We define "brightness" to be
  /// R+G+B.  Other component weightings are possible, but for
  /// current purposes this is simplest.  Ideally, the input is
  /// monochromatic (e.g., grey scale), so the relative weights
  /// don't matter.

  double maxbright_recip=1.0/double(maxbright);

  if(picheight<1 || picwidth<1) {
    Message("Warning in Grid2D::Mask(Nb_DString %s): "
            "Empty image (%d x %d)",filepoint.GetStr(),
            picwidth,picheight);
    return;
  }

  // Process image data
  OC_INT4m i,k,row,column;
  OC_INT4m r,g,b;
  double xstep = double(picwidth)/double(Nx);
  double zstep = double(picheight)/double(Nz);
  for(k=0;k<Nz;++k) {
    row = picheight - 1 - OC_INT4m(floor((double(k)+0.5)*zstep));
    assert(0<=row && row<picheight);
    for(i=0;i<Nx;++i) {
      column = OC_INT4m(floor((double(i)+0.5)*xstep));
      assert(0<=column && column<picwidth);
      image.PixelValue(row,column,r,g,b);
      OC_INT4m val = maxbright - (r + g + b);
      if(val<0) val=0; // Safety
      double pixthick = static_cast<double>(val) * maxbright_recip;
      m[i][k].SetThickness(static_cast<float>(pixthick));
    }
  }
}

void Grid2D::InitOval(Nb_DString &inprop)
{ // Rounds corners off Nx by Nz rectangle by zeroing
  // thickness outside of quarter circles of radius...
  double rounded_proportion=Nb_Atof(inprop.GetStr());
  if(rounded_proportion>1.0) rounded_proportion=1.0;
  if(rounded_proportion<0.0) rounded_proportion=0.0;
  
  double radius=0.5*rounded_proportion*OC_MIN(Nx,Nz);

  // Corner rounding circle center coordinates
  double cx1,cx2,cz1,cz2;
  cx1=radius-0.5;   cx2=Nx-0.5-radius;
  cz1=radius-0.5;   cz2=Nz-0.5-radius;
  if(cx1>cx2 || cz1>cz2)
    PlainError(1,"Error in Grid2D::InitOval: %s; Nx=%d, Nz=%d, radius=%g",
               ErrProgramming,Nx,Nz,radius);

  // Round lower left corner
  int i,k;   double xdistsq,zdistsq,radsq=OC_SQ(radius);
  for(i=0;i<(int)floor(cx1);i++) {
    xdistsq=cx1-i; xdistsq*=xdistsq;
    for(k=0;k<(int)floor(cz1);k++) {
      zdistsq=cz1-k; zdistsq*=zdistsq;
      if(xdistsq+zdistsq>radsq+OC_SQRT_REAL8_EPSILON)
        m[i][k].SetThickness(0.0);
    }
  }
  // Round lower right corner
  for(i=(int)ceil(cx2);i<Nx;i++) {
    xdistsq=cx2-i; xdistsq*=xdistsq;
    for(k=0;k<(int)floor(cz1);k++) {
      zdistsq=cz1-k; zdistsq*=zdistsq;
      if(xdistsq+zdistsq>radsq+OC_SQRT_REAL8_EPSILON)
        m[i][k].SetThickness(0.0);
    }
  }
  // Round upper right corner
  for(i=(int)ceil(cx2);i<Nx;i++) {
    xdistsq=cx2-i; xdistsq*=xdistsq;
    for(k=(int)floor(cz2);k<Nz;k++) {
      zdistsq=cz2-k; zdistsq*=zdistsq;
      if(xdistsq+zdistsq>radsq+OC_SQRT_REAL8_EPSILON)
        m[i][k].SetThickness(0.0);
    }
  }
  // Round upper left corner
  for(i=0;i<(int)floor(cx1);i++) {
    xdistsq=cx1-i; xdistsq*=xdistsq;
    for(k=(int)floor(cz2);k<Nz;k++) {
      zdistsq=cz2-k; zdistsq*=zdistsq;
      if(xdistsq+zdistsq>radsq+OC_SQRT_REAL8_EPSILON)
        m[i][k].SetThickness(0.0);
    }
  }

  hValid=torqueValid=energyValid=0;
}

void Grid2D::InitPyramid(Nb_DString &inprop)
{ // Simulates a truncated pyramid, with ramp transition base
  // width (overhang) specified by import (in m).
  double base_width=Nb_Atof(inprop.GetStr())/cellsize;

// Set base_height to the minimum height -- the height of the
// base on which rests the truncated pyramid.  For a truncated
// pyramid, this is zero, but set a different compile time constant
// to experiment with truncated pyramids on top of a rectangular base.
//  const double base_height=0.75;
  const double base_height=0.0;

  for(int i=0;i<Nx;i++) {
    int edge_x_dist=OC_MIN(i,Nx-1-i);
    for(int k=0;k<Nz;k++) {
      int edge_z_dist=OC_MIN(k,Nz-1-k);
      double edge_dist=double(OC_MIN(edge_x_dist,edge_z_dist))+0.5;
      double thick=1.0;
      if(edge_dist<base_width) {
	thick = edge_dist/base_width;
	// thick = sqrt(thick);
	thick *= (1.0-base_height);
	thick += base_height;
      }
      m[i][k].SetThickness(static_cast<float>(thick));
    }
  }
  hValid=torqueValid=energyValid=0;
}


void Grid2D::SetBoundaryThickness(double thick)
{ // Sets all boundary magelt's thickness magnitudes to thick
  // Note: This subroutine will likely change which elements
  //       answer "1" to MagElt::IsBoundary().
  int i,k,bdrycount=0;
  MagElt** meptr=new MagElt *[Nx*Nz];
  if(!meptr) 
    PlainError(1,"Error in Grid2D::SetBoundaryThickness(double): %s",
               ErrNoMem);
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    if(m[i][k].GetThickness()>0. && m[i][k].IsBoundary())

      meptr[bdrycount++]=&m[i][k];
  }
  for(i=0;i<bdrycount;i++)
    meptr[i]->SetThickness(static_cast<float>(thick));
  delete[] meptr;
  hValid=torqueValid=energyValid=0;
}

void Grid2D::RoughenBoundaryThickness()
{ // Multiplies each boundary thickness (separately) by an amount
  // drawn from a uniform distribution on [0,1].
  int i,k,bdrycount=0;
  MagElt** meptr=new MagElt *[Nx*Nz];
  if(!meptr) {
    PlainError(1,"Error in Grid2D::RoughenBoundaryThickness(): %s",
               ErrNoMem);
  }
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    if(m[i][k].GetThickness()>0. && m[i][k].IsBoundary())
      meptr[bdrycount++]=&m[i][k];
  }
  for(i=0;i<bdrycount;i++) {
    meptr[i]->SetThickness(static_cast<float>(meptr[i]->GetThickness()*
					                  Oc_UnifRand()));
  }
  delete[] meptr;
  hValid=torqueValid=energyValid=0;
}

void Grid2D::Initialize(MIF &mif,MagEltAnisType new_magelt_anistype)
{
  int i,k;
  double dtemp;

  energy=DBL_MAX;  // Initialize to big number
  exch_energy=anis_energy=demag_energy=zeeman_energy=energy;

  Ms                 = mif.GetMs();
  K1                 = mif.GetK1();
  EdgeK1             = mif.GetEdgeK1();
  if(EdgeK1!=0.0)    DoBdryAnis=1;
  else               DoBdryAnis=0;
#ifndef ANIS_BDRY_ADJUSTMENT
  if(DoBdryAnis) {
    PlainError(1,"Error in Grid2D::Initialize: Edge anisotropy"
               " requested, but not compiled into this build");
  }
#endif // ANIS_BDRY_ADJUSTMENT
  exchange_stiffness = mif.GetA();
  magelt_anistype    = new_magelt_anistype;
  MagElt::SetupConstants(Ms,exchange_stiffness,
                         cellsize,magelt_anistype);
  // Note: cellsize is initialized in Grid2D::Grid2D(MIF) constructor
  // mainline before calling Grid2D::Initialize(...)

  double new_stepsize=mif.GetInitialIncrement();
  StepSize=OC_MAX(OC_REAL8_EPSILON,new_stepsize);
  InitialStepSize=NextStepSize=StepSize0=StepSize;
  step_total=reject_total=reject_energy_count=reject_position_count=0;

  hUpdateCount=0; // Count of calls to hUpdate
  ODEIterCount=0; // ODE history state count
  hValid=torqueValid=energyValid=0;

  // Setup internal demag function, if necessary
  // (see also Grid2D::Allocate())
  if(ExternalDemag->calc==(DF_Calc *)NULL && DoDemag) {
    ADemagCoef[0][0]=CDemagCoef[0][0]=0.0;
#if defined PAPER_COEFS
    SelfDemag= -0.5;
#elif defined SQUARE_COEFS
    SelfDemag= -0.5;
#else // CYL_COEFS
    SelfDemag=(-PI/6);
#endif
    for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
      if(i!=0 || k!=0) {
#if defined PAPER_COEFS // Use MSZ coefficients (APS Vol 49, #22, p15745-15752)
        if((dtemp=OC_SQ(2*i+1)+4*OC_SQ(k))<0)
          PlainError(1,"Error in Grid2D::Initialize: %s","Integer overflow");
        ADemagCoef[i][k]=
          Oc_Atan2(double(4*i-2),double(OC_SQ(2*i-1)+4*OC_SQ(k)-1))
          -Oc_Atan2(double(4*i+2),double(OC_SQ(2*i+1)+4*OC_SQ(k)-1));
        dtemp=4*OC_SQ(OC_SQ(i))+8*OC_SQ(i)*OC_SQ(k)+4*OC_SQ(OC_SQ(k))+1;
        CDemagCoef[i][k]=0.5*log((dtemp+8*i*k)/(dtemp-8*i*k));
        ADemagCoef[i][k]/=2*PI; // I don't know if this is the proper
        CDemagCoef[i][k]/=2*PI; // normalization or not, but it looks good.
#elif defined SQUARE_COEFS
        dtemp=(2*k+1)/sqrt(OC_SQ(2*i-1)+OC_SQ(2*k+1));
        dtemp-=(2*k-1)/sqrt(OC_SQ(2*i-1)+OC_SQ(2*k-1));
        ADemagCoef[i][k]=2*dtemp/(2*i-1);
        dtemp=(2*k+1)/sqrt(OC_SQ(2*i+1)+OC_SQ(2*k+1));
        dtemp-=(2*k-1)/sqrt(OC_SQ(2*i+1)+OC_SQ(2*k-1));
        ADemagCoef[i][k]-=2*dtemp/(2*i+1);
        CDemagCoef[i][k]=2./sqrt(OC_SQ(2*i-1)+OC_SQ(2*k-1))
                        -2./sqrt(OC_SQ(2*i-1)+OC_SQ(2*k+1))
                        -2./sqrt(OC_SQ(2*i+1)+OC_SQ(2*k-1))
                        +2./sqrt(OC_SQ(2*i+1)+OC_SQ(2*k+1));
#else // CYL_COEFS
        if((dtemp=(i*i+k*k))<1)
          PlainError(1,"Error in Grid2D::Initialize: Integer overflow");
        dtemp=2*PI*OC_SQ(dtemp); // Denominator-Area consider. cancel cellsize
        ADemagCoef[i][k]=(i*i-k*k)/dtemp;
        CDemagCoef[i][k]=2.0*(i*k)/dtemp;
#endif // Coefficient selection
      }
    }
  }

  // Set initial H field to 0
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    h[i][k][0]=h[i][k][1]=h[i][k][2]=0.0;
    h0[i][k][0]=h0[i][k][1]=h0[i][k][2]=0.0;
    h1[i][k][0]=h1[i][k][1]=h1[i][k][2]=0.0;
    if(DoDemag) hdemag[i][k][0]=hdemag[i][k][1]=hdemag[i][k][2]=0.0;
  }

  // Set initial torques to 0
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    torque1[i][k][0]=torque1[i][k][1]=torque1[i][k][2]=0.0;
    torque0[i][k][0]=torque0[i][k][1]=torque0[i][k][2]=0.0;
    torque[i][k][0]=torque[i][k][1]=torque[i][k][2]=0.0;
  }

  hValid=torqueValid=energyValid=0;  // Safety;  Should be 0 from above
}

int Grid2D::InitMagnetization()
{ // Note: the maginit functions work in the OOMMF xy+z coordinate
  // system, so after initialization we have to do a coordinate
  // change on the magnetization vectors.
  if(MagInitArgs.argc<1) return 1; // Safety
  MI_Function* InitM=MagInit::LookUp(MagInitArgs.argv[0]);
  if(InitM==NULL) return 2;        // Safety
  (*InitM)(Nx,Nz,m,&(MagInitArgs.argv[1]));
  for(int i=0;i<Nx;i++) for(int k=0;k<Nz;k++) {
    ConvertXyzToXzy(m[i][k]);
    m[i][k].Scale(1.0); // Protect against poorly designed
    /// maginit functions.
  }
  return 0;
}

void Grid2D::Reset()
{ // The calling routine is responsible for resetting hApplied
  if(randomizer_seed==0) Oc_Srand();
  else                   Oc_Srand(randomizer_seed);
  InitMagnetization();
  StepSize0=StepSize=NextStepSize=InitialStepSize;
  ResetODE();
}

Grid2D::Grid2D(MIF mif)
{ // Error message handling allows the event loop to be re-entered,
  // with the danger that some code outside of this constructor may
  // delete the mif instance.  To provide some protection, we bring
  // the mif data in by value instead of by reference.

  STOPWATCH_START(sw_proc_total);
  if(!mif.IsValid()) {
    PlainError(1,"Error in Grid2D::Grid2D: Invalid mif object.");
  }
  dfparams=(char **)NULL; // Safety;
  randomizer_seed=mif.GetRandomizerSeed();
  if(randomizer_seed==0) Oc_Srand();
  else                   Oc_Srand(randomizer_seed);

  int xcount,zcount,i,k;
  MagEltAnisType new_magelt_anistype=meat_uniaxial; // Safety
  thickness_sum=1.0;  // Safety

  // Get basic info
  if((cellsize=mif.GetCellSize())<=0.0)
    PlainError(1,"Error in Grid2D::Grid2D(MIF &): "
             "Illegal cell size (%g meters)",cellsize);
  xcount=(int)OC_ROUND(mif.GetPartWidth()/cellsize);
  zcount=(int)OC_ROUND(mif.GetPartHeight()/cellsize);
  xmax=xcount*cellsize;
  zmax=zcount*cellsize;
  ymax=mif.GetPartThickness();
  DoPrecess=mif.GetDoPrecess();
  GyRatio=mif.GetGyRatio();
  DampCoef=mif.GetDampCoef();
  if(DampCoef==0.0) PlainError(1,"Error in Grid2D::Grid2D(MIF &): "
                               "DampCoef must be non-zero.");

  // Do demag initialization
  if((dfparams=new char*[OC_MAX(2,MAXDFPARAMS)])==0
     || (dfparams[0]=new char[MAXDFNAMELENGTH])==0)
    PlainError(1,"Error in Grid2D::Grid2D(MIF &): %s",ErrNoMem);
  strncpy(dfparams[0],mif.GetDemagName(),MAXDFNAMELENGTH);
  *(dfparams[1]=(char *)strchr((const char *)dfparams[0],'\0')+1)='\0';
  if(DF_LookUpName(dfparams[0],&ExternalDemag)!=0) // No match!
    PlainError(1,"Error in Grid2D::Grid2D(MIF &): "
             "No demag calculation routine by the name ->%s<- found",
             dfparams[0]);
  if(Nb_StrCaseCmp(ExternalDemag->name,"NoDemag")==0) DoDemag=0;
  else                                                 DoDemag=1;
  if(ExternalDemag->paramcount<1) ExternalDemag->params=(char**)NULL;
  else {
    ExternalDemag->params=dfparams;
    if(ExternalDemag->paramcount>1)
      sprintf(dfparams[1],"%.17g",mif.GetPartThickness()/cellsize);
  }

  // Determine magelt_anistype and anisotropy directions
#if (VECDIM != MIFVECDIM)
#error VECDIM and MIFVECDIM must be equal
#endif
  switch(mif.GetAnisType())
    {
    case mif_uniaxial: new_magelt_anistype=meat_uniaxial; break;
    case mif_cubic:    new_magelt_anistype=meat_gencubic; break;
    default: PlainError(1,"Error in Grid2D::Grid2D(MIF &): "
                        "Unknown Anisotropy Type in MIF");
             break;
    }
  double dirA_arr[VECDIM];  ThreeVector dirA(dirA_arr);
  double dirB_arr[VECDIM];  ThreeVector dirB(dirB_arr);
  double dirC_arr[VECDIM];  ThreeVector dirC(dirC_arr);
  Convert(dirA,mif.GetAnisDir(1));
  ConvertXyzToXzy(dirA);  // Convert to internal coords
  if(fabs(dirA.MagSq()-1)>8*sqrt(OC_REAL8_EPSILON))
    PlainError(1,"Error in Grid2D::Grid2D(MIF &):"
               " Non-unit anisotropy direction (1).");
  if(new_magelt_anistype!=meat_uniaxial) { // Some cubic type
    Convert(dirB,mif.GetAnisDir(2));
    ConvertXyzToXzy(dirB);  // Convert to internal coords
    if(fabs(dirB.MagSq()-1)>8*sqrt(OC_REAL8_EPSILON))
      PlainError(1,"Error in Grid2D::Grid2D(MIF &):"
                 " Non-unit anisotropy direction (2).");
    double dot=dirA*dirB;
    if(fabs(dot)>8*sqrt(OC_REAL8_EPSILON))
      PlainError(1,"Error in Grid2D::Grid2D(MIF &):"
                 " Crystal axes not perpendicular.");
    dirC=dirA^dirB;
    if(fabs(dirC.MagSq()-1)>8*sqrt(OC_REAL8_EPSILON))
      PlainError(1,"Error in Grid2D::Grid2D(MIF &):"
                 " Non-unit anisotropy direction (3).");
    if(dirA[0]==1.0 && dirB[1]==1.0)
      new_magelt_anistype=meat_cubic;  // Standard cubic anisotropy
  }

  // Allocate arrays
  Allocate(xcount,zcount);

  // Initialize program constants
  Initialize(mif,new_magelt_anistype);

  // Setup applied field.  The SetCoords() call must be done
  // *after* Nx & Nz are setup, which is currently done in Allocate.
  // Perhaps the SetCoords() call should be moved there as well?
  ArgLine zargs=mif.GetZeemanArgs();
  if(zargs.argc<1) {
    // Applied field not specified.  Default to uniform field.
    const char *temp[1];
    temp[0]="uniform";
    hApplied=Zeeman::MakeZeeman(1,(const char **)temp);
  } else {
    hApplied=Zeeman::MakeZeeman(zargs.argc,(const char **)zargs.argv);
  }
  hApplied->SetCoords(this,Nx,Nz);
  //  hApplied->SetNomFieldVec3D(Ms,Vec3D(0.,0.,0.),0); // Init to 0 field./**/
  /// 0 field initialization *should* be unnecessary!                      /**/

  // Setup anisotropy axes inside MagElt's.  Anisotropy arrays should
  // be unchanging, so they can be shared across all the magelt arrays.
  ANIS_INIT_FUNC* aifp=AnisInitList.GetAnisInitFunc(mif.GetAnisInitName());
  if(aifp==NULL)
    PlainError(1,"Error in Grid2D::Grid2D(MIF &): "
               "Unknown AnisInitFunc: ->%s<-",mif.GetAnisInitName());
  ArgLine ai=mif.GetAnisInitArgs();
  (*aifp)(ai.argc,(const char* const*)ai.argv,  // Cast to keep VC++ happy
          Nx,Nz,magelt_anistype,K1,dirA,dirB,dirC,
          m,anisa,anisb,anisc);
  double *aptr=NULL,*bptr=NULL,*cptr=NULL;
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    m[i][k].GetAnisDirs(aptr,bptr,cptr);   // Copy anis pointers from m
    m0[i][k].QuickInitAnisDirs(aptr,bptr,cptr); // to m0,
    m1[i][k].QuickInitAnisDirs(aptr,bptr,cptr); // to m1,
    m2[i][k].QuickInitAnisDirs(aptr,bptr,cptr); // and to m2,
  }

  // Setup sample geometry.  NOTE: We need to setup the geometry (by
  //  setting some magelt thickness to 0) _before_ calling the neighborhood
  //  constructor routine, because the latter needs to know where the
  //  boundaries are in order to get proper boundary conditions.
  // NOTE 2: These geometry initialization routines must not
  //  use the m->ngbr arrays (obviously).
  MifGeometryType mgt; Nb_DString geomparam;
  mgt=mif.GetGeometry(geomparam);
  if(mgt==mif_rectangle)      InitRectangle(geomparam);
  else if(mgt==mif_ellipse)   InitEllipse(geomparam);
  else if(mgt==mif_ellipsoid) InitEllipsoid(geomparam);
  else if(mgt==mif_oval)      InitOval(geomparam);
  else if(mgt==mif_pyramid)   InitPyramid(geomparam);
  else if(mgt==mif_mask)      Mask(geomparam);
  else PlainError(1,"Error in Grid2D::Grid2D(MIF &):"
		  " Part geometry not specified.");

  // Once thickness is set up, determine Ny_corrections
#ifndef BAD_LOCAL_DEMAG_HACK
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    float thick = m[i][k].GetThickness();
    if(thick==1.0 || thick<=0.0) {
      // Base calculation is proper; no correction necessary
      m[i][k].SetNyCorrection(0.0);
    }
    else {
      m[i][k].SetNyCorrection(1 - thick);
      // With this correction, Nx + Ny + Nz = 1 for the cell,
      // and for any uniformly magnetized body.
    }
  }
#else // BAD_LOCAL_DEMAG_HACK
  // Setup thickness modification for both Ny (out-of-plane) and Nx/Nz
  // (in-plane) cell self-demag factors.  This sets the proper cellwise
  // local demag, but is *not* balanced with global demag field.
  // This hack is *not* recommended.
  double cell_aspect = mif.GetPartThickness()/cellsize;
  double base_Ny = SelfDemagNy(1.0,cell_aspect,1.0);
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    float thick = m[i][k].GetThickness();
    if(thick==1.0 || thick<=0.0) {
      // Base calculation is proper; no correction necessary
      m[i][k].SetNyCorrection(0.0);
    }
    else {
      double Ny = SelfDemagNy(1.0,thick*cell_aspect,1.0);
      m[i][k].SetNyCorrection(Ny - base_Ny*thick);
      // NOTE: Corresponding Nx and Nz correction term (in-plane)
      // is  0.5 * (1 - thick - Ny_correction).
    }
  }
#endif // BAD_LOCAL_DEMAG_HACK

  // Initialize m and copy data (vector, thickness, Dy_correction and K1)
  // into m0, m1, and m2.  Note that _after_ this all magelt's have their
  // thickness magnitude set, so one may then set up the nbhd structure.
  //  Also, the maginit functions work in the OOMMF xy+z coordinate
  // system, so after initialization we have to do a coordinate
  // change on the magnetization vectors.
  MagInitArgs=mif.GetMagInitArgs();
  InitMagnetization();
  thickness_sum=0.0;
  for(i=0;i<Nx;i++) {
    double thickness_column_sum=0.0;
    for(k=0;k<Nz;k++) {
      thickness_column_sum+=fabs(m[i][k].GetThickness());
      m0[i][k].CopyData(m[i][k]);
      m1[i][k].CopyData(m[i][k]);
      m2[i][k].CopyData(m[i][k]);
      energy_density[i][k]=energy_density1[i][k]=0.0;
    }
    thickness_sum+=thickness_column_sum;
  }
  if(thickness_sum<OC_REAL8_EPSILON) thickness_sum=OC_REAL8_EPSILON; // Safety

  // Set up neighbor structure in m and m0, m1, m2
  int new_ngbrcnt;
  int maxmagngbrs=MagElt::GetMaxNgbrcnt();
  MagEltLink* Neighbors = new MagEltLink[maxmagngbrs];

  if(maxmagngbrs!=4 && maxmagngbrs!=8)
   PlainError(1,"Error in Grid2D::Grid2D(MIF &): "
            "Current code only supports 4 and 8 neighborhoods");

  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    new_ngbrcnt=FindNeighbors(maxmagngbrs,m,Neighbors,i,k);
    m[i][k].SetupNeighbors(new_ngbrcnt,Neighbors);
    new_ngbrcnt=FindNeighbors(maxmagngbrs,m0,Neighbors,i,k);
    m0[i][k].SetupNeighbors(new_ngbrcnt,Neighbors);
    new_ngbrcnt=FindNeighbors(maxmagngbrs,m1,Neighbors,i,k);
    m1[i][k].SetupNeighbors(new_ngbrcnt,Neighbors);
    new_ngbrcnt=FindNeighbors(maxmagngbrs,m2,Neighbors,i,k);
    m2[i][k].SetupNeighbors(new_ngbrcnt,Neighbors);
  }
  delete[] Neighbors;

#ifdef BDRY_CODE
  // Setup grid bdry list.  The #ifdef blocks are a little messy,
  // but we don't want to waste time building the boundary list
  // if we aren't going to use it.
# ifdef DEMAG_EDGE_CORRECTION
  FillBdryList();
# else
  if(DoBdryAnis) FillBdryList();
# endif
#endif // BDRY_CODE

  // Additional boundary adjustments, which use m->ngbr arrays.
#ifdef WEAK_BOUNDARY
  if(WEAK_BOUNDARY>0.0) SetBoundaryThickness(WEAK_BOUNDARY);
#endif // WEAK_BOUNDARY
#ifdef ROUGH_BOUNDARY
  RoughenBoundaryThickness();
#endif // ROUGH_BOUNDARY
}

void Grid2D::Deallocate()
{
  int i;
  if(ExternalDemag->calc!=NULL) (*ExternalDemag->destroy)();
  ThreeVector::Release2DArray(h);
  ThreeVector::Release2DArray(h0);
  ThreeVector::Release2DArray(h1);
  ThreeVector::Release2DArray(anisa);
  ThreeVector::Release2DArray(anisb);
  ThreeVector::Release2DArray(anisc);
  if(DoDemag) ThreeVector::Release2DArray(hdemag);
  ThreeVector::Release2DArray(torque1);
  ThreeVector::Release2DArray(torque0);
  ThreeVector::Release2DArray(torque);
  m.Deallocate(); m0.Deallocate(); m1.Deallocate(); m2.Deallocate();
  energy_density.Deallocate(); energy_density1.Deallocate();
  if(ExternalDemag->calc==NULL && DoDemag) {
    for(i=0;i<Nx;i++) {
      delete[] ADemagCoef[i]; delete[] CDemagCoef[i];
    }
    delete[] ADemagCoef; delete[] CDemagCoef;
  }
  if(dfparams!=NULL) { delete dfparams[0]; delete dfparams; }
}

Grid2D::~Grid2D()
{
  delete hApplied;
  Deallocate();
}

#ifdef STOPWATCH_ON
void Grid2D::PrintUsageTimes(FILE *fptr)
{
  Oc_TimeVal cpu,wall;
  fprintf(fptr,"# Timing (seconds)     CPU       Elapsed\n");
  sw_exch.GetTimes(cpu,wall);
  fprintf(fptr,"#  Exchange    %12.3f %12.3f\n",
          double(cpu),double(wall));
  sw_anis.GetTimes(cpu,wall);
  fprintf(fptr,"#  Anisotropy  %12.3f %12.3f\n",
          double(cpu),double(wall));
  sw_demag.GetTimes(cpu,wall);
  fprintf(fptr,"#  Demag       %12.3f %12.3f\n",
          double(cpu),double(wall));
  sw_zeeman.GetTimes(cpu,wall);
  fprintf(fptr,"#  Zeeman      %12.3f %12.3f\n",
          double(cpu),double(wall));
  sw_solve_total.GetTimes(cpu,wall);
  fprintf(fptr,"#  Solve Total %12.3f %12.3f\n",
          double(cpu),double(wall));
  sw_proc_total.GetTimes(cpu,wall);
  fprintf(fptr,"#  Proc  Total %12.3f %12.3f\n",
          double(cpu),double(wall));
  fflush(fptr);
}
#else
void Grid2D::PrintUsageTimes(FILE *) {}
#endif // STOPWATCH_ON

void Grid2D::GetUsageTimes(Nb_DString& str)
{
  str="";
#ifdef STOPWATCH_ON
  char buf[512];
  Oc_TimeVal cpu,wall;

  sprintf(buf,"Timing (seconds)     CPU       Elapsed\n");
  str.Append(buf,sizeof(buf));

  sw_exch.GetTimes(cpu,wall);
  sprintf(buf," Exchange    %12.3f %12.3f\n",
          double(cpu),double(wall));
  str.Append(buf,sizeof(buf));

  sw_anis.GetTimes(cpu,wall);
  sprintf(buf," Anisotropy  %12.3f %12.3f\n",
          double(cpu),double(wall));
  str.Append(buf,sizeof(buf));

  sw_demag.GetTimes(cpu,wall);
  sprintf(buf," Demag       %12.3f %12.3f\n",
          double(cpu),double(wall));
  str.Append(buf,sizeof(buf));

  sw_zeeman.GetTimes(cpu,wall);
  sprintf(buf," Zeeman      %12.3f %12.3f\n",
          double(cpu),double(wall));
  str.Append(buf,sizeof(buf));

  sw_solve_total.GetTimes(cpu,wall);
  sprintf(buf," Solve Total %12.3f %12.3f\n",
          double(cpu),double(wall));
  str.Append(buf,sizeof(buf));

  sw_proc_total.GetTimes(cpu,wall);
  sprintf(buf," Proc  Total %12.3f %12.3f",
          double(cpu),double(wall));
  str.Append(buf,sizeof(buf));
#endif // STOPWATCH_ON
}

void Grid2D::Perturb(double max_mag)
{ // Randomly perturb all m by <magnitude
  for(int i=0;i<Nx;i++) for(int k=0;k<Nz;k++) {
    m[i][k].Perturb(max_mag);
  }
  hValid=torqueValid=energyValid=0;
}

// The next two functions (ADemag() & CDemag()) access the demagcoef
// matrices using symmetries to allow negative values for i and k.
double Grid2D::ADemag(int i,int k)
{
  return ADemagCoef[(i>=0?i:-i)][(k>=0?k:-k)];
}

double Grid2D::CDemag(int i,int k)
{
  int sign=1;
  if(i<0) { i*=-1; sign=-1; }
  if(k<0) { k*=-1; sign*=-1; }
  return  sign*CDemagCoef[i][k];
}

/* Calculate the demag field at (i,k) including the self-field */
ThreeVector& Grid2D::CalculateDemag(int i,int k)
{
  static double hdarr[3];
  static ThreeVector hd(hdarr);
  if(!DoDemag) return hd;

  hd[0]=hd[1]=hd[2]=0.0;

  double A,C,mx,mz;
  double mot;
  int i2,k2;
  for(i2=0;i2<Nx;i2++) for(k2=0;k2<Nz;k2++) {
    A=ADemag(i2-i,k2-k); C=CDemag(i2-i,k2-k);
    mx=m[i2][k2][0];     mz=m[i2][k2][2];
    hd[0]+=(mot=m[i2][k2].GetThickness())*(A*mx+C*mz);
    hd[2]+=mot*(C*mx-A*mz);
  }
  mot=m[i][k].GetThickness();
  hd[0]+=mot*SelfDemag*m[i][k][0]; // Self-demag
  hd[2]+=mot*SelfDemag*m[i][k][2];
  return hd;
} 

/* Calculate the average energy density/(mu_0*Ms^2). */
void Grid2D::CalculateEnergy()
{
  Nb_Xpfloat exch_bucket,anis_bucket,demag_bucket,zeeman_bucket;

  int i,k;
  MagElt* mptr;
  double temp;

  if(!hValid) hUpdate();

  exch_energy=anis_energy=demag_energy=zeeman_energy=0.0;
  exch_bucket=anis_bucket=demag_bucket=zeeman_bucket=0.0;

  for(i=0;i<Nx;i++) {
    STOPWATCH_START(sw_exch);
    for(k=0,mptr=m[i];k<Nz;k++,mptr++) {
      exch_bucket.Accum((temp=mptr->CalculateExchangeEnergy()));
      energy_density[i][k]=temp; // Initialize with exchange energy
    }
    STOPWATCH_STOP(sw_exch);
    STOPWATCH_START(sw_anis);
    for(k=0,mptr=m[i];k<Nz;k++,mptr++) {
      anis_bucket.Accum((temp=(mptr->*MagElt::AnisotropyEnergy)()));
      energy_density[i][k]+=temp; // Increment with all other energies
    }
    STOPWATCH_STOP(sw_anis);
    STOPWATCH_START(sw_demag);
    if(DoDemag) {
      for(k=0,mptr=m[i];k<Nz;k++,mptr++) {
	double thick=mptr->GetThickness();
	if(thick>0.0) {
	  temp = -0.5 * thick*((*mptr)*hdemag[i][k]);
	  demag_bucket.Accum(temp);
	  energy_density[i][k]+=temp;
	}
      }
    }
    STOPWATCH_STOP(sw_demag);
    STOPWATCH_START(sw_zeeman);
    double happarr[VECDIM]; ThreeVector happ(happarr);
    for(k=0,mptr=m[i];k<Nz;k++,mptr++) {
      hApplied->GetLocalhField(i,k,happ);
      ConvertXyzToXzy(happ);  // Coords change
      temp= -1*mptr->GetThickness()*((*mptr)*happ);
      zeeman_bucket.Accum(temp);
      energy_density[i][k]+=temp;
    }
    STOPWATCH_STOP(sw_zeeman);
  }

#ifdef ANIS_BDRY_ADJUSTMENT
  STOPWATCH_START(sw_anis);
  // Add surface anistropy
  if(DoBdryAnis) {
    for(BdryElt *bep=bdry.GetFirst();bep!=NULL;bep=bdry.GetNext()) {
      double acoef=bep->anisotropy_coef;
      if(acoef!=0.0) {
        int bi=bep->i;
        int bk=bep->k;
        double bdot = bep->normal_dir * m[bi][bk];
        if(acoef<=0.0) temp = -acoef*bdot*bdot;
        else           temp =  acoef*(1-bdot*bdot);
	temp *= m[bi][bk].GetThickness();
        anis_bucket.Accum(temp); // Include as part of standard
        /// anisotropy energy.
        energy_density[bi][bk] += temp;
      }
    }
  }
  STOPWATCH_STOP(sw_anis);
#endif // ANIS_BDRY_ADJUSTMENT

  exch_energy=exch_bucket.GetValue();
  anis_energy=anis_bucket.GetValue();
  demag_energy=demag_bucket.GetValue();
  zeeman_energy=zeeman_bucket.GetValue();

#ifdef DEMAG_EDGE_CORRECTION
  fprintf(stderr,"Original  demag energy=%g\n",demag_energy); /**/
  // Demag boundary correction
  demag_energy+=GetDemagBdryCorrection();                     /**/
  fprintf(stderr,"Corrected demag energy=%g\n",demag_energy); /**/
#endif // DEMAG_EDGE_CORRECTION

  // Total energy density---
  Nb_Xpfloat esum(exch_energy);
  esum.Accum(anis_energy);
  esum.Accum(demag_energy);
  esum.Accum(zeeman_energy);
  energy = esum.GetValue();

  // Normalize energies
  double mult=1.0/thickness_sum;
  exch_energy*=mult;
  anis_energy*=mult;
  demag_energy*=mult;
  zeeman_energy*=mult;
  energy*=mult;

#ifdef DEMAG_EDGE_CORRECTION
  DumpDemagEnergyPpm("EnergyGrid.ppm"); /**/
#endif // DEMAG_EDGE_CORRECTION

#ifdef ALTERNATE_ENERGY_CALCULATION
  // Here's another way to calculate (supposedly) the same thing:
  double energy2=0.;
  for(i=0;i<Nx;i++) for(k=0,mptr=m[i];k<Nz;k++,mptr++) {
    energy2 += mptr->GetThickness() * ((*mptr)*h[i][k]);
  }
  energy2 /= -thickness_sum;
  energy2 -= demag_energy;  // Demag energy double counted
  energy=energy2;
#endif // ALTERNATE_ENERGY_CALCULATION

  energyValid=1;
}

double Grid2D::CalculateEnergyDifference()
{ // Calculates the energy change across energy_density1-energy_density.
  // Presumably this has higher accuracy than calculating each
  // energy separately and subtracting.
  Nb_Xpfloat change_bucket;
  for(int i=0;i<Nx;i++) for(int k=0;k<Nz;k++) {
    change_bucket.Accum(energy_density1[i][k]-energy_density[i][k]);
  }
  return change_bucket.GetValue()/thickness_sum;
}

void Grid2D::DumpDemagEnergyPpm(const char* filename)
{ // Debug facility: Dumps out Demag energy density as a function of
  // position to a 'P3' PPM file.  Red is negative energy, blue is
  // positive.

  if(!DoDemag) return;
  FILE* fptr=Nb_FOpen(filename,"w");  if(fptr==NULL) return;
  Message("Writing energy density to PPM file %s",filename);

  int i,k;
  double value,minvalue,maxvalue;
  value = -0.5 * m[0][0].GetThickness() * (m[0][0]*hdemag[0][0]);
  minvalue=maxvalue=value;
  for(i=0;i<Nx;i++) {
    for(k=0;k<Nz;k++) {
      value = -0.5 * m[i][k].GetThickness() * (m[i][k]*hdemag[i][k]);
      if(value<minvalue) minvalue=value;
      if(value>maxvalue) maxvalue=value;
    }
  }
  double scale=255.0/OC_MAX(maxvalue,-minvalue);

  // Write header
  fprintf(fptr,"P3\n%d %d\n255\n",Nx,Nz);

  // Write pixels, from top to bottom
  for(k=Nz-1;k>=0;k--) {
    for(i=0;i<Nx;i++) {
      value = -0.5 * m[i][k].GetThickness() * (m[i][k]*hdemag[i][k]);
      const double contrast(1.);
      int pix=(int)OC_ROUND(value*scale*contrast);
      if(pix>255)  pix=255;
      if(pix<-255) pix=-255;  // Safety
      if(value>=0.) fprintf(fptr,"%d %d 255\n",255-pix,255-pix);
      else          fprintf(fptr,"255 %d %d\n",255+pix,255+pix);
    }
  }
  fclose(fptr);
}

void Grid2D::InternalDemagCalc()
{
#ifdef CODE_CHECKS
  if(!DoDemag)
    PlainError(1,"Error in Grid2D::InternalDemagCalc: "
             "This routine shouldn't be called because DoDemag is false");
#endif
  for(int i=0;i<Nx;i++) for(int k=0;k<Nz;k++)
    hdemag[i][k]=CalculateDemag(i,k);
}

void Grid2D::hUpdate()
{ // NOTE: Exchange and anisotropy fields are excluded
  //       in 0-thickness cells.
#ifdef CODE_CHECKS
  if(hValid) Message("Warning in Grid2D::hUpdate: "
                     "Updating h even though hValid is TRUE.");
#endif // CODE_CHECKS
  int i,k;
  hUpdateCount++;

  STOPWATCH_START(sw_zeeman);
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    hApplied->GetLocalhField(i,k,h[i][k]);
    ConvertXyzToXzy(h[i][k]);
  }
  STOPWATCH_STOP(sw_zeeman);

  STOPWATCH_START(sw_exch);
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    // Exclude exchange field from 0-thickness sites
    if(m[i][k].GetThickness()!=0.0)
      h[i][k]+=m[i][k].CalculateExchange();
  }
  STOPWATCH_STOP(sw_exch);

  STOPWATCH_START(sw_anis);
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    // Exclude anisotropy field from 0-thickness sites
    if(m[i][k].GetThickness()!=0.0)
      h[i][k]+=(m[i][k].*MagElt::AnisotropyField)();
  }
  STOPWATCH_STOP(sw_anis);

#ifdef ANIS_BDRY_ADJUSTMENT
  // Include surface anistropy.  All cells in list "bdry"
  // have thickness!=0.0.
  STOPWATCH_START(sw_anis);
  if(DoBdryAnis) {
    for(BdryElt *bep=bdry.GetFirst();bep!=NULL;bep=bdry.GetNext()) {
      if(bep->anisotropy_coef!=0.0) {
        int bi=bep->i;
        int bk=bep->k;
        double bdot = bep->normal_dir * m[bi][bk];
        double mag = 2 * bep->anisotropy_coef * bdot ;
        h[bep->i][bep->k].Accum(mag,bep->normal_dir);
      }
    }
  }
  STOPWATCH_STOP(sw_anis);
#endif // ANIS_BDRY_ADJUSTMENT

  if(DoDemag) {
    STOPWATCH_START(sw_demag);
    if(ExternalDemag->calc!=NULL) // Use external demag routine
      (*ExternalDemag->calc)(WriteGrid2Dm,FillGrid2Dh,m,hdemag);
    else // Use internal explicit (brute-force) demag calculation
      InternalDemagCalc();
    for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
      double thick=m[i][k].GetThickness();
      if(thick!=1.0 && thick>0.0) {
        // Make thickness adjustment to self-demag
        double Ny_correction = m[i][k].GetNyCorrection();
        hdemag[i][k][1] -= m[i][k][1]*Ny_correction;
#ifdef BAD_LOCAL_DEMAG_HACK
	// Modify Nx & Nz demag factors too.  This cellwise correction
	// is not balanced against global terms, and is *not*
	// recommended.
        double Nx_Nz_correction = 0.5 * (1 - thick - Ny_correction);
        hdemag[i][k][0] -= m[i][k][0]*Nx_Nz_correction;
        hdemag[i][k][2] -= m[i][k][2]*Nx_Nz_correction;
#endif // BAD_LOCAL_DEMAG_HACK
      }
      h[i][k]+=hdemag[i][k];
    }
    STOPWATCH_STOP(sw_demag);
  }

  hValid=1;
  torqueValid=energyValid=0;
}

void Grid2D::hFastUpdate()
{ // NOTE: Exchange and anisotropy fields are excluded
  //       in 0-thickness cells.
  int i,k;

  STOPWATCH_START(sw_zeeman);
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    hApplied->GetLocalhField(i,k,h[i][k]);
    ConvertXyzToXzy(h[i][k]);
  }
  STOPWATCH_STOP(sw_zeeman);

  STOPWATCH_START(sw_exch);
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    // Exclude exchange field from 0-thickness sites
    if(m[i][k].GetThickness()!=0.0)
      h[i][k]+=m[i][k].CalculateExchange();
  }
  STOPWATCH_STOP(sw_exch);

  STOPWATCH_START(sw_anis);
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    // Exclude anisotropy field from 0-thickness sites
    if(m[i][k].GetThickness()!=0.0)
      h[i][k]+=(m[i][k].*MagElt::AnisotropyField)();
  }
  STOPWATCH_STOP(sw_anis);

#ifdef ANIS_BDRY_ADJUSTMENT
  STOPWATCH_START(sw_anis);
  // Include surface anistropy.  All cells in list "bdry"
  // have thickness!=0.0.
  if(DoBdryAnis) {
    for(BdryElt *bep=bdry.GetFirst();bep!=NULL;bep=bdry.GetNext()) {
      if(bep->anisotropy_coef!=0.0) {
        int bi=bep->i;
        int bk=bep->k;
        double bdot = bep->normal_dir * m[bi][bk];
        double mag = 2 * bep->anisotropy_coef * bdot ;
        h[bep->i][bep->k].Accum(mag,bep->normal_dir);
      }
    }
  }
  STOPWATCH_STOP(sw_anis);
#endif // ANIS_BDRY_ADJUSTMENT

  if(DoDemag) {
    for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) h[i][k]+=hdemag[i][k];
  }                                           // Use old demag
  torqueValid=energyValid=0;
}

void Grid2D::CalculateTorque(Nb_Array2D<MagElt> &m_in,
                               ThreeVector** &h_in,
                               ThreeVector** &t_out)
{ // Calculates "torque" (actually (-1/DampCoef)(mxh)-mx(mxh); see
  // "BIG NOTE" at top of this file) from imports m_in & h_in, storing
  // result in t_out.
  static double vtemparr[3];  static ThreeVector vtemp(vtemparr);
  int i,k;
  MagElt *mp;
  ThreeVector *hp;
  ThreeVector *tp;

  if(DoPrecess) {
    for(i=0;i<Nx;i++) {
      hp=h_in[i]; tp=t_out[i]; mp=m_in[i];
      for(k=0;k<Nz;k++,hp++,tp++,mp++) {
        if(mp->GetThickness()==0.0) tp->Set(0.,0.,0.);
        else {
          vtemp  =  *mp;
          vtemp ^=  *hp;
          (*tp) = vtemp;
          (*tp) *= PRECESSION_RATIO;  // Precession term
          vtemp ^=  *mp;
          (*tp) += vtemp;             // Damping
        }
      }
    }
  } else {
    for(i=0;i<Nx;i++) {
      hp=h_in[i]; tp=t_out[i]; mp=m_in[i];
      for(k=0;k<Nz;k++,hp++,tp++,mp++) {
        if(mp->GetThickness()==0.0) tp->Set(0.,0.,0.);
        else {
          vtemp  =  *mp;
          vtemp ^=  *hp;         // -Precession term
          vtemp ^=  *mp;         // Damping
          (*tp)  = vtemp;
        }
      }
    }
  }
  // Note: It is also possible to calculate the damping term
  //  -mx(mxh) via  h-(h*m)m, but the former can be several
  //  orders of magnitude more accurate...
}

double Grid2D::GetMaxTorque()
{ // Returns maximum modulus of "torque," which
  // is defined to be (-1/DampCoef)*(mxh) - mx(mxh).
  // (If DoPrecess is false, then DampCoef is effectively
  // infinite.) This routine updates the h and torque
  // arrays as necessary.
  int i,k;
  double dtemp,maxtorquesq=0.0;
  if(!hValid) hUpdate();
  if(!torqueValid) { CalculateTorque(m,h,torque); torqueValid=1; }
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    if((dtemp=torque[i][k].MagSq())>maxtorquesq) maxtorquesq=dtemp;
  }
  return sqrt(maxtorquesq);
}

double Grid2D::GetMxH(double torque_)
{
  double mxh=torque_;
  if(DoPrecess) {
    mxh/=sqrt(1.+1./(DampCoef*DampCoef));
  }
  return mxh;
}

double Grid2D::GetMaxMxH()
{
  return GetMxH(GetMaxTorque());
}

double Grid2D::GetdEdt()
{ // Returns  - \sum (thickness.(torque*h))/thickness_sum
  int i,k;
  Nb_Xpfloat bucket;
  double temp;
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    double thick=m[i][k].GetThickness();
    if(thick>0.0) {
      temp=torque[i][k][0]*h[i][k][0]
         + torque[i][k][1]*h[i][k][1]
         + torque[i][k][2]*h[i][k][2];
      temp *= thick;
      bucket.Accum(temp);
    }
  }
  return -bucket.GetValue()/thickness_sum;
  // NB: If this value is positive, then there are round-off problems.
}

void Grid2D::StepEuler(double minstep,double maxtorque,double &next_stepsize)
{ // Takes one Euler step, with stepsize control based on energy.  Assumes
  // m, h, torque & energy are current coming in, and leaves them current
  // going out.  Saves entry values of m, h, and torque in m0, h0, and
  // torque0.
  //   The attempted step size is determined by the value of *this->StepSize
  // on entry.  StepSize may be adjusted by this routine; the value of
  // StepSize on exit is the step size actually used.  The export
  // next_stepsize should be used by the calling routine (on the next pass)
  // to set *this->StepSize _before_ calling this routine.
  //   The import minstep is ignored on the initial pass, but StepSize
  // will not be reduced by this routine to below minstep.  If a valid
  // step is not found, StepSize is set to 0.

  const double CutRatio=0.5;        // Determined empirically
  const double IncreaseRatio=1.1;   //          ""
  // NOTE: I've tried more sophisticated stepsize correction, but this
  //       simple scheme seems to work as well as anything else.
  ThreeVector** htemp; //  Temp. pointer used for swapping h and torque
  double orig_energy,orig_stepsize=StepSize;
  double dEdt_base=GetdEdt();
  double predicted_change,actual_change;  // Change in energy
  double error=0.;
  int i,k;

  // Save current m, h and energy
  m.Swap(m0);
  htemp=h; h=h0; h0=htemp;
  orig_energy=energy;

  while(1) {
    step_total++;
    predicted_change=dEdt_base*StepSize;
    hValid=0;
    for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
      m[i][k].FastAdd(1.-OC_SQ(StepSize)*torque[i][k].MagSq()/2.,
		      m0[i][k],
                      StepSize,torque[i][k]);
      m[i][k].Scale(1);
    }
    hUpdate();  // m, h now hold new position
    CalculateEnergy();
    actual_change=energy-orig_energy;
    error=fabs(actual_change-predicted_change);
    if(actual_change<OC_REAL8_EPSILON && error<(AllowedSpinError*Nx)*Nz)
      break; // Good step

    // Collect step reject statistics
    reject_total++;
    if(actual_change>=OC_REAL8_EPSILON)    reject_energy_count++;
    if(error>=(AllowedSpinError*Nx)*Nz) reject_position_count++;

    // Bad Step
    if(StepSize<=minstep || ((StepSize*CutRatio)*maxtorque)<OC_REAL8_EPSILON) {
      // Step too small
      SendSmallStepMessage("Grid2D::Euler",StepSize,maxtorque);
      StepSize=0; next_stepsize=orig_stepsize;
      m.Swap(m0); htemp=h; h=h0; h0=htemp; // Undo changes
      energy=orig_energy;
      return;
    }
    StepSize*=CutRatio;
    if(StepSize<minstep) StepSize=minstep;
  }
  next_stepsize=StepSize*(1.+(IncreaseRatio-1.)*(1.-error));

  htemp=torque; torque=torque0; torque0=htemp; // Save old torque and
  CalculateTorque(m,h,torque);                 // calculate new torque
  torqueValid=1;
}

void Grid2D::StepPredict2(double minstep,double maxtorque,
                          double &next_stepsize)
{ // Takes one step using a 2nd order predictor, with stepsize
  // control.  Assumes m, h, torque & energy are current coming in, and
  // leaves them current going out.  Saves entry values of m, h, torque
  // and energy_density in m0, h0, and torque0. m1, h1, torque1 and
  // energy_density1 are used for temporary workspace.
  //   The attempted step size is determined by the value of
  // *this->StepSize on entry.  StepSize may be adjusted by this routine;
  // the value of StepSize on exit is the step size actually used.
  // The export next_stepsize should be used by the calling routine (on
  // the next pass) to set *this->StepSize _before_ calling this routine.
  //   If ODEIterCount<1, this routine just calls StepRungeKutta.
  // Otherwise, it assumes m0, torque0 and StepSize0 have valid values
  // from the previous step.  The controlling program *must* call
  // ResetODE() to reset ODEIterCount on each new integration segment
  // (i.e., when m0, torque0 or StepSize0 are invalid), or when a ODE
  // history restart is desired.
  //   The import minstep is ignored on the initial pass, but StepSize
  // will not be reduced by this routine to below minstep.  If a valid
  // step is not found, StepSize is set to 0.

  // The following step adjustment parameters fit empirically.
  const double UpperCutRatio=0.8;  // On bad step, always cut at least UCR,
  const double LowerCutRatio=0.1;  // but never more than LCR.
  const double UpperIncreaseRatio=1.2;  // On good step, never increase more
  const double LowerIncreaseRatio=0.5;  // than UIR, or cut more than LIR.
  const double ASEAimRatio=0.5;    // Headroom adjustment for pred-corr
  const double AllowedEnergyErrorRatio=0.67;
  const double EnergyHeadroom=0.8; // Headroom adjustment for energy fit
  const double EnergyIncreaseOffset=0.25;
  const double ErrorAdjExp=Oc_Nop(1./3.); // Errors should grow as cube
                                         /// of stepsize.

  static double vtemparr[3];  static ThreeVector vtemp(vtemparr);

  if(ODEIterCount<1) { // Insufficient past history to do multistep
    // StepEuler(minstep,maxtorque,next_stepsize);
    StepRungeKutta4(minstep,maxtorque,next_stepsize);
    return;
  }

  ThreeVector** htemp; //  Temp. pointer used for swapping h and torque
  int i,k;
  double actual_energy_change=0.;
  double expected_energy_change=0.;

  double orig_stepsize=StepSize;
  double orig_energy=energy;
  double energy_slack=2*fabs(orig_energy)*OC_REAL8_EPSILON;
  double relstep,t0coef,tcoef,t1coef,dtemp;
  double error=0.0;
  double adj_ratio,energy_adj_ratio,pc_adj_ratio;
  double dEdt_base=GetdEdt();  // Note: If this is positive then we have
  double dEdt_1;               //       round-off problems.

  energy_density1.Swap(energy_density); // Save current energy_density
  /// in scratch space to use later in accurate energy change
  /// calculation.

  // Estimate m1, using m, torque, and torque0.  (Predictor)
  relstep=StepSize/StepSize0;
  t0coef= -0.5*relstep*StepSize; tcoef=StepSize*(1+0.5*relstep);
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    vtemp.FastAdd(t0coef,torque0[i][k],tcoef,torque[i][k]);
    m1[i][k].FastAdd(vtemp,1.-vtemp.MagSq()/2.,m[i][k]);
    /// Includes correction due to restriction that |m1[i][k]|==1.
    /// Not sure how this affects error estimator.
    m1[i][k].Scale(1);
  }

  double AllowedSolverError=AllowedSpinError;
  while(1) {
    AllowedSolverError=OC_MIN(AllowedSpinError,0.2*StepSize*maxtorque);
    /// Error should never be larger than AllowedSpinError, but for
    /// small steps a more relevant comparison is relative to |m1-m|.
    /// (For small steps the first criterion is never triggered.)
    step_total++;
    m1.Swap(m);
    htemp=h; h=h1; h1=htemp;
    hValid=0; hUpdate(); // Calculate h1 from m1 estimate
    m1.Swap(m);
    htemp=h; h=h1; h1=htemp;
    CalculateTorque(m1,h1,torque1);

    // Now calculate m1 using m, torque, and torque1.  (Corrector)
    t1coef=tcoef=StepSize/2.;
    error=0.0;
    for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
      vtemp.FastAdd(tcoef,torque[i][k],t1coef,torque1[i][k]);
      vtemp+=m[i][k];
      vtemp.Scale(1);
      m1[i][k]-=vtemp;
      dtemp=m1[i][k].MagSq();
      if(dtemp>error) error=dtemp;
      m1[i][k].SetDirection(vtemp);
    }

    error=sqrt(error);  // Maximum diff between m1 estimate and m1 final.
    error*=(1./6.);
    /// Theory predicts the actual error to be about 1/6 the difference
    /// between predicted m and corrected m.  See NOTES p178, 15-Sep-1998.
    hValid=0;
    m1.Swap(m); // Swap new m with m on entry
    htemp=torque; torque=torque1; torque1=htemp;
    htemp=h; h=h1; h1=htemp; // Save old field
    hUpdate();  // m, h now hold new position
    CalculateEnergy();
    CalculateTorque(m,h,torque);  // Calculate "real" new torque
    torqueValid=1;

    dEdt_1=GetdEdt();

    // Check if this was a good step.
    actual_energy_change = -CalculateEnergyDifference();
    /// NB: m & m1 and energy_density & energy_density1 are swapped.
    // actual_energy_change=energy-orig_energy;  // This should be <0.
    expected_energy_change=(dEdt_base+dEdt_1)*StepSize/2.;
    /// Note: If expected_energy_change is >0., then there are round-off
    ///       problems.  If this happens we should probably raise a
    ///       warning, but for now just be sure to account for this
    ///       possibility.
#ifdef TEST_CODE
  fprintf(stderr,"\nOIC: %d, thickness_sum=%g\n",ODEIterCount,thickness_sum);
  fprintf(stderr," torque1[%d][%d]=(%g,%g,%g)\n",Nx/4,Nz/4,
          torque[Nx/4][Nz/4][0],torque[Nx/4][Nz/4][1],torque[Nx/4][Nz/4][2]);
  fprintf(stderr," Expected change: %.16g\n",expected_energy_change);
  fprintf(stderr," Calculated new: %.16g\n",actual_energy_change);
  fprintf(stderr," Calculated old: %.16g\n",energy-orig_energy);
  fflush(stderr);
/**/
#endif // TEST_CODE

    if(error<AllowedSolverError &&
       expected_energy_change<energy_slack &&
       actual_energy_change < 
       expected_energy_change*AllowedEnergyErrorRatio+energy_slack)
      break; // Good step

    // Else, bad step
    m1.Swap(m); htemp=h; h=h1; h1=htemp; // Undo changes
    htemp=torque; torque=torque1; torque1=htemp;
    energy=orig_energy;
    double bad_step_size=StepSize;

    reject_total++;

    energy_adj_ratio=1.0;
    if(expected_energy_change>-OC_REAL8_EPSILON ||
       dEdt_base>-OC_REAL8_EPSILON) {
      // *Real* bad energy behavior.  (Likely to be round-off error.)
      reject_energy_count++;
      energy_adj_ratio=0.;
    } else if(actual_energy_change
              >=expected_energy_change*AllowedEnergyErrorRatio) {
      // Bad energy behavior
      reject_energy_count++;
      if(actual_energy_change>=0.0) {
        // Energy increased.  I tried fitting a quadratic through
        // the endpoints with dEdt_base, but this proved less
        // effective than just setting the size to LowerCutRatio
        // (presently 0.1) -mjd 26-Apr-1999
        energy_adj_ratio=0.0;
      } else {
        energy_adj_ratio=
          sqrt(EnergyHeadroom*actual_energy_change/expected_energy_change);
      }
    }
    // Note: If energy_adj_ratio is <=0, then the "LowerCutRatio" check
    //       below will cut in and reset it.

    pc_adj_ratio=1.0;
    if(error>=AllowedSolverError) {
      // Step rejected because |predictor - corrector| error too large.
      reject_position_count++;
      pc_adj_ratio=pow(Oc_Nop(ASEAimRatio*AllowedSolverError/error),
                       ErrorAdjExp);
      /// Error should vary with cube of stepsize.  Use Oc_Nop to
      /// work around optimizer bug in egcs-1.0.3.
    }

    adj_ratio=OC_MIN(energy_adj_ratio,pc_adj_ratio);
    if(adj_ratio>UpperCutRatio) adj_ratio=UpperCutRatio;
    if(adj_ratio<LowerCutRatio) adj_ratio=LowerCutRatio;
    StepSize*=adj_ratio;
    if(StepSize<minstep) StepSize=minstep;

    if((StepSize*maxtorque)<OC_REAL8_EPSILON) {  // Precision check
      StepSize=bad_step_size*UpperCutRatio; // Try smaller cut
    }

    if(bad_step_size<=minstep || (StepSize*maxtorque)<OC_REAL8_EPSILON) {
        // Step too small.
      char tempbuf[4096];
      sprintf(tempbuf," Error: %.16g\n Original Energy: %.16g\n"
              " Expected energy change: %.16g\n"
              " Actual  energy  change: %.16g\n",
              error,orig_energy,
              expected_energy_change,actual_energy_change);
      SendSmallStepMessage("Grid2D::StepPredict2",StepSize,maxtorque,
                           tempbuf);
      StepSize=0; next_stepsize=orig_stepsize;
      energy_density1.Swap(energy_density); // Restore original
      /// energy_density.
      return;
    }

    // Produce new m1 estimate using m, torque, and torque1.
    // Note: I'm not sure how this will effect the error estimator.
    relstep=StepSize/bad_step_size;
    tcoef =  StepSize*(1-0.5*relstep);
    t1coef=  StepSize*0.5*relstep;
    for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
      vtemp.FastAdd(tcoef,torque[i][k],t1coef,torque1[i][k]);
      m1[i][k].FastAdd(vtemp,1.-vtemp.MagSq()/2.,m[i][k]);
      // Includes correction due to restriction that |m1[i][k]| \in S^2
      m1[i][k].Scale(1);
    }

  }

  // Successful step found.

  // Save entry values of m, h and torque
  m1.Swap(m0); htemp=h0; h0=h1; h1=htemp;
  htemp=torque0; torque0=torque1; torque1=htemp;

  // Adjust stepsize for next call
  energy_adj_ratio=UpperIncreaseRatio;
  if(expected_energy_change<-OC_REAL8_EPSILON) {
    /// If the above test fails, then we are in a BAD round-off
    /// error situation.
    energy_adj_ratio=actual_energy_change/expected_energy_change
      +EnergyIncreaseOffset;
    if(energy_adj_ratio>0.) // This check *should* always pass.
      energy_adj_ratio=sqrt(energy_adj_ratio);
  }

  pc_adj_ratio=UpperIncreaseRatio;
  dtemp=OC_MAX(ASEAimRatio*AllowedSolverError,0.0);
  if(dtemp<
     error*UpperIncreaseRatio*UpperIncreaseRatio*UpperIncreaseRatio) {
    pc_adj_ratio=pow(Oc_Nop(dtemp/error),ErrorAdjExp);
    /// Error should vary with cube of stepsize.  Use Oc_Nop to
    /// work around optimizer bug in egcs-1.0.3.
  }

  adj_ratio=OC_MIN(energy_adj_ratio,pc_adj_ratio);
  if(adj_ratio>UpperIncreaseRatio) adj_ratio=UpperIncreaseRatio;
  if(adj_ratio<LowerIncreaseRatio) adj_ratio=LowerIncreaseRatio;

  next_stepsize=adj_ratio*StepSize;
}

void Grid2D::IncRungeKutta4(Nb_Array2D<MagElt> &m_in,
                            ThreeVector** &h_in,
                            double step,
                            Nb_Array2D<MagElt> &m_out)
{ // Takes one 4th order Runge-Kutta step, w/o stepsize control.
  // Used as base stepper for Grid2D::StepRungeKutta4()
  // m_in and h_in are imports; results stored in m_out
  // Overwrites m, h, & torque (used for temporary storage)
  // NOTE: m_out *must* be distinct from m_in, and both must be distinct
  //       from m;  Also, h_in must be distinct from h.
  //
  //   The import minstep is ignored on the initial pass, but StepSize
  // will not be reduced by this routine to below minstep.  If a valid
  // step is not found, StepSize is set to 0.
  int i,k;
  
  CalculateTorque(m_in,h_in,torque); // Torque at starting location

  // Test step 1
  hValid=0;
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    m_out[i][k].FastAdd(m_in[i][k],step/6.0,torque[i][k]);
    m[i][k].FastAdd(m_in[i][k],step/2.0,torque[i][k]);
    m[i][k].Scale(1);  // Might want to delay rescaling, but if we do,
                       // will hUpdate() & CalculateTorque() work properly?
  }
#ifdef RUNGE_KUTTA_FAST
  hFastUpdate();  // m, h now hold first test location
#else
  hUpdate();      // m, h now hold first test location
#endif

  // Test step 2
  CalculateTorque(m,h,torque);
  hValid=0;
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    m_out[i][k].Accum(step/3.0,torque[i][k]);
    m[i][k].FastAdd(m_in[i][k],step/2.0,torque[i][k]);
    m[i][k].Scale(1);  // Ditto rescaling comment.
  }
#ifdef RUNGE_KUTTA_FAST
  hFastUpdate();  // m, h now hold second test location
#else
  hUpdate();      // m, h now hold second test location
#endif

  // Test step 3
  CalculateTorque(m,h,torque);
  hValid=0;
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    m_out[i][k].Accum(step/3.0,torque[i][k]);
    m[i][k].FastAdd(m_in[i][k],step,torque[i][k]);
    m[i][k].Scale(1);  // Ditto rescaling comment.
  }
#ifdef RUNGE_KUTTA_FAST
  hFastUpdate();  // m, h now hold third test location
#else
  hUpdate();      // m, h now hold third test location
#endif

  // Test step 4
  CalculateTorque(m,h,torque);
  hValid=0;
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    m_out[i][k].Accum(step/6.0,torque[i][k]);
    m_out[i][k].Scale(1);
  }
}

void Grid2D::StepRungeKutta4(double minstep,double maxtorque,
                             double &next_stepsize,int forcestep)
{ // Takes one 4th order Runge-Kutta step, with adaptive stepsize
  // control.  Assumes m, h, torque & energy are current coming in, and
  // leaves them current going out.
  // The attempted step size is determined by the value of *this->StepSize
  // on entry.  StepSize may be adjusted by this routine; the value of
  // StepSize on exit is the step size actually used.  The export
  // next_stepsize should be used by the calling routine (on the next pass)
  // to set *this->StepSize _before_ calling this routine.
  //  Note: If the "forcestep" import is 1, then the smallest allowed step
  // is accepted, even if the error is above tolerance.  This really
  // shouldn't be used... ;^)

  const double AllowedError=AllowedSpinError;

  const double SafetyFactor=0.8;  // Set empirically
  const double HeadRoom=0.9;      // Ditto
  static double vtemparr[3];  static ThreeVector vtemp(vtemparr);
  ThreeVector** htemp; //  Temp. pointer used for swapping h
  double halfstep,fullstep;
  double oldenergy,error,dtemp,orig_stepsize=StepSize;
  int minstep_invoked=0;
  int i,k;

  // Save current m, h and energy
  m.Swap(m0); htemp=h; h=h0; h0=htemp;
  oldenergy=energy;

  // Do one valid step
  next_stepsize=StepSize;
  if(StepSize<minstep) {
    minstep=StepSize;
    minstep_invoked=1;
  }
  while(1) {
    step_total++;
    if((next_stepsize*maxtorque)<OC_REAL8_EPSILON ||
        next_stepsize<minstep) {
      // Precision check
      SendSmallStepMessage("Grid2D::StepRungeKutta4",
                           next_stepsize,maxtorque);
      if(!forcestep) {
        StepSize=0; next_stepsize=orig_stepsize;
        m.Swap(m0); htemp=h; h=h0; h0=htemp; energy=oldenergy; // Undo changes
        return;
      }
    }
    halfstep=(fullstep=next_stepsize)/2.0;
    // Do first half step, saving result in m1
    IncRungeKutta4(m0,h0,halfstep,m1);
    // Prepare for second half step
    m1.Swap(m); hValid=0; hUpdate();
    m.Swap(m1); htemp=h; h=h1; h1=htemp;
    // Do second half step, saving result in m2
    IncRungeKutta4(m1,h1,halfstep,m2);

    // Do full step, saving result in m1
    IncRungeKutta4(m0,h0,fullstep,m1);
    StepSize=fullstep;

    // Determine error estimate
    error=0; hValid=0;
    for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
      vtemp.FastAdd(m2[i][k],-1.0,m1[i][k]);
      if((dtemp=vtemp.MagSq())>error) error=dtemp;
#ifdef OLD_CODE
      m[i][k].FastAdd(m2[i][k],1./15.,vtemp);  // <== Here is final m!
#endif  // On 1st standard problem, straight m2 converges in fewer steps.
    }
#ifndef OLD_CODE
    m.Swap(m2);   // <== Here is final m!
#endif
    error=OC_MAX(sqrt(error),AllowedError/1024.);
    // The "1024." above is arbitrary, used to insure error>0.
    // This also keeps next_stepsize from increasing by more than
    // a factor of 4 in one step.

    // Adjust step size based on error estimate & energy, and
    // terminate "one valid step" loop if appropriate.
    next_stepsize=fullstep*pow(AllowedError/error,0.2)*SafetyFactor;
    if(!minstep_invoked && next_stepsize<minstep) {
      next_stepsize=minstep;
      minstep_invoked=1;
    }
    if(forcestep && (next_stepsize*maxtorque)<OC_REAL8_EPSILON) {
      // nextstep too small, but forcestep is requested.
      hUpdate(); CalculateEnergy();
      break;
    }

    // Check for step errors and take appropriate actions.
    if(error>AllowedError) {
      // Bad step; cut and retry
      reject_total++;  reject_position_count++;
      continue;
    }
    hUpdate(); CalculateEnergy(); // Else check that energy is decreasing
    if(energy<oldenergy+OC_REAL8_EPSILON) break;   // Good step!
    reject_total++;  reject_energy_count++;
    next_stepsize=fullstep/2.0; // Bad step;  Cut and retry
    if(!minstep_invoked && next_stepsize<minstep) {
      next_stepsize=minstep;
      minstep_invoked=1;
    }
  }
  next_stepsize*=HeadRoom;
  CalculateTorque(m,h,torque); torqueValid=1;
}

double Grid2D::StepODE(double mintimestep,double maxtimestep,
                       int& errorcode)
{ // Iterates one step, updating m, h, and energy.
  // If maxtimestep>0, then will keep step small enough to insure
  //  that the resulting delta time <= maxtimestep
  // If mintimestep>0, then will give up if stepsize reduces below
  //  this value.
  // Returns max m x h value.
  // Will update h as needed.  Will update energy & torque only if
  // hValid is FALSE or ODEIterCount<1.  ***THIS SHOULD BE CONSIDERED***
  //                                     ***   A BUG\b\b\bFEATURE!   ***
  // errorcode>0 on error, =0 on success, <0 if successful with warnings
  STOPWATCH_START(sw_solve_total);
#define PERTURB_RETRIES 2  // Number of perturbation retries before
                          /// failing
  int perturb_count=0;
  double minstep,maxstep,maxtorque;

  errorcode=0;
  if(StepSize<=0.) StepSize=InitialStepSize;
  StepSize0=StepSize; // Store size of previous step

  // Convert timestep bounds from seconds to internal solver step units
  maxstep=ConvertTimeToStepSize(maxtimestep);
  minstep=ConvertTimeToStepSize(mintimestep);

  // Get max value of torque.  This will update h and
  // torque if necessary. 
  maxtorque=GetMaxTorque();
  /// NOTE: As noted at top of this file, "torque" is defined here
  /// to be (-1/DampCoef)(mxh)-mx(mxh).

  // Update energy, if necessary.
  if(!energyValid) CalculateEnergy();

  // Take one step
  while(1) {
    if(NextStepSize>0) StepSize=NextStepSize;
    // Check stepsize bounds
    if(StepSize*maxtorque>MaxTorqueStep) {
      StepSize=MaxTorqueStep/maxtorque;
    }
    if(StepSize<minstep) StepSize=minstep;
    if(maxtorque>0.0 && StepSize*maxtorque<OC_REAL8_EPSILON) {
      StepSize=OC_REAL8_EPSILON/maxtorque;
    }
    if(maxstep>0. && StepSize>maxstep) {
      StepSize=maxstep;
    }

#ifdef EULER_ODE               // 1st order Euler
    StepEuler(minstep,maxtorque,NextStepSize);
#elif defined(RUNGE_KUTTA_ODE) // 4th order Runge-Kutta
    StepRungeKutta4(minstep,maxtorque,NextStepSize);
#elif defined(PREDICTOR_2_ODE)
    StepPredict2(minstep,maxtorque,NextStepSize);
#else
#error Unimplemented ODE solver request
#endif

    if(StepSize>0) break;  // Valid step found

#ifndef RUNGE_KUTTA_ODE
    // Couldn't find valid step.  Try a 4th order Runge-Kutta step
    ResetODE();
    if(maxtorque>OC_REAL8_EPSILON) { // Protect against 0 torque
      StepSize=MaxTorqueStep/maxtorque;
    } else {
      StepSize=InitialStepSize;
    }

    // Check stepsize bounds
    if(StepSize*maxtorque>MaxTorqueStep) {
      StepSize=MaxTorqueStep/maxtorque;
    }
    if(StepSize<minstep) StepSize=minstep;
    if(maxtorque>0.0 && StepSize*maxtorque<OC_REAL8_EPSILON) {
      StepSize=OC_REAL8_EPSILON/maxtorque;
    }
    if(maxstep>0. && StepSize>maxstep) {
      StepSize=maxstep;
    }

    StepRungeKutta4(minstep,maxtorque,NextStepSize,1);
    if(StepSize>0.) break;  // Step accepted
#endif

    if(perturb_count>=PERTURB_RETRIES) break; // Give up

    // Otherwise, no valid step found.  Perturb and retry
    MessageLocker::Append("Warning in Grid2D::StepODE: "
                          "Unable to find a valid step,"
                          "  Will jiggle values and retry.\n");
    perturb_count++;
    Perturb(PERTURBATION_SIZE); hUpdate();  CalculateEnergy();
    CalculateTorque(m,h,torque); torqueValid=1;
    maxtorque=GetMaxTorque();
    if(NextStepSize*maxtorque>MaxTorqueStep)
      NextStepSize=MaxTorqueStep/maxtorque;
  }

  // Adjust NextStepSize as required by MaxTorqueStep
  maxtorque=GetMaxTorque();
  if(NextStepSize*maxtorque>MaxTorqueStep) {
    NextStepSize=MaxTorqueStep/maxtorque;
  }

  // Check for errors, and increment ODEIterCount if applicable
  if(StepSize<=0) {
    errorcode=1;
    MessageLocker::Append("ERROR in Grid2D::StepODE: "
                          "Unable to find a valid step (maxtorque=%g),"
                          "  Giving up.\n",maxtorque);
    Message("ERROR in Grid2D::StepODE: "
            "Unable to find a valid step (maxtorque=%g),"
            "  Giving up.\n",maxtorque);
  } else {
    ODEIterCount++;
    errorcode = -perturb_count;
  }

  STOPWATCH_STOP(sw_solve_total);
  return GetMxH(maxtorque);
#undef PERTURB_RETRIES
}

void Grid2D::GetNearestGridNode(double x,double y,int &i,int &j)
{
#ifdef CODE_CHECKS
  if(cellsize<OC_REAL8_EPSILON)
    PlainError(1,"Error in Grid2D::GetNearestGridNode: "
             "Cellsize (=%g) is too small or not initialized",cellsize);
#endif // CODE_CHECKS
  i=int(ceil(x/cellsize));  j=int(ceil(y/cellsize));
  if(i<0) i=0; else if(i>=Nx) i=Nx-1;
  if(j<0) j=0; else if(j>=Nz) j=Nz-1;
}

void Grid2D::GetRealCoords(int i,int j,
                           double &xout,double &yout,double & zout)
{ // Node points are in cell centers.  Also note change of
  // coordinate systems, from xz+y to xy+z
  if(Nx<1) xout=0.0; else xout=xmax*double(i)/double(Nx);
  if(Nz<1) yout=0.0; else yout=zmax*double(j)/double(Nz);
  xout+=cellsize/2.; yout+=cellsize/2.;
  zout=0.;
}

double Grid2D::GetMaxNeighborAngle()
{ // Returns maximum angle (in radians) between
  // neighboring spin directions in the grid.
  int i,k;
  double dot,mindot;
  mindot=1.0;
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
    // Note: Spins with zero-thickness *should* have
    //  no neighbors, and therefore return dot=1.0
    dot=m[i][k].GetMinNeighborDot();
    if(dot<mindot) mindot=dot;
  }
  if(fabs(mindot)>1) {
    fprintf(stderr,"ERROR: Mindot=%15.12f\n",mindot);
    PlainError(1,"Error in Grid2D::GetMaxNeighborAngle: %s",ErrProgramming);
  }
  return acos(mindot);
}

double Grid2D::GetEnergyDensity() {
  // Returns average energy density, in J/m^3
  if(!energyValid) CalculateEnergy();
  double mult=mu0*Ms*Ms;
  return energy*mult;
}

void Grid2D::GetEnergyDensities(double& exch,double& anis,double& demag,
                                double& zeeman,double& total)
{
  // Returns average energy densities, in J/m^3
  if(!energyValid) CalculateEnergy();
  double mult=mu0*Ms*Ms;
  exch=exch_energy*mult;
  anis=anis_energy*mult;
  demag=demag_energy*mult;
  zeeman=zeeman_energy*mult;
  total=energy*mult;
}

double Grid2D::GetFieldComponentNorm(const OC_CHAR* component,
				     double thick_request,
				     int& errorcode)
{
  errorcode=0;

  if(!hValid) { hUpdate(); CalculateEnergy(); }

  int i,k;
  const double thick(thick_request);
  double hnorm=0.0;
  double thick_sum=0.0;
  double arr[VECDIM]; ThreeVector htemp(arr);
  double hthick=1.0; // If thick_request<0, then hthick is
  /// held at 1.0 for all cells, and the returned result is the
  /// average h field across the entire gridded region.  If
  /// thick_request>=0, then hthick is set the the thickness
  /// of each cell, and the returned result is the average h field
  /// inside each cell of the sample with cell thickness >=
  /// thick_request.

  if(strcmp(component,"total")==0) {
    for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
      if(thick>=0.0) hthick=m[i][k].GetThickness();
      if(hthick>thick) {
	hnorm +=  hthick*h[i][k].MagSq();
	thick_sum += hthick;
      }
    }
  } else if(strcmp(component,"demag")==0) {
    if(DoDemag) {
      for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
	if(thick>=0.0) hthick=m[i][k].GetThickness();
	if(hthick>thick) {
	  hnorm += hthick*hdemag[i][k].MagSq();
	  thick_sum += hthick;
	}
      }
    }
  } else {
    // Calculate field component on the fly
    if(strcmp(component,"anis")==0 || strcmp(component,"anisotropy")==0) {
      for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
	double cellthick=m[i][k].GetThickness();
	if(thick>=0.0) hthick=cellthick;
	if(hthick>thick) {
	  if(cellthick>0) {
	    // Anisotropy is 0 outside of part
	    hnorm +=
	      hthick*((m[i][k].*MagElt::AnisotropyField)().MagSq());
	  }
	  thick_sum += hthick;
	}
      }
    } else if(strcmp(component,"applied")==0
	      || strcmp(component,"zeeman")==0) {
      for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
	if(thick>=0.0) hthick=m[i][k].GetThickness();
	if(hthick>thick) {
	  hApplied->GetLocalhField(i,k,htemp);
	  hnorm += hthick*htemp.MagSq();
	  thick_sum += hthick;
	}
      }
    } else if(strcmp(component,"exchange")==0) {
      for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
	double cellthick=m[i][k].GetThickness();
	if(thick>=0.0) hthick=cellthick;
	if(hthick>thick) {
	  if(cellthick>0) {
	    // Exchange is 0 outside of part
	    hnorm += hthick*(m[i][k].CalculateExchange().MagSq());
	  }
	  thick_sum += hthick;
	}
      }
    } else {
      Message("Error in Grid2D::GetFieldComponentNorm; "
	      "Unknown field component: %s",component);
      errorcode=1;
      return -1.0;
    }
  }

  if(thick_sum<=0.0) return 0.0;
  hnorm = sqrt(hnorm/thick_sum);
  return hnorm;
}


Vec3D Grid2D::GetAveMag()
{
  int i,k;
  Vec3D AveMag(0.,0.,0.);
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++)
    AveMag.Accum(m[i][k].GetThickness(),m[i][k]);
  AveMag*=(1.0/thickness_sum);
  AveMag.ConvertXzyToXyz();
  return AveMag;
}

void Grid2D::GetAverageMagnetization(ThreeVector& AveMag)
{
  Convert(AveMag,GetAveMag());  // Note that the conversion
  /// from xz+y coords to xy+z coords is done inside GetAveMag().
}

Vec3D Grid2D::GetAveH()
{
  // Note that this routine performs conversion from xz+y coords
  // (internal coordinates) to xy+z coords (external coordinates).
  if(!hValid) hUpdate();
  int i,k;
  unsigned long cellcount;
  Vec3D AveHCol,AveH;
  cellcount=0;
  AveH=Vec3D(0.,0.,0.);
  for(i=0;i<Nx;i++) {
    AveHCol=Vec3D(0.,0.,0.);
    for(k=0;k<Nz;k++) {
      if(m[i][k].GetThickness()>0) {
        // Only include those locations where m is non-zero
        AveHCol+=h[i][k];
        cellcount++;
      }
    }
    AveH+=AveHCol;
  }  
  AveH*=(1./double(cellcount));
  AveH.ConvertXzyToXyz();
  return AveH;
}

#ifdef CONTRIB_SPT001
double Grid2D::GetResistance()
{ // Returns 'resistance' for the following assumptions 1) We are
  // examining the AMR effect 2) It is a THIN film being examined 3) The
  // individual 'cells' are in a SINGLE domain state i.e. the
  // magnetization accross a 'cell' is uniform - I believe this is the
  // assuption the solver works on (correct ?).
  //   For this special case we can say the resistance of a 'cell' is
  // equal to: R0 + (deltaR*((cos theta)^2))) Where R0 is the base
  // resistance, deltaR is the maximum change in resistance (typically
  // about %2) and theta is the angle between the 'cell' magnetization
  // and 'current'.
  //   Version 1 of this function merely returns <(cos theta)^2> of the
  // magnetizations and the 'current'. Current is currently 'fixed' as
  // (1,0,0) in xyz co-ords (need to add it to a MIF input file). The
  // next stage is to have R0 and deltaR defined as material parameters,
  // and then remove the definition of the values from this function to
  // handle 'real' resistance characteristics.
  //   Problems: Doesn't handle masks yet. Masks have a zero magnetization
  // vector. All it would do is to 'dilute' the average of the
  // resistance.

  int i,k;
  double R1,Rt,magCheck,currCheck,R0,deltaR; // R0 and deltaR are the
  /// resistance properties of the material and should be incorporated
  /// into the MIF description.  One question is will the MIF file define
  /// deltaR or deltaR/R0 ?

  Vec3D Mag(0.,0.,0.);
  Vec3D Current(1.,0.,0.); // Currently a vector in xyz co-ords (MIF input
                          /// format). Doesn't have to be a unit vector.

  Current.ConvertXyzToXzy(); // Keep everything INSIDE Grid2D as xzy
                            /// co-ords.

  R1=Rt=(double)0.0;
  R0=(double)0.0;     // Fix R0 and deltaR such that the function
                      // currently returns just <(cos theta)^2>,
  deltaR=(double)1.0; // the same as before until these values can be
                      // incorporated into material parameters.

  // deltaR*=R0;     // This conversion used to scale from MR co-efficient
                     // just once, if that system is used.

  currCheck=Current.MagSq();                                    
  currCheck=(!currCheck)?0:currCheck=sqrt(currCheck);
  /// Does Sqrt blow up for sqrt(0) ? Let's not find out.
  /// This has been moved to out of the loop for efficiency.
  /// It implies the current is uniform accross the grid.
  for(i=0;i<Nx;i++) {
    for(k=0;k<Nz;k++) {
      Mag=m[i][k];

      magCheck=m[i][k].GetThickness();
      /// Check for modulus of zero on the magnetization.

      // Make sure we dont break the oommf by trying to divide by zero
      if( (magCheck != 0) && (currCheck != 0) ) {
        R1=(Mag*Current);       // Dot the two vectors together.
        R1/=currCheck;
        /// Divide by the modulus' of the Current:(Mag is a unit vec).
        R1*=R1;                 // Square the result -> ((Cos Theta)^2).
      } else {
        // One of the vectors is zero - How to deal with this undefined
        // behaviour ?
        R1 = 0;                 // Just set resistance to 0 currently.
      }
      Rt+=(R1/(Nx*Nz));  // Add a fraction to the total (equiv to summing
                         // all and dividing by number summed).
    }   
  }

  return (R0+deltaR*Rt); // Maximum resistance when Mag and Current are
  /// parallel, min when perpendicular when the MR co-efficient is positive.
}
#endif // CONTRIB_SPT001

void Grid2D::GetMag(double** arr)
{ // Fills arr with magnetizations.  The calling routine must
  // make sure arr is big enough (cf. Grid2D::GetDimens()).
  int i,k;
  for(i=0;i<Nx;i++) for(k=0;k<Nz;k++)
    m[i][k].DumpMag(arr[i]+k*VECDIM);
}

void Grid2D::Dump(OC_CHAR *filename,OC_CHAR *note)
{
  int i,k;
  FILE *fptr;
  double Bapparr[3]; ThreeVector Bapp(Bapparr);
  hApplied->GetLocalBField(0,0,Bapp);

  if((fptr=Nb_FOpen(filename,"wb"))==NULL) {
    PlainError(1,"FATAL ERROR: Unable to open output file %s\n",filename);
  }

  double maxmxh=GetMaxMxH();
  double e_exch,e_anis,e_demag,e_zeeman,e_total;
  GetEnergyDensities(e_exch,e_anis,e_demag,e_zeeman,e_total);

  if(note!=NULL) fprintf(fptr,"%s\n\n",note);
  fprintf(fptr,
          "\nMicromagnetic grid parameters-------\n"
          " Grid size                     %3dx%d\n"
          " Demag field routine        demag = %s\n"     
          " Saturation magnetization      Ms = %g (A/m)\n"
          " Anisotropy constant           K1 = %g (J/m^3)\n"
          " Edge anisotropy constant     eK1 = %g (J/m^3)\n"
          " Exchange stiffness             A = %g (J/m)\n"
          " Applied field               Bapp = <%g,%g,%g> (T)\n"
          " Mesh cell edge length       mesh = %g (m)\n"
          " Final step size         StepSize = %g\n\n",
          Nx,Nz,ExternalDemag->name,Ms,K1,EdgeK1,exchange_stiffness,
          Bapp[0],Bapp[1],Bapp[2],
          cellsize,StepSize);
  fprintf(fptr,"Micromagnetic modeling results-------\n");
  fprintf(fptr,
          "Energy:  Total = %10.6f (J/m^3)\n"
          "          Exch = %10.6f (J/m^3)\n"
          "          Anis = %10.6f (J/m^3)\n"
          "         Demag = %10.6f (J/m^3)\n"
          "        Zeeman = %10.6f (J/m^3)\n",
          e_total,e_exch,e_anis,e_demag,e_zeeman);
  fprintf(fptr,"Max |mxh|: %10.7g\n",maxmxh);
  double mom;
  for(i=0;i<Nx;i++) {
    fprintf(fptr,"\nRow %d:\n",i);
    for(k=0;k<Nz;k++) {
      // Print out moments and torques.  Note change of coords
      // from internal xz+y to xy+z.
      mom=m[i][k].GetThickness();
      fprintf(fptr,"m[%2d][%2d]=(%7.4f,%7.4f,%7.4f)"
              "\tt[%2d][%2d]=(%10.7f,%10.7f,%10.7f)\n",
              i,k,mom*m[i][k][0],mom*m[i][k][2],-mom*m[i][k][1],
              i,k,torque[i][k][0],torque[i][k][2],-torque[i][k][1]);
    }
  }

  fprintf(fptr,"\n\n");
  fclose(fptr);
}

// Binary dump of (reduced) magnetization (VecFil format)
void Grid2D::BinDumpm(const OC_CHAR *filename, const OC_CHAR *note)
{
  int i;
  FileVector fv;

  fv.SetNote((OC_BYTE *)note);

  if(fv.WriteHeader(filename,Nx,1,Nz,VECDIM)) {
    Message("Warning in Grid2D::BinDumpm: %s",ErrBadFile);
    return;
  }

  int k;
  double *temparr= new double[VECDIM*Nz];
  for(i=0;i<Nx;i++) {
    for(k=0;k<Nz;k++) m[i][k].DumpMag(temparr+k*VECDIM);
    if(fv.WriteVecs(Nz,temparr)) {
      Message("Warning in Grid2D::BinDumpm: %s",ErrBadFile);
      break;
    }
  }
  delete[] temparr;
  fv.Close();
}

// Binary dump of (reduced) field (VecFil format)
void Grid2D::BinDumph(const OC_CHAR *filename,const OC_CHAR *note)
{
  int i;
  FileVector fv;

  if(!hValid) { hUpdate(); CalculateEnergy(); }

  fv.SetNote((OC_BYTE *)note);

  if(fv.WriteHeader(filename,Nx,1,Nz,VECDIM)) {
    Message("Warning in Grid2D::BinDumph: %s",ErrBadFile);
    return;
  }

  for(i=0;i<Nx;i++) {
    if(fv.WriteVecs(Nz,&(h[i][0][0]))) { // NOTE: &(h[i][0][0])     /**/
                                         // is a *BAD* thing        /**/
      Message("Warning in Grid2D::BinDumph: %s",ErrBadFile);
      break;
    }
  }
  fv.Close();
}

// Writes an ovf file, of either m (if type=='m') or h (if type=='h').
// The datastyle import should be one of "binary4", "binary8" or "text",
// with a NULL pointer or empty string implying the default value
// "binary4".  NB: the ovf format uses x & y in plane, as opposed to
// Grid2D which uses x & z in plane (both are righthand systems).  This
// routine does the necessary conversions.
void Grid2D::WriteOvf(OC_CHAR type,
                      const OC_CHAR *datatype,const OC_CHAR *precision,
                      const OC_CHAR *filename,const OC_CHAR *realname,
                      const OC_CHAR *note,const ThreeVector * const *arr)
{
  if((type!='m' && type!='h' && type!='o') || filename==NULL
     || (type=='o' && arr==NULL) || (type!='o' && arr!=NULL)) {
    Message("Warning in Grid2D::WriteOvf: %s",ErrBadParam);
    return;
  }

  // Handle default inputs
  if(datatype==NULL || datatype[0]=='\0') {
    datatype="binary";
  }
  if(precision==NULL || precision[0]=='\0') {
    if(strcmp(datatype,"binary")==0)
      precision="4";
    else
      precision="%# .17g";
  }

  // Determine datastyle
  Nb_DString datastyle("text");
  if(strcmp(datatype,"binary")==0) {
    if(atoi(precision)>4) datastyle.Dup("binary8");
    else                  datastyle.Dup("binary4");
  }

  // Safety check
  if(strcmp(datastyle.GetStr(),"binary4")!=0 &&
     strcmp(datastyle.GetStr(),"binary8")!=0 &&
     strcmp(datastyle.GetStr(),"text")!=0) {
    Message("Warning in Grid2D::WriteOvf; Unknown data style: %s",
            datastyle.GetStr());
    return;
  }

  FILE *fptr=Nb_FOpen(filename,"wb");
  if(fptr==NULL) {
    Message("Warning in GridD::WriteOvf: Unable to open file %s"
            " for writing",filename);
    return;
  }

  if(type=='h' && !hValid) {
    hUpdate(); CalculateEnergy();
  }

  if(realname==NULL || realname[0]=='\0') realname=filename;

  // Write ovf header /////////////////////////////////////////////////
  fprintf(fptr,
          "# OOMMF: rectangular mesh v1.0\n"
          "# Segment count: 1\n"
          "# Begin: Segment\n"
          "# Begin: Header\n");
  fprintf(fptr,"# Title: %s\n",realname);
  if(note!=NULL && note[0]!='\0') {
    if(strchr(note,' ')!=NULL) {  // Spaces-only => no Desc:
      char* buf=new char[strlen(note)+1];
      char* nexttoken = buf;
      strcpy(buf,note);
      char* cptr=Nb_StrSep(&nexttoken,"\n");
      while(cptr!=NULL) {
        fprintf(fptr,"# Desc: %s\n",cptr);
        cptr=Nb_StrSep(&nexttoken,"\n");
      }
      delete[] buf;
    }
  } else {
    fprintf(fptr,"# Desc: Reduced %s\n",
            (type=='m' ? "magnetization" : "field"));
  }

  // Note the coordinate system transformation: y -> -z
  fprintf(fptr,
          "# meshtype: rectangular\n"
          "# meshunit: m\n"
          "# xbase: %.16g\n"
          "# ybase: %.16g\n"
          "# zbase: %.16g\n",cellsize/2.,cellsize/2.,ymax/2.);
  fprintf(fptr,
          "# xstepsize: %.16g\n"
          "# ystepsize: %.16g\n"
          "# zstepsize: %.16g\n",cellsize,cellsize,ymax);
  fprintf(fptr,
          "# xnodes: %d\n"
          "# ynodes: %d\n"
          "# znodes: %d\n",Nx,Nz,1);
  fprintf(fptr,
          "# xmin: %.16g\n"
          "# ymin: %.16g\n"
          "# zmin: %.16g\n",0.,0.,0.);
  fprintf(fptr,
          "# xmax: %.16g\n"
          "# ymax: %.16g\n"
          "# zmax: %.16g\n",xmax,zmax,ymax);
#if 0
  // As of 6/2001, mmDisp supports display of out-of-plane rotations;
  // Representing the boundary under these conditions is awkward with
  // a single polygon.  So don't write the boundary line, and rely
  // on defaults based on the data range and step size.
  fprintf(fptr,
          "# boundary:"
          " %.16g %.16g %.16g %.16g %.16g %.16g"
          " %.16g %.16g %.16g %.16g %.16g %.16g"
          " %.16g %.16g %.16g\n",
          0.,0.,ymax/2.,0.,zmax,ymax/2.,
          xmax,zmax,ymax/2.,xmax,0.,ymax/2.,
          0.,0.,ymax/2.);
#endif
  fprintf(fptr,
          "# valueunit: A/m\n"
          "# valuemultiplier: %.17g\n",Ms);
  if(type=='m') {
    fprintf(fptr,
            "# ValueRangeMinMag: 1e-8\n"
            "# ValueRangeMaxMag: 1.0\n");
  } else {
    // Determine largest field magnitude
    double max_mag=0,mag;
    if(type=='h') {
      for(int i=0;i<Nx;i++) for(int k=0;k<Nz;k++) {
        mag=h[i][k].MagSq();
        if(mag>max_mag) max_mag=mag;
      }
    } else {
      for(int i=0;i<Nx;i++) for(int k=0;k<Nz;k++) {
        mag=arr[i][k].MagSq();
        if(mag>max_mag) max_mag=mag;
      }
    }
    max_mag=sqrt(max_mag);
    fprintf(fptr,
            "# ValueRangeMinMag: %.17g\n"
            "# ValueRangeMaxMag: %.17g\n",max_mag*1e-8,max_mag);
  }
  fprintf(fptr,
          "# End: Header\n");

  // Dump data ////////////////////////////////////////////////////////
  if(strcmp(datastyle.GetStr(),"binary4")==0) {
    // 4-byte binary format //
    fprintf(fptr,
            "# Begin: Data Binary 4\n");
    OC_REAL4 checkvalue=1234567.;
# if (OC_BYTEORDER == 4321)  // Flip bytes from LSB to MSB
    Oc_Flip4(&checkvalue);
# endif
    fwrite((char *)&checkvalue,sizeof(OC_REAL4),1,fptr);
    union { OC_REAL4 x; OC_UCHAR c[4]; } outarr[3];
    if(sizeof(outarr[0])!=4) {
      PlainError(1,"Error in Grid2D::WriteOvf; "
                 "PROGRAMMING ERROR---OC_REAL4 union %d bytes wide",
                 sizeof(outarr[0]));
    }
    if(type=='m') {
      // Write reduced magnetization
      double mtemp[3];
      for(int k=0;k<Nz;k++) for(int i=0;i<Nx;i++) {
        m[i][k].DumpMag(mtemp);  // Includes "relative moment" adjustment
        outarr[0].x = static_cast<OC_REAL4>( mtemp[0]);
	outarr[1].x = static_cast<OC_REAL4>( mtemp[2]);
	outarr[2].x = static_cast<OC_REAL4>(-mtemp[1]);
        /// Note coordinate change.
#     if (OC_BYTEORDER == 4321)  // Flip bytes from LSB to MSB
        Oc_Flip4(outarr[0].c); Oc_Flip4(outarr[1].c); Oc_Flip4(outarr[2].c);
#     endif // OC_BYTEORDER
        fwrite((char *)outarr,sizeof(OC_REAL4),3,fptr);
        /// The above fwrite ASSUMES outarr is packed.
      }
    } else if(type=='h') {
      // Write reduced field
      for(int k=0;k<Nz;k++) for(int i=0;i<Nx;i++) {
        outarr[0].x= static_cast<OC_REAL4>( h[i][k][0]);
        outarr[1].x= static_cast<OC_REAL4>( h[i][k][2]);
        outarr[2].x= static_cast<OC_REAL4>(-h[i][k][1]);
        /// Note coordinate change.
#     if (OC_BYTEORDER == 4321)  // Flip bytes from LSB to MSB
        Oc_Flip4(outarr[0].c); Oc_Flip4(outarr[1].c); Oc_Flip4(outarr[2].c);
#     endif // OC_BYTEORDER
        fwrite((char *)outarr,sizeof(OC_REAL4),3,fptr);
        /// The above fwrite ASSUMES outarr is packed.
      }
    } else {
      // Write "other" field
      for(int k=0;k<Nz;k++) for(int i=0;i<Nx;i++) {
        outarr[0].x= static_cast<OC_REAL4>( arr[i][k][0]);
        outarr[1].x= static_cast<OC_REAL4>( arr[i][k][2]);
        outarr[2].x= static_cast<OC_REAL4>(-arr[i][k][1]);
        /// Note coordinate change.
#     if (OC_BYTEORDER == 4321)  // Flip bytes from LSB to MSB
        Oc_Flip4(outarr[0].c); Oc_Flip4(outarr[1].c); Oc_Flip4(outarr[2].c);
#     endif // OC_BYTEORDER
        fwrite((char *)outarr,sizeof(OC_REAL4),3,fptr);
        /// The above fwrite ASSUMES outarr is packed.
      }
    }
    fprintf(fptr,"\n"
            "# End: Data Binary 4\n");

  } else if(strcmp(datastyle.GetStr(),"binary8")==0) {
    // 8-byte binary format //
    fprintf(fptr,
            "# Begin: Data Binary 8\n");
    OC_REAL8 checkvalue=123456789012345.;
# if (OC_BYTEORDER == 4321)  // Flip bytes from LSB to MSB
    Oc_Flip8(&checkvalue);
# endif
    fwrite((char *)&checkvalue,sizeof(OC_REAL8),1,fptr);
    union { OC_REAL8 x; OC_UCHAR c[8]; } outarr[3];
    if(sizeof(outarr[0])!=8) {
      PlainError(1,"Error in Grid2D::WriteOvf; "
                 "PROGRAMMING ERROR---OC_REAL8 union %d bytes wide",
                 sizeof(outarr[0]));
    }
    if(type=='m') {
      // Write reduced magnetization
      double mtemp[3];
      for(int k=0;k<Nz;k++) for(int i=0;i<Nx;i++) {
        m[i][k].DumpMag(mtemp);  // Includes "relative moment" adjustment
        outarr[0].x=mtemp[0]; outarr[1].x=mtemp[2]; outarr[2].x= -mtemp[1];
        /// Note coordinate change.
#     if (OC_BYTEORDER == 4321)  // Flip bytes from LSB to MSB
        Oc_Flip8(outarr[0].c); Oc_Flip8(outarr[1].c); Oc_Flip8(outarr[2].c);
#     endif // OC_BYTEORDER
        fwrite((char *)outarr,sizeof(OC_REAL8),3,fptr);
        /// The above fwrite ASSUMES outarr is packed.
      }
    } else if(type=='h') {
      // Write reduced field
      for(int k=0;k<Nz;k++) for(int i=0;i<Nx;i++) {
        outarr[0].x=  h[i][k][0];
        outarr[1].x=  h[i][k][2];
        outarr[2].x= -h[i][k][1];
        /// Note coordinate change.
#     if (OC_BYTEORDER == 4321)  // Flip bytes from LSB to MSB
        Oc_Flip8(outarr[0].c); Oc_Flip8(outarr[1].c); Oc_Flip8(outarr[2].c);
#     endif // OC_BYTEORDER
        /// Note coordinate change.
        fwrite((char *)outarr,sizeof(OC_REAL8),3,fptr);
        /// The above fwrite ASSUMES outarr is packed.
      }
    } else {
      // Write "other" field
      for(int k=0;k<Nz;k++) for(int i=0;i<Nx;i++) {
        outarr[0].x=  arr[i][k][0];
        outarr[1].x=  arr[i][k][2];
        outarr[2].x= -arr[i][k][1];
        /// Note coordinate change.
#     if (OC_BYTEORDER == 4321)  // Flip bytes from LSB to MSB
        Oc_Flip8(outarr[0].c); Oc_Flip8(outarr[1].c); Oc_Flip8(outarr[2].c);
#     endif // OC_BYTEORDER
        /// Note coordinate change.
        fwrite((char *)outarr,sizeof(OC_REAL8),3,fptr);
        /// The above fwrite ASSUMES outarr is packed.
      }
    }
    fprintf(fptr,"\n"
            "# End: Data Binary 8\n");

  } else if(strcmp(datastyle.GetStr(),"text")==0) {
    // Text data format //
    fprintf(fptr,
            "# Begin: Data Text\n");
    Nb_DString outfmt(precision);  // Build up output format
    outfmt.Append(" "); outfmt.Append(precision);
    outfmt.Append(" "); outfmt.Append(precision);
    outfmt.Append("\n");
    if(type=='m') {
      // Write reduced magnetization
      double mtemp[3];
      for(int k=0;k<Nz;k++) for(int i=0;i<Nx;i++) {
        m[i][k].DumpMag(mtemp);  // Includes "relative moment" adjustment
        fprintf(fptr,outfmt.GetStr(),mtemp[0],mtemp[2],-mtemp[1]);
        /// Note coordinate change.
      }
    } else if(type=='h') {
      // Write reduced field
      for(int k=0;k<Nz;k++) for(int i=0;i<Nx;i++) {
        fprintf(fptr,outfmt.GetStr(),
                h[i][k][0],h[i][k][2],-h[i][k][1]);
        /// Note coordinate change.
      }
    } else {
      // Write "other" field
      for(int k=0;k<Nz;k++) for(int i=0;i<Nx;i++) {
        fprintf(fptr,outfmt.GetStr(),
                arr[i][k][0],arr[i][k][2],-arr[i][k][1]);
        /// Note coordinate change.
      }
    }
    fprintf(fptr,
            "# End: Data Text\n");

  } else {
    // Safety: Should never get here!
    fclose(fptr);
    PlainError(1,"Error in Grid2D::WriteOvf; "
               "PROGRAMMING ERROR---Bad filestyle: %s",datastyle.GetStr());
  }
  fprintf(fptr,"# End: Segment\n");
  fclose(fptr);
}

// Writes an OVF file of a h field component.  The import "component"
// should be one of anis, applied, demag, exchange or total.
// NOTE: The h1 array is used as temporary workspace.
void
Grid2D::WriteFieldComponent(const OC_CHAR *component,
                            const OC_CHAR *datatype,const OC_CHAR *precision,
                            const OC_CHAR *filename,const OC_CHAR *realname,
                            const OC_CHAR *note,int &errorcode)
{
  int i,k;
  errorcode=0;

  if(!hValid) { hUpdate(); CalculateEnergy(); }

  if(strcmp(component,"total")==0) {
    WriteOvf('h',datatype,precision,filename,realname,note);
    return;
  }

  if(strcmp(component,"demag")==0) {
    if(DoDemag) {
      WriteOvf('o',datatype,precision,filename,realname,note,
        (const ThreeVector * const *)hdemag);
    } else {
      for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) h1[i][k].Set(0.,0.,0.);
      WriteOvf('o',datatype,precision,filename,realname,note,
        (const ThreeVector * const *)h1);
    }
    return;
  }

  // Calculate field component on the fly, and stick into h1
  if(strcmp(component,"anis")==0 || strcmp(component,"anisotropy")==0) {
    for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
      // 0-thickness sites have 0 anis field
      if(m[i][k].GetThickness()!=0.0)
        h1[i][k]=(m[i][k].*MagElt::AnisotropyField)();
      else
        h1[i][k].Set(0.,0.,0.);
    }
  } else if(strcmp(component,"applied")==0
            || strcmp(component,"zeeman")==0) {
    for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
      hApplied->GetLocalhField(i,k,h1[i][k]);
      ConvertXyzToXzy(h1[i][k]);
    }
  } else if(strcmp(component,"exchange")==0) {
    for(i=0;i<Nx;i++) for(k=0;k<Nz;k++) {
      // 0-thickness sites have 0 exchange field
      if(m[i][k].GetThickness()!=0.0)
        h1[i][k]=m[i][k].CalculateExchange();
      else
        h1[i][k].Set(0.,0.,0.);
    }
  } else {
    Message("Error in Grid2D::WriteHComponent; "
            "Unknown field component: %s",component);
    errorcode=1;
    return;
  }

  // Write out h1
  WriteOvf('o',datatype,precision,filename,realname,note,
        (const ThreeVector * const *)h1);
}

void WriteGrid2Dm(double** dat,void* m,int xdim,int ydim,int xyz)
{
  int i,k;
  int component=0,sign=1;
  switch(xyz) // Adjust for differences in component ordering
    {
    case 0: component=0;  sign= 1; break;
    case 1: component=2;  sign= 1; break;
    case 2: component=1;  sign=-1; break;  // Actually, this sign shouldn't
      // matter because it gets reversed back in FillGrid2Dh()
    default: 
      PlainError(1,"Error in WriteGrid2Dm: Illegal component request: %d",
		 xyz);
      break;
    }
  MagElt* mptr;
  // In-plane component. Include thickness adjustment.
  for(i=0;i<xdim;i++) for(k=0,mptr=((MagElt**)m)[i];k<ydim;k++,mptr++)
    dat[i][k]=sign*(mptr->GetThickness())*((*mptr)[component]);

#ifdef EDGE_ADJUST
/**/ // Soften demag on edges
    for(k=0;k<ydim;k++) { 
      dat[0][k]*=EDGE_ADJUST; dat[xdim-1][k]*=EDGE_ADJUST;
    }
    for(i=1;i<xdim-1;i++) {
      dat[i][0]*=EDGE_ADJUST; dat[i][ydim-1]*=EDGE_ADJUST;
    }
#endif // EDGE_ADJUST
}

void FillGrid2Dh(double** dat,void* h,int xdim,int ydim,int xyz)
{
  int i,k;
  int component=0,sign=1;
  switch(xyz) // Adjust for differences in component ordering
    {
    case 0: component=0; sign= 1; break;
    case 1: component=2; sign= 1; break;
    case 2: component=1;  sign=-1; break;  // Actually, this sign shouldn't
            // matter because it gets reversed back in FillGrid2Dh()
    default: PlainError(1,"Error in FillGrid2Dm: Illegal component request: %d",xyz);
            break;
    }

#ifdef EDGE_ADJUST
/**/ // Soften demag on edges
    for(k=0;k<ydim;k++) { 
      dat[0][k]*=EDGE_ADJUST; dat[xdim-1][k]*=EDGE_ADJUST;
    }
    for(i=1;i<xdim-1;i++) {
      dat[i][0]*=EDGE_ADJUST; dat[i][ydim-1]*=EDGE_ADJUST;
    }
#endif // EDGE_ADJUST
    for(i=0;i<xdim;i++) for(k=0;k<ydim;k++)
      ((ThreeVector**)h)[i][k][component]=sign*dat[i][k];
}

#ifdef BDRY_CODE
void Grid2D::FillBdryList()
{
#ifdef ANIS_BDRY_ADJUSTMENT
  // Constants used in setting up surface anisotropy
  const double sqrthalf = 1./sqrt(2.);
  static double        left[3] = { -1, 0,  0 };
  static double  bottomleft[3] = { -sqrthalf,0,-sqrthalf };
  static double      bottom[3] = {  0, 0, -1 };
  static double bottomright[3] = {  sqrthalf,0,-sqrthalf };
  static double       right[3] = {  1, 0,  0 };
  static double    topright[3] = {  sqrthalf,0, sqrthalf };
  static double         top[3] = {  0, 0,  1 };
  static double     topleft[3] = { -sqrthalf,0, sqrthalf };
  static double        zero[3] = {  0, 0,  0 };
  double K1norm = EdgeK1/(mu0*OC_SQ(Ms));
#endif // ANIS_BDRY_ADJUSTMENT

  bdry.Free();
  for(int i=0;i<Nx;i++) for(int k=0;k<Nz;k++) {
    // Mark bits according to the following schematic:
    //
    //                                2 4 7
    //                                1 x 6
    //                                0 3 5
    //
    // where a set bit indicates that the thickness at that position is
    // less than the thickness in the center.  Thus no bits set mean
    // the center cell is (effectively) an interior cell.  Thickness off
    // the edge of the grid (i<0, i>=Nx, etc.) are considered to be zero.
    double basethick=m[i][k].GetThickness();
    if(basethick==0.0) continue; // Don't count exterior cells
    int bitsum=0,bit=1;
    for(int i1=-1;i1<=1;i1++) {
      int i2=i+i1;
      for(int k1=-1;k1<=1;k1++) {
        if(i1==0 && k1==0) continue;
        int k2=k+k1;
        double thick=0.0;
        if(i2>=0 && i2<Nx && k2>=0 && k2<Nz)
          thick=m[i2][k2].GetThickness();
        if(basethick>thick) bitsum+=bit;
        bit*=2; 
      }
    }
    if(bitsum!=0) {
      // Bdry elt found!
#    ifndef ANIS_BDRY_ADJUSTMENT
      bdry.Append(BdryElt(i,k,bitsum));
#    else // ANIS_BDRY_ADJUSTMENT
      double *tmpptr=NULL;
      int ni=0,nk=0; // Neighbor in across bdry in normal direction
      switch(bitsum)
        {
        case MagNbhdLeft:        tmpptr=left;        ni=-1;        break;
        case MagNbhdBottomLeft:  tmpptr=bottomleft;  ni=-1; nk=-1; break;
        case MagNbhdBottom:      tmpptr=bottom;             nk=-1; break;
        case MagNbhdBottomRight: tmpptr=bottomright; ni= 1; nk=-1; break;
        case MagNbhdRight:       tmpptr=right;       ni= 1;        break;
        case MagNbhdTopRight:    tmpptr=topright;    ni= 1; nk= 1; break;
        case MagNbhdTop:         tmpptr=top;                nk= 1; break;
        case MagNbhdTopLeft:     tmpptr=topleft;     ni=-1; nk= 1; break;
	case MagNbhdInterior:    tmpptr=NULL;        break;
        default:                 tmpptr=NULL;        break;
        }
      if(tmpptr!=NULL) {
        // Good boundary surface.   NOTE: All elements of "bdry"
        // have thick!=0. (See "Don't count exterior cells" remark
        // above.)
        double ngbrthick=0.;
        ni+=i; nk+=k;
        if(0<=ni && ni<Nx && 0<=nk && nk<Nz)
          ngbrthick=m[ni][nk].GetThickness();
        if(basethick>ngbrthick) {
          // Put surface anisotropy on thicker cell only.  This check
          // is done for safety only; if it fails then the bdry was
          // not interpreted properly.
          bdry.Append(BdryElt(i,k,bitsum,
                              K1norm*(basethick-ngbrthick)/basethick,
                              Vec3D(tmpptr)));
        } else {
          PlainError(1,"Error in Grid2D::FillBdryList(): "
                     "Boundary interpretation error at cell (%d,%d)",i,k);
        }
      } else {
        // Boundary surface not well defined
        bdry.Append(BdryElt(i,k,bitsum,0.,Vec3D(zero)));
      }
#    endif // ANIS_BDRY_ADJUSTMENT
    }
  }
}
#endif // BDRY_CODE

#ifdef DEMAG_EDGE_CORRECTION
double Grid2D::GetDemagBdryCorrection()
{
  const double compsize=1./sqrt(2.);
  const double alpha =  0.03;
  const double beta  = -0.05;
  double energy_correction=0.0;
  double arrnorm[3];  ThreeVector edgenorm(arrnorm);
  double arrpara[3];  ThreeVector edgepara(arrpara);
  for(BdryElt *bep=bdry.GetFirst();bep!=NULL;bep=bdry.GetNext()) {
    switch(bep->nbhdcode)
      {
      case MagNbhdBottomLeft:  edgenorm.Set(-compsize,0,-compsize); break;
      case MagNbhdBottomRight: edgenorm.Set( compsize,0,-compsize); break;
      case MagNbhdTopRight:    edgenorm.Set( compsize,0, compsize); break;
      case MagNbhdTopLeft:     edgenorm.Set(-compsize,0, compsize); break;
      default:                 edgenorm.Set(0,0,0);                 break;
      }
    edgepara.Set(edgenorm[2],0,-edgenorm[0]);
    MagElt& mer=m[bep->i][bep->k];
    double dotnorm = mer * edgenorm;
    double dotpara = mer * edgepara;
    energy_correction += (dotnorm*dotnorm*alpha +
                          dotpara*dotpara*beta)*mer.GetThickness();
  }
  return energy_correction;
}
#endif // DEMAG_EDGE_CORRECTION
