#pragma once

#include <memory>
#include <unordered_set>

#include <boost/bimap.hpp>

#include "schrodinger/sketcher/definitions.h"
#include "schrodinger/sketcher/widget/abstract_draw_tool_widget.h"

class QAbstractButton;
class QWidget;

namespace Ui
{
class MonomerToolWidget;
}

namespace schrodinger
{
namespace sketcher
{

enum class AminoAcidTool;
enum class NucleicAcidTool;
class PauseUpdatesToModelRAII;

/**
 * Side bar tool for selecting monomers
 */
class SKETCHER_API MonomerToolWidget : public AbstractDrawToolWidget
{
    // Q_OBJECT

  public:
    MonomerToolWidget(QWidget* parent = nullptr);
    ~MonomerToolWidget();

    void setModel(SketcherModel* model) override;
    void updateCheckedButton() override;
    std::unordered_set<QAbstractButton*> getCheckableButtons() override;

//   signals:


  protected:
    std::unique_ptr<Ui::MonomerToolWidget> ui;
    boost::bimap<QAbstractButton*, AminoAcidTool> m_button_amino_acid_bimap;
    boost::bimap<QAbstractButton*, NucleicAcidTool> m_button_nucleic_acid_bimap;
    bool m_updates_to_model_paused = false;

    void onAminoOrNucleicBtnClicked(QAbstractButton* button);
    void onAminoAcidClicked(QAbstractButton* button);
    void onNucleicAcidClicked(QAbstractButton* button);
    void updateMonomerButtons();

  friend class PauseUpdatesToModelRAII;
};

} // namespace sketcher
} // namespace schrodinger
