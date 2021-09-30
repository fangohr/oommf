/* FILE: ddtest.cc
 *  Development test file for Xp_DoubleDouble class.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 */

/***************************************************************
Some test cases:

# For Add #######################################################

 [0x10000000000000xb0,0x1FFFFFFFFFFFFFxb-53] [0x1FFFFFFFFFFFFFxb-01,0x00000000000000xb-54]
 [ 0x1880520AE6CF7Cxb+877,-0x1E51993D78C8FExb+823]  [ 0x11EB1AF5400712xb+876,-0x12FDCB23B1AE68xb+818]
 [-0x1B46ED8522AC10xb+874, 0x1BD67540F7BBBExb+820]  [-0x1A184B79B224AFxb+875, 0x161F5D725AF6D3xb+818]

# For Multiply ##################################################

 [-0x1C92191D0F3D9Fxb+877,-0x1D68CEEA902804xb+821]    [-0x110C3F12549846xb+878, 0x14E4B2B5C66410xb+823]

# For Sin #######################################################
# Respond to CORR_B:
0x1B63A203211975xb-63  0x16167DEE9BEDCBxb-119
0x1AE63107DABDFFxb-62 -0x1CC3941F2C57CCxb-116


# Respond to CORR_A
0x1E3CDDBF797DE0xb-61 -0x1AF0472C084FABxb-116  =   3.69113e-03    3.6911242606218310e-03
0x1FDAB5358684C5xb-61 -0x1EC89B649CC25Axb-115  =   3.88847e-03    3.8884579171606392e-03


# Respond to CORR_D
0x1462D9839253DCxb-71 -0x1D8B4868312CA3xb-126  =   2.43022e-06    2.4302162527423504e-06


# Respond to CORR_E
0x198FF6AA45C726xb-67 -0x13DED0FF08400Exb-124  =   4.87563e-05    4.8756327726397912e-05
0x1BFDC33D3B5599xb-70  0x188B3291BEB0F8xb-125  =   6.67364e-06    6.6736365230936052e-06

# Sensitive to single-word series computation
0x102C5EA8067EECxb-60  0x156B397D27C799xb-115  =   3.94856e-03    3.9485540318593415e-03
0x1F07AB4DC1D60Dxb-61 -0x185018E45C7159xb-115  =   3.78784e-03    3.7878275678977572e-03
0x1A55644D8B9D75xb-61  0x153D4193417A36xb-116  =   3.21455e-03    3.2145405760334008e-03
0x127F5D26FADA7Fxb-59 -0x101FF3B929296Bxb-113  =   9.03199e-03    9.0318670151298582e-03

0x1F5C5AA2166540xb-60  0x1412CA49C7DE63xb-118  =   7.65644e-03    7.6563602424953433e-03

 -0.848680 : [ 0x1AE5E6E5DC3F4Dxb-53, 0x1E0344DD4843F2xb-109]  =   8.40564e-01    7.4501958546481917e-01


# Sensitive to CORR_C
MAXERR:  2.050061 : [ 0x1088C8ED5603B0xb-53,-0x14E9FA2A07A99Cxb-109]  =   5.16697e-01    4.9401134931528434e-01
MINERR: -1.662137 : [ 0x10643ACF6C477Dxb-53, 0x16D8CAE02472D9xb-108]  =   5.12235e-01    4.9012667459861636e-01

# Sensitive to <1024.pi reduction
MAXERR:     1.010530 : [ 0x1D4BC97F2EC1DCxb-51,-0x12AEF98C419616xb-105]  =  3.66201e+000  -4.9723830073309117e-001
MINERR:    -0.944760 : [ 0x181B90067CC44Axb-49, 0x142F60B8203F05xb-116]  =  1.20538e+001  -4.9039018639204490e-001

# Sensitive to >1024.pi reduction
MAXERR:     1.021980 : [ 0x1223EF0EDA23F4xb-38, 0x1CBBD54721E1CCxb-92]   =  1.85757e+004   4.8124914920165596e-001
MINERR:    -1.124309 : [ 0x1C68B96DDB3AE2xb-38, 0x145813C5DC2B2Axb-92]   =  2.90909e+004  -2.4802608654223121e-001

# MISC
MINERR: -0.954257 : [ 0x1C4BD513BECBC8xb-53,-0x1108FADEAEFA97xb-108]  =  8.84257e-001   7.7344416119147485e-001

./ddtest Sin -oneshot \
  0x1B63A203211975xb-63  0x16167DEE9BEDCBxb-119 \
  0x1AE63107DABDFFxb-62 -0x1CC3941F2C57CCxb-116 \
  0x1E3CDDBF797DE0xb-61 -0x1AF0472C084FABxb-116 \
  0x1FDAB5358684C5xb-61 -0x1EC89B649CC25Axb-115 \
  0x1462D9839253DCxb-71 -0x1D8B4868312CA3xb-126 \
  0x198FF6AA45C726xb-67 -0x13DED0FF08400Exb-124 \
  0x1BFDC33D3B5599xb-70  0x188B3291BEB0F8xb-125 \
  0x102C5EA8067EECxb-60  0x156B397D27C799xb-115 \
  0x1F07AB4DC1D60Dxb-61 -0x185018E45C7159xb-115 \
  0x1A55644D8B9D75xb-61  0x153D4193417A36xb-116 \
  0x127F5D26FADA7Fxb-59 -0x101FF3B929296Bxb-113 \
  0x1F5C5AA2166540xb-60  0x1412CA49C7DE63xb-118 \
  0x1AE5E6E5DC3F4Dxb-53  0x1E0344DD4843F2xb-109 \
  0x1088C8ED5603B0xb-53 -0x14E9FA2A07A99Cxb-109 \
  0x10643ACF6C477Dxb-53  0x16D8CAE02472D9xb-108 \
  0x1D4BC97F2EC1DCxb-51 -0x12AEF98C419616xb-105 \
  0x181B90067CC44Axb-49  0x142F60B8203F05xb-116 \
  0x1223EF0EDA23F4xb-38  0x1CBBD54721E1CCxb-92 \
  0x1C68B96DDB3AE2xb-38  0x145813C5DC2B2Axb-92 \
  0x1C4BD513BECBC8xb-53 -0x1108FADEAEFA97xb-108 \
| tclsh ddtest.tcl -p 500


# For Expm1 #######################################################

MINERR:    -0.501062 : [-0x11D68C323FAE01xb-61,-0x1A0E4107BA671Cxb-119]  =  -2.17750e-03   -2.1751305678638683e-03
MAXERR:     0.501077 : [ 0x1A01E4D0E06567xb-61,-0x16CEE86E336730xb-119]  =  3.17473e-003   3.1797759612254479e-003
MAXERR:     0.500154 : [-0x1D268FCAF5A391xb-63, 0x199202FC675584xb-117]  = -8.89607e-004  -8.8921109466606513e-004

MAXERR:     0.970424 : [ 0x19AAED0382D057xb-54,-0x1FFC84334DD5A6xb-113]  =  4.01057e-001   4.9340311436372347e-001
MINERR:    -0.999092 : [ 0x171D0BA5C22506xb-54, 0x1E4D8FD32B33F6xb-109]  =  3.61148e-001   4.3497552958206970e-001

MAXERR:     0.972845 : [ 0x12B6C730B3A0E9xb-46, 0x123CFC902F89EBxb-103]  =  7.48559e+001   3.2322698542073116e+032
MINERR:    -0.650946 : [ 0x129ED5CD6751E8xb-47,-0x12B3811E0CE76Axb-102]  =  3.72409e+001   1.4911359950481150e+016

MINERR:    -0.500136 : [ 0x18D79B2251A4ECxb-51, 0x1AF47436A14E0Fxb-111]  =  3.10528e+000   2.1315385280788430e+001

MAXERR:     0.501394 : [ 0x17771DF8F23A36xb-56,-0x11F68758A76241xb-110]  =   9.16613e-02    9.5993581699892735e-02
MINERR:    -0.500833 : [ 0x1671B0F65E2498xb-54,-0x16ACC3425D7644xb-108]  =   3.50689e-01    4.2004585560878555e-01

MAXERR:     0.500058 : [ 0x12320DCD88D6E6xb-48,-0x12BE63C61C01D8xb-102]  =   1.81955e+01    7.9839034946037918e+07

MAXERR:     0.501688 : [-0x1914CDE63AD880xb-57, 0x17B86CB2002266xb-111]  =  -4.89868e-02   -4.7806348246038056e-02
MINERR:    -0.502036 : [ 0x170DB1A1807C88xb-56,-0x16DB725E55731Cxb-110]  =   9.00527e-02    9.4231950269331469e-02

# For Exp #########################################################

# For Atan #######################################################
MAXERR:     1.278448 : [-0x1D1D8AF317327Exb-58,-0x1D40CD83F49F2Axb-112]  = -2.84330e-002  -2.8425350855575932e-002
MINERR:    -1.697250 : [ 0x1E891ADFBE43CAxb-62,-0x1417A834505DBExb-116]  =  1.86374e-003   1.8637409032738793e-003
MAXERR:     1.421883 : [-0x1FBC3D51DD799Bxb-61,-0x1121FA6FE92847xb-116]  = -3.87394e-003  -3.8739199111586225e-003
MINERR:    -1.415021 : [ 0x19F866C4DF64B2xb-56, 0x1118B4126218BAxb-112]  =  1.01447e-001   1.0110067928309348e-001
MAXERR:     1.133742 : [ 0x1636D246457F04xb-52,-0x1A7C10867341FBxb-107]  =  1.38838e+000   9.4660090083718551e-001
MINERR:    -1.085745 : [ 0x13D1A06E1E93EExb-52, 0x1CF50090B3898Bxb-106]  =  1.23868e+000   8.9161268803502747e-001
MAXERR:     0.533896 : [ 0x18AD6E654AA2C4xb-49,-0x1A7EC68B9E5C29xb-105]  =  1.23387e+001   1.4899274778333487e+000
MINERR:    -0.523932 : [ 0x1B165D7EE49420xb-49,-0x1F319DEDCE16D4xb-106]  =  1.35437e+001   1.4970948986560044e+000
MAXERR:     0.500203 : [-0x10E4AC2EEEAEEExb-42,-0x13B47254CBADFAxb-96]   = -1.08117e+003  -1.5698714015490978e+000
MINERR:    -0.500067 : [-0x1E7B348A9D7E90xb-40,-0x15D53695763BECxb-94]   = -7.80321e+003  -1.5706681743288577e+000
MAXERR:     0.499986 : [-0x12B0451F080D75xb-37, 0x14984326D2F5EBxb-92]   = -3.82742e+004  -1.5707701995069618e+000
MINERR:    -0.499660 : [-0x17D19BCCEDD20Axb-37,-0x15EAB7500D57F7xb-91]   = -4.87809e+004  -1.5707758269549970e+000
MAXERR: 10519.721310 : [ 0x12607EE73B7007xb-19, 0x182C5D0DC0545Fxb-79]   =  9.86604e+009   1.5707963266935390e+000
MINERR: -8485.544459 : [ 0x111A4E5B243FB5xb-19,-0x13FE7AA5478C3Exb-76]   =  9.18197e+009   1.5707963266859877e+000
MAXERR:     0.499560 : [ 0x1714AD98125DBDxb+08, 0x15227260F6F4BBxb-46]   =  1.66315e+018   1.5707963267948966e+000
MINERR:    -0.499860 : [ 0x18B337A98D38E5xb+13, 0x1DE0743AC5D373xb-44]   =  5.69545e+019   1.5707963267948966e+000
MAXERR:     0.499789 : [-0x11F38B3A38F2F2xb+43,-0x1D083D2D1A94D2xb-11]   = -4.44454e+028  -1.5707963267948966e+000
MINERR:    -0.499931 : [-0x1391FBB7038E2Fxb+43, 0x13E411D20D3CECxb-11]   = -4.84536e+028  -1.5707963267948966e+000
MAXERR:     0.501655 : [ 0x15248D25B7206Cxb+55, 0x1BCE541C979D3Dxb+01]   =  2.14413e+032   1.5707963267948966e+000
MINERR:    -0.499801 : [ 0x120AAC9DBC97AFxb+49, 0x13FA66C95BD58Exb-06]   =  2.85882e+030   1.5707963267948966e+000
MAXERR:     1.202160 : [ 0x14F40E7347B8B6xb-53,-0x15F376EA46E496xb-107]  =  6.54792e-001   5.7973659741330807e-001
MINERR:    -0.949650 : [-0x1662BFFB30E429xb-53, 0x171C10AA88F424xb-108]  = -6.99554e-001  -6.1042686447452676e-001
MAXERR:     0.500017 : [ 0x1DD31F9B98913Dxb-55,-0x1837549A403FD4xb-111]  =   2.33005e-01    2.2892098262919580e-01
MAXERR:     0.499993 : [-0x141FD0385A46E0xb-87, 0x1A86C9CE560C9Cxb-141]  =  -3.66058e-11   -3.6605835588496713e-11
MINERR:    -0.499938 : [ 0x1904EE64EE6927xb-75,-0x1910DDC9FB2688xb-131]  =   1.86408e-07    1.8640803253479592e-07
MINERR:    -0.500042 : [-0x1F48A02C8C70BExb-59,-0x1E1B8D079D19A3xb-113]  =  -1.52752e-02   -1.5274053334699092e-02
MAXERR:     0.731577 : [ 0x13372F7AF57E55xb-52, 0x1D25F323140A07xb-107]  =   1.20097e+00    8.7645663653179562e-01
MINERR:    -0.728926 : [-0x10540EA7EFDBBCxb-52, 0x1CAD70B4358B1Dxb-109]  =  -1.02052e+00   -7.9555449234230458e-01
MINERR:    -0.501655 : [-0x1526E5B7E9C938xb+55, 0x1B2C9E5689BBE3xb+01]   =  -2.14506e+32   -1.5707963267948966e+00
MAXERR:     0.500010 : [ 0x1CDB470AA4C082xb-54,-0x13D7AD4B9CC5BBxb-108]  =   4.50884e-01    4.2358851622991939e-01
MINERR:    -0.500016 : [ 0x1E0BF450C6ED7Axb-52, 0x168A6CCA1101F2xb-113]  =   1.87792e+00    1.0814845393885120e+00

# For Log1p #######################################################

MAXERR:     0.499965 : [ 0x1215EDA9E4590Fxb+277,-0x1C5352E35DB43Exb+221] =   1.23618e+99    2.2816795291439760e+02
MINERR:    -0.499969 : [ 0x15D9A945599AA1xb+360, 0x16FC7C86053B64xb+303] =  1.44442e+124    2.8588826153359776e+02

**************************************************************/

// Enable long double printf support in MinGW.  The
// __USE_MINGW_ANSI_STDIO macro needs to be set before
// the system header files are #include'd.
#if defined(__MINGW32__) && !defined(__USE_MINGW_ANSI_STDIO)
# define __USE_MINGW_ANSI_STDIO 1
#endif


