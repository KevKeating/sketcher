#include "schrodinger/sketcher/dialog/custom_monomer_dialog.h"

#include <algorithm>
#include <map>
#include <unordered_set>

#include <QDialogButtonBox>
#include <QPushButton>
#include <QStringList>

#include <rdkit/GraphMol/MolOps.h>

#include "schrodinger/rdkit_extensions/file_format.h"
#include "schrodinger/rdkit_extensions/monomer_mol.h"
#include "schrodinger/rdkit_extensions/rgroup.h"
#include "schrodinger/sketcher/dialog/message_box_dialog.h"
#include "schrodinger/sketcher/public_constants.h"
#include "schrodinger/sketcher/rdkit/monomeric.h"
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

/**
 * @return a list of all R-groups that appear more than once in the specified
 * molecule
 */
static QStringList get_duplicate_r_groups(const RDKit::ROMol& mol)
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
    return duplicate_r_groups;
    // TODO: have this return integers instead of strings, move stringification
    //       from format_r_groups to format_duplicate_r_groups, get rid of
    //       format_r_groups, and change name of format_duplicate_r_groups to
    //       format_r_group_list
}

/**
 * @return formatted text listing all R-groups in the given list of R-groups
 */
static QString format_duplicate_r_groups(QStringList duplicate_r_groups)
{
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

static QString format_r_groups(const std::vector<int>& r_group_numbers)
{
    QStringList r_groups;
    for (const auto r_group_num : r_group_numbers) {
        r_groups.append("R" + QString::number(r_group_num));
    }
    return format_duplicate_r_groups(r_groups);
}

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

void CustomMonomerDialog::setRequiredAttachmentPoints(
    std::vector<int> required_attachment_points)
{
    // TODO: this should probably be the responsibility of the caller
    std::erase_if(required_attachment_points,
                  [](const int attachment_point) {
                      return attachment_point <= 0;
                  });
    std::ranges::sort(required_attachment_points);
    // TODO: use unique_copy here instead?
    const auto unique_end = std::ranges::unique(required_attachment_points);
    required_attachment_points.erase(unique_end.begin(), unique_end.end());
    m_required_attachment_points = std::move(required_attachment_points);
}

std::vector<int> CustomMonomerDialog::getMissingRequiredAttachmentPoints(
    const std::string& smiles) const
{
    std::unordered_set<int> present_attachment_points;
    for (const auto& attachment_point :
         get_attachment_points_for_smiles(smiles)) {
        present_attachment_points.insert(attachment_point.first);
    }

    std::vector<int> missing_attachment_points;
    std::ranges::copy_if(
        m_required_attachment_points,
        std::back_inserter(missing_attachment_points),
        [&present_attachment_points](const int attachment_point) {
            return !present_attachment_points.contains(attachment_point);
        });
    return missing_attachment_points;
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
    const auto duplicate_r_groups = get_duplicate_r_groups(*mol);
    if (!duplicate_r_groups.empty()) {
        show_error_dialog(
            "Invalid Attachment Points",
            "Multiple " + format_duplicate_r_groups(duplicate_r_groups) +
                " attachment points found. All attachment points must be unique.",
            this);
        return;
    }

    const auto smiles =
        ui->sketcher_widget->getString(Format::EXTENDED_SMILES);
    const auto type = ui->monomer_type_combo->currentData().value<ChainType>();
    // TODO: this should mol instead of round-tripping through SMILES
    const auto missing_attachment_points =
        getMissingRequiredAttachmentPoints(smiles);
    if (!missing_attachment_points.empty()) {
        // TODO: move text formatting to static method
        const bool plural = missing_attachment_points.size() != 1;
        const auto attachment_points =
            format_r_groups(missing_attachment_points);
        const auto warning_text =
            attachment_points + (plural ? " have" : " has") +
            " been removed from this monomer but " +
            (plural ? "are" : "is") +
            " currently bound. Continuing will remove " +
            (plural ? "these connections." : "this connection.");
        auto* warning_dialog = show_warning_dialog(
            "Remove Bound Connections?", warning_text, this);
        connect(warning_dialog, &MessageBoxDialog::accepted, this,
                [this, smiles, type]() {
                    emit customMonomerAccepted(smiles, type);
                    ModalDialog::accept();
                });
        return;
    }

    emit customMonomerAccepted(smiles, type);
    ModalDialog::accept();
}

} // namespace sketcher
} // namespace schrodinger

#include "schrodinger/sketcher/dialog/custom_monomer_dialog.moc"
