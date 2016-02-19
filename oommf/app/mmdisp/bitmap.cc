/* FILE: bitmap.cc                 -*-Mode: c++-*-
 *
 * OommfBitmap C++ class code
 *
 */

#include "bitmap.h"

/* End includes */     // This is an optional directive to build.tcl,
                       // that excludes the remainder of the file from
                       // '#include ".*"' dependency building

#ifndef NDEBUG
#define CODECHECKS
#endif

//////////////////////////////////////////////////////////////////////////
// Drawing parameters and OommfPolygon helper functions
static OC_REAL8m ArrowLength(0.90);
static OC_REAL8m BitmapLineWidth(0.08);
static OC_REAL8m BitmapArrowHeadRatio(0.5);
static OC_REAL8m InOutTipRadius(0.075);
static OC_REAL8m DotRadius(0.025);
static OC_REAL8m CrossLineWidth(0.025);
static OC_REAL8m MinCrossDisplaySize(22.);

static void MakeStandardDiamond(OommfPolygon& diamond)
{
  diamond.Clear();
  OC_REAL8m radius=InOutTipRadius*sqrt(2.);
  diamond.AddVertex(PlanePoint(-radius,0.));
  diamond.AddVertex(PlanePoint(0.,-radius));
  diamond.AddVertex(PlanePoint(radius,0.));
  diamond.AddVertex(PlanePoint(0.,radius));
}

static void MakeStandardCross(OommfPolygon& cross)
{
  cross.Clear();
  OC_REAL8m inner=CrossLineWidth/2.;
  OC_REAL8m outer=InOutTipRadius*sqrt(2.)-inner;
  cross.AddVertex(PlanePoint(outer,inner));
  cross.AddVertex(PlanePoint(inner,inner));
  cross.AddVertex(PlanePoint(inner,outer));
  cross.AddVertex(PlanePoint(-inner,outer));
  cross.AddVertex(PlanePoint(-inner,inner));
  cross.AddVertex(PlanePoint(-outer,inner));
  cross.AddVertex(PlanePoint(-outer,-inner));
  cross.AddVertex(PlanePoint(-inner,-inner));
  cross.AddVertex(PlanePoint(-inner,-outer));
  cross.AddVertex(PlanePoint(inner,-outer));
  cross.AddVertex(PlanePoint(inner,-inner));
  cross.AddVertex(PlanePoint(outer,-inner));
}

static void MakeStandardArrow(OommfPolygon& arrow)
{
  arrow.Clear();

  arrow.AddVertex(PlanePoint(-ArrowLength/2.,BitmapLineWidth/2.));
  arrow.AddVertex(PlanePoint(ArrowLength/2.-BitmapArrowHeadRatio*0.8,
                             BitmapLineWidth/2.));
  arrow.AddVertex(PlanePoint(ArrowLength/2.-BitmapArrowHeadRatio,
                             BitmapArrowHeadRatio*0.4));

  arrow.AddVertex(PlanePoint(ArrowLength/2.,BitmapLineWidth/4.));
  arrow.AddVertex(PlanePoint(ArrowLength/2.,-BitmapLineWidth/4.));

  arrow.AddVertex(PlanePoint(ArrowLength/2.-BitmapArrowHeadRatio,
                             -BitmapArrowHeadRatio*0.4));
  arrow.AddVertex(PlanePoint(ArrowLength/2.-BitmapArrowHeadRatio*0.8,
                             -BitmapLineWidth/2.));
  arrow.AddVertex(PlanePoint(-ArrowLength/2.,-BitmapLineWidth/2.));

}

//////////////////////////////////////////////////////////////////////////
// OOMMF Polygon class
const ClassDoc OommfPolygon::class_doc("OommfPolygon",
		    "Michael J. Donahue (michael.donahue@nist.gov)",
		    "1.0.0","31-Oct-1998");

OommfPolygon::OommfPolygon(const OommfPolygon& poly)
{
  vertex=poly.vertex;
}

OommfPolygon& OommfPolygon::operator=(OommfPolygon& poly)
{
#define MEMBERNAME "operator=(OommfPolygon&)"
  vertex=poly.vertex;
  return *this;
#undef MEMBERNAME
}

void OommfPolygon::Clear()
{
  vertex.Clear();  // Note: Does not release memory
}

OC_INDEX OommfPolygon::GetVertexCount()
{
  return vertex.GetSize();
}

void OommfPolygon::AddVertex(const PlanePoint& pt)
{ // Adds a new vertex to polygon
  vertex.Append(pt);
}

void OommfPolygon::GetBounds(OC_REAL8m& xmin,OC_REAL8m& ymin,
               OC_REAL8m& xmax,OC_REAL8m& ymax) const
{ // Returns 0,0,0,0 if polygon is empty
  Nb_List_Index<PlanePoint> key;
  const PlanePoint* pt=vertex.GetFirst(key);
  if(pt==NULL) {
    xmin=ymin=xmax=ymax=0.0;
    return;
  }
  xmin=xmax=pt->x;
  ymin=ymax=pt->y;
  while((pt=vertex.GetNext(key))!=NULL) {
    OC_REAL8m x=pt->x;
    if(xmin>x) xmin=x;
    if(xmax<x) xmax=x;
    OC_REAL8m y=pt->y;
    if(ymin>y) ymin=y;
    if(ymax<y) ymax=y;
  }
}

