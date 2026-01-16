
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
    std::unordered_map<std::string, const RDKit::Atom*> exp_bound;
    std::unordered_set<std::string> exp_available;
    const RDKit::Atom *atom0, *atom1, *atom2;
    
    // a lone alanine has no bound attachment points
    auto mol = rdkit_extensions::to_rdkit("PEPTIDE1{A}$$$$V2.0");
    {
        atom0 = mol->getAtomWithIdx(0);
        BOOST_TEST(get_bound_attachment_point_names_and_atoms(atom0).empty());
        exp_available = {"N", "C", "X"};
        BOOST_TEST(get_available_attachment_point_names(atom0) == exp_available);
    }

    // two alanines next to each other
    mol = rdkit_extensions::to_rdkit("PEPTIDE1{A.A}$$$$V2.0");
    {
        const auto* atom0 = mol->getAtomWithIdx(0);
        const auto* atom1 = mol->getAtomWithIdx(1);

        exp_bound = {{"C", atom1}};
        BOOST_TEST(get_bound_attachment_point_names_and_atoms(atom0) == exp_bound);
        exp_available = {"N", "X"};
        BOOST_TEST(get_available_attachment_point_names(atom0) == exp_available);

        exp_bound = {{"N", atom0}};
        BOOST_TEST(get_bound_attachment_point_names_and_atoms(atom1) == exp_bound);
        exp_available = {"C", "X"};
        BOOST_TEST(get_available_attachment_point_names(atom1) == exp_available);
    }

    // a side-chain interaction between two non-adjacent residues
    mol = rdkit_extensions::to_rdkit(
        "PEPTIDE1{C.A.C}$PEPTIDE1,PEPTIDE1,1:R3-3:R3$$$V2.0");
    {
        atom0 = mol->getAtomWithIdx(0);
        atom1 = mol->getAtomWithIdx(1);
        atom2 = mol->getAtomWithIdx(2);
        
        exp_bound = {{"C", atom1}, {"X", atom2}};
        BOOST_TEST(get_bound_attachment_point_names_and_atoms(atom0) == exp_bound);
        exp_available = {"N"};
        BOOST_TEST(get_available_attachment_point_names(atom0) == exp_available);

        exp_bound = {{"N", atom0}, {"C", atom2}};
        BOOST_TEST(get_bound_attachment_point_names_and_atoms(atom1) == exp_bound);
        exp_available = {"X"};
        BOOST_TEST(get_available_attachment_point_names(atom1) == exp_available);

        exp_bound = {{"N", atom1}, {"X", atom0}};
        BOOST_TEST(get_bound_attachment_point_names_and_atoms(atom2) == exp_bound);
        exp_available = {"C"};
        BOOST_TEST(get_available_attachment_point_names(atom2) == exp_available);
    }

    // a side-chain interaction between two adjacent residues
    mol = rdkit_extensions::to_rdkit(
        "PEPTIDE1{C.C}$PEPTIDE1,PEPTIDE1,1:R3-2:R3$$$V2.0");
    {
        atom0 = mol->getAtomWithIdx(0);
        atom1 = mol->getAtomWithIdx(1);

        exp_bound = {{"C", atom1}, {"X", atom1}};
        BOOST_TEST(get_bound_attachment_point_names_and_atoms(atom0) == exp_bound);
        exp_available = {"N"};
        BOOST_TEST(get_available_attachment_point_names(atom0) == exp_available);

        exp_bound = {{"N", atom0}, {"X", atom0}};
        BOOST_TEST(get_bound_attachment_point_names_and_atoms(atom1) == exp_bound);
        exp_available = {"C"};
        BOOST_TEST(get_available_attachment_point_names(atom1) == exp_available);
    }
    
    // TODO: test CHEM monomers
    // TODO: test NA_PHOSPHATE monomers
}

} // namespace sketcher
} // namespace schrodinger
