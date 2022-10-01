/* FILE: dlist.h                 -*-Mode: c++-*-
 *
 * List  C++ template declarations
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2013/05/22 16:25:53 $
 * Last modified by: $Author: donahue $
 */

#ifndef _NB_DLIST
#define _NB_DLIST

#include <cassert>

#include "oc.h"   /* Includes tcl.h, needed for Tcl_SplitList */
#include "errhandlers.h"

/* End includes */     /* Optional directive to build.tcl */

//////////////////////////////////////////////////////////////////////////
// List template
//   The list is now (Sep-97) implemented as an expanding 2D array.  First
// a "spine" array arr of T** of size spine_xtnd_size is allocated.  Then
// "ribs" of T* of size rib_xntd_size are allocated as needed, and affixed
// to the spine. The variable alloc_spine is the size of the currently
// allocated spine.  (This will be an integral multiple of spine_xtnd_size,
// except as noted in the UPDATE NOTE below.) The variable used_spine is the
// index of the last currently used spine elt, i.e., the index of the last
// rib with valid data.
//   Each rib has the same allocated length, rib_xtnd_size.  The variable
// used_rib is the index of the last entry used off of the rib affixed to
// the spine at index used_spine.  If a request for a new elt is received,
// that elt is placed at
//
//                 arr[used_spine][++used_rib]
//
// unless ++used_rib is == rib_xtnd_size, in which case
//
//                 arr[++used_spine][used_rib=0]
//
// is used.  If ++used_spine>=alloc_rib_count, then a new rib will be
// allocated and affixed to the spine at index arr[++used_spine].  If
// this index >= alloc_spine, then the spine must be extended.  In this
// case, a new T** array is allocated, of length alloc_spine+spine_xtnd_size
// (but see UPDATE NOTE), and the old T** array is copied into it. The old
// array is released, and we continue as in the second case.  Since arr is
// a list of pointers, we can make spine_xtnd_size pretty large w/o eating
// up much memory, so hopefully the last case won't happen too often.  Even
// when it does (or alternately), pointers are short so they shouldn't take
// too long to copy. In addition, any pointers into the ribs stay valid
// across this extension.
//   The variables rib_index and spine_index mark the elt to be returned
// by the next GetNext() function call.  If spine_index>used_spine, or
// (spine_index==used_spine && rib_index==used_rib), then we are past the
// end of the used array space.
//
// UPDATE NOTE: The first several spine allocations are done with size
//   less than the full spine_xtnd_size.  See ExtendSpine().  This is
//   for more effective memory use when one allocates a large array
//   of small List's.

#define SPINE_EXTEND_SIZE 8             // Fixed value for spine_xtnd_size
#define RIB_EXTEND_MEMSIZE OC_PAGESIZE  // At initialization, the client
/// routine may specify a value for rib_xtnd_size, which will corresponds
/// to a memory footprint of rib_xtnd_size*sizeof(T).  If the client does
/// not explicitly set this value, then rib_xtnd_size is set to the default
/// of floor(RIB_EXTEND_MEMSIZE/sizeof(T)).

#define NB_LIST_DOC_INFO \
              "Nb_List", \
              "Michael J. Donahue (michael.donahue@nist.gov)", \
              "2.1.0","31-Oct-1998"

template<class T> class Nb_List;

template<class T> class Nb_List_Index {
private:
  friend class Nb_List<T>;
  OC_INDEX spine,rib;  // Index of "next" element.
public:
  Nb_List_Index() : spine(0),rib(0) {}
};

