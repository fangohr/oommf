/* FILE: ptsearch.cc                 -*-Mode: c++-*-
 *
 * Classes for searching unordered lists of points in space.
 *
 * Last modified on: $Date: 2013/05/22 07:15:38 $
 * Last modified by: $Author: donahue $
 */

////////////////////////////////////////////////////////////////////////
//  These classes/routines are modification of those in magdat2out.cc,
// which is some code I (mjd) wrote up in Aug/Sep 1996 to extract point
// subsets out of the 1st muMag standard problem submissions, for which
// the points are not required to lie on regular grids.
//  This current code has several flaws (inherited from its predecessor),
// chiefly:
//        1) This is a 2D code;  Although 3D position vectors are
//            required, the z component is ignored.
//        2) The subbox refinement process is fixed a priori, i.e., it is
//            not data driven.  (On refinement the box dimensions are
//            exactly halved;  typically both x and y extents are divided
//            at each step, though if one is substantially longer than
//            the other then only the longer extent will be cut.)
//        3) Does not handle closest point searching for points outside
//            the originally designated region.

#include <cstring>
#include "ptsearch.h"

static const OC_REAL8 Vf_BoxEps=1e-14;
/// Box overlap to allow for roundoff errors

//////////////////////////////////////////////////////////////////////////
// class Vf_Box: Atomic LocatedVector* list element.
const ClassDoc Vf_Box::class_doc("Vf_Box",
		      "Michael J. Donahue (michael.donahue@nist.gov)",
		      "1.0.0","5-Sep-1997");

OC_BOOL Vf_Box::IsIn2D(OC_REAL8 x,OC_REAL8 y) const
{
  // True if (x,y) is inside box
  if(x<xmin || x>xmax || y<ymin || y>ymax) return 0;
  return 1;
}
OC_BOOL Vf_Box::IsIn2D(const Nb_Vec3<OC_REAL8> &pos) const
{
  // True if pos is inside box
  return IsIn2D(pos.x,pos.y);
}

OC_REAL8m Vf_Box::GetBoxArea() const
{
  if(xmax<=xmin || ymax<=ymin) return (OC_REAL8m)0.;
  return (xmax-xmin)*(ymax-ymin);
}

OC_INDEX Vf_Box::GetSpaceWaste() const
{
  if(list==NULL) return 0;
  return list->GetSpaceWaste();
}

