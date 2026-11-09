#include "schrodinger/sketcher/dialog/custom_title_bar.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QToolButton>

namespace schrodinger
{
namespace sketcher
{

static const QString TITLE_STYLE = "font-size: 14px; font-weight: 600;";

static const QString DARK_CLOSE_BUTTON_STYLE =
    "QToolButton { color: white; font-size: 16px; font-weight: bold; "
    "    background-color: transparent; border: none; }"
    "QToolButton:hover { color: #cccccc; }";

CustomTitleBar::CustomTitleBar(const QString& title, QWidget* parent,
                               TitleBarAppearance appearance) :
    QFrame(parent),
    m_appearance(appearance)
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    this->setFixedHeight(30);
    m_title_label = new QLabel(title, this);
    m_title_label->setStyleSheet(appearance == TitleBarAppearance::DARK
                                     ? "color: white; " + TITLE_STYLE
                                     : TITLE_STYLE);
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
    if (m_appearance == TitleBarAppearance::DARK) {
        // a multiplication sign, as in the mockups
        m_close_button->setText(QString(QChar(0x00D7)));
        m_close_button->setStyleSheet(DARK_CLOSE_BUTTON_STYLE);
    } else {
        // Use the platform style's own icon so that the button fits in
        m_close_button->setIcon(style()->standardIcon(
            QStyle::SP_TitleBarCloseButton, nullptr, this));
        m_close_button->setAutoRaise(true);
    }
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
    if (m_appearance == TitleBarAppearance::DARK) {
        painter.setBrush(QBrush(QColor(64, 64, 64)));
        painter.drawRect(rect());
        return;
    }
    painter.fillRect(rect(), palette().color(QPalette::Window));
    // separate the title bar from the contents below it
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawLine(rect().bottomLeft(), rect().bottomRight());
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