template<class T> class Nb_List {
private:
  const ClassDoc class_doc; // Static data member templates not
  /// implemented by gnu C++ compiler.

  OC_INDEX spine_xtnd_size,rib_xtnd_size; // Alloc increment amounts (>0).
  /// spine_xtnd_size *should* be a const class static data member,
  /// but some compilers don't yet allow this.

  OC_INDEX alloc_spine;                   // Allocated spine size.  The
  /// corresponding value for ribs is always just rib_xtnd_size.
  OC_INDEX alloc_rib_count;               // Number of ribs actually
  /// allocated.  This value should be <= alloc_spine.

  OC_INDEX last_used_spine_index;  // Index on spine of last rib with valid data
  OC_INDEX last_used_rib_index;    // Index on last rib of last valid elt

  T* next_elt;                  // Pointer to "next" elt, NULL iff past end
  OC_INDEX spine_index,rib_index;  // Index of "next" element.

  /// NOTE 1: When the list is empty, last_used_spine_index == -1 and
  ///  last_used_rib_index == rib_xtnd_size-1.  In fact, GetSize()==0
  ///  iff used_spine<0.
  /// NOTE 2: The variables next_elt, spine_index and rib_index should
  ///  be manipulated _only_ by the routines RestartGet() and GetNext(),
  ///  to ease the maintaining of class internal consistency, although
  ///  the Copy() function also sets these directly, and Append() will
  ///  update these values to point to the new last elt if next_elt
  ///  was NULL before Append() call.
  /// NOTE 3: The following orderings apply:
  ///
  ///   ALWAYS:
  ///           last_used_spine_index < alloc_rib_count <= alloc_spine
  ///           last_used_rib_index   < rib_xtnd_size
  ///
  ///   last_used_spine_index & last_used_rib_index mark where the
  ///   last appended element went (and are used to calculated where
  ///   to put the next appended element).  See NOTE 1 above.
  ///
  ///   IF(next_elt!=NULL):
  ///           spine_index  <= last_used_spine_index
  ///           rib_index    < rib_xtnd_size,
  ///    AND IF(spine_index == last_used_spine_index):
  ///           rib_index    <= last_used_rib_index
  ///
  ///   IF(next_elt==NULL AND last_used_rib_index+1 < rib_xtnd_size)
  ///           spine_index  = last_used_spine_index
  ///           rib_index    = last_used_rib_index+1
  ///   ELSE IF(next_elt==NULL AND last_used_rib_index+1==rib_xtnd_size)
  ///           spine_index  = last_used_spine_index+1
  ///           rib_index    = 0
  ///
  ///   spine_index and rib_index mark the location of the "next" element
  ///   to be returned by GetNext() (or PeekNext()).  next_elt is a pointer
  ///   into that memory location.  The last stanza above is the case
  ///   when the "next" element is past the end of valid data.
  /// NOTE 4: These internal index markers are deprecated.  Use the
  ///   Nb_List_Index class instead, which is passed back and forth to
  ///   external routines.

  T **arr;                      // Spine array
  void AddRib();                // Allocate a new rib & affix to spine,
  /// and increments alloc_rib_count.

  void ExtendSpine(OC_INDEX new_length=0);
  /// Typically called with new_length==0, in which case this routine
  /// allocates a new T** arr of size alloc_spine+spine_xtnd_size, copies
  /// the old T** arr into the new space, and frees the old array space.
  /// Should only be called by AddRib(), except that the "Copy" operator
  /// also calls this with new_length>0, for efficency.

  void Reset(OC_INDEX _rib_xtnd_size=0); // Initializes all member
  /// variables, and deallocates memory if arr!=NULL.  Because
  /// of the latter, if calling from a constructor, be sure
  /// to set arr=NULL first!
  /// SURPRISE: This is where deallocation is squirreled away,
  ///   so this routine is called by the destructor as well
  ///   as the constructors!!!

public:
  void Copy(const Nb_List<T> &src);

  Nb_List(OC_INDEX _rib_xtnd_size=0);   // Default constructor
  Nb_List(const Nb_List<T> &src);       // Copy constructor

  ~Nb_List() { Reset(rib_xtnd_size); }
  void Free(void) { Reset(rib_xtnd_size); }
  OC_INDEX GetSize() const { 
    return (last_used_spine_index*rib_xtnd_size)+last_used_rib_index+1;
  }
  void RestartGet();

  // Old iterators.  Maintained for backwards compatibility.
  T *GetNext();
  T *PeekNext() { return next_elt; }
  T *GetFirst() { RestartGet(); return GetNext(); }
  /// A convenience routine to use inside for-loops, for example:
  ///   for( T* tp=list.GetFirst() ; tp!=NULL ; tp=list.GetNext()) {...}

  // New iterators.  Use these in preference to the previous iterators.
  // These pass a index key to and from the calling routine, so calls
  // can be mixed across different routines, and these can be 'const'.
  // In these routines, 'key' holds the index of the current element.
  const T* GetFirst(Nb_List_Index<T>& key) const;
  T* GetFirst(Nb_List_Index<T>& key);
  const T* GetNext(Nb_List_Index<T>& key) const;
  T* GetNext(Nb_List_Index<T>& key);
  /// If all ready at end of list, then return value is NULL, and key is
  /// set to point to last valid element (if any).

  OC_INT4m GetElt(const Nb_List_Index<T>& key,T& elt) const;
  /// Copies from position specified by key into elt.  Returns 0
  /// on success, 1 if key is invalid or out-of-bounds

  OC_INT4m ReplaceElt(const Nb_List_Index<T>& key,const T &elt);
  /// Copies elt into position specified by key.  Returns 0
  /// on success, 1 if key is invalid or out-of-bounds

  void Clear(void) { 
    last_used_spine_index=-1;
    last_used_rib_index=rib_xtnd_size-1;
    RestartGet();
  }
  void SetFirst(const T &elt); // Copies elt & sets "used" length to 1.
  void Append(const T &elt);   // Indexes off of "used" length.
  /// Updates last_used_rib_index and last_used_spine_index.  Also
  /// updates next_elt pointer if it had pointed past the end of
  /// valid data.

  // Construct a list from a character string representation of
  // a Tcl list.  This routine is only valid for those class T
  // that have a cast from char* to T.  Return value is 1 on
  // success, or 0 if import "str" is not a valid Tcl list.
  OC_BOOL TclSplit(const char* str) {
    Free(); // Release current *this storage, if any.

    int argc;
    CONST84 char ** argv;
    if(Tcl_SplitList(NULL,Oc_AutoBuf(str),&argc,&argv)!=TCL_OK) {
      return 0; // Bad list
    }

    if(argc>0) {
      SetFirst(T(argv[0]));
    }
    for(int i=1;i<argc;++i) {
      Append(T(argv[i]));
    }

    Tcl_Free((char *)argv);

    return 1; // Success
  }

  Nb_List<T> &operator=(const Nb_List<T> &other) { Copy(other); return *this; }

  // Debug and optimization aids
  OC_INDEX GetAllocRibCount() const { return alloc_rib_count; }
  OC_INDEX GetSpaceWaste() const; // Returns # of unused _bytes_ allocated.
  void DumpState(FILE* fptr=stderr) const;

};