void Vf_Box::FillList(Nb_List<Vf_BoxElt *> *biglist)
{ // Fills "list" with a refined version of biglist using both Sup & L2
  // norms; also sets incount.
  //
  // Description of algorithm:
  //    First we find, by exhaustive search, the point Q in biglist that
  // is closest to the box center C.  We then set "slack" to the maximum
  // of the distances from Q to the box corners.  (This is a refinement
  // on the parent code in magdat2out, which sets "slack" to ||Q-C||_2+
  // ||C-box_rad||_2, where box_rad is the L2 distance from C to the box
  // corners.)
  //    Now, by the convexity of the box, we know that any point inside
  // the box is no farther than "slack" from Q wrt L2 norm.  The second
  // stage of the algorithm is to weed out of biglist any point which is
  // farther than "slack" from _all_ points in the box.  This is done in
  // two steps.  The first step thins out any point which is farther
  // than "slack" from box wrt to Sup norm.  The Sup norm bounding
  // region is easy to calculation (just form an enclosing box with a
  // margin on all sides of width "slack"), and majorizes the L2 norm.
  //    Any points that pass the Sup norm check are then compared against
  // an L2 norm check.  The L2 bounding region is the same as the Sup
  // norm bounding region, but with rounded corners.  To test this in a
  // tight fashion, one would have to first determine if a candidate
  // point P was in a corner region, and if so then calculate its
  // distance from the associated corner.  Rather than this, the current
  // code makes a weaker test: it tests to see if candidate P is farther
  // from C than slack+box_rad (wrt L2).  If the box aspect is far from
  // square, then the latter test is a poor bound on the proper test,
  // but in this case the corner "rounded area" is small wrt to the
  // total slack region, so we probably don't include too many extra
  // points in this case.  For a square box with "slack" large, this
  // weaker L2 test is nearly identical with the precise test.  (Another
  // point: the weaker test can reuse the distance calculations done to
  // find the closest point from biglist to C.)
  //    In the end, this gives a refined list that includes all points
  // from biglist that are closer to _some_ point inside the box than
  // any other point from biglist, and also a few additional not needed
  // points.  If one wants to improve the refinement, one could tighten
  // the L2 check described in the last paragraph, but that probably
  // won't buy much.  A better improvement can probably be had by
  // replacing the single pair distance Q-C above with a point
  // collection. For example, one could find the minimum distance from
  // the points of biglist to the 5 point collection of C + box corners.
  // Then set "slack" equal to this distance + effective_rad, where
  // effective_rad is the maximum distance from any point inside the box
  // to the 5 point collection.  This distance will be > box_rad/2 and
  // <= box_rad/sqrt(2) (box_rad as above), the upper limit being
  // attained iff the box is square.  An alternative to this 5 point
  // comparison would be to introduce 9 points: C, the 4 corners, and
  // the midpoints of the 4 sides.  This seems to have the flavor of
  // being halfway to the next refinement.
  //                                                    -mjd, 7-Sep-1997
#define MEMBERNAME "FillList"
  incount=0;
  if(list==NULL)
    FatalError(-1,STDDOC,"::list member variable not initialized");
  list->Clear();

  OC_INDEX isize = biglist->GetSize();
  if(isize<1) {
    NonFatalError(STDDOC,"Empty biglist import");
    return;
  }
  struct Vf_BoxEltWork { Vf_BoxElt* bep; OC_REAL8 distsq; };
  Vf_BoxEltWork *bed=new Vf_BoxEltWork[size_t(isize)];
  /// Performance note: If there are many box list refinements, then
  /// this memory allocation gets done a lot.  We might want to move
  /// it into static storage, or pass workspace on the parameter list.

  // Find point Q from biglist closest to box center C.
  OC_INDEX i,qi=0;  OC_REAL8 temp_distsq,qdistsq=(OC_REAL8)0.0;
  Nb_Vec3<OC_REAL8> box_center(OC_REAL8((xmin+xmax)/2.),
			    OC_REAL8((ymin+ymax)/2.),OC_REAL8(0.));
  biglist->RestartGet();
  for(i=0;i<isize;i++) {
    bed[i].bep = *(biglist->GetNext());
    temp_distsq=bed[i].bep->DistSq2D(box_center);
    bed[i].distsq=temp_distsq;
    if(i==0 || temp_distsq<qdistsq) {
      qi=i; qdistsq=temp_distsq;
    }
  }

  // Set slack variables;  See discussion in function preamble.
  OC_REAL8 xdelta=xmax-xmin;      OC_REAL8 ydelta=ymax-ymin;
  OC_REAL8 box_rad=OC_REAL8(sqrt(xdelta*xdelta+ydelta*ydelta)/4.);

  OC_REAL8 qx=bed[qi].bep->lv.location.x;
  OC_REAL8 qy=bed[qi].bep->lv.location.y;
  OC_REAL8 xtemp=OC_REAL8(fabs(qx-box_center.x)+xdelta);
  OC_REAL8 ytemp=OC_REAL8(fabs(qy-box_center.y)+ydelta);
  OC_REAL8 slack=OC_REAL8(sqrt(xtemp*xtemp+ytemp*ytemp));

  // Sup norm bounding region
  OC_REAL8 sup_xmin=xmin-slack;   OC_REAL8 sup_xmax=xmax+slack;
  OC_REAL8 sup_ymin=ymin-slack;   OC_REAL8 sup_ymax=ymax+slack;

  // L2 norm bounding radius
  OC_REAL8 l2_rad=slack+box_rad;  OC_REAL8 l2_radsq=l2_rad*l2_rad;

  // Biglist weeding
  for(i=0;i<isize;i++) {
    // First do Sup norm check
    OC_REAL8 px=bed[i].bep->lv.location.x;
    if(px<sup_xmin || px>sup_xmax) continue;
    OC_REAL8 py=bed[i].bep->lv.location.y;
    if(py<sup_ymin || py>sup_ymax) continue;

    // If the above pass, then do L2 check
    if(bed[i].distsq>l2_radsq) continue;

    // Include point in refined list
    list->Append(bed[i].bep);

    // Increment incount if element is actually inside box extents.
    // Since any such point is automatically in the refined list,
    // we could make this the first check; But I would guess that
    // the majority of points from biglist will be outside the box
    // extents, so the present test ordering is probably faster.
    if(IsIn2D(px,py)) incount++;
  }

  delete[] bed;
#undef  MEMBERNAME
}

