/* FILE: mifio.h           -*-Mode: c++-*-
 *
 * Header file for I/O routines involving standardized
 * Micromagnetic Input Form (MIF) data.
 */

#ifndef MIFIO_H
#define MIFIO_H

#define MIFVECDIM 3

#include "nb.h"

#include "zeeman.h"

/* End includes */

struct ArgLine {
public:
  int argc;
  char **argv;
  ArgLine() { argc=0; argv=NULL; }
  ArgLine(const ArgLine& other);
  void Reset(); // Frees allocated space and sets argc=0, argv=NULL.
  void Set(int new_argc,const char * const *new_argv);
  void Set(int new_argc,const Nb_DString *new_strv); // Dups the strings
  /// into private memory (argv[0]) and sets pointers to the dup'ed strings.
  void Set(const char* argstr); // As above, but with an unparsed string.
  /// Also substitutes '/' for '\', after which string must be a proper
  /// Tcl list.
  void Get(Nb_DString &argstr); // Copies args back into a string.
  ArgLine& operator=(const ArgLine &rhs);
  ~ArgLine() { Reset(); }
};

enum MifAnisotropyType { mif_uniaxial, mif_cubic, mif_anis_notset };
enum MifGeometryType { mif_rectangle, mif_ellipse, mif_ellipsoid,
		       mif_oval, mif_pyramid, mif_mask, mif_geom_notset };
enum MifDemagnetizationType { mif_slowpipe, mif_fastpipe, mif_2dslab,
			      mif_3dslab, mif_3dcharge, mif_3dconstmag,
			      mif_nodemag, mif_demag_notset };

// Micromagnetic Input Form
class MIF {
private:
  Nb_DString InName; // Name of input file
  // Material parameters
  double Ms;    // Saturation magnetization in A/m;
  double A;     // Exchange stiffness in J/m;
  double K1;    // First anisotropy constant, in J/m^3;
  double EdgeK1;  // Edge surface anisotropy constant, in J/m^3
  MifAnisotropyType AnisType;
  Vec3D AnisDir1,AnisDir2; // Primary and secondary anisotropy directions.
  ArgLine AnisInitArgs; // Anis init routine name and parameters.
  OC_BOOL DoPrecess; // If true, include precession term in Torque.
  double GyRatio;    // Gyromagnetic ratio; default is 2.21e5 m/A.s.
  double DampCoef;      // The damping coefficient, dimensionless.  Used
  /// in conjuction with the gyromagnetic ratio.  The damping
  /// coefficient may be obtained via FMR studies as
  ///
  ///    DampCoef=(Delta H).GyRatio/(2.(fmr_driving_frequency)).
  ///
  /// 'Delta H' here is the FMR line width.  In the literature,
  /// GyRatio is denoted by \gamma, fmr_driving_frequency is \omega_0,
  /// and DampCoef is often written \alpha or \alpha/(1+\alpha).
  ///   Realistic values for DampCoef are in the range [.004,.15], but for
  /// fast convergence of the ODE, DampCoef=0.5 seems close to optimal.
  ///
  /// LLG:
  ///       dm/dt = (-GyRatio.Ms)(mxh)-(DampCoef.GyRatio.Ms)(mx(mxh))
  ///
  /// Grid2D actually solves RHS=(-1/DampCoef)(mxh)-mx(mxh).
  /// The routine Grid2D::GetTimeStep() does the conversion from StepSize
  /// to actual time step in seconds, via
  ///             time_step = StepSize/(DampCoef.GyRatio.Ms)

  // Demag specifications
  MifDemagnetizationType DemagType;
  // Part geometry
  double PartWidth;   // Nominal part width in meters.
  double PartHeight;  // Nominal part height in meters.
  double PartThickness; // Part thickness in meters.  Required for
                        // some DemagTypes.
  double CellSize;    // Cell size in meters.  May affect PartWidth and
                      // PartHeight.
  MifGeometryType GeomType;
  Nb_DString GeomTypeParam;  // Shape specific parameter
  // Initial magnetization specification
  ArgLine MagInitArgs; // Mag init routine name and parameters.

  // Experiment parameters
  Nb_DString FieldRangeList;
  ArgLine AppliedFieldArgs;
  Nb_DString DefaultControlPointSpec;  // Replaces ConvergeMxHValue

  // Output Specifications
  Nb_DString OutBaseName;
  Nb_DString OutMagFormat;
  Nb_DString OutTotalFieldFormat;
  Nb_DString OutDataTableFormat;
  int LogLevel;

  // Miscellaneous
  int RandomizerSeed;
  double InitialIncrement;     // in _radians_
  int UserInteractionLevel;
  int UserReportCode;
  double MinTimeStep,MaxTimeStep;
  Nb_DString UserComment;

