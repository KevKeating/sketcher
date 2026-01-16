
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
    std::unordered_set<std::string> exp_names;
    std::unordered_map<std::string, const RDKit::Atom*> exp_bound;
    const RDKit::Atom *atom0, *atom1, *atom2;
    
    // a lone alanine has no bound attachment points
    auto mol = rdkit_extensions::to_rdkit("PEPTIDE1{A}$$$$V2.0");
    {
        atom0 = mol->getAtomWithIdx(0);
        BOOST_TEST(get_bound_attachment_point_names_and_bound_atoms(atom0).empty());
        exp_names = {"N", "C", "X"};
        BOOST_TEST(get_available_attachment_point_names(atom0) == exp_names);
    }

    // two alanines next to each other
    mol = rdkit_extensions::to_rdkit("PEPTIDE1{A.A}$$$$V2.0");
    {
        // the first alanine has a bound C-terminus
        const auto* atom0 = mol->getAtomWithIdx(0);
        const auto* atom1 = mol->getAtomWithIdx(1);
        exp_bound = {{"C", atom1}};
        BOOST_TEST(get_bound_attachment_point_names_and_bound_atoms(atom0) == exp_bound);
        exp_names = {"N", "X"};
        BOOST_TEST(get_available_attachment_point_names(atom0) == exp_names);

        // the second alanine has a bound N-terminus
        exp_bound = {{"N", atom0}};
        BOOST_TEST(get_bound_attachment_point_names_and_bound_atoms(atom1) == exp_bound);
        exp_names = {"C", "X"};
        BOOST_TEST(get_available_attachment_point_names(atom1) == exp_names);
    }

    // a side-chain interaction between two non-adjacent residues
    mol = rdkit_extensions::to_rdkit(
        "PEPTIDE1{C.A.C}$PEPTIDE1,PEPTIDE1,1:R3-3:R3$$$V2.0");
    {
        atom0 = mol->getAtomWithIdx(0);
        atom1 = mol->getAtomWithIdx(1);
        atom2 = mol->getAtomWithIdx(2);
        
        exp_bound = {{"C": atom1, "X"};
        BOOST_TEST(get_bound_attachment_point_names_and_bound_atoms(atom0) == exp_bound);
        exp_names = {"N"};
        BOOST_TEST(get_available_attachment_point_names(atom0) == exp_names);

        exp_bound = {1, 2};
        BOOST_TEST(get_bound_attachment_point_names_and_bound_atoms(atom1) == exp_bound);
        exp_names = {"X"};
        BOOST_TEST(get_available_attachment_point_names(atom1) == exp_names);

        exp_bound = {1, 3};
        BOOST_TEST(get_bound_attachment_point_names_and_bound_atoms(atom2) == exp_bound);
        exp_names = {"C"};
        BOOST_TEST(get_available_attachment_point_names(atom2) == exp_names);
    }

    // // a side-chain interaction between two adjacent residues
    // mol = rdkit_extensions::to_rdkit(
    //     "PEPTIDE1{C.C}$PEPTIDE1,PEPTIDE1,1:R3-2:R3$$$V2.0");
    // atom = mol->getAtomWithIdx(0);
    // exp_bound = {2, 3};
    // BOOST_TEST(get_bound_attachment_points(atom) == exp_bound);
    // exp_available = {1};
    // BOOST_TEST(get_available_attachment_points(atom) == exp_available);
    // exp_names = {"N"};
    // BOOST_TEST(get_available_attachment_point_names(atom) == exp_names);

    // atom = mol->getAtomWithIdx(1);
    // exp_bound = {1, 3};
    // BOOST_TEST(get_bound_attachment_points(atom) == exp_bound);
    // exp_available = {2};
    // BOOST_TEST(get_available_attachment_points(atom) == exp_available);
    // exp_names = {"C"};
    // BOOST_TEST(get_available_attachment_point_names(atom) == exp_names);
    
    // TODO: test CHEM monomers
    // TODO: test NA_PHOSPHATE monomers
}

} // namespace sketcher
} // namespace schrodinger
