#include "schrodinger/sketcher/widget/multi_select_combo_box.h"

#include <algorithm>

#include <QAbstractItemView>
#include <QApplication>
#include <QFontMetrics>
#include <QHoverEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QStringList>
#include <QStyle>
#include <QStyleOptionComboBox>
#include <QStylePainter>

namespace schrodinger
{
namespace sketcher
{

static const QString ALL_CHECKED_TEXT = "Any";
static const QString NONE_CHECKED_TEXT = "None";
static const QString DISPLAY_TEXT_SEPARATOR = ", ";

/**
 * @return Whether the key toggles the current item of the dropdown
 */
static bool is_toggle_key(int key)
{
    return key == Qt::Key_Space || key == Qt::Key_Enter ||
           key == Qt::Key_Return;
}

MultiSelectComboBox::MultiSelectComboBox(QWidget* parent) : QComboBox(parent)
{
    m_model = new QStandardItemModel(this);
    setModel(m_model);
    connect(m_model, &QStandardItemModel::rowsInserted, this,
            &MultiSelectComboBox::onRowsInserted);
    connect(m_model, &QStandardItemModel::dataChanged, this,
            &MultiSelectComboBox::onDataChanged);
    connect(m_model, &QStandardItemModel::rowsRemoved, this,
            &MultiSelectComboBox::checkedItemsChanged);
    // The display text depends on the check states, which don't change the
    // current item, so repaint whenever they change
    connect(this, &MultiSelectComboBox::checkedItemsChanged, this,
            qOverload<>(&MultiSelectComboBox::update));
    // Calling view() creates the dropdown, which installs its own event
    // filters on the view and its viewport. Event filters installed later run
    // first, so these filters see each event before the dropdown can close
    // itself in response to it.
    view()->installEventFilter(this);
    view()->viewport()->installEventFilter(this);
}

void MultiSelectComboBox::onRowsInserted(const QModelIndex& parent, int first,
                                         int last)
{
    if (parent.isValid()) {
        return;
    }
    QSignalBlocker blocker(m_model);
    for (int row = first; row <= last; ++row) {
        auto* item = m_model->item(row);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
}

void MultiSelectComboBox::onDataChanged(const QModelIndex&, const QModelIndex&,
                                        const QList<int>& roles)
{
    if (!roles.isEmpty() && !roles.contains(Qt::CheckStateRole)) {
        return;
    }
    if (m_suppress_changed_signal) {
        m_changed_while_suppressed = true;
    } else {
        emit checkedItemsChanged();
    }
}

void MultiSelectComboBox::setCheckedData(const QList<QVariant>& data)
{
    m_suppress_changed_signal = true;
    m_changed_while_suppressed = false;
    for (int row = 0; row < count(); ++row) {
        setItemChecked(row, data.contains(itemData(row)));
    }
    m_suppress_changed_signal = false;
    if (m_changed_while_suppressed) {
        emit checkedItemsChanged();
    }
}

QList<QVariant> MultiSelectComboBox::checkedData() const
{
    QList<QVariant> data;
    for (int row = 0; row < count(); ++row) {
        if (isItemChecked(row)) {
            data.append(itemData(row));
        }
    }
    return data;
}

void MultiSelectComboBox::setItemChecked(int index, bool checked)
{
    auto* item = m_model->item(index);
    if (item == nullptr) {
        return;
    }
    const auto state = checked ? Qt::Checked : Qt::Unchecked;
    if (item->checkState() != state) {
        item->setCheckState(state);
    }
}

bool MultiSelectComboBox::isItemChecked(int index) const
{
    auto* item = m_model->item(index);
    return item != nullptr && item->checkState() == Qt::Checked;
}

QString MultiSelectComboBox::getDisplayText() const
{
    QStringList checked_texts;
    for (int row = 0; row < count(); ++row) {
        if (isItemChecked(row)) {
            checked_texts.append(itemText(row));
        }
    }
    if (checked_texts.isEmpty()) {
        return NONE_CHECKED_TEXT;
    } else if (checked_texts.size() == count()) {
        return ALL_CHECKED_TEXT;
    }
    return checked_texts.join(DISPLAY_TEXT_SEPARATOR);
}

void MultiSelectComboBox::toggleItemByUser(int row)
{
    auto* item = m_model->item(row);
    if (item == nullptr || !item->isEnabled()) {
        return;
    }
    setItemChecked(row, !isItemChecked(row));
}

bool MultiSelectComboBox::eventFilter(QObject* watched, QEvent* event)
{
    auto* item_view = view();
    if (watched == item_view->viewport()) {
        switch (event->type()) {
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonDblClick: {
                auto* mouse_event = static_cast<QMouseEvent*>(event);
                m_pressed_row =
                    item_view->indexAt(mouse_event->position().toPoint()).row();
                // Let a single press through so that the view can make the
                // pressed row current, but swallow a double-click so that it
                // can't toggle the item on its own
                return event->type() == QEvent::MouseButtonDblClick;
            }
            case QEvent::MouseButtonRelease: {
                auto* mouse_event = static_cast<QMouseEvent*>(event);
                const int row =
                    item_view->indexAt(mouse_event->position().toPoint()).row();
                if (row >= 0 && row == m_pressed_row) {
                    toggleItemByUser(row);
                }
                m_pressed_row = -1;
                // Swallow the release so that the dropdown stays open and the
                // view's delegate doesn't also toggle the item
                return true;
            }
            default:
                break;
        }
    } else if (watched == item_view) {
        if (event->type() == QEvent::ShortcutOverride ||
            event->type() == QEvent::KeyPress) {
            auto* key_event = static_cast<QKeyEvent*>(event);
            if (is_toggle_key(key_event->key()) &&
                key_event->modifiers() == Qt::NoModifier) {
                if (event->type() == QEvent::ShortcutOverride) {
                    // Ask for the key press rather than letting the dropdown
                    // close itself in response to Enter or Return
                    event->accept();
                } else if (!key_event->isAutoRepeat()) {
                    toggleItemByUser(item_view->currentIndex().row());
                }
                return true;
            }
        }
    }
    return QComboBox::eventFilter(watched, event);
}

void MultiSelectComboBox::clearHoverHighlight()
{
    QHoverEvent leave(QEvent::HoverLeave, QPointF(-1, -1), QPointF(-1, -1),
                      QPointF(-1, -1));
    QApplication::sendEvent(view()->viewport(), &leave);
}

void MultiSelectComboBox::showPopup()
{
    QComboBox::showPopup();
    clearHoverHighlight();
}

void MultiSelectComboBox::hidePopup()
{
    QComboBox::hidePopup();
    clearHoverHighlight();
    m_pressed_row = -1;
}

QSize MultiSelectComboBox::sizeHint() const
{
    auto hint = QComboBox::sizeHint();
    // The widest text that can be displayed has every item checked but one
    const auto metrics = fontMetrics();
    int text_width = std::max(metrics.horizontalAdvance(ALL_CHECKED_TEXT),
                              metrics.horizontalAdvance(NONE_CHECKED_TEXT));
    for (int skipped = 0; skipped < count(); ++skipped) {
        QStringList texts;
        for (int row = 0; row < count(); ++row) {
            if (row != skipped) {
                texts.append(itemText(row));
            }
        }
        text_width = std::max(text_width, metrics.horizontalAdvance(texts.join(
                                              DISPLAY_TEXT_SEPARATOR)));
    }
    QStyleOptionComboBox option;
    initStyleOption(&option);
    const auto size_from_contents = style()->sizeFromContents(
        QStyle::CT_ComboBox, &option, QSize(text_width, hint.height()), this);
    hint.setWidth(std::max(hint.width(), size_from_contents.width()));
    return hint;
}

void MultiSelectComboBox::paintEvent(QPaintEvent*)
{
    QStylePainter painter(this);
    painter.setPen(palette().color(QPalette::Text));
    QStyleOptionComboBox option;
    initStyleOption(&option);
    option.currentIcon = QIcon();
    const auto text_rect = style()->subControlRect(
        QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxEditField, this);
    option.currentText = fontMetrics().elidedText(
        getDisplayText(), Qt::ElideRight, text_rect.width());
    painter.drawComplexControl(QStyle::CC_ComboBox, option);
    painter.drawControl(QStyle::CE_ComboBoxLabel, option);
}

} // namespace sketcher
} // namespace schrodinger

#include "schrodinger/sketcher/widget/multi_select_combo_box.moc"