// CrossSegment returns 1 if the ray from point "Q" in
// direction (1,eps) (where eps is an arbitrarily small positive
// value) crosses the line segment between points P1 and P2.
// The return value is -1 if the distance from "Q" to the line
// segment is approx < OC_REAL4_EPSILON.  Otherwise the return value
// is 0. See page 11-12 (7-Sept-95) of MJD's micromagnetic NIST lab
// notebook.  This routine is used by OommfPolygon::IsInside().
// (Note: The distance comparison is only "approx < OC_REAL4_EPSILON",
// because just to simplify the code the distance is taken in some
// cases wrt the L_\infty metric.  This could be cleaned up if
// necessary.)
static int CrossSegment(const PlanePoint& Q,
                        const PlanePoint& P1,const PlanePoint& P2)
{
  OC_REAL8 delta;  // Discriminant for 2x2 inversion matrix
  delta=P2.y-P1.y;
  if(fabs(delta)<OC_REAL8_EPSILON) {
    // Ray and line are parallel.
    // NOTE: Do _not_ confuse OC_REAL8_EPSILON with eps is preceding
    //       discussion.
    if(fabs(Q.y-P1.y)<OC_REAL4_EPSILON &&
       Q.x>OC_MIN(P1.x,P2.x)-OC_REAL4_EPSILON &&
       Q.x<OC_MAX(P1.x,P2.x)+OC_REAL4_EPSILON)
      return -1;
    return 0;
  }
  OC_REAL8 delta_inv=1./delta;

  OC_REAL8 s=(P1.x-Q.x)+(Q.y-P1.y)*(P2.x-P1.x)*delta_inv;
  /// s is the distance from Q to line through P1 & P2

  OC_REAL8 t=(Q.y-P1.y)*delta_inv;
  /// t is the location of the intersection point on P1+P2,
  /// using convex coordinate representation.

  if(fabs(s)<OC_REAL4_EPSILON &&
     t>(-OC_REAL4_EPSILON) && t<(1+OC_REAL4_EPSILON)) return -1; // Very close!

  // Make sure intersection is on positive side of ray
  if(s<0) return 0;  // Negative side

  // Is intersection inside line segment proper?
  if(delta>0) {
    if(t<0. || t>=1.) return 0;
  } else {
    if(t<=0. || t>1.) return 0;
  }

  // Else we have a valid intersection
  return 1;
}

// OommfPolygon::IsInside returns 1 if "pt" is inside the polygon,
// 0 otherwise.  The technique is to draw a ray from "pt" in
// direction (1,eps) (where eps is an arbitrarily small positive
// value) and count how many polygon edges get crossed.  An even
// count occurs for points outside the polygon, odd for inside.
// The eps shift is used to avoid vertices.  See also page 11-12
// (7-Sept-95) of MJD's micromagnetic NIST lab notebook.
// Additionally (2-Nov-1998), a check is made to see if Q is very
// near (OC_REAL4_EPSILON) the boundary of the polygon.  If so, then
// it is included.  (See item (4) on page 12 of lab notebook.)
int OommfPolygon::IsInside(const PlanePoint& pt) const
{
  const PlanePoint *pt0=NULL,*pta=NULL,*ptb=NULL;
  Nb_List_Index<PlanePoint> key;
  if((pt0=vertex.GetFirst(key))==NULL
     || (ptb=vertex.GetNext(key))==NULL)
    return 0; // Not enough points in polygon
  pta=pt0;
  int intersections,crosscode;
  intersections=0;
  do {
    crosscode=CrossSegment(pt,*pta,*ptb);
    if(crosscode<0) return 1; // pt is on or nearly on this edge
    intersections+=crosscode;
    pta=ptb;
  } while((ptb=vertex.GetNext(key))!=NULL);

  crosscode=CrossSegment(pt,*pta,*pt0);  // Close polygon
  if(crosscode<0) return 1; // pt is on or nearly on this edge
  intersections+=crosscode;
  return intersections%2;
}

// Polygon coordinate transformation routines (Scale, Shift and Rotate)
void OommfPolygon::Scale(OC_REAL8m xscale,OC_REAL8m yscale)
{
  Nb_List_Index<PlanePoint> key;
  for(PlanePoint* pt=vertex.GetFirst(key); pt!=NULL;
      pt=vertex.GetNext(key)) {
    pt->x*=xscale; pt->y*=yscale;
  }
}

void OommfPolygon::Shift(OC_REAL8m xoffset,OC_REAL8m yoffset)
{
  Nb_List_Index<PlanePoint> key;
  for(PlanePoint* pt=vertex.GetFirst(key) ;pt!=NULL;
      pt=vertex.GetNext(key)) {
    pt->x+=xoffset; pt->y+=yoffset;
  }
}

