/* FILE: ptsearch.h                 -*-Mode: c++-*-
 *
 * Classes for searching unordered lists of points in space.
 *
 * Last modified on: $Date: 2013/05/22 07:15:38 $
 * Last modified by: $Author: donahue $
 */

#ifndef _VF_PTSEARCH
#define _VF_PTSEARCH

#include "nb.h"

/* End includes */     // This is an optional directive to build.tcl,
                       // that excludes the remainder of the file from
                       // '#include ".*"' dependency building

#define VF_BOX_REFINELEVEL 5    // Default box refinement level.
/// 4**VF_BOX_REFINELEVEL >= # of search boxes, and probably closer to
/// = than to > :-)   NOTE: 4**5 = 1024

//////////////////////////////////////////////////////////////////////////
struct Vf_BoxElt {
  OC_INT4m select_count;      // Convenience variable, incremented by
  /// BoxList::GetClosest2D(...), used to keep track of number of
  /// times this element has been included in a client list.
  Nb_LocatedVector<OC_REAL8> lv;
  Vf_BoxElt() { select_count=0; }
  Vf_BoxElt(const Nb_LocatedVector<OC_REAL8> &_lv) { select_count=0; lv=_lv; }

  // A couple convenience functions
  OC_REAL8 DistSq2D(const Vf_BoxElt &other) const {
    OC_REAL8 xdelta=lv.location.x-other.lv.location.x;
    OC_REAL8 ydelta=lv.location.y-other.lv.location.y;
    return xdelta*xdelta+ydelta*ydelta;
  }
  OC_REAL8 DistSq2D(const Nb_Vec3<OC_REAL8> &v) const {
    OC_REAL8 xdelta=lv.location.x-v.x;
    OC_REAL8 ydelta=lv.location.y-v.y;
    return xdelta*xdelta+ydelta*ydelta;
  }
};


//////////////////////////////////////////////////////////////////////////
// Atomic LocatedVector* list element.
class Vf_BoxList;  // Forward declaration
class Vf_Box {
private:
  static const ClassDoc class_doc;
  OC_REAL8 xmin,ymin,xmax,ymax;  // Box extents
  Nb_List<Vf_BoxElt *> *list;    // Comparison list
  void FillList(Nb_List<Vf_BoxElt *> *biglist); // Fills "list" with a refined
  /// version of biglist using both Sup & L2 norms; also sets incount.
  OC_INDEX incount;              // Number of elements of list inside box
  OC_BOOL IsIn2D(OC_REAL8 x,OC_REAL8 y) const;
public:
  Vf_Box() { list=NULL; xmin=ymin=xmax=ymax=(OC_REAL8)0.0; incount=0; }
  ~Vf_Box() { if(list!=NULL) delete list; }
  void InitNoRefine(OC_REAL8 _xmin,OC_REAL8 _ymin,OC_REAL8 _xmax,OC_REAL8 _ymax,
		    Nb_List<Vf_BoxElt> *wholelist);
  /// Used for initialization of the first box *only*.  Sets up pointers
  /// in Nb_List<Vf_BoxElt *> list to the entire Nb_List<Vf_BoxElt> biglist
  /// WITHOUT refinement.  Also sets incount.

  void Init(OC_REAL8 _xmin,OC_REAL8 _ymin,OC_REAL8 _xmax,OC_REAL8 _ymax,
	    Vf_Box &bigbox);
  /// Called by Vf_BoxList::Refine(OC_INDEX), this routine first deletes
  /// Nb_List<Vf_BoxElt *> list, if it exists, and then copies those
  /// elements of Nb_List<Vf_BoxElt *> biglist that could be "closest
  /// 2D position" to current box, as defined by above box extents.
  /// Also sets incount.

  OC_BOOL IsIn2D(const Nb_Vec3<OC_REAL8> &pos) const; /// True if pos is inside box

  OC_REAL8m GetBoxArea() const;   // Returns (xmax-xmin)*(ymax-ymin)
  OC_INDEX GetSpaceWaste() const; // Returns size of unused list space (bytes)

  OC_INDEX InCount(void) const { return incount; }
  OC_INDEX ListCount(void) const { return list->GetSize(); }
  Vf_BoxElt* GetClosest2D(const Nb_Vec3<OC_REAL8> &pos);
};

//////////////////////////////////////////////////////////////////////////
// Client interface to a list of Vf_BoxElt, Vf_Box* box, which is a list of
// lists of LocatedVector*'s.  Also holds Nb_List<Vf_BoxElt> wholelist,
// which is the list of LocatedVector's pointed into by entries of
// Vf_Box* box.
class Vf_BoxList_Index
{
public:
  Nb_List_Index<Vf_BoxElt> key;
};

