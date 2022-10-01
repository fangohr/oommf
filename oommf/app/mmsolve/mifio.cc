/* FILE: mifio.cc           -*-Mode: c++-*-
 *
 * I/O routines involving *.mif (Micromagnetic Input File) files.
 *
 */

#include <cstdio>
#include <cstring>

#include "oc.h"
#include "nb.h"

#include "maganis.h"
#include "maginit.h"
#include "mifio.h"
#include "zeeman.h"

/* End includes */

ArgLine::ArgLine(const ArgLine& other)
{
  argc=0; argv=NULL;
  *this = other;
}

void ArgLine::Reset()
{
  if(argc>0 && argv!=NULL) {
    delete[] argv[0];  // Delete strings
    delete[] argv;     // Delete pointers to strings
  }
  argc=0; argv=NULL;
}

void ArgLine::Set(int new_argc,const char * const *new_argv)
{
  int i;
  size_t count;
  if(new_argc<1 || new_argv==NULL) { Reset(); return; }
  for(count=0,i=0;i<new_argc;i++) {
    if(new_argv[i]!=NULL) count+=strlen(new_argv[i])+1;
    else count+=1;
  }
  Reset();
  argc=new_argc;
  argv=new char*[argc];
  argv[0]=new char[count];
  strcpy(argv[0],new_argv[0]);
  for(i=1;i<argc;i++) {
    argv[i]=strchr(argv[i-1],'\0')+1;
    if(new_argv[i]!=NULL) strcpy(argv[i],new_argv[i]);
    else               strcpy(argv[i],"");
  }
}

void ArgLine::Set(int new_argc,const Nb_DString *new_strv)
{
  int i;
  size_t count;
  if(new_argc<1 || new_strv==NULL) { Reset(); return; }
  for(count=0,i=0;i<new_argc;i++) count+=new_strv[i].Length()+1;
  Reset();
  argc=new_argc;
  argv=new char*[argc];
  argv[0]=new char[count];
  strcpy(argv[0],new_strv[0].GetStr());
  for(i=1;i<argc;i++) {
    argv[i]=strchr(argv[i-1],'\0')+1;
    strcpy(argv[i],new_strv[i].GetStr());
  }
}

void ArgLine::Set(const char* argstr)
{
  char *buf=new char[strlen(argstr)+1];

  // Copy input string into buf, assumeing any backslashes are filename
  // path separators, and translate them into forward slashes.  This is
  // necessary because Tcl_SplitList (used to parse the arguments) does
  // backslash substitution.
  const char *aptr=argstr;
  char *bptr=buf;
  while(*aptr!='\0') {
    if(*aptr=='\\') *bptr='/';
    else            *bptr=*aptr;
    aptr++;
    bptr++;
  }
  *bptr='\0';

  // Use Tcl_SplitList to parse input string
  int new_argc;  CONST84 char ** new_argv;
  if(Tcl_SplitList(NULL,buf,&new_argc,&new_argv)!=TCL_OK)
    return;  // ERROR! Improper list structure.  No change made.

  // Fill *this
  Set(new_argc,(const char * const *)new_argv);
  /// The (const char * const *) is needed for some older (4.0?),
  /// confused versions of VC++.

  // Release buffer space
  Tcl_Free((char *)new_argv);
  delete[] buf;
}


ArgLine& ArgLine::operator=(const ArgLine &rhs)
{
  if(&rhs==this) return *this;  // Nothing to do!
  Set(rhs.argc,(const char **)rhs.argv);
  return *this;
}

void ArgLine::Get(Nb_DString &argstr)
{ // Copies args back into a string.
  char *cptr=Tcl_Merge(argc,argv);
  if(cptr!=NULL) {
    argstr=cptr;
    Tcl_Free(cptr);
  }
}

#define MifSlowPipeName   "SlowPipe"
#define MifFastPipeName   "FastPipe"
#define Mif2dSlabName     "2dSlab"
#define Mif3dSlabName     "3dSlab"
#define Mif3dChargeName   "3dCharge"
#define Mif3dConstMagName "ConstMag"
#define MifNoDemagName    "NoDemag"

//////////////////////////////////////////////////////////////////////

#define MIFBIGBUFSIZE 4096
#define MIFIDDELIMS ":="                 // Field ID delimiters
#define MIFDELIMS  " \t,;:\"\'`[]{}()="  // Line parameter delimiters
#define MIFCOMMENT '#'     // String denoting comment start

#define MIFMAXPARAMS 1024    // Maximum number of parameters per input line

void MIF::Init(void)
{ // Set MIF member variables to dummy values
  Ms = A = -1.0;
  K1 = EdgeK1 = 0.0;
  AnisType=mif_anis_notset;
  AnisDir1=AnisDir2=Vec3D(0.,0.,0.);
  DemagType=mif_demag_notset;
  PartWidth=PartHeight=PartThickness=CellSize= -1.0;
  GeomType=mif_geom_notset;   GeomTypeParam="";
  RandomizerSeed= -1;
  UserInteractionLevel= -1;
  UserReportCode= -1;
  InitialIncrement= -1.;
  LogLevel=-1;
  FieldRangeList.Free();
  AppliedFieldArgs.Reset();
  DefaultControlPointSpec.Free();
  MinTimeStep=MaxTimeStep=0.;
  UserComment.Free();
  InName.Free();
  OutBaseName.Free();
  OutMagFormat.Free();
  OutTotalFieldFormat.Free();
  OutDataTableFormat.Free();
}

MIF::MIF(void)
{
  SetDefaults();
}

MIF::~MIF(void)
{
}

