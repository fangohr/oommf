/* FILE: demagtensor.cc            -*-Mode: c++-*-
 *
 * Program to compute demagnetization tensor elements using
 * the OOMMF Oxs classes.
 *
 * STANDALONE NOTE: This program is normally built as part of
 * the standard OOMMF Oxs build process, with full support and
 * dependencies inherent therein.  However, it can also
 * be built using only the oxs/ext/demagcoef.{h,cc} files,
 * using the STANDALONE macro like so:
 *
 *    g++ -std=c++11 -DSTANDALONE=1 -DOC_USE_SSE=1 \
 *         demagtensor.cc ext/demagcoef.cc -lm -o demagtensor
 *
 * For Visual C++ use
 *
 *    cl /GR /EHac /DSTANDALONE=1 /DOC_USE_SSE=1 ^
 *       /Tpdemagtensor.cc /Tpext\demagcoef.cc /Fedemagtensor.exe
 *
 * IMPORTANT NOTE: Normal builds of this file embedded in the OOMMF
 * environment compute the near field tensor using the high-precision
 * Xp_DoubleDouble class.  Standalone builds substitute a simple wrapper
 * of long double as a stand-in for Xp_DoubleDouble, but of course the
 * accuracy is compromised.
 *
 * See '#if STANDALONE / #endif' blocks for additional details.
 */

#if defined(STANDALONE) && STANDALONE==0
# undef STANDALONE
#endif

// By default, MinGW uses I/O from the Windows Microsoft C runtime,
// which doesn't support 80-bit floats.  Substitute MinGW versions for
// printf etc.  This macro must be set before stdio.h is included.
#if defined(STANDALONE) && (defined(__MINGW32__) || defined(__MINGW64__))
# define __USE_MINGW_ANSI_STDIO 1
#endif

#include "ext/demagcoef.h"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cfloat>
#include <cstdio>
#include <cctype>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <fstream>

#include <algorithm>
#include <limits>
#include <string>
#include <cstring>

/* End includes */

// Default settings
const int default_display_digits=17; // == precision
const int default_group_digits=0;    // groups size, 0 == disable grouping
#if STANDALONE
// In standalone setting double-double is not available to compute the
// tensor in the analytic (near field) range.  Bump default error
// upwards so code doesn't try to do entire range with asymptotics
// (which would require massively refinements in the near field).
const OXS_DEMAG_REAL_ASYMP default_maxerror = 1e-12;
#else
const OXS_DEMAG_REAL_ASYMP default_maxerror 
   = 8*std::numeric_limits<OXS_DEMAG_REAL_ASYMP>::epsilon();
#endif
const int default_maxorder = 11;

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

using namespace std;  // Get automatic type overloading on standard
                      // library functions.
void Usage()
{
  fprintf(stderr,
   "Usage: demagtensor [-d #] [-g #] hx hy hz x y z"
   " [-pbc dir W] [-maxerror #] [-maxorder #] [-file Nfile]\n"
   " where -d is the number of digits to display (default %d),\n"
   "       -g is the space delineated digit group size (default %d),\n"
   "       hx hy hz are cell dimensions,\n"
   "       x y z is offset,\n"
   "       dir is periodic direction: x, y, z, or n for none (default=n),\n"
   "       W is window length in periodic direction,\n"
   "       maxerror is requested bound on tensor error (default %g)\n"
   "       maxorder is asymptotic order (5, 7, 9 or 11; default %d)\n"
   " If the '-file Nfile' option is used, then 'x y z' denote range values\n"
   " instead of a single offset and an OVF2 file named Nfile is created\n"
   " containing six tensor values per line (Nxx Nxy Nxz Nyy Nyz Nzz).\n",
          default_display_digits,default_group_digits,
          double(default_maxerror),int(default_maxorder));
  exit(1);
}