#include <cassert>
#include <cstring>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <fstream>
#include <ios>
#include <iostream>
#include <iomanip>

#include <limits>  // For std::numeric_limits<T>::infinity()
#include <string>
#include <vector>

#include "oc.h"
#include "xpint.h"

/* end includes */

#define xp_stringify(s) xp_tstringify(s)
#define xp_tstringify(s) #s

using namespace std;

// Workarounds for some missing or misnamed functions in various compilers
#ifdef _MSC_VER
# if _MSC_VER<1800
inline long double strtold(const char* str,char** endptr) {
  return static_cast<long double>(strtod(str,endptr));
}
# endif
# if _MSC_VER<1900
#  define snprintf _snprintf
# endif
#endif /* _MSC_VER */
#ifdef __DMC__
#define snprintf _snprintf
#endif
#ifdef __BORLANDC__
inline long double strtold(const char* str,char** endptr) {
  return _strtold(str,endptr);
}
#endif
#if 0 && defined(__CYGWIN__)
inline long double strtold(const char* str,char** endptr) {
  return static_cast<long double>(strtod(str,endptr));
}
#endif // __CYGWIN__

////////////////////////////////////////////////////////////////////////

// Platform dependent time and count support

#ifdef _WIN32 // Windows version
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
double GetTicks() {
  return 1e-3*double(GetTickCount());
}
typedef __int64 BIGINT;
#else // Unix version:
#include <sys/time.h>
double GetTicks() {
  timeval ticks;
  gettimeofday(&ticks,0);
  return double(ticks.tv_sec) + 1e-6*(ticks.tv_usec);
}
typedef long int BIGINT;
#endif // _WIN32

////////////////////////////////////////////////////////////////////////

typedef XP_DDFLOAT_TYPE WORK_FLOAT;

////////////////////////////////////////////////////////////////////////

std::string DDWrite(const Xp_DoubleDouble& val) {
  XP_DDFLOAT_TYPE hi,lo;
  val.DebugBits(hi,lo); // Use this interface in case
  // val is not normalized.
  std::string buf = "[";
  buf += HexBinaryFloatFormat(hi);
#if XP_USE_ALT_DOUBLEDOUBLE == 0
  buf += ",";
  buf += HexBinaryFloatFormat(lo);
#endif
  buf += "]";
  return buf;
}

////////////////////////////////////////////////////////////////////////

// Function info structs and array

typedef Xp_DoubleDouble (*XDD_1P)(const Xp_DoubleDouble&);
typedef Xp_DoubleDouble (*XDD_2P)(const Xp_DoubleDouble&,
                                  const Xp_DoubleDouble&);
typedef Xp_DoubleDouble (*XDD_2P_dd_sd)(const Xp_DoubleDouble&,
                                        const XP_DDFLOAT_TYPE&);
typedef Xp_DoubleDouble (*XDD_2P_sd_dd)(const XP_DDFLOAT_TYPE&,
                                        const Xp_DoubleDouble&);
// Note: For 32-bit builds, it is significantly faster (2.5 times?!)
// to pass the single-double argument as a reference rather than a
// pass-by-value.  For 64-bit builds the latter is about 3% faster.

Xp_DoubleDouble Nop1(const Xp_DoubleDouble&)
{ // Nop with one argument, for timing tests
  return static_cast<Xp_DoubleDouble>(static_cast<XP_DDFLOAT_TYPE>(0.0));
}

Xp_DoubleDouble Nop2(const Xp_DoubleDouble&,const Xp_DoubleDouble&)
{ // Nop with two arguments, for timing tests
  return static_cast<Xp_DoubleDouble>(static_cast<XP_DDFLOAT_TYPE>(0.0));
}

Xp_DoubleDouble Nop2_dd_sd(const Xp_DoubleDouble&,XP_DDFLOAT_TYPE)
{ // Nop with two arguments, for timing tests
  return static_cast<Xp_DoubleDouble>(static_cast<XP_DDFLOAT_TYPE>(0.0));
}

Xp_DoubleDouble Add(const Xp_DoubleDouble& x,const Xp_DoubleDouble& y)
{
  return x + y;
}

Xp_DoubleDouble Subtract(const Xp_DoubleDouble& x,const Xp_DoubleDouble& y)
{
  return x - y;
}

Xp_DoubleDouble Multiply(const Xp_DoubleDouble& x,const Xp_DoubleDouble& y)
{
  return x * y;
}

Xp_DoubleDouble Multiply_sd_dd(const XP_DDFLOAT_TYPE& x,
                               const Xp_DoubleDouble& y)
{
  return x * y;
}

Xp_DoubleDouble Multiply_dd_sd(const Xp_DoubleDouble& x,
                               const XP_DDFLOAT_TYPE& y)
{
  return x * y;
}

Xp_DoubleDouble Divide(const Xp_DoubleDouble& x,const Xp_DoubleDouble& y)
{
  return x / y;
}

Xp_DoubleDouble Divide_dd_sd(const Xp_DoubleDouble& x,
                             const XP_DDFLOAT_TYPE& y)
{
  return x / y;
}

Xp_DoubleDouble DivideRecip(const Xp_DoubleDouble& x)
{
  return static_cast<Xp_DoubleDouble>(static_cast<XP_DDFLOAT_TYPE>(1.0)) / x;
}

Xp_DoubleDouble ReduceModTwoPi(const Xp_DoubleDouble& x)
{
  Xp_DoubleDouble result = x;
  result.ReduceModTwoPi();
  return result;
}

Xp_DoubleDouble Square(const Xp_DoubleDouble& x)
{
  Xp_DoubleDouble result = x;
  result.Square();
  return result;
}

// Definitions for func_array initialization
Xp_DoubleDouble Recip(const Xp_DoubleDouble& x)     { return recip(x); }
Xp_DoubleDouble Sqrt(const Xp_DoubleDouble& x)      { return sqrt(x); }
Xp_DoubleDouble RecipSqrt(const Xp_DoubleDouble& x) { return recipsqrt(x); }
Xp_DoubleDouble Sin(const Xp_DoubleDouble& x)       { return sin(x); }
Xp_DoubleDouble Cos(const Xp_DoubleDouble& x)       { return cos(x); }
Xp_DoubleDouble Exp(const Xp_DoubleDouble& x)       { return exp(x); }
Xp_DoubleDouble Expm1(const Xp_DoubleDouble& x)     { return expm1(x); }
Xp_DoubleDouble Atan(const Xp_DoubleDouble& x)      { return atan(x); }
Xp_DoubleDouble Log(const Xp_DoubleDouble& x)       { return log(x); }
Xp_DoubleDouble Log1p(const Xp_DoubleDouble& x)     { return log1p(x); }

struct FuncInfo {
  const char* name;
  void* fptr;

  // Enumeration of functions by parameter type and count.
  enum FuncTypes {
    FT_INVALID, FT_DD, FT_DD_DD, FT_DD_SD, FT_SD_DD
  } func_type;

  int ArgPartCount() const {
    // Returns the number of component parts in function call
    int count=0;
    switch(func_type) {
    case FT_DD:    count = 2; break;
    case FT_DD_DD: count = 4; break;
    case FT_DD_SD: count = 3; break;
    case FT_SD_DD: count = 3; break;
    default:
      std::string errmsg = "Invalid FuncType in FuncInfo::ArgCount() call; ";
      if(name == 0) {
        errmsg += "name not set";
      } else {
        errmsg += "name=";
        errmsg += name;
      }
      throw errmsg;
    }
    return count;
  }

  FuncInfo() : name(0), fptr(0), func_type(FT_INVALID) {}
  FuncInfo(const char* in_name,void* in_fptr,FuncTypes in_func_type=FT_DD)
    : name(in_name), fptr(in_fptr), func_type(in_func_type) {}
};

FuncInfo func_array[] = {
  FuncInfo("Add",reinterpret_cast<void*>(Add),
           FuncInfo::FT_DD_DD),
  FuncInfo("Subtract",reinterpret_cast<void*>(Subtract),
           FuncInfo::FT_DD_DD),
  FuncInfo("Multiply",reinterpret_cast<void*>(Multiply),
           FuncInfo::FT_DD_DD),
  FuncInfo("Multiply_sd_dd",reinterpret_cast<void*>(Multiply_sd_dd),
           FuncInfo::FT_SD_DD),
  FuncInfo("Multiply_dd_sd",reinterpret_cast<void*>(Multiply_dd_sd),
           FuncInfo::FT_DD_SD),
  FuncInfo("Divide",reinterpret_cast<void*>(Divide),
           FuncInfo::FT_DD_DD),
  FuncInfo("Divide_dd_sd",reinterpret_cast<void*>(Divide_dd_sd),
           FuncInfo::FT_DD_SD),
  FuncInfo("ReduceModTwoPi",reinterpret_cast<void*>(ReduceModTwoPi)),
  FuncInfo("Square",reinterpret_cast<void*>(Square)),
  FuncInfo("Recip",reinterpret_cast<void*>(Recip)),
  FuncInfo("DivideRecip",reinterpret_cast<void*>(DivideRecip)),
  FuncInfo("Sqrt",reinterpret_cast<void*>(Sqrt)),
  FuncInfo("RecipSqrt",reinterpret_cast<void*>(RecipSqrt)),
  FuncInfo("Sin",reinterpret_cast<void*>(Sin)),
  FuncInfo("Cos",reinterpret_cast<void*>(Cos)),
  FuncInfo("Exp",reinterpret_cast<void*>(Exp)),
  FuncInfo("Expm1",reinterpret_cast<void*>(Expm1)),
  FuncInfo("Atan",reinterpret_cast<void*>(Atan)),
  FuncInfo("Log",reinterpret_cast<void*>(Log)),
  FuncInfo("Log1p",reinterpret_cast<void*>(Log1p)),
  FuncInfo("Nop1",reinterpret_cast<void*>(Nop1)),
  FuncInfo("Nop2",reinterpret_cast<void*>(Nop2),FuncInfo::FT_DD_DD),
  FuncInfo("Nop2_dd_sd",reinterpret_cast<void*>(Nop2),FuncInfo::FT_DD_SD)
};

const FuncInfo* FindFuncInfo(const char* name)
{
  FuncInfo* func_info = 0;
  for(size_t i=0;i<sizeof(func_array)/sizeof(func_array[0]);++i) {
    if(strcmp(name,func_array[i].name)==0) {
      func_info = &(func_array[i]);
      break;
    }
  }
  return func_info;
}

////////////////////////////////////////////////////////////////////////
// Testing class
class DDTest {
public:
  DDTest(const char* funcname,std::vector<std::string> initdata)
    : func_info(FindFuncInfo(funcname)),x(0.,0.),y(0.,0.),t(0.,0.) {
    if(initdata.size()<4 || initdata.size()>6) {
      std::string msg
        = "Error in DDTest: import initdata has invalid size for function ";
      msg += funcname;
      throw msg;
    }
    switch(func_info->func_type) {
    case FuncInfo::FT_DD:
      x = Xp_DoubleDouble(ScanFloat(initdata[0].c_str()),
                          ScanFloat(initdata[1].c_str()));
      break;
    case FuncInfo::FT_DD_SD:
      x = Xp_DoubleDouble(ScanFloat(initdata[0].c_str()),
                          ScanFloat(initdata[1].c_str()));
      y = Xp_DoubleDouble(ScanFloat(initdata[2].c_str()));
      break;
    case FuncInfo::FT_DD_DD:
      x = Xp_DoubleDouble(ScanFloat(initdata[0].c_str()),
                          ScanFloat(initdata[1].c_str()));
      y = Xp_DoubleDouble(ScanFloat(initdata[2].c_str()),
                          ScanFloat(initdata[3].c_str()));
      break;
    case FuncInfo::FT_SD_DD:
      x = Xp_DoubleDouble(ScanFloat(initdata[0].c_str()));
      y = Xp_DoubleDouble(ScanFloat(initdata[1].c_str()),
                          ScanFloat(initdata[2].c_str()));
      break;
    default:
      std::string msg
        = "Error in DDTest: Invalid function type for function ";
      msg += funcname;
      throw msg;
    }
    const size_t dlen = initdata.size();
    t = Xp_DoubleDouble(ScanFloat(initdata[dlen-2].c_str()),
                        ScanFloat(initdata[dlen-1].c_str()));
  }

  DDTest(const char* funcname,
         const Xp_BigFloatVec& import_x,const Xp_BigFloatVec& import_t)
    : func_info(FindFuncInfo(funcname)),x(import_x),y(0.),t(import_t) {
    std::string msg;
    switch(func_info->func_type) {
    case FuncInfo::FT_DD:
      // OK
      break;
    case FuncInfo::FT_DD_SD:
    case FuncInfo::FT_DD_DD:
    case FuncInfo::FT_SD_DD:
      msg = "Error in DDTest: Invalid arg count (2) for function ";
      msg += funcname;
      throw msg;
      break;
    default:
      msg  = "Error in DDTest: Invalid function type for function ";
      msg += funcname;
      throw msg;
    }
  }

  DDTest(const char* funcname,
         const Xp_BigFloatVec& import_x,const Xp_BigFloatVec& import_y,
         const Xp_BigFloatVec& import_t)
    : func_info(FindFuncInfo(funcname)),x(import_x),y(import_y),t(import_t) {
    std::string msg;
    switch(func_info->func_type) {
    case FuncInfo::FT_DD:
      msg = "Error in DDTest: Invalid arg count (1) for function ";
      msg += funcname;
      throw msg;
      break;
    case FuncInfo::FT_DD_SD:
    case FuncInfo::FT_DD_DD:
    case FuncInfo::FT_SD_DD:
      // OK
      break;
    default:
      msg = "Error in DDTest: Invalid function type for function ";
      msg += funcname;
      throw msg;
    }
  }

  DDTest(const char* funcname,
         const char* xhi,const char* xlo,
         const char* thi,const char* tlo)
    : func_info(FindFuncInfo(funcname)),
      x(ScanFloat(xhi),ScanFloat(xlo)),t(ScanFloat(thi),ScanFloat(tlo)) {
    if(func_info->func_type != FuncInfo::FT_DD) {
      std::string msg
        = "Error in DDTest: Wrong argument type/count for function ";
      msg += funcname;
      throw msg;
    }
  }

  DDTest(const char* funcname,
         const char* xhi,const char* xlo,
         const char* yhi,const char* ylo,
         const char* thi,const char* tlo)
    : func_info(FindFuncInfo(funcname)),
      x(ScanFloat(xhi),ScanFloat(xlo)),
      y(ScanFloat(yhi),ScanFloat(ylo)),
      t(ScanFloat(thi),ScanFloat(tlo)) {
    if(func_info->func_type != FuncInfo::FT_DD_DD) {
      std::string msg
        = "Error in DDTest: Wrong argument type/count for function ";
      msg += funcname;
      throw msg;
    }
  }

