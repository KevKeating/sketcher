#include "schrodinger/sketcher/widget/monomer_symbol_popup_utils.h"

#include <QButtonGroup>
#include <QGridLayout>
#include <QScrollArea>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include "schrodinger/rdkit_extensions/monomer_database.h"
#include "schrodinger/sketcher/sketcher_css_style.h"
#include "schrodinger/sketcher/widget/modular_popup.h"

namespace schrodinger
{
namespace sketcher
{

static constexpr size_t MAX_NUMBER_OF_COLUMNS = 8;
static constexpr float MAX_ROWS_TO_COLUMNS_RATIO = 1.25;
static constexpr int MAX_BUTTON_HEIGHT = 32;

/**
 * Determine the number of columns we should use for a monomer popup containing
 * the specified number of monomers. This function tries to keep the popup
 * roughly square-ish while using the same number of columns for popups with
 * similar numbers of monomers, up to a maximum of eight columns.
 */
static size_t get_num_columns(const size_t num_monomers)
{
    const size_t MIN_NUMBER_OF_COLUMNS = 4;

    size_t num_columns = MIN_NUMBER_OF_COLUMNS;
    while (true) {
        if (num_columns == MAX_NUMBER_OF_COLUMNS ||
            MAX_ROWS_TO_COLUMNS_RATIO * num_columns * num_columns >=
                num_monomers) {
            // Use enough columns to satisfy the row ratio, or stop at the cap
            // and let the remaining rows scroll.
            return num_columns;
        }
        num_columns *= 2;
    }
}

QButtonGroup* build_monomer_symbol_buttons(
    ModularPopup* popup, const std::string& object_name_prefix,
    const std::string& standard_symbol, const std::string& standard_name,
    const std::vector<rdkit_extensions::MonomerInfo>& analogs,
    std::unordered_map<int, std::string>& id_to_symbol)
{
    const auto num_monomers = analogs.size() + 1;
    const auto num_columns = get_num_columns(num_monomers);
    const bool needs_scroll =
        num_monomers > MAX_ROWS_TO_COLUMNS_RATIO * num_columns * num_columns;
    auto* button_widget = needs_scroll ? new QWidget(popup) : popup;
    auto* layout = new QGridLayout(button_widget);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(0);

    auto* group = new QButtonGroup(popup);

    int id = 0;
    auto make_button = [&](const std::string& symbol, const std::string& name) {
        auto* btn = new QToolButton(button_widget);
        btn->setText(QString::fromStdString(symbol));
        btn->setToolTip(QString::fromStdString(name));
        btn->setCheckable(true);
        btn->setMinimumSize(32, 30);
        btn->setMaximumSize(32, MAX_BUTTON_HEIGHT);
        btn->setObjectName(
            QString::fromStdString(object_name_prefix + "_" + symbol + "_btn"));
        btn->setStyleSheet(symbol.size() >= COMPACT_STYLE_MIN_LENGTH
                               ? ATOM_ELEMENT_OR_MONOMER_COMPACT_STYLE
                               : ATOM_ELEMENT_OR_MONOMER_STYLE);
        layout->addWidget(btn, id / num_columns, id % num_columns);
        group->addButton(btn);
        id_to_symbol[id] = symbol;
        ++id;
    };

    make_button(standard_symbol, standard_name);
    for (const auto& analog : analogs) {
        make_button(analog.symbol.value_or(""), analog.name.value_or(""));
    }
    if (needs_scroll) {
        // Keep the grid at its natural size and show at most ten full rows.
        // Reserve space for the scrollbar so no columns are clipped.
        auto* scroll_area = new QScrollArea(popup);
        scroll_area->setFrameShape(QFrame::NoFrame);
        scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        layout->setSizeConstraint(QLayout::SetFixedSize);
        scroll_area->setWidget(button_widget);
        const int max_rows = MAX_ROWS_TO_COLUMNS_RATIO * MAX_NUMBER_OF_COLUMNS;
        const auto margins = layout->contentsMargins();
        scroll_area->setFixedSize(
            layout->sizeHint().width() +
                scroll_area->style()->pixelMetric(QStyle::PM_ScrollBarExtent),
            max_rows * MAX_BUTTON_HEIGHT + margins.top() + margins.bottom());
        auto* popup_layout = new QVBoxLayout(popup);
        popup_layout->setContentsMargins(0, 0, 0, 0);
        popup_layout->addWidget(scroll_area);
    }
    return group;
}

} // namespace sketcher
} // namespace schrodinger
