#include "schrodinger/sketcher/widget/monomer_tool_widget.h"

#include <boost/assign.hpp>

#include <QButtonGroup>

#include "schrodinger/sketcher/model/sketcher_model.h"
#include "schrodinger/sketcher/sketcher_css_style.h"
#include "schrodinger/sketcher/ui/ui_monomer_tool_widget.h"
#include "schrodinger/sketcher/widget/tool_button_with_popup.h"
#include "schrodinger/sketcher/widget/nucleotide_popup.h"
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

    for (auto* btn_group :
         {ui->amino_monomer_group, ui->nucleic_monomer_group}) {
        for (auto* btn : btn_group->buttons()) {
            if (auto* btn_with_popup = dynamic_cast<ToolButtonWithPopup*>(btn)) {
                // make sure that we call the subclass's version of
                // setStyleSheet, since it's overriden but not virtual
                btn_with_popup->setStyleSheet(ATOM_ELEMENT_OR_MONOMER_STYLE);
            } else {
                btn->setStyleSheet(ATOM_ELEMENT_OR_MONOMER_STYLE);
            }
        }
    }
    ui->unk_btn->setStyleSheet(UNKNOWN_MONOMER_STYLE);
    ui->na_n_btn->setStyleSheet(UNKNOWN_MONOMER_STYLE);
    ui->amino_monomer_btn->setStyleSheet(TEXT_LINK_STYLE);
    ui->nucleic_monomer_btn->setStyleSheet(TEXT_LINK_STYLE);

    using ButtonAminoAcidBimapType = boost::bimap<QAbstractButton*, AminoAcidTool>;
    using ButtonNucleicAcidBimapType =
        boost::bimap<QAbstractButton*, NucleicAcidTool>;
    // clang-format off
    m_button_amino_acid_bimap =
        boost::assign::list_of<ButtonAminoAcidBimapType::relation>
            (ui->ala_btn, AminoAcidTool::ALA)
            (ui->arg_btn, AminoAcidTool::ARG)
            (ui->asn_btn, AminoAcidTool::ASN)
            (ui->asp_btn, AminoAcidTool::ASP)
            (ui->cys_btn, AminoAcidTool::CYS)
            (ui->gln_btn, AminoAcidTool::GLN)
            (ui->glu_btn, AminoAcidTool::GLU)
            (ui->gly_btn, AminoAcidTool::GLY)
            (ui->his_btn, AminoAcidTool::HIS)
            (ui->ile_btn, AminoAcidTool::ILE)
            (ui->leu_btn, AminoAcidTool::LEU)
            (ui->lys_btn, AminoAcidTool::LYS)
            (ui->met_btn, AminoAcidTool::MET)
            (ui->phe_btn, AminoAcidTool::PHE)
            (ui->pro_btn, AminoAcidTool::PRO)
            (ui->ser_btn, AminoAcidTool::SER)
            (ui->thr_btn, AminoAcidTool::THR)
            (ui->trp_btn, AminoAcidTool::TRP)
            (ui->tyr_btn, AminoAcidTool::TYR)
            (ui->val_btn, AminoAcidTool::VAL)
            (ui->unk_btn, AminoAcidTool::UNK);
    m_button_nucleic_acid_bimap =
        boost::assign::list_of<ButtonNucleicAcidBimapType::relation>
            (ui->na_a_btn, NucleicAcidTool::A)
            (ui->na_u_btn, NucleicAcidTool::U)
            (ui->na_g_btn, NucleicAcidTool::G)
            (ui->na_c_btn, NucleicAcidTool::C)
            (ui->na_t_btn, NucleicAcidTool::T)
            (ui->na_n_btn, NucleicAcidTool::N)
            (ui->na_r_btn, NucleicAcidTool::R)
            (ui->na_dr_btn, NucleicAcidTool::dR)
            (ui->na_p_btn, NucleicAcidTool::P)
            (ui->na_rna_btn, NucleicAcidTool::RNA_NUCLEOTIDE)
            (ui->na_dna_btn, NucleicAcidTool::DNA_NUCLEOTIDE);
    // clang-format on

    connect(ui->amino_or_nucleic_group, &QButtonGroup::buttonClicked, this,
            &MonomerToolWidget::onAminoOrNucleicBtnClicked);
    connect(ui->amino_monomer_group, &QButtonGroup::buttonClicked, this,
            &MonomerToolWidget::onAminoAcidClicked);
    connect(ui->nucleic_monomer_group, &QButtonGroup::buttonClicked, this,
            &MonomerToolWidget::onNucleicAcidClicked);

    m_rna_popup = new NucleotidePopup(NucleicAcidTool::RNA_NUCLEOTIDE, ModelKey::RNA_NUCLEOBASE, "R", "U", this);
    m_dna_popup = new NucleotidePopup(NucleicAcidTool::DNA_NUCLEOTIDE, ModelKey::DNA_NUCLEOBASE, "dR", "T", this);
    ui->na_rna_btn->setPopupWidget(m_rna_popup);
    ui->na_dna_btn->setPopupWidget(m_dna_popup);
}

MonomerToolWidget::~MonomerToolWidget() = default;

void MonomerToolWidget::setModel(SketcherModel* model)
{
    AbstractDrawToolWidget::setModel(model);
    m_rna_popup->setModel(model);
    m_dna_popup->setModel(model);
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
    ui->na_rna_btn->setEnumItem(static_cast<int>(model->getRNANucleobase()));
    ui->na_dna_btn->setEnumItem(static_cast<int>(model->getDNANucleobase()));
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

template <typename T> static void
on_tool_clicked(SketcherModel* model, const ModelKey key,
                   const boost::bimap<QAbstractButton*, T>& button_tool_bimap,
                   QAbstractButton* button)
{
    auto tool = button_tool_bimap.left.at(button);
    auto tool_as_variant = QVariant::fromValue(tool);
    if (model->hasActiveSelection()) {
        // ping the model to indicate that we want to replace the selection
        // without changing the tool
        model->pingValue(key, tool_as_variant);
    } else {
        // change the tool
        model->setValue(key, tool_as_variant);
    }
}

void MonomerToolWidget::onAminoAcidClicked(QAbstractButton* button)
{
    if (!m_updates_to_model_paused) {
        on_tool_clicked<AminoAcidTool>(getModel(), ModelKey::AMINO_ACID_TOOL,
                                      m_button_amino_acid_bimap, button);
    }
}

void MonomerToolWidget::onNucleicAcidClicked(QAbstractButton* button)
{
    if (!m_updates_to_model_paused) {
        on_tool_clicked<NucleicAcidTool>(getModel(), ModelKey::NUCLEIC_ACID_TOOL,
                                        m_button_nucleic_acid_bimap, button);
    }
    // TODO: handle full nucleotide buttons
}

} // namespace sketcher
} // namespace schrodinger
