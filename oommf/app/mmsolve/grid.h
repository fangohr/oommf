/* FILE: grid.h           -*-Mode: c++-*-
 *
 * This file contains class declarations for micromagnetic grid classes.
 *
 */

#ifndef _GRID_H
#define _GRID_H
#include <cstring>
#include <cstdio>

#include "nb.h"

#include "demag.h"
#include "magelt.h"
#include "maginit.h"
#include "mifio.h"
#include "threevec.h"
#include "zeeman.h"

/* End includes */

#if (OC_SYSTEM_TYPE == OC_UNIX)
#define STOPWATCH_ON  // #define for routine timing
#endif

#undef DEMAG_EDGE_CORRECTION // Enable for experimental demag edge corrections
#define ANIS_BDRY_ADJUSTMENT  // Enable for experimental surface anisotropy

#if defined(DEMAG_EDGE_CORRECTION) || defined(ANIS_BDRY_ADJUSTMENT)
# define BDRY_CODE
#endif

#define COOKIELENGTH 8     // Length of file identifier (magic cookie) for
                           // G2DFil files.

#define PERTURBATION_SIZE 0.04 // Magnitude (relative to 1) of random pertur-
    // bations to magnetization; Used in magnetization initialiation routines.
    // The arctan of this is the maximum perturbation angle.

// Grid (mesh) file magic cookie string
extern const char GridFileMagic02[8];

#ifdef BDRY_CODE
// Demag edge correction support
struct BdryElt {
public:
  int i,k;       // Grid offset
  int nbhdcode;  // Local neighborhood type
#ifndef ANIS_BDRY_ADJUSTMENT
  BdryElt() : i(0), k(0), nbhdcode(0) {}  // Needed for array inits
  BdryElt(int newi,int newk,int newnbhdcode) :
    i(newi), k(newk), nbhdcode(newnbhdcode) {}
#else // ANIS_BDRY_ADJUSTMENT
  double anisotropy_coef; // K1*thickness_change/(mu0*OC_SQ(Ms)*thickness)
  // NOTE: There is no code in place to change these boundary
  // anisotropy_coef's in the event of change in the thickness of
  // the underlying magelt.  Therefore, the magelt's thickness must
  // be set before setting up the BdryElt's, and must not be
  // changed.  This is reasonable, because if the thicknesses change
  // then potentially the boundaries are also changed.
  Vec3D normal_dir; // Unit vector that is normal to boundary surface,
  /// or 0 if boundary surface is not well defined at this point.
  BdryElt() : i(0), k(0), nbhdcode(0), anisotropy_coef(0.) {}  // For arrays
  BdryElt(int newi,int newk,int newnbhdcode,
          double newacoef, const Vec3D& newdir) :
    i(newi), k(newk), nbhdcode(newnbhdcode),
    anisotropy_coef(newacoef), normal_dir(newdir) {}
#endif // ANIS_BDRY_ADJUSTMENT
};
#endif // BDRY_CODE

//////////////////////// Grid2D Declaration //////////////////////////
class Grid2D {
private:
  int randomizer_seed;  // Set from mif file. 0 => random seed.
  double xmax,ymax,zmax,cellsize; // Dimensions in meters
  int Nx,Nz;            // Mesh size
  double thickness_sum; // ==Nx*Nz if all spins are unit spins
  double energy;        // Mesh energy density
  double exch_energy,anis_energy,demag_energy,zeeman_energy;  // All
  // these energies set by CalculateEnergy()
  Nb_Array2D<MagElt>m;     // Magnetization vector array
  Nb_Array2D<MagElt>m0;
  Nb_Array2D<MagElt>m1;
  Nb_Array2D<MagElt>m2;
  Nb_Array2D<OC_REALWIDE>energy_density;
  Nb_Array2D<OC_REALWIDE>energy_density1;
  MagEltAnisType magelt_anistype; // MagElt Anisotropy type
  ThreeVector **anisa,**anisb,**anisc;
  int hValid; // ==0 => h is invalid for current m
  ThreeVector **h;      // h field vector array
  ThreeVector **h0,**h1;  // Temporary h field vector arrays
  ThreeVector **hdemag; // H demag field vector array
  Zeeman *hApplied; // Reduced applied field, constant across grid
  /// NOTE: hApplied stores, works and returns data in the OOMMF
  ///  xy+z coord system, as opposed to the xz+y coords used in
  ///  Grid2D.  All accesses to this class that are used internally
  ///  should pass the result through to ConvertXyzToXzy for the
  ///  appropriate conversions.

