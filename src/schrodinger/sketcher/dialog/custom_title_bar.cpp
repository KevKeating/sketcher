#include "schrodinger/sketcher/dialog/custom_title_bar.h"

#include <QApplication>
#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QToolButton>
#include <QtGlobal>

#include "schrodinger/sketcher/sketcher_css_style.h"

namespace schrodinger
{
namespace sketcher
{

namespace
{

/**
 * The sizes and colors of a CustomTitleBar
 */
struct TitleBarMetrics {
    int height;
    int title_font_size;
    int close_button_font_size;
    QColor background_color;
};

static const TitleBarMetrics POPUP_TITLE_BAR_METRICS(
    /* height: */ 24,
    /* title font size: */ 12,
    /* close button font size: */ 14,
    /* background color: */ QColor(POPUP_TITLE_BAR_COLOR));

static const TitleBarMetrics DIALOG_TITLE_BAR_METRICS(
    /* height: */ 30,
    /* title font size: */ 14,
    /* close button font size: */ 16,
    /* background color: */ QColor(64, 64, 64));

} // namespace

/**
 * @return The sizes and colors for the given title bar appearance
 */
static TitleBarMetrics get_metrics(TitleBarAppearance appearance)
{
    if (appearance == TitleBarAppearance::POPUP) {
        return POPUP_TITLE_BAR_METRICS;
    } else if (appearance == TitleBarAppearance::DIALOG) {
        return DIALOG_TITLE_BAR_METRICS;
    }
    Q_UNREACHABLE_RETURN(DIALOG_TITLE_BAR_METRICS);
}

CustomTitleBar::CustomTitleBar(const QString& title, QWidget* parent,
                               TitleBarAppearance appearance) :
    QFrame(parent),
    m_appearance(appearance)
{
    const auto metrics = get_metrics(appearance);
    QHBoxLayout* layout = new QHBoxLayout(this);
    this->setFixedHeight(metrics.height);
    m_title_label = new QLabel(title, this);
    m_title_label->setStyleSheet(
        QString("color: white; font-size: %1px; font-weight: 600;")
            .arg(metrics.title_font_size));
    layout->addWidget(m_title_label);
}

CustomTitleBar::~CustomTitleBar() = default;

QToolButton* CustomTitleBar::addCloseButton()
{
    if (m_close_button != nullptr) {
        return m_close_button;
    }
    m_close_button = new QToolButton(this);
    m_close_button->setToolTip("Close");
    // a multiplication sign, as in the mockups
    m_close_button->setText(QString(QChar(0x00D7)));
    m_close_button->setStyleSheet(
        QString("QToolButton { color: white; font-size: %1px; "
                "    font-weight: bold; background-color: transparent; "
                "    border: none; }"
                "QToolButton:hover { color: #cccccc; }")
            .arg(get_metrics(m_appearance).close_button_font_size));
    auto* layout = qobject_cast<QHBoxLayout*>(this->layout());
    // Without vertical margins, the button fits in the fixed height. The title
    // is still centered vertically.
    auto margins = layout->contentsMargins();
    margins.setTop(0);
    margins.setBottom(0);
    margins.setRight(4);
    layout->setContentsMargins(margins);
    layout->addStretch();
    layout->addWidget(m_close_button);
    return m_close_button;
}

QToolButton* CustomTitleBar::getCloseButton() const
{
    return m_close_button;
}

void CustomTitleBar::setDraggable(bool draggable)
{
    m_draggable = draggable;
}

void CustomTitleBar::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    const auto background_color = get_metrics(m_appearance).background_color;
    if (m_appearance == TitleBarAppearance::POPUP) {
        // the popup draws its own outline
        painter.fillRect(rect(), background_color);
        return;
    }
    painter.setBrush(QBrush(background_color));
    painter.drawRect(rect());
}

void CustomTitleBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_cursor_start_pos = event->position();
        m_drag_started = false;
    }
}

void CustomTitleBar::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_draggable) {
        return;
    }
    if (event->buttons() & Qt::LeftButton) {
        if (!m_drag_started &&
            (event->position() - m_cursor_start_pos).manhattanLength() >=
                QApplication::startDragDistance()) {
            m_drag_started = true;
        }
        if (m_drag_started && parentWidget()) {
            parentWidget()->move(parentWidget()->pos() +
                                 event->position().toPoint() -
                                 m_cursor_start_pos.toPoint());
        }
    }
}

} // namespace sketcher
} // namespace schrodinger

#include "schrodinger/sketcher/dialog/custom_title_bar.moc"
