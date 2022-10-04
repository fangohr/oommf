/* FILE: dstring.cc                   -*-Mode: c++-*-
 *
 * Generic C++ Dynamic String class
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2013/05/22 07:15:32 $
 * Last modified by: $Author: donahue $
 */

#include <cctype>
#include <climits>
#include <cstring>

#include "dstring.h"
#include "errhandlers.h"

/* End includes */     

//////////////////////////////////////////////////////////////////////////
// String class
//
const ClassDoc Nb_DString::class_doc("Nb_DString",
	      "Michael J. Donahue (michael.donahue@nist.gov)",
	      "2.0.0","Jan-1998");

void Nb_DString::Empty()
{
#define MEMBERNAME "Empty"
  if(str!=NULL) delete[] str;
  bufsize=1;
  str=new char[size_t(bufsize)];
  if(str==0) FatalError(-1,STDDOC,"No memory");
  str[0]='\0';
#undef MEMBERNAME
}

Nb_DString::Nb_DString()
{
#define MEMBERNAME "Nb_DString"
  str=NULL;
  Empty();
#undef MEMBERNAME
}

Nb_DString::Nb_DString(const char *cptr)
{
#define MEMBERNAME "Nb_DString(const char *cptr)"
  if(cptr==NULL) {
    bufsize=1;
  } else {
    size_t size = strlen(cptr);
    bufsize=static_cast<OC_INDEX>(size+1);
    assert(size < size_t(-1) && bufsize>0 && size+1 == size_t(bufsize));
  }
  str=new char[size_t(bufsize)];
  if(str==0) FatalError(-1,STDDOC,"No memory");
  if(cptr==NULL) str[0]='\0';
  else           strcpy(str,cptr);
#undef MEMBERNAME
}

Nb_DString::Nb_DString(const Nb_DString &rhs)
{
#define MEMBERNAME "Nb_DString(const String &)"
  bufsize=rhs.Length()+1;
  assert(bufsize>0);
  str=new char[size_t(bufsize)];
  if(str==0) FatalError(-1,STDDOC,"No memory");
  strcpy(str,rhs.str);
#undef MEMBERNAME
}

Nb_DString::Nb_DString(OC_INDEX _bufsize)
{ // Allocates buffer of length _bufsize and fills with a null string.
#define MEMBERNAME "Nb_DString(OC_INDEX)"
  if(_bufsize<1) bufsize=1;
  else           bufsize=_bufsize;
  str=new char[size_t(bufsize)];
  if(str==0) FatalError(-1,STDDOC,"No memory");
  str[0]='\0';
#undef MEMBERNAME
}

Nb_DString::~Nb_DString()
{
#define MEMBERNAME "~Nb_DString"
  delete[] str;
  bufsize=0;
#undef MEMBERNAME
}

Nb_DString& Nb_DString::Dup(const char *cptr)
{
#define MEMBERNAME "Dup(const char *)"
  if(cptr==NULL) FatalError(-1,STDDOC,"Null pointer passed (cptr)");
  if(cptr==str) return *this;  // Nothing to do!
  size_t stringsize = strlen(cptr);
  OC_INDEX new_length=static_cast<OC_INDEX>(stringsize);
  assert(new_length>=0 && stringsize == size_t(new_length));
  if(new_length>=bufsize) {
    // Old buffer too small; throw it away and allocate new one
    delete[] str;
    bufsize=new_length+2;    // Safety
    assert(bufsize>1);
    str=new char[size_t(bufsize)];
    if(str==0) FatalError(-1,STDDOC,"No memory");
  }
  strncpy(str,cptr,size_t(new_length)+1);
  str[new_length]='\0';           // Safety
  return *this;
#undef MEMBERNAME
}


Nb_DString& Nb_DString::operator=(const Nb_DString &rhs)
{ // Assignment operator
#define MEMBERNAME "operator=(const Nb_DString &)"
  if(&rhs==this) return *this;  // Nothing to do!
  Dup(rhs.str);
  return *this;
#undef MEMBERNAME
}