void Vf_Box::InitNoRefine(OC_REAL8 _xmin,OC_REAL8 _ymin,OC_REAL8 _xmax,OC_REAL8 _ymax,
		       Nb_List<Vf_BoxElt> *wholelist)
{
#define MEMBERNAME "InitNoCheck"
  if(_xmin>_xmax || _ymin>_ymax) {
    FatalError(-1,STDDOC,"Illegal box corner input: (%f,%f) not <= (%f,%f)",
	       _xmin,_ymin,_xmax,_ymax);
  }
  xmin=_xmin; ymin=_ymin; xmax=_xmax; ymax=_ymax;

  if(list!=NULL) delete list;
  list=new Nb_List<Vf_BoxElt *>(wholelist->GetSize());

  incount=0;
  for(Vf_BoxElt* bep=wholelist->GetFirst(); bep!=NULL;
      bep=wholelist->GetNext()) {
    list->Append(bep);
    if(IsIn2D(bep->lv.location)) incount++;
  }
#undef  MEMBERNAME
}

void Vf_Box::Init(OC_REAL8 _xmin,OC_REAL8 _ymin,OC_REAL8 _xmax,OC_REAL8 _ymax,
	       Vf_Box &bigbox)
{ // Called by Vf_BoxList::Refine(OC_INDEX), this routine first clears existing
  // Nb_List<Vf_BoxElt *> list, if any, and then copies those elements of
  // Nb_List<Vf_BoxElt *> biglist that could be "closest 2D position" to
  // current box, as defined by above box extents.
#define MEMBERNAME "Init"
  if(_xmin>_xmax || _ymin>_ymax) {
    FatalError(-1,STDDOC,"Illegal box corner input: (%f,%f) not <= (%f,%f)",
	       _xmin,_ymin,_xmax,_ymax);
  }
  xmin=_xmin; ymin=_ymin; xmax=_xmax; ymax=_ymax;

  OC_REAL8m this_area=GetBoxArea();
  OC_REAL8m big_area=bigbox.GetBoxArea();

  //    Initialize new list with appropriate allocation block size.
  // The way this routine is typically used (quartering an existing
  // box), it seems that we usually keep about 0.3 of the elts from
  // "biglist".  There are competing criteria here: 1) We don't want
  // too small a block size, because then the Nb_List class will be
  // doing too many memory allocs, 2) We want the data to fit tightly
  // in an integral number of allocated blocks, so we don't waste memory
  // (there are likely to be >1000 of Vf_Box's in a typical refined
  // Vf_BoxList class).
  //    THEORY: Find the ratio of the new box area to the big box area,
  // and multiply by .3/.25=1.2.  Multiply this value against the size
  // of big list, and round up to the first value that divides evenly
  // into big list size.  Finally, round up to the first integer not
  // smaller than this.
  //    Example:  Suppose the big list has 127 elts, and is 4 times the
  // area of *this.  So we take (1/4)*1.2 = .3; .3 * 127 = 38.1
  // 127/38.1 = 1/.3 (of course), = 3.33 which we round down to 3, i.e.,
  // we want 1/3 of 127 = 42.3, which we then round to 43.  Then, worst
  // case we need 3 memory allocs (since we know the length of the new
  // list is not more than the length of biglist), and we waste at most
  // 42 cells.  The smallest new list size we can reasonably hope for
  // is 127/4=32, and on average we expect 32*1.2=38
  //    PRACTICE: Playing around with a handful of test cases, a
  // reasonable compromise between minimizing number of alloc calls
  // and minimizing wasted space, appears to be given by replacing
  // the 1.2 above with 1.0, and then dividing the resulting block
  // size by 2.  I would guess using this that most boxes will require
  // either 2 or 3 such sized blocks.   -mjd, 25-Sep-1997
  OC_REAL8m length_frac=OC_REAL8m(1.0);
  if(this_area>0 && big_area>this_area) {
    length_frac=OC_MAX(OC_REAL8m(1.),OC_REAL8m(floor(big_area/(1.0*this_area))));
    /// Note: One can try replace 1.0 in the last line with 1.2
    ///   (or similar), as suggested in the THEORY paragraph above.
  }
  OC_INDEX this_length
    = OC_MAX(1,OC_INDEX(ceil(bigbox.ListCount()/length_frac)));
  this_length=OC_MAX(1,(this_length+1)/2);
  /// This halving of the block size is optional if one exaggerates
  /// the base length by enough to include _most_ lists in 1 block.

  if(list!=NULL) delete list;
  list=new Nb_List<Vf_BoxElt *>(this_length);

  // Finally fill this.list as a refinement of bigbox.list
  FillList(bigbox.list);

#undef  MEMBERNAME
}

