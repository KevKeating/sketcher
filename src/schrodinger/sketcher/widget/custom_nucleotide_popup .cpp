#include "schrodinger/sketcher/widget/custom_nucleotide_popup.h"

#include <QRegularExpressionValidator>

#include "schrodinger/sketcher/sketcher_css_style.h"
#include "schrodinger/sketcher/model/sketcher_model.h"
#include "schrodinger/sketcher/ui/ui_custom_nucleotide_popup.h"

namespace schrodinger
{
namespace sketcher
{

CustomNucleotidePopup::CustomNucleotidePopup(QWidget* parent) :
    QWidget(parent)
{
    ui.reset(new Ui::CustomNucleotidePopup());
    ui->setupUi(this);
    setStyleSheet(CUSTOM_NUCLEOTIDE_STYLE);

    auto* alphanumeric_validator = new QRegularExpressionValidator("\\w+", this);
    ui->sugar_le->setValidator(alphanumeric_validator);
    ui->base_le->setValidator(alphanumeric_validator);
    ui->phosphate_le->setValidator(alphanumeric_validator);
}

CustomNucleotidePopup::~CustomNucleotidePopup() = default;