void OommfPolygon::Rotate(OC_REAL8m angcos,OC_REAL8m angsin)
{
  OC_REAL8m tempx,tempy;
  Nb_List_Index<PlanePoint> key;
  for(PlanePoint* pt=vertex.GetFirst(key); pt!=NULL;
      pt=vertex.GetNext(key)) {
    tempx=pt->x; tempy=pt->y;
    pt->x=angcos*tempx-angsin*tempy;
    pt->y=angsin*tempx+angcos*tempy;

  }
}

// Rotates and rescales bitmap as necessary to shift vertex[0]
// to the nearest integral coordinate.  This may make some bitmaps
// prettier.
void OommfPolygon::AdjustFirstToInteger(void)
{
  PlanePoint newpt;
  OC_REAL8 magold,magnew,angcos,angsin,scale;
  Nb_List_Index<PlanePoint> key;
  PlanePoint* pta=vertex.GetFirst(key);
  if(pta==NULL) return; // Empty polygon; do nothing
  magold=pta->Mag();
  if(magold<OC_REAL8_EPSILON) return; // pta is too small to do anything with.
  newpt.x=OC_ROUND(pta->x);
  newpt.y=OC_ROUND(pta->y);
  magnew=newpt.Mag();
  scale=magnew/magold;
  angcos=(pta->x*newpt.x+pta->y*newpt.y)/(magold*magnew);
  angsin=sqrt(1-angcos*angcos);
  if(pta->x*newpt.y<pta->y*newpt.x) angsin*=-1;
  this->Rotate(angcos,angsin);
  this->Scale(scale,scale);
}

// Simply adjusts each vertex individually to nearest integer
void OommfPolygon::AdjustAllToIntegers()
{
  Nb_List_Index<PlanePoint> key;
  for(PlanePoint* pt=vertex.GetFirst(key); pt!=NULL;
      pt=vertex.GetNext(key)) {
    pt->x=OC_ROUND(pt->x);
    pt->y=OC_ROUND(pt->y);
  }
}

void OommfPolygon::MakeVertexList(Nb_List<PlanePoint>& vlist) const
{ // Copies vertex into vlist, repeating the first and last vertex to
  // make a closed polyhedron.
  assert(vertex.GetSize()>=3);
  vlist.Clear();
  Nb_List_Index<PlanePoint> key;
  const PlanePoint* firstpt=vertex.GetFirst(key);
  if(firstpt != NULL) {
    vlist.SetFirst(*firstpt);
    const PlanePoint* pt;
    while((pt=vertex.GetNext(key))!=NULL) {
      vlist.Append(*pt);
    }
    vlist.Append(*firstpt);
  }
}

void OommfPolygon::DumpVertices(FILE* fout) const
{ // Debugging tool
  Nb_List_Index<PlanePoint> key;
  for(const PlanePoint* pt=vertex.GetFirst(key); pt!=NULL;
      pt=vertex.GetNext(key)) {
    fprintf(fout,"(%g,%g) ",
            static_cast<double>(pt->x),
            static_cast<double>(pt->y));
  }
  fprintf(fout,"\n");
}

//////////////////////////////////////////////////////////////////////////
// OOMMF Bitmap class
const ClassDoc OommfBitmap::class_doc("OommfBitmap",
		    "Michael J. Donahue (michael.donahue@nist.gov)",
		    "1.0.0","23-Oct-1998");

void OommfBitmap::Free()
{
  if(bitmap==NULL) return;
  delete[] bitmap;
  bitmap=NULL;
  xmin=ymin=xmax=ymax=xsize=ysize=totalsize=0;
}

void OommfBitmap::Alloc(int new_xmin,int new_ymin,
			int new_xmax,int new_ymax)
{
#define MEMBERNAME "Alloc(int,int,int,int,int)"
  Free();  // Release previous bitmap, if any
  xmin=new_xmin; xmax=new_xmax;
  ymin=new_ymin; ymax=new_ymax;
  xsize=xmax-xmin+1;
  ysize=ymax-ymin+1;
  if(xsize<1 || ysize<1 )
    FatalError(-1,STDDOC,"Illegal bitmap size request: %d x %d",xsize,ysize);
  totalsize=xsize*ysize;
  if(totalsize<xsize || totalsize<ysize)
    FatalError(-1,STDDOC,"Integer overflow detecting allocating bitmap"
	       " of size: %d x %d",xsize,ysize);
  bitmap=new OommfPackedRGB[totalsize];
  if(bitmap==0)
    FatalError(-1,STDDOC,"Insufficient memory for allocating bitmap"
	       " of size: %d x %d",xsize,ysize);
#undef MEMBERNAME
}

OommfBitmap::OommfBitmap() {
  bitmap=NULL;
  MakeStandardArrow(default_arrow);
  MakeStandardDiamond(default_diamond);
  MakeStandardCross(default_cross);
  Setup(0,0,1,1);
}

void OommfBitmap::Setup(int new_xmin,int new_ymin,
                        int new_xmax,int new_ymax)
{
  Alloc(new_xmin,new_ymin,new_xmax,new_ymax);
}

