#include "schrodinger/sketcher/widget/custom_nucleotide_popup.h"


#include "schrodinger/sketcher/sketcher_css_style.h"
#include "schrodinger/sketcher/model/sketcher_model.h"
#include "schrodinger/sketcher/ui/ui_custom_nucleotide_popup.h"

namespace schrodinger
{
namespace sketcher
{

CustomNucleotidePopup::CustomNucleotidePopup(const NucleicAcidTool tool,
                                 const ModelKey model_key, const QString& sugar,
                                 const QString& u_or_t, QWidget* parent) :
    ModularPopup(parent),
    m_tool(tool),
    m_model_key(model_key),
    m_sugar(sugar),
    m_u_or_t(u_or_t)
{
    ui.reset(new Ui::CustomNucleotidePopup());
    ui->setupUi(this);
    setButtonGroup(ui->group);
    setStyleSheet(ATOM_ELEMENT_OR_MONOMER_STYLE);

    // add text to the buttons
    QString btn_name_fmt("%1(%2)P");
    ui->a_btn->setText(btn_name_fmt.arg(sugar, "A"));
    ui->c_btn->setText(btn_name_fmt.arg(sugar, "C"));
    ui->g_btn->setText(btn_name_fmt.arg(sugar, "G"));
    ui->u_or_t_btn->setText(btn_name_fmt.arg(sugar, u_or_t));
    ui->n_btn->setText(btn_name_fmt.arg(sugar, "N"));
}

CustomNucleotidePopup::~CustomNucleotidePopup() = default;