// Basic initialization AND de-initialization!
template<class T> void Nb_List<T>::Reset(OC_INDEX _rib_xtnd_size)
{
#define MEMBERNAME "Reset()"
  if(alloc_spine>0 && arr!=NULL) {
    // Deallocate existing memory
    for(OC_INDEX i=alloc_rib_count-1;i>=0;--i) {
      delete[] arr[i];
    }
    delete[] arr;
  }
  alloc_rib_count=0; alloc_spine=0;  arr=NULL;

  spine_xtnd_size=SPINE_EXTEND_SIZE;
  if(_rib_xtnd_size>0) rib_xtnd_size=_rib_xtnd_size;
  else                 rib_xtnd_size=RIB_EXTEND_MEMSIZE / OC_INDEX(sizeof(T));
  if(rib_xtnd_size<1)  rib_xtnd_size=1; // Absolute minimum extend size!

  last_used_spine_index = -1;
  last_used_rib_index   = rib_xtnd_size-1;
  RestartGet();
#undef MEMBERNAME
}

// Default constructor
template<class T> Nb_List<T>::Nb_List(OC_INDEX _rib_xtnd_size)
  : class_doc(NB_LIST_DOC_INFO)
{
  arr=NULL;  Reset(_rib_xtnd_size);
  /// Note: If _rib_xtnd_size is 0, then one gets the default length,
  ///  which is RIB_EXTEND_MEMSSIZE/sizeof(T);  RIB_EXTEND_MEMSIZE is
  ///  currently OC_PAGESIZE, which is (suppose to be) system dependent,
  ///  but 4K is a good guess.  If one plans to use many small lists, then
  ///  this value should be specified (and small).
}

