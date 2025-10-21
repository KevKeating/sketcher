#pragma once
#include <memory>

#include "schrodinger/sketcher/definitions.h"
#include "schrodinger/sketcher/widget/modular_popup.h"

class QString;

namespace Ui
{
class NucleotidePopup;
}

namespace schrodinger
{
namespace sketcher
{

enum class NucleicAcidTool;
enum class ModelKey;

// TODO: override set popup to trigger initial state of the button?

/**
 * Popup used to provide functionality choices for a selection tool button.
 */
class SKETCHER_API NucleotidePopup : public ModularPopup
{
    Q_OBJECT

  public:
    NucleotidePopup(const NucleicAcidTool tool, const ModelKey model_key,
                    const QString& sugar, const QString& u_or_t,
                    QWidget* parent = nullptr);
    ~NucleotidePopup();

  protected:
    NucleicAcidTool m_tool;
    ModelKey m_model_key;
    QString m_sugar;
    QString m_u_or_t;

    std::unique_ptr<Ui::NucleotidePopup> ui;
    void generateButtonPackets() override;
    int getButtonIDToCheck() override;
};

} // namespace sketcher
} // namespace schrodinger
