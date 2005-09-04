/*
 *  Path.h
 *  nlivarot
 *
 *  Created by fred on Tue Jun 17 2003.
 *
 */

#ifndef my_path
#define my_path

#include <vector>
#include "LivarotDefs.h"
#include "livarot/livarot-forward.h"
#include "libnr/nr-point.h"

/*
 * the Path class: a structure to hold path description and their polyline approximation (not kept in sync)
 * the path description is built with regular commands like MoveTo() LineTo(), etc
 * the polyline approximation is built by a call to Convert() or its variants
 * another possibility would be to call directly the AddPoint() functions, but that is not encouraged
 * the conversion to polyline can salvage data as to where on the path each polyline's point lies; use
 * ConvertWithBackData() for this. after this call, it's easy to rewind the polyline: sequences of points
 * of the same path command can be reassembled in a command
 */

// polyline description commands
enum
{
  polyline_lineto = 0,  // a lineto 
  polyline_moveto = 1,  // a moveto
  polyline_forced = 2   // a forced point, ie a point that was an angle or an intersection in a previous life
                        // or more realistically a control point in the path description that created the polyline
                        // forced points are used as "breakable" points for the polyline -> cubic bezier patch operations
                        // each time the bezier fitter encounters such a point in the polyline, it decreases its treshhold,
                        // so that it is more likely to cut the polyline at that position and produce a bezier patch
};

class Shape;

// path creation: 2 phases: first the path is given as a succession of commands (MoveTo, LineTo, CurveTo...); then it
// is converted in a polyline
// a polylone can be stroked or filled to make a polygon
class Path
{
  friend class Shape;
    public:

  // flags for the path construction
  enum
  {
    descr_ready = 0,        
    descr_adding_bezier = 1, // we're making a bezier spline, so you can expect  pending_bezier_* to have a value
    descr_doing_subpath = 2, // we're doing a path, so there is a moveto somewhere
    descr_delayed_bezier = 4,// the bezier spline we're doing was initiated by a TempBezierTo(), so we'll need an endpoint
    descr_dirty = 16         // the path description was modified
  };
  
  // some data for the construction: what's pending, and some flags
  int         descr_flags;
  int         pending_bezier_cmd;
  int         pending_bezier_data;
  int         pending_moveto_cmd;
  int         pending_moveto_data;
  // the path description
  std::vector<PathDescr*> descr_cmd;

  // polyline storage: a series of coordinates (and maybe weights)
  // also back data: info on where this polyline's segment comes from, ie wich command in the path description: "piece"
  // and what abcissis on the chunk of path for this command: "t"
  // t=0 means it's at the start of the command's chunk, t=1 it's at the end
  struct path_lineto
  {
    path_lineto(bool m, NR::Point pp) : isMoveTo(m), p(pp), piece(-1), t(0) {}
    path_lineto(bool m, NR::Point pp, int pie, double tt) : isMoveTo(m), p(pp), piece(pie), t(tt) {}
    
    int isMoveTo;
    NR::Point  p;
    int piece;
    double t;
  };
  
  std::vector<path_lineto> pts;

  bool back;

  Path();
  ~Path();

  // creation of the path description
  void Reset();		// reset to the empty description
  void Copy (Path * who);

  // the commands...
  int ForcePoint();
  int Close();
  int MoveTo ( NR::Point const &ip);
  int LineTo ( NR::Point const &ip);
  int CubicTo ( NR::Point const &ip,  NR::Point const &iStD,  NR::Point const &iEnD);
  int ArcTo ( NR::Point const &ip, double iRx, double iRy, double angle, bool iLargeArc, bool iClockwise);
  int IntermBezierTo ( NR::Point const &ip);	// add a quadratic bezier spline control point
  int BezierTo ( NR::Point const &ip);	// quadratic bezier spline to this point (control points can be added after this)
  int TempBezierTo();	// start a quadratic bezier spline (control points can be added after this)
  int EndBezierTo();
  int EndBezierTo ( NR::Point const &ip);	// ends a quadratic bezier spline (for curves started with TempBezierTo)

  // transforms a description in a polyline (for stroking and filling)
  // treshhold is the max length^2 (sort of)
  void Convert (double treshhold);
  void ConvertEvenLines (double treshhold);	// decomposes line segments too, for later recomposition
  // same function for use when you want to later recompose the curves from the polyline
  void ConvertWithBackData (double treshhold);

