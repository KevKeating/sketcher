#define BOOST_TEST_MODULE Test_Sketcher

#include <QDialogButtonBox>
#include <QWidget>
#include <boost/test/unit_test.hpp>

#include "../test_common.h"
#include "schrodinger/sketcher/dialog/message_box_dialog.h"

BOOST_GLOBAL_FIXTURE(QApplicationRequiredFixture);

namespace schrodinger
{
namespace sketcher
{

BOOST_AUTO_TEST_CASE(message_box_warning_has_ok_and_cancel_buttons)
{
    QWidget parent;

    show_warning_dialog("Warning", "Warning text", &parent);

    auto* dialog = parent.findChild<MessageBoxDialog*>();
    BOOST_REQUIRE(dialog != nullptr);
    auto* button_box = dialog->findChild<QDialogButtonBox*>("button_box");
    BOOST_REQUIRE(button_box != nullptr);
    BOOST_TEST(button_box->standardButtons().testFlag(QDialogButtonBox::Ok));
    BOOST_TEST(button_box->standardButtons().testFlag(QDialogButtonBox::Cancel));
}

} // namespace sketcher
} // namespace schrodinger
