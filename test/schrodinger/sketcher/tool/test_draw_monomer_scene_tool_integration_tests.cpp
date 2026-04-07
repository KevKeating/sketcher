#define BOOST_TEST_MODULE test_draw_monomer_scene_tool_integration_tests

#include <memory>
#include <string>
#include <vector>

#include <QApplication>
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
    for (int i = 0; i < 3; ++i) {
        QApplication::processEvents();
        QApplication::sendPostedEvents();
    }
}

/**
 * Simulate a mouse click at the given position.
 */
void simulateClick(Scene* scene, const QPointF& pos)
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
void simulateDrag(Scene* scene, const QPointF& start, const QPointF& end)
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
QPointF getAttachmentPointPos(Scene* scene, MolModel* model,
                              unsigned int monomer_idx,
                              const std::string& ap_name)
{
    auto mol = model->getMol();
    auto* monomer = mol->getAtomWithIdx(monomer_idx);

    // Find the monomer graphics item
    auto* monomer_item = qgraphicsitem_cast<AbstractMonomerItem*>(
        scene->getTopInteractiveItemAt(getMonomerPos(model, monomer_idx),
                                       InteractiveItemFlag::ATOM));

    // Search through child items to find the attachment point
    for (auto* child : monomer_item->childItems()) {
        if (auto* ap_item =
                qgraphicsitem_cast<UnboundMonomericAttachmentPointItem*>(
                    child)) {
            if (ap_item->getModelName() == ap_name) {
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
        scene = TestScene::getScene();
        mol_model = scene->m_mol_model;
        sketcher_model = scene->m_sketcher_model;
        setAminoAcidTool(AminoAcidTool::ALA); // Default tool
    }

    void setAminoAcidTool(AminoAcidTool tool)
    {
        sketcher_model->setValues(
            {{ModelKey::MONOMER_TOOL_TYPE,
              QVariant::fromValue(MonomerToolType::AMINO_ACID)},
             {ModelKey::AMINO_ACID_TOOL, QVariant::fromValue(tool)}});
        processQtEvents();
    }

    void setNucleicAcidTool(NucleicAcidTool tool)
    {
        sketcher_model->setValues(
            {{ModelKey::MONOMER_TOOL_TYPE,
              QVariant::fromValue(MonomerToolType::NUCLEIC_ACID)},
             {ModelKey::NUCLEIC_ACID_TOOL, QVariant::fromValue(tool)}});
        processQtEvents();
    }

    void clearModel()
    {
        mol_model->clear();
        processQtEvents();
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

    verifyHELM(fix.mol_model, "PEPTIDE1{A}$$$$V2.0");
}

BOOST_AUTO_TEST_CASE(test_click_existing_monomer_same_residue_no_change)
{
    MonomerToolTestFixture fix;
    fix.setAminoAcidTool(AminoAcidTool::ALA);

    // Add initial monomer
    import_mol_text(fix.mol_model, "PEPTIDE1{A}$$$$V2.0");
    auto pos = getMonomerPos(fix.mol_model, 0);

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
    auto pos = getMonomerPos(fix.mol_model, 0);

    // Click with cysteine tool
    fix.setAminoAcidTool(AminoAcidTool::CYS);
    simulateClick(fix.scene.get(), pos);

    // Should mutate to cysteine
    verifyHELM(fix.mol_model, "PEPTIDE1{C}$$$$V2.0");
}

BOOST_AUTO_TEST_CASE(test_click_existing_monomer_adds_connected_default_ap)
{
    MonomerToolTestFixture fix;
    fix.setAminoAcidTool(AminoAcidTool::ALA);

    // Add initial monomer
    import_mol_text(fix.mol_model, "PEPTIDE1{A}$$$$V2.0");
    auto pos = getMonomerPos(fix.mol_model, 0);

    // Click on it to add another connected monomer
    // (Hovering over the monomer itself should use default attachment point)
    simulateClick(fix.scene.get(), pos);

    // Should add a second alanine connected via default APs (R1-R2)
    verifyHELM(fix.mol_model, "PEPTIDE1{A.A}$$$$V2.0");
}

BOOST_AUTO_TEST_CASE(test_click_attachment_point_adds_connected_via_clicked_ap)
{
    MonomerToolTestFixture fix;
    fix.setAminoAcidTool(AminoAcidTool::ALA);

    // Add initial monomer
    import_mol_text(fix.mol_model, "PEPTIDE1{A}$$$$V2.0");

    // Get position of R1 attachment point
    auto ap_pos = getAttachmentPointPos(fix.scene.get(), fix.mol_model, 0, "R1");

    // Click on the R1 attachment point
    simulateClick(fix.scene.get(), ap_pos);

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

    // Add two separate monomers
    import_mol_text(fix.mol_model, "PEPTIDE1{A}$$$$V2.0");
    auto pos1 = getMonomerPos(fix.mol_model, 0);

    fix.clearModel();
    import_mol_text(fix.mol_model, "PEPTIDE1{A.A}$$$$V2.0");
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

    // Add two separate monomers
    import_mol_text(fix.mol_model, "PEPTIDE1{A.A}$$$$V2.0");

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
