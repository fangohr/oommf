// FILE: mpfrtest.cc
//
// DESC: Development test file for using boost/mpfr in single float
//       alternative interface to Xp_DoubleDouble.
//
// LICENSE: Please see the file ../../LICENSE
//
// Sample build command:
//
//  g++ -std=c++11 mpfrtest.cc -o mpfrtest -lmpfr -DMANTDIG=32
//
// This program is not included in the OOMMF pimake build process.

#include <iostream>
#include <boost/multiprecision/mpfr.hpp>
#include <boost/math/special_functions/fpclassify.hpp>
#include <boost/math/special_functions/sign.hpp>
#include <boost/math/special_functions/expm1.hpp>
#include <boost/math/special_functions/log1p.hpp>
#include <boost/math/special_functions/next.hpp>
// No ulp.hpp in older boost installs?

using boost::math::isfinite;
using boost::math::signbit;
using boost::math::expm1;
using boost::math::log1p;

struct Xp_BigFloatVec {
public:
  int sign;    // +1 or -1
  int offset;  // Power-of-two offset of first chunk
  int width;   // Width of each chunk, in bits.
  std::vector<unsigned int> chunk;  // List of chunks
};

// Code to fill an object of type T with data from an Xp_BigFloatVec.
// This code assumes that type T can handle bit integer values.  (See
// the line marked "Integer sums".)
template <typename T>
T Xp_UnsplitBigFloat
(const Xp_BigFloatVec& data) // Import
{
  T two_m = pow(static_cast<T>(2),data.width);
  int n = data.chunk.size();
  T val = data.chunk[n-1];
  for(int i=n-2;i>=0;--i) {
    val = val/two_m + data.chunk[i];
    /// Note: No rounding error if T is a radix 2 float.
  }
  val *= data.sign*pow(2,data.offset);
  return val;
}