Vf_BoxElt* Vf_Box::GetClosest2D(const Nb_Vec3<OC_REAL8> &pos)
{ // Note: Non-const function because calls to list.GetFirst() and
  //       list.GetNext() change list's internal pointer.
#define MEMBERNAME "GetClosest2D"
  if(!IsIn2D(pos)) {
    FatalError(-1,STDDOC,"Point request (%f,%f,%f) outside box"
	       " (%f,%f,*)x(%f,%f,*)",pos.x,pos.y,pos.z,
	       xmin,ymin,xmax,ymax);
  }
  Vf_BoxElt *bep,*bep_best,**bepp;
  OC_REAL8 distsq,distsq_best;
  bepp=list->GetFirst();
  if(bepp==NULL) {
    FatalError(-1,STDDOC,"Programming error: box %p, with extents"
	       " (%f,%f,*)x(%f,%f,*), has an empty Vf_Box* list",
	       this,xmin,ymin,xmax,ymax);
  }
  bep_best= *bepp; // Copy pointer
  distsq_best=bep_best->DistSq2D(pos);
  while((bepp=list->GetNext())!=NULL) {
    bep= *bepp;  // Copy pointer
    distsq=bep->DistSq2D(pos);
    if(distsq<distsq_best) {
      distsq_best=distsq; bep_best=bep;
    }
  }
  return bep_best;
#undef  MEMBERNAME
}


//////////////////////////////////////////////////////////////////////////
// class Vf_BoxList: Client interface to a list of Vf_BoxElt, Vf_Box* box,
// which is a list of lists of LocatedVector*'s.  Also holds
//              Nb_List<Vf_BoxElt> wholelist
// which is the list of LocatedVector's pointed into by entries of Vf_Box* box.
const ClassDoc Vf_BoxList::class_doc("Vf_BoxList",
		      "Michael J. Donahue (michael.donahue@nist.gov)",
		      "1.0.0","5-Sep-1997");

void Vf_BoxList::MakeBoxValid() {
#define MEMBERNAME "MakeBoxValid"
  if(box_valid) return;
  Refine();
#undef MEMBERNAME
}