class Vf_BoxList
{
private:
  static const ClassDoc class_doc;
  OC_REAL8 xmin,ymin,xmax,ymax; // Whole box region
  OC_INDEX Nx,Ny;               // Subbox counts, x & y
  OC_REAL8 xdelta,ydelta;       // Box cell dimensions
  Vf_Box* box;                  // Array, size=Nx*Ny
  OC_BOOL box_valid;            // True if current box refinement is valid.
  Nb_List<Vf_BoxElt> *wholelist;
  /// It is into this list that the lists of box[] point.
  OC_REAL8 aveincount,avelistcount; // Box averages; set by ::Refine()
  void MakeBoxValid();       // Convenience routine.  Does nothing
  /// if box_valid is true.  Otherwise, calls ::Refine(), and if
  /// DEBUGLVL is large enough then issues a warning message.
  void RefineOnce();         // Called as needed from ::Refine()
  void InitBox();            // Should only be called when box==NULL
public:
  void ClearList();  // Releases *box, empties wholelist, and zeros
  /// box region x/ymin, x/ymax.

  Vf_BoxList();
  ~Vf_BoxList();

  OC_INDEX GetSize() const {
    return wholelist->GetSize();
  }
  OC_REAL8 GetAveInCount() { MakeBoxValid(); return aveincount; }
  OC_REAL8 GetAveListCount() { MakeBoxValid(); return avelistcount; }
  OC_INDEX GetBoxCount() const { return Nx*Ny; }
  OC_INDEX GetSpaceWaste() const;

  void AddPoint(const Nb_LocatedVector<OC_REAL8> &lv);
  /// Vf_BoxList forms an internal list (wholelist) of each
  ///  Nb_LocatedVector<OC_REAL8> passed in, since the last call to
  ///  ::ClearList().  Pointers returned ::GetClosest point
  ///  into this list.  Also the Nb_List<Vf_BoxElt *> lists in each
  ///  box[] point into this same list.
  /// NOTE: AddPoint automatically enlarges the box region as
  ///  necessary to include the point lv.  The client should
  ///  call SetBoxRegion after all calls to AddPoint if it
  ///  desires different behavior.

  void GetBoxRegion(OC_REAL8 &_xmin,OC_REAL8 &_ymin,
                    OC_REAL8 &_xmax,OC_REAL8 &_ymax) const
  {
    _xmin=xmin; _xmax=xmax; _ymin=ymin; _ymax=ymax;
  }
  void AddMarginBoxRegion(OC_REAL8 xmargin,OC_REAL8 ymargin) {
    xmin-=xmargin; xmax+=xmargin; ymin-=ymargin; ymax+=ymargin;
    box_valid=0;
  }
  void InflateBoxRegion(OC_REAL8 xscale,OC_REAL8 yscale) {
    OC_REAL8 xoldsize=xmax-xmin;
    OC_REAL8 xnewsize=xoldsize*xscale;
    OC_REAL8 yoldsize=ymax-ymin;
    OC_REAL8 ynewsize=yoldsize*yscale;
    AddMarginBoxRegion(OC_REAL8((xnewsize-xoldsize)/2.),
		       OC_REAL8((ynewsize-yoldsize)/2.));
  }
  void SetBoxRegion(OC_REAL8 _xmin,OC_REAL8 _ymin,
                    OC_REAL8 _xmax,OC_REAL8 _ymax) {
    xmin=_xmin; xmax=_xmax; ymin=_ymin; ymax=_ymax;
    box_valid=0;
  }
  void ExpandBoxRegion(OC_REAL8 _xmin,OC_REAL8 _ymin,
                       OC_REAL8 _xmax,OC_REAL8 _ymax) {
    if(_xmin<xmin) xmin=_xmin;
    if(_xmax>xmax) xmax=_xmax;
    if(_ymin<ymin) ymin=_ymin;
    if(_ymax>ymax) ymax=_ymax;
    box_valid=0;
  }

  void DeleteRefinement();
  /// Pitches any existing *box array, and marks box_valid=1.

