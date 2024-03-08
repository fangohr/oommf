/* FILE: psdraw.cc                 -*-Mode: c++-*-
 *
 * OOMMF C++ classes for PostScript rendering
 *
 */

#include <ctime>

#include "psdraw.h"
#include "nb.h"

/* End includes */     // This is an optional directive to build.tcl,
                       // that excludes the remainder of the file from
                       // '#include ".*"' dependency building

////////////////////////////////////////////////////////////////////////
const ClassDoc OommfPSDraw::class_doc("OommfPSDraw",
                    "Michael J. Donahue (michael.donahue@nist.gov)",
                    "1.0.0","3-Sep-2003");

// Initialize OommfPSDraw object and write header.  Offsets and print
// sizes are in points, px/y range values in pixels.  x/yoff are to
// lower lefthand corner of image.  Pixel coordinates run from upper
// lefthand corner of image.
OommfPSDraw::OommfPSDraw
(Tcl_Channel use_channel,
 OC_REAL8m pxoff,OC_REAL8m pyoff,
 OC_REAL8m printwidth,OC_REAL8m printheight,
 const char* page_orientation,
 OC_REAL8m xmin,OC_REAL8m ymin,
 OC_REAL8m xmax,OC_REAL8m ymax,
 OommfPackedRGB background_color,
 OC_REAL8m matwidth,
 OommfPackedRGB matcolor,
 OommfPackedRGB arrow_outline_color,
 OC_REAL8m arrow_outline_width,
 OC_REAL8m arrow_width,
 OC_REAL8m arrow_tip_width,
 OC_REAL8m arrow_length,
 OC_REAL8m arrow_head_width,
 OC_REAL8m arrow_head_ilen,
 OC_REAL8m arrow_head_olen)
  : channel(use_channel),
    bbox_xmin(int(floor(pxoff))),
    bbox_ymin(int(floor(pyoff))),
    bbox_xmax(int(ceil(pxoff
     +(strcmp("Landscape",page_orientation)==0 ? printheight : printwidth)))),
    bbox_ymax(int(ceil(pyoff
     +(strcmp("Landscape",page_orientation)==0 ? printwidth : printheight)))),
    scale(OC_MIN(printwidth/(xmax-xmin),printheight/(ymax-ymin))),
    mat_width(matwidth),
    mat_color(matcolor),
    ArrowOutlineColor(arrow_outline_color),
    ArrowOutlineWidth(arrow_outline_width),
    ArrowWidth(arrow_width),
    ArrowTipWidth(arrow_tip_width),
    ArrowLength(arrow_length),
    ArrowHeadWidth(arrow_head_width),
    ArrowHeadInnerLength(arrow_head_ilen),
    ArrowHeadOuterLength(arrow_head_olen),
    InOutTipRadius(0.15)
{
  // Write header
  // NOTE: When using Nb_WriteChannel, each '%' in the output
  //  string is written to the output channel.  However, when
  //  using Nb_FprintfChannel, the passed string is treated as
  //  a format string, so to get a '%' on the output channel
  //  you need to use "%%".
  Nb_WriteChannel(channel,"%!PS-Adobe-3.0 EPSF-3.0\n",-1);
  Nb_WriteChannel(channel,"%%Creator: OommfPSDraw\n",-1);
  Nb_WriteChannel(channel,"%%CreationDate: ",-1);
  time_t current_time = time(NULL);
  Nb_WriteChannel(channel,ctime(&current_time),-1);
  Nb_FprintfChannel(channel,NULL,1024,"%%%%BoundingBox: %d %d %d %d\n",
                    bbox_xmin,bbox_ymin,bbox_xmax,bbox_ymax);

  Nb_FprintfChannel(channel,NULL,1024,"%%%%Orientation: %s\n",
                    page_orientation);

  Nb_WriteChannel(channel,"%%Pages: 1\n",-1);
  Nb_WriteChannel(channel,"%%DocumentData: Clean7Bit\n",-1);
  Nb_WriteChannel(channel,"%%EndComments\n\n",-1);

  // Base draw routines
  Nb_WriteChannel(channel,"%%BeginProlog\n",-1);

  // Position adjusment to device.  Intended for drawing lines
  Nb_WriteChannel(channel,"/DevHalfAdj { %stack: x y\n",-1);
  Nb_WriteChannel(channel,"  transform\n",-1);
  Nb_WriteChannel(channel,"  exch floor 0.5 add exch floor 0.5 add\n",-1);
  Nb_WriteChannel(channel,"  itransform\n",-1);
  Nb_WriteChannel(channel,"} def\n\n",-1);

  // Arrow routines. See NOTES IX, 27-Oct-2021, pp. 4-5 for details
  Nb_WriteChannel(channel,"/MakeStandardArrow { %stack: r g b\n",-1);
  if(ArrowOutlineWidth>0.0) { // Draw outline about standard arrow
    // Compute needed trig ratios
    // Special case handling: NOTES IX, 29-Oct-2021, pp. 6-7.
    // SPECIAL CASES: ArrowHeadWidth == ArrowTipWidth
    //                ArrowHeadOuterLength == 0
    //                ArrowHeadWidth == ArrowWidth
    // Convenience variables
    const OC_REAL8m Ld = ArrowHeadOuterLength - ArrowHeadInnerLength;
    const OC_REAL8m Lo = ArrowHeadOuterLength;
    const OC_REAL8m Wb = ArrowHeadWidth-ArrowWidth;    // width at back
    const OC_REAL8m Wt = ArrowHeadWidth-ArrowTipWidth; // width at tip
    const OC_REAL8m sAOW = ArrowOutlineWidth*ArrowWidth*0.25;

    // Compute cot(alpha2)+csc(alpha2) in NOTES IX
    const OC_REAL8m max_boff = sAOW + Ld;
    OC_REAL8m boff = sAOW*(2*Ld+sqrt(Wb*Wb+4*Ld*Ld));
    if(Ld<=0.0 || boff<Wb*max_boff) {
      boff /= Wb;
    } else {
      boff = max_boff;
    }


    // Special case handling at tip
    OC_REAL8m c2x=ArrowLength/2.-ArrowHeadOuterLength+sAOW;
    OC_REAL8m c2y=ArrowHeadWidth/2.+sAOW;
    OC_REAL8m dx =ArrowLength/2.+sAOW;
    OC_REAL8m dy =ArrowTipWidth/2.+sAOW;
    if(Wt>0.0 || Lo>0.0) {
      // Compute csc(alpha1)-cot(alpha1) in NOTES IX:
      OC_REAL8m coff = sAOW*Wt / (sqrt(Wt*Wt+4*Lo*Lo)+2*Lo);
      // Compute sec(alpha1)-tan(alpha1) in NOTES IX:
      OC_REAL8m doff = 2*sAOW*Lo / (sqrt(Wt*Wt+4*Lo*Lo)+Wt);
      c2x = ArrowLength/2.-ArrowHeadOuterLength+coff;
      dy =ArrowTipWidth/2.+doff;
    } else {
      dx = c2x = OC_MAX(dx,c2x);
      dy = c2y = OC_MAX(dy,c2y);
    }

    // Outline setup
    unsigned int red,green,blue;
    ArrowOutlineColor.Get(red,green,blue);
    Nb_FprintfChannel(channel,NULL,1024,
                      "  %.6g %.6g %.6g setrgbcolor\n",
                      red/255.,green/255.,blue/255.);
    Nb_WriteChannel(channel,"  newpath\n",-1);

    // Render upper half of arrow outline
    Nb_FprintfChannel(channel,NULL,1024,"  %.6g %.6g moveto\n",
         static_cast<double>(-ArrowLength/2.-sAOW),
         static_cast<double>(ArrowWidth/2.+sAOW));                  // Pt a
    Nb_FprintfChannel(channel,NULL,1024,"  %.6g %.6g lineto\n",
         static_cast<double>(ArrowLength/2.-ArrowHeadInnerLength-boff),
         static_cast<double>(ArrowWidth/2.+sAOW));                  // Pt b
    if(Ld<=0.0 || Wb/2>0.75*sAOW) {
      Nb_FprintfChannel(channel,NULL,1024,"  %.6g %.6g lineto\n",
         static_cast<double>(ArrowLength/2.-ArrowHeadOuterLength-boff),
         static_cast<double>(ArrowHeadWidth/2.+sAOW));              // Pt c1
      Nb_FprintfChannel(channel,NULL,1024,"  %.6g %.6g lineto\n",
         static_cast<double>(c2x),static_cast<double>(c2y));        // Pt c2
    }
    Nb_FprintfChannel(channel,NULL,1024,"  %.6g %.6g lineto\n",
         static_cast<double>(dx),static_cast<double>(dy));          // Pt d
    // Bottom half
    Nb_FprintfChannel(channel,NULL,1024,"  %.6g %.6g lineto\n",
         static_cast<double>(dx),static_cast<double>(-dy));         // Pt d
    if(Ld<=0.0 || Wb/2>0.75*sAOW) {
      Nb_FprintfChannel(channel,NULL,1024,"  %.6g %.6g lineto\n",
         static_cast<double>(c2x),static_cast<double>(-c2y));       // Pt c2
      Nb_FprintfChannel(channel,NULL,1024,"  %.6g %.6g lineto\n",
        static_cast<double>(ArrowLength/2.-ArrowHeadOuterLength-boff),
        static_cast<double>(-(ArrowHeadWidth/2.+sAOW)));            // Pt c1
    }
    Nb_FprintfChannel(channel,NULL,1024,"  %.6g %.6g lineto\n",
        static_cast<double>(ArrowLength/2.-ArrowHeadInnerLength-boff),
        static_cast<double>(-(ArrowWidth/2.+sAOW)));                // Pt b
    Nb_FprintfChannel(channel,NULL,1024,"  %.6g %.6g lineto\n",
        static_cast<double>(-ArrowLength/2.-sAOW),
        static_cast<double>(-(ArrowWidth/2.+sAOW)));                // Pt a
    Nb_WriteChannel(channel,"  closepath\n  fill\n",-1);
  }
  // Render "standard" arrow (no translation, rotation, or scaling)
  Nb_WriteChannel(channel,"  setrgbcolor\n",-1);  // stack: r g b
  Nb_WriteChannel(channel,"  newpath\n",-1);
  // Top half
  Nb_FprintfChannel(channel,NULL,1024,"  %.6g %.6g moveto\n",
                      static_cast<double>(-ArrowLength/2.),
                    static_cast<double>(ArrowWidth/2.));       // Pt A
  Nb_FprintfChannel(channel,NULL,1024,"  %.6g %.6g lineto\n",
                      static_cast<double>(ArrowLength/2.-ArrowHeadInnerLength),
                      static_cast<double>(ArrowWidth/2.));     // Pt B
  Nb_FprintfChannel(channel,NULL,1024,"  %.6g %.6g lineto\n",
                      static_cast<double>(ArrowLength/2.-ArrowHeadOuterLength),
                      static_cast<double>(ArrowHeadWidth/2.)); // Pt C
  Nb_FprintfChannel(channel,NULL,1024,"  %.6g %.6g lineto\n",
                      static_cast<double>(ArrowLength/2.),
                      static_cast<double>(ArrowTipWidth/2.));  // Pt D
  // Do reflected bottom half
  Nb_FprintfChannel(channel,NULL,1024,"  %.6g %.6g lineto\n",
                      static_cast<double>(ArrowLength/2.),
                      static_cast<double>(-ArrowTipWidth/2.));
  Nb_FprintfChannel(channel,NULL,1024,"  %.6g %.6g lineto\n",
                      static_cast<double>(ArrowLength/2.-ArrowHeadOuterLength),
                      static_cast<double>(-ArrowHeadWidth/2.));
  Nb_FprintfChannel(channel,NULL,1024,"  %.6g %.6g lineto\n",
                      static_cast<double>(ArrowLength/2.-ArrowHeadInnerLength),
                      static_cast<double>(-ArrowWidth/2.));
  Nb_FprintfChannel(channel,NULL,1024,"  %.6g %.6g lineto\n",
                      static_cast<double>(-ArrowLength/2.),
                      static_cast<double>(-ArrowWidth/2.));
  Nb_WriteChannel(channel,"  closepath\n  fill\n} def\n\n",-1);
  // MakeArrow wrapper routine that provides translation, rotation, and
  // scaling to MakeStandardArrow
  Nb_WriteChannel(channel,
                  "/MakeArrow { %stack: r g b xscale yscale rot xpos ypos\n",
                  -1);
  Nb_WriteChannel(channel,"  gsave\n",-1);
  Nb_WriteChannel(channel,"  translate\n  rotate\n  scale\n",-1);
  Nb_WriteChannel(channel,"  MakeStandardArrow\n",-1);
  Nb_WriteChannel(channel,"  grestore\n} def\n\n",-1);

  // Solid, filled rectangle
  Nb_WriteChannel(channel,"/SolidRect {"
                  " %stack: r g b x y width height\n",-1);
  Nb_WriteChannel(channel,"  gsave\n",-1);
  Nb_WriteChannel(channel,"  newpath\n",-1);
  Nb_WriteChannel(channel,"  iydevscale mul /h exch def\n",-1);
  Nb_WriteChannel(channel,"  ixdevscale mul /w exch def\n",-1);
  Nb_WriteChannel(channel,"  moveto\n",-1);
  Nb_WriteChannel(channel,"  setrgbcolor\n",-1);
  Nb_WriteChannel(channel,"  xdevscale ydevscale scale\n",-1);
  Nb_WriteChannel(channel,"  currentpoint transform\n",-1);
  // Adjust rectangle base and side lengths to device pixels.
  Nb_WriteChannel(channel,"  exch dup w 0 gt\n",-1);
  Nb_WriteChannel(channel,
                  "  { floor sub dup w add ceiling /w exch def neg }\n",-1);
  Nb_WriteChannel(channel,
                  "  { ceiling sub dup w add floor /w exch def neg }\n",-1);
  Nb_WriteChannel(channel,"  ifelse\n",-1);
  Nb_WriteChannel(channel,"  exch dup h 0 gt\n",-1);
  Nb_WriteChannel(channel,
                  "  { floor sub dup h add ceiling /h exch def neg }\n",-1);
  Nb_WriteChannel(channel,
                  "  { ceiling sub dup h add floor /h exch def neg }\n",-1);
  Nb_WriteChannel(channel,"  ifelse\n",-1);
  Nb_WriteChannel(channel,"  rmoveto % Adjust corners to device pixels\n",-1);
  Nb_WriteChannel(channel,"  w 0 rlineto\n",-1);
  Nb_WriteChannel(channel,"  0 h rlineto\n",-1);
  Nb_WriteChannel(channel,"  w neg 0 rlineto\n",-1);
  Nb_WriteChannel(channel,"  closepath\n",-1);
  Nb_WriteChannel(channel,"  fill\n",-1);
  Nb_WriteChannel(channel,"  grestore\n",-1);
  Nb_WriteChannel(channel,"} def\n\n",-1);


  // Routines for stipple fill
  Nb_WriteChannel(channel,"/StplRect {"
                  " %stack: r g b stipwidth stipheight"
                  " stipple x y width height\n",
                  -1);
  Nb_WriteChannel(channel,"  gsave\n",-1);
  Nb_WriteChannel(channel,"  newpath\n",-1);
  Nb_WriteChannel(channel,"  4 2 roll moveto\n",-1);
  Nb_WriteChannel(channel,"  exch dup 0 rlineto\n",-1);
  Nb_WriteChannel(channel,"  exch 0 exch rlineto\n",-1);
  Nb_WriteChannel(channel,"  neg 0 rlineto\n",-1);
  Nb_WriteChannel(channel,"  closepath\n",-1);
  Nb_WriteChannel(channel,"  clip\n",-1);
  Nb_WriteChannel(channel,"  /stipple exch def\n",-1);
  Nb_WriteChannel(channel,"  /stipheight exch def\n",-1);
  Nb_WriteChannel(channel,"  /stipwidth exch def\n",-1);
  Nb_WriteChannel(channel,"  setrgbcolor\n",-1);
  Nb_WriteChannel(channel,"  xdevscale ydevscale scale"
                  " % Adjust to device pixel size\n",-1);
  Nb_WriteChannel(channel,"  pathbbox %stack: x1 y1 x2 y2\n",-1);
  Nb_WriteChannel(channel,"  exch ceiling cvi\n",-1);
  Nb_WriteChannel(channel,"  exch ceiling cvi\n",-1);
  Nb_WriteChannel(channel,"  4 2 roll\n",-1);
  Nb_WriteChannel(channel,"  stipheight div floor cvi stipheight mul\n",-1);
  Nb_WriteChannel(channel,"  exch stipwidth div floor cvi stipwidth mul\n",-1);
  Nb_WriteChannel(channel,"  %stack: x2' y2' y1' x1'\n",-1);
  Nb_WriteChannel(channel,"  dup 2 index translate % Shift to stipple base\n",
                  -1);
  Nb_WriteChannel(channel,"  4 1 roll sub /h exch def\n",-1);
  Nb_WriteChannel(channel,"  sub neg 0 stipwidth 3 2 roll {\n",-1);
  Nb_WriteChannel(channel,"    pop\n",-1);
  Nb_WriteChannel(channel,"    stipwidth h true matrix {stipple} imagemask\n",
                  -1);
  Nb_WriteChannel(channel,"    stipwidth 0 translate\n",-1);
  Nb_WriteChannel(channel,"  } for\n",-1);
  Nb_WriteChannel(channel,"  grestore\n} def\n\n",-1);

  // Dotted rectangle
  Nb_WriteChannel(channel,"/DotRect {"
                  " %stack: rI gI bI rO gO bO x y radius\n",-1);
  Nb_WriteChannel(channel,"  2 mul dup\n",-1);
  Nb_WriteChannel(channel,"  ixdevscale mul ceiling cvi /w exch def\n",-1);
  Nb_WriteChannel(channel,"  iydevscale mul ceiling cvi /h exch def\n",-1);
  Nb_WriteChannel(channel,"  gsave\n",-1);
  Nb_WriteChannel(channel,"  newpath\n",-1);
  Nb_WriteChannel(channel,"  moveto\n",-1);
  Nb_WriteChannel(channel,"  xdevscale ydevscale scale"
                  " % Adjust to device pixel size\n",-1);
  Nb_WriteChannel(channel,"  w -2 div h -2 div rmoveto\n",-1);
  Nb_WriteChannel(channel,"  setrgbcolor\n",-1);
  Nb_WriteChannel(channel,"  currentpoint transform\n",-1);
  Nb_WriteChannel(channel,"  exch dup floor sub neg\n",-1);
  Nb_WriteChannel(channel,"  exch dup floor sub neg\n",-1);
  Nb_WriteChannel(channel,"  rmoveto % Move corner to device pixel\n",-1);
  Nb_WriteChannel(channel,"  currentpoint\n",-1);
  Nb_WriteChannel(channel,"  w 0 rlineto\n",-1);
  Nb_WriteChannel(channel,"  0 h rlineto\n",-1);
  Nb_WriteChannel(channel,"  w neg 0 rlineto\n",-1);
  Nb_WriteChannel(channel,"  closepath\n",-1);
  Nb_WriteChannel(channel,"  fill\n",-1);
  Nb_WriteChannel(channel,"  moveto\n",-1);
  Nb_WriteChannel(channel,"  /wI w 3 div round cvi"
                  " dup w add 2 mod add def\n",-1);
  Nb_WriteChannel(channel,"  /hI h 3 div round cvi"
                  " dup w add 2 mod add def\n",-1);
  Nb_WriteChannel(channel,"  w wI sub 2 idiv h hI sub 2 idiv rmoveto\n",-1);
  Nb_WriteChannel(channel,"  wI 0 rlineto\n",-1);
  Nb_WriteChannel(channel,"  0 hI rlineto\n",-1);
  Nb_WriteChannel(channel,"  wI neg 0 rlineto\n",-1);
  Nb_WriteChannel(channel,"  closepath\n",-1);
  Nb_WriteChannel(channel,"  setrgbcolor\n",-1);
  Nb_WriteChannel(channel,"  fill\n",-1);
  Nb_WriteChannel(channel,"  grestore\n",-1);
  Nb_WriteChannel(channel,"} def\n\n",-1);

  // Crossed diamond
  Nb_WriteChannel(channel,"/XDiamond {"
                  " %stack: rX gX bX rD gD bD x y radius\n",-1);
  Nb_WriteChannel(channel,"  dup\n",-1);
  Nb_WriteChannel(channel,"  ixdevscale mul ceiling cvi /rx exch def\n",-1);
  Nb_WriteChannel(channel,"  iydevscale mul ceiling cvi /ry exch def\n",-1);
  Nb_WriteChannel(channel,"  gsave\n",-1);
  Nb_WriteChannel(channel,"  newpath\n",-1);
  Nb_WriteChannel(channel,"  moveto\n",-1);
  Nb_WriteChannel(channel,"  setrgbcolor\n",-1);
  Nb_WriteChannel(channel,"  xdevscale ydevscale scale"
                  " % Adjust to device pixel size\n",-1);
  Nb_WriteChannel(channel,"  currentpoint transform\n",-1);
  Nb_WriteChannel(channel,"  exch dup floor sub neg\n",-1);
  Nb_WriteChannel(channel,"  exch dup floor sub neg\n",-1);
  Nb_WriteChannel(channel,"  rmoveto % Move center to device pixel\n",-1);
  Nb_WriteChannel(channel,"  currentpoint\n",-1);
  Nb_WriteChannel(channel,"  rx neg 0 rmoveto\n",-1);
  Nb_WriteChannel(channel,"  rx ry neg rlineto\n",-1);
  Nb_WriteChannel(channel,"  rx ry rlineto\n",-1);
  Nb_WriteChannel(channel,"  rx neg ry rlineto\n",-1);
  Nb_WriteChannel(channel,"  closepath\n",-1);
  Nb_WriteChannel(channel,"  clip\n",-1);
  Nb_WriteChannel(channel,"  fill\n",-1);
  Nb_WriteChannel(channel,"  ry add moveto\n",-1);
  Nb_WriteChannel(channel,"  0 ry -2 mul rlineto\n",-1);
  Nb_WriteChannel(channel,"  rx neg ry rmoveto\n",-1);
  Nb_WriteChannel(channel,"  rx 2 mul 0 rlineto\n",-1);
  Nb_WriteChannel(channel,"  setrgbcolor\n",-1);
  Nb_WriteChannel(channel,"  rx ry add 14 div round cvi setlinewidth\n",-1);
  Nb_WriteChannel(channel,"  stroke\n",-1);
  Nb_WriteChannel(channel,"  grestore\n",-1);
  Nb_WriteChannel(channel,"} def\n",-1);
  Nb_WriteChannel(channel,"%%EndProlog\n\n",-1);

  Nb_WriteChannel(channel,"%%BeginSetup\n",-1);
  Nb_WriteChannel(channel,"%%EndSetup\n\n",-1);

  Nb_WriteChannel(channel,"%%Page: 1 1\nsave\n\n",-1);

  unsigned int red,green,blue;
  background_color.Get(red,green,blue);
  Nb_FprintfChannel(channel,NULL,1024,
                    "%% Background and clipping region\n"
                    "%.6g %.6g %.6g setrgbcolor\n",
                    red/255.,green/255.,blue/255.);
  Nb_FprintfChannel(channel,NULL,1024,
                    "%d %d moveto\n"
                    "%d %d lineto\n"
                    "%d %d lineto\n"
                    "%d %d lineto\n"
                    "closepath\nclip\nfill\n\n",
                    bbox_xmin,bbox_ymin,
                    bbox_xmax,bbox_ymin,
                    bbox_xmax,bbox_ymax,
                    bbox_xmin,bbox_ymax);

  if(strcmp("Landscape",page_orientation)==0) {
    Nb_FprintfChannel(channel,NULL,1024,
                      "%.6g %.6g translate\n",
                      static_cast<double>(bbox_xmax),
                      static_cast<double>(bbox_ymin));
    Nb_WriteChannel(channel,"90 rotate\n",-1);
  } else { // Portrait orientation
    Nb_FprintfChannel(channel,NULL,1024,
                      "%.6g %.6g translate\n",
                      static_cast<double>(bbox_xmin),
                      static_cast<double>(bbox_ymin));
  }
  Nb_FprintfChannel(channel,NULL,1024,
                    "%.6g %.6g scale\n",
                    static_cast<double>(scale),
                    static_cast<double>(-1*scale));
  Nb_FprintfChannel(channel,NULL,1024,
                    "%.6g %.6g translate\n\n",
                    static_cast<double>(-xmin),
                    static_cast<double>(-ymax));

  Nb_WriteChannel(channel,"% Device pixel dimensions\n",-1);
  Nb_WriteChannel(channel,"1 1 idtransform /ydevscale exch def"
                  " /xdevscale exch def\n",-1);
  Nb_WriteChannel(channel,"/ixdevscale 1. xdevscale div def\n",-1);
  Nb_WriteChannel(channel,"/iydevscale 1. ydevscale div def\n\n",-1);
}

