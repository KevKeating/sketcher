#include "schrodinger/sketcher/molviewer/unbound_monomeric_attachment_point_item.h"

#include <cmath>

#include <QPainter>

#include "schrodinger/sketcher/molviewer/abstract_monomer_item.h"
#include "schrodinger/sketcher/molviewer/fonts.h"
#include "schrodinger/sketcher/molviewer/monomer_constants.h"
#include "schrodinger/sketcher/tool/draw_monomer_scene_tool.h"

namespace schrodinger
{
namespace sketcher
{

namespace
{
const qreal SQRT_HALF = M_SQRT1_2; // 1/sqrt(2) ≈ 0.707
} // namespace

// TODO: we want to hit the corners of the shape with diagonals, not
    //       necessarily 45 degrees
QPointF direction_to_unit_vector(Direction dir)
{
    switch (dir) {
        case Direction::N:
            return QPointF(0, -1);
        case Direction::S:
            return QPointF(0, 1);
        case Direction::E:
            return QPointF(1, 0);
        case Direction::W:
            return QPointF(-1, 0);
        case Direction::NE:
            return QPointF(SQRT_HALF, -SQRT_HALF);
        case Direction::NW:
            return QPointF(-SQRT_HALF, -SQRT_HALF);
        case Direction::SE:
            return QPointF(SQRT_HALF, SQRT_HALF);
        case Direction::SW:
            return QPointF(-SQRT_HALF, SQRT_HALF);
        default:
            return QPointF(1, 0);
    }
}

UnboundMonomericAttachmentPointItem::UnboundMonomericAttachmentPointItem(
    const UnboundAttachmentPoint& attachment_point,
    AbstractMonomerItem* parent_monomer, const Fonts& fonts) :
    QGraphicsItem(parent_monomer),
    m_attachment_point(attachment_point),
    m_parent_monomer(parent_monomer),
    m_fonts(fonts)
{
    setFlag(QGraphicsItem::ItemStacksBehindParent);

    m_line_pen.setWidthF(UNBOUND_AP_LINE_THICKNESS);
    m_line_pen.setCapStyle(Qt::RoundCap);

    updateCachedData();
}

void UnboundMonomericAttachmentPointItem::setActive(bool active)
{
    if (m_is_active != active) {
        m_is_active = active;
        updateColors();
    }
}

bool UnboundMonomericAttachmentPointItem::isActive() const
{
    return m_is_active;
}

QRectF UnboundMonomericAttachmentPointItem::boundingRect() const
{
    return m_bounding_rect;
}

void UnboundMonomericAttachmentPointItem::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->save();

    // Draw the line from center to endpoint
    painter->setPen(m_line_pen);
    painter->drawLine(QPointF(0, 0), m_line_end);

    // Draw the filled circle at the endpoint
    painter->setPen(Qt::NoPen);
    painter->setBrush(m_circle_brush);
    qreal radius = UNBOUND_AP_CIRCLE_DIAMETER / 2.0;
    painter->drawEllipse(m_line_end, radius, radius);

    // Draw the label
    painter->setFont(m_fonts.m_monomeric_attachment_point_label_font);
    painter->setPen(m_line_pen);
    painter->drawText(m_label_rect, Qt::AlignCenter, m_label_text);

    painter->restore();
}

void UnboundMonomericAttachmentPointItem::updateCachedData()
{
    prepareGeometryChange();

    // Get direction unit vector
    QPointF dir = direction_to_unit_vector(m_attachment_point.direction);

    // Calculate the line endpoint based on the parent's bounding rect
    // The line extends from center (0,0) outward in the direction
    QRectF parent_bounds = m_parent_monomer->boundingRect();

    // Calculate how far to extend based on direction
    // For cardinal directions, use half width or half height
    // For diagonal directions, use the larger of the two
    qreal half_width = parent_bounds.width() / 2.0;
    qreal half_height = parent_bounds.height() / 2.0;

    // Project the direction onto the bounding rect to find the extent
    qreal extent;
    if (qFuzzyIsNull(dir.x())) {
        // Vertical direction (N or S)
        extent = half_height;
    } else if (qFuzzyIsNull(dir.y())) {
        // Horizontal direction (E or W)
        extent = half_width;
    } else {
        // Diagonal direction - find where the ray exits the bounding rect
        qreal t_x = half_width / qAbs(dir.x());
        qreal t_y = half_height / qAbs(dir.y());
        extent = qMin(t_x, t_y);
    }

    // Line endpoint is extent + line length in the direction
    m_line_end = dir * (extent + UNBOUND_AP_LINE_LENGTH);

    m_label_text = prep_attachment_point_name(m_attachment_point.name);

    // Calculate label rect size and position
    m_label_rect = m_fonts.m_monomeric_attachment_point_label_fm.boundingRect(
        m_label_text);
    position_ap_label_rect(m_label_rect, {0.0, 0.0}, dir);

    // Calculate bounding rect as union of all elements
    qreal half_line_width = m_line_pen.widthF() / 2.0;

    // Line bounding rect (from origin to endpoint)
    QRectF line_bounds = QRectF(QPointF(0, 0), m_line_end).normalized();
    line_bounds.adjust(-half_line_width, -half_line_width, half_line_width,
                       half_line_width);

    // Circle bounding rect
    qreal radius = UNBOUND_AP_CIRCLE_DIAMETER / 2.0;
    QRectF circle_bounds(m_line_end.x() - radius, m_line_end.y() - radius,
                         UNBOUND_AP_CIRCLE_DIAMETER,
                         UNBOUND_AP_CIRCLE_DIAMETER);

    m_bounding_rect = line_bounds.united(circle_bounds).united(m_label_rect);

    updateColors();
}

void UnboundMonomericAttachmentPointItem::updateColors()
{
    QColor color =
        m_is_active ? UNBOUND_AP_ACTIVE_COLOR : UNBOUND_AP_INACTIVE_COLOR;
    m_line_pen.setColor(color);
    m_circle_brush.setColor(color);
    update();
}

} // namespace sketcher
} // namespace schrodinger