  // Helper functions
  void Init(void);
  void SetDefaults(void);
  int  ReadLine(FILE* infile,int max_strc,int &strc,Nb_DString* strv);
  void AdjustAnisDirs(void);
  int  ConsistencyCheck(void);

public:
  // NOTE: The built-in copy constructor is used for this class,
  //  so be careful to code instance variables with this in mind!
  MIF(void);
  ~MIF(void);
  void Read(const char* inname,int &errorcode);
  Nb_DString Write(int &errorcode);
  const char* GetOutBaseName() { return OutBaseName.GetStr(); }
  const char* GetMagnetizationOutputFormat() {
    return OutMagFormat.GetStr();
  }
  const char* GetTotalFieldOutputFormat() {
    return OutTotalFieldFormat.GetStr();
  }
  const char* GetDataTableOutputFormat() {
    return OutDataTableFormat.GetStr();
  }
  int GetLogLevel(void) { return LogLevel; }
  const char* GetDemagName(void);
  double GetPartHeight(void)    { return PartHeight; }
  double GetPartThickness(void) { return PartThickness; }
  double GetPartWidth(void)     { return PartWidth; }
  double GetCellSize(void)      { return CellSize; }
  MifAnisotropyType GetAnisType(void) { return AnisType; }
  Vec3D GetAnisDir(int i);
  const char* GetAnisInitName(void) { return AnisInitArgs.argv[0]; }
  ArgLine GetAnisInitArgs() { return AnisInitArgs; }
  OC_BOOL GetDoPrecess() { return DoPrecess; }
  double GetGyRatio() { return GyRatio; }
  double GetDampCoef() { return DampCoef; }
  MifGeometryType GetGeometry(Nb_DString &param)
  { param=GeomTypeParam; return GeomType; }
  ArgLine GetMagInitArgs(void) { return MagInitArgs; }
  void SetMagInitArgs(const char* argstr) { MagInitArgs.Set(argstr); }

  double GetA(void)  { return A; }
  Nb_DString GetDefaultControlPointSpec() {
    return DefaultControlPointSpec; // Replaces GetConvergeMxHValue
  }
  Nb_DString GetFieldRangeList() { return FieldRangeList; }
  double GetInitialIncrement(void) { return InitialIncrement; }
                    // Maximum initial increment in _radians_.
  double GetK1(void) { return K1; }
  double GetEdgeK1(void) { return EdgeK1; }
  double GetMs(void) { return Ms; }
  int GetRandomizerSeed(void) { return RandomizerSeed; }
  int GetUserInteractionLevel(void) { return UserInteractionLevel; }
  int GetUserReportCode(void) { return UserReportCode; }

  ArgLine GetZeemanArgs() { return AppliedFieldArgs; }

  void DumpPartialInfo(FILE* fptr);
  int IsValid() { return !ConsistencyCheck(); }

  double GetMinTimeStep() { return MinTimeStep; }
  double GetMaxTimeStep() { return MaxTimeStep; }
  Nb_DString GetUserComment() { return UserComment; }

  // Input routines
  double SetA(double newA)   { return A =newA;  }
  double SetCellSize(double newcellsize)   {
    return CellSize=newcellsize;
  }
  void SetDefaultControlPointSpec(const char* newspec) {
    DefaultControlPointSpec=newspec; // Replaces SetConvergeMxHValue
  }
  double SetK1(double newK1) { return K1=newK1; }
  double SetEdgeK1(double newEK1) { return EdgeK1=newEK1; }
  double SetMs(double newMs) { return Ms=newMs; }
  void SetOutBaseName(const char* newname) {
    OutBaseName.Dup(newname);
  }
  void SetMagnetizationOutputFormat(const char* newname) {
    OutMagFormat.Dup(newname);
  }
  void SetTotalFieldOutputFormat(const char* newname) {
    OutTotalFieldFormat.Dup(newname);
  }
  void SetDataTableOutputFormat(const char* newname) {
    OutDataTableFormat.Dup(newname);
  }
  void SetDemagType(const char* demagname,int &errorcode);
  double SetPartHeight(double newheight)   {
    return PartHeight=newheight;
  }
  double SetPartThickness(double newthick) {
    return PartThickness=newthick;
  }
  double SetPartWidth(double newwidth)     {
    return PartWidth=newwidth;
  }
  int SetRandomizerSeed(int newseed) {
    return RandomizerSeed=newseed;
  }
  void SetUserComment(const char* newcomment) {
    UserComment.Dup(newcomment);
  }
};

#endif // .NOT. MIFIO_H