// Copy constructor
template<class T> Nb_List<T>::Nb_List(const Nb_List<T> &src)
  : class_doc(NB_LIST_DOC_INFO)
{
  arr=NULL;  Reset(src.rib_xtnd_size);  Copy(src);
}

template<class T> void Nb_List<T>::Copy(const Nb_List<T> &src)
{
#define MEMBERNAME "Copy(Nb_List<T> &)"
  if(&src==this) return;  // Nothing to do!

  // Currently just pitch any alloc'ed space in *this and
  // start fresh.  If heavy use is made of this function
  // to lists having lots of alloc'ed space (e.g., NOT
  // the copy constructor), then we should be more sophisticated
  // and try to hold onto existing alloc'ed memory.  Even then,
  // though, if the rib_xtnd_size's of the two are different,
  // then we have to pitch everything anyway.
  Reset(src.rib_xtnd_size);
  if(spine_xtnd_size!=src.spine_xtnd_size)
    FatalError(-1,STDDOC,"Supposed class const spine_xtnd_size isn't!");
  ExtendSpine(src.alloc_spine);
  for(OC_INDEX i=0;i<src.alloc_rib_count;i++) AddRib();

  // Copy ribs.  NOTE: It is ASSUMED that the class T
  // has a valid operator=().
  last_used_spine_index=src.last_used_spine_index;
  last_used_rib_index=src.last_used_rib_index;
  if(last_used_spine_index>=0) {
    // Non-empty list
    for(spine_index=0;spine_index<last_used_spine_index;spine_index++) {
      for(rib_index=0;rib_index<rib_xtnd_size;rib_index++)
	arr[spine_index][rib_index]=src.arr[spine_index][rib_index];
    }
    // Copy last, possibly incomplete, rib
    for(rib_index=0;rib_index<=last_used_rib_index;rib_index++)
      arr[spine_index][rib_index]=src.arr[spine_index][rib_index];
  }

  // Update remaining member variables
  spine_index=src.spine_index;
  rib_index=src.rib_index;
  if(src.next_elt==NULL) next_elt=NULL;
  else                   next_elt=&(arr[spine_index][rib_index]);

#undef MEMBERNAME
}

template<class T> void Nb_List<T>::ExtendSpine(OC_INDEX new_length)
{ // Typically called with new_length==0, in which case this routine
  // allocates a new T** arr of size alloc_spine+spine_xtnd_size, copies
  // the old T** arr into the new space, and frees the old array space.
  // Should only be called by AddRib(), except that the "Copy" operator
  // also calls this with new_length>0, for efficency.
#define MEMBERNAME "ExtendSpine"
  // First several extensions add a piece smaller than full spine_xtnd_size.
  OC_INDEX xtnd_size=new_length;
  if(xtnd_size<=0) {
    if(alloc_spine>=spine_xtnd_size)
      xtnd_size=spine_xtnd_size;
    else {
      OC_INDEX small_add=OC_MAX(1,spine_xtnd_size/4);
      xtnd_size=spine_xtnd_size-alloc_spine;
      if(xtnd_size>=2*small_add)
	xtnd_size=small_add;
    }
  }
  if(xtnd_size<1) xtnd_size=1;  // Safety

  T** tarr;
  assert(alloc_spine>=0);
  if((tarr=new T*[size_t(alloc_spine+xtnd_size)])==0)
    FatalError(-1,STDDOC,ErrNoMem);
  if(arr!=NULL) {
    if(alloc_rib_count>alloc_spine)
      FatalError(-1,STDDOC,
		 "Programming error: alloc_rib_count=%d > alloc_spine=%d",
		 alloc_rib_count,alloc_spine);
    for(OC_INDEX i=0;i<alloc_rib_count;i++) tarr[i]=arr[i];
    delete[] arr;
  }
  arr=tarr;
  alloc_spine+=xtnd_size;
#undef MEMBERNAME
}

