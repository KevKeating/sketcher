#include "schrodinger/sketcher/widget/sketcher_side_bar.h"

#include <QButtonGroup>
#include <QToolButton>

#include "schrodinger/sketcher/sketcher_css_style.h"
#include "schrodinger/sketcher/model/sketcher_model.h"
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
    // TODO: these buttons should also activate the most recently used atom or
    // monomer tool
    connect(ui->atomistic_or_monomer_group, &QButtonGroup::buttonClicked, this,
            &SketcherSideBar::onAtomisticOrMonomerButtonClicked);
    // TODO: make sure that programatically switching the stack contents changes
    //       the checked button
    // TODO: option to set to atomistic-only or monomer-only
}

SketcherSideBar::~SketcherSideBar() = default;

void SketcherSideBar::setModel(SketcherModel* model)
{
    SketcherView::setModel(model);
    ui->select_options_wdg->setModel(model);
    ui->draw_tools_wdg->setModel(model);
    ui->ring_tool_wdg->setModel(model);
    ui->enumeration_tool_wdg->setModel(model);
    ui->monomer_wdg->setModel(model);
}

// TODO: make sure that we switch atomistic versus monomeric if tool gets
//       switched in SketcherModel (otherwise keyboard shortcuts won't update
//       the side bar)

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
    ui->monomer_btn->setVisible(show_atom_mono_buttons);
    if (!show_atom_mono_buttons) {
        // only one type of interface is enabled, so switch to that one
        if (interface_type == InterfaceType::ATOMISTIC) {
            ui->atomistic_or_monomer_stack->setCurrentWidget(ui->atomistic_page);
        } else {
            ui->atomistic_or_monomer_stack->setCurrentWidget(ui->monomer_page);
        }
    } else {
        // TODO: disable atomistic_btn and monomer_btn if there's already a
        // molecule of the other type
    }
}

void SketcherSideBar::onAtomisticOrMonomerButtonClicked(QAbstractButton* button)
{
    const std::unordered_set<DrawTool> atomistic_tools = {
        DrawTool::ATOM, DrawTool::BOND,        DrawTool::CHARGE,
        DrawTool::RING, DrawTool::ENUMERATION, DrawTool::EXPLICIT_H,
    };
    QWidget* page;
    std::optional<DrawTool> new_draw_tool = std::nullopt;
    auto model = getModel();
    if (button == ui->atomistic_btn) {
        page = ui->atomistic_page;
        auto cur_draw_tool = model->getDrawTool();
        if (!atomistic_tools.contains(cur_draw_tool)) {
            new_draw_tool = DrawTool::ATOM;
        }
    } else {
        page = ui->monomer_page;
        new_draw_tool = DrawTool::MONOMER;
    }
    ui->atomistic_or_monomer_stack->setCurrentWidget(page);
    if (new_draw_tool.has_value() && !(model->hasActiveSelection())) {
        model->setValue(ModelKey::DRAW_TOOL, *new_draw_tool);
    }
}

} // namespace sketcher
} // namespace schrodinger

#include "schrodinger/sketcher/widget/sketcher_side_bar.moc"