  DDTest(const char* funcname,
         const char* a,const char* b,const char* c,
         const char* thi,const char* tlo)
    : func_info(FindFuncInfo(funcname)),
      x(0.,0.),y(0.,0.),
      t(ScanFloat(thi),ScanFloat(tlo)) {
    switch(func_info->func_type) {
    case FuncInfo::FT_DD_SD:
      x = Xp_DoubleDouble(ScanFloat(a),ScanFloat(b));
      y = Xp_DoubleDouble(ScanFloat(c));
      break;
    case FuncInfo::FT_SD_DD:
      x = Xp_DoubleDouble(ScanFloat(a));
      y = Xp_DoubleDouble(ScanFloat(b),ScanFloat(c));
      break;
    default:
      std::string msg
        = "Error in DDTest: Wrong argument type/count for function ";
      msg += funcname;
      throw msg;
    }
  }

  void TestReport(Xp_DoubleDouble& result,
                  WORK_FLOAT result_error,const char* errstr=0);

  int Test(WORK_FLOAT referr=0.0,WORK_FLOAT refulp=0.0);  // Returns 1
  /// if func applied to the specified inputs (x or (x,y)) returns
  /// exactly the specified output (t).  Otherwise, 0.  The product of
  /// the import parameters referr and refulp gives the size of error in
  /// the reference result.  If non-zero then it gives the reference
  /// value in effectively triple-double precision.  (referr and refulp
  /// are held separately to protect against underflow.)

  static double SetComparisonSlack(double slack) {
    // In the base and file tests cases, an error is raised if the
    // computed value differs from the reference value by more than
    // slack*ULP.  The default setting for slack is 0.5, but can be
    // changed via this interface.  In particular, if slack is set to
    // 0.0, then an error is raised any time the computed value is not
    // exactly the same as the reference value.
    double old_setting = allowed_compare_error;
    allowed_compare_error = slack;
    return old_setting;
  }

  static int SetVerbose(int val) {
    int oldval = verbose; verbose = val; return oldval;
  }

private:
  const FuncInfo* func_info;
  Xp_DoubleDouble x; // Note single double args are held in high
  Xp_DoubleDouble y; // part of double-double.
  Xp_DoubleDouble t;
  static double allowed_compare_error;
  static int verbose;
};

double DDTest::allowed_compare_error = 0.5; // Default; allow up to 0.5 ULP error
int DDTest::verbose = 1; // Default verbosity

void DDTest::TestReport(Xp_DoubleDouble& result,
                        WORK_FLOAT result_error,const char* errstr)
{
  cerr << "Func: " << func_info->name << "\n";
  cerr << "   x: " << DDWrite(x) << "\n";
  if(func_info->func_type != FuncInfo::FT_DD) {
    // Function takes two inputs
    cerr << "   y: " << DDWrite(y) << "\n";
  }
  cerr << " Ref: " << DDWrite(t) << "\n";
  cerr << "Test: " << DDWrite(result) << "\n";
  if(errstr==0) {
#ifdef _WIN32
    cerr << "Diff: " << double(result_error) << " ULP\n";
    /// I/O on Windows frequently breaks on 80-bit long doubles.
    /// Casting to double works around that problem but OTOH will break
    /// if result_error is outside double range.
#else
    cerr << "Diff: " << result_error << " ULP\n";
#endif
  } else {
    cerr << "Diff: " << errstr << "\n";
  }
}

int DDTest::Test(WORK_FLOAT referr,WORK_FLOAT refulp)
{
  Xp_DoubleDouble result;
  try {
    switch(func_info->func_type) {
    case FuncInfo::FT_DD:
      result = reinterpret_cast<XDD_1P>(func_info->fptr)(x);
      break;
    case FuncInfo::FT_DD_DD:
      result = reinterpret_cast<XDD_2P>(func_info->fptr)(x,y);
      break;
    case FuncInfo::FT_DD_SD:
      result = reinterpret_cast<XDD_2P_dd_sd>(func_info->fptr)(x,y.Hi());
      break;
    case FuncInfo::FT_SD_DD:
      result = reinterpret_cast<XDD_2P_sd_dd>(func_info->fptr)(x.Hi(),y);
      break;
    default:
      std::string msg = "Unsupported function type for function ";
      msg += func_info->name;
      throw msg;
    }
  } catch(const char* errmsg) {
    fprintf(stderr,"ERROR: %s\n",errmsg);
    throw;
  }

  // Check result
  if(!result.IsNormalized()) {
    if(verbose) TestReport(result,0.0,"ERROR: Unnormalized output");
    return 0;
  }
  if(Xp_IsNaN(t.Hi()) && Xp_IsNaN(result.Hi())) {
    // Result agrees with reference
    return 1;
  }
  if(Xp_IsNaN(t.Hi()) || Xp_IsNaN(result.Hi())) {
    if(verbose) TestReport(result,0.0,"ERROR: NaN mismatch");
    return 0;
  }
  if(Xp_AreEqual(result.Hi(),t.Hi()) && Xp_AreEqual(result.Lo(),t.Lo())) {
    if(Xp_IsZero(t.Hi())) {
      // Signed zero check
      if(Xp_SignBit(result.Hi()) != Xp_SignBit(t.Hi())
         || Xp_SignBit(result.Lo()) != Xp_SignBit(t.Lo())) {
        if(verbose) TestReport(result,0.0,"ERROR: Signed zero mismatch");
        return 0;
      }
    }
    // Result agrees with reference
    if(verbose>=3) TestReport(result,0.0,"0 (exact match)");
    return 1;
  }

  if(refulp == 0.0) refulp = t.ULP();
  WORK_FLOAT result_error = result.ComputeDiffULP(t,refulp);
  if(!Xp_IsNaN(result_error) && allowed_compare_error>0.0) {
    // Look closely and ignore differences between computed value and
    // reference that are smaller than allowed_compare_error * 1 ULP.
    // This check includes adjustment using the stated error in the
    // reference value.  Note that rounding in this test may hide errors
    // which are in fact slightly more than allowed_compare_error * 1
    // ULP.  Underflow might also cause detection problems.
    //   Note: The NaN test is protection against compilers that don't
    // do comparisons properly with NaNs.  (In particular, if
    // result_error is a NaN, then the following <= comparison should
    // return false, but some compilers mess this up.)
    result_error += referr;  // Total error in ULPs
    if(fabs(result_error) <= allowed_compare_error) {
      if(verbose>=3) TestReport(result,result_error);
      return 1;
    }
  }

  // Report error
  if(verbose) TestReport(result,result_error);
  return 0;
}