// The following representation of pi carries 64*32 = 2048 bits of
// precision, or approximately 2048*log(2)/log(10) ~ 600 decimal digits.
const Xp_BigFloatVec bigpi = {
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

int main(int,char**)
{
  using namespace boost::multiprecision;
  typedef number<mpfr_float_backend<MANTDIG> >   MYFLOAT;  // Decimal digits precision
  MYFLOAT x=1.0;
  MYFLOAT y=3.0;
  x /= y;
  std::cout << "x/y= " << x << std::endl;
  std::cout << "log(3)=" << log(y) << std::endl;
  std::cout << "signbit(x) =" << signbit(x) << std::endl;
  x *= -1;
  std::cout << "signbit(-x)=" << signbit(x) << std::endl;

  std::cout << "\ndouble        Digits: " << std::numeric_limits<double>::digits << std::endl;
  std::cout << "double      Digits10: " << std::numeric_limits<double>::digits10 << std::endl;
  std::cout << "double  max Digits10: " << std::numeric_limits<double>::max_digits10 << std::endl;
  std::cout << "double       has_inf: " << std::numeric_limits<double>::has_infinity << std::endl;
  std::cout << "double      has_qNaN: " << std::numeric_limits<double>::has_quiet_NaN << std::endl;
  std::cout << "double      has_sNaN: " << std::numeric_limits<double>::has_signaling_NaN << std::endl;
  std::cout << "double       min exp: " << std::numeric_limits<double>::min_exponent << std::endl;
  std::cout << "double       max exp: " << std::numeric_limits<double>::max_exponent << std::endl;
  std::cout << "double     min exp10: " << std::numeric_limits<double>::min_exponent10 << std::endl;
  std::cout << "double     max exp10: " << std::numeric_limits<double>::max_exponent10 << std::endl;
  std::cout << "double       DBL_MIN: " << std::numeric_limits<double>::min() << std::endl;
  std::cout << "double       DBL_MAX: " << std::numeric_limits<double>::max() << std::endl;
  std::cout << "double           EPS: " << std::numeric_limits<double>::epsilon() << std::endl;
  std::cout << "double           INF: " << std::numeric_limits<double>::infinity() << std::endl;
  std::cout << "double          qNaN: " << std::numeric_limits<double>::quiet_NaN() << std::endl;
  std::cout << "double          sNaN: " << std::numeric_limits<double>::signaling_NaN() << "\n\n";

  std::cout << "MYFLOAT       Digits: " << std::numeric_limits<MYFLOAT>::digits << std::endl;
  std::cout << "MYFLOAT     Digits10: " << std::numeric_limits<MYFLOAT>::digits10 << std::endl;
  std::cout << "MYFLOAT max Digits10: " << std::numeric_limits<MYFLOAT>::max_digits10 << std::endl;
  std::cout << "MYFLOAT      has_inf: " << std::numeric_limits<MYFLOAT>::has_infinity << std::endl;
  std::cout << "MYFLOAT     has_qNaN: " << std::numeric_limits<MYFLOAT>::has_quiet_NaN << std::endl;
  std::cout << "MYFLOAT     has_sNaN: " << std::numeric_limits<MYFLOAT>::has_signaling_NaN << std::endl;
  std::cout << "MYFLOAT      min exp: " << std::numeric_limits<MYFLOAT>::min_exponent << std::endl;
  std::cout << "MYFLOAT      max exp: " << std::numeric_limits<MYFLOAT>::max_exponent << std::endl;
  std::cout << "MYFLOAT    min exp10: " << std::numeric_limits<MYFLOAT>::min_exponent10 << std::endl;
  std::cout << "MYFLOAT    max exp10: " << std::numeric_limits<MYFLOAT>::max_exponent10 << std::endl;
  std::cout << "MYFLOAT      DBL_MIN: " << std::numeric_limits<MYFLOAT>::min() << std::endl;
  std::cout << "MYFLOAT      DBL_MAX: " << std::numeric_limits<MYFLOAT>::max() << std::endl;
  std::cout << "MYFLOAT          EPS: " << std::numeric_limits<MYFLOAT>::epsilon() << std::endl;
  std::cout << "MYFLOAT           INF: " << std::numeric_limits<MYFLOAT>::infinity() << std::endl;
  std::cout << "MYFLOAT          qNaN: " << std::numeric_limits<MYFLOAT>::quiet_NaN() << std::endl;
  std::cout << "MYFLOAT          sNaN: " << std::numeric_limits<MYFLOAT>::signaling_NaN() << std::endl;

  mpfr_float::default_precision(500); // 500 decimal digits precision
  mpfr_float piref = 1.0;
  piref = 4*atan(piref); // Note: I read that Boost constants (such as pi)
  /// defined in header files typically only carry about 100 decimal
  /// digits precision.

  for(int i = 10;i<125;i+=13) {
    mpfr_float::default_precision(i);
    mpfr_float tst = Xp_UnsplitBigFloat<mpfr_float>(bigpi);
    int digits = i;
    std::cout << "--- precision: " << i << " ---\n"
              << std::scientific << std::setprecision(digits-1)
              << "Pi computed: " << piref << std::endl
              << "Pi  unsplit: " << tst << std::endl;
  }

  // Note: boost::math::log1p() seems to need
  //  ::std::numeric_limits<T>::is_specialized
  // which means we can't use default mpfr_float with 0 (variable)
  // precision.
  number<mpfr_float_backend<MANTDIG> > log1p_tstMD = -0.125;
  log1p_tstMD = log1p(log1p_tstMD);
  std::cout << "\nLog1p(-0.125) at prec= " << MANTDIG << ": "
            << std::scientific << std::setprecision(MANTDIG-1)
            << log1p_tstMD << "\n";

  number<mpfr_float_backend<2*MANTDIG> > log1p_tst2MD = -0.125;
  log1p_tst2MD = log1p(log1p_tst2MD);
  std::cout << "Log1p(-0.125) at prec= " << 2*MANTDIG << ": "
            << std::scientific << std::setprecision(2*MANTDIG-1)
            << log1p_tst2MD << "\n";

  number<mpfr_float_backend<MANTDIG> > myulp = boost::math::float_next(log1p_tstMD) - log1p_tstMD;
  number<mpfr_float_backend<2*MANTDIG> > error = (log1p_tstMD-log1p_tst2MD)/myulp;
  std::cout << "Error: "
            << std::fixed << std::setprecision(5)
            << error << " ULPs\n";

  return 0;
}
