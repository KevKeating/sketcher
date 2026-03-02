#pragma once

#include <QBrush>
#include <QGraphicsItem>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QString>

#include "schrodinger/sketcher/definitions.h"
#include "schrodinger/sketcher/molviewer/fonts.h"
#include "schrodinger/sketcher/rdkit/monomeric.h"

namespace schrodinger
{
namespace sketcher
{

class AbstractMonomerItem;

QRectF get_bounding_rect_for_unbound_monomer_attachment_point_item(
    const UnboundAttachmentPoint& attachment_point,
    const AbstractMonomerItem* const parent_monomer, const Fonts& fonts);

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
        AbstractMonomerItem* parent_monomer, const Fonts& fonts);

    enum { Type = QGraphicsItem::UserType + 2000 };
    int type() const override;

    /**
     * Set whether this attachment point indicator is active (black) or
     * inactive (gray).
     * @param active true for black coloring, false for gray
     */
    void setActive(bool active);

    bool withinHoverArea(const QPointF& scene_pos) const;
    const UnboundAttachmentPoint& getAttachmentPoint() const;

    // QGraphicsItem overrides
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

  private:
    // Data references
    UnboundAttachmentPoint m_attachment_point;
    // TODO: this could be a pointer to the scene tool's fonts
    Fonts m_fonts;

    // Cached geometry (line always starts from origin)
    QPointF m_line_end;
    QRectF m_label_rect;
    QString m_label_text;
    QRectF m_bounding_rect;
    QPainterPath m_hover_area;

    // Drawing resources
    QPen m_line_pen;
    QBrush m_circle_brush{Qt::SolidPattern};

    // State
    bool m_is_active = false;

    /**
     * Update pen and brush colors based on active state.
     */
    void updateColors();

    /**
     * Update all cached geometry and visual data
     */
    void calculateGeometry(const AbstractMonomerItem* parent_monomer);
};

} // namespace sketcher
} // namespace schrodinger