#if XP_USE_ALT_DOUBLEDOUBLE == 0 && XP_DDFLOAT_MANTISSA_PRECISION == 53
// Test data for XP_DDFLOAT_TYPE == IEEE 8-byte float (53 bit mantissa)
static DDTest test_data[] = {
  {"Add"," 0x10000000000001xb+000","-0x10000000000000xb-105"," 0x1FFFFFFFFFFFFFxb-001"," 0x00000000000000xb-053",
         " 0x10000000000000xb+001"," 0x1FFFFFFFFFFFFExb-054"},
  {"Add"," 0x1880520AE6CF7Cxb+877","-0x1E51993D78C8FExb+823"," 0x11EB1AF5400712xb+876","-0x12FDCB23B1AE68xb+818",
         " 0x10BAEFC2C36982xb+878"," 0x108B3C34B4D4C7xb+824"},
  {"Add","-0x1B46ED8522AC10xb+874"," 0x1BD67540F7BBBExb+820","-0x1A184B79B224AFxb+875"," 0x161F5D725AF6D3xb+818",
         "-0x13DDE11E21BD5Bxb+876","-0x17A86CD89C61A3xb+822"},
  {"Add","0x10000000000001xb+054","-0x10000000000001xb+000","-0x10000000000000xb+054","-0x10000000000000xb+000",
         "0x1FFFFFFFFFFFFFxb+000"," 0x00000000000000xb-053"},
  {"Add","0x10F3F007F39288xb-025","0x1E49C6436DAAF2xb-079","-0x10F3F007F39289xb-025","0x144641DD15A173xb-081",
         "-0x1CA4A9454CECB1xb-079","-0x10000000000000xb-133"},
  {"Subtract","0x10000000000001xb+056","-0x144641DD15A173xb+000","0x10000000000000xb+056","0x1E49C6436DAAF2xb+002",
         "0x1CA4A9454CECB1xb+02","0x10000000000000xb-52"},
  {"Multiply","-0x1C92191D0F3D9Fxb+877","-0x1D68CEEA902804xb+821","-0x110C3F12549846xb+878","0x14E4B2B5C66410xb+823","Inf","Inf"},
  { "Sin", " 0x1B63A203211975xb-063"," 0x16167DEE9BEDCBxb-119"," 0x1B63A1CD9F70E5xb-063","-0x116A31328E4D01xb-118"},
  { "Sin", " 0x1AE63107DABDFFxb-062","-0x1CC3941F2C57CCxb-116"," 0x1AE6303D1C8180xb-062"," 0x1A656970F9F4EAxb-116"},
  { "Sin", " 0x1E3CDDBF797DE0xb-061","-0x1AF0472C084FABxb-116"," 0x1E3CD93F83D55Axb-061"," 0x1737D80DEDB009xb-115"},
  { "Sin", " 0x1FDAB5358684C5xb-061","-0x1EC89B649CC25Axb-115"," 0x1FDAAFF2C12505xb-061"," 0x1B6E9ADCC5397Axb-115"},
  { "Sin", " 0x1462D9839253DCxb-071","-0x1D8B4868312CA3xb-126"," 0x1462D983923DCCxb-071","-0x1E2F283A09CA97xb-125"},
  { "Sin", " 0x198FF6AA45C726xb-067","-0x13DED0FF08400Exb-124"," 0x198FF6AA1A479Bxb-067","-0x1B3FE446BC0EF9xb-121"},
  { "Sin", " 0x1BFDC33D3B5599xb-070"," 0x188B3291BEB0F8xb-125"," 0x1BFDC33D3A7125xb-070"," 0x14AB992FFAC3BFxb-124"},
  { "Sin", " 0x102C5EA8067EECxb-060"," 0x156B397D27C799xb-115"," 0x102C5BE6EEE600xb-060","-0x13B4BA0AF0B99Dxb-114"},
  { "Sin", " 0x1F07AB4DC1D60Dxb-061","-0x185018E45C7159xb-115"," 0x1F07A670DD4010xb-061"," 0x1737E4F50B2958xb-115"},
  { "Sin", " 0x1A55644D8B9D75xb-061"," 0x153D4193417A36xb-116"," 0x1A556154A9CEBAxb-061","-0x181478528FB893xb-115"},
  { "Sin", " 0x127F5D26FADA7Fxb-059","-0x101FF3B929296Bxb-113"," 0x127F4CAB9D4B5Axb-059","-0x1B83F331FE690Axb-113"},
  { "Sin", " 0x1F5C5AA2166540xb-060"," 0x1412CA49C7DE63xb-118"," 0x1F5C468D910C4Fxb-060","-0x1A4B038DE98464xb-114"},
  { "Sin", " 0x1AE5E6E5DC3F4Dxb-053"," 0x1E0344DD4843F2xb-109"," 0x17D733504E6D92xb-053","-0x14281ADD824082xb-107"},
  { "Sin", " 0x1088C8ED5603B0xb-053","-0x14E9FA2A07A99Cxb-109"," 0x1F9DE1C74A5DDAxb-054","-0x188A95DE99C9E2xb-108"},
  { "Sin", " 0x10643ACF6C477Dxb-053"," 0x16D8CAE02472D9xb-108"," 0x1F5E3C45931734xb-054"," 0x12DCD76A3E609Axb-112"},
  { "Sin", " 0x1D4BC97F2EC1DCxb-051","-0x12AEF98C419616xb-105","-0x1FD2C097FDE740xb-054","-0x167D8FD9330D9Dxb-109"},
  { "Sin", " 0x181B90067CC44Axb-049"," 0x142F60B8203F05xb-116","-0x1F628D853552C6xb-054"," 0x18BB035E0DC93Fxb-108"},
  { "Sin", " 0x1223EF0EDA23F4xb-038"," 0x1CBBD54721E1CCxb-092"," 0x1ECCC93B4321C8xb-054","-0x17142C75B25807xb-108"},
  { "Sin", " 0x1C68B96DDB3AE2xb-038"," 0x145813C5DC2B2Axb-092","-0x1FBF519D207AD7xb-055"," 0x14693F375358ECxb-109"},
  { "Sin", " 0x1C4BD513BECBC8xb-053","-0x1108FADEAEFA97xb-108"," 0x18C00DF8332F68xb-053"," 0x1914884274D39Cxb-107"},
  { "Cos", " 0x1B63A203211975xb-063"," 0x16167DEE9BEDCBxb-119"," 0x1FFFFF447543E0xb-053","-0x12F09B5242FEDBxb-107"},
  { "Cos", " 0x1D4BC97F2EC1DCxb-051","-0x12AEF98C419616xb-105","-0x1BC37E75695EA2xb-053","-0x178A0C66770176xb-107"},
  {"Expm1","-0x11D68C323FAE01xb-061","-0x1A0E4107BA671Cxb-119","-0x11D19454E983C9xb-061"," 0x1F4BF1C1684384xb-115"},
  {"Expm1"," 0x1A01E4D0E06567xb-061","-0x16CEE86E336730xb-119"," 0x1A0C793862E12Dxb-061","-0x1CC334DEACBD8Cxb-115"},
  {"Expm1","-0x1D268FCAF5A391xb-063"," 0x199202FC675584xb-117","-0x1D233E4910FE6Axb-063","-0x1FAABDD0B0A586xb-117"},
  {"Expm1"," 0x19AAED0382D057xb-054","-0x1FFC84334DD5A6xb-113"," 0x1F93EAA7FBF38Dxb-054"," 0x140EA506BE9E6Cxb-110"},
  {"Expm1"," 0x171D0BA5C22506xb-054"," 0x1E4D8FD32B33F6xb-109"," 0x1BD6A39A876095xb-054"," 0x1344BF900E50C1xb-108"},
  {"Expm1"," 0x12B6C730B3A0E9xb-046"," 0x123CFC902F89EBxb-103"," 0x1FDF656F9685B8xb+055","-0x125E2ED497013Bxb+001"},
  {"Expm1"," 0x129ED5CD6751E8xb-047","-0x12B3811E0CE76Axb-102"," 0x1A7CE6C47BB380xb+001","-0x12D5E86E2BBF7Bxb-053"},
  {"Expm1"," 0x18D79B2251A4ECxb-051"," 0x1AF47436A14E0Fxb-111"," 0x1550BD16FAA047xb-048"," 0x1AB2D6C5D3B8F0xb-102"},
  {"Expm1"," 0x17771DF8F23A36xb-056","-0x11F68758A76241xb-110"," 0x1893090E06E5C3xb-056","-0x195ACD44CE842Cxb-110"},
  {"Expm1"," 0x1671B0F65E2498xb-054","-0x16ACC3425D7644xb-108"," 0x1AE208032A3E9Fxb-054","-0x1F5FD7E2BC9E4Fxb-108"},
  {"Expm1"," 0x12320DCD88D6E6xb-048","-0x12BE63C61C01D8xb-102"," 0x1308FCEBC8BE2Axb-026","-0x11965829EAEAF1xb-080"},
  {"Expm1","-0x1914CDE63AD880xb-057"," 0x17B86CB2002266xb-111","-0x187A12DC840E25xb-057"," 0x1A7525FC0AB0AAxb-111"},
  {"Expm1"," 0x170DB1A1807C88xb-056","-0x16DB725E55731Cxb-110"," 0x181F95C8A523C5xb-056","-0x198710FE22D50Axb-110"},
  { "Atan","-0x1B34F9451B4E41xb-058","-0x18000000000000xb-112","-0x1B3355E26AC7EExb-058"," 0x1ACF635D668E37xb-113"},
  { "Atan"," 0x18FA84DA5078E5xb-056"," 0x14000000000000xb-111"," 0x18E6575D3355E8xb-056","-0x14E4ACDE378CE0xb-110"},
  { "Atan"," 0x1636D246457F04xb-052"," 0x1118B4126218BAxb-112"," 0x1E4A8DF8EEB719xb-053"," 0x1C97DAE9A7D3F7xb-107"},
  { "Atan"," 0x13D1A06E1E93EExb-052"," 0x1CF50090B3898Bxb-106"," 0x1C881754F9E418xb-053","-0x112CE49A3A4EABxb-107"},
  { "Atan"," 0x19E1E5F2179B72xb-048"," 0x00000000000000xb-053"," 0x1883CE6740330Dxb-052","-0x1B2AF0D5E03825xb-106"},
  { "Atan","-0x115A2FCB2C9526xb-039"," 0x10000000000000xb-093","-0x1921854DEDCF30xb-052"," 0x157D63B9E7427Dxb-106"},
  { "Atan","-0x12B0451F080D75xb-037"," 0x14984326D2F5EBxb-092","-0x1921DFEEC69EE9xb-052","-0x147BF8F9E5F6E7xb-106"},
  { "Atan"," 0x126078F2D47CCCxb-019","-0x1D20A000000000xb-073"," 0x1921FB543D35FBxb-052"," 0x1E8931ED991CDExb-107"},
  { "Atan"," 0x111A4E5B243FB5xb-019"," 0x182C5D0DC0545Fxb-079"," 0x1921FB543CB126xb-052","-0x1EF894CED834FDxb-106"},
  { "Atan"," 0x1714AD98125DBDxb+008"," 0x15227260F6F4BBxb-046"," 0x1921FB54442D18xb-052"," 0x1179C8857B0417xb-106"},
  { "Atan"," 0x18B337A98D38E5xb+013"," 0x1DE0743AC5D373xb-044"," 0x1921FB54442D18xb-052"," 0x11A4DA8A5FD2DCxb-106"},
  { "Atan","-0x12C2C3789E4090xb+044","-0x10000000000000xb-009","-0x1921FB54442D18xb-052","-0x11A6263314589Exb-106"},
  { "Atan"," 0x15248D25B7206Cxb+055"," 0x13E411D20D3CECxb-011"," 0x1921FB54442D18xb-052"," 0x11A62633145C07xb-106"},
  { "Atan"," 0x120AAC9DBC97CBxb+049","-0x18D5F1B4316180xb-006"," 0x1921FB54442D18xb-052"," 0x11A62633145BEAxb-106"},
  { "Atan"," 0x13FA66C95BD5B8xb-006","-0x17E319708E9400xb-062"," 0x1921FB54442CE5xb-052"," 0x14316AA5B375DBxb-110"},
  { "Atan","-0x1662BFFB30E429xb-053"," 0x171C10AA88F424xb-108","-0x13889DEB7092C6xb-053","-0x1B8702D25738C9xb-107"},
  { "Atan"," 0x1DD31F9B84716Dxb-055","-0x1C2D2370000000xb-110"," 0x1D4D4862CE442Cxb-055"," 0x1B539F98467C2Axb-111"},
  { "Atan"," 0x1904EE64EE6927xb-075"," 0x1A86C9CE560C9Cxb-141"," 0x1904EE64EE68D5xb-075"," 0x1B9FEE986AE37Cxb-129"},
  { "Atan"," 0x12F89E3A9C6574xb-052","-0x1F000000000000xb-106"," 0x1BD84F5C3DE8ADxb-053","-0x133AAD89DE9E59xb-107"},
  { "Atan","-0x10540EA7EFDBBCxb-052"," 0x1D25F323140A07xb-107","-0x19752EB1D9797Exb-053","-0x1CDBB459CBD678xb-109"},
  { "Atan","-0x1526E5B7E9C938xb+055"," 0x1CAD70B4358B1Dxb-109","-0x1921FB54442D18xb-052","-0x11A62633145C07xb-106"},
  { "Atan"," 0x1B2C9E5689BBE3xb+001"," 0x1CDB470AA4C082xb-054"," 0x1921FB54442D18xb-052","-0x1313BB8B7E9DEFxb-110"},
  { "Atan"," 0x1E0BF450C6ED7Axb-052"," 0x168A6CCA1101F2xb-113"," 0x114DC2BB7CDA3Fxb-052"," 0x15EF30FF984F2Bxb-106"},
  {"Log1p"," 0x1215EDA9E4590Fxb+277","-0x1C5352E35DB43Exb+221"," 0x1C855FDECA5361xb-045"," 0x1005C92AC1A364xb-099"},
  {"Log1p"," 0x15D9A945599AA1xb+360"," 0x16FC7C86053B64xb+303"," 0x11DE3651B9D18Exb-044","-0x13A396972826F9xb-098"},
  /* The following are from the doubledouble.cc QuickTest */
  { "Log" , "2.0","0.0"," 0x162E42FEFA39EFxb-053"," 0x1ABC9E3B39803Fxb-108"},
  {"Log1p","-0.5","0.0","-0x162E42FEFA39EFxb-053","-0x1ABC9E3B39803Fxb-108"},
  { "Atan", "1.0","0.0"," 0x1921FB54442D18xb-053"," 0x11A62633145C07xb-107"},
  { "Sin" ," 0x1F800000000000xb+03","0.0"," 0x1EE8CEB6765F34xb-053","-0x190913024A6072xb-110"},
  { "Cos" ," 0x1F800000000000xb+03","0.0"," 0x1090EA7862A521xb-054"," 0x115EC2B0E06C66xb-109"}
};
#elif XP_USE_ALT_DOUBLEDOUBLE > 0 && XP_DDFLOAT_MANTISSA_PRECISION == 53
// Not double-double; integral single piece variable with 53 bit mantissa
static DDTest test_data[] = {
  { "Sin", " 0x1B63A203211975xb-063","0.0"," 0x1B63A1CD9F70E5xb-063","0.0"},
  { "Sin", " 0x1AE63107DABDFFxb-062","0.0"," 0x1AE6303D1C8181xb-062","0.0"},
  { "Sin", " 0x1E3CDDBF797DE0xb-061","0.0"," 0x1E3CD93F83D55Bxb-061","0.0"},
  { "Sin", " 0x1FDAB5358684C5xb-061","0.0"," 0x1FDAAFF2C12506xb-061","0.0"},
  { "Sin", " 0x1462D9839253DCxb-071","0.0"," 0x1462D983923DCCxb-071","0.0"},
  { "Sin", " 0x198FF6AA45C726xb-067","0.0"," 0x198FF6AA1A479Bxb-067","0.0"},
  { "Sin", " 0x1BFDC33D3B5599xb-070","0.0"," 0x1BFDC33D3A7125xb-070","0.0"},
  { "Sin", " 0x102C5EA8067EECxb-060","0.0"," 0x102C5BE6EEE600xb-060","0.0"},
  { "Sin", " 0x1F07AB4DC1D60Dxb-061","0.0"," 0x1F07A670DD4011xb-061","0.0"},
  { "Sin", " 0x1A55644D8B9D75xb-061","0.0"," 0x1A556154A9CEB9xb-061","0.0"},
  { "Sin", " 0x127F5D26FADA7Fxb-059","0.0"," 0x127F4CAB9D4B5Axb-059","0.0"},
  { "Sin", " 0x1F5C5AA2166540xb-060","0.0"," 0x1F5C468D910C4Fxb-060","0.0"},
  { "Sin", " 0x1AE5E6E5DC3F4Dxb-053","0.0"," 0x17D733504E6D92xb-053","0.0"},
  { "Sin", " 0x1088C8ED5603B0xb-053","0.0"," 0x1F9DE1C74A5DDAxb-054","0.0"},
  { "Sin", " 0x10643ACF6C477Dxb-053","0.0"," 0x1F5E3C45931734xb-054","0.0"},
  { "Sin", " 0x1D4BC97F2EC1DCxb-051","0.0","-0x1FD2C097FDE742xb-054","0.0"},
  { "Sin", "-0x1066B4DC768CE4xb-053","0.0","-0x1F628D853552C6xb-054","0.0"},
  { "Sin", " 0x1C4BD513BECBC8xb-053","0.0"," 0x18C00DF8332F68xb-053","0.0"},
  {"Expm1","-0x11D68C323FAE01xb-061","0.0","-0x11D19454E983C8xb-061","0.0"},
  {"Expm1"," 0x1A01E4D0E06567xb-061","0.0"," 0x1A0C793862E12Dxb-061","0.0"},
  {"Expm1","-0x1D268FCAF5A391xb-063","0.0","-0x1D233E4910FE6Bxb-063","0.0"},
  {"Expm1"," 0x19AAED0382D057xb-054","0.0"," 0x1F93EAA7FBF38Dxb-054","0.0"},
  {"Expm1"," 0x171D0BA5C22506xb-054","0.0"," 0x1BD6A39A876095xb-054","0.0"},
  {"Expm1"," 0x12B6C730B3A0E9xb-046","0.0"," 0x1FDF656F9685B3xb+055","0.0"},
  {"Expm1"," 0x129ED5CD6751E8xb-047","0.0"," 0x1A7CE6C47BB387xb+001","0.0"},
  {"Expm1"," 0x18D79B2251A4ECxb-051","0.0"," 0x1550BD16FAA047xb-048","0.0"},
  {"Expm1"," 0x17771DF8F23A36xb-056","0.0"," 0x1893090E06E5C3xb-056","0.0"},
  {"Expm1"," 0x1671B0F65E2498xb-054","0.0"," 0x1AE208032A3E9Fxb-054","0.0"},
  {"Expm1"," 0x12320DCD88D6E6xb-048","0.0"," 0x1308FCEBC8BE2Fxb-026","0.0"},
  {"Expm1","-0x1914CDE63AD880xb-057","0.0","-0x187A12DC840E25xb-057","0.0"},
  {"Expm1"," 0x170DB1A1807C88xb-056","0.0"," 0x181F95C8A523C5xb-056","0.0"},
  { "Atan","-0x1B34F9451B4E41xb-058","0.0","-0x1B3355E26AC7EDxb-058","0.0"},
  { "Atan"," 0x18FA84DA5078E5xb-056","0.0"," 0x18E6575D3355E8xb-056","0.0"},
  { "Atan"," 0x1636D246457F04xb-052","0.0"," 0x1E4A8DF8EEB719xb-053","0.0"},
  { "Atan"," 0x13D1A06E1E93EExb-052","0.0"," 0x1C881754F9E417xb-053","0.0"},
  { "Atan"," 0x19E1E5F2179B72xb-048","0.0"," 0x1883CE6740330Dxb-052","0.0"},
  { "Atan","-0x115A2FCB2C9526xb-039","0.0","-0x1921854DEDCF30xb-052","0.0"},
  { "Atan","-0x12B0451F080D75xb-037","0.0","-0x1921DFEEC69EE9xb-052","0.0"},
  { "Atan"," 0x126078F2D47CCCxb-019","0.0"," 0x1921FB543D35FBxb-052","0.0"},
  { "Atan"," 0x111A4E5B243FB5xb-019","0.0"," 0x1921FB543CB126xb-052","0.0"},
  { "Atan"," 0x1714AD98125DBDxb+008","0.0"," 0x1921FB54442D18xb-052","0.0"},
  { "Atan"," 0x18B337A98D38E5xb+013","0.0"," 0x1921FB54442D18xb-052","0.0"},
  { "Atan","-0x12C2C3789E4090xb+044","0.0","-0x1921FB54442D18xb-052","0.0"},
  { "Atan"," 0x15248D25B7206Cxb+055","0.0"," 0x1921FB54442D18xb-052","0.0"},
  { "Atan"," 0x120AAC9DBC97CBxb+049","0.0"," 0x1921FB54442D18xb-052","0.0"},
  { "Atan"," 0x13FA66C95BD5B8xb-006","0.0"," 0x1921FB54442CE5xb-052","0.0"},
  { "Atan","-0x1662BFFB30E429xb-053","0.0","-0x13889DEB7092C7xb-053","0.0"},
  { "Atan"," 0x1DD31F9B84716Dxb-055","0.0"," 0x1D4D4862CE442Cxb-055","0.0"},
  { "Atan"," 0x1904EE64EE6927xb-075","0.0"," 0x1904EE64EE68D5xb-075","0.0"},
  { "Atan"," 0x12F89E3A9C6574xb-052","0.0"," 0x1BD84F5C3DE8ADxb-053","0.0"},
  { "Atan","-0x10540EA7EFDBBCxb-052","0.0","-0x19752EB1D9797Exb-053","0.0"},
  { "Atan","-0x1526E5B7E9C938xb+055","0.0","-0x1921FB54442D18xb-052","0.0"},
  { "Atan"," 0x1B2C9E5689BBE3xb+001","0.0"," 0x1921FB54442D18xb-052","0.0"},
  { "Atan"," 0x1E0BF450C6ED7Axb-052","0.0"," 0x114DC2BB7CDA3Fxb-052","0.0"},
  {"Log1p"," 0x1215EDA9E4590Fxb+277","0.0"," 0x1C855FDECA5361xb-045","0.0"},
  {"Log1p"," 0x15D9A945599AA1xb+360","0.0"," 0x11DE3651B9D18Exb-044","0.0"},
  /* The following are from the doubledouble.cc QuickTest */
  { "Log" , "2.0","0.0"," 0x162E42FEFA39EFxb-053","0.0"},
  {"Log1p","-0.5","0.0","-0x162E42FEFA39EFxb-053","0.0"},
  { "Atan", "1.0","0.0"," 0x1921FB54442D18xb-053","0.0"},
  { "Sin" ," 0x10000000000000xb-052","0.0","0x1AED548F090CEExb-053","0.0"},
  { "Cos" ," 0x10000000000000xb-052","0.0","0x114A280FB5068Cxb-053","0.0"}
};
#elif XP_USE_ALT_DOUBLEDOUBLE == 0 && XP_DDFLOAT_MANTISSA_PRECISION == 64
// Test data for XP_DDFLOAT_TYPE == IEEE 10-byte float (64 bit mantissa)
static DDTest test_data[] = {
  /* The following are from the doubledouble.cc QuickTest */
  { "Log" , "2.0","0.0"," 0xB17217F7D1CF79ACxb-64","-0xD871319FF0342543xb-130"},
  {"Log1p","-0.5","0.0","-0xB17217F7D1CF79ACxb-64"," 0xD871319FF0342543xb-130"},
  { "Atan", "1.0","0.0"," 0xC90FDAA22168C235xb-64","-0xECE675D1FC8F8CBBxb-130"},
  { "Sin" ," 0x1F800000000000xb+03","0.0"," 0xF74675B3B2F99F9Cxb-64","-0x913024A60724AB45xb-130"},
  { "Cos" ," 0x1F800000000000xb+03","0.0"," 0x848753C315290916xb-65","-0x9EA78FC9CD101EE2xb-132"}
};
#elif XP_USE_ALT_DOUBLEDOUBLE > 0 && XP_DDFLOAT_MANTISSA_PRECISION == 64
// Not double-double; integral single piece variable with 64 bit mantissa
static DDTest test_data[] = {
  /* The following are from the doubledouble.cc QuickTest */
  { "Log" , "2.0","0.0"," 0xB17217F7D1CF79ACxb-64","0x0000000000000000xb-0064"},
  {"Log1p","-0.5","0.0","-0xB17217F7D1CF79ACxb-64","0x0000000000000000xb-0064"},
  { "Atan", "1.0","0.0"," 0xC90FDAA22168C235xb-64","0x0000000000000000xb-0064"},
#if 0 // Skip tests that rely on accurate 2*pi reduction
  { "Sin" ," 0x1F800000000000xb+03","0.0"," 0xF74675B3B2F99F9Cxb-64","0x0000000000000000xb-0064"},
  { "Cos" ," 0x1F800000000000xb+03","0.0"," 0x848753C315290916xb-65","0x0000000000000000xb-0064"}
#endif

};
#else
// Dummy test, so test_data[] isn't empty
static DDTest test_data[] = {
  { "Sin", "0.0", "0.0", "0.0", "0.0" }
};
#endif

