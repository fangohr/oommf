/* FILE: magelt.h          -*-Mode: c++-*-
 *
 * This file contains class declarations for micromagnetic element classes.
 *
 */
/* Revision history:
 * 16-Jul-00: Cut "thickness" width from double to float, and
 *            introduced out-of-plane self-demag thickness correction
 *            coefficient Ny_correction (also float).
 *  5-May-00: Changed "moment" member variable to "thickness"
 *            which is a more accurate description of the new
 *            use of this value.
 *  2-May-00: Changed anisotropy coefficient back to
 *                 anisotropy_coef = K1/(mu0*OC_SQ(Ms));
 *            Storing moment inside anis_coef turns out to
 *            be a bad idea because if moment is set to 0,
 *            then anis_coef gets adjusted to 0, so if later
 *            moment is set to something non-zero we can't
 *            recover the proper value of anis_coef.  This type
 *            of moment changing behavior occurs in the new multi-grey
 *            level geometry mask file routine.  We could outlaw
 *            this behavior, but the initialization code is confusing
 *            enough as it is.  -mjd
 * 27-Apr-00: Added thickness adjustment to exchange term.
 * 18-Apr-00: Renamed anisotropy_coef to anis_coef, and changed it to
 *                 anis_coef = K1*moment/(mu0*OC_SQ(Ms));
 *            instead of
 *                 anisotropy_coef = K1/(mu0*OC_SQ(Ms));
 *            Completed removal of UNIT_MAG code.
 *            Removed energy offsets from anisotropy terms.
 * 21-Nov-97: Added individual link weights.
 *  7-Apr-97: Moved anisotropy vectors from static member to
 *            instantiation specific pointers into external array,
 *            with separate initialization routines.
 *  9-Jul-96: Added support for max angle difference determination
 * 13-May-96: Moved EXCHANGE_COS, EXCHANGE_4NGBR_ANGLE, and
 *            MAGNGBRCNT #defines from magelt.h to magelt.cc
 * 4-May-96: Added support for non-unit magnetizations.
 *
 * 24-June-96: Moved memory for ThreeVector inside MagElt structure
 *
 * 30-June-96: Added Array2D template code
 *
 *  1-July-96: Incorporated MagElt subclasses into MagElt (thereby
 *             removing the subclasses).  Currently this is the best
 *             way I can figure to get the Array2D code to work.
 */

#ifndef _MAGELT_H
#define _MAGELT_H

#include <cstdio>
#include "constants.h"
#include "threevec.h"

/* End includes */

#define MAXMAGNGBRCNT 8 // Must be at least as large as MAGNGBRCNT
                        // #defined in magelt.cc

// #define EXCHANGE_THICKNESS_ADJ(m1,m2) OC_MIN(m1,m2)
#define EXCHANGE_THICKNESS_ADJ(m1,m2) ((m1+m2)==0.?0.:2*m1*m2/(m1+m2))
/// EXCHANGE_THICKNESS_ADJ is exchange link weight adjustment factor.
/// The inputs m1 & m2 should each be in [0,1].  This function should
/// have the properties outlined in NOTES II, 1-Feb-2001, p78, namely
///   1) EXCHANGE_THICKNESS_ADJ(m1,m2) = EXCHANGE_THICKNESS_ADJ(m2,m1)
///   2) min(m1,m2) <= EXCHANGE_THICKNESS_ADJ(m1,m2) <= 2m1.m2/(m1+m2)


//////////////////////// MagElt Declaration //////////////////////////

class MagElt;  // Forward declaration
enum MagEltAnisType {meat_unknown=-1, meat_cubic=0, meat_gencubic=1,
		     meat_uniaxial=2};

struct MagEltLink {
  MagElt* node;
  double   weight;
};

class MagElt : public ThreeVector {
protected:
  float thickness;  // Effective thickness, relative to nominal value.
  /// This is intended to model thickness relative to nominal thickness,
  /// or perhaps density/proportion of magnetic specie in a matrix.
  /// NOTE: Code may assume 0<=thickness<=1.
  float Ny_correction;  // Out-of-plane self demag correction to handle
  /// variation in cell aspect if thickness != 1.  This correction assumes
  /// the cell is a flat plate of height 'thickness'; in particular, as
  /// thickness ->0, the correction -> infinity.
  /// ALSO:  Our cells have square in-plane cross-section, so Nx and
  /// Nz are equal.  If we use the relation Nx + Ny + Nz = 1, then it
  /// follows that the Nx and Nz correction corresponding to Ny_correction
  /// is  0.5 * (1 - thick - Ny_correction).

