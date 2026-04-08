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
 * Process Qt events to ensure all deferred operations complete.
 * Critical for proper test execution - without this, events may not be
 * processed and tests will fail intermittently.
 */
void processQtEvents()
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

// TODO: all of these functions could be methods on the fixture

/**
 * Simulate a mouse click at the given position.
 */
void simulateClick(TestScene* scene, const QPointF& pos)
{
    QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
    QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);

    for (auto* event : {&press, &release}) {
        event->setScenePos(pos);
        event->setButton(Qt::LeftButton);
        event->setButtons(Qt::LeftButton);
    }

    scene->mousePressEvent(&press);
    processQtEvents();
    scene->mouseReleaseEvent(&release);
    processQtEvents();
}

/**
 * Simulate a mouse drag from start to end position.
 */
void simulateDrag(TestScene* scene, const QPointF& start, const QPointF& end)
{
    QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
    QGraphicsSceneMouseEvent move(QEvent::GraphicsSceneMouseMove);
    QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);

    press.setScenePos(start);
    press.setButton(Qt::LeftButton);
    press.setButtons(Qt::LeftButton);

    move.setScenePos(end);
    move.setButton(Qt::LeftButton);
    move.setButtons(Qt::LeftButton);

    release.setScenePos(end);
    release.setButton(Qt::LeftButton);
    release.setButtons(Qt::NoButton);

    scene->mousePressEvent(&press);
    processQtEvents();
    scene->mouseMoveEvent(&move);
    processQtEvents();
    scene->mouseReleaseEvent(&release);
    processQtEvents();
}

/**
 * Get the scene position of a monomer at the given atom index.
 */
QPointF getMonomerPos(MolModel* model, unsigned int atom_idx)
{
    auto mol = model->getMol();
    // TODO: convert this to a BOOST_REQUIRE
    if (!mol || atom_idx >= mol->getNumAtoms()) {
        throw std::runtime_error("Invalid atom index");
    }
    return to_scene_xy(mol->getConformer().getAtomPos(atom_idx));
}

/**
 * Return an arbitrary position in empty space.
 */
QPointF emptySpacePos()
{
    return QPointF(100, 100);
}

/**
 * Verify that the MolModel contains the expected HELM string.
 * Note: This is flexible about chain IDs since they may vary due to global state.
 */
void verifyHELM(MolModel* model, const std::string& expected)
{
    auto actual = get_mol_text(model, rdkit_extensions::Format::HELM);
    BOOST_TEST(actual == expected);
}

/**
 * Get the position of an unbound attachment point on a monomer.
 * This searches for an attachment point item with the given name.
 */
QPointF getAttachmentPointPos(TestScene* scene, MolModel* model,
                              unsigned int monomer_idx,
                              const std::string& ap_name)
{
    auto mol = model->getMol();
    auto* monomer = mol->getAtomWithIdx(monomer_idx);

    // TODO: this method is completely wrong, getTopInteractiveItemAt might
    // return a hint structure item, and attachment point graphics items don't
    // have a pos set

    // Find the monomer graphics item
    auto* monomer_item = scene->getTopInteractiveItemAt(getMonomerPos(model, monomer_idx),
                                       InteractiveItemFlag::MONOMER);
    // BOOST_REQUIRE(item != nullptr);
    // auto* monomer_item = dynamic_cast<AbstractMonomerItem*>(item);
    BOOST_REQUIRE(monomer_item != nullptr);

    // Search through child items to find the attachment point
    for (auto* child : monomer_item->childItems()) {
        if (auto* ap_item =
                qgraphicsitem_cast<UnboundMonomericAttachmentPointItem*>(
                    child)) {
            if (ap_item->getAttachmentPoint().model_name == ap_name) {
                return ap_item->scenePos();
            }
        }
    }

    throw std::runtime_error("Attachment point " + ap_name + " not found");
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
        scene = std::make_shared<TestScene>(mol_model, sketcher_model);
        undo_stack->setParent(mol_model);
        mol_model->setParent(scene.get());
        sketcher_model->setParent(scene.get());

        // Set interface type to support monomeric structures
        sketcher_model->setValue(ModelKey::INTERFACE_TYPE,
                                static_cast<int>(InterfaceType::ATOMISTIC_OR_MONOMERIC));
        processQtEvents();
        setAminoAcidTool(AminoAcidTool::ALA); // Default tool
    }

    void setAminoAcidTool(AminoAcidTool tool)
    {
        sketcher_model->setValues(
            {{ModelKey::DRAW_TOOL, QVariant::fromValue(DrawTool::MONOMER)},
             {ModelKey::MONOMER_TOOL_TYPE,
              QVariant::fromValue(MonomerToolType::AMINO_ACID)},
             {ModelKey::AMINO_ACID_TOOL, QVariant::fromValue(tool)}});
        processQtEvents();
    }

    void setNucleicAcidTool(NucleicAcidTool tool)
    {
        sketcher_model->setValues(
            {{ModelKey::DRAW_TOOL, QVariant::fromValue(DrawTool::MONOMER)},
             {ModelKey::MONOMER_TOOL_TYPE,
              QVariant::fromValue(MonomerToolType::NUCLEIC_ACID)},
             {ModelKey::NUCLEIC_ACID_TOOL, QVariant::fromValue(tool)}});
        processQtEvents();
    }

    void clearModel()
    {
        mol_model->clear();
        processQtEvents();
    }
    
    QPointF getMonomerPos(unsigned int atom_idx)
    {
        auto mol = mol_model->getMol();
        BOOST_REQUIRE(mol);
        BOOST_REQUIRE(atom_idx < mol->getNumAtoms());
        return to_scene_xy(mol->getConformer().getAtomPos(atom_idx));
    }
    
    void verifyHELM(const std::string& expected)
    {
        auto actual = get_mol_text(mol_model, rdkit_extensions::Format::HELM);
        BOOST_TEST(actual == expected);
    }
};

