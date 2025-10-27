#include "schrodinger/sketcher/widget/custom_nucleotide_popup.h"

#include <QLineEdit>
#include <QRegularExpressionValidator>

#include "schrodinger/sketcher/sketcher_css_style.h"
#include "schrodinger/sketcher/model/sketcher_model.h"
#include "schrodinger/sketcher/ui/ui_custom_nucleotide_popup.h"

namespace schrodinger
{
namespace sketcher
{

CustomNucleotidePopup::CustomNucleotidePopup(QWidget* parent) :
    SketcherView(parent)
{
    ui.reset(new Ui::CustomNucleotidePopup());
    ui->setupUi(this);
    setStyleSheet(CUSTOM_NUCLEOTIDE_STYLE);

    auto* alphanumeric_validator = new QRegularExpressionValidator("\\w+", this);
    for (auto le : {ui->sugar_le, ui->base_le, ui->phosphate_le}) {
        le->setValidator(alphanumeric_validator);
        connect(le, &QLineEdit::textEdited, this, &CustomNucleotidePopup::onTextEdited);
    }
    // ui->sugar_le->setValidator(alphanumeric_validator);
    // ui->base_le->setValidator(alphanumeric_validator);
    // ui->phosphate_le->setValidator(alphanumeric_validator);
}

void CustomNucleotidePopup::onTextEdited()
{
    // make sure that the inputs are valid (i.e. non-empty)
    for (auto le : {ui->sugar_le, ui->base_le, ui->phosphate_le}) {
        if (le->validator->validate(le->text()) != QValidator::State::Acceptable) {
            return;
        }
    }
    std::tuple<std::string, std::string, std::string> nt({
        ui->sugar_le->text().toStdString(),
        ui->base_le->text().toStdString(),
        ui->phosphate_le->text().toStdString()
    };
    model()->setValue(ModelKey::CUSTOM_NUCLEOTIDE, nt);
}


CustomNucleotidePopup::~CustomNucleotidePopup() = default;