void MIF::SetDefaults(void)
{ // Set some member variables to default values
  const char *cpptr[1];
  Init(); // Clear and set variables to dummy values
  AnisDir1=Vec3D(1.,0.,0.);
  AnisDir2=Vec3D(0.,1.,0.);
  EdgeK1=0.0;
  DoPrecess=1;  // Enable precession by default.
  GyRatio=2.21e5; // Units are m/A.s.
  DampCoef=0.5;  //   Realistic values for DampCoef are in the range
  /// [.004,.15], but for fast convergence of the ODE, DampCoef=0.5
  /// is usually pretty good.
  cpptr[0]="constant";
  AnisInitArgs.Set(1,cpptr);
  DemagType=mif_nodemag;
  GeomType=mif_rectangle;
  cpptr[0]="";
  MagInitArgs.Set(1,cpptr);
  RandomizerSeed=0;
  UserInteractionLevel=0;
  UserReportCode=0;
  LogLevel=0;
  InitialIncrement= 1.0*PI/180.;  // 1 degree.
  cpptr[0]="uniform";
  AppliedFieldArgs.Set(1,cpptr);
  DefaultControlPointSpec="-torque 1e-5";
  OutMagFormat="binary 4";
  OutTotalFieldFormat="binary 4";
  OutDataTableFormat="%.16g";
}

// MIF::ReadLine reads and tokenizes 1 logical line from infile.
// A non-zero return value indicates error; 1 => end-of-file
#define MRLsuccess  0  // Successful read
#define MRLeof      1  // Proper end-of-file
#define MRLbadeof   2  // Improper eof
#define MRLferror   3  // Other file error
#define MRLbufof    4  // Input buffer overflow
#define MRLparamof  5  // Parameter space overflow
#define MRLbadlist  6  // Parameters don't form proper Tcl list

int MIF::ReadLine(FILE* infile,int max_strc,int &strc,Nb_DString* strv)
{
  static char buf[MIFBIGBUFSIZE];
  static Nb_DString line(MIFBIGBUFSIZE);

  strc=0;

  // Read one logical line
  buf[sizeof(buf)-1]=buf[sizeof(buf)-2]='\0';  // Safety; Shouldn't be needed.
  line="";
  int line_done,eof_flag=0;
  size_t read_length;
  do {
    line_done=0;
    buf[0]='\0'; // Safety
    if(fgets(buf,sizeof(buf)-1,infile)==NULL) {
      eof_flag=1; break;   // Assume error is EOF  
    }
    read_length=strlen(buf);
    if(read_length==0) line_done=1;
    else {
      if(buf[read_length-1]=='\n') {
	read_length--;
        if(buf[read_length-1]=='\r') read_length--; // DOS-style line ending
	if(buf[read_length-1]=='\\') read_length--;
	else                         line_done=1;
      }
      line.Append(buf,read_length);
    }
  } while(!line_done);
  line.TrimLeft();
  if(eof_flag && line.Length()==0) return MRLeof;

  if(line[0]=='\0' || line[0]==MIFCOMMENT)
    return MRLsuccess; // Comment or blank line

  // Pull out and trim field identifier
  if(max_strc<1) return MRLparamof;
  size_t line_len=line.Length();
  char *workbuf=new char[line_len+2]; // Be sure to delete[] before rtn!
  workbuf[line_len+1]='\0'; // Safety
  strcpy(workbuf,line.GetStr());
  size_t offset=strcspn(workbuf,MIFIDDELIMS);
  if(offset+1>=line_len) {
    // Field ID terminator not found; ASSUME this is a comment line
    delete[] workbuf; return MRLsuccess;
  }
  workbuf[offset]='\0';
  strv[0].Dup(workbuf);
  strv[0].Trim();
  strc=1;
  offset=offset+1+strspn(workbuf+offset+1,MIFIDDELIMS);
  if(offset>=line_len) {
    // Nothing after end of field ID terminator! Bad line...
    delete[] workbuf; return MRLbadlist;
  }

  // Assume any backslashes in remainder are filename path separators,
  // and translate them into forward slashes.  This is necessary because
  // Tcl_SplitList (used to parse the arguments) does backslash
  // substitution.
  for(char *cp=workbuf+offset;*cp!='\0';cp++) {
    if(*cp=='\\') *cp='/';
  }

  // Strip comments and break remainder into list elements
  // Comments are detected by truncating the line at each
  // comment character, starting from the left, and testing
  // to see if we have a proper Tcl list.  If a comment char
  // is buried inside braces or quotes, then the string to
  // the left of the comment character will not be a proper
  // list, in which case we reinstate the character and
  // retry at the next comment character (if any).
  char *cmtptr;
  int paramc=0;  CONST84 char ** paramv=NULL;
  cmtptr=workbuf+offset;
  while(1) {
    if((cmtptr=strchr(cmtptr,MIFCOMMENT))!=NULL) 
      *cmtptr='\0';  // Truncate comment
    // In the following, replace NULL with Oc_GlobalInterpreter() if you
    // want to get back an error message.
    if(Tcl_SplitList(NULL,workbuf+offset,&paramc,&paramv)==TCL_OK) {
      // Proper list structure => success
      break;
    } else {
      // Improper list structure
      if(cmtptr!=NULL) { // Bad comment truncation
	*(cmtptr++)=MIFCOMMENT;
      } else {           // Bad entry
	delete[] workbuf;
	return MRLbadlist;
      }
    }
  }

  // Copy parameter strings into strv
  int error_code=MRLsuccess;
  for(int i=0;i<paramc;i++) {
    if(strc>=max_strc) { error_code=MRLparamof; break; } // Overflow
    strv[strc++].Dup(paramv[i]);
  }

  delete[] workbuf;
  Tcl_Free((char *)paramv);
  return error_code;
}