void Vf_BoxList::InitBox()
{
#define MEMBERNAME "InitBox"
  if(box!=NULL) FatalError(-1,STDDOC,"*box structure already in place");
  box=new Vf_Box[1];
  Nx=Ny=1;
  xdelta=xmax-xmin;
  ydelta=ymax-ymin;
  box[0].InitNoRefine(xmin,ymin,xmax,ymax,wholelist);
  avelistcount=OC_REAL8(box[0].ListCount());
  aveincount=OC_REAL8(box[0].InCount());
  box_valid=1;
#undef MEMBERNAME
}

void Vf_BoxList::ClearList() {
#define MEMBERNAME "ClearList"
  xmin=ymin=xmax=ymax=OC_REAL8(0.0);
  Nx=Ny=0;
  xdelta=ydelta=OC_REAL8(0.0);
  if(box!=NULL) { delete[] box; box=NULL; }
  box_valid=1;
  wholelist->Free();
  aveincount=avelistcount=OC_REAL8(0.0);
#undef MEMBERNAME
}

Vf_BoxList::Vf_BoxList() {
#define MEMBERNAME "Vf_BoxList"
  box=NULL;
  wholelist=new Nb_List<Vf_BoxElt>;
  ClearList();
#undef MEMBERNAME
}

Vf_BoxList::~Vf_BoxList() {
#define MEMBERNAME "~Vf_BoxList"
  ClearList();
  if(wholelist!=NULL) delete wholelist;
#undef MEMBERNAME
}

void Vf_BoxList::AddPoint(const Nb_LocatedVector<OC_REAL8> &lv)
{
#define MEMBERNAME "AddPoint"
#undef MEMBERNAME
  box_valid=0;  // Any existing refinement is out-of-date

  // Update bounding box
  if(GetSize()==0) {
    xmin=xmax=lv.location.x;
    ymin=ymax=lv.location.y;
  }
  else {
    if(lv.location.x<xmin) xmin=lv.location.x;
    if(lv.location.x>xmax) xmax=lv.location.x;
    if(lv.location.y<ymin) ymin=lv.location.y;
    if(lv.location.y>ymax) ymax=lv.location.y;
  }

  // Add point, with initial select_count=0.
  wholelist->Append(Vf_BoxElt(lv));
#undef MEMBERNAME
}


void Vf_BoxList::RefineOnce()
{
#define MEMBERNAME "RefineOnce"
{ // Refines list of boxes by halving cell dimension(s)
  OC_INDEX newNx=Nx,newNy=Ny,bxstep=1,bystep=1;
  OC_REAL8 newxdelta=xdelta,newydelta=ydelta;
  if(xdelta>1.5*ydelta) { // Cut x dimension
    newNx*=2; newxdelta/=2; bxstep=2;
  }
  else if(ydelta>1.5*xdelta) { // Cut y dimension
    newNy*=2; newydelta/=2; bystep=2;
  }
  else { // Cut both x and y
    newNx*=2; newxdelta/=2; bxstep=2;
    newNy*=2; newydelta/=2; bystep=2;
  }
  // NOTE: For large Nx & Ny (bigger than maybe 256?), it is probably
  //       better to always cut only one (either x *or* y) dimension,
  //       but my tests with smaller Nx and Ny built faster when both
  //       xdelta and ydelta were cut (provided xdelta \approx ydelta).
  //
  // NOTE 2: It is important to not call this routine too many times,
  //       because the length of the box array grows exponentially.

  Vf_Box *newbox,*boxptr,*newboxptr;
  newbox = new Vf_Box[size_t(newNx)*size_t(newNy)];

  // Initialize new boxes
  OC_INDEX intotal=0,listtotal=0; // Tmps storing box incounts and listcounts
  OC_INDEX i,j;
  OC_REAL8 x1,x2,y1,y2;
  x1=xmin; boxptr= &(box[0]); newboxptr= &(newbox[0]);
  for(i=0;i<newNx;i++) {
    x2=xmin+(i+1)*newxdelta; // Minimize roundoff error
    y1=ymin;
    for(j=0;j<newNy;j++) {
      y2=ymin+(j+1)*newydelta; // Minimize roundoff error
      // Include a small overlap to allow for roundoff error.
      newboxptr->Init(x1-Vf_BoxEps,y1-Vf_BoxEps,
                      x2+Vf_BoxEps,y2+Vf_BoxEps,*boxptr);
      intotal+=newboxptr->InCount();
      listtotal+=newboxptr->ListCount();
      y1=y2;
      newboxptr++;
      if(bystep==1 || j%2==1) boxptr++; // *ASSUMES* bystep is 1 or 2, only
    }
    x1=x2;
    if(bxstep!=1 && i%2==0) // Repeat boxptr on last column; bxstep=1 or 2 only
      boxptr=box+(i/2)*Ny;
  }

  // Release old box list, and save newbox as box
  delete[] box;
  box=newbox;
  Nx=newNx; Ny=newNy;
  xdelta=newxdelta; ydelta=newydelta;
  avelistcount=OC_REAL8(listtotal)/OC_REAL8(Nx*Ny);
  aveincount=OC_REAL8(intotal)/OC_REAL8(Nx*Ny);
  /// For distinct boxes, this value is computable; But there is a small
  /// chance that an input point falls on the exact bdry of two boxes,
  /// and so gets included in both boxes.
  /// NOTE: The ave* values will be wrong if Nx*Ny overflows an OC_INDEX.

  box_valid=1;
}

#undef MEMBERNAME
}

