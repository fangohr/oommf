/* FILE: demag.h                    -*-Mode: c++-*-
 *
 * 	Double precision demag routine
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2010/07/16 22:33:03 $
 * Last modified by: $Author: donahue $
 */

#ifndef _DEMAG_H
#define _DEMAG_H

/* Standard libraries */
#include <cstdio>   
#include <cstring>

/* End includes */

/* Demag. Field function typedef's */
typedef void DF_Export_m(double** dat,void* m,int xdim,int ydim,int xyz);
typedef void DF_Import_h(double** dat,void* h,int xdim,int ydim,int xyz);
typedef void DF_Calc(DF_Export_m *writem,DF_Import_h *readh,
		     void* m,void *h);
typedef void DF_Init(const int xdim,const int ydim,
		     int paramc, char** params);
typedef void DF_Destroy(void);
/* BTW, DF => "Demag Field" */

/*
 * OK! Here is the low-down on the routines that must be provided by
 * the caller to do the demag calculations, i.e., DF_Export_m and
 * DF_Import_h, aka writem() and readh(), respectively.  writem()
 * writes an xdim*ydim array of magnetization components into the
 * arrays pointed to by dat.  dat is a pointer to the first element of
 * an array of xdim pointers.  Each of these xdim pointers points to
 * an array of ydim floating point numbers.  writem() writes the x, y,
 * and z-components for xyz = 0, 1, 2 respectively.  readh() reads
 * demag field vector components from the dat array and writes them
 * into the users data structure: x, y, z components for xyz = 0, 1, 2
 * respectively.  aspect is the ratio of (2-D cell size)/(film
 * thickness).  The cells are assumed to be square.
 */

/* Demag field function to name mappings; For runtime selection */
#define MAXDFPARAMS       8 /* Maximum number of params */
#define MAXDFNAMELENGTH 256 /* Maximum allowed size for name string */
                            /* + length of all param strings inc. \0's */
struct DF_FuncInfo
{
  const char* name;       /* Function name (null terminated string) */
  int         paramcount; /* Number of command line parameters needed */
  char**      params;	  /* Actual parameters (ASCII), set at runtime */
  DF_Calc*    calc;	  /* Pointer to field calc. actual function. */
  DF_Init*    init;	  /* Pointer to initializer */
  DF_Destroy* destroy;    /* Pointer to destructor */
};

int DF_LookUpName(char* name,struct DF_FuncInfo **fi);
/* Given an ASCII name, get corresponding function info; Returns 0 on
 * success, >0 on error.  This is the main external interface.
 */
void DF_List(FILE* output); /* Writes all valid DF names to *output */

/* Self demag coefficients.  All cell size imports must be >0 */
OC_REALWIDE SelfDemagNx(OC_REALWIDE xsize,OC_REALWIDE ysize,OC_REALWIDE zsize);
OC_REALWIDE SelfDemagNy(OC_REALWIDE xsize,OC_REALWIDE ysize,OC_REALWIDE zsize);
OC_REALWIDE SelfDemagNz(OC_REALWIDE xsize,OC_REALWIDE ysize,OC_REALWIDE zsize);

#endif /* _DEMAG_H */
