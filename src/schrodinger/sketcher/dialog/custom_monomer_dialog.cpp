#include "schrodinger/sketcher/dialog/custom_monomer_dialog.h"

#include <map>

#include <QDialogButtonBox>
#include <QPushButton>
#include <QStringList>

#include <rdkit/GraphMol/MolOps.h>

#include "schrodinger/rdkit_extensions/file_format.h"
#include "schrodinger/rdkit_extensions/monomer_mol.h"
#include "schrodinger/rdkit_extensions/rgroup.h"
#include "schrodinger/sketcher/dialog/error_dialog.h"
#include "schrodinger/sketcher/public_constants.h"
#include "schrodinger/sketcher/sketcher_css_style.h"
#include "schrodinger/sketcher/sketcher_widget.h"
#include "schrodinger/sketcher/ui/ui_custom_monomer_dialog.h"

using schrodinger::rdkit_extensions::ChainType;
using schrodinger::rdkit_extensions::Format;
using schrodinger::rdkit_extensions::get_r_group_number;

namespace schrodinger
{
namespace sketcher
{

namespace
{

QString format_duplicate_r_groups(const RDKit::ROMol& mol)
{
    std::map<unsigned int, unsigned int> r_group_counts;
    for (const auto* atom : mol.atoms()) {
        if (const auto r_group_num = get_r_group_number(atom)) {
            ++r_group_counts[*r_group_num];
        }
    }

    QStringList duplicate_r_groups;
    for (const auto& [r_group_num, count] : r_group_counts) {
        if (count > 1) {
            duplicate_r_groups.append("R" + QString::number(r_group_num));
        }
    }

    if (duplicate_r_groups.empty()) {
        return {};
    }
    if (duplicate_r_groups.size() == 1) {
        return duplicate_r_groups.front();
    }

    const auto last_r_group = duplicate_r_groups.takeLast();
    const auto separator = duplicate_r_groups.size() == 1 ? " " : ", ";
    return duplicate_r_groups.join(", ") + separator + "and " + last_r_group;
}

} // namespace

CustomMonomerDialog::CustomMonomerDialog(QWidget* parent) : ModalDialog(parent)
{
    ui.reset(new Ui::CustomMonomerDialog());
    setupDialogUI(*ui);
    setStyleSheet(CUSTOM_MONOMER_DIALOG_STYLE);
    setWindowTitle("Sketch Custom Monomer");
    ui->sketcher_widget->setInterfaceType(InterfaceType::ATOMISTIC);

    // remove the standard margins set by ModalDialog so that there's no gap
    // between the SketcherWidget and the edge of the dialog
    m_dlg_layout->setContentsMargins(0, 0, 0, 0);
    qobject_cast<QVBoxLayout*>(layout())->setContentsMargins(0, 0, 0, 0);

    ui->monomer_type_combo->addItem("Amino acid",
                                    QVariant::fromValue(ChainType::PEPTIDE));
    ui->monomer_type_combo->addItem("Nucleic acid",
                                    QVariant::fromValue(ChainType::RNA));
    ui->monomer_type_combo->addItem("CHEM",
                                    QVariant::fromValue(ChainType::CHEM));

    connect(ui->sketcher_widget, &SketcherWidget::moleculeChanged, this,
            &CustomMonomerDialog::updateOkButton);
    connect(ui->sketcher_widget, &SketcherWidget::representationChanged, this,
            &CustomMonomerDialog::updateOkButton);
    updateOkButton();
}

CustomMonomerDialog::~CustomMonomerDialog() = default;

void CustomMonomerDialog::setMonomerType(
    const rdkit_extensions::ChainType chain_type)
{
    auto index =
        ui->monomer_type_combo->findData(QVariant::fromValue(chain_type));
    if (index < 0) {
        throw std::runtime_error("Chain type not found");
    }
    ui->monomer_type_combo->setCurrentIndex(index);
}

void CustomMonomerDialog::setMonomerTypeInputEnabled(const bool enabled)
{
    ui->monomer_type_combo->setEnabled(enabled);
}

void CustomMonomerDialog::addSMILES(const std::string& smiles)
{
    ui->sketcher_widget->addFromString(smiles, Format::EXTENDED_SMILES);
}

void CustomMonomerDialog::updateOkButton()
{
    bool valid = false;
    try {
        if (!ui->sketcher_widget->isEmpty()) {
            auto mol = ui->sketcher_widget->getRDKitMolecule();
            valid = mol->getNumAtoms() > 0 &&
                    RDKit::MolOps::getMolFrags(*mol, false).size() == 1;
            if (valid) {
                ui->sketcher_widget->getString(Format::EXTENDED_SMILES);
            }
        }
    } catch (const std::exception&) {
        valid = false;
    }
    ui->button_box->button(QDialogButtonBox::Ok)->setEnabled(valid);
}

void CustomMonomerDialog::accept()
{
    const auto mol = ui->sketcher_widget->getRDKitMolecule();
    const auto duplicate_r_groups = format_duplicate_r_groups(*mol);
    if (!duplicate_r_groups.isEmpty()) {
        show_error_dialog(
            "Invalid Attachment Points",
            "Multiple " + duplicate_r_groups +
                " attachment points found. All attachment points must be unique.",
            this);
        return;
    }

    auto smiles = ui->sketcher_widget->getString(Format::EXTENDED_SMILES);
    auto type = ui->monomer_type_combo->currentData().value<ChainType>();
    emit customMonomerAccepted(smiles, type);
    ModalDialog::accept();
}

} // namespace sketcher
} // namespace schrodinger

#include "schrodinger/sketcher/dialog/custom_monomer_dialog.moc"
