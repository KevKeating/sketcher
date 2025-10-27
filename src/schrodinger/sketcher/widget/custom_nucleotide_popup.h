#pragma once
#include <memory>

#include "schrodinger/sketcher/definitions.h"
#include "schrodinger/sketcher/widget/sketcher_view.h"

// class QString;

namespace Ui
{
class CustomNucleotidePopup;
}

namespace schrodinger
{
namespace sketcher
{

/**
 * Popup used to provide base choices for the nucleotide buttons
 */
class SKETCHER_API CustomNucleotidePopup : public SketcherView
{
  public:
    CustomNucleotidePopup(QWidget* parent = nullptr);
    ~CustomNucleotidePopup();
    
    void setModel(SketcherModel* model) override;

  protected:
    std::unique_ptr<Ui::CustomNucleotidePopup> ui;
    
    void onTextEdited();
    void updateFromModel();
};

} // namespace sketcher
} // namespace schrodinger