  int    ngbrcnt;  // May differ between MagElt's!
  MagEltLink ngbr[MAXMAGNGBRCNT];
  double vecarr[VECDIM];
  static double Ms,mesh_size;
  static double exchange_stiffness,exchange_coef;
  double anisotropy_coef; // = K1/(mu0*OC_SQ(Ms))
  ThreeVector AnisDirA,AnisDirB,AnisDirC;  // Anisotropy directions.  Would
  /// really like these to be 'const', but don't see a reasonable way to do
  /// it without introducing a "constant vector" type, i.e., a 3 vector with
  /// a constant double*.
  double AngleBetween(const MagElt& other) const;
  /// Returns angle between *this and other
  /// NOTE: This routine _ASSUMES_ MagElt's are UNIT VECTORS.  If this is
  ///       not the case then this routine must be modified.
  void         MagInit(void); // Called by initializers

  // Anisotropy Energy/Field functions
  static MagEltAnisType meat;  // Don't count on this being a static member
  double UniaxialAnisotropyEnergy();
  double CubicAnisotropyEnergy();
  double GenCubicAnisotropyEnergy();
  const ThreeVector& UniaxialAnisotropyField();
  const ThreeVector& CubicAnisotropyField();
  const ThreeVector& GenCubicAnisotropyField();
 
  // Typedef's for pointers to exchange energy and exchange field functions
  typedef double (MagElt::*MEAEFP)(void);
  typedef const ThreeVector& (MagElt::*MEAFFP)(void);

public:
  static MEAEFP AnisotropyEnergy;
  static MEAFFP AnisotropyField;

  void         SetK1(double new_K1) {
    // NOTE: Ms must be set *BEFORE* calling this function!!!
    anisotropy_coef=new_K1/(mu0*OC_SQ(Ms));
  }
  void         SetThickness(float val) {
    thickness=val;
  }
  void         SetNyCorrection(float corr) {
    Ny_correction=corr;
  }
  // Note: I would like to make the above Set* function private, and allow
  //       access to only selected member functions of Grid2D, but I can't
  //       figure out any good way to do this.  It seems that Grid2D must
  //       be declared before MagElt so that I can use, for example,
  //               friend void Grid2D::InitEllipse(void)
  //       but that is at best unnatural and in any event too easy to break.
  MagElt(void);
  ~MagElt(void);
  inline double&  operator[](int n);
  inline const double&  operator[](int n) const;
  inline void SetDirection(const ThreeVector& u);  // |u| should be 1.0
  static int   GetMaxNgbrcnt(void);
  static void  SetupConstants(double new_Ms,
			     double new_exchange_stiffness,
			     double new_mesh_size,MagEltAnisType new_meat);

  // The next 3 routines set/return pointers to the underlying 'double'
  // arrays inside AnisDirA, AnisDirB, and AnisDirC.  These are inherently
  // dangerous to use, and should be accessed *ONLY* by 'class Grid2D'
  // initialization routines!!!
  void QuickInitAnisDirs(double* anis_dir_a,
			 double* anis_dir_b=NULL,
			 double* anis_dir_c=NULL);  // Init w/o checks.
  void InitAnisDirs(double* anis_dir_a,
		    double* anis_dir_b=NULL,
		    double* anis_dir_c=NULL);
  void GetAnisDirs(double* &anis_dir_a,double* &anis_dir_b,
		   double* &anis_dir_c) const;

  void         SetupNeighbors(int new_ngbrcnt,MagEltLink* myngbrs);

  void         CopyData(const MagElt& mother);
  // Copies vector, thickness and K1 info
  void         DumpMag(double *outarr);  // Dumps thickness*arr[]
  double       GetMinNeighborDot(void) const;  // Returns minimum value
  // of dot product of moment *direction* with that of neighbors.

  double       GetK1(void) const {
    return anisotropy_coef*mu0*OC_SQ(Ms);
  }
  float       GetThickness(void) const { return thickness; }
  float       GetNyCorrection(void) const { return Ny_correction; }

  int          IsBoundary(void) const; // Returns 1 if
  /// ngbrcnt<MAGNGBRCNT, or if any of its neighbors have
  ///     | thickness - this->thickness |
  ///              > 1e-8 * ( thickness + this->thickness )
  /// NOTE: Returns true elts on bdry, both inside *and* outside.

  ThreeVector& PerpComp(const ThreeVector& v) const; // Returns v-(v*m)m

  ThreeVector& CalculateExchange(void) const;
  double       CalculateExchangeEnergy(void) const;

  void         Perturb(double max_mag);
};


inline double& MagElt::operator[](int n)
{
#ifdef CODE_CHECKS
  if(n<0 || n>=VECDIM)
    PlainError(1,"Error in MagElt::operator[]: %s",ErrBadParam);
#endif // CODE_CHECKS
  return vecarr[n];
}

inline void MagElt::SetDirection(const ThreeVector& u)
{ // It is the responsibility of the calling routine to normalize
  // the direction (e.g., by insuring |u|=1.0.).
  for(int i=0;i<VECDIM;i++) vecarr[i]=u[i];
}

inline const double& MagElt::operator[](int n) const
{
#ifdef CODE_CHECKS
  if(n<0 || n>=VECDIM)
    PlainError(1,"Error in MagElt::operator[] const: %s",ErrBadParam);
#endif // CODE_CHECKS
  return vecarr[n];
}

#endif // _MAGELT_H
