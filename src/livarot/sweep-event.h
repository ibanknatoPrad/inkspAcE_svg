#ifndef INKSCAPE_LIVAROT_SWEEP_EVENT_H
#define INKSCAPE_LIVAROT_SWEEP_EVENT_H

class SweepEvent;
class SweepTree;

// the structure to hold the intersections events encountered during the sweep
// it's an array of SweepEvent (not allocated with "new SweepEvent[n]" but with a malloc)
// there's a list of indices because it's a binary heap: inds[i] tell that events[inds[i]] has position i in the
// heap
// each SweepEvent has a field to store its index in the heap, too
typedef struct SweepEventQueue
{
  int nbEvt, maxEvt;		// number of events currently in the heap, allocated size of the heap
  int *inds;			// indices
  SweepEvent *events;		// events
}
SweepEventQueue;

// one intersection event
class SweepEvent
{
public:
  SweepTree * leftSweep;	// sweep element associated with the left edge of the intersection
  SweepTree *rightSweep;	// sweep element associated with the right edge 

  NR::Point posx;		// coordinates of the intersection
  double tl, tr;			// coordinates of the intersection on the left edge (tl) and on the right edge (tr)

  int ind;			// index in the binary heap

    SweepEvent (void);		// not used
   ~SweepEvent (void);		// not used

  // inits a SweepEvent structure
   void MakeNew (SweepTree * iLeft, SweepTree * iRight, NR::Point &iPt,
		double itl, double itr);
  // voids a SweepEvent structure
  void MakeDelete (void);

  // create the structure to store the binary heap
  static void CreateQueue (SweepEventQueue & queue, int size);
  // destroy the structure
  static void DestroyQueue (SweepEventQueue & queue);
  // add one intersection in the binary heap
  static SweepEvent *AddInQueue (SweepTree * iLeft, SweepTree * iRight,
				 NR::Point &iPt, double itl, double itr,
				 SweepEventQueue & queue);
  // the calling SweepEvent removes itself from the binary heap
  void SupprFromQueue (SweepEventQueue & queue);
  // look for the topmost intersection in the heap
  static bool PeekInQueue (SweepTree * &iLeft, SweepTree * &iRight, NR::Point &oPt, double &itl, double &itr,
			   SweepEventQueue & queue);
  // extract the topmost intersection from the heap
  static bool ExtractFromQueue (SweepTree * &iLeft, SweepTree * &iRight,
				NR::Point &oPt, double &itl, double &itr,
				SweepEventQueue & queue);

  // misc: change a SweepEvent structure's postion in the heap
  void Relocate (SweepEventQueue & queue, int to);
};

#endif