// ============================================================================
// Click Tests
// ============================================================================

BOOST_AUTO_TEST_CASE(test_click_empty_space_adds_monomer)
{
    MonomerToolTestFixture fix;
    fix.setAminoAcidTool(AminoAcidTool::ALA);

    simulateClick(fix.scene.get(), emptySpacePos());

    fix.verifyHELM("PEPTIDE1{A}$$$$V2.0");
}

BOOST_AUTO_TEST_CASE(test_click_existing_monomer_same_residue_no_change)
{
    MonomerToolTestFixture fix;
    fix.setAminoAcidTool(AminoAcidTool::ALA);

    // Add initial monomer
    import_mol_text(fix.mol_model, "PEPTIDE1{A}$$$$V2.0");
    auto pos = fix.getMonomerPos(0);

    // Click on it with same tool
    simulateClick(fix.scene.get(), pos);

    // Should remain unchanged
    verifyHELM(fix.mol_model, "PEPTIDE1{A}$$$$V2.0");
}

BOOST_AUTO_TEST_CASE(test_click_existing_monomer_different_residue_mutates)
{
    MonomerToolTestFixture fix;

    // Add alanine
    fix.setAminoAcidTool(AminoAcidTool::ALA);
    import_mol_text(fix.mol_model, "PEPTIDE1{A}$$$$V2.0");
    auto pos = fix.getMonomerPos(0);

    // Click with cysteine tool
    fix.setAminoAcidTool(AminoAcidTool::CYS);
    simulateClick(fix.scene.get(), pos);

    // Should mutate to cysteine
    fix.verifyHELM("PEPTIDE1{C}$$$$V2.0");
}

BOOST_AUTO_TEST_CASE(test_click_existing_monomer_same_residue_does_nothing)
{
    MonomerToolTestFixture fix;
    fix.setAminoAcidTool(AminoAcidTool::ALA);

    // Add initial monomer
    import_mol_text(fix.mol_model, "PEPTIDE1{A}$$$$V2.0");
    auto pos = getMonomerPos(fix.mol_model, 0);

    // Click on it with the same tool
    // (Should do nothing - not mutate, not add)
    simulateClick(fix.scene.get(), pos);

    // Should remain unchanged
    verifyHELM(fix.mol_model, "PEPTIDE1{A}$$$$V2.0");
}