template<class T> void Nb_List<T>::AddRib()
{ // Allocates a new rib & affixes to spine, calling ExtendSpine() &
  // incrementing alloc_rib_count as necessary.
#define MEMBERNAME "AddRib"
  if(alloc_rib_count+1>alloc_spine) ExtendSpine();
  assert(rib_xtnd_size>=0);
  if((arr[alloc_rib_count++]=new T[size_t(rib_xtnd_size)])==0)
    FatalError(-1,STDDOC,ErrNoMem);
#undef MEMBERNAME
}

template<class T> OC_INT4m
Nb_List<T>::GetElt(const Nb_List_Index<T>& key,T& elt) const
{ // Copies from position specified by key into elt.  Returns 0
  // on success, 1 if key is invalid or out-of-bounds

  if(key.spine<0 || key.rib<0 || key.rib>=rib_xtnd_size) {
    return 1; // Invalid key
  }
  if(key.spine>last_used_spine_index
     || (key.spine==last_used_spine_index
	 && key.rib>last_used_rib_index)) {
    return 1; // Out-of-bounds
  }

  // Valid element
  elt=arr[key.spine][key.rib];  /// ASSUMES a valid class T operator=().

  return 0;
}

template<class T> OC_INT4m
Nb_List<T>::ReplaceElt(const Nb_List_Index<T>& key,const T &elt)
{ // Copies elt into position specified by key.  Returns 0
  // on success, 1 if key is invalid or out-of-bounds

  if(key.spine<0 || key.rib<0 || key.rib>=rib_xtnd_size) {
    return 1; // Invalid key
  }
  if(key.spine>last_used_spine_index
     || (key.spine==last_used_spine_index
	 && key.rib>last_used_rib_index)) {
    return 1; // Out-of-bounds
  }

  // Valid element
  arr[key.spine][key.rib]=elt;  /// ASSUMES a valid class T operator=().

  return 0;
}

template<class T> void Nb_List<T>::Append(const T &elt)
{
#define MEMBERNAME "Append"
  if(++last_used_rib_index>=rib_xtnd_size) {
    while(last_used_spine_index+1>=alloc_rib_count) AddRib();
    last_used_rib_index=0;
    last_used_spine_index++;
  }
  arr[last_used_spine_index][last_used_rib_index]=elt;
  /// ASSUMES a valid class T operator=().

  if(next_elt==NULL) {
    // We _were_ pointing past end of the list...but not now!
    spine_index=last_used_spine_index;  // Should be redundant
    rib_index=last_used_rib_index;      //        ""
    next_elt=&(arr[spine_index][rib_index]);
  }
  /// Note: if next_elt != NULL, then it holds a valid address into one
  ///   of the arr[] subarrays, and so remains valid even after spine
  ///   extension.
#undef MEMBERNAME
}

template<class T> void Nb_List<T>::SetFirst(const T &elt)
{
#define MEMBERNAME "SetFirst"
  Clear();
  Append(elt);
#undef MEMBERNAME
}

template<class T> void Nb_List<T>::RestartGet()
{
  spine_index=rib_index=0;
  if(last_used_spine_index<0) next_elt=NULL;
  else                        next_elt=&(arr[0][0]);
  /// Empty list case has last_used_spine_index<0
}

template<class T> T* Nb_List<T>::GetNext()
{
  T* tptr=next_elt;
  // If we are not already past end of used space, increment "next" pointer
  if(tptr!=NULL) {
    if((++rib_index)>=rib_xtnd_size) { rib_index=0; ++spine_index; }
    if(spine_index>last_used_spine_index
       || (spine_index==last_used_spine_index
	   && rib_index>last_used_rib_index)) {
      next_elt=NULL;
    }
    else {
      next_elt=&(arr[spine_index][rib_index]);
    }
  }
  return tptr;
}