void OommfBitmap::SetPixel(int x,int y,OommfPackedRGB color)
{
#define MEMBERNAME "SetPixel(int,int,OommfPackedRGB)"
  int offset= (y-ymin)*xsize + (x-xmin);
#ifdef CODECHECKS
  if(offset<0 || offset>=totalsize)
    FatalError(-1,STDDOC,"Out-of-bounds bitmap write attempted: "
	       "(%d,%d) (should be < (%d,%d) )",x,y,xsize,ysize);
#endif
  bitmap[offset]=color;
#undef MEMBERNAME
}

OommfPackedRGB OommfBitmap::GetPixel(int x,int y) const
{
#define MEMBERNAME "GetPixel(int,int)"
  int offset= (y-ymin)*xsize + (x-xmin);
#ifdef CODECHECKS
  if(offset<0 || offset>=totalsize)
    FatalError(-1,STDDOC,"Out-of-bounds bitmap read attempted: "
	       "(%d,%d) (should be < (%d,%d) )",x,y,xsize,ysize);
#endif
  return bitmap[offset];
#undef MEMBERNAME
}

void OommfBitmap::Erase(OommfPackedRGB color)
{
  for(int i=0;i<totalsize;i++) bitmap[i]=color;
}

void OommfBitmap::DrawFilledRectangle(OC_REAL8m x1,OC_REAL8m y1,
                                      OC_REAL8m x2,OC_REAL8m y2,
                                      OommfPackedRGB color,
				      const char* transparency_stipple)
{ // (x1,y1) and (x2,y2) are opposing vertices, and are *both*
  // included in the rectangle.  Note that this differs from the
  // Tk canvas widget specification, in which only the upper and left
  // edges are included (the lower and right are not).
  //   Specify transparency_stipple to NULL or "" to get no stipple.
#define MEMBERNAME "DrawFilledRectangle(4xOC_REAL8m,OommfPackedRGB,const char*)"
  int ix1,ix2;
  if(x2<x1) { ix1=int(floor(x2)); ix2=int(ceil(x1)); }
  else      { ix1=int(floor(x1)); ix2=int(ceil(x2)); }
  int iy1,iy2;
  if(y2<y1) { iy1=int(floor(y2)); iy2=int(ceil(y1)); }
  else      { iy1=int(floor(y1)); iy2=int(ceil(y2)); }
  if(ix1<xmin) ix1=xmin;
  if(ix2>xmax) ix2=xmax;
  if(iy1<ymin) iy1=ymin;
  if(iy2>ymax) iy2=ymax;
  if(transparency_stipple!=NULL &&
     strcmp("gray25",transparency_stipple)==0) {
    // 25% stipple
    for(int ix=ix1+(ix1%2);ix<=ix2;ix+=2) {
      for(int iy=iy1+(iy1+ix/2)%2;iy<=iy2;iy+=2) {
	SetPixel(ix,iy,color);
      }
    }
  } else {
    if(transparency_stipple!=NULL && transparency_stipple[0]!='\0') {
      NonFatalError(STDDOC,"Unsupported stipple pattern: \"%s\";"
		    "  Using solid fill.",
		    transparency_stipple);
    }
    // No stipple
    for(int ix=ix1;ix<=ix2;ix++) for(int iy=iy1;iy<=iy2;iy++) {
      SetPixel(ix,iy,color);
    }
  }
#undef MEMBERNAME
}

void OommfBitmap::DrawRectangleFrame(OC_REAL8m x1,OC_REAL8m y1,
				     OC_REAL8m x2,OC_REAL8m y2,
				     OC_REAL8m borderwidth,
				     OommfPackedRGB color)
{ // A frame is drawn outside the rectangle specified by opposing
  // vertices (x1,y1) and (x2,y2).  The line width of the frame
  // is specified by 'borderwidth', and the color by 'color'.

  if(x1>x2) { OC_REAL8m temp=x1; x2=x1; x1=temp; }
  if(y1>y2) { OC_REAL8m temp=y1; y2=y1; y1=temp; }
  int ix1=int(OC_ROUND(x1))-1;   // Make sure we don't overlap inside
  int ix2=int(OC_ROUND(x2))+1;   // rectangle
  int iy1=int(OC_ROUND(y1))-1;
  int iy2=int(OC_ROUND(y2))+1;
  int iwidth=int(OC_ROUND(borderwidth-1.)); // Subtract one because
  /// all runs below are made inclusive to their endpoints.

  int ixa,iya,ixb,iyb;  // Working vertices
  int ix,iy;            // Pixel points

  // Top line
  ixa=ix1-iwidth;  if(ixa<xmin) ixa=xmin;
  ixb=ix2+iwidth;  if(ixb>xmax) ixb=xmax;
  iya=iy1-iwidth;  if(iya<ymin) iya=ymin;
  iyb=iy1;         if(iyb>ymax) iyb=ymax;
  for(ix=ixa;ix<=ixb;ix++) for(iy=iya;iy<=iyb;iy++) {
    SetPixel(ix,iy,color);
  }

  // Bottom line
  iya=iy2;         if(iya<ymin) iya=ymin;
  iyb=iy2+iwidth;  if(iyb>ymax) iyb=ymax;
  for(ix=ixa;ix<=ixb;ix++) for(iy=iya;iy<=iyb;iy++) {
    SetPixel(ix,iy,color);
  }

  // Left edge
  ixa=ix1-iwidth;  if(ixa<xmin) ixa=xmin;
  ixb=ix1;	   if(ixb>xmax) ixb=xmax;
  iya=iy1;	   if(iya<ymin) iya=ymin;
  iyb=iy2;	   if(iyb>ymax) iyb=ymax;
  for(ix=ixa;ix<=ixb;ix++) for(iy=iya;iy<=iyb;iy++) {
    SetPixel(ix,iy,color);
  }

  // Right edge
  ixa=ix2;         if(ixa<xmin) ixa=xmin;
  ixb=ix2+iwidth;  if(ixb>xmax) ixb=xmax;
  for(ix=ixa;ix<=ixb;ix++) for(iy=iya;iy<=iyb;iy++) {
    SetPixel(ix,iy,color);
  }

}

