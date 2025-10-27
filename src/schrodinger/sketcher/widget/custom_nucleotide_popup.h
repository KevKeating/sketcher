#pragma once
#include <memory>

#include <QWidget>

#include "schrodinger/sketcher/definitions.h"

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
class SKETCHER_API CustomNucleotidePopup : public QWidget
{
  public:
    CustomNucleotidePopup(QWidget* parent = nullptr);
    ~CustomNucleotidePopup();

  protected:
    std::unique_ptr<Ui::CustomNucleotidePopup> ui;
};

} // namespace sketcher
} // namespace schrodinger