BOOST_AUTO_TEST_CASE(test_click_attachment_point_adds_connected_via_clicked_ap)
{
    MonomerToolTestFixture fix;
    std::cout << "instantiated test fixture\n";
    fix.setAminoAcidTool(AminoAcidTool::ALA);
    std::cout << "set tool\n";

    // Add initial monomer
    import_mol_text(fix.mol_model, "PEPTIDE1{A}$$$$V2.0");
    std::cout << "imported HELM string\n";
    // Process events to ensure scene is fully updated with graphics items
    processQtEvents();
    std::cout << "processed events\n";

    // TODO: this should be a function like click and drag
    // First hover over the monomer to trigger AP label creation
    auto monomer_pos = getMonomerPos(fix.mol_model, 0);
    QGraphicsSceneMouseEvent hover(QEvent::GraphicsSceneMouseMove);
    hover.setScenePos(monomer_pos);
    hover.setButton(Qt::NoButton);
    hover.setButtons(Qt::NoButton);
    std::cout << "instantiated mouse move\n";
    fix.scene->mouseMoveEvent(&hover);
    std::cout << "sent mouse move\n";
    processQtEvents();
    std::cout << "processed events again\n";

    // Click on the R1 attachment point
    auto ap_pos = getAttachmentPointPos(fix.scene.get(), fix.mol_model, 0, "R1");
    std::cout << "got ap_pos";
    simulateClick(fix.scene.get(), ap_pos);
    std::cout << "simulated click\n";

    // Should add a second alanine connected via R1-R2
    // The connection annotation in HELM would be: 2:R2-1:R1
    verifyHELM(fix.mol_model,
               "PEPTIDE1{A.A}$PEPTIDE1,PEPTIDE1,2:R2-1:R1$$$V2.0");
}

// ============================================================================
// Drag Tests
// ============================================================================

BOOST_AUTO_TEST_CASE(test_drag_monomer_to_empty_adds_connected_default_ap)
{
    MonomerToolTestFixture fix;
    fix.setAminoAcidTool(AminoAcidTool::ALA);

    // Add initial monomer
    import_mol_text(fix.mol_model, "PEPTIDE1{A}$$$$V2.0");
    auto start_pos = getMonomerPos(fix.mol_model, 0);
    auto end_pos = start_pos + QPointF(100, 0); // Drag to the right

    // Drag from monomer to empty space
    simulateDrag(fix.scene.get(), start_pos, end_pos);

    // Should add a second monomer connected via default APs
    verifyHELM(fix.mol_model, "PEPTIDE1{A.A}$$$$V2.0");
}

BOOST_AUTO_TEST_CASE(test_drag_ap_to_empty_adds_connected_via_dragged_ap)
{
    MonomerToolTestFixture fix;
    fix.setAminoAcidTool(AminoAcidTool::ALA);

    // Add initial monomer
    import_mol_text(fix.mol_model, "PEPTIDE1{A}$$$$V2.0");
    auto start_pos =
        getAttachmentPointPos(fix.scene.get(), fix.mol_model, 0, "R1");
    auto end_pos = start_pos + QPointF(100, 0);

    // Drag from R1 attachment point to empty space
    simulateDrag(fix.scene.get(), start_pos, end_pos);

    // Should add a second monomer connected via R1-R2
    verifyHELM(fix.mol_model,
               "PEPTIDE1{A.A}$PEPTIDE1,PEPTIDE1,2:R2-1:R1$$$V2.0");
}

BOOST_AUTO_TEST_CASE(test_drag_monomer_to_monomer_connects_default_aps)
{
    MonomerToolTestFixture fix;
    fix.setAminoAcidTool(AminoAcidTool::ALA);

    // Add two separate, unconnected monomers by creating two chains
    import_mol_text(fix.mol_model, "PEPTIDE1{A}|PEPTIDE2{A}$$$$V2.0");
    auto pos1 = getMonomerPos(fix.mol_model, 0);
    auto pos2 = getMonomerPos(fix.mol_model, 1);

    // Drag from first monomer to second monomer
    simulateDrag(fix.scene.get(), pos1, pos2);

    // Should connect via default APs (R2 of first to R1 of second)
    verifyHELM(fix.mol_model, "PEPTIDE1{A.A}$$$$V2.0");
}