void OommfBitmap::DrawFilledPoly(const OommfPolygon& poly,
				 OommfPackedRGB color,OC_BOOL antialias)
{
  OC_REAL8m raxmin,raymin,raxmax,raymax;
  poly.GetBounds(raxmin,raymin,raxmax,raymax);
  int axmin=int(floor(raxmin));  if(axmin<xmin) axmin=xmin;
  int aymin=int(floor(raymin));  if(aymin<ymin) aymin=ymin;
  int axmax=int(floor(raxmax));  if(axmax>xmax) axmax=xmax;
  int aymax=int(floor(raymax));  if(aymax>ymax) aymax=ymax;
  if(antialias) {
#   define AASIZE 4   // This is subsampling array edge count.
    OommfPackedRGB pixcolor;
    /// 2 is definitely not enough, 3 may be okay, 4 is pretty good.
    for(int x=axmin;x<=axmax;x++) for(int y=aymin;y<=aymax;y++) {
      int incount=0;
      for(OC_REAL8m ax=(OC_REAL8m(1-AASIZE)/OC_REAL8m(2*AASIZE));
	  ax<0.5;ax+=(1./OC_REAL8m(AASIZE))) {
	for(OC_REAL8m ay=(OC_REAL8m(1-AASIZE)/OC_REAL8m(2*AASIZE));
	    ay<0.5;ay+=(1./OC_REAL8m(AASIZE))) {
	  if(poly.IsInside(PlanePoint(double(x)+ax,double(y)+ay))) {
	    incount++;
	  }
	}
      }
      if(incount>=AASIZE*AASIZE) SetPixel(x,y,color);
      else if(incount>0) {
	pixcolor.Blend(OC_REAL8m(incount)/OC_REAL8m(AASIZE*AASIZE),color,
		       OC_REAL8m(AASIZE*AASIZE-incount)/OC_REAL8m(AASIZE*AASIZE),
		       GetPixel(x,y));
	SetPixel(x,y,pixcolor);
      }
    }
#   undef AASIZE
  } else { // No antialiasing
    for(int x=axmin;x<=axmax;x++) for(int y=aymin;y<=aymax;y++) {
      if(poly.IsInside(PlanePoint(double(x),double(y)))) {
	SetPixel(x,y,color);
      }
    }
  }
}

void OommfBitmap::DrawPolyLine(const Nb_List<PlanePoint>& vlist,
			       OC_REAL8m linewidth,OommfPackedRGB color,
			       int joinstyle,int antialias)
{ // Draws (potentially fat) line segments between each successive pair
  // of points in vlist.  If the joinstyle>0, then the segment ends are
  // connected with bevel joins.  In general, this is an unclosed
  // figure, unless the last and first point are equal, in which case
  // those ends will be joined if joinstyle>0.

  const PlanePoint *pta,*ptb; // Polyline vertex point pair
  PlanePoint qa1,qa2,qb1,qb2; // Fat line corner points
  const PlanePoint *rta=NULL; // First corner point
  PlanePoint ra1,ra2;         // Corner points for 1st vertex of 1st line

  OommfPolygon fatline;

  OC_REAL8m offset=linewidth/2.;

  Nb_List_Index<PlanePoint> key;
  pta=vlist.GetFirst(key);
  while((ptb=vlist.GetNext(key))!=NULL) {
    // Construct perpendicular to line segment
    OC_REAL8m zy= ptb->x - pta->x;
    OC_REAL8m zx= pta->y - ptb->y;
    OC_REAL8m magsq=zx*zx+zy*zy;
    if(magsq>OC_REAL8_EPSILON) {
      // Line segment big enough to work with
      OC_REAL8m adj=offset/sqrt(magsq);
      zx*=adj; zy*=adj;
      qa1.x=pta->x+zx;      qa2.x=pta->x-zx;
      qa1.y=pta->y+zy;      qa2.y=pta->y-zy;
      if(joinstyle>0 && rta!=NULL) {
	// Join ends with last segment
	fatline.Clear();
	fatline.AddVertex(qb1); fatline.AddVertex(qa1);
	fatline.AddVertex(qb2); fatline.AddVertex(qa2);
	DrawFilledPoly(fatline,color,antialias);
      }
      qb1.x=ptb->x+zx;      qb2.x=ptb->x-zx;
      qb1.y=ptb->y+zy;      qb2.y=ptb->y-zy;
      fatline.Clear();
      fatline.AddVertex(qa1);  fatline.AddVertex(qa2);
      fatline.AddVertex(qb2);  fatline.AddVertex(qb1);
      DrawFilledPoly(fatline,color,antialias);
      if(rta==NULL) {
        rta=pta; ra1=qa1; ra2=qa2; // Save 1st (rendered) vertex info
      }
    }
    pta=ptb;
  }
  if(joinstyle>0 && rta!=NULL) {
    OC_REAL8m diffx = pta->x - rta->x;
    OC_REAL8m diffy = pta->y - rta->y;
    if( diffx*diffx+diffy*diffy < OC_REAL8_EPSILON ) {
      // Join first and last segments
      fatline.Clear();
      fatline.AddVertex(qb1); fatline.AddVertex(ra1);
      fatline.AddVertex(qb2); fatline.AddVertex(ra2);
      DrawFilledPoly(fatline,color,antialias);
    }
  }
}

