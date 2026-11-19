#define BOOST_TEST_MODULE test_multi_select_combo_box
#include <boost/test/unit_test.hpp>

#include <QAbstractItemView>
#include <QApplication>
#include <QFontMetrics>
#include <QSignalBlocker>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QTest>

#include "../test_common.h"
#include "schrodinger/sketcher/widget/multi_select_combo_box.h"

BOOST_GLOBAL_FIXTURE(QApplicationRequiredFixture);

namespace schrodinger
{
namespace sketcher
{

/**
 * A visible multi-select combo box with three items, whose data is 1, 2 and
 * 4
 */
struct ComboFixture {
    ComboFixture()
    {
        combo.addItem("Peptide", 1);
        combo.addItem("Nucleic acid", 2);
        combo.addItem("Structure", 4);
        combo.show();
    }

    ~ComboFixture()
    {
        combo.hidePopup();
    }

    /**
     * Click the given item in the open dropdown
     */
    void clickItem(int row)
    {
        auto* view = combo.view();
        BOOST_REQUIRE(view->isVisible());
        QTest::mouseClick(
            view->viewport(), Qt::LeftButton, Qt::NoModifier,
            view->visualRect(combo.model()->index(row, 0)).center());
        QApplication::processEvents();
    }

    std::vector<bool> getCheckStates()
    {
        std::vector<bool> states;
        for (int row = 0; row < combo.count(); ++row) {
            states.push_back(combo.isItemChecked(row));
        }
        return states;
    }

    MultiSelectComboBox combo;
};

/**
 * Added items are checkable and unchecked, and the display text joins the
 * checked items in item order, or reads "Any" when all are checked or "None"
 * when none are
 */
BOOST_AUTO_TEST_CASE(display_text)
{
    ComboFixture fixture;
    auto& combo = fixture.combo;
    for (int row = 0; row < combo.count(); ++row) {
        auto flags = combo.model()->index(row, 0).flags();
        BOOST_TEST(flags.testFlag(Qt::ItemIsUserCheckable));
        BOOST_TEST(!combo.isItemChecked(row));
    }
    BOOST_TEST(combo.getDisplayText() == "None");
    combo.setItemChecked(2, true);
    combo.setItemChecked(0, true);
    BOOST_TEST(combo.getDisplayText() == "Peptide, Structure");
    combo.setItemChecked(1, true);
    BOOST_TEST(combo.getDisplayText() == "Any");
    combo.setItemChecked(0, false);
    BOOST_TEST(combo.getDisplayText() == "Nucleic acid, Structure");
    // the display text never comes from the current item
    BOOST_TEST(combo.currentText() == "Peptide");

    // there's room for the widest text that can be displayed
    const auto metrics = combo.fontMetrics();
    BOOST_TEST(combo.sizeHint().width() >
               metrics.horizontalAdvance("Nucleic acid, Structure"));
}

/**
 * The data API checks exactly the given items and reports them in item order,
 * emitting the change signal once per call
 */
BOOST_AUTO_TEST_CASE(checked_data)
{
    ComboFixture fixture;
    auto& combo = fixture.combo;
    QSignalSpy spy(&combo, &MultiSelectComboBox::checkedItemsChanged);
    combo.setCheckedData({4, 1});
    BOOST_TEST(spy.count() == 1);
    BOOST_CHECK(combo.checkedData() == (QList<QVariant>{1, 4}));
    BOOST_TEST(fixture.getCheckStates() ==
                   (std::vector<bool>{true, false, true}),
               boost::test_tools::per_element());

    // no change, no signal
    combo.setCheckedData({1, 4});
    BOOST_TEST(spy.count() == 1);

    combo.setCheckedData({2});
    BOOST_TEST(spy.count() == 2);
    BOOST_CHECK(combo.checkedData() == (QList<QVariant>{2}));

    // programmatic single-item changes emit the signal too, unless blocked
    combo.setItemChecked(0, true);
    BOOST_TEST(spy.count() == 3);
    {
        QSignalBlocker blocker(&combo);
        combo.setItemChecked(0, false);
    }
    BOOST_TEST(spy.count() == 3);
    BOOST_TEST(!combo.isItemChecked(0));
}

/**
 * Clicking an item in the dropdown toggles it and keeps the dropdown open. A
 * press and release on different rows doesn't toggle anything, nor does a
 * click on a disabled item.
 */
BOOST_AUTO_TEST_CASE(click_toggles)
{
    ComboFixture fixture;
    auto& combo = fixture.combo;
    QSignalSpy spy(&combo, &MultiSelectComboBox::checkedItemsChanged);
    combo.showPopup();
    auto* view = combo.view();
    BOOST_REQUIRE(view->isVisible());

    fixture.clickItem(1);
    BOOST_TEST(fixture.getCheckStates() ==
                   (std::vector<bool>{false, true, false}),
               boost::test_tools::per_element());
    BOOST_TEST(spy.count() == 1);
    BOOST_TEST(view->isVisible());
    fixture.clickItem(0);
    fixture.clickItem(1);
    BOOST_TEST(fixture.getCheckStates() ==
                   (std::vector<bool>{true, false, false}),
               boost::test_tools::per_element());
    BOOST_TEST(spy.count() == 3);
    BOOST_TEST(view->isVisible());

    // press on one row and release on another
    auto center = [&](int row) {
        return view->visualRect(combo.model()->index(row, 0)).center();
    };
    QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                      center(1));
    QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                        center(2));
    BOOST_TEST(fixture.getCheckStates() ==
                   (std::vector<bool>{true, false, false}),
               boost::test_tools::per_element());
    BOOST_TEST(view->isVisible());