Nb_DString& Nb_DString::operator=(const char *cptr)
{ // Assignment operator
#define MEMBERNAME "operator=(const char *)"
  Dup(cptr);
  return *this;
#undef MEMBERNAME
}

void Nb_DString::ExtendBuf(OC_INDEX newsize)
{
#define MEMBERNAME "ExtendBuf(OC_INDEX)"
  assert(newsize>=0);
  if(bufsize>=newsize) return; // Nothing to do
  OC_INDEX addsize = newsize-bufsize;
  if(addsize<64) addsize+=8;
  if(addsize<16) addsize=16;
  char* newstr=new char[size_t(bufsize+addsize)];
  if(newstr==0) FatalError(-1,STDDOC,"No memory");
  memcpy(newstr,str,static_cast<size_t>(bufsize));
  delete[] str;
  str=newstr;
  bufsize+=addsize;
  assert(bufsize>=addsize); // Overflow check
#undef MEMBERNAME
}

// Append functions; grows str as needed.  The char version appends
// up to the first '\0' character or maxlen, which ever happens first.
// Both return *this.
Nb_DString& Nb_DString::Append(const Nb_DString& appendstr)
{
#define MEMBERNAME "Append(const Nb_DString&)"
  // This routine should work OK if appendstr==*this.
  if(appendstr.str[0]=='\0') return *this; // Nothing to do.
  OC_INDEX len1=Length();
  OC_INDEX len2=appendstr.Length();
  OC_INDEX len3=len1+len2;
  assert(len3>=0);
  if(len3>=bufsize) {
    // Old buffer too small; throw it away and allocate new one
    bufsize=len3+1;
    assert(bufsize>0);
    char *newstr=new char[size_t(bufsize)];
    if(newstr==0) FatalError(-1,STDDOC,"No memory");
    memcpy(newstr,str,size_t(len1));
    delete[] str;
    str=newstr;
  }
  memcpy(str+len1,appendstr,size_t(len2));
  str[len3]='\0';
  return *this;
#undef MEMBERNAME
}

Nb_DString& Nb_DString::Append(const char *appendarr,OC_INDEX maxlen)
{
#define MEMBERNAME "Append(const char*,OC_INDEX)"
  // Determine length of string to append
  OC_INDEX len2=0;
  if(maxlen>1) {
    while(len2<maxlen && appendarr[len2]!='\0') len2++;
  } else {
    size_t appendsize = strlen(appendarr);
    len2=static_cast<OC_INDEX>(appendsize);
    assert(len2>=0 && appendsize == size_t(len2));
  }
  if(len2<1) return *this; // Nothing to do.
  OC_INDEX len1=Length();
  OC_INDEX len3=len1+len2;
  char *dummy_str=NULL;  // To handle case when appendarr points into str[]
  if(len3>=bufsize) {
    // Old buffer too small; throw it away and allocate new one
    bufsize=len3+1;
    assert(bufsize>0);
    char *newstr=new char[size_t(bufsize)];
    if(newstr==0) FatalError(-1,STDDOC,"No memory");
    memcpy(newstr,str,static_cast<size_t>(len1));
    dummy_str=str;
    str=newstr;
  }
  memcpy(str+len1,appendarr,static_cast<size_t>(len2));
  str[len3]='\0';
  if(dummy_str!=NULL) delete[] dummy_str;
  return *this;
#undef MEMBERNAME
}