  // creation of the polyline (you can tinker with these function if you want)
  void SetBackData (bool nVal);	// has back data?
  void ResetPoints(); // resets to the empty polyline
  int AddPoint ( NR::Point const &iPt, bool mvto = false);	// add point
  int AddPoint ( NR::Point const &iPt, int ip, double it, bool mvto = false);
  int AddForcedPoint ( NR::Point const &iPt);	// add point
  int AddForcedPoint ( NR::Point const &iPt, int ip, double it);

  // transform in a polygon (in a graph, in fact; a subsequent call to ConvertToShape is needed)
  //  - fills the polyline; justAdd=true doesn't reset the Shape dest, but simply adds the polyline into it
  // closeIfNeeded=false prevent the function from closing the path (resulting in a non-eulerian graph
  // pathID is a identification number for the path, and is used for recomposing curves from polylines
  // give each different Path a different ID, and feed the appropriate orig[] to the ConvertToForme() function
  void Fill (Shape * dest, int pathID = -1, bool justAdd =
	     false, bool closeIfNeeded = true, bool invert = false);

  // - stroke the path; usual parameters: type of cap=butt, type of join=join and miter (see LivarotDefs.h)
  // doClose treat the path as closed (ie a loop)
  void Stroke (Shape * dest, bool doClose, double width, JoinType join,
	       ButtType butt, double miter, bool justAdd = false);

  // build a Path that is the outline of the Path instance's description (the result is stored in dest)
  // it doesn't compute the exact offset (it's way too complicated, but an approximation made of cubic bezier patches
  //  and segments. the algorithm was found in a plugin for Impress (by Chris Cox), but i can't find it back...
  void Outline (Path * dest, double width, JoinType join, ButtType butt,
		double miter);

  // half outline with edges having the same direction as the original
  void OutsideOutline (Path * dest, double width, JoinType join, ButtType butt,
		       double miter);

  // half outline with edges having the opposite direction as the original
  void InsideOutline (Path * dest, double width, JoinType join, ButtType butt,
		      double miter);

  // polyline to cubic bezier patches
  void Simplify (double treshhold);

  // description simplification
  void Coalesce (double tresh);

  // utilities
  // piece is a command no in the command list
  // "at" is an abcissis on the path portion associated with this command
  // 0=beginning of portion, 1=end of portion.
  void PointAt (int piece, double at, NR::Point & pos);
  void PointAndTangentAt (int piece, double at, NR::Point & pos, NR::Point & tgt);

  // last control point before the command i (i included)
  // used when dealing with quadratic bezier spline, cause these can contain arbitrarily many commands
  const NR::Point PrevPoint (const int i) const;
  
  // dash the polyline
  // the result is stored in the polyline, so you lose the original. make a copy before if needed
  void  DashPolyline(float head,float tail,float body,int nbD,float *dashs,bool stPlain,float stOffset);
  
  //utilitaire pour inkscape
  void  LoadArtBPath(void *iP,NR::Matrix const &tr,bool doTransformation);
	void* MakeArtBPath();
	
	void  Transform(const NR::Matrix &trans);
  
  // decompose le chemin en ses sous-chemin
  // killNoSurf=true -> oublie les chemins de surface nulle
  Path**      SubPaths(int &outNb,bool killNoSurf);
  // pour recuperer les trous
  // nbNest= nombre de contours
  // conts= debut de chaque contour
  // nesting= parent de chaque contour
  Path**      SubPathsWithNesting(int &outNb,bool killNoSurf,int nbNest,int* nesting,int* conts);
  // surface du chemin (considere comme ferme)
  double      Surface();
  void        PolylineBoundingBox(double &l,double &t,double &r,double &b);
  void        FastBBox(double &l,double &t,double &r,double &b);
  // longueur (totale des sous-chemins)
  double      Length();
  
  void             ConvertForcedToMoveTo();
  void             ConvertForcedToVoid();
  struct cut_position {
    int          piece;
    double        t;
  };
  cut_position*    CurvilignToPosition(int nbCv,double* cvAbs,int &nbCut);
  cut_position    PointToCurvilignPosition(NR::Point const &pos) const;
  //Should this take a cut_position as a param?
  double           Path::PositionToLength(int piece, double t);
  
