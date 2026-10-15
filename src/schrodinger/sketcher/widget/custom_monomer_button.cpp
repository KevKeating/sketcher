#include "schrodinger/sketcher/widget/custom_monomer_button.h"

#include <QAction>
#include <QMenu>
#include <QStyleOptionButton>
#include <QStyleOptionToolButton>
#include <QStylePainter>

#include "schrodinger/rdkit_extensions/monomer_mol.h"
#include "schrodinger/sketcher/dialog/custom_monomer_dialog.h"
#include "schrodinger/sketcher/sketcher_css_style.h"

namespace schrodinger
{
namespace sketcher
{

CustomMonomerButton::CustomMonomerButton(QWidget* parent) : QToolButton(parent)
{
    setAttribute(Qt::WA_Hover, true);
    setCheckable(true);
    setStyleSheet(CUSTOM_MONOMER_BUTTON_STYLE);

    m_menu = new QMenu(this);
    m_monomer_action = m_menu->addAction(QString());
    m_menu->addSeparator();
    auto* chem_action = m_menu->addAction("Draw Structure...");
    setChainType(rdkit_extensions::ChainType::PEPTIDE);

    // Do not install a QToolButton menu: its launch logic in Qt 6.7 enters a
    // nested event loop, which requires asyncify in WASM builds.
    connect(this, &QToolButton::clicked, this,
            [this]() { m_menu->popup(mapToGlobal(QPoint(0, height()))); });
    connect(m_monomer_action, &QAction::triggered, this,
            [this]() { sketchCustomMonomer(m_chain_type); });
    connect(chem_action, &QAction::triggered, this, [this]() {
        sketchCustomMonomer(rdkit_extensions::ChainType::CHEM);
    });
}

void CustomMonomerButton::setChainType(rdkit_extensions::ChainType chain_type)
{
    Q_ASSERT(chain_type == rdkit_extensions::ChainType::PEPTIDE ||
             chain_type == rdkit_extensions::ChainType::RNA);
    m_chain_type = chain_type;
    m_monomer_action->setText(chain_type == rdkit_extensions::ChainType::PEPTIDE
                                  ? "Sketch Residue..."
                                  // TODO: change this as part of SKETCH-2796
                                  : "Sketch Base...");
}

void CustomMonomerButton::sketchCustomMonomer(
    rdkit_extensions::ChainType chain_type)
{
    auto* dialog = new CustomMonomerDialog(chain_type, false, this);
    connect(dialog, &CustomMonomerDialog::customMonomerAccepted, this,
            &CustomMonomerButton::customMonomerAccepted);
    dialog->show();
}

void CustomMonomerButton::nextCheckState()
{
}

void CustomMonomerButton::paintEvent(QPaintEvent* event)
{
    QStylePainter painter(this);
    QStyleOptionToolButton option;
    initStyleOption(&option);
    if (isChecked() ||
        (isEnabled() && (option.state & QStyle::State_MouseOver))) {
        option.features |= QStyleOptionToolButton::HasMenu;
    }

    // Paint the full-row background and native menu indicator separately from
    // the label so the icon and text stay centered together.
    auto background = option;
    background.text.clear();
    background.icon = QIcon();
    painter.drawComplexControl(QStyle::CC_ToolButton, background);

    QStyleOptionButton label;
    label.initFrom(this);
    label.rect = option.rect;
    label.state = option.state;
    label.palette = option.palette;
    label.fontMetrics = option.fontMetrics;
    label.text = option.text;
    label.icon = option.icon;
    label.iconSize = option.iconSize;
    painter.setFont(option.font);
    painter.drawControl(QStyle::CE_PushButtonLabel, label);
}

} // namespace sketcher
} // namespace schrodinger

#include "schrodinger/sketcher/widget/custom_monomer_button.moc"