void GroupNumberString
(int grpsize,std::string& val)
{ // Expands the scientific (exponential) string representation of val
  // by adding spaces in the mantissa to form numeric groups of width
  // grpsize.
  if(grpsize<1) return; // Do nothing
  std::string tmp;
  int mode = 0;
  int grpcnt = 0;
  for(const char* cptr=val.c_str(); *cptr != '\0'; ++cptr) {
    tmp += *cptr;
    switch(mode) {
    case 0:
      if(*cptr == '.') {
        mode = 1;
        grpcnt = 0;
      }
      break;
    case 1:
      if( *cptr < '0' || '9'< *cptr) {
        mode = 2;
      } else {
        if(++grpcnt == grpsize
           && ('0' <= *(cptr+1) && *(cptr+1) <= '9')) {
          // Worst case cptr+1 points to '\0' string terminator.
          grpcnt = 0;
          tmp += ' ';
        }
      }
      break;
    default:
      break;
    }
  }
  val = tmp;
}

struct TensorParams {
  enum PbcDirection { none, x, y, z } direction;
  OXS_DEMAG_REAL_ASYMP W;
  OXS_DEMAG_REAL_ASYMP maxerror;
  OXS_DEMAG_REAL_ASYMP hx;
  OXS_DEMAG_REAL_ASYMP hy;
  OXS_DEMAG_REAL_ASYMP hz;
  int maxorder;
  TensorParams()
    : direction(PbcDirection::none), W(0),
      maxerror(default_maxerror),
      hx(1), hy(1), hz(1),
      maxorder(default_maxorder) {}
  void SetDirection(char* dirstr) {
    std::string dircheck = dirstr;
    std::transform(dircheck.begin(), dircheck.end(),
                   dircheck.begin(), ::tolower);
    /// Note: Visual C++ complains here about possible precision loss
    /// because ::tolower returns an int rather than a char.
    if(dircheck.compare("none")==0) {
      direction = PbcDirection::none;
    } else if(dircheck.compare("x")==0) {
      direction = PbcDirection::x;
    } else if(dircheck.compare("y")==0) {
      direction = PbcDirection::y;
    } else if(dircheck.compare("z")==0) {
      direction = PbcDirection::z;
    } else {
      throw std::runtime_error("Invalid PBC direction.");
    }
  }
};

// Templates to rotate function arguments.  Some reference suggest
// that use should look like
//
//   RotateArgsLeft<&foo>(1,2,3)
//
// rather than
//
//   RotateArgsLeft<foo>(1,2,3)
//
// but I haven't come across a compiler that complained.
template <OXS_DEMAG_REAL_ANALYTIC F(OXS_DEMAG_REAL_ANALYTIC,
                                    OXS_DEMAG_REAL_ANALYTIC,
                                    OXS_DEMAG_REAL_ANALYTIC)>
OXS_DEMAG_REAL_ANALYTIC
Oxs_RotateArgsRight(OXS_DEMAG_REAL_ANALYTIC x,
                    OXS_DEMAG_REAL_ANALYTIC y,
                    OXS_DEMAG_REAL_ANALYTIC z) {
  return F(z,x,y);
}

template <OXS_DEMAG_REAL_ANALYTIC F(OXS_DEMAG_REAL_ANALYTIC,
                                    OXS_DEMAG_REAL_ANALYTIC,
                                    OXS_DEMAG_REAL_ANALYTIC)>
OXS_DEMAG_REAL_ANALYTIC
Oxs_RotateArgsLeft(OXS_DEMAG_REAL_ANALYTIC x,
                   OXS_DEMAG_REAL_ANALYTIC y,
                   OXS_DEMAG_REAL_ANALYTIC z) {
  return F(y,z,x);
}


