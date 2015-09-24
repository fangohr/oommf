/* FILE: zeeman.h           -*-Mode: c++-*-
 *
 * This file contains class declarations for micromagnetic zeeman fields
 *
 * NOTE: The Zeeman classes work in xyz (as opposed to xzy) coordinates.
 *   The Grid class itself calls ConvertXyzToXzy() to convert applied
 *   fields from user (xyz) to grid (xzy) coordinates.
 */

#ifndef ZEEMAN_H
#define ZEEMAN_H

#include "nb.h"
#include "vf.h"
#include "threevec.h"

/* End includes */

class Grid2D;

//////////////////////// Zeeman Declaration //////////////////////////
class Zeeman {
protected:
  int Nx,Ny;  // Grid2D extents
  double Ms;
  int nomhfieldstep;
  double nomhfieldmag;
  double nomhfieldarr[VECDIM];
  ThreeVector nomhfield; // Nominal applied field
public:
  Zeeman(int argc,const char* argp[]);
  virtual ~Zeeman();
  static Zeeman* MakeZeeman(int argc,const char* argp[]);
  // static member MakeZeeman is the field type selection routine
  virtual int SetCoords(Grid2D* grid,int imax,int jmax);
  virtual int SetNomField(double newMs,const ThreeVector& newBfield,
			  int fieldstep);
  int SetNomFieldVec3D(double newMs,const Vec3D& newBfield,
		       int fieldstep);
  // Note: Some subclasses will ignore SetNomField()
  virtual double GetNomBFieldMag();
  virtual Vec3D  GetNomBField();
  virtual int GetLocalhField(int i,int j,ThreeVector& localfield) const =0;
  virtual double GetLocalhFieldMag(int i,int j) const;
  virtual int GetLocalBField(int i,int j,ThreeVector& localfield) const;
};

// Uniform external field
class UniformZeeman : public Zeeman {
private:
public:
  UniformZeeman(int argc,const char* argp[]);
  UniformZeeman(double Ms,double Bx,double By,double Bz);
  int GetLocalhField(int,int,ThreeVector& localfield) const
     { localfield=nomhfield; return 0; }
  double GetLocalhFieldMag(int,int) const { return nomhfieldmag; }  
};

// Field due to a "ribbon" of charge
// This type is insensitive to changes in the "nominal" field (SetNomField())
class RibbonZeeman : public Zeeman {
private:
  ThreeVector** h;      // h field vector array
  double relcharge;     // Charge magnitude, relative to Ms
  double ribx0,riby0,ribx1,riby1;  // Endpoints of charge ribbon
  double ribheight;                // Height of ribbon
  void FieldCalc(double dpar,double riblength,double dperp,
		 double &mx,double &my);
public:
  RibbonZeeman(int argc,const char* argp[]);
  // argp's are charge & ribbon endpoints:
  //    relcharge ribx0 riby0 ribx1 riby1 ribheight
  // The charge is relative to Ms.
  ~RibbonZeeman();
  int SetCoords(Grid2D* grid,int imax,int jmax);
  int GetLocalhField(int i,int j,ThreeVector& localfield) const;
};

// "Fixed" magnetization line
// This type is ignores calls to SetNomField(); hnomfield is setup
// in constructor, and is never changed.
class TieZeeman : public Zeeman {
private:
  ThreeVector** h;      // h field vector array
  double ribx0,riby0,ribx1,riby1; // Endpoints of fixed field
  double ribwidth;  // width of ribbon
public:
  TieZeeman(int argc,const char* argp[]);
  // argp's are relative field, ribbon endpoints & ribbon width:
  //        rf0 rf1 rf2 ribx0 riby0 ribx1 riby1 ribwidth
  // The field strength is relative to Ms.
  ~TieZeeman();
  int SetCoords(Grid2D* grid,int imax,int jmax);
  int GetLocalhField(int i,int j,ThreeVector& localfield) const;
  int SetNomField(double,const ThreeVector&,int) { return 0; }
};

// Input from file
// NOTE: The input routine assumes that mesh->GetValueUnit and Ms
//        are in the same units!!!
class FileZeeman : public Zeeman {
protected:
  Nb_DString filename;  // Name of file specifing B field
  OC_REAL8 fieldmult;      // Multiplier to apply to input from file.
  Nb_Array2D< Nb_Vec3<OC_REAL8> > h;  // Field array
  int FillArray();
public:
  FileZeeman(int argc,const char* argp[]);
  // argp's are a filename and optional field strength multiplier
  ~FileZeeman() {}
  int SetCoords(Grid2D* grid,int imax,int jmax);
  int GetLocalhField(int i,int j,ThreeVector& localfield) const;
};

class OneFileZeeman : public FileZeeman {
public:
  OneFileZeeman(int argc,const char* argp[]);
  ~OneFileZeeman() {}
};

class FileSeqZeeman : public FileZeeman {
private:
  Nb_DString scriptfile;
  Nb_DString procname;
public:
  FileSeqZeeman(int argc,const char* argp[]);
  int SetNomField(double newMs,const ThreeVector& newBfield,int fieldstep);
  ~FileSeqZeeman();
};


// Multiple type element
class MultiZeeman : public Zeeman {
private:
  int Fcount; // Number of fields
  Zeeman** z; // Zeeman field routine pointer array
public:
  MultiZeeman(int argc,const char* argp[]);
  // argp's are number of routines, and then routine names & parameters.
  // Each name is preceded by the number of parameters for that routine.
  ~MultiZeeman();
  int SetCoords(Grid2D* grid,int imax,int jmax);
  int SetNomField(double newMs,const ThreeVector& newBfield,int fieldstep);
  int GetLocalhField(int i,int j,ThreeVector& localfield) const;
};

#endif // ZEEMAN_H


