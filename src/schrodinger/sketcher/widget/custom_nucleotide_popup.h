#pragma once
#include <memory>

#include "schrodinger/sketcher/definitions.h"
#include "schrodinger/sketcher/widget/tool_button_with_popup.h"

class QString;

namespace Ui
{
class CustomNucleotidePopup;
}

namespace schrodinger
{
namespace sketcher
{

enum class NucleicAcidTool;
enum class ModelKey;
enum class StdNucleobase;

/**
 * Popup used to provide base choices for the nucleotide buttons
 */
class SKETCHER_API CustomNucleotidePopup : public ToolButtonWithPopup
{
  public:
    CustomNucleotidePopup(const NucleicAcidTool tool, const ModelKey model_key,
                    const QString& sugar, const QString& u_or_t,
                    QWidget* parent = nullptr);
    ~CustomNucleotidePopup();

  protected:
    QString m_sugar;
    QString m_base;
    QString m_phosphate;

    std::unique_ptr<Ui::CustomNucleotidePopup> ui;
    void generateButtonPackets() override;
    int getButtonIDToCheck() override;
};

} // namespace sketcher
} // namespace schrodinger
