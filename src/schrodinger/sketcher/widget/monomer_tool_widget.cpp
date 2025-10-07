#include "schrodinger/sketcher/widget/monomer_tool_widget.h"

#include <boost/assign.hpp>

#include <QButtonGroup>

#include "schrodinger/sketcher/model/sketcher_model.h"
#include "schrodinger/sketcher/sketcher_css_style.h"
#include "schrodinger/sketcher/ui/ui_monomer_tool_widget.h"
#include "schrodinger/sketcher/widget/widget_utils.h"

namespace schrodinger
{
namespace sketcher
{

// TODO: confirm that I actually need this
class PauseUpdatesToModelRAII
{
  public:
    PauseUpdatesToModelRAII(MonomerToolWidget* monomer_tool_widget);
    ~PauseUpdatesToModelRAII();

  protected:
    MonomerToolWidget* m_monomer_tool_widget = nullptr;
    bool m_prev_value = false;
};

PauseUpdatesToModelRAII::PauseUpdatesToModelRAII(
    MonomerToolWidget* monomer_tool_widget) :
    m_monomer_tool_widget(monomer_tool_widget),
    m_prev_value(monomer_tool_widget->m_updates_to_model_paused)
{
    m_monomer_tool_widget->m_updates_to_model_paused = true;
}

PauseUpdatesToModelRAII::~PauseUpdatesToModelRAII()
{
    m_monomer_tool_widget->m_updates_to_model_paused = m_prev_value;
}

MonomerToolWidget::MonomerToolWidget(QWidget* parent) :
    AbstractDrawToolWidget(parent)
{
    ui.reset(new Ui::MonomerToolWidget());
    ui->setupUi(this);

    for (auto btn_group :
         {ui->amino_monomer_group, ui->nucleic_monomer_group}) {
        for (auto btn : btn_group->buttons()) {
            btn->setStyleSheet(ATOM_ELEMENT_STYLE);
        }
    }
    ui->amino_monomer_btn->setStyleSheet(TEXT_LINK_STYLE);
    ui->nucleic_monomer_btn->setStyleSheet(TEXT_LINK_STYLE);

    using ButtonAminoAcidBimapType = boost::bimap<QAbstractButton*, AminoAcid>;
    using ButtonNucleicAcidBimapType =
        boost::bimap<QAbstractButton*, NucleicAcid>;
    // clang-format off
    m_button_amino_acid_bimap =
        boost::assign::list_of<ButtonAminoAcidBimapType::relation>
            (ui->ala_btn, AminoAcid::ALA)
            (ui->arg_btn, AminoAcid::ARG)
            (ui->asn_btn, AminoAcid::ASN)
            (ui->asp_btn, AminoAcid::ASP)
            (ui->cys_btn, AminoAcid::CYS)
            (ui->gln_btn, AminoAcid::GLN)
            (ui->glu_btn, AminoAcid::GLU)
            (ui->gly_btn, AminoAcid::GLY)
            (ui->his_btn, AminoAcid::HIS)
            (ui->ile_btn, AminoAcid::ILE)
            (ui->leu_btn, AminoAcid::LEU)
            (ui->lys_btn, AminoAcid::LYS)
            (ui->met_btn, AminoAcid::MET)
            (ui->phe_btn, AminoAcid::PHE)
            (ui->pro_btn, AminoAcid::PRO)
            (ui->ser_btn, AminoAcid::SER)
            (ui->thr_btn, AminoAcid::THR)
            (ui->trp_btn, AminoAcid::TRP)
            (ui->tyr_btn, AminoAcid::TYR)
            (ui->val_btn, AminoAcid::VAL)
            (ui->unk_btn, AminoAcid::UNK);
    m_button_nucleic_acid_bimap =
        boost::assign::list_of<ButtonNucleicAcidBimapType::relation>
            (ui->rna_a_btn, NucleicAcid::A)
            (ui->rna_u_btn, NucleicAcid::U)
            (ui->rna_g_btn, NucleicAcid::G)
            (ui->rna_c_btn, NucleicAcid::C)
            (ui->rna_n_btn, NucleicAcid::N)
            (ui->rna_r_btn, NucleicAcid::R)
            (ui->rna_p_btn, NucleicAcid::P)
            (ui->rna_r_p_btn, NucleicAcid::RP)
            (ui->dna_a_btn, NucleicAcid::dA)
            (ui->dna_t_btn, NucleicAcid::dT)
            (ui->dna_g_btn, NucleicAcid::dG)
            (ui->dna_c_btn, NucleicAcid::dC)
            (ui->dna_n_btn, NucleicAcid::dN)
            (ui->dna_r_btn, NucleicAcid::dR)
            // the phosphate is the same for DNA or RNA
            (ui->dna_p_btn, NucleicAcid::P)
            (ui->dna_r_p_btn, NucleicAcid::dRP);
    // clang-format on

    connect(ui->amino_or_nucleic_group, &QButtonGroup::buttonClicked, this,
            &MonomerToolWidget::onAminoOrNucleicBtnClicked);
    connect(ui->amino_monomer_group, &QButtonGroup::buttonClicked, this,
            &MonomerToolWidget::onAminoAcidClicked);
    connect(ui->nucleic_monomer_group, &QButtonGroup::buttonClicked, this,
            &MonomerToolWidget::onNucleicAcidClicked);
}

MonomerToolWidget::~MonomerToolWidget() = default;

void MonomerToolWidget::setModel(SketcherModel* model)
{
    AbstractDrawToolWidget::setModel(model);
    updateCheckedButton();
}

std::unordered_set<QAbstractButton*> MonomerToolWidget::getCheckableButtons()
{
    auto buttons = AbstractDrawToolWidget::getCheckableButtons();
    for (auto group : {ui->amino_monomer_group, ui->nucleic_monomer_group}) {
        for (auto button : group->buttons()) {
            buttons.insert(button);
        }
    }
    return buttons;
}

void MonomerToolWidget::updateCheckedButton()
{
    auto model = getModel();
    if (model == nullptr) {
        return;
    }
    PauseUpdatesToModelRAII update_pauser(this);
    QAbstractButton* amino_button = nullptr;
    QAbstractButton* nucleic_button = nullptr;
    bool has_sel = model->hasActiveSelection();
    if (model->getDrawTool() == DrawTool::MONOMER) {
        if (model->getMonomerToolType() == MonomerToolType::AMINO_ACID) {
            ui->amino_monomer_btn->setChecked(true);
            ui->amino_or_nucleic_stack->setCurrentWidget(ui->amino_page);
            if (!has_sel) {
                auto amino_acid = model->getAminoAcidTool();
                amino_button = m_button_amino_acid_bimap.right.at(amino_acid);
            }
        } else {
            ui->nucleic_monomer_btn->setChecked(true);
            ui->amino_or_nucleic_stack->setCurrentWidget(ui->nucleic_page);
            if (!has_sel) {
                auto nucleic_acid = model->getNucleicAcidTool();
                nucleic_button = m_button_nucleic_acid_bimap.right.at(nucleic_acid);
            }
        }
    }
    check_button_or_uncheck_group(amino_button, ui->amino_monomer_group);
    check_button_or_uncheck_group(nucleic_button, ui->nucleic_monomer_group);
}

void MonomerToolWidget::onAminoOrNucleicBtnClicked(QAbstractButton* button)
{
    if (m_updates_to_model_paused) {
        return;
    }
    QWidget* page = nullptr;
    MonomerToolType tool_type;
    if (button == ui->amino_monomer_btn) {
        page = ui->amino_page;
        tool_type = MonomerToolType::AMINO_ACID;
    } else {
        page = ui->nucleic_page;
        tool_type = MonomerToolType::NUCLEIC_ACID;
    }
    ui->amino_or_nucleic_stack->setCurrentWidget(page);
    getModel()->setValues(
        {{ModelKey::DRAW_TOOL, QVariant::fromValue(DrawTool::MONOMER)},
         {ModelKey::MONOMER_TOOL_TYPE, QVariant::fromValue(tool_type)}});
    updateCheckedButton();
}

template <typename T> void
on_residue_clicked(SketcherModel* model, const ModelKey key,
                   const boost::bimap<QAbstractButton*, T>& button_res_bimap,
                   QAbstractButton* button)
{
    auto res = button_res_bimap.left.at(button);
    auto res_as_variant = QVariant::fromValue(res);
    if (model->hasActiveSelection()) {
        // ping the model to indicate that we want to replace the selection
        // without changing the tool
        model->pingValue(key, res_as_variant);
    } else {
        // change the tool
        model->setValue(key, res_as_variant);
    }
}

void MonomerToolWidget::onAminoAcidClicked(QAbstractButton* button)
{
    if (!m_updates_to_model_paused) {
        on_residue_clicked<AminoAcid>(getModel(), ModelKey::AMINO_ACID,
                                      m_button_amino_acid_bimap, button);
    }
}

void MonomerToolWidget::onNucleicAcidClicked(QAbstractButton* button)
{
    if (!m_updates_to_model_paused) {
        on_residue_clicked<NucleicAcid>(getModel(), ModelKey::NUCLEIC_ACID,
                                        m_button_nucleic_acid_bimap, button);
    }
}

} // namespace sketcher
} // namespace schrodinger