// Appends the contents of argv[0] through argv[argc-1] to
// *this, inserting a single space between elements.
Nb_DString& Nb_DString::AppendArgs(int argc,const char **argv)
{
  if(argc<1) return *this; // Nothing to do
  int i;
  OC_INDEX appendlen=0;
  for(i=0;i<argc;i++) appendlen += static_cast<OC_INDEX>(strlen(argv[i]));
  assert(appendlen>=0);

  // Add space for separating spaces
  OC_INDEX curlen=Length();
  int leadspace=1; // Set to 1 if we need a leading space before argv[0]
  if(curlen<1 || isspace(str[curlen-1])) leadspace=0;
  if(leadspace) appendlen+=argc; 
  else          appendlen+=(argc-1);
  OC_INDEX newbufsize=curlen+appendlen+1;
  assert(newbufsize>0);
  char *newstr=new char[size_t(newbufsize)];
  strcpy(newstr,str);
  if(leadspace) strcat(newstr," ");
  for(i=0;i<argc;i++) {
    strcat(newstr,argv[i]);
    if(i+1<argc) strcat(newstr," ");
  }
  delete[] str;  // Delete old string
  str=newstr;
  bufsize=newbufsize;
  return *this;
}

// Appends the contents of argv[0] through argv[argc-1] to
// *this, inserting a single space between elements.
Nb_DString& Nb_DString::AppendArgs(int argc,const Nb_DString *argv)
{
  if(argc<1) return *this; // Nothing to do
  int i;
  OC_INDEX appendlen=0;
  for(i=0;i<argc;i++) appendlen+=argv[i].Length();

  // Add space for separating spaces
  OC_INDEX curlen=Length();
  int leadspace=1; // Set to 1 if we need a leading space before argv[0]
  if(curlen<1 || isspace(str[curlen-1])) leadspace=0;
  if(leadspace) appendlen+=argc;
  else          appendlen+=(argc-1);
  OC_INDEX newbufsize=curlen+appendlen+1;
  assert(newbufsize>0);
  char *newstr=new char[size_t(newbufsize)];
  strcpy(newstr,str);
  if(leadspace) strcat(newstr," ");
  for(i=0;i<argc;i++) {
    strcat(newstr,argv[i].GetStr());
    if(i+1<argc) strcat(newstr," ");
  }
  delete[] str;  // Delete old string
  str=newstr;
  bufsize=newbufsize;
  return *this;
}

// Merges the contents of argv[0] through argv[argc-1],
// maintaining proper Tcl list structure, and sets the
// result into *this.  (Based on the Tcl_Merge function.)
Nb_DString& Nb_DString::MergeArgs(int argc,const Nb_DString *argv)
{
  // Copy Nb_DString argv into array of char*'s.
  int i;
  OC_INDEX totalsize;
  for(i=0,totalsize=0;i<argc;i++) totalsize+=argv[i].Length()+1;
  assert(totalsize>0);
  char **cargv = 0;
  try {
    cargv=new char*[size_t(argc)];
    cargv[0]=new char[size_t(totalsize)];
    strcpy(cargv[0],argv[0].GetStr());
    for(i=1;i<argc;i++) {
      cargv[i]=cargv[i-1]+argv[i-1].Length()+1;
      strcpy(cargv[i],argv[i].GetStr());
    }

    // Call Tcl_Merge to form proper list.
    char* cptr=Tcl_Merge(argc,cargv);
    if(cptr!=NULL) {
      Dup(cptr);
      Tcl_Free(cptr);
    }
  } catch(...) {
    delete[] cargv[0];
    delete[] cargv;
    throw;
  }

  // Release scratch space
  delete[] cargv[0];
  delete[] cargv;
  return *this;
}
Nb_DString& Nb_DString::MergeArgs(const Nb_List<Nb_DString>& argv)
{
  // Copy argv into array of char*'s.
  Nb_List_Index<Nb_DString> key;
  OC_INDEX argc = argv.GetSize();
  OC_INDEX totalsize=0;
  const Nb_DString* argptr;
  for(argptr=argv.GetFirst(key); argptr!=NULL; argptr=argv.GetNext(key)) {
    totalsize += argptr->Length() + 1;
  }
  assert(totalsize>=argc);
  Oc_AutoBuf buf(static_cast<size_t>(totalsize));
  char **cargv = 0;
  try {
    cargv=new char*[size_t(argc)];
    int i=0;
    for(argptr=argv.GetFirst(key); argptr!=NULL; argptr=argv.GetNext(key)) {
      if(i==0) {
        cargv[0]=buf.GetStr();
      } else {
        cargv[i]=cargv[i-1] + strlen(cargv[i-1]) + 1;
      }
      strcpy(cargv[i],argptr->GetStr());
      ++i;
    }

    // Call Tcl_Merge to form proper list.
    assert(argc<INT_MAX);
    char* cptr=Tcl_Merge(static_cast<int>(argc),cargv);
    if(cptr!=NULL) {
      Dup(cptr);
      Tcl_Free(cptr);
    }
  } catch(...) {
    delete[] cargv;
    throw;
  }

  // Release scratch space
  delete[] cargv;
  return *this;
}

