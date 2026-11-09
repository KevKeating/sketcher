#define BOOST_TEST_MODULE Test_Sketcher

#include <QApplication>
#include <QMouseEvent>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>

#include "../test_common.h"
#include "schrodinger/sketcher/dialog/custom_title_bar.h"

BOOST_GLOBAL_FIXTURE(QApplicationRequiredFixture);
BOOST_TEST_DONT_PRINT_LOG_VALUE(QPoint);
BOOST_TEST_DONT_PRINT_LOG_VALUE(schrodinger::sketcher::TitleBarAppearance);

namespace schrodinger
{
namespace sketcher
{

/**
 * Drag the title bar with the left mouse button from its center by the given
 * offset
 */
static void drag_title_bar(CustomTitleBar& title_bar, const QPoint& offset)
{
    const QPointF start = title_bar.rect().center();
    const QPointF end = start + offset;
    QMouseEvent press(QEvent::MouseButtonPress, start,
                      title_bar.mapToGlobal(start), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&title_bar, &press);
    QMouseEvent move(QEvent::MouseMove, end, title_bar.mapToGlobal(end),
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&title_bar, &move);
    QMouseEvent release(QEvent::MouseButtonRelease, end,
                        title_bar.mapToGlobal(end), Qt::LeftButton,
                        Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&title_bar, &release);
}

/**
 * A title bar has no close button unless one is added, and adding one twice
 * returns the same button
 */
BOOST_DATA_TEST_CASE(test_close_button,
                     boost::unit_test::data::make({TitleBarAppearance::DARK,
                                                   TitleBarAppearance::LIGHT}),
                     appearance)
{
    QWidget parent;
    auto* title_bar = new CustomTitleBar("Title", &parent, appearance);
    BOOST_TEST(title_bar->getCloseButton() == nullptr);
    auto* close_button = title_bar->addCloseButton();
    BOOST_REQUIRE(close_button != nullptr);
    BOOST_TEST(close_button->parent() == title_bar);
    BOOST_TEST(title_bar->getCloseButton() == close_button);
    BOOST_TEST(title_bar->addCloseButton() == close_button);
    // the dark title bar shows text, and the light one the platform's icon
    BOOST_TEST(close_button->icon().isNull() ==
               (appearance == TitleBarAppearance::DARK));
    // the button fits in the title bar's fixed height
    title_bar->resize(200, title_bar->height());
    parent.show();
    QApplication::processEvents();
    BOOST_TEST(title_bar->rect().contains(close_button->geometry()));
}

/**
 * Dragging the title bar moves its parent unless dragging is disabled
 */
BOOST_AUTO_TEST_CASE(test_draggable)
{
    QWidget parent;
    auto* layout = new QVBoxLayout(&parent);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* title_bar = new CustomTitleBar("Title", &parent);
    layout->addWidget(title_bar);
    parent.resize(200, 100);
    parent.move(100, 100);
    parent.show();
    QApplication::processEvents();
    const QPoint offset(40, 30);

    const auto start_pos = parent.pos();
    drag_title_bar(*title_bar, offset);
    BOOST_TEST(parent.pos() == start_pos + offset);

    title_bar->setDraggable(false);
    const auto undragged_pos = parent.pos();
    drag_title_bar(*title_bar, offset);
    BOOST_TEST(parent.pos() == undragged_pos);
}

} // namespace sketcher
} // namespace schrodinger