// Fixup AnisDir1 and AnisDir2 as necessary
#define MIFNAME "MIF::AdjustAnisDirs: "
void MIF::AdjustAnisDirs(void)
{
  double temp;
  // Make AnisDir1 a unit vector
  temp=AnisDir1.MagSq();
  if(temp<OC_SQRT_REAL8_EPSILON)
    PlainError(1,"Error in " MIFNAME "AnisDir1 has zero length");
  if(temp!=1.0) AnisDir1 *= 1./sqrt(temp);

  // If AnisType is cubic, then AnisDir2 should be unit vector perpendicular
  // to AnisDir1
  if(AnisType==mif_cubic) {
    temp=AnisDir1*AnisDir2;
    if(temp!=0.0) { // Eliminate AnisDir1 component from AnisDir2
      AnisDir2.Accum(-temp,AnisDir1);
    }
    // Make AnisDir2 a unit vector
    temp=AnisDir2.MagSq();
    if(temp<OC_SQRT_REAL8_EPSILON) PlainError(1,"Error in " MIFNAME
          "Component of AnisDir2 perpendicular to AnisDir1 has zero length");
    if(temp!=1.0) AnisDir2 *= 1./sqrt(temp);
  }
}
#undef MIFNAME

#define MIFNAME "MIF::ConsistencyCheck: "
#define MIFERRF(x) \
   PlainWarning("Warning in " MIFNAME "Illegal value for %s: %f",#x,x),\
   errcount++
#define MIFERRD(x) \
   PlainWarning("Warning in " MIFNAME "Illegal value for %s: %d",#x,x),\
   errcount++
#define MIFERRE(v,s) \
   PlainWarning("Warning in " MIFNAME "Illegal value for %s: %s",#v,#s),\
   errcount++

// Routine to perform some consistency checks on member variables
// Return value is the number of errors detected
int MIF::ConsistencyCheck(void)
{
  int errcount; double temp;
  errcount=0;
  // Material parameters
  if(Ms<0.0) MIFERRF(Ms);
  if(A<0.0)  MIFERRF(A);
  if(AnisType==mif_anis_notset) MIFERRE(AnisType,mif_anis_notset);
  temp=AnisDir1.MagSq();
  if(fabs(temp-1.0)>OC_SQRT_REAL8_EPSILON)
    PlainWarning("Warning in " MIFNAME "AnisDir1 has length %f != 1",
		 sqrt(temp)),errcount++;
  if(AnisType==mif_cubic) { // Check for illegal AnisDir2
    temp=AnisDir2.MagSq();
    if(fabs(temp-1.0)>OC_SQRT_REAL8_EPSILON)
      PlainWarning("Warning in " MIFNAME "AnisDir2 has length %f != 1",
		   sqrt(temp)),errcount++;
    temp=AnisDir1*AnisDir2;
    if(fabs(temp)>OC_SQRT_REAL8_EPSILON)
      PlainWarning("Warning in " MIFNAME "AnisDir2 not perpendicular to"
		   " AnisDir1; Inner product =%f",temp),errcount++;
  }
  if(AnisInitList.GetAnisInitFunc(AnisInitArgs.argv[0])==NULL)
    PlainWarning("Warning in " MIFNAME "Unknown AnisInitFunc: ->%s<-",
		 AnisInitArgs.argv[0]),errcount++;

  // Demag specification
  if(DemagType==mif_demag_notset) MIFERRE(DemagType,mif_demag_notset);

  // Part geometry
  if(PartWidth<=0.0)     MIFERRF(PartWidth);
  if(PartHeight<=0.0)    MIFERRF(PartHeight);
  if((DemagType==mif_2dslab || DemagType==mif_3dslab)
     && PartThickness<=0.0) MIFERRF(PartThickness);
  if(CellSize<=0.0)      MIFERRF(CellSize);
  if(CellSize>PartWidth) {
    PlainWarning("Warning in " MIFNAME "CellSize=%g > PartWidth=%g",
		 CellSize,PartWidth);
    errcount++;
  }
  if(CellSize>PartHeight) {
    PlainWarning("Warning in " MIFNAME "CellSize=%g > PartHeight=%g",
		 CellSize,PartHeight);
    errcount++;
  }
  if(fabs(1-floor((PartWidth+CellSize/2.)/CellSize)*CellSize/PartWidth)
     >=0.0001) {
    PlainWarning("Warning in " MIFNAME 
		 "CellSize=%g does not divide PartWidth=%g",
		 CellSize,PartWidth);
    errcount++;
  }
  if(fabs(1-floor((PartHeight+CellSize/2.)/CellSize)*CellSize/PartHeight)
     >=0.0001) {
    PlainWarning("Warning in " MIFNAME 
		 "CellSize=%g does not divide PartHeight=%g",
		 CellSize,PartHeight);
    errcount++;
  }
  if(GeomType==mif_geom_notset)   MIFERRE(GeomType,mif_geom_notset);


  // Magnetization initialization
  if(MagInit::NameCheck(MagInitArgs.argv[0])==NULL)
    PlainWarning("Warning in " MIFNAME
		 "Unknown magnetization initialization routine %s",
		 MagInitArgs.argv[0]),errcount++;
  else {
    if(MagInit::GetParamCount(MagInitArgs.argv[0])!=MagInitArgs.argc-1)
      PlainWarning("Warning in " MIFNAME
		  "Wrong number of parameters to mag init routine %s,"
		  " %d instead of %d",
		   MagInitArgs.argv[0],MagInitArgs.argc-1,
		   MagInit::GetParamCount(MagInitArgs.argv[0])),errcount++;
  }

  // Experiment parameters
  //  NOTE: Currently no check on FieldRange list.
  if(AppliedFieldArgs.argc<1) {
      PlainWarning("Warning in " MIFNAME "External field type not set");
      errcount++;
  }

  // Output specification
  if(OutBaseName.Length()<1) {
    PlainWarning("Warning in " MIFNAME "Output filename not set");
    errcount++;
  }
  if(OutMagFormat.Length()<1) {
    PlainWarning("Warning in " MIFNAME 
		 "Output magnetization format not set");
    errcount++;
  }
  if(OutTotalFieldFormat.Length()<1) {
    PlainWarning("Warning in " MIFNAME 
		 "Output total field format not set");
    errcount++;
  }
  if(OutDataTableFormat.Length()<1) {
    PlainWarning("Warning in " MIFNAME 
		 "Output data table format not set");
    errcount++;
  }
  if(LogLevel<0) {
    PlainWarning("Warning in " MIFNAME "Invalid Log level %d<0",LogLevel);
    errcount++;
  }



  // Miscellaneous
  if(InitialIncrement<=0.0)
    PlainWarning("Warning in " MIFNAME "Initial increment non-positive (%f)",
		InitialIncrement),errcount++;
  if(UserInteractionLevel<0 || UserInteractionLevel>100)
    MIFERRD(UserInteractionLevel);

  return errcount;
}
#undef MIFNAME
#undef MIFERRF
#undef MIFERRD
#undef MIFERRE

class MIFIOResources {
  // This is a little dummy class to support automatic
  // cleanup of resources on scope exit.  The main prod
  // to create this class is that the Nb_String param
  // array may be too big to fit in the stack area, so
  // rather than an auto 'Nb_DString param[MIFMAXPARAMS]'
  // declaration, we want to use the heap via new[].  But
  // the MIF::Read class has a lot of embedded returns,
  // so insuring a 'delete[] param' call is a PITA.  This
  // could be handled by using auto_ptr's, but this portion
  // of the code is meant to work without needing the STL.
  // (Or exceptions.)
private:
  int paramcount;
  Nb_DString* params;
  Nb_DString FileName; // Name of input file
  FILE* infile;
public:
  MIFIOResources(int paramcount_request,const char* inname);
  /// If infile != NULL then initialization succeeded.
  /// If infile == NULL, then an error message will be
  ///  written to MessageLocker.
  ~MIFIOResources();
  FILE* FilePtr() const { return infile; }
  int ParamCount() const { return paramcount; }
  Nb_DString* Params() const { return params; }
  Nb_DString Name() const { return FileName; }
};

MIFIOResources::MIFIOResources(int paramcount_request,const char* inname)
  : paramcount(paramcount_request),params(NULL),
    FileName(inname),infile(NULL)
{
  params = new Nb_DString[paramcount];
  if(params==NULL) {
    paramcount = 0;
    MessageLocker::Append("Error in MIFIOResources;"
			  " Insufficient memory for %d Nb_DString's\n",
			  paramcount);
    return;
  }
  if((infile=Nb_FOpen(FileName.GetStr(),"r"))==0) {
    // Add a .mif extension and retry
    FileName.Append(".mif");
    if((infile=Nb_FOpen(FileName.GetStr(),"r"))==0) {
      MessageLocker::Append("Error in MIFIOResources;"
			    " Unable to open file %s\n",
			    inname);
      return;
    }
  }
}

MIFIOResources::~MIFIOResources()
{
  if(params != NULL) delete[] params;
  if(infile != NULL) fclose(infile);
  FileName.Free();
}

#define MIFNAME "MIF::Read: "
#define MIFCOUNT "Incorrect number of parameters (%d) for entry \"%s\""
void MIF::Read(const char* inname,int &errorcode)
{ // Returns "".  Argument errorcode == 0 if no error,
  //                                  > 0 on fatal error.
  //                                  < 0 on non-fatal error(s).
  //  Use MessageLocker::GetMessage(Nb_DString &) to retrieve
  //  error and warning messages (if errorcode!=0).
  int paramcount,readcode;

  errorcode=1;     // Cleared after successful completion
  int warncount=0; // Number of non-fatal warnings

  SetDefaults();

  // Kludge to handle MIF 1.2 cellsize record
  OC_BOOL check_zcellsize = 0;
  double zcellsize = 0;

  MIFIOResources ioresource(MIFMAXPARAMS,inname);
  if(ioresource.FilePtr() == NULL) return; // Resource initialization
  /// failure.  Probably a bad filename.
  Nb_DString* const param = ioresource.Params(); // For convenience

  // Check header line for MIF file version
  {
    char verbuf[MIFBIGBUFSIZE];
    if(fgets(verbuf,sizeof(verbuf)-1,ioresource.FilePtr())==NULL) {
      MessageLocker::Append("Error in " MIFNAME "Empty file?\n");
      return;
    }
    
    char* nexttoken=verbuf;
    char *token1 = Nb_StrSep(&nexttoken," \t");
    char *token2 = Nb_StrSep(&nexttoken," \t");
    char *token3 = Nb_StrSep(&nexttoken," \t");
    if(strcmp(token1,"#")!=0 || strcmp(token2,"MIF")!=0
       || token3 == NULL) {
      MessageLocker::Append("Error in " MIFNAME
                            "Input is not in MIF format\n");
      return;
    }
    nexttoken=token3;
    char *major_version = Nb_StrSep(&nexttoken,".");
    if(strcmp(major_version,"1")!=0) {
      MessageLocker::Append("Error in " MIFNAME
                            "Input is not in MIF 1.x format\n");
      return;
    }
  }

  do {
    // Read input line
    if((readcode=ReadLine(ioresource.FilePtr(),ioresource.ParamCount(),
			  paramcount,param))!=MRLsuccess) break;

    if(paramcount<1) continue; // Empty line

    param[0].StripWhite(); param[0].ToLower();

    // MATERIAL PARAMETERS
    if(strcmp(param[0],"ms")==0) {
      if(paramcount!=2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      Ms=Nb_Atof(param[1]);
    }
    else if(strcmp(param[0],"a")==0) {
      if(paramcount!=2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      A=Nb_Atof(param[1]);
    }
    else if(strcmp(param[0],"k1")==0) {
      if(paramcount!=2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      K1=Nb_Atof(param[1]);
    }
    else if(strcmp(param[0],"edgek1")==0) {
      if(paramcount!=2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      EdgeK1=Nb_Atof(param[1]);
    }
    else if(strcmp(param[0],"anisotropytype")==0) {
      if(paramcount!=2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      param[1].Trim(); param[1].CollapseStr(); param[1].ToLower();
      if(strcmp(param[1],"uniaxial")==0)   AnisType=mif_uniaxial;
      else if(strcmp(param[1],"cubic")==0) AnisType=mif_cubic;
      else {
	MessageLocker::Append("Warning in " MIFNAME
			      "Unknown AnisType: %s\n",param[1].GetStr());
	warncount++;
      }
    }
    else if(strcmp(param[0],"anisotropydir1")==0) {
      if(paramcount!=1+MIFVECDIM) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      AnisDir1=Vec3D(Nb_Atof(param[1]),
		     Nb_Atof(param[2]),Nb_Atof(param[3]));
    }
    else if(strcmp(param[0],"anisotropydir2")==0) {
      if(paramcount!=1+MIFVECDIM) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      AnisDir2=Vec3D(Nb_Atof(param[1]),
		     Nb_Atof(param[2]),Nb_Atof(param[3]));
    }
    // Anisotropy initialization function
    else if(strcmp(param[0],"anisotropyinitialization")==0 ||
	    strcmp(param[0],"anisotropyinit")==0) {
      if(paramcount<2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      AnisInitArgs.Set(paramcount-1,param+1);
    }
    else if(strcmp(param[0],"doprecess")==0) {
      if(paramcount!=2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      DoPrecess=atoi(param[1]);
    }
    else if(strcmp(param[0],"gyratio")==0) {
      if(paramcount!=2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      GyRatio=Nb_Atof(param[1]);
    }
    else if(strcmp(param[0],"dampcoef")==0) {
      if(paramcount!=2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      DampCoef=Nb_Atof(param[1]);
    }

    // DEMAG SPECIFICATION
    else if(strcmp(param[0],"demagtype")==0) {
      if(paramcount!=2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      int demag_type_error;
      SetDemagType(param[1],demag_type_error);
      if(demag_type_error) {
	MessageLocker::Append("Warning in " MIFNAME "Unknown DemagType: %s\n",
			      param[1].GetStr());
	warncount++;
      }
    }
    // SOLVER TYPE (ignored)
    else if(strcmp(param[0],"solvertype")==0) {
    }
    // PART GEOMETRY
    else if(strcmp(param[0],"partwidth")==0) {
      if(paramcount!=2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      PartWidth=Nb_Atof(param[1]);
    }
    else if(strcmp(param[0],"partheight")==0) {
      if(paramcount!=2)
	PlainError(1,"Error in " MIFNAME MIFCOUNT,paramcount-1,
		   param[0].GetStr());
      PartHeight=Nb_Atof(param[1]);
    }
    else if(strcmp(param[0],"partthickness")==0) {
      if(paramcount!=2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      PartThickness=Nb_Atof(param[1]);
    }
    else if(strcmp(param[0],"cellsize")==0) {
      if(paramcount==2) {
	CellSize=Nb_Atof(param[1]);
	check_zcellsize=0;
      } else if(paramcount==4) {
	CellSize=Nb_Atof(param[1]);
	double ycellsize = Nb_Atof(param[2]);
	double diff = fabs(ycellsize-CellSize);
	double sum = fabs(ycellsize) + fabs(CellSize);
	if(diff>OC_SQRT_REAL8_EPSILON*sum) {
	  MessageLocker::Append("Error in " MIFNAME "cell x-size (%g)"
				" differs from y-size (%g)\n",
				CellSize,ycellsize);
	  return;
	}
	zcellsize = Nb_Atof(param[3]);
	check_zcellsize=1;
      } else {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
    }
    else if(strcmp(param[0],"partshape")==0) {
      if(paramcount<2 || paramcount>3) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      param[1].Trim(); param[1].CollapseStr(); param[1].ToLower();
      if(strcmp(param[1],"rectangle")==0)      GeomType=mif_rectangle;
      else if(strcmp(param[1],"ellipse")==0)   GeomType=mif_ellipse;
      else if(strcmp(param[1],"ellipsoid")==0) GeomType=mif_ellipsoid;
      else if(strcmp(param[1],"oval")==0)      GeomType=mif_oval;
      else if(strcmp(param[1],"pyramid")==0)   GeomType=mif_pyramid;
      else if(strcmp(param[1],"mask")==0)      GeomType=mif_mask;
      else {
	MessageLocker::Append("Warning in " MIFNAME "Unknown GeomType: %s\n",
			      param[1].GetStr());
	warncount++;
      }
      if(paramcount==3) GeomTypeParam=param[2];
      else              GeomTypeParam="";
    }
    // INITIAL MAGNETIZATION
    else if(strcmp(param[0],"initmag")==0) {
      if(paramcount<2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      MagInitArgs.Set(paramcount-1,param+1);
    }
    // EXPERIMENT PARAMETERS
    else if(strcmp(param[0],"fieldrangecount")==0) {
      // Maintained for backwards compatibility, but not currently used.
    }
    else if(strcmp(param[0],"fieldrange")==0) {
      if(paramcount<2*MIFVECDIM+2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      FieldRangeList.Append(" {",3);    // Append new list element
      FieldRangeList.AppendArgs(paramcount-1,param+1);
      FieldRangeList.Append(" }",3);
    }
    else if(strcmp(param[0],"fieldtype")==0) {
      if(paramcount<2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      AppliedFieldArgs.Set(paramcount-1,param+1);
    }

    // OUTPUT SPECIFICATION
    else if(strcmp(param[0],"baseoutputfilename")==0) {
      if(paramcount!=2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      OutBaseName=param[1];
    }
    else if(strcmp(param[0],"magnetizationoutputformat")==0) {
      if(paramcount!=3) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      param[1].ToLower();
      OutMagFormat.MergeArgs(paramcount-1,param+1);
    }
    else if(strcmp(param[0],"totalfieldoutputformat")==0) {
      if(paramcount!=3) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      param[1].ToLower();
      OutTotalFieldFormat.MergeArgs(paramcount-1,param+1);
    }
    else if(strcmp(param[0],"datatableoutputformat")==0) {
      if(paramcount!=2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      OutDataTableFormat=param[1];
    }
    else if(strcmp(param[0],"loglevel")==0) {
      if(paramcount!=2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      LogLevel=atoi(param[1]);
    }
    // MISCELLANEOUS
    else if(strcmp(param[0],"randomizerseed")==0) {
      if(paramcount!=2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      RandomizerSeed=atoi(param[1]);
    }
    else if(strcmp(param[0],"initialincrement")==0) {
      if(paramcount!=2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      InitialIncrement=Nb_Atof(param[1])*PI/180.;
    }
    else if(strcmp(param[0],"userinteractionlevel")==0) {
      if(paramcount!=2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      UserInteractionLevel=atoi(param[1]);
    }
    else if(strcmp(param[0],"userreportcode")==0) {
      if(paramcount!=2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      UserReportCode=atoi(param[1]);
    }
    else if(strcmp(param[0],"defaultcontrolpointspec")==0) {
      // Replaces convergemxhvalue
      if(paramcount<2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      DefaultControlPointSpec.Dup("");
      DefaultControlPointSpec.AppendArgs(paramcount-1,param+1);
    }
    else if(strcmp(param[0],"convergemxhvalue")==0 ||
	    strcmp(param[0],"converge|mxh|value")==0) {
      // NOTE: Use of convergemxhvalue is deprecated!
      //       Use defaultcontrolpointspec instead.
      if(paramcount!=2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      DefaultControlPointSpec="-torque ";
      DefaultControlPointSpec.Append(param[1]);
    }
    else if(strcmp(param[0],"usercomment")==0) {
      if(paramcount<1) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      UserComment.Free();
      UserComment.AppendArgs(paramcount-1,param+1);
    }
    else if(strcmp(param[0],"mintimestep")==0) {
      if(paramcount!=2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      MinTimeStep=Nb_Atof(param[1]);
    }
    else if(strcmp(param[0],"maxtimestep")==0) {
      if(paramcount!=2) {
	MessageLocker::Append("Error in " MIFNAME MIFCOUNT "\n",
			      paramcount-1,param[0].GetStr());
	return;
      }
      MaxTimeStep=Nb_Atof(param[1]);
    }
    // UNKNOWN
    else {
      MessageLocker::Append("Warning in " MIFNAME "Unknown line tag: %s\n",
			    param[0].GetStr());
      warncount++;
    }
  } while(readcode==0); // End of input read loop

  // Check for input errors
  if(readcode!=MRLeof) {
    if(readcode==MRLbadeof)
      MessageLocker::Append("Error in " MIFNAME "Improper eof\n");
    else if(readcode==MRLferror)
      MessageLocker::Append("Error in " MIFNAME "File error\n");
    else if(readcode==MRLbufof)
      MessageLocker::Append("Error in " MIFNAME "Input buffer overflow\n");
    else if(readcode==MRLparamof)
      MessageLocker::Append("Error in " MIFNAME "Parameter space overflow\n");
    else if(readcode==MRLbadlist)
      MessageLocker::Append("Error in " MIFNAME 
					"Invalid Tcl list in input file\n");
    else 
      MessageLocker::Append("Error in " MIFNAME "Unknown file input error\n");
    return;
  }

  // Kludge to handle MIF 1.2 cellsize record
  if(check_zcellsize) {
    double diff = fabs(zcellsize-PartThickness);
    double sum = fabs(zcellsize) + fabs(PartThickness);
    if(diff>OC_SQRT_REAL8_EPSILON*sum) {
      MessageLocker::Append("Error in " MIFNAME "cell z-size (%g)"
			    " differs from part thickness (%g)\n",
			    zcellsize,PartThickness);
      return;
    }
  }

  // Adjust AnisDir1 and AnisDir2
  AdjustAnisDirs();

  // Check for valid specification
  int errcount;
  if((errcount=ConsistencyCheck())!=0) {
    MessageLocker::Append("Error in " MIFNAME 
			  "%d error%s found in input file %s\n",
			  errcount,(errcount==1?"":"s"),inname);
    return;
  }

  // If we get here, then there are no fatal errors.
  errorcode = -warncount;  // Possibly some warnings, though.

  return;
}
#undef MIFNAME
#undef MIFCOUNT

const char* MIF::GetDemagName(void)
{
  const char* cptr;
  switch(DemagType)
    {
    case mif_slowpipe:    cptr=MifSlowPipeName;   break;
    case mif_fastpipe:    cptr=MifFastPipeName;   break;
    case mif_2dslab:      cptr=Mif2dSlabName;     break;
    case mif_3dslab:      cptr=Mif3dSlabName;     break;
    case mif_3dcharge:    cptr=Mif3dChargeName;   break;
    case mif_3dconstmag:  cptr=Mif3dConstMagName; break;
    case mif_nodemag:     cptr=MifNoDemagName;    break;
    default:              cptr=MifFastPipeName;   break;
    }
  return cptr;
}

void MIF::SetDemagType(const char* demagname,int &errorcode)
{
  Nb_DString name(demagname);
  name.Trim(); name.CollapseStr(); name.ToLower();
  errorcode=0;
  if(strcmp(name,"slowpipe")==0)      DemagType=mif_slowpipe;
  else if(strcmp(name,"fastpipe")==0) DemagType=mif_fastpipe;
  else if(strcmp(name,"2dslab")==0)   DemagType=mif_2dslab;
  else if(strcmp(name,"3dslab")==0)   DemagType=mif_3dslab;
  else if(strcmp(name,"3dcharge")==0) DemagType=mif_3dcharge;
  else if(strcmp(name,"constmag")==0) DemagType=mif_3dconstmag;
  else if(strcmp(name,"none")==0)     DemagType=mif_nodemag;
  else errorcode=1;
}


Vec3D MIF::GetAnisDir(int i)
{
  if(i==1) return AnisDir1;
  if(i==2) return AnisDir2;
  return Vec3D(0.,0.,0.);
}

void MIF::DumpPartialInfo(FILE* fptr)
{
  int i;
  fprintf(fptr,"# MIF input file: %s\n",InName.GetStr());
  fprintf(fptr,"#  Material information---\n");
  fprintf(fptr,"#   Ms=%.5e  A=%.5e  K1=%.5e\n",Ms,A,K1);
  fprintf(fptr,"#   AnisType= %s\n",
	  (AnisType==mif_uniaxial ? "Uniaxial" :
	   (AnisType==mif_cubic ? "Cubic" : "Not set")));
  fprintf(fptr,"#   AnisDir1=(%6.3f,%6.3f,%6.3f)\n",
	  AnisDir1.GetX(),AnisDir1.GetY(),AnisDir1.GetZ());
  if(AnisType==mif_cubic)
    fprintf(fptr,"#   AnisDir2=(%6.3f,%6.3f,%6.3f)\n",
	    AnisDir2.GetX(),AnisDir2.GetY(),AnisDir2.GetZ());
  fprintf(fptr,"#   AnisInit=");
  for(i=0;i<AnisInitArgs.argc;i++)
    fprintf(fptr," %s",AnisInitArgs.argv[i]);
  fprintf(fptr,"\n");
  fprintf(fptr,"   Edge K1=%.5e\n",EdgeK1);
  fprintf(fptr,"#  Demag type: ");
  switch(DemagType)
    {
    case mif_slowpipe:     fprintf(fptr,"Slow Pipe\n"); break;
    case mif_fastpipe:     fprintf(fptr,"Fast Pipe\n"); break;
    case mif_2dslab:       fprintf(fptr,"2D Slab\n"); break;
    case mif_3dslab:       fprintf(fptr,"3D Slab\n"); break;
    case mif_3dcharge:     fprintf(fptr,"3D Charge\n"); break;
    case mif_3dconstmag:   fprintf(fptr,"3D Const Mag\n"); break;
    case mif_nodemag:      fprintf(fptr,"No demag\n"); break;
    case mif_demag_notset: fprintf(fptr,"Not set\n"); break;
    default:           fprintf(fptr,"ERROR: UNKNOWN DEMAG TYPE\n"); break;
    }
  fprintf(fptr,"#  Geometry information---\n");
  fprintf(fptr,"#   Part width:     %.4e m\n",PartWidth);
  fprintf(fptr,"#   Part height:    %.4e m\n",PartHeight);
  fprintf(fptr,"#   Part thickness: %.4e m\n",PartThickness);
  fprintf(fptr,"#   Cell size:      %.4e m\n",CellSize);
  fprintf(fptr,"#   Part shape:     %s %s\n",
	  (GeomType==mif_rectangle ? "rectangle" :
	   (GeomType==mif_ellipse ? "ellipse" :
	    (GeomType==mif_ellipsoid ? "ellipsoid" :
	     (GeomType==mif_mask ? "mask" : 
	      (GeomType==mif_oval ? "oval" :
	       (GeomType==mif_pyramid ? "pyramid" : "not set")))))),
	   GeomTypeParam.GetStr());

  fprintf(fptr,"#   Part shape:     %s",
	  (GeomType==mif_rectangle ? "rectangle" :
	   (GeomType==mif_ellipse ? "ellipse" :
	    (GeomType==mif_ellipsoid ? "ellipsoid" :
	     (GeomType==mif_mask ? "mask" : 
	      (GeomType==mif_oval ? "oval" :
	       (GeomType==mif_pyramid ? "pyramid" : "not set")))))));
  if(GeomTypeParam.Length()>0) {
    fprintf(fptr," %s\n",GeomTypeParam.GetStr());
  } else {
    fputs("\n",fptr);
  }

  fprintf(fptr,"#  Initial Magnetization:");
  for(i=0;i<MagInitArgs.argc;i++)
    fprintf(fptr," %s",MagInitArgs.argv[i]);
  fprintf(fptr,"\n");
  fprintf(fptr,"#  Randomizer seed: %d\n",RandomizerSeed);
  fprintf(fptr,"#  Output base name: %s\n",OutBaseName.GetStr());
  fflush(fptr);
}

Nb_DString MIF::Write(int &errorcode)
{ // Returns "".  Argument errorcode == 0 if no error,
  //                                  > 0 on fatal error.
  //                                  < 0 on non-fatal error(s).

  errorcode=0;
  Oc_AutoBuf ab;
  Nb_DString sbuf;
  char buf[MIFBIGBUFSIZE];

  Nb_DString outstr;
  outstr="# MIF 1.1\n";

  // Material parameters
  sprintf(buf,"Ms: %.17g\nA: %.17g\nK1: %.17g\nEdge K1: %.17g\n",
	  Ms,A,K1,EdgeK1);
  outstr.Append(buf);
  buf[0]='\0';
  strcpy(buf,"Anisotropy Type: ");
  switch(AnisType)
    {
    case mif_uniaxial: strcat(buf,"uniaxial\n"); break;
    case mif_cubic:    strcat(buf,"cubic\n");    break;
    default: errorcode++; buf[0]='\0'; break;
    }
  outstr.Append(buf);
  sprintf(buf,"Anisotropy Dir1: %.17g %.17g %.17g\n",
	  AnisDir1.GetX(),AnisDir1.GetY(),AnisDir1.GetZ());
  outstr.Append(buf);
  sprintf(buf,"Anisotropy Dir2: %.17g %.17g %.17g\n",
	  AnisDir2.GetX(),AnisDir2.GetY(),AnisDir2.GetZ());
  outstr.Append(buf);
  AnisInitArgs.Get(sbuf);
  outstr.Append("Anisotropy Initialization: ");
  outstr.Append(sbuf);
  outstr.Append("\n");
  sprintf(buf,"Do Precess: %d\nGyratio: %.17g\nDamp Coef: %.17g\n",
	  DoPrecess,GyRatio,DampCoef);
  outstr.Append(buf);
  
  // Demag Specification
  strcpy(buf,"Demag Type: ");
  switch(DemagType)
    {
    case mif_slowpipe:   strcat(buf,"SlowPipe\n"); break;
    case mif_fastpipe:   strcat(buf,"FastPipe\n"); break;
    case mif_2dslab:     strcat(buf,"2dSlab\n"); break;
    case mif_3dslab:     strcat(buf,"3dSlab\n"); break;
    case mif_3dcharge:   strcat(buf,"3dCharge\n"); break;
    case mif_3dconstmag: strcat(buf,"ConstMag\n"); break;
    case mif_nodemag:    strcat(buf,"None\n"); break;
    default: errorcode++; buf[0]='\0'; break;
    }
  outstr.Append(buf);

  // Part Geometry
  sprintf(buf,"Part Width: %.17g\nPart Height: %.17g\n"
	  "Part Thickness: %.17g\nCell Size: %.17g\n",
	  PartWidth,PartHeight,PartThickness,CellSize);
  outstr.Append(buf);
  strcpy(buf,"Part Shape: ");
  switch(GeomType)
    {
    case mif_rectangle: strcat(buf,"Rectangle"); break;
    case mif_ellipse:   strcat(buf,"Ellipse"); break;
    case mif_ellipsoid: strcat(buf,"Ellipsoid"); break;
    case mif_oval:      strcat(buf,"Oval"); break;
    case mif_pyramid:   strcat(buf,"Pyramid"); break;
    case mif_mask:      strcat(buf,"Mask"); break;
    default: errorcode++; buf[0]='\0'; break;
    }
  if(buf[0]!='\0') {
    outstr.Append(buf);
    if(GeomTypeParam.Length()>0) {
      outstr.Append(" ");
      char *gtp_argv[1];
      gtp_argv[0]=ab(GeomTypeParam.GetStr());
      char *gtp_cptr=Tcl_Merge(1,gtp_argv);  // Protect spaces
      if(gtp_cptr!=NULL) {
	outstr.Append(gtp_cptr);
	Tcl_Free((char *)gtp_cptr);
      }
    }
    outstr.Append("\n");
  }

  // Initial Magnetization
  outstr.Append("Init Mag: ");
  MagInitArgs.Get(sbuf);
  outstr.Append(sbuf);
  outstr.Append("\n");

  // Experiment Parameters 
  int frl_argc; CONST84 char ** frl_argv;
  if(Tcl_SplitList(NULL,ab(FieldRangeList.GetStr()),
		   &frl_argc,&frl_argv)==TCL_OK) {
    for(int i=0;i<frl_argc;i++) {
      outstr.Append("Field Range: ");
      outstr.Append(frl_argv[i]);
      outstr.Append("\n");
    }
    Tcl_Free((char *)frl_argv);
  }
  outstr.Append("Default Control Point Spec: ");
  outstr.Append(DefaultControlPointSpec);
  outstr.Append("\n");
  outstr.Append("Field Type: ");
  AppliedFieldArgs.Get(sbuf);
  outstr.Append(sbuf);
  outstr.Append("\n");

  // Output Specification
  char *ospec_argv[1],*ospec_cptr;
  outstr.Append("Base Output Filename: ");
  ospec_argv[0]=ab(OutBaseName.GetStr());
  ospec_cptr=Tcl_Merge(1,ospec_argv);  // Protect spaces
  if(ospec_cptr!=NULL) {
    outstr.Append(ospec_cptr);
    Tcl_Free((char *)ospec_cptr);
  }
  outstr.Append("\n");
  if(OutMagFormat.Length()>0) {
    outstr.Append("Magnetization Output Format: ");
    outstr.Append(OutMagFormat);  // OutMagFormat
    /// is formed via a call to Nb_DString::MergeArgs(), which
    /// creates a valid Tcl list with appropriate space protection.
    outstr.Append("\n");
  }
  if(OutTotalFieldFormat.Length()>0) {
    outstr.Append("Total Field Output Format: ");
    outstr.Append(OutTotalFieldFormat);  // OutTotalFieldFormat
    /// is formed via a call to Nb_DString::MergeArgs(), which
    /// creates a valid Tcl list with appropriate space protection.
    outstr.Append("\n");
  }
  if(OutDataTableFormat.Length()>0) {
    outstr.Append("Data Table Output Format: ");
    ospec_argv[0]=ab(OutDataTableFormat.GetStr());
    ospec_cptr=Tcl_Merge(1,ospec_argv);  // Protect spaces
    if(ospec_cptr!=NULL) {
      outstr.Append(ospec_cptr);
      Tcl_Free((char *)ospec_cptr);
    }
    outstr.Append("\n");
  }

  // Miscellaneous
  sprintf(buf,"Randomizer Seed: %d\n"
	  "Min Time Step: %.17g\nMax Time Step: %.17g\n",
	  RandomizerSeed,MinTimeStep,MaxTimeStep);
  outstr.Append(buf);
  if(UserComment.Length()>0) {
    outstr.Append("User Comment: ");
    outstr.Append(UserComment);
    outstr.Append("\n");
  }
  return outstr;
}