void OommfBitmap::DrawFilledArrow(OC_REAL8m xc,OC_REAL8m yc,OC_REAL8m size,
                                 OC_REAL8m xcos,OC_REAL8m ycos,OC_REAL8m zcos,
                                 OommfPackedRGB color,OC_BOOL antialias,
                                 OC_REAL8m outline_width,
                                 OommfPackedRGB outline_color)
{ // Draws arrow centered at point (xc,yc) in bitmap, of specified size,
  // directional cosines xcos,ycos,zcos, with specified color.
  work_poly=default_arrow;
  OC_REAL8m sizeadj=1.-zcos*zcos;
  if(sizeadj<OC_REAL8_EPSILON) return;  // In-plane comp. too small to display
  sizeadj=sqrt(sizeadj);
  work_poly.Scale(size*sizeadj,size);
  work_poly.Rotate(xcos/sizeadj,ycos/sizeadj);
  work_poly.Shift(xc,yc);
  DrawFilledPoly(work_poly,color,antialias);
  if(outline_width != 0.0) {
    const OC_REAL8m base_width = 1.5; // Emperical value
    Nb_List<PlanePoint> vlist;
    work_poly.MakeVertexList(vlist);
    DrawPolyLine(vlist,outline_width*base_width,outline_color,2,antialias);
  }
}

void OommfBitmap::DrawDiamondWithCross(OC_REAL8m xc,OC_REAL8m yc,OC_REAL8m size,
				       OommfPackedRGB outercolor,
				       OommfPackedRGB innercolor)
{ // Symbol representing out-of-plane vectors
  xc=OC_ROUND(xc); yc=OC_ROUND(yc); // Make it look nice
  work_poly=default_diamond;
  work_poly.Scale(size,size);
  work_poly.AdjustAllToIntegers();
  work_poly.Shift(xc,yc);
  DrawFilledPoly(work_poly,outercolor,0);
  if(size>MinCrossDisplaySize) {
    work_poly=default_cross;
    work_poly.Scale(size,size);
    work_poly.Shift(xc,yc);
    DrawFilledPoly(work_poly,innercolor,0);
  }
}

void OommfBitmap::DrawSquareWithDot(OC_REAL8m xc,OC_REAL8m yc,OC_REAL8m size,
				    OommfPackedRGB outercolor,
				    OommfPackedRGB innercolor)
{ // Symbol representing in-to-plane vectors.
  xc=OC_ROUND(xc); yc=OC_ROUND(yc); // Make it look nice
  OC_REAL8m radius1=ceil(InOutTipRadius*size);
  DrawFilledRectangle(xc-radius1,yc-radius1,xc+radius1,yc+radius1,
		      outercolor,NULL);
  OC_REAL8m radius2=floor(DotRadius*size);
  if(radius2>0.5) {
    DrawFilledRectangle(xc-radius2,yc-radius2,xc+radius2,yc+radius2,
			innercolor,NULL);
  }
}

void OommfBitmap::AddMat(OC_REAL8m width,OommfPackedRGB color)
{ // Lays flat mat around border of specified width (in pixels)
  // and color.
  if(width<0.5) return;

  OC_REAL8m x1=xmin+width;
  OC_REAL8m y1=ymin+width;
  OC_REAL8m x2=xmax-width;
  OC_REAL8m y2=ymax-width;

  if(x2<=x1 || y2<=y1) {
    DrawFilledRectangle(xmin,ymin,xmax,ymax,color,NULL);
    /// Fill entire space
  } else {
    DrawRectangleFrame(x1,y1,x2,y2,width,color);
  }
}