void WriteTensorFile
(TensorParams& tp,
 char* Nfilename,
 int display_digits,
 OXS_DEMAG_REAL_ASYMP xrange,
 OXS_DEMAG_REAL_ASYMP yrange,
 OXS_DEMAG_REAL_ASYMP zrange)
{
  // Adjust range to full number of elements
  int xcount = static_cast<int>(ceil(xrange/tp.hx));
  int ycount = static_cast<int>(ceil(yrange/tp.hy));
  int zcount = static_cast<int>(ceil(zrange/tp.hz));
  if(xcount<1 || ycount<1 || zcount<1) {
    std::cerr << "Invalid range; range values must be positive.\n";
    exit(99);
  }
  xrange = xcount*tp.hx;
  yrange = ycount*tp.hy;
  zrange = zcount*tp.hz;

  Oxs_3DArray<OXS_DEMAG_REAL_ANALYTIC> Nxxarr(xcount,ycount+2,zcount+2);
  Oxs_3DArray<OXS_DEMAG_REAL_ANALYTIC> Nxyarr(xcount,ycount+2,zcount+2);
  Oxs_3DArray<OXS_DEMAG_REAL_ANALYTIC> Nxzarr(xcount,ycount+2,zcount+2);
  Oxs_3DArray<OXS_DEMAG_REAL_ANALYTIC> Nyyarr(xcount,ycount+2,zcount+2);
  Oxs_3DArray<OXS_DEMAG_REAL_ANALYTIC> Nyzarr(xcount,ycount+2,zcount+2);
  Oxs_3DArray<OXS_DEMAG_REAL_ANALYTIC> Nzzarr(xcount,ycount+2,zcount+2);

  Oxs_DemagNxxAsymptotic asympNxx(tp.hx,tp.hy,tp.hz,tp.maxerror,tp.maxorder);
  Oxs_DemagNxyAsymptotic asympNxy(tp.hx,tp.hy,tp.hz,tp.maxerror,tp.maxorder);
  Oxs_DemagNxzAsymptotic asympNxz(tp.hx,tp.hy,tp.hz,tp.maxerror,tp.maxorder);
  Oxs_DemagNyyAsymptotic asympNyy(tp.hx,tp.hy,tp.hz,tp.maxerror,tp.maxorder);
  Oxs_DemagNyzAsymptotic asympNyz(tp.hx,tp.hy,tp.hz,tp.maxerror,tp.maxorder);
  Oxs_DemagNzzAsymptotic asympNzz(tp.hx,tp.hy,tp.hz,tp.maxerror,tp.maxorder);

  const OXS_DEMAG_REAL_ASYMP arad = asympNxx.GetAsymptoticStart();

  if(tp.direction == TensorParams::PbcDirection::none) {
    ComputeD6f(Oxs_Newell_f_xx,asympNxx,Nxxarr,
               tp.hx,tp.hy,tp.hz,arad,0,0,0);
    ComputeD6f(Oxs_Newell_g_xy,asympNxy,Nxyarr,
               tp.hx,tp.hy,tp.hz,arad,0,0,0);
    ComputeD6f(Oxs_Newell_g_xz,asympNxz,Nxzarr,
               tp.hx,tp.hy,tp.hz,arad,0,0,0);
    ComputeD6f(Oxs_Newell_f_yy,asympNyy,Nyyarr,
               tp.hx,tp.hy,tp.hz,arad,0,0,0);
    ComputeD6f(Oxs_Newell_g_yz,asympNyz,Nyzarr,
               tp.hx,tp.hy,tp.hz,arad,0,0,0);
    ComputeD6f(Oxs_Newell_f_zz,asympNzz,Nzzarr,
               tp.hx,tp.hy,tp.hz,arad,0,0,0);

    // Special case handling for offset (0,0,0)
    Nxxarr(0,0,0) = Oxs_SelfDemagNx(OXS_DEMAG_REAL_ANALYTIC(tp.hx),
                                    OXS_DEMAG_REAL_ANALYTIC(tp.hy),
                                    OXS_DEMAG_REAL_ANALYTIC(tp.hz));
    Nyyarr(0,0,0) = Oxs_SelfDemagNy(OXS_DEMAG_REAL_ANALYTIC(tp.hx),
                                    OXS_DEMAG_REAL_ANALYTIC(tp.hy),
                                    OXS_DEMAG_REAL_ANALYTIC(tp.hz));
    Nzzarr(0,0,0) = Oxs_SelfDemagNz(OXS_DEMAG_REAL_ANALYTIC(tp.hx),
                                    OXS_DEMAG_REAL_ANALYTIC(tp.hy),
                                    OXS_DEMAG_REAL_ANALYTIC(tp.hz));
    Nxyarr(0,0,0) = Nxzarr(0,0,0) = Nyzarr(0,0,0) = 0.0;
  } else {
    // Periodic boundaries
    // TODO
  }

  std::ofstream Nfile;
  std::streambuf* cout_buffer = 0;
  if(strcmp("-",Nfilename)!=0) {
    // Write to file
    Nfile.open(Nfilename);
    if(Nfile.fail()) {
      std::cerr << "ERROR: Unable to open file \"" << Nfilename << "\"\n";
      throw std::runtime_error("File open error.");
    }
    // Redirect std::cout to file
    cout_buffer = std::cout.rdbuf();
    std::cout.rdbuf(Nfile.rdbuf());
  }
  
  std::cout << std::setprecision(17);
  std::cout << "# OOMMF OVF 2.0\n#\n";
  std::cout << "# Segment count: 1\n#\n";
  std::cout << "# Begin: segment\n";
  std::cout << "# Begin: header\n";
  std::cout << "#\n";
  std::cout << "# Title: Demag tensor elements\n";
  std::cout << "#\n";
  std::cout << "# meshunit: {}\n";
  std::cout << "# meshtype: rectangular\n";
  std::cout << "# xbase: 0.0\n";
  std::cout << "# ybase: 0.0\n";
  std::cout << "# zbase: 0.0\n";
  std::cout << "# xstepsize: " << tp.hx << "\n";
  std::cout << "# ystepsize: " << tp.hy << "\n";
  std::cout << "# zstepsize: " << tp.hz << "\n";
  std::cout << "# xnodes: " << xcount << "\n";
  std::cout << "# ynodes: " << ycount << "\n";
  std::cout << "# znodes: " << zcount << "\n";
  std::cout << "#\n";
  std::cout << "# xmin: " << -tp.hx/2. << "\n";
  std::cout << "# ymin: " << -tp.hy/2. << "\n";
  std::cout << "# zmin: " << -tp.hz/2. << "\n";
  std::cout << "# xmax: " << xrange - tp.hx/2. << "\n";
  std::cout << "# ymax: " << yrange - tp.hy/2. << "\n";
  std::cout << "# zmax: " << zrange - tp.hz/2. << "\n";
  std::cout << "#\n";
  std::cout << "# valuedim: 6\n";
  std::cout << "# valuelabels: Nxx Nxy Nxz Nyy Nyz Nzz\n";
  std::cout << "# valueunits: {} {} {} {} {} {}\n";
  std::cout << "#\n";
  std::cout << "# End: header\n#\n";

  std::cout << "# Begin: data text\n";

  std::cout << scientific << setprecision(display_digits-1);

  const int colwidth = display_digits + 8;

  for(int k=0;k<zcount;++k) {
    for(int j=0;j<ycount;++j) {
      for(int i=0;i<xcount;++i) {
        std::cout << setw(colwidth) << Nxxarr(i,j,k) << "  "
              << setw(colwidth) << Nxyarr(i,j,k) << "  "
              << setw(colwidth) << Nxzarr(i,j,k) << "  ";
        std::cout << setw(colwidth) << Nyyarr(i,j,k) << "  "
              << setw(colwidth) << Nyzarr(i,j,k) << "  ";
        std::cout << setw(colwidth) << Nzzarr(i,j,k) << "\n";
      }
    }
  }
  
  std::cout << "# End: data text\n#\n";
  std::cout << "# End: segment\n";

  if(cout_buffer != 0) {
    std::cout.rdbuf(cout_buffer);
    Nfile.close();
  }
}

