#pragma once

#include <QComboBox>
#include <QList>
#include <QString>
#include <QVariant>

#include "schrodinger/sketcher/definitions.h"

class QStandardItemModel;

namespace schrodinger
{
namespace sketcher
{

/**
 * A combo box that lets the user check any number of its items.
 *
 * Every item added to the combo box (e.g. with addItem() or addItems()) is
 * user-checkable and starts unchecked. Clicking an item in the dropdown, or
 * pressing Space, Enter or Return on it, toggles it and keeps the dropdown
 * open. Escape or a click outside the dropdown closes it.
 *
 * The combo box displays its checked items joined with ", " in item order,
 * "Any" when every item is checked, or "None" when no item is checked. The
 * current index is not used.
 */
class SKETCHER_API MultiSelectComboBox : public QComboBox
{
    Q_OBJECT
  public:
    MultiSelectComboBox(QWidget* parent = nullptr);

    /**
     * Check exactly the items whose Qt::UserRole data is in the given list,
     * and uncheck all others. This emits checkedItemsChanged() at most once.
     */
    void setCheckedData(const QList<QVariant>& data);

    /**
     * @return The Qt::UserRole data of the checked items, in item order
     */
    QList<QVariant> checkedData() const;

    /**
     * Check or uncheck the item at the given index
     */
    void setItemChecked(int index, bool checked);

    /**
     * @return Whether the item at the given index is checked
     */
    bool isItemChecked(int index) const;

    /**
     * @return The text that the combo box displays: the checked items' text
     * joined with ", ", "Any" if every item is checked, or "None" if no item
     * is checked
     */
    QString getDisplayText() const;

    /**
     * Overridden QComboBox method: make room for the widest text that the
     * combo box can display
     */
    QSize sizeHint() const override;

    /**
     * Overridden QComboBox methods: clear any hover highlight in the dropdown
     * when it opens and closes
     */
    void showPopup() override;
    void hidePopup() override;

  signals:
    /**
     * Emitted whenever an item is checked or unchecked, whether by the user or
     * programmatically
     */
    void checkedItemsChanged();

  protected:
    /**
     * Overridden QObject method: toggle items in the dropdown in response to
     * clicks and key presses without closing it
     */
    bool eventFilter(QObject* watched, QEvent* event) override;

    /**
     * Overridden QComboBox method: paint the display text instead of the
     * current item's text
     */
    void paintEvent(QPaintEvent* event) override;

  private:
    /**
     * Toggle the item at the given row in response to the user, unless the
     * item is disabled
     */
    void toggleItemByUser(int row);

    /**
     * Make newly added items checkable and unchecked
     */
    void onRowsInserted(const QModelIndex& parent, int first, int last);

    /**
     * Emit checkedItemsChanged() if the check state changed
     */
    void onDataChanged(const QModelIndex& top_left,
                       const QModelIndex& bottom_right,
                       const QList<int>& roles);

    /**
     * Clear the dropdown's hover highlight
     */
    void clearHoverHighlight();

    QStandardItemModel* m_model = nullptr;
    // The row that a mouse press in the dropdown landed on, or -1
    int m_pressed_row = -1;
    // Whether checkedItemsChanged() is suppressed during a batch update
    bool m_suppress_changed_signal = false;
    // Whether a check state changed while checkedItemsChanged() was suppressed
    bool m_changed_while_suppressed = false;
};

} // namespace sketcher
} // namespace schrodinger