int OommfBitmap::WritePPMFile(const char* filename,int ppm_type)
{ // Use filename == NULL or "-" to write to stdout.
  // "ppm_type" corresponds to the PPM magic number.  It should be either
  // 3 (ASCII text) or 6 (binary).
  // Return value is
  //     0 => No error
  //     1 => Bad parameter
  //     2 => Unable to open filename for output
  //     4 => Write error
#define MEMBERNAME "WritePPMFile(char*,int)"
  if(ppm_type!=3 && ppm_type!=6) {
    NonFatalError(STDDOC,"Illegal PPM format type requested: %d."
		  "  Should be either 3 or 6.",ppm_type);
    return 1;
  }

  // Open file and write header
  FILE* fptr;
  if(filename==NULL || strcmp(filename,"-")==0) fptr=stdout;
  else if((fptr=Nb_FOpen(filename,"w"))==NULL) {
    NonFatalError(STDDOC,"Unable to open file %s for output",filename);
    return 2;
  }
  setvbuf(fptr,(char *)NULL,_IOFBF,8192);

#if (TKCOLORLENGTH != 6)
# error "Current OommfBitmap code only supports TKCOLORLENGTH==6"
#endif

  int errorcode=0;
  if(ppm_type==6) {
    fprintf(fptr,"P6\n%d %d\n%d\n",xsize,ysize,255);
    for(int i=0;i<totalsize;i++) {
      // Note: The following code breaks encapsulation of
      //       the OommfPackedRGB class.
      if(fwrite((char *)(bitmap+i),1,3,fptr) != 3) {
	errorcode=4;
	break;
      }
    }
  } else {
    unsigned int red,green,blue;
    fprintf(fptr,"P3\n%d %d\n%d\n",xsize,ysize,255);
    for(int i=0;i<totalsize;i++) {
      bitmap[i].Get(red,green,blue);
      fprintf(fptr,"%u %u %u\n",red,green,blue);
      if(feof(fptr) || ferror(fptr)) {
	errorcode=4;
	break;
      }
    }
  }
  fclose(fptr);
  return errorcode;
#undef MEMBERNAME

}

int OommfBitmap::WritePPMChannel(Tcl_Channel channel,int ppm_type)
{ // "ppm_type" corresponds to the PPM magic number.  It should be either
  // 3 (ASCII text) or 6 (binary).
  // Return value is
  //     0 => No error
  //     1 => Bad parameter
  //     4 => Write error
#define MEMBERNAME "WritePPMChannel(Tcl_Channel,int)"
  if(ppm_type!=3 && ppm_type!=6) {
    NonFatalError(STDDOC,"Illegal PPM format type requested: %d."
		  "  Should be either 3 or 6.",ppm_type);
    return 1;
  }

  // Write header
#if (TKCOLORLENGTH != 6)
# error "Current OommfBitmap code only supports TKCOLORLENGTH==6"
#endif

  // Safety against callers who might not pass us a binary channel
  Tcl_SetChannelOption(NULL, channel,
                       Oc_AutoBuf("-translation"), Oc_AutoBuf("binary"));

  /// Note: On some (non-ANSI) compilers, sprintf() returns a pointer
  ///       to the buffer instead of an output byte count.
  if(ppm_type==6) {
    char scratch[256];  // Make sure this is big enough as used below!
    sprintf(scratch,"P6\n%d %d\n%d\n",xsize,ysize,255);
    Tcl_Write(channel,scratch,int(strlen(scratch)));
    for(int i=0;i<totalsize;i++) {
      // Note: The following code breaks encapsulation of
      //       the OommfPackedRGB class.
      if(Tcl_Write(channel,(char *)(bitmap+i),3) != 3) {
	return 4;
      }
    }
  } else {
    char scratch[1024];  // Make sure this is big enough as used below!
    sprintf(scratch,"P3\n%d %d\n%d\n",xsize,ysize,255);
    Tcl_Write(channel,scratch,int(strlen(scratch)));
    unsigned int comp[12];
    int i,chunksize;
    for(i=0;i<totalsize-3;i+=4) {
      // Unrolled loop
      bitmap[i].Get(comp[0],comp[1],comp[2]);   // Red, Green, Blue
      bitmap[i+1].Get(comp[3],comp[4],comp[5]);
      bitmap[i+2].Get(comp[6],comp[7],comp[8]);
      bitmap[i+3].Get(comp[9],comp[10],comp[11]);
      sprintf(scratch,"%u %u %u\n%u %u %u\n%u %u %u\n%u %u %u\n",
	      comp[0],comp[1],comp[2],comp[3],comp[4],comp[5],
	      comp[6],comp[7],comp[8],comp[9],comp[10],comp[11]);
      chunksize=int(strlen(scratch));
      if(Tcl_Write(channel,scratch,chunksize) != chunksize) {
	return 4;
      }
    }
    for(;i<totalsize;i++) {
      // Remainder
      bitmap[i].Get(comp[0],comp[1],comp[2]);
      sprintf(scratch,"%u %u %u\n",comp[0],comp[1],comp[2]);
      chunksize=int(strlen(scratch));
      if(Tcl_Write(channel,scratch,chunksize) != chunksize) {
	return 4;
      }
    }
  }
  return 0;
#undef MEMBERNAME
}