void PrintTensor
(TensorParams& tp,
 int display_digits,
 int group_digits,
 OXS_DEMAG_REAL_ASYMP x,
 OXS_DEMAG_REAL_ASYMP y,
 OXS_DEMAG_REAL_ASYMP z)
{
  OXS_DEMAG_REAL_ASYMP gamma
    = pow(tp.hx*tp.hy*tp.hz,OXS_DEMAG_REAL_ASYMP(1)/OXS_DEMAG_REAL_ASYMP(3));
  OXS_DEMAG_REAL_ASYMP pdist = sqrt(x*x+y*y+z*z)/gamma;
  // std::cout << "gamma=" << gamma << ", pdist=" << pdist << "\n";

  std::string sNxx,sNxy,sNxz,sNyy,sNyz,sNzz;
  std::stringstream ss;
  ss << scientific << setprecision(display_digits-1);

  if(tp.direction != TensorParams::PbcDirection::none) {
    OXS_DEMAG_REAL_ASYMP Nxx,Nxy,Nxz,Nyy,Nyz,Nzz;
    Oxs_DemagPeriodic* pbctensor = 0;
    if(tp.direction == TensorParams::PbcDirection::x) {
      pbctensor = new Oxs_DemagPeriodicX(tp.hx,tp.hy,tp.hz,
                                         tp.maxerror,tp.maxorder,
                                         OC_INDEX(floor(tp.W/tp.hx+0.5)),
                                         0,0,0);
      /// Note: Setting constructor parameters wdimx/y/z (the last three
      /// parameters) to 0 makes "hole" in ComputePeriodicHoleTensor
      /// call empty.
    } else if(tp.direction == TensorParams::PbcDirection::y) {
      pbctensor = new Oxs_DemagPeriodicY(tp.hx,tp.hy,tp.hz,
                                         tp.maxerror,tp.maxorder,
                                         OC_INDEX(floor(tp.W/tp.hy+0.5)),
                                         0,0,0);
    } else if(tp.direction == TensorParams::PbcDirection::z) {
      pbctensor = new Oxs_DemagPeriodicZ(tp.hx,tp.hy,tp.hz,
                                         tp.maxerror,tp.maxorder,
                                         OC_INDEX(floor(tp.W/tp.hz+0.5)),
                                         0,0,0);
    } else {
      std::cerr << "ERROR: Invalid PBC direction.\n";
      throw std::runtime_error("Invalid PBC direction.");
    }
    OC_INDEX i = OC_INDEX(rint(x/tp.hx));
    OC_INDEX j = OC_INDEX(rint(y/tp.hy));
    OC_INDEX k = OC_INDEX(rint(z/tp.hz));
    if(fabs(i*tp.hx-x)>4*DBL_EPSILON*x ||
       fabs(j*tp.hy-y)>4*DBL_EPSILON*y ||
       fabs(k*tp.hz-z)>4*DBL_EPSILON*z) {
      if(fabs(i*tp.hx-x)>1024*DBL_EPSILON*x ||
         fabs(j*tp.hy-y)>1024*DBL_EPSILON*y ||
         fabs(k*tp.hz-z)>1024*DBL_EPSILON*z) {
        std::cerr <<
          "\nERROR: PBC code requires (x,y,z) to be"
          " an integral multiple of (hx,hy,hz)\n\n";
        Usage();
      }
      std::cerr << std::setprecision(17);
      if(fabs(i*tp.hx-x)>4*DBL_EPSILON*x) {
        std::cerr << "Warning: Rounding x to " << i*tp.hx << "\n";
      }
      if(fabs(j*tp.hy-y)>4*DBL_EPSILON*y) {
        std::cerr << "Warning: Rounding y to " << j*tp.hy << "\n";
      }
      if(fabs(k*tp.hz-z)>4*DBL_EPSILON*z) {
        std::cerr << "Warning: Rounding z to " << k*tp.hz << "\n";
      }
    }
    pbctensor->ComputePeriodicHoleTensor(i,j,k,Nxx,Nxy,Nxz,Nyy,Nyz,Nzz);
    /// Note: Empty "hole".
    ss << Nxx; sNxx = ss.str();  ss.str("");
    ss << Nxy; sNxy = ss.str();  ss.str("");
    ss << Nxz; sNxz = ss.str();  ss.str("");
    ss << Nyy; sNyy = ss.str();  ss.str("");
    ss << Nyz; sNyz = ss.str();  ss.str("");
    ss << Nzz; sNzz = ss.str();
    delete pbctensor;
  } else {
    // PBC direction == none
    Oxs_DemagNxxAsymptotic ANxx(tp.hx,tp.hy,tp.hz,tp.maxerror,tp.maxorder);
    const OXS_DEMAG_REAL_ASYMP arad = ANxx.GetAsymptoticStart();
    if(arad<0.0 || pdist<arad) {
      // Near field
      Xp_DoubleDouble Nxx,Nxy,Nxz,Nyy,Nyz,Nzz;
      Nxx = Oxs_CalculateNxx(x,y,z,tp.hx,tp.hy,tp.hz);
      Nxy = Oxs_CalculateNxy(x,y,z,tp.hx,tp.hy,tp.hz);
      Nxz = Oxs_CalculateNxz(x,y,z,tp.hx,tp.hy,tp.hz);
      Nyy = Oxs_CalculateNyy(x,y,z,tp.hx,tp.hy,tp.hz);
      Nyz = Oxs_CalculateNyz(x,y,z,tp.hx,tp.hy,tp.hz);
      Nzz = Oxs_CalculateNzz(x,y,z,tp.hx,tp.hy,tp.hz);
      ss << Nxx; sNxx = ss.str();  ss.str("");
      ss << Nxy; sNxy = ss.str();  ss.str("");
      ss << Nxz; sNxz = ss.str();  ss.str("");
      ss << Nyy; sNyy = ss.str();  ss.str("");
      ss << Nyz; sNyz = ss.str();  ss.str("");
      ss << Nzz; sNzz = ss.str();
    } else {
      // Far field
      Oxs_DemagNxyAsymptotic ANxy(tp.hx,tp.hy,tp.hz,tp.maxerror,tp.maxorder);
      Oxs_DemagNxzAsymptotic ANxz(tp.hx,tp.hy,tp.hz,tp.maxerror,tp.maxorder);
      Oxs_DemagNyyAsymptotic ANyy(tp.hx,tp.hy,tp.hz,tp.maxerror,tp.maxorder);
      Oxs_DemagNyzAsymptotic ANyz(tp.hx,tp.hy,tp.hz,tp.maxerror,tp.maxorder);
      Oxs_DemagNzzAsymptotic ANzz(tp.hx,tp.hy,tp.hz,tp.maxerror,tp.maxorder);
      OXS_DEMAG_REAL_ASYMP Nxx,Nxy,Nxz,Nyy,Nyz,Nzz;
      Nxx = ANxx.Asymptotic(x,y,z);
      Nxy = ANxy.Asymptotic(x,y,z);
      Nxz = ANxz.Asymptotic(x,y,z);
      Nyy = ANyy.Asymptotic(x,y,z);
      Nyz = ANyz.Asymptotic(x,y,z);
      Nzz = ANzz.Asymptotic(x,y,z);
      ss << Nxx; sNxx = ss.str();  ss.str("");
      ss << Nxy; sNxy = ss.str();  ss.str("");
      ss << Nxz; sNxz = ss.str();  ss.str("");
      ss << Nyy; sNyy = ss.str();  ss.str("");
      ss << Nyz; sNyz = ss.str();  ss.str("");
      ss << Nzz; sNzz = ss.str();
    }
  }

  if(group_digits>0) {
    GroupNumberString(group_digits,sNxx);
    GroupNumberString(group_digits,sNxy);
    GroupNumberString(group_digits,sNxz);
    GroupNumberString(group_digits,sNyy);
    GroupNumberString(group_digits,sNyz);
    GroupNumberString(group_digits,sNzz);
  }

  const int colwidth = display_digits + 8
    + (group_digits<1 ? 0 : (display_digits-2)/group_digits);
  std::cout << "--- Demag tensor; point offset=" << pdist
            << ", error request=" << tp.maxerror
            << ", order request=" << tp.maxorder << "\n";
  std::cout << setw(colwidth) << sNxx << "  "
            << setw(colwidth) << sNxy << "  "
            << setw(colwidth) << sNxz << "\n";
  std::cout << setw(colwidth) << ""   << "  "
            << setw(colwidth) << sNyy << "  "
            << setw(colwidth) << sNyz << "\n";
  std::cout << setw(colwidth) << "" << "  "
            << setw(colwidth) << "" << "  "
            << setw(colwidth) << sNzz << "\n";
  std::cout << "-------------------\n";
}