  ThreeVector **torque; // Torques for updating m
  ThreeVector **torque0,**torque1;
  int torqueValid; // ==0 => torque is invalid for current m and h
  int energyValid; // ==0 => energy is invalid for current m and h
  /// BIG NOTE: *Everywhere* in this code, "torque" refers to the RHS
  /// of the solved ODE, i.e., (-1/DampCoef)(mxh)-mx(mxh).  The public
  /// interface provides "|mxh|," which should be free of confusion.
  /// If DoPrecess is false, then DampCoef is effectively infinite,
  /// i.e., the (mxh) term is ignored.

  double** ADemagCoef;   // Coefficient matrix for demag calculations
  double** CDemagCoef;   // Coefficient matrix for demag calculations
  double ADemag(int i,int k);  // Returns element A(i,j) of A demag matrix
  double CDemag(int i,int k);  // Returns element C(i,j) of C demag matrix
  double SelfDemag;      // Self-demag coefficient
  double exchange_stiffness,Ms,K1;  // Material constants.
  // K1 is a nominal value (J/m^3) that may vary between mesh points.

  // Edge anisotropy
  int DoBdryAnis;    // If !=0, then calculate surface anisotropy
  double EdgeK1;  // Uniaxial surface anisotropy coef.

  // ODE parameters
  int step_total,reject_total,reject_energy_count,reject_position_count;
  /// Step rejection counts.  The first should not be larger than the
  /// sum of the rest. These should be reset to 0 whenever hUpdateCount is. 
  /// NOTE: step_total is the total number of steps *attempted*.
  double GyRatio;  // Gyromagnetic ratio
  OC_BOOL DoPrecess;    // If true, enable precession component in Torque.
  double DampCoef;   // Damping coefficient
  // dm/dt = (-GyRatio.Ms)(mxh)-(DampCoef.GyRatio.Ms)(mx(mxh))
  //  This program actually solves RHS=(-1/DampCoef)(mxh)-mx(mxh)
  //  Convert StepSize (below) to actual time (in seconds) via
  //              time_step = StepSize/(DampCoef.GyRatio.Ms)
  //   cf. Grid2d::GetTimeStep()
  double StepSize;
  double StepSize0; // Stepsize on previous pass
  double NextStepSize; // Suggested stepsize for next pass
  double InitialStepSize;  // Very first stepsize
  void Allocate(int xcount,int zcount);
  void Deallocate();
  void Initialize(MIF &mif,MagEltAnisType new_magelt_anistype);
  ArgLine MagInitArgs;  // Initial magnetization specification
  int FindNeighbors(int nsize,Nb_Array2D<MagElt> &me,MagEltLink men[],
		    int i,int k);

  ThreeVector& CalculateDemag(int i,int k); // Demag. vector at (i,j)
  void CalculateEnergy();
  double CalculateEnergyDifference();
  int hUpdateCount; // Count of calls to hUpdate
  void hUpdate(); // Calculates h (=h_effective) from m
  void hFastUpdate(); // Same as hUpdate, but does not recalculate
                          // hdemag.
  int ODEIterCount; // ODE Iteration count for current segment.  Reset by
                    // ResetODE(), incremented by StepODE;  Used by
                    // multistep methods to determine if there is sufficient
                    // past history on this segment.
  void CalculateTorque(Nb_Array2D<MagElt> &m_in,
		       ThreeVector** &h_in,
		       ThreeVector** &t_out);