// Extra high precision input values
const Xp_BigFloatVec big_test_0_0625 = {  1, -35, 32, { 2147483648 } };
const Xp_BigFloatVec big_test_0_125  = {  1, -34, 32, { 2147483648 } };
const Xp_BigFloatVec big_test_m0_125 = { -1, -34, 32, { 2147483648 } };
const Xp_BigFloatVec big_test_0_25   = {  1, -33, 32, { 2147483648 } };
const Xp_BigFloatVec big_test_m0_25  = { -1, -33, 32, { 2147483648 } };
const Xp_BigFloatVec big_test_0_5    = {  1, -32, 32, { 2147483648 } };
const Xp_BigFloatVec big_test_m0_5   = { -1, -32, 32, { 2147483648 } };
const Xp_BigFloatVec big_test_1      = {  1, -31, 32, { 2147483648 } };
const Xp_BigFloatVec big_test_m1     = { -1, -31, 32, { 2147483648 } };
const Xp_BigFloatVec big_test_2      = {  1, -30, 32, { 2147483648 } };
const Xp_BigFloatVec big_test_2_5    = {  1, -30, 32, { 2684354560 } };
const Xp_BigFloatVec big_test_3      = {  1, -30, 32, { 3221225472 } };
const Xp_BigFloatVec big_test_m3     = { -1, -30, 32, { 3221225472 } };
const Xp_BigFloatVec big_test_4      = {  1, -29, 32, { 2147483648 } };

// Result values
const Xp_BigFloatVec big_sin_1 = {
  1, -32, 32,
  { 3614090360, 1214738464, 3337218313, 3306110002, 2313490707,  793873229,
    4021733983, 3603319229, 4211857919,  517788759, 1985328514, 1782397776,
    3261289827, 1576865003, 4270433829,  216728599,  691661016,  257644955,
     832601185, 1841778038, 3957597971, 1428787909,  333943189,  881076017,
    3830306373,  811410667, 3707506276, 1972909574, 2936138876, 2293414960,
    1757560066,  913698279, 1629206030,  230081268, 1375259654, 4058156127,
    2205477243, 3561164351, 3514418238, 3572603902, 1729027698,  403853332,
    3879269024, 1247978766, 1702187936,  300524549, 4195757482, 2373843694,
    3307562546, 3451666458, 1112329668, 1003720666, 2461991742, 4082310639,
    4115819316,  926400499,   60362059,  611290380, 2004239874, 4214875182,
    2888207192, 1397745625, 2085597570, 2646047527}
};

const Xp_BigFloatVec big_sin_2_5 = {
  1, -32, 32,
  { 2570418286, 2119533573, 2065268029,  887809200, 1119784058, 2727695858,
    2895062934,  231083683, 1533809456, 2166093421, 1442634718, 3962352006,
    1369907447, 3098313824, 1027733681,  353527796,  658668760, 1725606960,
    3534338927, 3648919938,  220715617,  179935895, 2997057587, 1120277776,
     639434751, 2416231060, 1484272125,  392500024, 3674348283,  220216411,
    3450639093,  277267294, 1094668809,  325698414, 3337811146, 3572888447,
    3759388551, 1864038970,  329786177, 3185752760, 4025266967, 1580344786,
    3906095780, 1212557250, 2779961897, 2928893225, 2366303401, 2076567275,
    2500142164, 2393017251, 1243031471,  364161504,  897504111, 3443814069,
    4033256472, 3621875776, 1807273310, 1688801539,  250695415, 1452234579,
    2447391099, 4161124989,  862014719,  338368423}
};

const Xp_BigFloatVec big_sin_m3 = {
 -1, -34, 32,
  { 2424423277, 3064650360, 3825967386,  206838361, 1808121589, 2524510009,
        302167, 1824226465,  396145816, 3065194461,   30527354, 1402604021,
    3565706781, 1974466211,  309147722, 1919996446, 3642901809,  447464973,
    1313210013, 1757507840, 1780646668,  854916983, 1082622913, 3602802546,
    3142326142, 1010412435,  175263574, 2147775698, 2955052826, 2129208261,
    2908277998, 2054521000,  214153490, 1812830045, 2435107019, 3521999461,
    4132867367,   83077529, 3925190005, 3944082653, 3429219925,  283002996,
     790810025, 3405001216, 1865066529, 3765037046, 1062532491, 1395007612,
    1930350893, 1417899864, 3703634236,  149741559,  687939097, 2952923498,
    2892087916,  321159156, 1055221382, 3551898956, 3523392993, 4040732656,
    2457300815, 2778766062, 3189356689, 2266533873}
};

const Xp_BigFloatVec big_cos_1 = {
  1, -32, 32,
  { 2320580733, 2822003857, 3259395479, 1752284457, 2721528457, 1335280567,
    4063232576, 3076581114, 2527675440, 2771193502, 1140016398,  999291291,
    3375644737, 3682730500, 2517039651, 1387553653, 2068903937, 4128253779,
    2808155519, 2675664974, 2949547781, 1967261468, 3131658303, 2482375634,
    1801000532, 2254968148, 4227380838, 1734614921, 1294076667, 1454696990,
    1052101784, 3034813002, 2976176930, 3425965637, 3524285261, 1028675772,
    4192526450, 3700464260, 1948766860, 1290766304, 2268314724, 1511643598,
    3712872817, 2174516999,  277947933, 2955752792, 1806045204, 3553579929,
    2202194763, 3703469681, 2587505057, 1525259785, 1314497141,  873203288,
    3207825899, 4231995476, 1381895150, 2728362472, 2885211730, 1766060889,
    1371018004,  677430975, 3994827845, 3197919010}
};

const Xp_BigFloatVec big_cos_2_5 = {
 -1, -32, 32,
  { 3440885628,  744221016, 2096800126, 2536064895,  525147087, 2984018965,
     486117072, 1154698527, 3121477671, 1644790838, 3624441078, 1852622576,
     471176754, 4043076720, 1861004368, 2480745748, 1346888310, 3945987646,
     533356314, 3433582987, 1332074060,  549020981,  388111403, 2272966353,
     371381847,  383183368, 2515795288, 3594873920, 1337649839, 3489695410,
    1188018907,  440145554, 2045958433,  752451217, 3993072289, 2172220754,
     376976509, 3438966769, 2486606631, 1738953522, 1407832175, 3938427648,
    4022194484, 3337458333, 3692470731, 2675948719, 3420639675,  708159631,
    1032218228, 3542952082, 1715986691, 1858841890, 4078456771, 2815731907,
    2396942011,  201046555, 2113715321, 3169740175,  372815050,  614886039,
    1934755216,  745855022, 3034894976, 1828462086}
};

const Xp_BigFloatVec big_cos_m3 = {
 -1, -32, 32,
  { 4251985396,  791581447, 3757584351, 1785746411, 1587848071,  543879107,
    1715879047,  922837243, 2341953879, 2332529158, 2185224703, 2057737902,
    2442074329, 2220323433,  531879192, 3290829343, 3658314917, 1989627981,
     732811707, 2812518183, 2017363857, 1193199844, 4288797971, 2950193976,
     107512675, 2201301411, 1167949015, 2316175609, 3288251699, 1847614531,
      66725257, 3578875394, 1257489372, 1681769142, 3441370014, 1170181755,
    2168015005, 3008767668, 2135459347, 2943691441, 1686263431, 1097197737,
    3289063413, 1059539320, 1845842400, 2671752266, 2927828198, 2158114412,
    3190368568,  104618948, 1540142981, 3229379864,  667182413, 1673800182,
     247650718, 2689692215, 2298377755, 3492904433, 3903879289,  147730366,
     553594825,  385922193,  835515388, 1564259767}
};

const Xp_BigFloatVec big_exp_1 = {
  1, -30, 32,
  { 2918732888, 2730183322, 2950452768,  658324721, 3636053379, 3459069589,
    2850108993,  342111227, 3432226254,  614153977, 2100290403, 1661760984,
    4135694850, 2932105594, 3554614997, 3590153569,  607384863, 1594257104,
    2237883733, 1038949107, 3042382686, 2136459573, 2555317360, 3773205367,
    3802565082, 4092586098,  502356129,  917366581,  816630351, 1211791738,
    3154817410, 3005545313, 3507005771, 2999510011, 3110787767, 1624765544,
     491733667, 3728297460, 2924932583, 1668463385,  185051080, 3993660784,
    2650995937, 3455574764, 3224634573,  674508641, 2440232604, 3917841407,
    2387546674, 4008870275, 3288218395, 1282387315, 1001782460,  784474117,
    3314479491, 2098627506, 3337832998, 3249729530, 2288730680, 1629474780,
    3728038715, 1696138075, 3157587166, 4187750968}
};

const Xp_BigFloatVec big_exp_2 = {
  1, -29, 32,
  { 3966969286, 2800578145, 2635976963, 1531272660,  601308539,  663818198,
    3017469742, 4196482601,  251943342, 4040501979, 4275619091, 4276775272,
    3913809073,   71550315,  687829590, 1471635481,   15364218,  568180643,
    1562481385, 1707568696, 1012543567, 1128148670, 4227228347, 1435505523,
     789479903, 1653514977, 2404069429, 3036787573, 3221012870, 3513101104,
    2734993049,   93567350, 4016185171, 1846154737, 2791328243, 3037059144,
    1072231844, 1035111857, 4144177347, 3985885507,  843772341,  760597990,
    3960642734, 3718161114, 3510959239, 1209332141, 1048469567, 1272295384,
    2119465623, 1239534473, 3403018739, 1724453725, 1781716441, 1203784148,
    3730983441, 2213919949, 4247771433, 2486068464, 3914079077, 2481549046,
    2808417221,  500901643, 1646928265,  401856718}};

const Xp_BigFloatVec big_exp_m3 = {
  1, -36, 32,
  { 3421341286, 1971680842, 1093671196, 2215143427, 2693456796, 3694234721,
    1198663608, 4276371218,  987702118,  792005659, 2637941245, 1057547658,
     673833801,  580878855, 3191201720, 3668699188, 1286575947, 3338320923,
    2345989708, 3703079720,  327371328, 1336083925,  877523052, 2930660284,
    3616370030, 2715463114, 1483553087, 1118973582, 2148085295, 3890516932,
     744340344, 1134144136, 3903209588, 1413388328, 3047447893,  744642058,
     488787315, 2301298868,  936274313, 3733764737,  947982573,  829304704,
    1399151224,  130190334, 3613066670, 2284168942, 1036813333, 3356729082,
    3544721285, 2541080766, 2584124959,  164209541, 3478288829,  366057308,
    4063726588,  452221208, 2890298007, 3356784973, 1439097720, 2196308254,
    2051130963, 3700815003, 1968274721, 1233153650}
};

const Xp_BigFloatVec big_expm1_2_5 = {
  1, -28, 32,
  { 3001777865, 2399525098, 1868670708,  620537659,  208939239, 3751945283,
    2619069454, 2990628527, 2662345556, 1414531533, 3646422207,  425621775,
    3451129574, 2487706142, 3279321736, 3786540054, 3299436194, 1969296631,
     617249846, 1852515756,  835391544, 2855299561, 1324835702,  992416893,
    4044394040, 1236837217, 2400082195, 2968394363, 2252400601, 3857894005,
    4243765367,  839182743, 4058074502, 3208270945, 2379293789, 1935496922,
    2016922019, 1679063971,  344509099,  579325657, 1223795820,  945024857,
    2194788576,  295124300, 2987228139, 2390579649,  417220130, 2584225892,
    1122111566, 1113090082, 2674045994, 1315007000, 2742745183, 3961668429,
    2605695647, 1756494758, 4176587991, 3588414275, 3300703120, 2925495549,
    3476771398,  342552638,  304367871, 1643923126}
};

const Xp_BigFloatVec big_expm1_m0_125 = {
 -1, -35, 32,
  { 4037375684, 2675416606,  175557382, 2232789034,  335458418, 1147582769,
    1596340674, 1856195086,  179940594, 2833883247, 1711212602, 2446838952,
    2721808022, 3141209252, 1043005187, 1506475050,  904976093, 1793644190,
    4067037260, 1486626846, 3750458147, 2618458301, 1917756972, 1055106194,
    2217271810, 4284214624,  902411863, 3485353165, 1212773455,  283095776,
    3317622512, 2832536656,  421214910, 2500553175, 1569614146, 1294084529,
    1794320837,  105591048, 3480596329, 3142485673, 1417000415, 1183603462,
    1292301138, 2369576103, 2434319043, 3847597687,  658949023, 1284214212,
     758275863, 1702578554,  991603875,  312619854, 2434087477, 3085409655,
    1658753202, 2670264498, 3152800190, 2398845327,  379276113, 3618756188,
    2243477454,  586184165,  566611998, 3852691362}};

