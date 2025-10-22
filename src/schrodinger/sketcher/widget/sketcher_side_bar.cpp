#include "schrodinger/sketcher/widget/sketcher_side_bar.h"

#include <QButtonGroup>
#include <QToolButton>

#include "schrodinger/sketcher/sketcher_css_style.h"
#include "schrodinger/sketcher/ui/ui_sketcher_side_bar.h"

namespace schrodinger
{
namespace sketcher
{

SketcherSideBar::SketcherSideBar(QWidget* parent) : SketcherView(parent)
{
    ui.reset(new Ui::SketcherSideBar());
    ui->setupUi(this);

    // Force maximum width
    setMaximumWidth(100);

    ui->title_lbl->setStyleSheet(PALETTE_TITLE_STYLE);

    // Forward required slots
    connect(ui->select_options_wdg, &SelectOptionsWidget::selectAllRequested,
            this, &SketcherSideBar::selectAllRequested);
    connect(ui->select_options_wdg,
            &SelectOptionsWidget::clearSelectionRequested, this,
            &SketcherSideBar::clearSelectionRequested);
    connect(ui->select_options_wdg,
            &SelectOptionsWidget::invertSelectionRequested, this,
            &SketcherSideBar::invertSelectionRequested);
    connect(ui->atomistic_or_monomeric_group, &QButtonGroup::buttonClicked, this,
            &SketcherSideBar::onAtomisticOrMonomerButtonClicked);
}

SketcherSideBar::~SketcherSideBar() = default;

void SketcherSideBar::setModel(SketcherModel* model)
{
    SketcherView::setModel(model);
    ui->select_options_wdg->setModel(model);
    ui->draw_tools_wdg->setModel(model);
    ui->ring_tool_wdg->setModel(model);
    ui->enumeration_tool_wdg->setModel(model);
    ui->monomeric_wdg->setModel(model);
}

void SketcherSideBar::updateWidgetsEnabled()
{
    auto model = getModel();
    auto has_selection = model->hasActiveSelection();
    std::string title = has_selection ? "EDIT ACTIONS" : "DRAW";
    ui->title_lbl->setText(QString::fromStdString(title));

    auto interface_type = model->getInterfaceType();
    bool show_atom_mono_buttons =
        interface_type == InterfaceType::ATOMISTIC_OR_MONOMERIC;
    ui->atomistic_btn->setVisible(show_atom_mono_buttons);
    ui->monomeric_btn->setVisible(show_atom_mono_buttons);
    if (!show_atom_mono_buttons) {
        // only one type of interface is allowed, so switch to that one
        if (interface_type == InterfaceType::ATOMISTIC) {
            ui->atomistic_or_monomeric_stack->setCurrentWidget(ui->atomistic_page);
        } else {
            ui->atomistic_or_monomeric_stack->setCurrentWidget(ui->monomeric_page);
        }
    } else {
        // both types of interface are allowed, but we cant' have both in the
        // workspace at the same time, so disable the atomistic or monomer
        // buttons if there's already a molecule of the other type
        auto mol_type = model->getMoleculeType();
        ui->atomistic_btn->setEnabled(mol_type != MoleculeType::MONOMERIC);
        ui->monomeric_btn->setEnabled(mol_type != MoleculeType::ATOMISTIC);
    }
}

void SketcherSideBar::onAtomisticOrMonomerButtonClicked(QAbstractButton* button)
{
    ToolSet tool_set = button == ui->atomistic_btn ? ToolSet::ATOMISTIC : ToolSet::MONOMERIC;
    // this setValue() call will trigger a call to updateCheckState
    getModel()->setValue(ModelKey::TOOL_SET, tool_set);
}      

void SketcherSideBar::updateCheckState()
{
    static const std::unordered_set<DrawTool> ATOMISTIC_TOOLS = {
        DrawTool::ATOM, DrawTool::BOND,        DrawTool::CHARGE,
        DrawTool::RING, DrawTool::ENUMERATION, DrawTool::EXPLICIT_H,
    };
    QWidget* page;
    std::optional<DrawTool> new_draw_tool = std::nullopt;
    auto model = getModel();
    auto cur_draw_tool = model->getDrawTool();
    auto tool_set = model->getToolSet();
    if (tool_set == ToolSet::ATOMISTIC) {
        page = ui->atomistic_page;
        if (!ATOMISTIC_TOOLS.contains(cur_draw_tool)) {
            new_draw_tool = m_previous_atomistic_draw_tool;
        }
    } else {
        page = ui->monomeric_page;
        if (cur_draw_tool != DrawTool::MONOMER) {
            new_draw_tool = DrawTool::MONOMER;
            if (ATOMISTIC_TOOLS.contains(cur_draw_tool)) {
                // remember what atomistic draw tool we switched away from so
                // that we can switch back to it later
                m_previous_atomistic_draw_tool = cur_draw_tool;
            }
        }
    }
    ui->atomistic_or_monomeric_stack->setCurrentWidget(page);
    if (new_draw_tool.has_value() && !(model->hasActiveSelection())) {
        model->setValue(ModelKey::DRAW_TOOL, *new_draw_tool);
    }
}

} // namespace sketcher
} // namespace schrodinger

#include "schrodinger/sketcher/widget/sketcher_side_bar.moc"