  OC_INDEX Refine(OC_INDEX refinelevel=VF_BOX_REFINELEVEL);
  ///  If box_valid=1, then refines existing *box structure refinelevel
  /// times.  This will usually increase the number of boxes in *box by
  /// a factor of 4**refinelevel.
  ///  If box_valid=0, then ::Refine first throws away any existing *box
  /// array, and issues a warning message (for DEBUGLVL sufficiently
  /// large) to aid program efficiency development.
  ///  In all cases, box_valid is set to 1 upon exit.
  /// The return value is the total number of boxes Nx*Ny, which with
  /// luck will fit inside an OC_INDEX.  Overflow of a 4-byte index
  /// will likely occur at refinelevel of about 16, at which point you
  /// are probably out of memory; if not, everything will break down
  /// as the indexing into box[] fails.

  OC_INDEX Refine(OC_INDEX boxcount,
                  OC_REAL8m ave_incount,OC_REAL8m ave_listcount);
  /// An adaptive interface to the RefineOnce routine.  Does successive
  /// refinements until one of the following conditions is met:
  ///        number of boxes (=Nx*Ny) >= import boxcount 
  ///   or        average box incount <  import ave_incount
  ///   or      average box listcount <  import ave_listcount
  /// Note that each refinement step increases the number of boxes
  /// by a factor or 2 or 4, so the number of boxes may be up to
  /// 4 times larger than the import boxcount on return.  This can
  /// be reduced to a factor of 2 by judicious choice of boxcount.
  /// Example: If boxcount<=1024, then your are guaranteed of getting
  /// at most 2048 boxes, but if boxcount=1025, then you may get as
  /// many as 4096.
  ///   You may turn off any of these checks by setting the
  /// corresponding import to 0.  Setting all 3 to 0 raises an error.
  ///   The return value is Nx*Ny.

  OC_INT4m ClearSelectCounts();
  OC_INT4m GetClosest2D(const Nb_Vec3<OC_REAL8> &pos,
		     Nb_LocatedVector<OC_REAL8>* &lv,OC_INT4m &select_count);
  /// Given position pos, returns in lv the element of the wholelist
  /// that is closest to pos, ignoring the z component.  The value of
  /// selected_count is the number of times this element has been
  /// selected (via calls to GetClosest2D) since the last call to
  /// ::ClearSelectCounts (which is also effectively called at Vf_BoxList
  /// instantiation time), including this one.  In particular, each
  /// call to this function will increment the select_count field of
  /// one Vf_BoxElt, and the parameter select_count will always return >=1.
  ///  NOTE: If GetClosest2D is called at a time when box_valid is
  ///        false, then ::Refine() will be called, using the default
  ///        VF_BOX_REFINELEVEL value.  A warning message will be raised
  ///        in this case if DEBUGLVL is large enough.  Client routines
  ///        are encouraged to explicitly call ::Refine() as necessary.
  ///        The ::Refine() call is time consuming, so it is best if the
  ///        programmer is fully aware of when it is called.

  // Routines to access entire ::wholelist. Increments select_count
  // field of each entry of ::wholelist by one.  Returns 0 on success,
  // 1 on end-of-list or other error.
  void ResetWholeAccess();
  OC_INT4m GetWholeNext(Nb_LocatedVector<OC_REAL8>& lv);

  // Key-based index into wholelist.  These routines do not adjust
  // select count.
  const Nb_LocatedVector<OC_REAL8>* GetFirst(Vf_BoxList_Index& key) const;
  const Nb_LocatedVector<OC_REAL8>* GetNext(Vf_BoxList_Index& key) const;
  OC_INT4m SetValue(const Vf_BoxList_Index& key,
                    const Nb_Vec3<OC_REAL8>& value);
};

// USUAL Vf_BoxList USAGE:
//     // Initialize boxlist
//     Vf_BoxList boxlist;
//     LOOP1:
//       < Generate Nb_LocatedVector<OC_REAL8> lv >
//       boxlist.AddPoint(lv);
//       < More points?  Yes -> goto LOOP1 >
//     boxlist.InflateBoxRegion(1.2,1.2);   // Add a 10% margin on all sides
//     Refine();
//     ...
//     // Use boxlist to generate a sub-sampled list of points
//     Nb_List<Nb_LocatedVector<OC_REAL8> *> sublist;
//     Nb_LocatedVector<OC_REAL8> *lvp;
//     boxlist.ClearSelectCounts();  // Not required for first access
//     LOOP2:
//       < Generate sample point Nb_Vec3<OC_REAL8> pos >
//       boxlist.GetClosest2D(pos,lvp,count);
//       if(count==1) sublist.Append(lvp);
//       < More sample points?  Yes -> goto LOOP2 >
//     < Use sublist as desired >
//     ...
//     < When sublist & boxlist are _both_ not needed, the space
//      in boxlist can be freed via boxlist.ClearList().         >
//

#endif // _VF_PTSEARCH