const Xp_BigFloatVec big_expm1_0_0625 = {
  1, -35, 32,
  { 2216012734, 2591473325, 2365640465,  776817293, 3511331756, 1041381318,
     616480878, 4093440573,  793698754, 1568317829, 1865611092,  281737044,
    1243300620, 1411924841, 1295830892, 1053259543, 2746929692,  910072463,
    2197858108,  952949024,  716129521,  658085551, 2423927173, 1614668203,
    4181752823,  871409174, 2543559563, 1274952381, 3774696461,  601170664,
    1759607784, 4126715437, 2849633821,  671400003, 1677851351, 2931736755,
     975903424, 2768219460, 2691850361, 2854756442, 1993558954, 1400990065,
     605611634, 3456582415, 4194114423, 1600428215, 3499150270, 3591651583,
    1948288929, 1409307097,  273756028, 3318746467, 3314377248, 2277000311,
     288071805, 1968957621, 2318074021, 3160723421,  626950574, 1753545072,
    1287384799, 1177753428, 1573413796,  293953940}
};

const Xp_BigFloatVec big_log_0_25 = {
 -1, -31, 32,
  { 2977044471, 3520035243, 3387143064,   66254511, 1089684262, 1922610733,
    2316113755, 2343238187, 3887625760, 1844161688, 1435849467, 1257904912,
    3979259445, 3241681220,  660028201,  292141093, 1050069526,  575334597,
     449567249,  830224510, 3119160259, 2973130811,  628072684, 1985476427,
    1926137777, 2640660682, 4125075133,  942682696, 1700766087,  790684578,
    3660421061,  255841734,  133483025, 4217109392, 1628254456, 2414170530,
    3998653805, 4229888533, 2100175838,  335590294,  390465397, 3683225829,
    3376670514, 3027881267, 3435941477, 2475905356, 1276780043, 3520465245,
     627481395,  895787831, 1788641162, 1578405506,  122533377, 1560181411,
     206047828,  389352748, 2505920889, 2984344477, 2922462427, 1818258609,
     126841693,  455979803, 1599124760, 1348780427}
};

const Xp_BigFloatVec big_log_2 = {
  1, -32, 32,
  { 2977044471, 3520035243, 3387143064,   66254511, 1089684262, 1922610733,
    2316113755, 2343238187, 3887625760, 1844161688, 1435849467, 1257904912,
    3979259445, 3241681220,  660028201,  292141093, 1050069526,  575334597,
     449567249,  830224510, 3119160259, 2973130811,  628072684, 1985476427,
    1926137777, 2640660682, 4125075133,  942682696, 1700766087,  790684578,
    3660421061,  255841734,  133483025, 4217109392, 1628254456, 2414170530,
    3998653805, 4229888533, 2100175838,  335590294,  390465397, 3683225829,
    3376670514, 3027881267, 3435941477, 2475905356, 1276780043, 3520465245,
     627481395,  895787831, 1788641162, 1578405506,  122533377, 1560181411,
     206047828,  389352748, 2505920889, 2984344477, 2922462427, 1818258609,
     126841693,  455979803, 1599124760, 1348780427}
};

const Xp_BigFloatVec big_log_3 = {
  1, -31, 32,
  { 2359251925, 1746425019, 1376569002, 2191375962, 3741983084, 3689957112,
     516683637,  681876166, 1431028073,  107170856,  674823065, 2177714026,
    2972584579, 2404638288, 3885755512, 2282906418, 1130908273,  255486034,
    2750396232, 4266179237, 2407896977, 3695247475, 2495657793, 1511993906,
    1749819490, 1022898854, 4071514151, 1012002700, 2050358665, 2181581907,
    3840030404,  502053073, 2136100061, 3229020335,  379644809, 3768642897,
    1424791318, 3831087801, 1452084716, 3988366437, 1258404617, 2441972543,
     205622846, 2571921796, 1687567222, 2714928488, 3842136750, 2806555173,
     837108065, 3628376684, 3402157253, 2964418203,  386167461,  795606078,
    4049745745, 2319712718,  756994086, 3105000802, 3815529175,  275262964,
    2482186748, 1766063851, 2366695198, 4120700947}
};

const Xp_BigFloatVec big_log1p_2_5 = {
  1, -31, 32,
  { 2690287989, 2850749901, 2522378651, 2434036939, 2241938846, 3993093350,
    2452363696, 4087735303,  722764705,  211960148,  503778349,  349384144,
    4118192705, 1327631497, 3143544301,  489194193, 3860035511, 3040899561,
    4083930698, 1791044304, 2148032598,  658801798, 4278987843, 2397206135,
    3653370818, 4198421412, 2823475454, 3263940455, 3147072147, 2350652124,
    1382314050,  309593116,  703675736, 1961523290, 1899803536, 2907982116,
    3484263738, 3337301220, 1947873172, 1073089213, 3327024200, 2275248907,
    1733132026, 2706354480, 2239034663, 3153920308, 3303712480, 2938535552,
    4056233280, 4102289261, 2693347001, 2238952198, 2255165645,  990730167,
    3828270579, 2578732308, 3505034011, 2864219476, 1606828271,  861618425,
    1679153736, 2319934743,  794526027, 4166885087}
};

const Xp_BigFloatVec big_log1p_m0_5 = {
 -1, -32, 32,
  { 2977044471, 3520035243, 3387143064,   66254511, 1089684262, 1922610733,
    2316113755, 2343238187, 3887625760, 1844161688, 1435849467, 1257904912,
    3979259445, 3241681220,  660028201,  292141093, 1050069526,  575334597,
     449567249,  830224510, 3119160259, 2973130811,  628072684, 1985476427,
    1926137777, 2640660682, 4125075133,  942682696, 1700766087,  790684578,
    3660421061,  255841734,  133483025, 4217109392, 1628254456, 2414170530,
    3998653805, 4229888533, 2100175838,  335590294,  390465397, 3683225829,
    3376670514, 3027881267, 3435941477, 2475905356, 1276780043, 3520465245,
     627481395,  895787831, 1788641162, 1578405506,  122533377, 1560181411,
     206047828,  389352748, 2505920889, 2984344477, 2922462427, 1818258609,
     126841693,  455979803, 1599124760, 1348780427}
};

const Xp_BigFloatVec big_log1p_m0_125 = {
 -1, -34, 32,
  { 2294051857, 1059315441, 2623148003, 2532577053, 3666865212,  616008247,
    3204967764, 3223892261, 3844051963,  172710433, 3161601649, 2973198847,
    3183501219, 2427495891, 1606707679, 2718542490, 3290075891, 1750316761,
     989863478,  903376241, 3474053996, 1334762913,  857449799, 1001129628,
    3362004853,  422816050, 1822862835, 2904774405, 1314453405,  405161524,
    1044986903, 3864956238, 4028392908,  864819631, 2122574655,  344474608,
    4115120537, 2845731208, 1218421326, 2689943234, 2277333354, 2673880787,
     263406016, 2572214298,  985319918, 3165814972,  964409689,  360470241,
    2634755986,  117792334, 1352287878, 3305561052,  118811041,  260642649,
    1086989059, 3959799998,  597029616,  961000010, 1935138657, 3358154173,
     466405540, 2268229665, 2141822562, 3224966495}
};

const Xp_BigFloatVec big_log1p_0_0625 = {
  1, -36, 32,
  { 4166092288, 2333422347, 3863710583, 1503103373,  880071479, 1532123495,
     408023209, 1869186198,  388867773, 3898671209,  141700504, 1538116777,
    2668413406, 2056542697, 3157620887, 2120991078, 2413794773, 4083884750,
     257262658, 1011737996, 3461897687, 1025627949,  647150542, 4003280264,
    4181591353, 2160998115, 3317813961, 2669445056, 4272281531, 2239379039,
    2247946842, 1570536838, 1499865289,  909067020, 1064372939, 4252968028,
     665877744, 1850803612,  350791512, 2596706504, 2519929532,  755000777,
    3662131145, 4219328744, 1829596977, 2315756322, 4048231846, 3812171919,
     712056383, 3292335292,  784528920, 3861093679,  329026487,  887891051,
    1223382030, 3111943302, 2094215950, 1206341021,  669952405, 1354096082,
    3957602285, 1597142684, 3936270469, 3277352043}
};

const Xp_BigFloatVec big_atan_m0_25 = {
 -1, -34, 32,
  { 4208701385, 1678174997, 1841798901, 4154595301, 2860493059, 2293465963,
    2050651772, 2487854949, 2461607190, 1759868499,   96373851, 2776832574,
    1009500711,  119426970,  255968171,   60392313, 2659891011, 3824327255,
    4136224965, 1566301328,  147923799, 2900521479, 3777089087, 1046203181,
    3147069343, 1739081747,  208415126, 1309219068, 3565194244, 1555559673,
    3629787867, 3568501817, 3170149388, 3638726905, 3738260944,  396531316,
    4161559288, 1426504346,  992229598, 2729412731, 1406998856, 1874147148,
    3804578589, 2527517193, 2762091143, 3960773715, 1877747831, 4092777635,
    2206064380, 1679452759, 1759263107, 3135791573,  616191025, 4250505018,
    4069069340, 2618129741,  488793247, 3892900876, 1023773086, 3538745668,
    2709797520, 2489719304, 4184553472, 3108486300}
};

const Xp_BigFloatVec big_atan_1 = {
  1, -32, 32,
  { 3373259426,  560513588, 3301335691, 2161908945,  688016904, 2322058356,
      34324134,  991140642, 1363806329, 2385773789, 4019526067, 3443147547,
     808127085, 4066317367, 1340159341, 1834074693, 3833967990, 1650360006,
    4098638569, 2788683115,  201284790, 4094081005, 3996675067, 1518968741,
    2929665041, 2085298150, 1227384401, 3974388541, 3254811832, 2707668741,
    2564442166,  475386778, 1763065768, 4247048031, 2204458275, 3701714326,
     476246870,  545608379, 2664769799, 1888917101, 1728853326, 1253873668,
    4050938888, 3390579068,  848322118,  775343675, 3818813228,  403604995,
    2603058082, 3959923343, 3049610736, 1867272905, 3727412214, 2505578264,
     966084988, 3935660773,  366093848, 2566522128,  359829082, 2326447149,
    2905806605,   72383027, 2824151467, 3743201892}
};

const Xp_BigFloatVec big_atan_4 = {
  1, -31, 32,
  { 2847171752, 4108838098,  386756268, 3253197268, 1941068008,  424762374,
    2462347222, 2827642421, 2666718166, 3239532051, 2396866600, 1485430739,
    1755681321,  293292612,  234421496,  215912918, 2964610701, 3856673659,
    4118481360, 4203508185,  182794315, 4268386732, 4061409843, 1925064255,
    4146894109, 2404783843, 3885687071,  589510686,  661678904,  365740134,
    1573847770, 2713678611,  829926183, 1644723520, 1200304745, 3652147911,
    2103535607,  367295336, 1466999275, 2621482333, 4237333029, 1019605275,
    1427882917,  390284858, 4261157109,  817117873, 1973482013,  428878703,
     716687299, 1602508100, 3366573759, 4159653519,  966033776, 1437394224,
    3678676793, 1460910907, 1915607428, 2616780430, 2379341094, 2957845765,
     419598266, 4056135410, 2301082283, 3354641105}
};

static DDTest xhp_test_data[] = {
  {"Sin",   big_test_1,      big_sin_1 },
  {"Sin",   big_test_2_5,    big_sin_2_5},
  {"Sin",   big_test_m3,     big_sin_m3},
  {"Cos",   big_test_1,      big_cos_1},
  {"Cos",   big_test_2_5,    big_cos_2_5},
  {"Cos",   big_test_m3,     big_cos_m3},
  {"Exp",   big_test_1,      big_exp_1},
  {"Exp",   big_test_2,      big_exp_2},
  {"Exp",   big_test_m3,     big_exp_m3},
  {"Expm1", big_test_2_5,    big_expm1_2_5},
  {"Expm1", big_test_m0_125, big_expm1_m0_125},
  {"Expm1", big_test_0_0625, big_expm1_0_0625},
  {"Log",   big_test_0_25,   big_log_0_25},
  {"Log",   big_test_2,      big_log_2},
  {"Log",   big_test_3,      big_log_3},
  {"Log1p", big_test_2_5,    big_log1p_2_5},
  {"Log1p", big_test_m0_5,   big_log1p_m0_5},
  {"Log1p", big_test_m0_125, big_log1p_m0_125},
  {"Log1p", big_test_0_0625, big_log1p_0_0625},
  {"Atan",  big_test_m0_25,  big_atan_m0_25},
  {"Atan",  big_test_1,      big_atan_1},
  {"Atan",  big_test_4,      big_atan_4}
};

// Template that returns the size of a C-style array
template <typename T,size_t S>
inline size_t ArraySize(const T (& /* arr */)[S]) { return S; }

////////////////////////////////////////////////////////////////////////


void Usage()
{
  fprintf(stderr,"Usage: ddtest func"
          " [-quiet] [-verbose[=#]] [-nozero] [-repeat count]"
          " <minval> <maxval> <pointcount> [...]\n");
  fprintf(stderr," or\n");
  fprintf(stderr,"       ddtest func -oneshot <pointhi> <pointlo> [...]\n");
  fprintf(stderr," or\n");
  fprintf(stderr,"       ddtest basetest [-slack amount]\n");
  fprintf(stderr," or\n");
  fprintf(stderr,"       ddtest quicktest\n");
  fprintf(stderr," or\n");
  fprintf(stderr,"       ddtest formatsample\n");
  fprintf(stderr," or\n");
  fprintf(stderr,"       ddtest test [-slack amount] <file|->\n");
  fprintf(stderr,"Supported functions---\n");
  size_t istop = sizeof(func_array)/sizeof(func_array[0]);
  for(size_t i=0;i<istop;++i) {
    fprintf(stderr,"%18s",func_array[i].name);
    if((i+1)%4==0 || (i+1)==istop) fputs("\n",stderr);
  }
  exit(1);
}

////////////////////////////////////////////////////////////////////////

// Routine to return a list (vector) of double-double points
// across a specified range.