int OommfBitmap::WriteBMPChannel(Tcl_Channel channel,int bmp_type)
{ // Use filename == NULL or "-" to write to stdout.
  // "bmp_type" corresponds to the number of bits per pixel.
  // Currently only bmp_type == 24 is supported.
  // Return value is
  //     0 => No error
  //     1 => Bad parameter
  //     2 => Unable to open filename for output
  //     4 => Write error
#define MEMBERNAME "WriteBMPChannel(Tcl_Channel,int)"
  if(bmp_type!=24) {
    NonFatalError(STDDOC,"Unsupported BMP format requested: %d bpp."
		  "  Should be 24.",bmp_type);
    return 1;
  }

#if (TKCOLORLENGTH != 6)
# error "Current OommfBitmap code only supports TKCOLORLENGTH==6"
#endif

  // Compose header
  struct MSBitmapHeader {
    OC_BYTE  Type[2];
    OC_UINT4 FileSize;
    OC_UINT2 Reserved1;
    OC_UINT2 Reserved2;
    OC_UINT4 OffBits;
    OC_UINT4 BmiSize;
    OC_UINT4 Width;
    OC_UINT4 Height;
    OC_UINT2 Planes;
    OC_UINT2 BitCount;
    OC_UINT4 Compression;
    OC_UINT4 SizeImage;
    OC_UINT4 XPelsPerMeter;
    OC_UINT4 YPelsPerMeter;
    OC_UINT4 ClrUsed;
    OC_UINT4 ClrImportant;
  };
  MSBitmapHeader head;

  const int headsize=54; // Header size; Don't use sizeof(head) because
  /// that can influenced by machine-specific alignment restrictions
  int bpp=bmp_type;
  int rowsize=4*((xsize*bpp+31)/32); // Rows lie on a 4-byte bdry.
  int padsize=rowsize-(xsize*bpp+7)/8;  // # of 0 bytes to pad row with.

  head.Type[0]='B'; head.Type[1]='M';
  head.FileSize=headsize+rowsize*ysize;
  head.Reserved1=head.Reserved2=0;
  head.OffBits=headsize;
  head.BmiSize=headsize-14;
  head.Width=xsize;
  head.Height=ysize;
  head.Planes=1;
  head.BitCount=OC_UINT2(bpp);
  head.Compression=0;
  head.SizeImage=head.FileSize-headsize;
  head.XPelsPerMeter=head.YPelsPerMeter=0;  // Any better ideas?
  head.ClrUsed=0;
  head.ClrImportant=0;

  // MS Bitmap format is little-endian ordered;
  // Adjust byte ordering, if necessary.
#if OC_BYTEORDER != 4321
#if OC_BYTEORDER != 1234
#error "Unsupported byte-order format"
#endif
  Oc_Flip4(&head.FileSize);
  Oc_Flip2(&head.Reserved1);      Oc_Flip2(&head.Reserved2);
  Oc_Flip4(&head.OffBits);
  Oc_Flip4(&head.BmiSize);
  Oc_Flip4(&head.Width);          Oc_Flip4(&head.Height);
  Oc_Flip2(&head.Planes);         Oc_Flip2(&head.BitCount);
  Oc_Flip4(&head.Compression);    Oc_Flip4(&head.SizeImage);
  Oc_Flip4(&head.XPelsPerMeter);  Oc_Flip4(&head.YPelsPerMeter);
  Oc_Flip4(&head.ClrUsed);        Oc_Flip4(&head.ClrImportant);
#endif

  // Safety against callers who might not pass us a binary channel
  Tcl_SetChannelOption(NULL, channel,
                       Oc_AutoBuf("-translation"), Oc_AutoBuf("binary"));

  // Write header.  Do this one field at a time to avoid
  // machine specific alignment restrictions.  (Binary headers
  // blow chunks.)
  Tcl_Write(channel,(char *)head.Type,sizeof(head.Type));
#define OBWBC_Write(var) Tcl_Write(channel,(char *)(&var),sizeof(var))
  OBWBC_Write(head.FileSize);
  OBWBC_Write(head.Reserved1);
  OBWBC_Write(head.Reserved2);
  OBWBC_Write(head.OffBits);
  OBWBC_Write(head.BmiSize);
  OBWBC_Write(head.Width);
  OBWBC_Write(head.Height);
  OBWBC_Write(head.Planes);
  OBWBC_Write(head.BitCount);
  OBWBC_Write(head.Compression);
  OBWBC_Write(head.SizeImage);
  OBWBC_Write(head.XPelsPerMeter);
  OBWBC_Write(head.YPelsPerMeter);
  OBWBC_Write(head.ClrUsed);
  OBWBC_Write(head.ClrImportant);
#undef OBWBC_Write

  // Write bitmap, row by row, from bottom up.
  unsigned int red,green,blue;
  unsigned char bgr[3];
  OC_UINT4 padbuf=0;  // padsize should be <4
  for(int j=ysize-1 ; j>=0 ; j--) {
    for(int i=0;i<xsize;i++) {
      bitmap[j*xsize+i].Get(red,green,blue);
      bgr[0]=(unsigned char)blue;
      bgr[1]=(unsigned char)green;
      bgr[2]=(unsigned char)red;
      // BMP format stores color components in the order
      // blue-green-red.  Presumably this is an artifact
      // of the x86's little endian architecture.
      if(Tcl_Write(channel,(char *)bgr,3) != 3) {
        return 4;
      }
    }
    // Pad with 0's to rowsize bdry
    if(padsize>0) {
      if(Tcl_Write(channel,(char *)(&padbuf),padsize) != padsize)
        return 4;
    }
  }

  return 0;
#undef MEMBERNAME
}