OommfPSDraw::~OommfPSDraw()
{ // Writes trailer
  Nb_WriteChannel(channel,"\ngrestore\n",-1);
  ApplyMat();
  Nb_WriteChannel(channel,"\nshowpage\n",-1);
  Nb_WriteChannel(channel,"%%Trailer\n%%EOF\n",-1);
  channel=NULL;
}

void
OommfPSDraw::DrawPolyLine
(const Nb_List<PlanePoint>& vlist,
 OC_REAL8m linescale,
 OommfPackedRGB color,
 int joinstyle)
{ // Draws (potentially fat) line segments between each successive pair
  // of points in vlist.  If joinstyle == 0, then successive segments
  // are connected with a mitered join, == 1 gives a rounded joint, and
  // == 2 gives a beveled join.  In general, this is an unclosed figure,
  // unless the last and first point are equal, in which case those ends
  // will be joined using joinstyle.

  OC_INDEX pointcount = vlist.GetSize();
  if(pointcount<2) return; // Nothing to draw

  const PlanePoint *pta,*ptb;

  Nb_List_Index<PlanePoint> key;
  pta=vlist.GetFirst(key);

  Nb_FprintfChannel(channel,NULL,1024,
                    "%d setlinejoin\n"
                    "newpath\n%.6g %.6g DevHalfAdj moveto\n",
                    joinstyle,
                    static_cast<double>(pta->x),
                    static_cast<double>(pta->y));

  OC_INDEX pointindex=1;
  while((ptb=vlist.GetNext(key))!=NULL) {
    ++pointindex;
    if(pointindex==pointcount && (*ptb)==(*pta)) {
      Nb_WriteChannel(channel,"closepath\n",-1);
    } else {
      Nb_FprintfChannel(channel,NULL,1024,
                        "%.6g %.6g DevHalfAdj lineto\n",
                        static_cast<double>(ptb->x),
                        static_cast<double>(ptb->y));
    }
  }
  unsigned int red,green,blue;
  color.Get(red,green,blue);
  Nb_FprintfChannel(channel,NULL,1024,
                    "%.6g %.6g %.6g setrgbcolor\n",
                    red/255.,green/255.,blue/255.);
  Nb_FprintfChannel(channel,NULL,1024,"%.6g setlinewidth\n",
                    static_cast<double>(linescale));
  // Render line, and then reset join style to default (mitered).
  Nb_WriteChannel(channel,"stroke\n0 setlinejoin\n",-1);
}

