#define BOOST_TEST_MODULE test_draw_monomer_scene_tool_integration_tests

#include <memory>
#include <string>
#include <vector>

#include <QApplication>
#include <QEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPointF>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>

#include "../test_common.h"
#include "schrodinger/rdkit_extensions/convert.h"
#include "schrodinger/sketcher/model/sketcher_model.h"
#include "schrodinger/sketcher/molviewer/abstract_monomer_item.h"
#include "schrodinger/sketcher/molviewer/coord_utils.h"
#include "schrodinger/sketcher/molviewer/scene_utils.h"
#include "schrodinger/sketcher/molviewer/unbound_monomeric_attachment_point_item.h"
#include "schrodinger/sketcher/public_constants.h"
#include "schrodinger/sketcher/rdkit/monomeric.h"

#include <QtDebug>

namespace bdata = boost::unit_test::data;

BOOST_GLOBAL_FIXTURE(QApplicationRequiredFixture);

namespace schrodinger
{
namespace sketcher
{

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Process all Qt events, includeing DeferredDelete events.
 */
void process_qt_events()
{
    // call processEvents multiple times in case an any current events put new
    // events on the queue (e.g. starting a timer with a timeout of 0)
    for (int i = 0; i < 3; ++i) {
        QApplication::processEvents();
        // processEvents will never process DeferredDelete events, but
        // sendPostedEvents will if we explicitly pass their event type.
        // (Despite what Qt's documentation claims, passing 0 as the event type
        // processes everything *other than* DeferredDeletes.)
        QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
}

void set_event_pos(QGraphicsSceneMouseEvent& event, const QPointF& pos)
{
    event.setScenePos(pos);
    event.setScreenPos(pos.toPoint());
}

// ============================================================================
// Test Fixture
// ============================================================================

/**
 * Fixture that provides a clean Scene with MolModel and SketcherModel for
 * testing monomer drawing tools.
 */
struct MonomerToolTestFixture {
    std::shared_ptr<TestScene> scene;
    MolModel* mol_model;
    SketcherModel* sketcher_model;

    MonomerToolTestFixture()
    {
        // Create a fresh scene for each test to avoid state leakage
        auto undo_stack = new QUndoStack();
        mol_model = new MolModel(undo_stack);
        sketcher_model = new SketcherModel();
        // TODO: use static function?
        scene = std::make_shared<TestScene>(mol_model, sketcher_model);
        undo_stack->setParent(mol_model);
        mol_model->setParent(scene.get());
        sketcher_model->setParent(scene.get());

        // Set interface type to support monomeric structures
        sketcher_model->setValue(ModelKey::INTERFACE_TYPE,
                                static_cast<int>(InterfaceType::ATOMISTIC_OR_MONOMERIC));
        process_qt_events();
        setAminoAcidTool(AminoAcidTool::ALA); // Default tool
    }

    void setAminoAcidTool(AminoAcidTool tool)
    {
        sketcher_model->setValues(
            {{ModelKey::DRAW_TOOL, QVariant::fromValue(DrawTool::MONOMER)},
             {ModelKey::MONOMER_TOOL_TYPE,
              QVariant::fromValue(MonomerToolType::AMINO_ACID)},
             {ModelKey::AMINO_ACID_TOOL, QVariant::fromValue(tool)},
             {ModelKey::AMINO_ACID_SYMBOL, QString("")}});
        process_qt_events();
    }

    void setNucleicAcidTool(NucleicAcidTool tool)
    {
        sketcher_model->setValues(
            {{ModelKey::DRAW_TOOL, QVariant::fromValue(DrawTool::MONOMER)},
             {ModelKey::MONOMER_TOOL_TYPE,
              QVariant::fromValue(MonomerToolType::NUCLEIC_ACID)},
             {ModelKey::NUCLEIC_ACID_TOOL, QVariant::fromValue(tool)}});
        process_qt_events();
    }

    void importMolText(const std::string& text)
    {
        import_mol_text(mol_model, text);
        process_qt_events();
    }

    void clearModel()
    {
        mol_model->clear();
        process_qt_events();
    }
    
    QPointF getMonomerPos(unsigned int monomer_idx)
    {
        auto mol = mol_model->getMol();
        BOOST_REQUIRE(mol);
        BOOST_REQUIRE(monomer_idx < mol->getNumAtoms());
        return to_scene_xy(mol->getConformer().getAtomPos(monomer_idx));
    }

    QPointF getAttachmentPointPos(unsigned int monomer_idx,
                              const std::string& ap_display_name)
    {
        auto mol = mol_model->getMol();
        auto monomer_pos = getMonomerPos(monomer_idx);
        auto* monomer_item = scene->getTopInteractiveItemAt(monomer_pos,
                                       InteractiveItemFlag::MONOMER);
        BOOST_REQUIRE(monomer_item != nullptr);

        // Search through child items to find the attachment point
        for (auto* child : monomer_item->childItems()) {
            auto* ap_item =
                    qgraphicsitem_cast<UnboundMonomericAttachmentPointItem*>(
                        child);
            if (ap_item) {
                auto ap = ap_item->getAttachmentPoint();
                if (ap.display_name == ap_display_name) {
                    auto offset_vec = direction_to_qt_vector(ap.direction);
                    auto offset_dist = UNBOUND_AP_LINE_LENGTH - 1 + STANDARD_AA_BORDER_WIDTH / 2;
                    qDebug() << "monomer_pos = " << monomer_pos;
                    qDebug() << "ap_pos = " << (monomer_pos + offset_dist * offset_vec) << " -- " << ap_item->withinHoverArea(monomer_pos + offset_dist * offset_vec);
                    return monomer_pos + offset_dist * offset_vec;
                }
            }
        }

        throw std::runtime_error("Attachment point " + ap_display_name + " not found");
    }
    
    void verifyHELM(const std::string& expected)
    {
        auto actual = get_mol_text(mol_model, rdkit_extensions::Format::HELM);
        BOOST_TEST(actual == expected);
    }
    
    void mouseMove(const QPointF& pos, const Qt::MouseButtons btns = Qt::NoButton)
    {
        QGraphicsSceneMouseEvent event(QEvent::GraphicsSceneMouseMove);
        set_event_pos(event, pos);
        event.setButton(Qt::NoButton);
        event.setButtons(btns);
        scene->mouseMoveEvent(&event);
        process_qt_events();
    }

    void mousePress(const QPointF& pos)
    {
        QGraphicsSceneMouseEvent event(QEvent::GraphicsSceneMousePress);
        set_event_pos(event, pos);
        event.setButton(Qt::LeftButton);
        event.setButtons(Qt::LeftButton);
        scene->mousePressEvent(&event);
        process_qt_events();
    }

    void mouseRelease(const QPointF& pos)
    {
        QGraphicsSceneMouseEvent event(QEvent::GraphicsSceneMouseRelease);
        set_event_pos(event, pos);
        event.setButton(Qt::LeftButton);
        event.setButtons(Qt::NoButton);
        scene->mouseReleaseEvent(&event);
        process_qt_events();
    }
    
    void mouseClick(const QPointF& pos)
    {
        mouseMove(pos);
        mousePress(pos);
        mouseRelease(pos);
    }
    
    void mouseDrag(const QPointF& start, const QPointF& end)
    {
        mouseMove(start);
        mousePress(start);
        mouseMove(end, Qt::LeftButton);
        mouseRelease(end);
    }
};

/**
 * Confirm that clicking in empty space adds the appropriate monomer
 */
BOOST_AUTO_TEST_CASE(test_click_empty_space_adds_monomer)
{
    MonomerToolTestFixture fix;
    fix.setAminoAcidTool(AminoAcidTool::ALA);
    fix.mouseClick({0, 0});
    fix.verifyHELM("PEPTIDE1{A}$$$$V2.0");
}

/**
 * Confirm that clicking on an existing monomer with the equivalent monomer tool
 * adds a new monomer via the default attachment point.
 */
BOOST_AUTO_TEST_CASE(test_click_existing_monomer_same_residue_adds_residue)
{
    MonomerToolTestFixture fix;
    fix.importMolText("PEPTIDE1{A}$$$$V2.0");
    auto pos = fix.getMonomerPos(0);
    fix.setAminoAcidTool(AminoAcidTool::ALA);
    fix.mouseClick(pos);
    fix.verifyHELM("PEPTIDE1{A.A}$$$$V2.0");
}

/**
 * Confirm that clicking on an existing monomer with a different monomer tool of
 * the same monomer type mutates the monomer.
 */
BOOST_AUTO_TEST_CASE(test_click_existing_monomer_different_residue_mutates)
{
    MonomerToolTestFixture fix;

    fix.importMolText("PEPTIDE1{A}$$$$V2.0");
    auto pos = fix.getMonomerPos(0);
    fix.setAminoAcidTool(AminoAcidTool::CYS);
    fix.mouseClick(pos);
    fix.verifyHELM("PEPTIDE1{C}$$$$V2.0");
}

/**
 * Confirm that clicking on an existing monomer with a monomer tool of
 * a different monomer type has no effect.
 */
BOOST_AUTO_TEST_CASE(test_click_existing_monomer_different_monomer_type)
{
    MonomerToolTestFixture fix;
    fix.importMolText("PEPTIDE1{A}$$$$V2.0");
    auto pos = fix.getMonomerPos(0);
    fix.setNucleicAcidTool(NucleicAcidTool::P);
    fix.mouseClick(pos);
    fix.verifyHELM("PEPTIDE1{A}$$$$V2.0");
}

BOOST_AUTO_TEST_CASE(test_click_attachment_point)
{
    MonomerToolTestFixture fix;
    fix.importMolText( "PEPTIDE1{A}$$$$V2.0");
    auto monomer_pos = fix.getMonomerPos(0);
    fix.setAminoAcidTool(AminoAcidTool::CYS);
    // hover over the monomer to trigger AP label creation
    fix.mouseMove(monomer_pos);

    // click on the N terminus attachment point
    fix.setAminoAcidTool(AminoAcidTool::CYS);
    fix.mouseMove(monomer_pos);
    auto n_ap_pos = fix.getAttachmentPointPos(0, "N");
    fix.mouseClick(n_ap_pos);
    fix.verifyHELM("PEPTIDE1{C.A}$$$$V2.0");

    // click on the C terminus attachment point
    fix.setAminoAcidTool(AminoAcidTool::PHE);
    fix.mouseMove(monomer_pos);
    auto c_ap_pos = fix.getAttachmentPointPos(0, "C");
    fix.mouseClick(c_ap_pos);
    fix.verifyHELM("PEPTIDE1{C.A.F}$$$$V2.0");

    // click on the side chain attachment point
    fix.setAminoAcidTool(AminoAcidTool::TRP);
    fix.mouseMove(monomer_pos);
    auto x_ap_pos = fix.getAttachmentPointPos(0, "X");
    fix.mouseClick(x_ap_pos);
    fix.verifyHELM("PEPTIDE1{C.A.F}|PEPTIDE2{W}$PEPTIDE1,PEPTIDE2,2:R3-1:R3$$$V2.0");
}

// ============================================================================
// Drag Tests
// ============================================================================

/**
 * Confirm that click-and-drag from an existing monomer adds a new monomer using
 * the default attachment point
 */
BOOST_AUTO_TEST_CASE(test_drag_monomer_to_empty_adds_connected_default_ap)
{
    MonomerToolTestFixture fix;
    fix.importMolText("PEPTIDE1{A}$$$$V2.0");
    fix.setAminoAcidTool(AminoAcidTool::CYS);

    // Drag from the monomer to the right
    auto start_pos = fix.getMonomerPos(0);
    auto end_pos = start_pos + QPointF(100, 0);
    fix.mouseDrag(start_pos, end_pos);

    fix.verifyHELM("PEPTIDE1{A.C}$$$$V2.0");
}

/**
 * Confirm that click-and-drag from an existing monomer adds a new monomer using
 * the default attachment point, even when the monomer tool is equivalent to the
 * existing monomer (e.g. ALA tool on an A monomer).
 */
BOOST_AUTO_TEST_CASE(test_drag_monomer_to_empty_adds_connected_default_ap_same_monomer)
{
    MonomerToolTestFixture fix;
    fix.importMolText("PEPTIDE1{A}$$$$V2.0");
    fix.setAminoAcidTool(AminoAcidTool::ALA);

    // Drag from the monomer to the right
    auto start_pos = fix.getMonomerPos(0);
    auto end_pos = start_pos + QPointF(100, 0);
    fix.mouseDrag(start_pos, end_pos);

    fix.verifyHELM("PEPTIDE1{A.A}$$$$V2.0");
}

/**
 * Confirm that click-and-drag from the attachment point of an existing monomer
 * to empty space adds a new monomer via the specified attachment point of the
 * existing monomer
 */
BOOST_AUTO_TEST_CASE(test_drag_ap_to_empty_adds_connected_via_dragged_ap)
{
    MonomerToolTestFixture fix;
    fix.setAminoAcidTool(AminoAcidTool::CYS);

    // Add initial monomer
    fix.importMolText("PEPTIDE1{A}$$$$V2.0");
    
    // hover over the monomer so that the attachment point graphics items are
    // created
    auto ala_pos = fix.getMonomerPos(0);
    fix.mouseMove(ala_pos);

    // Drag from N attachment point to empty space
    auto start_pos = fix.getAttachmentPointPos(0, "N");
    auto end_pos = start_pos + QPointF(-100, 0);
    fix.mouseDrag(start_pos, end_pos);

    fix.verifyHELM("PEPTIDE1{C.A}$$$$V2.0");

    // hover over the first monomer so that its attachment point graphics items
    // are created again
    fix.setAminoAcidTool(AminoAcidTool::PHE);
    fix.mouseMove(ala_pos);

    // Drag from C attachment point to empty space
    start_pos = fix.getAttachmentPointPos(0, "C");
    end_pos = start_pos + QPointF(100, 100);
    fix.mouseDrag(start_pos, end_pos);

    fix.verifyHELM("PEPTIDE1{C.A.F}$$$$V2.0");

    // hover over the first monomer so that its attachment point graphics items
    // are created again
    fix.setAminoAcidTool(AminoAcidTool::ALA);
    fix.mouseMove(ala_pos);

    // Drag from C attachment point to empty space
    start_pos = fix.getAttachmentPointPos(0, "X");
    end_pos = start_pos + QPointF(-50, 100);
    fix.mouseDrag(start_pos, end_pos);

    fix.verifyHELM("PEPTIDE1{C.A.F}|PEPTIDE2{A}$PEPTIDE1,PEPTIDE2,2:R3-1:R3$$$V2.0");
}

/**
 * Confirm that click-and-drag from an existing monomer to an existing monomer
 * connects them via the default attachment points
 */
BOOST_AUTO_TEST_CASE(test_drag_monomer_to_monomer_connects_default_aps)
{
    MonomerToolTestFixture fix;
    fix.setAminoAcidTool(AminoAcidTool::ALA);
    import_mol_text(fix.mol_model, "PEPTIDE1{A}|PEPTIDE2{C}$$$$V2.0");
    auto pos1 = fix.getMonomerPos(0);
    auto pos2 = fix.getMonomerPos(1);
    fix.mouseDrag(pos1, pos2);
    fix.verifyHELM("PEPTIDE1{A.C}$$$$V2.0");
}

/**
 * Confirm that click-and-drag from the attachment point of one existing monomer
 * to the attachment point of another existing monomer connects the monomer via
 * the specified attachment points.
 */
BOOST_AUTO_TEST_CASE(test_drag_ap_to_ap_connects_via_both_aps)
{
    MonomerToolTestFixture fix;
    fix.setAminoAcidTool(AminoAcidTool::ALA);
    import_mol_text(fix.mol_model, "PEPTIDE1{A}|PEPTIDE2{C}$$$$V2.0");
    auto ala_pos = fix.getMonomerPos(0);
    auto cys_pos = fix.getMonomerPos(1);
    fix.mouseMove(ala_pos);
    auto start_pos = fix.getAttachmentPointPos(0, "N");
    fix.mouseMove(start_pos);
    fix.mousePress(start_pos);
    // fist, drag to the cysteine to make its attachment points appear
    fix.mouseMove(cys_pos, Qt::LeftButton);
    auto end_pos = fix.getAttachmentPointPos(1, "N");
    fix.mouseMove(end_pos, Qt::LeftButton);
    fix.mouseRelease(end_pos);
    fix.verifyHELM("PEPTIDE1{A}|PEPTIDE2{C}$PEPTIDE1,PEPTIDE2,1:R1-1:R1$$$V2.0");
}

BOOST_AUTO_TEST_CASE(test_drag_empty_to_empty_adds_two_connected_default_aps)
{
    MonomerToolTestFixture fix;
    fix.setAminoAcidTool(AminoAcidTool::ALA);

    auto start_pos = QPointF(100, 100);
    auto end_pos = start_pos + QPointF(100, 0);

    // Drag from empty space to empty space
    fix.mouseDrag(start_pos, end_pos);

    // Should create two connected monomers via default APs
    fix.verifyHELM("PEPTIDE1{A.A}$$$$V2.0");
}

// BOOST_AUTO_TEST_CASE(test_drag_empty_to_monomer_adds_connected_default_aps)
// {
//     MonomerToolTestFixture fix;
//     fix.setAminoAcidTool(AminoAcidTool::ALA);

//     // Add existing monomer
//     import_mol_text(fix.mol_model, "PEPTIDE1{A}$$$$V2.0");
//     auto existing_pos = getMonomerPos(fix.mol_model, 0);

//     auto start_pos = QPointF(100, 100);

//     // Drag from empty space to existing monomer
//     mouseDrag(fix.scene.get(), start_pos, existing_pos);

//     // Should add a new monomer connected via default APs
//     verifyHELM(fix.mol_model, "PEPTIDE1{A.A}$$$$V2.0");
// }

// BOOST_AUTO_TEST_CASE(test_drag_empty_to_ap_adds_connected_correct_aps)
// {
//     MonomerToolTestFixture fix;
//     fix.setAminoAcidTool(AminoAcidTool::ALA);

//     // Add existing monomer
//     import_mol_text(fix.mol_model, "PEPTIDE1{A}$$$$V2.0");

//     auto start_pos = QPointF(100, 100);
//     auto end_pos =
//         getAttachmentPointPos(fix.scene.get(), fix.mol_model, 0, "R1");

//     // Drag from empty space to R1 attachment point
//     mouseDrag(fix.scene.get(), start_pos, end_pos);

//     // Should add a new monomer connected to R1
//     // New monomer uses R2 (default outgoing AP) to connect to existing R1
//     verifyHELM(fix.mol_model,
//                "PEPTIDE1{A.A}$PEPTIDE1,PEPTIDE1,2:R2-1:R1$$$V2.0");
// }

// // ============================================================================
// // Nucleic Acid Special Cases
// // ============================================================================

BOOST_AUTO_TEST_CASE(
    test_nucleic_acid_base_drag_empty_to_empty_uses_pair_ap)
{
    MonomerToolTestFixture fix;
    fix.setNucleicAcidTool(NucleicAcidTool::A);

    auto start_pos = QPointF(100, 100);
    auto end_pos = start_pos + QPointF(100, 0);

    // Drag from empty space to empty space with nucleic acid base tool
    fix.mouseDrag(start_pos, end_pos);

    // Should create two bases connected via "pair" attachment point
    fix.verifyHELM("RNA1{A}|RNA2{A}$RNA1,RNA2,1:pair-1:pair$$$V2.0");
}

BOOST_AUTO_TEST_CASE(test_nucleic_acid_sugar_drag_empty_to_empty_ignored)
{
    MonomerToolTestFixture fix;
    fix.setNucleicAcidTool(NucleicAcidTool::R);

    auto start_pos = QPointF(100, 100);
    auto end_pos = start_pos + QPointF(100, 0);

    // Drag from empty space to empty space with nucleic acid sugar tool
    fix.mouseDrag(start_pos, end_pos);

    // Drag should be ignored - no monomers added
    auto mol = fix.mol_model->getMol();
    BOOST_TEST(mol->getNumAtoms() == 0);
}

} // namespace sketcher
} // namespace schrodinger