void FillRange(WORK_FLOAT minval,WORK_FLOAT maxval,
               size_t pointcount,int nozeros,
               std::vector<Xp_DoubleDouble>& pts)
{
  WORK_FLOAT startA=0,stopA=0, startB=0,stopB=0;
  size_t pointcountA=0, pointcountB=0;
  if(minval<0.0 && maxval>0.0) {
    // Two ranges
    startA = minval;
    stopA = 0;
    startB = 0;
    stopB = maxval;
    pointcountA = (size_t)(XP_FLOOR(0.5-pointcount*minval/(maxval-minval)));
    pointcountB = pointcount - pointcountA;
    if(!nozeros) {
      // Set one aside for zero
      if(pointcountB>pointcountA) --pointcountB;
      else                        --pointcountA;
    }
  } else if(maxval<=0.0) {
    // All negative
    startA = minval;
    stopA = maxval;
    pointcountA = pointcount;
    if(!nozeros && stopA == 0.0) --pointcountA; // Set one aside for zero
  } else {
    // All positive
    startB = minval;
    stopB = maxval;
    pointcountB = pointcount;
    if(!nozeros && startB == 0.0) --pointcountB;  // Set one aside for zero
  }
  if(pointcountA>0) {
    if(stopA == 0.0) {
      stopA = -startA/pointcountA;
    }
  }
  if(pointcountB>0) {
    if(startB == 0.0) {
      startB = stopB/pointcountB;
    }
  }

  pts.clear();
  pts.reserve(pointcount);
  WORK_FLOAT xprev, xnow=startA, xnext=startA;
  for(size_t i=0;i<pointcount;++i) {
    Xp_DoubleDouble xt;
    xprev=xnow; xnow=xnext;
    if(i<pointcountA) {
      // Pick point in first range
      if(i==0) { // Always include first point
        xprev = xnow = xnext = startA;
      } else if(i+1==pointcount) { // Always include last point
        xprev = xnow = xnext = stopA;
      } else if(i+2<pointcountA) {
        WORK_FLOAT lambda = (WORK_FLOAT)(i+1)/(pointcountA-1);
        xnext = -1*pow(fabs(startA),1-lambda)*pow(fabs(stopA),lambda);
      } else {
        xnext = stopA;
      }
    } else if(i >= pointcount-pointcountB) {
      // Pick point in second range
      if(i == pointcount-pointcountB) { // second range start
        xprev = xnow = xnext = startB;
      } else if(i+1==pointcount) { // Always include last point
        xprev = xnow = xnext = stopB;
      } else if(i+2<pointcount) {
        WORK_FLOAT lambda = (WORK_FLOAT)(pointcount-i-2)/(pointcountB-1);
        xnext = pow(startB,lambda)*pow(stopB,1-lambda);
      } else {
        xnext = stopB;
      }
    } else { // Zero point
      xprev = xnow = xnext = 0.0;
    }

    // Create two random numbers in the range [-1:1], then adjust
    // size of latter one and create an Xp_DoubleDouble.
    XP_DDFLOAT_TYPE x0
      = 2*(XP_DDFLOAT_TYPE(rand())/XP_DDFLOAT_TYPE(RAND_MAX)-0.5);
    XP_DDFLOAT_TYPE x1
      = 2*(XP_DDFLOAT_TYPE(rand())/XP_DDFLOAT_TYPE(RAND_MAX)-0.5);
    x1 *= pow(XP_DDFLOAT_TYPE(2.0),-XP_DDFLOAT_MANTISSA_PRECISION);
    xt = x0; xt += x1;

    // Adjust size of random variation to fit halfway between xprev and
    // xnext. This doesn't match logarithmic profile;  Look! A squirrel!
    // (So, rather that adding offset to xnow, which may land outside the
    // interval, add it to the interval midpoint.)
    xt *= static_cast<XP_DDFLOAT_TYPE>(0.5*xnext-0.5*xprev);

    // Set double-double test point
#if 1
    xt += static_cast<XP_DDFLOAT_TYPE>(0.5*xnext+0.5*xprev);
#else
    xt += static_cast<XP_DDFLOAT_TYPE>(xnow);
    if(xt.Hi()<xprev)      xt = static_cast<XP_DDFLOAT_TYPE>(xprev);
    else if(xt.Hi()>xnext) xt = static_cast<XP_DDFLOAT_TYPE>(xnext;
#endif
    pts.push_back(xt);
  }
}

void FillRange(WORK_FLOAT minval,WORK_FLOAT maxval,
               size_t pointcount, int nozeros, int argc,
               std::vector<Xp_DoubleDouble>& pts)
{ // NB: If argc == 1, then the return vector contains exactly
  //     pointcount Xp_DoubleDouble elements. If argc!=1, then the
  //     return vector contains roughly argc * pointcount
  //     Xp_DoubleDouble elements.  In particular, it may contain a
  //     good bit more.
  if(argc == 1) {
    FillRange(minval,maxval,pointcount,nozeros,pts);
  } else if(argc == 2) {
    // Fill two vectors, and form pts from Cartesian product
    size_t pcA = (size_t)XP_CEIL(sqrt((WORK_FLOAT)(pointcount)));
    if(pcA<1) pcA=1;
    size_t pcB = (size_t)XP_CEIL((WORK_FLOAT)(pointcount)/pcA);
    std::vector<Xp_DoubleDouble> ptA;
    std::vector<Xp_DoubleDouble> ptB;
    FillRange(minval,maxval,pcA,nozeros,ptA);
    FillRange(minval,maxval,pcB,nozeros,ptB);
    pts.clear();
    pts.reserve(ptA.size()*ptB.size());
    for(size_t i=0;i<ptA.size();++i) {
      for(size_t j=0;j<ptB.size();++j) {
        pts.push_back(ptA[i]);
        pts.push_back(ptB[j]);
      }
    }
  } else {
    fprintf(stderr,"Unsupported function argument count: %d\n",
            argc);
    exit(11);
  }
}

////////////////////////////////////////////////////////////////////////

Xp_DoubleDouble global_yt; // Make this global to thwart optimization
/// in speed testing (-quiet) mode.

int wrapped_main(int argc,char** argv)
{
#ifdef _MSC_VER
  // Disable annoying "This application has requested the Runtime to
  // terminate it in an unusual way." pop-up dialog when asserts are
  // triggered:
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif // _MSC_VER
#ifdef __BORLANDC__
  // Borland C++ enables floating point exceptions by default.  Turn
  // this off to get rolling Infs and NaNs.
  std::_control87(MCW_EM, MCW_EM); // Defined in float.h
#endif

  int quiet=0;
  int verbose=1;  // Standard level
  int oneshot=0;
  int nozeros=0;
  int repeat_count=1;

  // For standard Xp_DoubleDouble allowed slack is 0.5 ulp.
  // Otherwise default is to allow up to 1.0 ulp.
#if XP_USE_ALT_DOUBLEDOUBLE == 0
  double comparison_slack=0.5;
#else
  double comparison_slack=1.0;
#endif
#if XP_USE_FLOAT_TYPE == XP_MPFR_TYPE                    \
  && defined(MPFR_VERSION_MAJOR) && MPFR_VERSION_MAJOR<4
  comparison_slack = 5.0;
#endif

  double run_start_tick = GetTicks();

  // Pull options out of command line
  for(int i=1;i<argc;++i) {
    int eat_count=0;
    if(strcmp("--version",argv[i])==0) {
      cout << "ddtest version 1.0\n";
      return 0;
    } else if(strcmp("-q",argv[i])==0 || strcmp("-quiet",argv[i])==0) {
      // Quiet request (used for timing)
      quiet=1;
      eat_count=1;
    } else if(strcmp("-v",argv[i])==0 || strcmp("-verbose",argv[i])==0) {
      // Verbose request (dump info)
      verbose=2; // Standard + double-double type info
      eat_count=1;
    } else if(strncmp("-verbose=",argv[i],9)==0) {
      // Verbose request (dump info) with level spec
      verbose=atoi(argv[i]+9);
      eat_count=1;
    } else if(strcmp("-slack",argv[i])==0 && i+1<argc) {
      // Allowed slack (file test mode)
      comparison_slack = atof(argv[i+1]);
      eat_count=2;
    } else if(strcmp("-oneshot",argv[i])==0) {
      // Single point
      oneshot=1;
      eat_count=1;
    } else if(strcmp("-nozero",argv[i])==0) {
      // No zeros
      nozeros=1;
      eat_count=1;
    } else if(strcmp("-repeat",argv[i])==0 && i+1<argc) {
      // Repeat computation (for timing tests)
      repeat_count = atoi(argv[i+1]);
      if(repeat_count<0) Usage(); // Allow 0 for testing
      eat_count=2;
    }
    if(eat_count>0) {
      for(int j=i;j+eat_count<argc;++j) argv[j]=argv[j+eat_count];
      argc-=eat_count;
      --i;
    }
  }
  DDTest::SetComparisonSlack(comparison_slack);

  if(verbose>=2) {
#if XP_USE_ALT_DOUBLEDOUBLE==0
    cerr << "Standard Xp_DoubleDouble; base type="
         << XP_USE_FLOAT_TYPE_INFO
         << ", mantissa width=2*"
         << XP_DDFLOAT_MANTISSA_PRECISION << "+1="
         << Xp_DoubleDouble::GetMantissaWidth() << " bits total\n";
#else
    cerr << "Alternative Xp_DoubleDouble; variable type="
         << XP_USE_FLOAT_TYPE_INFO
         << ", mantissa width="
         << Xp_DoubleDouble::GetMantissaWidth() << " bits\n";
#endif
# if XP_USE_FLOAT_TYPE == XP_MPFR_TYPE
#  ifdef BOOST_VERSION
    cerr << "BOOST version " << xp_stringify(BOOST_VERSION);
#  endif
#  if defined (BOOST_VERSION) && defined(MPFR_VERSION_STRING)
    cerr << ", ";
#  endif
#  ifdef MPFR_VERSION_STRING
    cerr << "MPFR version " << MPFR_VERSION_STRING;
#  endif
#  if defined (BOOST_VERSION) || defined(MPFR_VERSION_STRING)
    cerr << "\n";
#  endif
# endif // XP_USE_FLOAT_TYPE == XP_MPFR_TYPE
  }

  DDTest::SetVerbose(verbose);

  // Check for "quicktest" request
  if(argc==2 && strcmp("quicktest",argv[1])==0) {
    // Quicktest
    if(Xp_DoubleDouble::QuickTest()) {
      fprintf(stderr,"QuickTest failure.\n");
      return 1;
    }
    printf("QuickTest passed.\n");
    return 0;
  }

  // Check for "formatsample" request
  if(argc==2 && strcmp("formatsample",argv[1])==0) {
    // Sample test values:
    std::vector<Xp_DoubleDouble> tests =
      { Xp_DoubleDouble::DD_PI,
        ldexp(Xp_DoubleDouble::DD_PI,520),
        ldexp(Xp_DoubleDouble::DD_PI,1022),
        ldexp(Xp_DoubleDouble::DD_PI,-1024+55-0),
        ldexp(Xp_DoubleDouble::DD_PI,-1024+55-3),
        ldexp(Xp_DoubleDouble::DD_PI,-1024+55-6),
        ldexp(Xp_DoubleDouble::DD_PI,-1024+55-59),
        -999999.75,
        Xp_DoubleDouble(1,-XP_DDFLOAT_EPSILON/4),
        Xp_DoubleDouble(1,-XP_DDFLOAT_EPSILON/12),
        Xp_DoubleDouble(XP_DDFLOAT_EPSILON/12)
      };
    int testno=0;
    for(const auto& it : tests) {
      if(testno != 0) std::cout << "\n";
      std::cout << "--- TEST "
                << std::setw(2) << testno++
                << ", VAL: "
                << std::scientific << std::setw(20)
                << std::setprecision(12)
                << it.Hi() << " ---\n";
      std::cout << std::fixed; // Use anything other than scientific to
                               // get HexFloatFormat for "it".
      std::cout << "+++ HEXVAL: " << it << "\n";
      std::cout << std::scientific;
      for(int ip=0;ip<38;++ip) {
        std::cout << " check " << std::setw(2)
                  << ip << ": "
                  << std::setw(0) << std::setprecision(ip)
                  << it << "\n";
      }
    }
    return 0;
  }

  // Check for "basetest" request
  if(argc==2 && strcmp("basetest",argv[1])==0) {
    // Base test built into this file
    int error_count = 0;
    const int test_count = static_cast<int>(ArraySize(test_data));
    const int xhp_test_count = static_cast<int>(ArraySize(xhp_test_data));
    for(int i=0;i<test_count;++i) {
      if(!test_data[i].Test()) {
        fprintf(stderr,"Test #%d failed\n",i);
        ++error_count;
      } else if(verbose>=3) {
        fprintf(stderr,"Test #%d passed\n",i);
      }
    }
    for(int i=0;i<xhp_test_count;++i) {
      if(!xhp_test_data[i].Test()) {
        fprintf(stderr,"Test #%d failed\n",test_count+i);
        ++error_count;
      } else if(verbose>=3) {
        fprintf(stderr,"Test #%d passed\n",test_count+i);
      }
    }
    if(error_count>0) {
      fprintf(stderr,"ERROR: %d/%d tests failed.\n",
              error_count,test_count+xhp_test_count);
      return 1;
    }
    printf("All %d tests passed.\n",test_count+xhp_test_count);
    return 0;
  }

  // Check for "test file" request
  if(argc==3 && strcmp("test",argv[1])==0) {
    const char* filename = argv[2];
    // Test from file.  Each line is one test.
    // The test format is:
    //
    //   index_number function xhi xlo yhi ylo zhi zlo  ulp  error/ulp
    //
    // where xhi,xlo and yhi,ylo are the inputs, zhi+zlo is the
    // reference result, ulp is the size of the unit in the last
    // place, and error/ulp is the difference between the reference
    // result zhi+zlo and the true result, in ULPs.  The number of
    // inputs (xhi,lo yhi,lo) depend on the function.  The last column
    // (error/ulp) should be <= 0.5 for all algebraic functions, and
    // not more than a smidgen more than that for transcendental
    // functions.
    //
    // The "test" routine flags an error if the live result computed by
    // doubledouble.cc doesn't exactly match the reference result.  In
    // principle this could be incorrect for the transcendentals, if the
    // reference result is more than 0.5 ulp in error; if this occurs in
    // practice then we can add a secondary check that computes the
    // error directly.  (The error/ulp value is signed.)  The user can
    // adjust the size of allowed error via the command line -slack
    // option.
    //
    // Lines in the test file starting with a '#' are comments, which
    // are ignored.

    // The filename "-" is used to request reading from stdin
    std::string line;
    ifstream checkfile;
    istream* testfileptr = &(std::cin);
    if(strcmp("-",filename)!=0) {
      checkfile.open(filename);
      if(!checkfile.is_open()) {
        std::string errmsg = "Error: unable to open test file \"";
        errmsg += filename;
        errmsg += "\"\n";
        cerr << errmsg;
        throw errmsg;
      }
      testfileptr = &(checkfile);
    }
    istream& testfile(*testfileptr);

    int test_count = 0, error_count = 0;
    while(line.clear(),getline(testfile,line)) {
      // Is this a comment line?
      size_t icmt = line.find_first_not_of(" \t");
      if(icmt != std::string::npos && line[icmt] == '#') {
        continue; // Comment line
      }

      // Otherwise, split line
      std::string sepchars = " \t\r,[]";  // Field separator characters
      std::vector<std::string> fields;
      size_t field_start = 0, field_end = 0;
      const size_t line_length = line.length();
      while(1) {
        field_start = line.find_first_not_of(sepchars,field_end);
        if(field_start == std::string::npos) break;
        field_end = line.find_first_of(sepchars,field_start);
        if(field_end == std::string::npos) field_end = line_length;
        fields.push_back(line.substr(field_start,field_end-field_start));
      }
      if(fields.size()<8 || fields.size()>10) {
        fprintf(stderr,"Invalid test line: %s\n",line.c_str());
        continue;
      }

      // Build test object and run test
      std::vector<std::string> testdata;
      for(size_t i=2;i<fields.size()-2;++i) {
        testdata.push_back(fields[i]);
      }
      try {
        DDTest test(fields[1].c_str(),testdata);
        WORK_FLOAT refulp = ScanFloat(fields[fields.size()-2].c_str());
        WORK_FLOAT referror = ScanFloat(fields[fields.size()-1].c_str());
        if(!test.Test(referror,refulp)) {
          fprintf(stderr,"Test #%s failed\n",fields[0].c_str());
          ++error_count;
        }
      } catch(...) {
        cerr << "Error processing test " << fields[0] << "\n";
        throw;
      }
      ++test_count;
    }
    if(checkfile.is_open()) checkfile.close();
    if(error_count>0) {
      fprintf(stderr,"ERROR: %d/%d tests failed.\n",error_count,test_count);
      return 1;
    }
    printf("All %d tests passed.\n",test_count);
    return 0;
  }

  // Pull function name out of command line
  if(argc<2) Usage();
  const char* funcname = argv[1];
  for(int j=1;j+1<argc;++j) argv[j]=argv[j+1];
  --argc;
  const FuncInfo* func_info = FindFuncInfo(funcname);
  if(func_info == 0) {
    fprintf(stderr,"ERROR: Unrecognized function: \"%s\"\n",funcname);
    Usage();
  }
  int func_argc = (func_info->ArgPartCount()+1)/2;
  /// ArgPartCount returns the number of XP_DDFLOAT_TYPE arguments,
  /// which will be 2, 3, or 4 corresponding to one DD, one DD and one
  /// SD, or two DD args.  func_argc is the number of args of either
  /// type.

  // Check remaining arguments
  int chunk_argc = 3;
  std::vector<WORK_FLOAT> oneshot_args; // Used in oneshot case
  if(!oneshot) {
    if(argc<4 || argc % chunk_argc != 1) Usage();
    // Argument check
    for(int loop=1;loop+2<argc;loop+=3) {
      WORK_FLOAT minval = ScanFloat(argv[loop]);
      WORK_FLOAT maxval = ScanFloat(argv[loop+1]);
      long int pointcount = atol(argv[loop+2]);
      if(pointcount<1 || minval>maxval) {
        printf("Bad input: %Lg %Lg %ld\n",
               (long double)minval,(long double)maxval,pointcount);
        Usage();
      }
    }
  } else { // Oneshot
    // Combine all remaining arguments into one string, and then split
    // string at whitespace, commas, and square brackets.  This supports
    // input using the [#,#] notations for double-double values.
    std::string buffer;
    for(int i=1;i<argc;++i) {
      buffer += argv[i];
      buffer += " ";
    }
    size_t start,stop=0;
    const char* splitchars = " \t\r\n,[]";
    while(stop<buffer.size() &&
          (start=buffer.find_first_not_of(splitchars,stop))!=string::npos) {
      if((stop=buffer.find_first_of(splitchars,start))==string::npos) {
        stop=buffer.size();
      }
      std::string subbuf = buffer.substr(start,stop-start);
      oneshot_args.push_back(ScanFloat(subbuf.c_str()));
    }
    chunk_argc = func_info->ArgPartCount();
    if(oneshot_args.size()<2 || oneshot_args.size()%chunk_argc != 0) Usage();
  }

  if(!quiet) {
    // Output header
    if(func_argc == 1) {
      fprintf(stdout,"#%6s %-15s %-52s   %s\n",
              "Index","   x approx","   xt",func_info->name);
    } else if(func_argc == 2) {
      fprintf(stdout,"#%6s %-15s %-15s %-52s %-52s   %s\n",
              "Index","   x approx","   y approx",
              "   xt","   yt",
              func_info->name);
    } else {
      fprintf(stderr,"PROGRAMMING ERROR: unsupported function type\n");
      exit(1);
    }
  }

  // Loop through points
  BIGINT total_point_count=0;
  double total_ticks = 0;
  int loop_start=1;
  int loop_stop=argc;
  if(oneshot) {
    loop_start=0;
    loop_stop=static_cast<int>(oneshot_args.size());
  }
  for(int loop=loop_start;loop+chunk_argc<=loop_stop;loop+=chunk_argc) {
    WORK_FLOAT minval=0.0;
    WORK_FLOAT maxval=0.0;
    size_t pointcount=0;
    std::vector<Xp_DoubleDouble> pts;

    if(!oneshot) {
      minval = ScanFloat(argv[loop]);
      if(minval > XP_DDFLOAT_MAX)       minval =  XP_DDFLOAT_MAX;
      else if(minval < -XP_DDFLOAT_MAX) minval = -XP_DDFLOAT_MAX;
      maxval = ScanFloat(argv[loop+1]);
      if(maxval > XP_DDFLOAT_MAX)       maxval =  XP_DDFLOAT_MAX;
      else if(maxval < -XP_DDFLOAT_MAX) maxval = -XP_DDFLOAT_MAX;
      pointcount = (size_t)atol(argv[loop+2]);
      FillRange(minval,maxval,pointcount,nozeros,func_argc,pts);
    } else {
      // One shot
      WORK_FLOAT ptlo,pthi;
      switch(func_info->func_type) {
      case FuncInfo::FT_DD:
        pthi = oneshot_args[loop];
        ptlo = oneshot_args[loop+1];
        pts.push_back(Xp_DoubleDouble(XP_DDFLOAT_TYPE(pthi),
                                      XP_DDFLOAT_TYPE(ptlo)));
        break;
      case FuncInfo::FT_DD_DD:
        pthi = oneshot_args[loop];
        ptlo = oneshot_args[loop+1];
        pts.push_back(Xp_DoubleDouble(XP_DDFLOAT_TYPE(pthi),
                                      XP_DDFLOAT_TYPE(ptlo)));
        pthi = oneshot_args[loop+2];
        ptlo = oneshot_args[loop+3];
        pts.push_back(Xp_DoubleDouble(XP_DDFLOAT_TYPE(pthi),
                                      XP_DDFLOAT_TYPE(ptlo)));
        break;
      case FuncInfo::FT_DD_SD:
        pthi = oneshot_args[loop];
        ptlo = oneshot_args[loop+1];
        pts.push_back(Xp_DoubleDouble(XP_DDFLOAT_TYPE(pthi),
                                      XP_DDFLOAT_TYPE(ptlo)));
        pthi = oneshot_args[loop+2];
        pts.push_back(Xp_DoubleDouble(XP_DDFLOAT_TYPE(pthi)));
        break;
      case FuncInfo::FT_SD_DD:
        pthi = oneshot_args[loop];
        pts.push_back(Xp_DoubleDouble(XP_DDFLOAT_TYPE(pthi)));
        pthi = oneshot_args[loop+1];
        ptlo = oneshot_args[loop+2];
        pts.push_back(Xp_DoubleDouble(XP_DDFLOAT_TYPE(pthi),
                                      XP_DDFLOAT_TYPE(ptlo)));
        break;
      default: throw "Invalid FuncType";
      }
    }

    double start_tick = GetTicks();
    for(int repeat_loop=0;repeat_loop<repeat_count;++repeat_loop) {

      if(!quiet) {
        for(size_t i=0;
            i + func_argc <= pts.size();
            i += func_argc) {
          Xp_DoubleDouble* xt = &(pts[i]);

          // Write index number and input point value to stdout
          if(func_info->func_type == FuncInfo::FT_DD) {
            fprintf(stdout,"%7.0f %15.6Le %-52s ",
                    double(total_point_count),
                    (long double)(xt[0].Hi()),DDWrite(xt[0]).c_str());
          } else if(func_info->func_type == FuncInfo::FT_DD_DD) {
            fprintf(stdout,"%7.0f %15.6e %15.6e %-52s %-52s ",
                    double(total_point_count),
                    (double)(xt[0].Hi()),(double)(xt[1].Hi()),
                    DDWrite(xt[0]).c_str(),DDWrite(xt[1]).c_str());

          } else if(func_info->func_type == FuncInfo::FT_DD_SD) {
            Xp_DoubleDouble tmp(xt[1].Hi()); // Sets low word to 0.0
            fprintf(stdout,"%7.0f %15.6Le %15.6Le %-52s %-52s ",
                    double(total_point_count),
                    (long double)(xt[0].Hi()),(long double)(xt[1].Hi()),
                    DDWrite(xt[0]).c_str(),DDWrite(tmp).c_str());
          } else if(func_info->func_type == FuncInfo::FT_SD_DD) {
            Xp_DoubleDouble tmp(xt[0].Hi()); // Sets low word to 0.0
            fprintf(stdout,"%7.0f %15.6Le %15.6Le %-52s %-52s ",
                    double(total_point_count),
                    (long double)(xt[0].Hi()),(long double)(xt[1].Hi()),
                    DDWrite(tmp).c_str(),DDWrite(xt[1]).c_str());
          } else {
            fprintf(stderr,"Unsupported function argc\n");
            exit(12);
          }

          // Evaluate test function(s) at xt and print results
          Xp_DoubleDouble yt;
          try {
            if(func_info->func_type == FuncInfo::FT_DD) {
              yt = reinterpret_cast<XDD_1P>(func_info->fptr)(xt[0]);
            } else if(func_info->func_type == FuncInfo::FT_DD_DD) {
              yt = reinterpret_cast<XDD_2P>(func_info->fptr)(xt[0],xt[1]);
            } else if(func_info->func_type == FuncInfo::FT_DD_SD) {
              yt = reinterpret_cast<XDD_2P_dd_sd>
                (func_info->fptr)(xt[0],xt[1].Hi());
            } else if(func_info->func_type == FuncInfo::FT_SD_DD) {
              yt = reinterpret_cast<XDD_2P_sd_dd>
                (func_info->fptr)(xt[0].Hi(),xt[1]);
            } else {
              fprintf(stderr,"Unsupported function argc\n");
              exit(13);
            }
          } catch(const char* errmsg) {
            fprintf(stderr,"ERROR: %s\n",errmsg);
            throw;
          }
          if(!yt.IsNormalized()) {
            fprintf(stderr,"*** ERROR: Unnormalized output ***\n");
            fprintf(stderr," Pt=%8.0f xt = %15.6Le = %-52s",
                    double(total_point_count),
                    (long double)(xt[0].Hi()),DDWrite(xt[0]).c_str());
            for(int j=1;j<func_argc;++j) {
              fprintf(stderr,"             xt[%d] = %15.6Le = %-52s",
                      j,(long double)(xt[j].Hi()),DDWrite(xt[j]).c_str());
            }
            fprintf(stderr,"             yt = %15.6Le = %-52s",
                    (long double)(yt.Hi()),DDWrite(yt).c_str());
            throw "RENORMALIZATION ERROR";
          }
          fprintf(stdout,"%-52s\n",DDWrite(yt).c_str());
          ++total_point_count;
        }
      } else {
        // Quiet (timing) mode
        if(func_info->func_type == FuncInfo::FT_DD) {
          XDD_1P fptr = reinterpret_cast<XDD_1P>(func_info->fptr);
          for(size_t i=0;i + func_argc <= pts.size(); ++i) {
            Xp_DoubleDouble* xt = &(pts[i]);
            global_yt = fptr(xt[0]);
            assert(global_yt.IsNormalized());
          }
          total_point_count += pts.size();
        } else if(func_info->func_type == FuncInfo::FT_DD_DD) {
          XDD_2P fptr = reinterpret_cast<XDD_2P>(func_info->fptr);
          for(size_t i=0;i + func_argc <= pts.size();i += 2) {
            Xp_DoubleDouble* xt = &(pts[i]);
            global_yt = fptr(xt[0],xt[1]);
            assert(global_yt.IsNormalized());
          }
          total_point_count += pts.size()/2;
        } else if(func_info->func_type == FuncInfo::FT_DD_SD) {
          XDD_2P_dd_sd fptr = reinterpret_cast<XDD_2P_dd_sd>(func_info->fptr);
          for(size_t i=0;i + func_argc <= pts.size();i += 2) {
            Xp_DoubleDouble* xt = &(pts[i]);
            global_yt = fptr(xt[0],xt[1].Hi());
            assert(global_yt.IsNormalized());
          }
          total_point_count += pts.size()/2;
        } else if(func_info->func_type == FuncInfo::FT_SD_DD) {
          XDD_2P_sd_dd fptr = reinterpret_cast<XDD_2P_sd_dd>(func_info->fptr);
          for(size_t i=0;i + func_argc <= pts.size();i += 2) {
            Xp_DoubleDouble* xt = &(pts[i]);
            global_yt = fptr(xt[0].Hi(),xt[1]);
            assert(global_yt.IsNormalized());
          }
          total_point_count += pts.size()/2;
        } else {
          fprintf(stderr,"Unsupported function argc\n");
          exit(13);
        }
      }
    }
    total_ticks += (GetTicks() - start_tick);
  }
  if(quiet) {
    printf("Eval time: %.2f seconds/%.0f points  = %.2f ns/pt\n",
           total_ticks,
           double(total_point_count),
           1e9*total_ticks/double(total_point_count));
    printf("Run  time: %.2f seconds\n",(GetTicks()-run_start_tick));
  }

  return 0;
}

// To test behavior near zero, gradual underflow needs to be enabled
// (i.e., no flush to zero or treating subnormals as zero).  We can
// force this behavior on x86 hardware with the Oc_FpuControlData class.
// To use this class, however, one must link in the Oc library.  The Oc
// library has "main()".  So to use the Oc library an application needs
// to have Oc_AppMain() instead of main().  Also, the Oc library also
// brings in dependencies for the Tcl and Tk libraries, so those must
// also be included on the link line.
//   An alternative is to compile the main() function with a compiler
// option disabling flush-to-zero, such as the -no-ftz option for the
// Intel C++ compiler (icpc).  Note that such options typically only
// work if employed during compile of main(), because all subsequent
// routine calls inherit the FPU control word.
int Oc_AppMain(int argc,char** argv)
{
  // Disable flush to zero
  Oc_FpuControlData fpctrl;  fpctrl.ReadData();
  Oc_FpuControlData::SetNoFlushToZero();

  try {
    return wrapped_main(argc,argv);
  } catch(const char* msg) {
    cerr << msg;
  }  catch(const std::string& msg) {
    cerr << msg;
  } catch(...) {
    cerr << "Uncaught exception";
  }
  cerr << "\n";
  return 9;
}