#if STANDALONE
int main(int argc,char** argv)
#else
int Oc_AppMain(int argc,char** argv)
#endif
{
  TensorParams tensor_params;
  int display_digits = default_display_digits;
  int   group_digits = default_group_digits;
  char*    Nfilename = 0;
  
  // Pull out options
  for(int i=1;i<argc-1;++i) {
    char* endptr;
    if(strcmp("-h",argv[i])==0 || strcmp("--help",argv[i])==0) {
      Usage();
    } else if(strcmp("-d",argv[i])==0 && i+1<argc) {
      display_digits = static_cast<int>(strtol(argv[i+1],&endptr,10));
      if(endptr == argv[i+1] || *endptr != '\0' || display_digits<0) Usage();
      for(int j=i+2;j<argc;++j) argv[j-2] = argv[j];
      argc -=2; --i;
    } else if(strcmp("-g",argv[i])==0 && i+1<argc) {
      group_digits = static_cast<int>(strtol(argv[i+1],&endptr,10));
      if(endptr == argv[i+1] || *endptr != '\0' || group_digits<0) Usage();
      for(int j=i+2;j<argc;++j) argv[j-2] = argv[j];
      argc -=2; --i;
    } else if(strcmp("-pbc",argv[i])==0 && i+2<argc) {
      // Following code will need rework for 2D PBC
      try {
        tensor_params.SetDirection(argv[i+1]);
      } catch(...) {
        std::cerr << "ERROR: Invalid PBC direction request: "
                  << argv[i+1] << "\n";
        Usage();
      }
      tensor_params.W
        = static_cast<OXS_DEMAG_REAL_ASYMP>(strtold(argv[i+2],&endptr));
      if(endptr == argv[i+2] || *endptr != '\0' || tensor_params.W <= 0.0) {
        std::cerr << "ERROR: Invalid window size: " << argv[i+2] << "\n";
        Usage();
      }
      for(int j=i+3;j<argc;++j) argv[j-3] = argv[j];
      argc -=3; --i;
    } else if(strcmp("-maxerror",argv[i])==0 && i+1<argc) {
      tensor_params.maxerror
        = static_cast<OXS_DEMAG_REAL_ASYMP>(strtold(argv[i+1],&endptr));
      if(endptr == argv[i+1] || *endptr != '\0'
         || tensor_params.maxerror<=0.0) Usage();
      for(int j=i+2;j<argc;++j) argv[j-2] = argv[j];
      argc -=2; --i;
    } else if(strcmp("-maxorder",argv[i])==0 && i+1<argc) {
      tensor_params.maxorder
        = static_cast<int>(strtol(argv[i+1],&endptr,10));
      if(endptr == argv[i+1] || *endptr != '\0'
         || tensor_params.maxorder<5 || tensor_params.maxorder>11
         || tensor_params.maxorder%2 != 1) Usage();
      for(int j=i+2;j<argc;++j) argv[j-2] = argv[j];
      argc -=2; --i;
    } else if(strcmp("-file",argv[i])==0 && i+1<argc) {
      Nfilename = argv[i+1];
      for(int j=i+2;j<argc;++j) argv[j-2] = argv[j];
      argc -=2; --i;
    }
  }
  
  if(argc!=7) Usage();

#if defined(_MSC_VER)
# define strtold(x,y) strtod((x),(y))
#elif defined(__BORLANDC__)
# define strtold(x,y) _strtold((x),(y))
#endif

#if 0
# if OXS_DEMAG_ASYMP_USE_SSE
  printf("SSE-enabled code\n");
# else
  printf("Non-SSE code\n");
# endif
#endif

  tensor_params.hx
    = static_cast<OXS_DEMAG_REAL_ASYMP>(strtold(argv[1],0));
  tensor_params.hy
    = static_cast<OXS_DEMAG_REAL_ASYMP>(strtold(argv[2],0));
  tensor_params.hz
    = static_cast<OXS_DEMAG_REAL_ASYMP>(strtold(argv[3],0));
  OXS_DEMAG_REAL_ASYMP
    x = static_cast<OXS_DEMAG_REAL_ASYMP>(strtold(argv[4],0));
  OXS_DEMAG_REAL_ASYMP
    y = static_cast<OXS_DEMAG_REAL_ASYMP>(strtold(argv[5],0));
  OXS_DEMAG_REAL_ASYMP
    z = static_cast<OXS_DEMAG_REAL_ASYMP>(strtold(argv[6],0));

  if(Nfilename == 0) {
    PrintTensor(tensor_params,display_digits,group_digits,
                x,y,z);
  } else {
    WriteTensorFile(tensor_params,Nfilename,display_digits,
                    x,y,z);
  }
  return 0;
}

#if STANDALONE
OC_REALWIDE Oc_Nop(OC_REALWIDE x) { return x; }
#endif // STANDALONE