template<class T> const T*
Nb_List<T>::GetFirst(Nb_List_Index<T>& key) const
{
  T* tptr=NULL;
  if(last_used_spine_index<0) {
    /// Empty list case has last_used_spine_index<0
    /// Leave tptr==NULL.
    key.spine=last_used_spine_index;
    key.rib=last_used_rib_index;
  } else {
    key.spine=0;
    key.rib=0;
    tptr=&(arr[0][0]);
  }
  return tptr;
}

template<class T> T*
Nb_List<T>::GetFirst(Nb_List_Index<T>& key)
{
  const Nb_List<T> * const mythis = this;
  /// Use the implementation in the 'const' member function
  return (T*)(mythis->GetFirst(key));
}

template<class T> const T*
Nb_List<T>::GetNext(Nb_List_Index<T>& key) const
{ // Increments key to point to the next element, and returns
  // a pointer to that element.  If the next element is past the
  // end of the list, then NULL is returned, and key is set to
  // the last valid element.
  T* tptr=NULL;
  OC_INDEX rib=key.rib;
  OC_INDEX spine=key.spine;
  if((++rib)>=rib_xtnd_size) { rib=0; ++spine; }
  if(spine<last_used_spine_index
     || (spine==last_used_spine_index && rib<=last_used_rib_index)) {
    // Valid element
    tptr=&(arr[spine][rib]);
    key.rib=rib;
    key.spine=spine;
  } else {
    // Past list end (leave tptr==NULL)
    key.rib=last_used_rib_index;
    key.spine=last_used_spine_index;
  }
  return tptr;
}

template<class T> T*
Nb_List<T>::GetNext(Nb_List_Index<T>& key)
{
  const Nb_List<T> * const mythis = this;
  /// Use the implementation in the 'const' member function
  return (T*)(mythis->GetNext(key));
}

// Debug and optimization aids

template<class T> OC_INDEX Nb_List<T>::GetSpaceWaste() const
{ // Returns number of _bytes_ allocated but not used
  OC_INDEX spine_waste=(alloc_spine-last_used_spine_index-1)
    * OC_INDEX(sizeof(T*));
  /// Unused space on the spine

  OC_INDEX full_rib_size=rib_xtnd_size * OC_INDEX(sizeof(T));
  OC_INDEX eves_ribs=(alloc_rib_count-last_used_spine_index-1)*full_rib_size;
  /// Ribs allocated but completely unused

  OC_INDEX spare_rib=(rib_xtnd_size-last_used_rib_index) * OC_INDEX(sizeof(T));
  /// Leftover space on the last rib

  return spine_waste+eves_ribs+spare_rib;
}

template<class T> void Nb_List<T>::DumpState(FILE* fptr) const
{
  fprintf(fptr,"      spine_xtnd_size = %3" OC_INDEX_MOD "d\n",
          spine_xtnd_size);
  fprintf(fptr,"        rib_xtnd_size = %3" OC_INDEX_MOD "d\n",
          rib_xtnd_size);
  fprintf(fptr,"          alloc_spine = %3" OC_INDEX_MOD "d\n",
          alloc_spine);
  fprintf(fptr,"      alloc_rib_count = %3" OC_INDEX_MOD "d\n",
          alloc_rib_count);
  fprintf(fptr,"last_used_spine_index = %3" OC_INDEX_MOD "d\n",
          last_used_spine_index);
  fprintf(fptr,"  last_used_rib_index = %3" OC_INDEX_MOD "d\n",
          last_used_rib_index);
  fprintf(fptr,"             next_elt = %p\n",next_elt);
  fprintf(fptr,"          spine_index = %3" OC_INDEX_MOD "d\n",spine_index);
  fprintf(fptr,"            rib_index = %3" OC_INDEX_MOD "d\n",rib_index);
  fprintf(fptr,"                  arr = %p\n",arr);
}

#endif // _NB_DLIST
