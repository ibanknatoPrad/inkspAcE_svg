// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef ERASER_TOOL_H_SEEN
#define ERASER_TOOL_H_SEEN

/*
 * Handwriting-like drawing mode
 *
 * Authors:
 *   Mitsuru Oka <oka326@parkcity.ne.jp>
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * The original dynadraw code:
 *   Paul Haeberli <paul@sgi.com>
 *
 * Copyright (C) 1998 The Free Software Foundation
 * Copyright (C) 1999-2002 authors
 * Copyright (C) 2001-2002 Ximian, Inc.
 * Copyright (C) 2008 Jon A. Cruz
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <2geom/point.h>

#include "message-stack.h"
#include "style.h"
#include "ui/tools/dynamic-base.h"
#include "object/sp-use.h"

namespace Inkscape {
namespace UI {
namespace Tools {

enum class EraserToolMode
{
    DELETE,
    CUT,
    CLIP
};
extern EraserToolMode const DEFAULT_ERASER_MODE;

/** Represents an item to erase */
struct EraseTarget
{
    SPItem *item = nullptr;    ///< Pointer to the item to be erased
    bool was_selected = false; ///< Whether the item was part of selection

    EraseTarget(SPItem *ptr, bool sel)
        : item{ptr}
        , was_selected{sel}
    {}
    inline bool operator==(EraseTarget const &other) const noexcept { return item == other.item; }
};

class EraserTool : public DynamicBase {

private:
    // non-static data:
    EraserToolMode mode = DEFAULT_ERASER_MODE;
    bool nowidth = false;
    std::vector<MessageId> _our_messages;
    SPItem *_acid = nullptr;
    std::vector<SPItem *> _survivers;

    // static data:
    inline static guint32 const trace_color_rgba   = 0xff0000ff; // RGBA red
    inline static SPWindRule const trace_wind_rule = SP_WIND_RULE_EVENODD;

    inline static double const tolerance = 0.1;

    inline static double const epsilon       = 0.5e-6;
    inline static double const epsilon_start = 0.5e-2;
    inline static double const vel_start     = 1e-5;

    inline static double const drag_default = 1.0;
    inline static double const drag_min     = 0.0;
    inline static double const drag_max     = 1.0;

    inline static double const min_pressure     = 0.0;
    inline static double const max_pressure     = 1.0;
    inline static double const default_pressure = 1.0;

    inline static double const min_tilt     = -1.0;
    inline static double const max_tilt     = 1.0;
    inline static double const default_tilt = 0.0;

public:
    // public member functions
    EraserTool(SPDesktop *desktop);
    ~EraserTool() override;
    bool root_handler(GdkEvent *event) final;

    using Error = std::uint64_t;
    inline static Error const ALL_GOOD      = 0x0;
    inline static Error const NON_EXISTENT  = 0x1 << 1;
    inline static Error const NO_AREA_PATH  = 0x1 << 2;
    inline static Error const RASTER_IMAGE  = 0x1 << 3;
    inline static Error const ERROR_GROUP   = 0x1 << 4;

private:
    // private member functions
    void _accumulate();
    bool _apply(Geom::Point p);
    bool _booleanErase(EraseTarget target, bool store_survivers);
    void _brush();
    void _cancel();
    void _clearCurrent();
    void _clearStatusBar();
    void _clipErase(SPItem *item) const;
    void _completeBezier(double tolerance_sq, bool releasing);
    bool _cutErase(EraseTarget target, bool store_survivers);
    bool _doWork();
    void _drawTemporaryBox();
    void _extinput(GdkEvent *event);
    void _failedBezierFallback();
    std::vector<EraseTarget> _filterByCollision(std::vector<EraseTarget> const &items, SPItem *with) const;
    std::vector<EraseTarget> _filterCutEraseables(std::vector<EraseTarget> const &items, bool silent = false);
    std::vector<EraseTarget> _findItemsToErase();
    void _fitAndSplit(bool releasing);
    void _fitDrawLastPoint();
    bool _handleKeypress(GdkEventKey const *key);
    void _handleStrokeStyle(SPItem *item) const;
    SPItem *_insertAcidIntoDocument(SPDocument *document);
    bool _performEraseOperation(std::vector<EraseTarget> const &items_to_erase, bool store_survivers);
    void _removeTemporarySegments();
    void _reset(Geom::Point p);
    void _setStatusBarMessage(char *message);
    void _updateMode();

    static void _generateNormalDist2(double &r1, double &r2);
    static void _addCap(SPCurve &curve, Geom::Point const &pre, Geom::Point const &from, Geom::Point const &to,
                        Geom::Point const &post, double rounding);
    static bool _isStraightSegment(SPItem *path);
    static Error _uncuttableItemType(SPItem *item);
    bool _probeUnlinkCutClonedGroup(EraseTarget &original_target, SPUse* clone, SPGroup* cloned_group,
                                    bool store_survivers = true);
};

} // namespace Tools
} // namespace UI
} // namespace Inkscape

#endif // ERASER_TOOL_H_SEEN

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
