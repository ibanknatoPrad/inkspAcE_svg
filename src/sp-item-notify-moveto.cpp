#include "sp-item-notify-moveto.h"
#include <sp-item.h>
#include <libnr/nr-matrix.h>
#include <libnr/nr-matrix-ops.h>
#include <sp-guide.h>
#include <approx-equal.h>
#include <sp-item-rm-unsatisfied-cns.h>
using std::vector;


/**
 * Called by sp_guide_moveto to indicate that the guide line corresponding to g has been moved, and
 * that consequently this item should move with it.
 *
 * Requires: &exist;[cn &isin; item.constraints] g eq cn.g.
 */
void sp_item_notify_moveto(SPItem &item, SPGuide const &mv_g, int const snappoint_ix,
                           double const position, bool const commit)
{
    g_return_if_fail(SP_IS_ITEM(&item));
    g_return_if_fail( unsigned(snappoint_ix) < 8 );
    NR::Point const dir( mv_g.normal );
    double const dir_lensq(dot(dir, dir));
    g_return_if_fail( dir_lensq != 0 );

    vector<NR::Point> snappoints = sp_item_snappoints(&item);
    g_return_if_fail( snappoint_ix < int(snappoints.size()) );

    double const pos0 = dot(dir, snappoints[snappoint_ix]);
    /* TODO/effic: skip if mv_g is already satisfied. */

    /* Translate along dir to make dot(dir, snappoints(item)[snappoint_ix]) == position. */

    /* Calculation:
       dot(dir, snappoints[snappoint_ix] + s * dir) = position.
       dot(dir, snappoints[snappoint_ix]) + dot(dir, s * dir) = position.
       pos0 + s * dot(dir, dir) = position.
       s * lensq(dir) = position - pos0.
       s = (position - pos0) / dot(dir, dir). */
    NR::translate const tr( ( position - pos0 )
                            * ( dir / dir_lensq ) );
    sp_item_set_i2d_affine(&item, sp_item_i2d_affine(&item) * tr);
    /* TODO: Reget snappoints, check satisfied. */

    if (commit) {
        /* TODO: Consider maintaining a set of dirty items. */

        /* Commit repr. */
        {
            sp_item_write_transform(&item, SP_OBJECT_REPR(&item), item.transform);
        }
        
        sp_item_rm_unsatisfied_cns(item);
#if 0 /* nyi */
        move_cn_to_front(mv_g, snappoint_ix, item.constraints);
        /* Note: If the guideline is connected to multiple snappoints of this
           item, then keeping those cns in order requires that the guide
           send notifications in order of increasing importance. */
#endif
    }
}


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