BOOST_AUTO_TEST_CASE(test_drag_ap_to_ap_connects_via_both_aps)
{
    MonomerToolTestFixture fix;
    fix.setAminoAcidTool(AminoAcidTool::ALA);

    // Add two separate, unconnected monomers by creating two chains
    import_mol_text(fix.mol_model, "PEPTIDE1{A}|PEPTIDE2{A}$$$$V2.0");

    auto start_pos =
        getAttachmentPointPos(fix.scene.get(), fix.mol_model, 0, "R1");
    auto end_pos =
        getAttachmentPointPos(fix.scene.get(), fix.mol_model, 1, "R1");

    // Drag from R1 of first to R1 of second
    simulateDrag(fix.scene.get(), start_pos, end_pos);

    // Should connect via R1-R1
    verifyHELM(fix.mol_model,
               "PEPTIDE1{A.A}$PEPTIDE1,PEPTIDE1,2:R1-1:R1$$$V2.0");
}

BOOST_AUTO_TEST_CASE(test_drag_empty_to_empty_adds_two_connected_default_aps)
{
    MonomerToolTestFixture fix;
    fix.setAminoAcidTool(AminoAcidTool::ALA);

    auto start_pos = emptySpacePos();
    auto end_pos = start_pos + QPointF(100, 0);

    // Drag from empty space to empty space
    simulateDrag(fix.scene.get(), start_pos, end_pos);

    // Should create two connected monomers via default APs
    verifyHELM(fix.mol_model, "PEPTIDE1{A.A}$$$$V2.0");
}

BOOST_AUTO_TEST_CASE(test_drag_empty_to_monomer_adds_connected_default_aps)
{
    MonomerToolTestFixture fix;
    fix.setAminoAcidTool(AminoAcidTool::ALA);

    // Add existing monomer
    import_mol_text(fix.mol_model, "PEPTIDE1{A}$$$$V2.0");
    auto existing_pos = getMonomerPos(fix.mol_model, 0);

    auto start_pos = emptySpacePos();

    // Drag from empty space to existing monomer
    simulateDrag(fix.scene.get(), start_pos, existing_pos);

    // Should add a new monomer connected via default APs
    verifyHELM(fix.mol_model, "PEPTIDE1{A.A}$$$$V2.0");
}

BOOST_AUTO_TEST_CASE(test_drag_empty_to_ap_adds_connected_correct_aps)
{
    MonomerToolTestFixture fix;
    fix.setAminoAcidTool(AminoAcidTool::ALA);

    // Add existing monomer
    import_mol_text(fix.mol_model, "PEPTIDE1{A}$$$$V2.0");

    auto start_pos = emptySpacePos();
    auto end_pos =
        getAttachmentPointPos(fix.scene.get(), fix.mol_model, 0, "R1");

    // Drag from empty space to R1 attachment point
    simulateDrag(fix.scene.get(), start_pos, end_pos);

    // Should add a new monomer connected to R1
    // New monomer uses R2 (default outgoing AP) to connect to existing R1
    verifyHELM(fix.mol_model,
               "PEPTIDE1{A.A}$PEPTIDE1,PEPTIDE1,2:R2-1:R1$$$V2.0");
}

// ============================================================================
// Nucleic Acid Special Cases
// ============================================================================

BOOST_AUTO_TEST_CASE(
    test_nucleic_acid_base_drag_empty_to_empty_uses_pair_ap)
{
    MonomerToolTestFixture fix;
    fix.setNucleicAcidTool(NucleicAcidTool::A);

    auto start_pos = emptySpacePos();
    auto end_pos = start_pos + QPointF(100, 0);

    // Drag from empty space to empty space with nucleic acid base tool
    simulateDrag(fix.scene.get(), start_pos, end_pos);

    // Should create two bases connected via "pair" attachment point
    verifyHELM(fix.mol_model, "RNA1{A.A}$RNA1,RNA1,2:pair-1:pair$$$V2.0");
}

BOOST_AUTO_TEST_CASE(test_nucleic_acid_sugar_drag_empty_to_empty_ignored)
{
    MonomerToolTestFixture fix;
    fix.setNucleicAcidTool(NucleicAcidTool::R);

    auto start_pos = emptySpacePos();
    auto end_pos = start_pos + QPointF(100, 0);

    // Drag from empty space to empty space with nucleic acid sugar tool
    simulateDrag(fix.scene.get(), start_pos, end_pos);

    // Drag should be ignored - no monomers added
    auto mol = fix.mol_model->getMol();
    BOOST_TEST(mol->getNumAtoms() == 0);
}

} // namespace sketcher
} // namespace schrodinger