void Vf_BoxList::DeleteRefinement()
{
#define MEMBERNAME "DeleteRefinement"
  if(box!=NULL) {
    delete[] box;
    box=NULL;
  }
  Nx=Ny=0;
  xdelta=ydelta=OC_REAL8(0.0);
  box_valid=0;
#undef MEMBERNAME
}


OC_INDEX Vf_BoxList::Refine(OC_INDEX refinelevel)
{
#define MEMBERNAME "Refine(OC_INDEX)"
  if(!box_valid && box!=NULL) {
    // Current refinement is invalid, so we pitch any existing refinement.
#if DEBUGLVL>9
    NonFatalError(STDDOC,"Automatic Vf_Box *box structure deletion");
#endif
    DeleteRefinement();
  }
  if(box==NULL) InitBox();  // Start from scratch!
  while( refinelevel-- > 0 ) RefineOnce();
  return Nx*Ny;
#undef MEMBERNAME
}

OC_INDEX Vf_BoxList::Refine(OC_INDEX boxcount,OC_REAL8m ave_incount,
                            OC_REAL8m ave_listcount)
{ // An adaptive interface to the RefineOnce routine.  Does successive
  // refinements until one of the following conditions is met:
  //        number of boxes (=Nx*Ny) >= import boxcount 
  //   or        average box incount <  import ave_incount
  //   or      average box listcount <  import ave_listcount
  // Note that each refinement step increases the number of boxes
  // by a factor or 2 or 4, so the number of boxes may be up to
  // 4 times larger than the import boxcount on return.  This can
  // be reduced to a factor of 2 by judicious choice of boxcount.
  // Example: If boxcount<=1024, then your are guaranteed of getting
  // at most 2048 boxes, but if boxcount=1025, then you may get as
  // many as 4096.
  //   You may turn off any of these checks by setting the
  // corresponding import to 0.  Setting all 3 to 0 raises an error.
  //   The return value is final boxcount (=Nx*Ny).
#define MEMBERNAME "Refine(OC_INDEX,OC_REAL8m,OC_REAL8m)"
  if(boxcount<=0 && ave_incount<=0. && ave_listcount) {
    FatalError(-1,STDDOC,
	       "At least one termination check must be activated");
  }
  if(!box_valid && box!=NULL) {
    // Current refinement is invalid, so we pitch any existing refinement.
#if DEBUGLVL>9
    NonFatalError(STDDOC,"Automatic Vf_Box *box structure deletion");
#endif
    DeleteRefinement();
  }
  if(box==NULL) InitBox();  // Start from scratch!
  while(1) {
    if(boxcount>0 && (GetBoxCount()>=boxcount)) break;
    if(GetAveInCount()<ave_incount)             break;
    if(GetAveListCount()<ave_listcount)         break;
    RefineOnce();
  }
  return GetBoxCount();
#undef MEMBERNAME
}

