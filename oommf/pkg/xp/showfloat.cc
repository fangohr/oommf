/* File: showfloat.cc
 *
 * Utility program for displaying floating point values in three
 * formats: standard decimal form, "%a" hexfloat form, and "xb" hexbin
 * form.  This is a development tool, and is not built by the OOMMF
 * pimake build process.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 */

#include <cstring>

#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <limits>

/* end includes */

using std::copysign;

typedef double WORK_FLOAT;

inline WORK_FLOAT XP_FREXP(WORK_FLOAT x,int* exp)
 { return std::frexp(x,exp); }


WORK_FLOAT ScanHexBinFloat(const char* cptr)
{
  // Safer to work with unsigned chars
  const unsigned char* ptr = (const unsigned char*)cptr;

  // Skip leading junk
  unsigned char ch;
  while((ch=*ptr) != 0x0 && ch!='+' && ch!='-' &&
        !(('0'<=ch && ch<='9')                                  \
          || ('a'<=ch && ch<='f') || ('A'<=ch && ch<='F'))) {
    ++ptr;
  }
  // Set sign on mantissa
  int sign = 1;
  if(ch=='-') {
    sign = -1;
    ch = *(++ptr);
  } else if(ch=='+') {
    ch = *(++ptr);
  }

  // Skip leading "0x", if any
  if(ch == '0') {
    ch = *(++ptr);
    if(ch == 'x' || ch == 'X') {
      ch = *(++ptr);
    }
  }

  // Read mantissa
  WORK_FLOAT value = 0.0;
  while(ch != 0x0) {
    unsigned int digit;
    if(ch=='-') {
      sign = -1;
    } else if(ch=='+') {
      sign = 1;
    } else {
      if('0'<=ch && ch<='9') {
        digit = static_cast<unsigned int>(ch - '0');
      } else if('a'<=ch && ch<='f') {
        digit = static_cast<unsigned int>(ch - 'a' + 10);
      } else if('A'<=ch && ch<='F') {
        digit = static_cast<unsigned int>(ch - 'A' + 10);
      } else { // Bad value
        break;
      }
      value = 16*value + digit;
    }
    ch=*(++ptr);
  }

  // Exponent
  int exponent=0;
  double base=16.;
  if(ch=='x' || ch=='X') {
    ch=*(++ptr);
    if(ch=='b' || ch=='B') {
      base=2.;
      ++ptr;
    }
    exponent = atoi((const char*)(ptr));
  }

  // Put it all together
  return sign * value * pow(WORK_FLOAT(base),WORK_FLOAT(exponent));
}

WORK_FLOAT ScanC99HexFloat(const char* cptr)
{ // The format for the C99/C++11 hexfloat format is
  //    sOx1.mmm...mmmpseee         (hexfloat)
  // where s is an optional mantisa sign (+ or -), 0x is a literal
  // identifier indicating a hexadecimal value, 1. is the first part of
  // the normalized hexadecimal floating point value. mmm...mmm is an
  // arbitrary length run of hexadecimal digits (0-9, a-f, A-F),
  // followed by a literal 'p' (or 'P') indicating the start of the
  // base-2 exponent, then an optional exponent sign, and then a run of
  // decimal digits representing the power of 2.  Examples of the C99
  // hexfloat format:
  //
  //     0x1.68p13
  //
  // is the decimal number (1+6/16+8/16^2) * 2^13 = (360/256) * 2^13
  // = 360 * 2^5= 11520, and
  //
  //    -0x1.20Fp+5
  //
  // is the decimal number -(1*16^3 + 2*16^2 + 15) * 2^(5-12)
  // = -4623/128 = -36.1171875
  //
  // This is very similar to the HexBin format above, the difference
  // being the location of the hex point in the mantissa---this
  // shifts the exponent and re-jiggers the binary-to-hex grouping
  // in the mantissa.

  // Safer to work with unsigned chars
  const unsigned char* ptr = (const unsigned char*)cptr;

  // Skip leading junk
  unsigned char ch;
  while((ch=*ptr) != 0x0 && ch!='+' && ch!='-' &&
        !(('0'<=ch && ch<='9')                                  \
          || ('a'<=ch && ch<='f') || ('A'<=ch && ch<='F'))) {
    ++ptr;
  }
  // Set sign on mantissa
  int sign = 1;
  if(ch=='-') {
    sign = -1;
    ch = *(++ptr);
  } else if(ch=='+') {
    ch = *(++ptr);
  }

  // Skip leading "0x", if any
  if(ch == '0') {
    ch = *(++ptr);
    if(ch == 'x' || ch == 'X') {
      ch = *(++ptr);
    }
  }

  // Next two characters should be "1." or "0.", the latter only on
  // zero input.
  if(ch == '0') {
    // Optimization bugs will sometimes drop sign on sign*0.0, so use
    // copysign instead.
    return copysign(static_cast<WORK_FLOAT>(0.0),
                    static_cast<WORK_FLOAT>(sign));
  }
  if(ch != '1' || *(++ptr) != '.') {
    std::string errmsg
      = "Error in ScanC99HexFloat; Invalid C99 hexfloat string: ";
    errmsg += cptr;
    throw errmsg;
  }


  // Read mantissa
  WORK_FLOAT value = 1.0;
  int exponent = 0;
  while( (ch = *(++ptr)) != 0x0) {
    unsigned int digit;
    if('0'<=ch && ch<='9') {
      digit = static_cast<unsigned int>(ch - '0');
    } else if('a'<=ch && ch<='f') {
      digit = static_cast<unsigned int>(ch - 'a' + 10);
    } else if('A'<=ch && ch<='F') {
      digit = static_cast<unsigned int>(ch - 'A' + 10);
    } else { // Bad value
      break;
    }
    value = 16*value + digit;
    exponent -= 4;
  }

  // Exponent (base is 2)
  if(ch=='p' || ch=='P') {
    exponent += atoi((const char*)(++ptr));
  }

  // Put it all together
  return sign * value * pow(WORK_FLOAT(2),WORK_FLOAT(exponent));
}