// Functions to trim leading & trailing whitespace (as defined by
// 'isspace' function) from string.
// NOTE: These routines currently operate "in-place"
Nb_DString& Nb_DString::TrimLeft()
{
  char *cptr1;
  cptr1=str;
  while(*cptr1!='\0' && isspace(*cptr1)) cptr1++;
  if(*cptr1=='\0') {
    str[0]='\0'; // Blank string
    return *this;
  }
  // *cptr1 points to first non-space

  if(cptr1!=str) {
    char *cptr2,ch;
    // Down copy string
    cptr2=str;
    do {
      *(cptr2++)=(ch=*(cptr1++));
    } while(ch!='\0');
  }
  return *this;
}

Nb_DString& Nb_DString::TrimRight()
{
  OC_INDEX i=Length();
  OC_INDEX newend=0;
  while(i>0) {
    if(!isspace(str[--i])) {
      newend=i+1;
      break;
    }
  }
  str[newend]='\0';  // If entire string is whitespace, then newend==0.
  return *this;
}

Nb_DString& Nb_DString::Trim()
{
  TrimRight();
  TrimLeft();
  return *this;
}

// Function to collapse runs of whitespace into single blanks.
// Also removes leading and trailing whitespace.  Both normal
// function and member function versions.
void CollapseStr(char *str)
{
  OC_INDEX i1,i2,lastblank;  char ch;
  for(i1=i2=0,lastblank=1;str[i2]!='\0';i2++) {
    if(isspace(ch=str[i2])) {
      if(!lastblank) { str[i1++]=' '; lastblank=1; }
    }
    else { str[i1++]=ch; lastblank=0; }
  }
  if(i1>0 && str[i1-1]==' ') str[i1-1]='\0';  // Remove trailing whitespace
  else                       str[i1]='\0';
}

Nb_DString& Nb_DString::CollapseStr()
{
  ::CollapseStr(str);
  return *this;
}

// Function to remove all whitespace
Nb_DString& Nb_DString::StripWhite()
{
  char ch,*cptr1,*cptr2;
  cptr1=cptr2=str;
  while((ch = *(cptr1++))!='\0') {
    if(!isspace(ch)) *(cptr2++)=ch;
  }
  *cptr2='\0';
  return *this;
}

// Converts string to lowercase, using C library function tolower().
// Both normal function and member function versions.
void LowerString(char *str)
{
  char *cptr=str;
  while(*cptr!='\0') {
    *cptr=(char)tolower(*cptr);
    cptr++;
  }
}

Nb_DString& Nb_DString::ToLower()
{
  LowerString(str);
  return *this;
}

OC_INT4m StringCompare(const Nb_DString &a,const Nb_DString &b)
{ // Replicates strcmp
  return strcmp(a.str,b.str);
}

OC_INDEX StringSpan(const Nb_DString &a,const char *b)
{ // Replicates strspn
  size_t span = strspn(a.str,b);
  OC_INDEX result = static_cast<OC_INDEX>(span);
  assert(result>=0 && span == size_t(result));
  return result;
}

Nb_DString operator+(const Nb_DString& a,const Nb_DString& b)
{
  Nb_DString result(a);
  result.Append(b);
  return result;
}
