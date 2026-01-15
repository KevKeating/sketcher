
#define BOOST_TEST_MODULE monomeric

#include <unordered_set>

#include <rdkit/GraphMol/RWMol.h>
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include "schrodinger/rdkit_extensions/convert.h"
#include "schrodinger/sketcher/rdkit/monomeric.h"

using namespace boost::unit_test;

namespace schrodinger
{
namespace sketcher
{

/**
 * Make sure that contains_two_monomer_linkages correctly detects two monomer
 * linkages in the same bond when there's a disulfide bond between neighboring
 * cysteines.
 */
BOOST_AUTO_TEST_CASE(test_contains_two_monomer_linkages)
{
    // two neighboring cysteines, but no disulfide
    auto mol = rdkit_extensions::to_rdkit("PEPTIDE1{C.C}$$$$V2.0");
    BOOST_TEST(mol->getNumBonds() == 1);
    BOOST_TEST(!contains_two_monomer_linkages(mol->getBondWithIdx(0)));

    // two neighboring cysteines with a disulfide
    mol = rdkit_extensions::to_rdkit(
        "PEPTIDE1{C.C}$PEPTIDE1,PEPTIDE1,1:R3-2:R3$$$V2.0");
    BOOST_TEST(mol->getNumBonds() == 1);
    BOOST_TEST(contains_two_monomer_linkages(mol->getBondWithIdx(0)));

    // a disulfide, but between two non-neighboring cysteines
    mol = rdkit_extensions::to_rdkit(
        "PEPTIDE1{C.A.C}$PEPTIDE1,PEPTIDE1,1:R3-3:R3$$$V2.0");
    BOOST_TEST(mol->getNumBonds() == 3);
    BOOST_TEST(!contains_two_monomer_linkages(mol->getBondWithIdx(0)));
    BOOST_TEST(!contains_two_monomer_linkages(mol->getBondWithIdx(1)));
    BOOST_TEST(!contains_two_monomer_linkages(mol->getBondWithIdx(2)));
}

BOOST_AUTO_TEST_CASE(test_get_attachment_points)
{
    // a lone alanine has no bound attachment points
    auto mol = rdkit_extensions::to_rdkit("PEPTIDE1{A}$$$$V2.0");
    auto atom = mol->getAtomWithIdx(0);
    BOOST_TEST(get_bound_attachment_points(atom).empty());
    std::unordered_set<int> exp_available = {1, 2, 3};
    BOOST_TEST(get_available_attachment_points(atom) == exp_available);

    // two alanines next to each other
    mol = rdkit_extensions::to_rdkit("PEPTIDE1{A.A}$$$$V2.0");
    // the first alanine has a bound C-terminus
    atom = mol->getAtomWithIdx(0);
    std::unordered_set<int> exp_bound = {2};
    BOOST_TEST(get_bound_attachment_points(atom) == exp_bound);
    exp_available = {1, 3};
    BOOST_TEST(get_available_attachment_points(atom) == exp_available);

    // the second alanine has a bound N-terminus
    atom = mol->getAtomWithIdx(1);
    exp_bound = {1};
    BOOST_TEST(get_bound_attachment_points(atom) == exp_bound);
    exp_available = {2, 3};
    BOOST_TEST(get_available_attachment_points(atom) == exp_available);

    // a side-chain interaction between two non-adjacent residues
    mol = rdkit_extensions::to_rdkit(
        "PEPTIDE1{C.A.C}$PEPTIDE1,PEPTIDE1,1:R3-3:R3$$$V2.0");
    atom = mol->getAtomWithIdx(0);
    exp_bound = {2, 3};
    BOOST_TEST(get_bound_attachment_points(atom) == exp_bound);
    exp_available = {1};
    BOOST_TEST(get_available_attachment_points(atom) == exp_available);

    atom = mol->getAtomWithIdx(1);
    exp_bound = {1, 2};
    BOOST_TEST(get_bound_attachment_points(atom) == exp_bound);
    exp_available = {3};
    BOOST_TEST(get_available_attachment_points(atom) == exp_available);

    atom = mol->getAtomWithIdx(2);
    exp_bound = {1, 3};
    BOOST_TEST(get_bound_attachment_points(atom) == exp_bound);
    exp_available = {2};
    BOOST_TEST(get_available_attachment_points(atom) == exp_available);

    // a side-chain interaction between two adjacent residues
    mol = rdkit_extensions::to_rdkit(
        "PEPTIDE1{C.C}$PEPTIDE1,PEPTIDE1,1:R3-2:R3$$$V2.0");
    atom = mol->getAtomWithIdx(0);
    exp_bound = {2, 3};
    BOOST_TEST(get_bound_attachment_points(atom) == exp_bound);
    exp_available = {1};
    BOOST_TEST(get_available_attachment_points(atom) == exp_available);

    atom = mol->getAtomWithIdx(1);
    exp_bound = {1, 3};
    BOOST_TEST(get_bound_attachment_points(atom) == exp_bound);
    exp_available = {2};
    BOOST_TEST(get_available_attachment_points(atom) == exp_available);
}

} // namespace sketcher
} // namespace schrodinger