  // caution: not tested on quadratic b-splines, most certainly buggy
  void             ConvertPositionsToMoveTo(int nbPos,cut_position* poss);
  void             ConvertPositionsToForced(int nbPos,cut_position* poss);

  void  Affiche();
  char *svg_dump_path() const;

    private:
  // utilitary functions for the path contruction
  void CancelBezier ();
  void CloseSubpath();
  void InsertMoveTo (NR::Point const &iPt,int at);
  void InsertForcePoint (int at);
  void InsertLineTo (NR::Point const &iPt,int at);
  void InsertArcTo (NR::Point const &ip, double iRx, double iRy, double angle, bool iLargeArc, bool iClockwise,int at);
  void InsertCubicTo (NR::Point const &ip,  NR::Point const &iStD,  NR::Point const &iEnD,int at);
  void InsertBezierTo (NR::Point const &iPt,int iNb,int at);
  void InsertIntermBezierTo (NR::Point const &iPt,int at);
  
  // creation of dashes: take the polyline given by spP (length spL) and dash it according to head, body, etc. put the result in
  // the polyline of this instance
  void DashSubPath(int spL, int spP, std::vector<path_lineto> const &orig_pts, float head,float tail,float body,int nbD,float *dashs,bool stPlain,float stOffset);

  // Functions used by the conversion.
  // they append points to the polyline
  void DoArc ( NR::Point const &iS,  NR::Point const &iE, double rx, double ry,
	      double angle, bool large, bool wise, double tresh);
  void RecCubicTo ( NR::Point const &iS,  NR::Point const &iSd,  NR::Point const &iE,  NR::Point const &iEd, double tresh, int lev,
		   double maxL = -1.0);
  void RecBezierTo ( NR::Point const &iPt,  NR::Point const &iS,  NR::Point const &iE, double treshhold, int lev, double maxL = -1.0);

  void DoArc ( NR::Point const &iS,  NR::Point const &iE, double rx, double ry,
	      double angle, bool large, bool wise, double tresh, int piece);
  void RecCubicTo ( NR::Point const &iS,  NR::Point const &iSd,  NR::Point const &iE,  NR::Point const &iEd, double tresh, int lev,
		   double st, double et, int piece);
  void RecBezierTo ( NR::Point const &iPt,  NR::Point const &iS, const  NR::Point &iE, double treshhold, int lev, double st, double et,
		    int piece);

  // don't pay attention
  struct offset_orig
  {
    Path *orig;
    int piece;
    double tSt, tEn;
    double off_dec;
  };
  void DoArc ( NR::Point const &iS,  NR::Point const &iE, double rx, double ry,
	      double angle, bool large, bool wise, double tresh, int piece,
	      offset_orig & orig);
  void RecCubicTo ( NR::Point const &iS,  NR::Point const &iSd,  NR::Point const &iE,  NR::Point const &iEd, double tresh, int lev,
		   double st, double et, int piece, offset_orig & orig);
  void RecBezierTo ( NR::Point const &iPt,  NR::Point const &iS,  NR::Point const &iE, double treshhold, int lev, double st, double et,
		    int piece, offset_orig & orig);

  static void ArcAngles ( NR::Point const &iS,  NR::Point const &iE, double rx,
                         double ry, double angle, bool large, bool wise,
                         double &sang, double &eang);
  static void QuadraticPoint (double t,  NR::Point &oPt,   NR::Point const &iS,   NR::Point const &iM,   NR::Point const &iE);
  static void CubicTangent (double t,  NR::Point &oPt,  NR::Point const &iS,
			     NR::Point const &iSd,  NR::Point const &iE,
			     NR::Point const &iEd);

  struct outline_callback_data
  {
    Path *orig;
    int piece;
    double tSt, tEn;
    Path *dest;
    double x1, y1, x2, y2;
    union
    {
      struct
      {
	double dx1, dy1, dx2, dy2;
      }
      c;
      struct
      {
	double mx, my;
      }
      b;
      struct
      {
	double rx, ry, angle;
	bool clock, large;
	double stA, enA;
      }
      a;
    }
    d;
  };

  typedef void (outlineCallback) (outline_callback_data * data, double tol,  double width);
  struct outline_callbacks
  {
    outlineCallback *cubicto;
    outlineCallback *bezierto;
    outlineCallback *arcto;
  };