OC_INT4m Vf_BoxList::ClearSelectCounts()
{
#define MEMBERNAME "ClearSelectCounts"
  for(Vf_BoxElt *bep=wholelist->GetFirst(); bep!=NULL;
      bep=wholelist->GetNext()) {
    bep->select_count=0;
  }
  return 0;
#undef MEMBERNAME
}

OC_INT4m Vf_BoxList::GetClosest2D(const Nb_Vec3<OC_REAL8> &pos,
			    Nb_LocatedVector<OC_REAL8>* &lv,
			    OC_INT4m &select_count)
{
#define MEMBERNAME "GetClosest2D"
  if(GetSize()<1) {
    // We could make this non-fatal, and return an error,
    // but the way this is currently (6/2001) used, this
    // shouldn't happen at all.
    FatalError(-1,STDDOC,"Point request (%f,%f,%f) made to empty box",
	       pos.x,pos.y,pos.z);
  }
  OC_INDEX i,j;
  i=(OC_INDEX)floor((pos.x-xmin)/xdelta);
  j=(OC_INDEX)floor((pos.y-ymin)/ydelta);
  if(i<0 || i>=Nx || j<0 || j>=Ny) {
    // Cf. note to preceding error message.
    FatalError(-1,STDDOC,"Input point outsize refinement boundary:\n"
	       " Pt: (%f,%f), Box extents: (%f,%f) to (%f,%f)",
	       pos.x,pos.y,xmin,ymin,xmax,ymax);
  }
  Vf_BoxElt *bep=box[i*Ny+j].GetClosest2D(pos);
  lv=&(bep->lv);
  select_count= ++(bep->select_count);
  return 0;
#undef MEMBERNAME
}

void Vf_BoxList::ResetWholeAccess()
{
  wholelist->RestartGet();
}

OC_INT4m Vf_BoxList::GetWholeNext(Nb_LocatedVector<OC_REAL8>& lv)
{
  Vf_BoxElt* bep=wholelist->GetNext();
  if(bep==NULL) return 1;
  lv=bep->lv;
  (bep->select_count)++;
  return 0;
}

// Key-based index into wholelist.  These routines do not adjust
// select count.
const Nb_LocatedVector<OC_REAL8>*
Vf_BoxList::GetFirst(Vf_BoxList_Index& index) const
{
  Vf_BoxElt* bep=wholelist->GetFirst(index.key);
  if(bep==NULL) return NULL;
  return &(bep->lv);
}

const Nb_LocatedVector<OC_REAL8>*
Vf_BoxList::GetNext(Vf_BoxList_Index& index) const
{
  Vf_BoxElt* bep=wholelist->GetNext(index.key);
  if(bep==NULL) return NULL;
  return &(bep->lv);
}

OC_INT4m
Vf_BoxList::SetValue(const Vf_BoxList_Index& index,
		     const Nb_Vec3<OC_REAL8>& value)
{
  Vf_BoxElt be;
  if(wholelist->GetElt(index.key,be)!=0) return 1; // Error
  be.lv.value = value;
  return wholelist->ReplaceElt(index.key,be);
}


OC_INDEX Vf_BoxList::GetSpaceWaste() const
{
  OC_INDEX wasted_space=0;
  if(wholelist!=NULL) {
    wasted_space+=wholelist->GetSpaceWaste();
  }
  for(OC_INDEX i=0;i<Nx*Ny;i++) wasted_space+=box[i].GetSpaceWaste();
  return wasted_space;
}