  // Interface to external/internal demag calculation routines
  DF_FuncInfo* ExternalDemag; // Set "->calc" to NULL to use internal routine
  // DF_Calc routines make use of externals DF_Export_m WriteGrid2Dm and
  // DF_Import_h FillGrid2Dh;
  char **dfparams; // Demag field routine parameter list, only used
                    // by Grid2D::Grid2D(char *filename)
  void InternalDemagCalc(); // Internal, brute-force routine
  int DoDemag;  // 0 => no demag.

  // Integration routines
  double GetdEdt(); // Returns torque*h
  void StepEuler(double minstep,double maxtorque,
		 double &next_stepsize);    // 1st order Euler
  void StepPredict2(double minstep,double maxtorque,
		    double &next_stepsize); // 2nd order predictor
  void IncRungeKutta4(Nb_Array2D<MagElt> &m_in,
		      ThreeVector** &h_in,
		      double step,
		      Nb_Array2D<MagElt> &m_out);
  void StepRungeKutta4(double minstep,double maxtorque,
		       double &next_stepsize,int forcestep=0);
  /// 4th order Runge-Kutta

  // Utility routines
  void ConvertXyzToXzy(ThreeVector &vec); // xyz := OOMMF coordinate system,
  void ConvertXzyToXyz(ThreeVector &vec); // xzy := 2D solver coords.
  /// Corresponding functions for the Vec3D class are member functions
  /// of that class.
#ifdef STOPWATCH_ON
  Nb_StopWatch sw_exch,sw_anis,sw_demag,sw_zeeman;
  Nb_StopWatch sw_solve_total,sw_proc_total;
#endif // STOPWATCH_ON
  int InitMagnetization();
  double GetMaxTorque(); // Returns maximum modulus of "torque," which
  /// is defined to be (-1/DampCoef)*(mxh) - mx(mxh).
  double GetMxH(double torque);  // Converts "torque" back to |mxh|.
  double ConvertTimeToStepSize(double time) const;
  /// Given import time in seconds, returns the corresponding StepSize.

#ifdef BDRY_CODE
  // Demag edge correction support
  Nb_List<BdryElt> bdry;
  void FillBdryList();
#ifdef DEMAG_EDGE_CORRECTION
  double GetDemagBdryCorrection();
#endif // DEMAG_EDGE_CORRECTION
#endif // BDRY_CODE

  void DumpDemagEnergyPpm(const char* filename);

public:
  // Birth and death routines
  // Standard constructor routine
  Grid2D(MIF mif);
  ~Grid2D();
  void Reset();  // Calling routine is responsible for resetting hApplied

  void InitRectangle(Nb_DString &);
  void InitEllipse(Nb_DString &);
  // Zero's thickness outside ellipse defined by Nx,Nz
  void InitEllipsoid(Nb_DString &);
  // Simulates an inscribed ellipsoid (Nx x Nz x thickness)
  void Mask(Nb_DString &a);
  void InitOval(Nb_DString &a);
  void InitPyramid(Nb_DString &transition_width);
  void SetBoundaryThickness(double thick);
       // Sets all boundary magelt's thickness magnitudes to thick
  void RoughenBoundaryThickness();
       // Multiplies each boundary thickness (separately) by an amount
       // drawn from a uniform distribution on [0,1].

  // Processing routines
  void Perturb(double max_mag);  // Randomly perturb all m by <max_mag
  double StepODE(double mintimestep,double maxtimestep,int& errorcode);
  /// Do one iteration. If maxtimestep>0, then resulting step will
  ///  satisfy delta t < maxtimestep.   Returns max m x h value.
  void MarkhInvalid() { hValid=torqueValid=0; }
  void ResetODE() { ODEIterCount=0; MarkhInvalid(); }
  void SetAppliedField(const Vec3D &Bfield,int fieldstep) {
    hApplied->SetNomFieldVec3D(Ms,Bfield,fieldstep);
    ResetODE();
  }