  void SubContractOutline (int off, int num_pd,
			   Path * dest, outline_callbacks & calls,
			   double tolerance, double width, JoinType join,
			   ButtType butt, double miter, bool closeIfNeeded,
			   bool skipMoveto, NR::Point & lastP, NR::Point & lastT);
  void DoStroke(int off, int N, Shape *dest, bool doClose, double width, JoinType join,
		ButtType butt, double miter, bool justAdd = false);

  static void TangentOnSegAt(double at, NR::Point const &iS, PathDescrLineTo const &fin,
			     NR::Point &pos, NR::Point &tgt, double &len);
  static void TangentOnArcAt(double at, NR::Point const &iS, PathDescrArcTo const &fin,
			     NR::Point &pos, NR::Point &tgt, double &len, double &rad);
  static void TangentOnCubAt (double at, NR::Point const &iS, PathDescrCubicTo const &fin, bool before,
			      NR::Point &pos, NR::Point &tgt, double &len, double &rad);
  static void TangentOnBezAt (double at, NR::Point const &iS,
			      PathDescrIntermBezierTo & mid,
			      PathDescrBezierTo & fin, bool before,
			      NR::Point & pos, NR::Point & tgt, double &len, double &rad);
  static void OutlineJoin (Path * dest, NR::Point pos, NR::Point stNor, NR::Point enNor,
			   double width, JoinType join, double miter);

  static bool IsNulCurve (std::vector<PathDescr*> const &cmd, int curD, NR::Point const &curX);

  static void RecStdCubicTo (outline_callback_data * data, double tol,
			     double width, int lev);
  static void StdCubicTo (outline_callback_data * data, double tol,
			  double width);
  static void StdBezierTo (outline_callback_data * data, double tol,
			   double width);
  static void RecStdArcTo (outline_callback_data * data, double tol,
			   double width, int lev);
  static void StdArcTo (outline_callback_data * data, double tol, double width);


  // fonctions annexes pour le stroke
  static void DoButt (Shape * dest, double width, ButtType butt, NR::Point pos,
		      NR::Point dir, int &leftNo, int &rightNo);
  static void DoJoin (Shape * dest, double width, JoinType join, NR::Point pos,
		      NR::Point prev, NR::Point next, double miter, double prevL,
		      double nextL, int *stNo, int *enNo);
  static void DoLeftJoin (Shape * dest, double width, JoinType join, NR::Point pos,
			  NR::Point prev, NR::Point next, double miter, double prevL,
			  double nextL, int &leftStNo, int &leftEnNo,int pathID=-1,int pieceID=0,double tID=0.0);
  static void DoRightJoin (Shape * dest, double width, JoinType join, NR::Point pos,
			   NR::Point prev, NR::Point next, double miter, double prevL,
			   double nextL, int &rightStNo, int &rightEnNo,int pathID=-1,int pieceID=0,double tID=0.0);
    static void RecRound (Shape * dest, int sNo, int eNo,
            NR::Point const &iS, NR::Point const &iE,
            NR::Point const &nS, NR::Point const &nE,
            NR::Point &origine,float width);


  void DoSimplify(int off, int N, double treshhold);
  bool AttemptSimplify(int off, int N, double treshhold, PathDescrCubicTo &res, int &worstP);
  static bool FitCubic(NR::Point const &start,
		       PathDescrCubicTo &res,
		       double *Xk, double *Yk, double *Qk, double *tk, int nbPt);
  
  struct fitting_tables {
    int      nbPt,maxPt,inPt;
    double   *Xk;
    double   *Yk;
    double   *Qk;
    double   *tk;
    double   *lk;
    char     *fk;
    double   totLen;
  };
  bool   AttemptSimplify (fitting_tables &data,double treshhold, PathDescrCubicTo & res,int &worstP);
  bool   ExtendFit(int off, int N, fitting_tables &data,double treshhold, PathDescrCubicTo & res,int &worstP);
  double RaffineTk (NR::Point pt, NR::Point p0, NR::Point p1, NR::Point p2, NR::Point p3, double it);
  void   FlushPendingAddition(Path* dest,PathDescr *lastAddition,PathDescrCubicTo &lastCubic,int lastAD);
};
#endif

/*
   Local Variables:
mode:c++
c-file-style:"stroustrup"
c-file-offsets:((innamespace . 0)(inline-open . 0))
indent-tabs-mode:nil
fill-column:99
End:
 */
// vim: filetype=c++:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