void
OommfPSDraw::DrawFilledArrow
(OC_REAL8m xc,OC_REAL8m yc,
 OC_REAL8m size,
 OC_REAL8m xcos,OC_REAL8m ycos,OC_REAL8m zcos,
 OommfPackedRGB color)
{ // Draws arrow centered at point (xc,yc) in bitmap, of specified size,
  // with directional cosines xcos,ycos,zcos, in the specified color.
  unsigned int red,green,blue;
  color.Get(red,green,blue);
  Nb_FprintfChannel(channel,NULL,1024,
                    "%.6g %.6g %.6g %.6g %.6g"
                    " %.6g %.6g %.6g MakeArrow\n",
                    red/255.,green/255.,blue/255.,
                    static_cast<double>(size*sqrt(1.-zcos*zcos)),
                    static_cast<double>(size),
                    static_cast<double>(Oc_Atan2(ycos,xcos)*180./PI),
                    static_cast<double>(xc),
                    static_cast<double>(yc));
}

void
OommfPSDraw::DrawFilledRectangle
(OC_REAL8m x1,OC_REAL8m y1,
 OC_REAL8m x2,OC_REAL8m y2,
 OommfPackedRGB color,
 const char* transparency_stipple)
{ // Opposing vertices (x1,y1) and (x2,y2) are included.
  // Specify transparency_stipple to NULL, "" to get no stipple.
  // Other valid transparency_stipple values are
  //       gray12,  grey12
  //       gray25,  grey25
  //       gray50,  grey50
  //       gray75,  grey75
  //       gray100, grey100
  // The last two are equivalent to no stipple.
  // Note: gray12, i.e., 12% stipple, is actually 12.5%,
  // in agreement with the Tk gray12 bitmask.
#define MEMBERNAME "DrawFilledRectangle(4xOC_REAL8m,OommfPackedRGB,const char*)"
  unsigned int red,green,blue;
  color.Get(red,green,blue);
  OC_REAL8m xmin = OC_MIN(x1,x2);
  OC_REAL8m xmax = OC_MAX(x1,x2);
  OC_REAL8m ymin = OC_MIN(y1,y2);
  OC_REAL8m ymax = OC_MAX(y1,y2);
  Nb_FprintfChannel(channel,NULL,1024,"%.6g %.6g %.6g ",
                    red/255.,green/255.,blue/255.);
  if(transparency_stipple==NULL ||
     transparency_stipple[0]=='\0' ||
     strcmp("gray100",transparency_stipple)==0 ||
     strcmp("grey100",transparency_stipple)==0) {
    Nb_FprintfChannel(channel,NULL,1024,
                      "%.6g %.6g %.6g %.6g SolidRect\n",
                      static_cast<double>(xmin),
                      static_cast<double>(ymin),
                      static_cast<double>(xmax-xmin),
                      static_cast<double>(ymax-ymin));
  } else if(strcmp("gray12",transparency_stipple)==0 ||
            strcmp("grey12",transparency_stipple)==0) {
    Nb_FprintfChannel(channel,NULL,1024,
                      "8 4 <00110044> %.6g %.6g %.6g %.6g StplRect\n",
                      static_cast<double>(xmin),
                      static_cast<double>(ymin),
                      static_cast<double>(xmax-xmin),
                      static_cast<double>(ymax-ymin));
  } else if(strcmp("gray25",transparency_stipple)==0 ||
            strcmp("grey25",transparency_stipple)==0) {
    Nb_FprintfChannel(channel,NULL,1024,
                      "8 2 <1144> %.6g %.6g %.6g %.6g StplRect\n",
                      static_cast<double>(xmin),
                      static_cast<double>(ymin),
                      static_cast<double>(xmax-xmin),
                      static_cast<double>(ymax-ymin));
  } else if(strcmp("gray50",transparency_stipple)==0 ||
            strcmp("grey50",transparency_stipple)==0) {
    Nb_FprintfChannel(channel,NULL,1024,
                      "8 2 <55AA> %.6g %.6g %.6g %.6g StplRect\n",
                      static_cast<double>(xmin),
                      static_cast<double>(ymin),
                      static_cast<double>(xmax-xmin),
                      static_cast<double>(ymax-ymin));
  } else if(strcmp("gray75",transparency_stipple)==0 ||
            strcmp("grey75",transparency_stipple)==0) {
    Nb_FprintfChannel(channel,NULL,1024,
                      "8 2 <77DD> %.6g %.6g %.6g %.6g StplRect\n",
                      static_cast<double>(xmin),
                      static_cast<double>(ymin),
                      static_cast<double>(xmax-xmin),
                      static_cast<double>(ymax-ymin));
  } else {
    NonFatalError(STDDOC,"Unsupported stipple pattern: \"%s\";"
                  "  Using solid fill.",
                  transparency_stipple);
    Nb_FprintfChannel(channel,NULL,1024,
                      "%.6g %.6g %.6g %.6g SolidRect\n",
                      static_cast<double>(xmin),
                      static_cast<double>(ymin),
                      static_cast<double>(xmax-xmin),
                      static_cast<double>(ymax-ymin));
  }
#undef MEMBERNAME
}

