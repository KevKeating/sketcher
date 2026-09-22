#define BOOST_TEST_MODULE Test_Sketcher

#include <string>
#include <utility>
#include <vector>

#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTextEdit>
#include <boost/test/unit_test.hpp>

#include "../test_common.h"
#include "schrodinger/sketcher/dialog/custom_monomer_dialog.h"
#include "schrodinger/sketcher/dialog/message_box_dialog.h"
#include "schrodinger/sketcher/sketcher_widget.h"

BOOST_GLOBAL_FIXTURE(QApplicationRequiredFixture);

namespace schrodinger
{
namespace sketcher
{

static QPushButton* get_ok_button(const QWidget& dialog)
{
    auto* button_box = dialog.findChild<QDialogButtonBox*>("button_box");
    BOOST_REQUIRE(button_box != nullptr);
    return button_box->button(QDialogButtonBox::Ok);
}

/**
 * Make sure that the OK button is enabled only when the dialog is non-empty
 */
BOOST_AUTO_TEST_CASE(custom_monomer_dialog_validation_and_acceptance)
{
    CustomMonomerDialog dialog;
    auto* sketcher = dialog.findChild<SketcherWidget*>();
    auto* ok_button = get_ok_button(dialog);
    BOOST_REQUIRE(sketcher != nullptr);

    BOOST_TEST(dialog.windowTitle() == "Sketch Custom Monomer");
    BOOST_TEST(!ok_button->isEnabled());

    dialog.addSMILES("CC");
    BOOST_TEST(ok_button->isEnabled());
}

BOOST_AUTO_TEST_CASE(custom_monomer_dialog_monomer_type_input_can_be_disabled)
{
    CustomMonomerDialog dialog;
    auto* combo = dialog.findChild<QComboBox*>("monomer_type_combo");
    BOOST_REQUIRE(combo != nullptr);

    dialog.setMonomerTypeInputEnabled(false);
    BOOST_TEST(!combo->isEnabled());
    dialog.setMonomerTypeInputEnabled(true);
    BOOST_TEST(combo->isEnabled());
}

/**
 * Numbered attachment points must be unique. Report every duplicated number in
 * numerical order and do not accept the custom monomer.
 */
BOOST_AUTO_TEST_CASE(custom_monomer_dialog_rejects_duplicate_attachment_points)
{
    const std::vector<std::pair<std::string, QString>> test_cases = {
        {"*C* |$_R1;;_R1$|", "Multiple R1 attachment points found. All "
                              "attachment points must be unique."},
        {"*C(*)(*)C(*)(*)* |$_R3;;_R1;_R2;;_R3;_R1;_R2$|",
         "Multiple R1, R2, and R3 attachment points found. All attachment "
         "points must be unique."}};

    for (const auto& [smiles, expected_error] : test_cases) {
        CustomMonomerDialog dialog;
        bool accepted = false;
        QObject::connect(&dialog, &CustomMonomerDialog::customMonomerAccepted,
                         [&accepted]() { accepted = true; });
        dialog.setRequiredAttachmentPoints({2});
        dialog.addSMILES(smiles);

        dialog.accept();

        BOOST_TEST(!accepted);
        auto* message_box_dialog = dialog.findChild<MessageBoxDialog*>();
        BOOST_REQUIRE(message_box_dialog != nullptr);
        auto* error_text =
            message_box_dialog->findChild<QTextEdit*>("text_edit");
        BOOST_REQUIRE(error_text != nullptr);
        BOOST_TEST(error_text->toPlainText() == expected_error);
    }
}

/** Unique numbered attachment points continue to be accepted. */
BOOST_AUTO_TEST_CASE(custom_monomer_dialog_accepts_unique_attachment_points)
{
    CustomMonomerDialog dialog;
    bool accepted = false;
    QObject::connect(&dialog, &CustomMonomerDialog::customMonomerAccepted,
                     [&accepted]() { accepted = true; });
    dialog.addSMILES("*C* |$_R1;;_R2$|");

    dialog.accept();

    BOOST_TEST(accepted);
    BOOST_TEST(dialog.findChild<MessageBoxDialog*>() == nullptr);
}

BOOST_AUTO_TEST_CASE(
    custom_monomer_dialog_warns_before_removing_bound_attachment_points)
{
    const std::vector<std::pair<std::vector<int>, QString>> test_cases = {
        {{3},
         "R3 has been removed from this monomer but is currently bound. "
         "Continuing will remove this connection."},
        {{1, 2, 3},
         "R1 and R3 have been removed from this monomer but are currently "
         "bound. Continuing will remove these connections."}};

    for (const auto& [required_attachment_points, expected_warning] :
         test_cases) {
        CustomMonomerDialog dialog;
        bool accepted = false;
        QObject::connect(&dialog, &CustomMonomerDialog::customMonomerAccepted,
                         [&accepted]() { accepted = true; });
        dialog.setRequiredAttachmentPoints(required_attachment_points);
        dialog.addSMILES("*C* |$_R1;;_R2$|");

        dialog.accept();

        BOOST_TEST(!accepted);
        auto* warning_dialog = dialog.findChild<MessageBoxDialog*>();
        BOOST_REQUIRE(warning_dialog != nullptr);
        auto* warning_text =
            warning_dialog->findChild<QTextEdit*>("text_edit");
        BOOST_REQUIRE(warning_text != nullptr);
        BOOST_TEST(warning_text->toPlainText() == expected_warning);

        auto* button_box =
            warning_dialog->findChild<QDialogButtonBox*>("button_box");
        BOOST_REQUIRE(button_box != nullptr);
        button_box->button(QDialogButtonBox::Cancel)->click();
        BOOST_TEST(!accepted);
        QCoreApplication::processEvents();
    }
}

BOOST_AUTO_TEST_CASE(
    custom_monomer_dialog_continues_after_attachment_point_warning)
{
    CustomMonomerDialog dialog;
    bool accepted = false;
    QObject::connect(&dialog, &CustomMonomerDialog::customMonomerAccepted,
                     [&accepted]() { accepted = true; });
    dialog.setRequiredAttachmentPoints({3});
    dialog.addSMILES("*C* |$_R1;;_R2$|");

    dialog.accept();

    BOOST_TEST(!accepted);
    auto* warning_dialog = dialog.findChild<MessageBoxDialog*>();
    BOOST_REQUIRE(warning_dialog != nullptr);
    auto* button_box =
        warning_dialog->findChild<QDialogButtonBox*>("button_box");
    BOOST_REQUIRE(button_box != nullptr);
    button_box->button(QDialogButtonBox::Ok)->click();
    BOOST_TEST(accepted);
}

} // namespace sketcher
} // namespace schrodinger
