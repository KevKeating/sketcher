#pragma once

#include <QBrush>
#include <QGraphicsItem>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QString>

#include "schrodinger/sketcher/definitions.h"
#include "schrodinger/sketcher/rdkit/monomeric.h"

namespace schrodinger
{
namespace sketcher
{

class AbstractMonomerItem;
class Fonts;

/**
 * A graphics item for representing an unbound (available) attachment point
 * on a monomer. Draws a short line extending from the monomer center outward
 * in the specified direction, a small filled circle at the end, and a text
 * label indicating the attachment point name.
 *
 * The item is drawn behind its parent monomer using ItemStacksBehindParent,
 * so the monomer's filled border naturally hides the portion of the line
 * inside the monomer.
 */
class SKETCHER_API UnboundMonomericAttachmentPointItem : public QGraphicsItem
{
  public:
    /**
     * @param attachment_point The attachment point data (name, direction)
     * @param parent_monomer The parent monomer item
     * @param fonts The fonts object for label rendering
     */
    UnboundMonomericAttachmentPointItem(
        const UnboundAttachmentPoint& attachment_point,
        AbstractMonomerItem& parent_monomer,
        const Fonts& fonts);

    /**
     * Set whether this attachment point indicator is active (black) or
     * inactive (gray).
     * @param active true for black coloring, false for gray
     */
    void setActive(bool active);

    /**
     * @return whether this attachment point indicator is currently active
     */
    bool isActive() const;

    /**
     * Update all cached geometry and visual data. Should be called when
     * the parent monomer's geometry changes.
     */
    void updateCachedData();

    // QGraphicsItem overrides
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

  private:
    /**
     * Position the label rect appropriately based on direction.
     */
    void positionLabelRect();

    /**
     * Update pen and brush colors based on active state.
     */
    void updateColors();

    // Data references
    const UnboundAttachmentPoint& m_attachment_point;
    AbstractMonomerItem& m_parent_monomer;
    const Fonts& m_fonts;

    // Cached geometry (line always starts from origin)
    QPointF m_line_end;
    QRectF m_label_rect;
    QString m_label_text;
    QRectF m_bounding_rect;

    // Drawing resources
    QPen m_line_pen;
    QBrush m_circle_brush;

    // State
    bool m_is_active = false;
};

/**
 * Convert a Direction enum value to a unit vector in scene coordinates.
 * Note: Y-axis is inverted in Qt (positive Y goes down), so N is (0, -1).
 * @param dir The direction
 * @return Unit vector pointing in that direction
 */
SKETCHER_API QPointF direction_to_unit_vector(Direction dir);

} // namespace sketcher
} // namespace schrodinger
