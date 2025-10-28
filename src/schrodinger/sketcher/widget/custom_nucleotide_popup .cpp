#include "schrodinger/sketcher/widget/custom_nucleotide_popup.h"

#include <QLineEdit>
#include <QRegularExpression>
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
    setWindowFlags(Qt::Popup);
    ui.reset(new Ui::CustomNucleotidePopup());
    ui->setupUi(this);
    setStyleSheet(CUSTOM_NUCLEOTIDE_STYLE);

    auto alphanumeric_re = QRegularExpression("\\w+");
    auto* alphanumeric_validator = new QRegularExpressionValidator(alphanumeric_re, this);
    for (auto le : {ui->sugar_le, ui->base_le, ui->phosphate_le}) {
        le->setValidator(alphanumeric_validator);
        connect(le, &QLineEdit::textEdited, this, &CustomNucleotidePopup::onTextEdited);
    }
    // ui->sugar_le->setValidator(alphanumeric_validator);
    // ui->base_le->setValidator(alphanumeric_validator);
    // ui->phosphate_le->setValidator(alphanumeric_validator);
}

CustomNucleotidePopup::~CustomNucleotidePopup() = default;

// TODO: copy paintEvent from ModularPopup?

void CustomNucleotidePopup::setModel(SketcherModel* model)
{
    SketcherView::setModel(model);
    updateFromModel();
}

void CustomNucleotidePopup::onModelValuesChanged(const std::unordered_set<ModelKey>& keys)
{
    SketcherView::onModelValuesChanged(keys);
    if (keys.contains(ModelKey::CUSTOM_NUCLEOTIDE)) {
        updateFromModel();
    }
}

void CustomNucleotidePopup::onTextEdited()
{
    // make sure that the inputs are valid (i.e. non-empty)
    for (auto le : {ui->sugar_le, ui->base_le, ui->phosphate_le}) {
        auto le_contents = le->text();
        int pos = 0;
        if (le->validator()->validate(le_contents, pos) != QValidator::State::Acceptable) {
            return;
        }
    }
    std::tuple<std::string, std::string, std::string> nt = {
        ui->sugar_le->text().toStdString(),
        ui->base_le->text().toStdString(),
        ui->phosphate_le->text().toStdString()
    };
    getModel()->setValue(ModelKey::CUSTOM_NUCLEOTIDE, nt);
}

void CustomNucleotidePopup::updateFromModel()
{
    auto [sugar, base, phosphate] = getModel()->getCustomNucleotide();
    ui->sugar_le->setText(QString::fromStdString(sugar));
    ui->base_le->setText(QString::fromStdString(base));
    ui->phosphate_le->setText(QString::fromStdString(phosphate));
}

} // namespace sketcher
} // namespace schrodinger