void
OommfPSDraw::DrawDiamondWithCross
(OC_REAL8m xc,OC_REAL8m yc,OC_REAL8m size,
 OommfPackedRGB outercolor,
 OommfPackedRGB innercolor)
{ // Symbol representing out-of-plane vectors
#define MEMBERNAME "DrawDiamondWithCross(3xOC_REAL8m,2xOommfPackedRGB)"
  unsigned int red,green,blue;
  innercolor.Get(red,green,blue);
  Nb_FprintfChannel(channel,NULL,1024,"%.6g %.6g %.6g ",
                    red/255.,green/255.,blue/255.);
  outercolor.Get(red,green,blue);
  Nb_FprintfChannel(channel,NULL,1024,"%.6g %.6g %.6g ",
                    red/255.,green/255.,blue/255.);
  Nb_FprintfChannel(channel,NULL,1024,
                    "%.6g %.6g %.6g XDiamond\n",
                    static_cast<double>(xc),
                    static_cast<double>(yc),
                    static_cast<double>(InOutTipRadius*size*SQRT2));
#undef MEMBERNAME
}

void
OommfPSDraw::DrawSquareWithDot
(OC_REAL8m xc,OC_REAL8m yc,OC_REAL8m size,
 OommfPackedRGB outercolor,
 OommfPackedRGB innercolor)
{ // Symbol representing in-to-plane vectors.
#define MEMBERNAME "DrawSquareWithDot(3xOC_REAL8m,2xOommfPackedRGB)"
  unsigned int red,green,blue;
  innercolor.Get(red,green,blue);
  Nb_FprintfChannel(channel,NULL,1024,"%.6g %.6g %.6g ",
                    red/255.,green/255.,blue/255.);
  outercolor.Get(red,green,blue);
  Nb_FprintfChannel(channel,NULL,1024,"%.6g %.6g %.6g ",
                    red/255.,green/255.,blue/255.);
  Nb_FprintfChannel(channel,NULL,1024,
                    "%.6g %.6g %.6g DotRect\n",
                    static_cast<double>(xc),
                    static_cast<double>(yc),
                    static_cast<double>(InOutTipRadius*size));
#undef MEMBERNAME
}