  // Information routines
  void PrintUsageTimes(FILE *fptr=stderr); // Dumps CPU & elapsed times.
  void GetUsageTimes(Nb_DString& result_str);
  /// Same as PrintUsageTimes, but to Nb_DString.
  void GetStepStats(int& _step_total,int& _reject_total,
		    int& _reject_energy_count,
		    int& _reject_position_count) {
    _step_total=step_total;
    _reject_total=reject_total;
    _reject_energy_count=reject_energy_count;
    _reject_position_count=reject_position_count;
  }

  double GetCellSize() { return cellsize; }
  double GetMaxNeighborAngle(); // Returns maximum angle (in radians)
  // between neighboring spin directions in the grid.
  double GetEnergyDensity(); // Returns average energy density, in J/m^3
  void GetEnergyDensities(double& exch,double& anis,double& demag,
			  double& zeeman,double& total);
  /// Returns average energy densities, in J/m^3
  double GetFieldComponentNorm(const OC_CHAR* component,double thick,
			       int& errorcode);

  double GetMaxMxH(); // Returns maximum value of |m x h|.
  double GetStepSize() const { return StepSize; }
  double GetTimeStep() const; // Returns size of last step, in seconds
  void GetDimens(int& xcount,int& zcount) { xcount=Nx; zcount=Nz; }
  int GethUpdateCount() { return hUpdateCount; }

  // File output routines
  void BinDumpm(const OC_CHAR *filename, const OC_CHAR *note=NULL);
  						 // Reduced magnetization m
                                                 // in VecFil format
  void BinDumph(const OC_CHAR *filename, const OC_CHAR *note=NULL);
  						 // Reduced field h
                                                 // in VecFil format
  void WriteOvf(OC_CHAR type,
		const OC_CHAR *datastyle,const OC_CHAR *precision,
		const OC_CHAR *filename,
		const OC_CHAR *realname=NULL,const OC_CHAR *note=NULL,
		const ThreeVector * const *arr=NULL);
  /// Writes an ovf file, of either m (if type=='m') or h (if type=='h'),
  /// or the data in arr (if type=='o' (for "other")).  The datastyle
  /// import should be either "binary" or "text".  If
  /// datastyle=="binary", then precision should be either "4" or "8".
  /// If datastyle=="text", then precision should be a printf-style
  /// format string.  If datastyle is a NULL pointer or an empty string,
  /// then the default is "binary".  If precision is a NULL pointer or an
  /// empty string, then the default is "4" if datastyle is "binary", or
  /// "%.15g" if datastyle is "text".  In all cases the output is 1)
  /// converted from the internal xz+y coords to the external xy+z coords
  /// system, and 2) assumed to be scaled relative to Ms.

  void WriteFieldComponent(const OC_CHAR *component,
			   const OC_CHAR *datastyle,const OC_CHAR *precision,
			   const OC_CHAR *filename,const OC_CHAR *realname,
			   const OC_CHAR *note,int &errorcode);
  /// Invokes WriteOvf to write out individual field components.

  // The following routines present a xy+z coords interface, performing
  // the conversions to/from the internal xz+y coords as necessary.
  void GetNearestGridNode(double x,double y,int &i,int &j);
  void GetRealCoords(int i,int j,double &x,double &y,double &z);
  double GetMs() const { return Ms; }
  void GetAverageMagnetization(ThreeVector& AveMag);
  Vec3D GetAveMag(); // Same as GetAverageMagnetization(), but returns Vec3D.
  Vec3D GetAveH();   // Returns average H (across non-zero thicknesses).
#ifdef CONTRIB_SPT001
  double GetResistance(); // CONTRIBUTED by Stephen Thompson,
  /// <mbgegspt@afs.mcc.ac.uk> 15-June-1998.
#endif // CONTRIB_SPT001

  Vec3D GetNomBApplied() { return hApplied->GetNomBField(); }
  void Dump(OC_CHAR *filename,OC_CHAR *note=NULL); // Dump of grid data in ASCII

  // The next routine is intended for internal use only, and therefore
  // the output data is kept in the internal xz+y coords system.
  void GetMag(double** arr); // Fills arr with magnetizations.
};

DF_Export_m WriteGrid2Dm;
DF_Import_h FillGrid2Dh;

#endif // _GRID_H
