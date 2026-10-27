#include "schrodinger/sketcher/molviewer/halo_highlighting_item.h"

#include <QBrush>
#include <QPen>
#include <QPainter>

#include "schrodinger/sketcher/molviewer/atom_item.h"
#include "schrodinger/sketcher/molviewer/constants.h"

namespace schrodinger
{
namespace sketcher
{

HaloHighlightingItem::HaloHighlightingItem(qreal z_value) :
    AbstractHighlightingItem()
{
    setPen(SELECTION_OUTLINE_COLOR);
    setBrush(SELECTION_BACKGROUND_COLOR);
    setZValue(z_value);
}

QPainterPath HaloHighlightingItem::getPathForItem(
    const AbstractGraphicsItem* const item) const
{
    auto path = item->selectionHighlightingPath();
    if (const auto* atom_item = dynamic_cast<const AtomItem*>(item)) {
        // Include the complete label, with a rounded margin. Union the paths
        // before painting so overlapping label components retain the halo's
        // requested opacity.
        for (const auto& rect : atom_item->getSubrects()) {
            QPainterPath label_path;
            label_path.addRoundedRect(
                rect.adjusted(-ATOM_LABEL_MARGIN, -ATOM_LABEL_MARGIN,
                              ATOM_LABEL_MARGIN, ATOM_LABEL_MARGIN),
                ATOM_LABEL_MARGIN, ATOM_LABEL_MARGIN);
            path |= label_path;
        }
    }
    return path;
}

} // namespace sketcher
} // namespace schrodinger