void
OommfPSDraw::ApplyMat()
{ // Lays flat mat around border of specified mat width
  // and color.
#define MEMBERNAME "ApplyMat(OC_REAL8m,OommfPackedRGB)"
  if(mat_width<=0.0) return;
  unsigned int red,green,blue;
  mat_color.Get(red,green,blue);
  Nb_WriteChannel(channel,"\n% Draw mat\n",-1);
  Nb_WriteChannel(channel,"gsave\nnewpath\n",-1);
  Nb_FprintfChannel(channel,NULL,1024,"%.6g %.6g %.6g setrgbcolor\n",
                    red/255.,green/255.,blue/255.);
  Nb_FprintfChannel(channel,NULL,1024,"%.6g %.6g moveto\n",
                    static_cast<double>(bbox_xmin),
                    static_cast<double>(bbox_ymin));
  Nb_FprintfChannel(channel,NULL,1024,"%.6g %.6g lineto\n",
                    static_cast<double>(bbox_xmax),
                    static_cast<double>(bbox_ymin));
  Nb_FprintfChannel(channel,NULL,1024,"%.6g %.6g lineto\n",
                    static_cast<double>(bbox_xmax),
                    static_cast<double>(bbox_ymax));
  Nb_FprintfChannel(channel,NULL,1024,"%.6g %.6g lineto\n",
                    static_cast<double>(bbox_xmin),
                    static_cast<double>(bbox_ymax));
  Nb_FprintfChannel(channel,NULL,1024,"%.6g %.6g lineto\n",
                    static_cast<double>(bbox_xmin),
                    static_cast<double>(bbox_ymin));
  Nb_FprintfChannel(channel,NULL,1024,"%.6g %.6g lineto\n",
                    static_cast<double>(bbox_xmin+mat_width),
                    static_cast<double>(bbox_ymin));
  Nb_FprintfChannel(channel,NULL,1024,"%.6g %.6g lineto\n",
                    static_cast<double>(bbox_xmin+mat_width),
                    static_cast<double>(bbox_ymax-mat_width));
  Nb_FprintfChannel(channel,NULL,1024,"%.6g %.6g lineto\n",
                    static_cast<double>(bbox_xmax-mat_width),
                    static_cast<double>(bbox_ymax-mat_width));
  Nb_FprintfChannel(channel,NULL,1024,"%.6g %.6g lineto\n",
                    static_cast<double>(bbox_xmax-mat_width),
                    static_cast<double>(bbox_ymin+mat_width));
  Nb_FprintfChannel(channel,NULL,1024,"%.6g %.6g lineto\n",
                    static_cast<double>(bbox_xmin),
                    static_cast<double>(bbox_ymin+mat_width));
  Nb_WriteChannel(channel,"closepath\nfill\ngrestore\n",-1);
#undef MEMBERNAME
}