WORK_FLOAT ScanFloat(const char* cptr)
{
  // If string contains an p or P, then calls ScanC99HexFloat.
  // If string contains an x or X, then calls ScanHexBinFloat.
  // Otherwise uses strtold()
  // Note: Check for 'p' first since C99HexFloat also contains
  //       an 'x' in the '0x' prefix.
  WORK_FLOAT value;
  if(strchr(cptr,'p') || strchr(cptr,'P')) {
    // Assume C99 hex-float
    value = ScanC99HexFloat(cptr);
  } else if(strchr(cptr,'x') || strchr(cptr,'X')) {
    // Assume hexhex-float
    value = ScanHexBinFloat(cptr);
  } else {
    // Assume decimal float
    value = static_cast<WORK_FLOAT>(strtold(cptr,0));
  }
  return value;
}

std::string HexBinaryFloatFormat(WORK_FLOAT value)
{
  char buf[1024];
  char* cptr = buf;
  WORK_FLOAT mantissa;
  int exp;
  mantissa = XP_FREXP(value,&exp);
  if(mantissa>=0.0) {
    *(cptr++) = ' ';  // Leave sign space
  } else {
    *(cptr++) = '-';
    mantissa *= -1;
  }

  // Write "0x" prefix to mantissa string
  *(cptr++) = '0';
  *(cptr++) = 'x';

  // Lead shift; if precision is not divisible by 4, then write
  // "left-over" bits (which won't fill a full hex digit) first.
  int mantissa_precision = 53;
  if(sizeof(WORK_FLOAT)>8) mantissa_precision = 64;

  if(mantissa_precision%4 != 0) {
    mantissa *= pow(WORK_FLOAT(2.0),mantissa_precision%4);
    exp -= mantissa_precision%4;
  } else {
    mantissa *= 16;
    exp -= 4;
  }

  // Write mantissa as hex digits
  for(int offset=0; 4*offset<mantissa_precision; ++offset) {
    int ival = static_cast<int>(mantissa); // i.e., floor(mantissa)
    if(ival<10) *(cptr++) = static_cast<char>('0' + ival);
    else        *(cptr++) = static_cast<char>('A' + ival - 10);
    mantissa -= ival;
    mantissa *= 16;
    exp -= 4;
  }

  exp += 4;
  if(sizeof(WORK_FLOAT)<=8) {
    snprintf(cptr,sizeof(buf)-(cptr-buf),"xb%+04d",exp);
  } else {
    snprintf(cptr,sizeof(buf)-(cptr-buf),"xb%+05d",exp);
  }
  return std::string(buf);
}

void Usage()
{
  fprintf(stderr,"Usage: showfloat value [value2 ...]\n");
  fprintf(stderr," where value is in decimal/hex/hexbin floating point format.\n");
  fprintf(stderr," Output is each value in all three formats.\n");
  exit(99);
}

int main(int argc,char** argv)
{
  if(argc<2 || (argv[1][0]=='0' && argv[1][1]=='h')) {
    Usage();
  }
  for(int i=1;i<argc;++i) {
    WORK_FLOAT value = ScanFloat(argv[i]);
    std::string hbf = HexBinaryFloatFormat(value);
    if(sizeof(WORK_FLOAT)<=8) {
      printf("%20s: %25.16e  %25.13a  %s\n",argv[i],value,value,hbf.c_str());
    } else {
      printf("%20s: %29.19e  %28.16a  %s\n",argv[i],value,value,hbf.c_str());
    }
  }
  return 0;
}
