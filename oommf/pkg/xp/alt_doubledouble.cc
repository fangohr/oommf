/* FILE: alt_doubledouble.cc
 * Main source file for alternative Xp_DoubleDouble class,
 * which is a simple wrapper for a user-defined floating
 * point type (usually long double or long long double).
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 */

// Enable long double printf support in MinGW.  The
// __USE_MINGW_ANSI_STDIO macro needs to be set before
// the system header files are #include'd.
#if defined(__MINGW32__) && !defined(__USE_MINGW_ANSI_STDIO)
# define __USE_MINGW_ANSI_STDIO 1
#endif

#include <cctype>
#include <cstdio>
#include <cstdlib>

#include <iomanip>
#include <iostream>
#include <limits>  // For std::numeric_limits<T>::infinity()
#include <string>

#include <cstring>

#include <sstream>

#include "alt_doubledouble.h"

/* End includes */

#if XP_USE_FLOAT_TYPE == XP_MPFR_TYPE && defined(BOOST_HAS_EXPM1)
#include <boost/math/special_functions/expm1.hpp>
using boost::math::expm1;
#endif

#if XP_USE_FLOAT_TYPE == XP_MPFR_TYPE && defined(BOOST_HAS_LOG1P)
#include <boost/math/special_functions/log1p.hpp>
using boost::math::log1p;
#endif

// Specify std namespace to get automatic overloading of basic math
// library functions.  We want this rather than explicitly calling, say,
// std::atan(x) because if x is a non-base type (e.g. an mpfr type) then
// we want overloading outside to a function outside of the std
// namespace.
using namespace std;

