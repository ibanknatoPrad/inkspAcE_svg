// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 * David Yip <yipdw@rose-hulman.edu>
 *
 * Copyright (c) 2005 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_COMPOSITE_UNDO_COMMIT_OBSERVER_H
#define SEEN_COMPOSITE_UNDO_COMMIT_OBSERVER_H

#include "inkgc/gc-alloc.h"
#include "undo-stack-observer.h"

#include <list>

namespace Inkscape {

struct Event;

/**
 * Aggregates UndoStackObservers for management and triggering in an SPDocument's undo/redo
 * system.
 *
 * Heavily inspired by Inkscape::XML::CompositeNodeObserver.
 */
class CompositeUndoStackObserver : public UndoStackObserver {
public:

	/**
	 * Structure for tracking UndoStackObservers.
	 */
	struct UndoStackObserverRecord {
	public:
		/**
		 * Constructor.
		 *
		 * \param o Reference to the UndoStackObserver that this UndoStackObserverRecord
		 * will track.
		 */
		UndoStackObserverRecord(UndoStackObserver& o) : to_remove(false), _observer(o) { }
		bool to_remove;

		/**
		 * Overloaded equality test operator to facilitate usage of STL find algorithms.
		 */
		bool operator==(UndoStackObserverRecord const& _x) const
		{
			return &(this->_observer) == &(_x._observer);
		}

		/**
		 * Issue a redo event to the UndoStackObserver that is associated with this UndoStackObserverRecord.
		 *
		 * \param log The event log generated by the redo event.
		 */
		void issueRedo(Event* log)
		{
			this->_observer.notifyRedoEvent(log);
		}

		/**
		 * Issue an undo event to the UndoStackObserver that is associated with this
		 * UndoStackObserverRecord.
		 *
		 * \param log The event log generated by the undo event.
		 */
		void issueUndo(Event* log)
		{
			this->_observer.notifyUndoEvent(log);
		}

		/**
		 * Issues a committed event to the UndoStackObserver that is associated with this
		 * UndoStackObserverRecord.
		 *
		 * \param log The event log being committed to the undo stack.
		 */
		void issueUndoCommit(Event* log)
		{
			this->_observer.notifyUndoCommitEvent(log);
		}

		/**
		 * Issue a clear undo event to the UndoStackObserver
		 * that is associated with this
		 * UndoStackObserverRecord.
		 */
		void issueClearUndo()
		{
			this->_observer.notifyClearUndoEvent();
		}
	     
		/**
		 * Issue a clear redo event to the UndoStackObserver
		 * that is associated with this
		 * UndoStackObserverRecord.
		 */
		void issueClearRedo()
		{
			this->_observer.notifyClearRedoEvent();
		}

	private:
		UndoStackObserver& _observer;
	};

	/// A list of UndoStackObserverRecords, used to aggregate multiple UndoStackObservers.
	typedef std::list< UndoStackObserverRecord, GC::Alloc< UndoStackObserverRecord, GC::MANUAL > > UndoObserverRecordList;

	/**
	 * Constructor.
	 */
	CompositeUndoStackObserver();

    ~CompositeUndoStackObserver() override;

	/**
	 * Add an UndoStackObserver.
	 *
	 * \param observer Reference to an UndoStackObserver to add.
	 */
	void add(UndoStackObserver& observer);

	/**
	 * Remove an UndoStackObserver.
	 *
	 * \param observer Reference to an UndoStackObserver to remove.
	 */
	void remove(UndoStackObserver& observer);

	/**
	 * Notify all registered UndoStackObservers of an undo event.
	 *
	 * \param log The event log generated by the undo event.
	 */
	void notifyUndoEvent(Event* log) override;

	/**
	 * Notify all registered UndoStackObservers of a redo event.
	 *
	 * \param log The event log generated by the redo event.
	 */
	void notifyRedoEvent(Event* log) override;

	/**
	 * Notify all registered UndoStackObservers of an event log being committed to the undo stack.
	 *
	 * \param log The event log being committed to the undo stack.
	 */
	void notifyUndoCommitEvent(Event* log) override;

	void notifyClearUndoEvent() override;
	void notifyClearRedoEvent() override;

private:
	// Remove an observer from a given list
	bool _remove_one(UndoObserverRecordList& list, UndoStackObserver& rec);

	// Mark an observer for removal from a given list
	bool _mark_one(UndoObserverRecordList& list, UndoStackObserver& rec);

	// Keep track of whether or not we are notifying observers
	unsigned int _iterating;

	// Observers in the active list
	UndoObserverRecordList _active;

	// Observers to be added
	UndoObserverRecordList _pending;

	// Prevents the observer vector from modifications during 
	// iteration through the vector
	void _lock() { this->_iterating++; }
	void _unlock();
};

}

#endif // SEEN_COMPOSITE_UNDO_COMMIT_OBSERVER_H