/**********************************************************************/
/* FILE: badstdargs.c
 *
 * This program demonstrates a bug in the SGI C compiler, cc,
 * MIPSpro Compilers: Version 7.3.1.1m, when built on R12000
 * hardware with
 *
 *        cc -Ofast badstdargs.c
 *
 * In this case sample output looks like
 *
 *        x = 1.06097e-314 (should be 43.21)
 *
 * There appear to be several linker switches that will defeat this
 * problem.  One is '-IPA:aggr_cprop=OFF'.
 *
 * To express the bug, the number and type of parameters to foo()
 * is important.  If n1 and n2 are changed to char*'s (so it has the
 * same signature as the sprintf() function), then the error disappears.
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

void foo(int n1,int n2, ...)
{
  double x;
  va_list arg_ptr;
  va_start(arg_ptr,n2);
  x = va_arg(arg_ptr,double);
  va_end(arg_ptr);
  printf("x = %g",x);
}

int main(int argc,char** argv)
{
  double x=43.21;
  foo(3,4,x);
  printf(" (should be %g)\n",x);
  return 0;
}