// Constants
// Each of the following constants carry 64*32 = 2048 bits of precision,
// or approximately 2048*log(2)/log(10) ~ 600 decimal digits.  They were
// computed using the Tcl proc XpBigFloatVecInitString in ddtest.tcl
// (requires the Tcl Mpexpr extension).
const Xp_BigFloatVec big_pi = {
  1, -30, 32,
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

const Xp_BigFloatVec big_e = {
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

const Xp_BigFloatVec big_log2 = {
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

const Xp_BigFloatVec big_sqrt2 = {
  1, -31, 32,
  { 3037000499, 4192101508, 1501399475, 1967832735,  493838522, 2302388300,
    3977751685, 2201196821, 1258062596,  985178819, 2830237295, 3699628857,
     259303518, 1134328650, 2013562678, 1039804264, 3525324423, 1118773070,
    1392876640,  297546619, 3400645777,  378029450, 1175108722,  796675635,
    3335891107, 2344085980, 3408552771,  838060109, 4113502735, 2205046830,
    3936657545, 2420165194, 2168015542, 3640463101, 4107509804, 3955834380,
    1113905852, 3576215121, 3100587035, 1870804679,   17095681, 4264703279,
     858414751, 2077892287, 4137795135, 3391230727, 3735052865, 4155146690,
    3254566366, 2734152755,  411273774, 2952131506, 3528411766, 1068284034,
    4261287695, 3551655808, 4188851777, 2038376453, 3259398238, 2459267107,
    1607438203, 3393102651, 1299996837, 3862160353}
};

const Xp_BigFloatVec big_sin1 = {
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

const Xp_BigFloatVec big_cos1 = {
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

// Externally accessible values:
const XP_DDFLOAT_TYPE
Xp_DoubleDouble::REF_ZERO = static_cast<XP_DDFLOAT_TYPE>(0.0);
const Xp_DoubleDouble
Xp_DoubleDouble::DD_SQRT2(big_sqrt2);
const Xp_DoubleDouble
Xp_DoubleDouble::DD_LOG2(big_log2);
const Xp_DoubleDouble
Xp_DoubleDouble::DD_PI(big_pi);
const Xp_DoubleDouble
Xp_DoubleDouble::DD_HALFPI(0.5*big_pi);


int
Xp_DoubleDouble::QuickTest()
{
  int error_count = 0;
  Xp_DoubleDouble truth;
  Xp_DoubleDouble log2check(2.0,0.0);
  log2check = log(log2check);
  truth = Xp_DoubleDouble(big_log2);
  if(log2check.a != truth.a) {
    ++error_count;
    cerr << " Log2: " << log2check << "\n"
         << "TRUTH: " << truth << "\n";
  }

  Xp_DoubleDouble loghalfcheck(-0.5,0.0);
  loghalfcheck = log1p(loghalfcheck);
  truth = Xp_DoubleDouble(big_log2);
  if(loghalfcheck.a != -truth.a) {
    ++error_count;
    cerr << "-Log2: " << loghalfcheck << "\n"
         << "TRUTH: " << -truth << "\n";
  }

  Xp_DoubleDouble picheck(1.0,0.0);
  picheck = static_cast<Xp_DoubleDouble>(4)*atan(picheck);
  truth = Xp_DoubleDouble(big_pi);
  if(picheck.a != truth.a) {
    ++error_count;
    cerr << "   pi: " << picheck << "\n"
         << "TRUTH: " << truth << "\n";
  }

  // Skip tests relying on accurate 2*pi reductions (at least until an
  // accurate 2*pi reduction routine is coded for alt_doubledouble).
  // Instead, simply check sin(1) and cos(1)
  Xp_DoubleDouble sin1(big_sin1);
  Xp_DoubleDouble cos1(big_cos1);

  XP_DDFLOAT_TYPE scinput = 1.0;
  Xp_DoubleDouble sincheck = sin(Xp_DoubleDouble(scinput));
  Xp_DoubleDouble coscheck = cos(Xp_DoubleDouble(scinput));
  if(sincheck.a != sin1.a) {
    ++error_count;
    cerr << " sin(" << scinput << "): "
         << sincheck << "\n"
         << "  TRUTH: " << sin1 << "\n";
  }
  if(coscheck.a != cos1.a) {
    ++error_count;
    cerr << " cos(" << scinput << "): "
         << coscheck << "\n"
         << "  TRUTH: " << cos1 << "\n";
  }
  // Interestingly, the computed value for single precision sinf(1.0f)
  // is not correctly rounded by some libraries, but is off by 0.53
  // ULPs.  However, the alt_doubledouble.h version of sin is inlined,
  // and depending on the compiler and optimization level sin(1) may be
  // evaluated as a compile time constant, and it can occur that such
  // calculation returns the correctly rounded value where as the
  // dynamic evaluation of sinf(1.0f) does not.  Is more accurate a bug
  // or a feature?

  return error_count;
}

// Size of unit-in-the-last-place.
XP_DDFLOAT_TYPE Xp_DoubleDouble::ULP() const
{
  if(!Xp_IsFinite(a)) return 0.0; // What else?
  if(a == 0.0) {
     // Note: pow may flush to zero, so use LdExp instead.
    return LdExp(1,XP_DDFLOAT_VERYTINY_EXP);
  }
  int exp;
  XP_FREXP(a,&exp);
  exp -= XP_DDFLOAT_MANTISSA_PRECISION;
  exp = (exp < XP_DDFLOAT_VERYTINY_EXP ? XP_DDFLOAT_VERYTINY_EXP : exp);
  return LdExp(1,exp);
}

// Compute difference, relative to refulp.  Returns absolute difference
// if refulp is zero.
XP_DDFLOAT_TYPE
Xp_DoubleDouble::ComputeDiffULP(const Xp_DoubleDouble& ref,
                                XP_DDFLOAT_TYPE refulp) const {
  XP_DDFLOAT_TYPE result0 = Hi() - ref.Hi();
  // Corresponding ::Lo() is always zero.
  if(refulp != 0.0) {
    if(refulp<1.0/XP_DDFLOAT_POW_2_MANTISSA) {
      // Protect against compilers that mess up division by subnormals
      refulp *= XP_DDFLOAT_POW_2_MANTISSA;
      result0 *= XP_DDFLOAT_POW_2_MANTISSA; // Assume this doesn't overflow
    }
    result0 /= refulp;  // Underflow protection
  }
  return result0;
}

// There is a duplicate of the following function in doubledouble.cc.
// Might want to find a way to maintain only one copy.
std::ostream& operator<<(std::ostream& os,const Xp_DoubleDouble& x) {
  std::ios_base::fmtflags original_flags = os.flags();
  if((original_flags & std::ios_base::scientific)
     == std::ios_base::scientific) {
    // Use scientific format
    os << ScientificFormat(x,static_cast<int>(os.width()),
                           static_cast<int>(os.precision()));
  } else {
    const int fieldwidth = HexFloatWidth();
    os << "[" << std::setw(fieldwidth) << HexFloatFormat(x.Hi())
       << "," << std::setw(fieldwidth) << HexFloatFormat(x.Lo()) << "]";
  }
  os.flags(original_flags);
  return os;
}

void Xp_DoubleDouble::ReadHexFloat(const char* buf,const char*& endptr)
{ // Reads two hex-float values from buf, makes a DoubleDouble value
  // from them, and returns a pointer to the first character beyond.
  // Throws a std::string on error.  Note that if values don't overlap
  // then smaller value is simply lost.
  const char* cptr;
  XP_DDFLOAT_TYPE val0 = ReadOneHexFloat(buf,cptr);
  if(cptr == 0) {
    std::string msg = "Bad input to ReadHexFloat (A): " + std::string(buf);
    throw msg;
  }
  XP_DDFLOAT_TYPE val1 = ReadOneHexFloat(cptr,cptr);
  if(cptr == 0) {
    std::string msg = "Bad input to ReadHexFloat (B): " + std::string(buf);
    throw msg;
  }
  if(Xp_IsFinite(val0)) {
    a = val0 + val1;
  } else {
    a = val0;
  }

  while(*cptr != ']' && *cptr != '\0') ++cptr;
  if(*cptr == ']') ++cptr;
  endptr = cptr;
}

std::string
ScientificFormat(const Xp_DoubleDouble& x,
                 int width, int precision)
{
  ostringstream fmt;
  fmt.precision(precision);
  fmt.width(width);
  fmt << std::scientific << x.a;
  return fmt.str();
}


// Miscellaneous
Xp_DoubleDouble& Xp_DoubleDouble::ReduceModTwoPi()
{ // Simple implementation that is less accurate than the method used in
  // doubledouble.cc; those techniques can be brought to bear here, if
  // necessary.
  int sign = 1;
  if(a<0.0) {
    sign = -1;
    a *= -1;
  }
  XP_DDFLOAT_TYPE n = floor(a/(2*DD_PI.Hi()));
  a -= static_cast<XP_DDFLOAT_TYPE>(2)*n*DD_PI.Hi();
  a *= sign;
  return *this;
}

// Friend functions.  The following definitions assume the library
// functions are polymorphic on XP_DDFLOAT_TYPE, i.e., take
// XP_DDFLOAT_TYPE on import and return an XP_DDFLOAT_TYPE.

Xp_DoubleDouble recip(const Xp_DoubleDouble& x)
{ return Xp_DoubleDouble(1.0/x.a); }

Xp_DoubleDouble sqrt(const Xp_DoubleDouble& x)
{ return Xp_DoubleDouble(sqrt(x.a)); }

Xp_DoubleDouble recipsqrt(const Xp_DoubleDouble& x)
{ return Xp_DoubleDouble(1.0/sqrt(x.a)); }

void sincos
(const Xp_DoubleDouble& x,
 Xp_DoubleDouble& sinx,Xp_DoubleDouble& cosx)
{
  sinx = Xp_DoubleDouble(sin(x.a));
  cosx = Xp_DoubleDouble(cos(x.a));
}

Xp_DoubleDouble atan
(const Xp_DoubleDouble& x)
{
  return Xp_DoubleDouble(atan(x.a));
}

Xp_DoubleDouble atan2
(const Xp_DoubleDouble& y,
 const Xp_DoubleDouble& x)
{
  XP_DDFLOAT_TYPE result;
  if(x.a==0.0 && y.a==0.0) {
    result = 0.0;
  } else {
    result = atan2(y.a,x.a);
  }
  return Xp_DoubleDouble(result);
}

Xp_DoubleDouble exp
(const Xp_DoubleDouble& x)
{
  return Xp_DoubleDouble(exp(x.a));
}

Xp_DoubleDouble log
(const Xp_DoubleDouble& x)
{
  return Xp_DoubleDouble(log(x.a));
}

Xp_DoubleDouble expm1
(const Xp_DoubleDouble& x)
{ // exp(x)-1
  // The expm1 function is in the standard C++ numeric library as of C++11
#if XP_USE_FLOAT_TYPE == XP_MPFR_TYPE && !defined(BOOST_HAS_EXPM1)
  XP_DDFLOAT_TYPE result;
  XP_DDFLOAT_TYPE y = exp(x.a);
  if(y == 1.0 || abs(x.a)>0.5) {
    y -= 1;
    result = y;
  } else {
    result = ((y-1)/log(y)) * x.a;
  }
#else // XPFR
  XP_DDFLOAT_TYPE result = expm1(x.a);
#endif // XPFR
  return static_cast<Xp_DoubleDouble>(result);
}

Xp_DoubleDouble log1p
(const Xp_DoubleDouble& x)
{ // log(1+x)
  // The log1p function is in the standard C++ numeric library as of C++11
#if XP_USE_FLOAT_TYPE == XP_MPFR_TYPE && !defined(BOOST_HAS_LOG1P)
  XP_DDFLOAT_TYPE y = x.a + 1;
  XP_DDFLOAT_TYPE result = x.a;
  XP_DDFLOAT_TYPE z = y - 1;  // Should we try making this atomic? (since MPFR doesn't support volatile)
  if(z != 0.0) {
    result *= (log(y)/z);
  }
#else // XPFR
  XP_DDFLOAT_TYPE result = log1p(x.a);
#endif // XPFR
  return static_cast<Xp_DoubleDouble>(result);
}

//////////////////////////////////////////////////////////////////////////
// Xp version of Nb_FindRatApprox (q.v.) for finding rational
// approximations.  Fills exports p and q with best result, and returns
// 1 or 0 according to whether or not p/q meets the specified relative
// error.
int Xp_FindRatApprox
(const Xp_DoubleDouble& xin,const Xp_DoubleDouble& yin,
 XP_DDFLOAT_TYPE relerr, XP_DDFLOAT_TYPE maxq,
 Xp_DoubleDouble& p,Xp_DoubleDouble& q)
{ // Uses Euclid's algorithm to find rational approximation p/q to x/y
  XP_DDFLOAT_TYPE x = xin.Hi();
  XP_DDFLOAT_TYPE y = yin.Hi();

  int sign = 1;
  if(x<0) { x *= -1;  sign *= -1; }
  if(y<0) { y *= -1;  sign *= -1; }

  int swap = 0;
  if(x<y) { XP_DDFLOAT_TYPE t = x; x = y; y = t; swap = 1; }

  const XP_DDFLOAT_TYPE x0 = x;
  const XP_DDFLOAT_TYPE y0 = y;

  XP_DDFLOAT_TYPE p0 = 0;  XP_DDFLOAT_TYPE p1 = 1;
  XP_DDFLOAT_TYPE q0 = 1;  XP_DDFLOAT_TYPE q1 = 0;

  XP_DDFLOAT_TYPE m = floor(x/y);

  XP_DDFLOAT_TYPE r = x - m*y;
  XP_DDFLOAT_TYPE p2 = m*p1 + p0;
  XP_DDFLOAT_TYPE q2 = m*q1 + q0;
  while(q2<maxq && fabs(x0*q2 - p2*y0) > relerr*x0*q2) {
    x = y;
    y = r;
    m = floor(x/y);
    r = x - m*y;
    p0 = p1; p1 = p2; p2 = m*p1 + p0;
    q0 = q1; q1 = q2; q2 = m*q1 + q0;
  }

  if(!swap) {
    p = p2;  q = q2;
  } else {
    p = q2;  q = p2;
  }
  return (fabs(x0*q2 - p2*y0) <= relerr*x0*q2);
}