    // a disabled item can't be toggled
    auto* model = qobject_cast<QStandardItemModel*>(combo.model());
    BOOST_REQUIRE(model != nullptr);
    model->item(2)->setEnabled(false);
    fixture.clickItem(2);
    BOOST_TEST(!combo.isItemChecked(2));
    BOOST_TEST(view->isVisible());
}

/**
 * Space, Enter and Return toggle the current item and keep the dropdown open,
 * and Escape closes it
 */
BOOST_AUTO_TEST_CASE(keys_toggle)
{
    ComboFixture fixture;
    auto& combo = fixture.combo;
    combo.showPopup();
    auto* view = combo.view();
    BOOST_REQUIRE(view->isVisible());
    view->setCurrentIndex(combo.model()->index(1, 0));
    std::vector<bool> expected{false, false, false};
    for (const auto key : {Qt::Key_Space, Qt::Key_Enter, Qt::Key_Return}) {
        BOOST_TEST_CONTEXT("key=" << key)
        {
            QTest::keyClick(view, key);
            expected[1] = !expected[1];
            BOOST_TEST(fixture.getCheckStates() == expected,
                       boost::test_tools::per_element());
            BOOST_TEST(view->isVisible());
        }
    }
    QTest::keyClick(view, Qt::Key_Escape);
    BOOST_TEST(!view->isVisible());
}

/**
 * The user can uncheck the last checked item, which leaves no items checked,
 * and can then check another item
 */
BOOST_AUTO_TEST_CASE(uncheck_last_item)
{
    ComboFixture fixture;
    auto& combo = fixture.combo;
    combo.setCheckedData({2});
    combo.showPopup();
    fixture.clickItem(1);
    BOOST_TEST(combo.checkedData().isEmpty());
    BOOST_TEST(combo.getDisplayText() == "None");
    BOOST_TEST(combo.view()->isVisible());
    fixture.clickItem(0);
    BOOST_CHECK(combo.checkedData() == (QList<QVariant>{1}));
    BOOST_TEST(combo.getDisplayText() == "Peptide");
    combo.view()->setCurrentIndex(combo.model()->index(0, 0));
    QTest::keyClick(combo.view(), Qt::Key_Space);
    BOOST_TEST(combo.checkedData().isEmpty());
    combo.hidePopup();
}

} // namespace sketcher
} // namespace schrodinger
