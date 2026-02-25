#include "schrodinger/sketcher/molviewer/unbound_monomeric_attachment_point_item.h"

#include <cmath>

#include <QPainter>

#include "schrodinger/sketcher/molviewer/abstract_monomer_item.h"
#include "schrodinger/sketcher/molviewer/fonts.h"
#include "schrodinger/sketcher/molviewer/monomer_constants.h"

namespace schrodinger
{
namespace sketcher
{

namespace
{
const qreal SQRT_HALF = M_SQRT1_2; // 1/sqrt(2) ≈ 0.707
} // namespace

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
    painter->setPen(Qt::black);
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

    // Build label text with Unicode prime substitution
    m_label_text = QString::fromStdString(m_attachment_point.name);
    m_label_text.replace('\'', QString::fromUtf8("\u2032"));

    // Calculate label rect size from font metrics
    m_label_rect = m_fonts.m_monomeric_attachment_point_label_fm.boundingRect(
        m_label_text);

    // Position the label based on direction
    positionLabelRect();

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

void UnboundMonomericAttachmentPointItem::positionLabelRect()
{
    QPointF dir = direction_to_unit_vector(m_attachment_point.direction);
    qreal gap = UNBOUND_AP_CIRCLE_DIAMETER / 2.0 + UNBOUND_AP_LABEL_GAP;

    // Calculate label center position offset from the circle center
    QPointF label_center =
        m_line_end + dir * (gap + m_label_rect.width() / 2.0);

    // Adjust based on direction components
    // For horizontal component: align left/right edge appropriately
    if (dir.x() > 0.1) {
        // East component: align left edge of label to just past circle
        label_center.setX(m_line_end.x() + gap + m_label_rect.width() / 2.0);
    } else if (dir.x() < -0.1) {
        // West component: align right edge of label to just before circle
        label_center.setX(m_line_end.x() - gap - m_label_rect.width() / 2.0);
    } else {
        // Pure N or S: center horizontally
        label_center.setX(m_line_end.x());
    }

    // For vertical component: align top/bottom edge appropriately
    if (dir.y() < -0.1) {
        // North component: align bottom of label to just above circle
        label_center.setY(m_line_end.y() - gap - m_label_rect.height() / 2.0);
    } else if (dir.y() > 0.1) {
        // South component: align top of label to just below circle
        label_center.setY(m_line_end.y() + gap + m_label_rect.height() / 2.0);
    } else {
        // Pure E or W: center vertically
        label_center.setY(m_line_end.y());
    }

    m_label_rect.moveCenter(label_center);
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
