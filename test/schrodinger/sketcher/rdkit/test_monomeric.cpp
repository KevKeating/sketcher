
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
    std::unordered_map<std::wstring, const RDKit::Atom*> exp_bound;
    std::unordered_set<std::wstring> exp_available;
    const RDKit::Atom *atom0, *atom1, *atom2;
    
    // a lone alanine has no bound attachment points
    auto mol = rdkit_extensions::to_rdkit("PEPTIDE1{A}$$$$V2.0");
    {
        atom0 = mol->getAtomWithIdx(0);
        BOOST_TEST(get_bound_attachment_point_names_and_atoms(atom0).empty());
        exp_available = {L"N", L"C", L"X"};
        BOOST_TEST(get_available_attachment_point_names(atom0) == exp_available);
    }

    // two alanines next to each other
    mol = rdkit_extensions::to_rdkit("PEPTIDE1{A.A}$$$$V2.0");
    {
        const auto* atom0 = mol->getAtomWithIdx(0);
        const auto* atom1 = mol->getAtomWithIdx(1);

        exp_bound = {{L"C", atom1}};
        BOOST_TEST(get_bound_attachment_point_names_and_atoms(atom0) == exp_bound);
        exp_available = {L"N", L"X"};
        BOOST_TEST(get_available_attachment_point_names(atom0) == exp_available);

        exp_bound = {{L"N", atom0}};
        BOOST_TEST(get_bound_attachment_point_names_and_atoms(atom1) == exp_bound);
        exp_available = {L"C", L"X"};
        BOOST_TEST(get_available_attachment_point_names(atom1) == exp_available);
    }

    // a side-chain interaction between two non-adjacent residues
    mol = rdkit_extensions::to_rdkit(
        "PEPTIDE1{C.A.C}$PEPTIDE1,PEPTIDE1,1:R3-3:R3$$$V2.0");
    {
        atom0 = mol->getAtomWithIdx(0);
        atom1 = mol->getAtomWithIdx(1);
        atom2 = mol->getAtomWithIdx(2);
        
        exp_bound = {{L"C", atom1}, {L"X", atom2}};
        BOOST_TEST(get_bound_attachment_point_names_and_atoms(atom0) == exp_bound);
        exp_available = {L"N"};
        BOOST_TEST(get_available_attachment_point_names(atom0) == exp_available);

        exp_bound = {{L"N", atom0}, {L"C", atom2}};
        BOOST_TEST(get_bound_attachment_point_names_and_atoms(atom1) == exp_bound);
        exp_available = {L"X"};
        BOOST_TEST(get_available_attachment_point_names(atom1) == exp_available);

        exp_bound = {{L"N", atom1}, {L"X", atom0}};
        BOOST_TEST(get_bound_attachment_point_names_and_atoms(atom2) == exp_bound);
        exp_available = {L"C"};
        BOOST_TEST(get_available_attachment_point_names(atom2) == exp_available);
    }

    // a side-chain interaction between two adjacent residues
    mol = rdkit_extensions::to_rdkit(
        "PEPTIDE1{C.C}$PEPTIDE1,PEPTIDE1,1:R3-2:R3$$$V2.0");
    {
        atom0 = mol->getAtomWithIdx(0);
        atom1 = mol->getAtomWithIdx(1);

        exp_bound = {{L"C", atom1}, {L"X", atom1}};
        BOOST_TEST(get_bound_attachment_point_names_and_atoms(atom0) == exp_bound);
        exp_available = {L"N"};
        BOOST_TEST(get_available_attachment_point_names(atom0) == exp_available);

        exp_bound = {{L"N", atom0}, {L"X", atom0}};
        BOOST_TEST(get_bound_attachment_point_names_and_atoms(atom1) == exp_bound);
        exp_available = {L"C"};
        BOOST_TEST(get_available_attachment_point_names(atom1) == exp_available);
    }
    
    // CHEM monomers
    mol = rdkit_extensions::to_rdkit(
        "CHEM1{[MONO1]}|CHEM2{[MONO2]}$CHEM1,CHEM2,1:R1-1:R3$$$V2.0");
    {
        atom0 = mol->getAtomWithIdx(0);
        atom1 = mol->getAtomWithIdx(1);

        exp_bound = {{L"R1", atom1}};
        BOOST_TEST(get_bound_attachment_point_names_and_atoms(atom0) == exp_bound);
        exp_available = {L"R2"};
        BOOST_TEST(get_available_attachment_point_names(atom0) == exp_available);

        exp_bound = {{L"R3", atom0}};
        BOOST_TEST(get_bound_attachment_point_names_and_atoms(atom1) == exp_bound);
        exp_available = {L"R1", L"R2", L"R4"};
        BOOST_TEST(get_available_attachment_point_names(atom1) == exp_available);
    }
    
    // NA_PHOSPHATE monomers name their unbound attachment points after the
    // attachment point of the bound sugar
    mol = rdkit_extensions::to_rdkit("RNA1{P.R(U)P.R(T)P}$$$$");
    {
        atom0 = mol->getAtomWithIdx(0);
        atom1 = mol->getAtomWithIdx(1);
        auto term_sugar = mol->getAtomWithIdx(5);
        auto term_phosphate = mol->getAtomWithIdx(6);
        
        exp_bound = {{L"", atom1}};
        BOOST_TEST(get_bound_attachment_point_names_and_atoms(atom0) == exp_bound);
        exp_available = {L"3′"};
        // std::cout << "get_available_attachment_point_names(atom0) = \n";
        // for (auto cur_name : get_available_attachment_point_names(atom0)) {
        //     std::cout << "\t" << cur_name << "\n";
        // }
        BOOST_TEST(get_available_attachment_point_names(atom0) == exp_available);

        exp_bound = {{L"", term_sugar}};
        BOOST_TEST(get_bound_attachment_point_names_and_atoms(term_phosphate) == exp_bound);
        exp_available = {L"5′"};
        BOOST_TEST(get_available_attachment_point_names(term_phosphate) == exp_available);
    }

    // NA_PHOSPHATE monomers still take their name from the bound sugar even if
    // they're at the end of a chain of phsophates
    mol = rdkit_extensions::to_rdkit("RNA1{P.R(U)P.R(T)P.P.P}$$$$");
    {
        auto term_sugar = mol->getAtomWithIdx(5);
        auto term_phos_chain_1 = mol->getAtomWithIdx(6);
        auto term_phos_chain_2 = mol->getAtomWithIdx(7);
        auto term_phos_chain_3 = mol->getAtomWithIdx(8);

        // TODO: both bound attachment points use empty names
        BOOST_TEST(get_available_attachment_point_names(term_phos_chain_1).empty());
        BOOST_TEST(get_available_attachment_point_names(term_phos_chain_2).empty());
        exp_bound = {{L"", term_phos_chain_2}};
        BOOST_TEST(get_bound_attachment_point_names_and_atoms(term_phos_chain_3) == exp_bound);
        exp_available = {L"5′"};
        BOOST_TEST(get_available_attachment_point_names(term_phos_chain_3) == exp_available);
    }

    // an amino acid with an unrecognized attachment point
    mol = rdkit_extensions::to_rdkit(
        "PEPTIDE1{A.A}$PEPTIDE1,PEPTIDE1,1:R3-2:R4$$$V2.0");
    {
        atom0 = mol->getAtomWithIdx(0);
        atom1 = mol->getAtomWithIdx(1);

        exp_bound = {{L"C", atom1}, {L"X", atom1}};
        BOOST_TEST(get_bound_attachment_point_names_and_atoms(atom0) == exp_bound);
        exp_available = {L"N"};
        BOOST_TEST(get_available_attachment_point_names(atom0) == exp_available);

        exp_bound = {{L"N", atom0}, {L"R4", atom0}};
        BOOST_TEST(get_bound_attachment_point_names_and_atoms(atom1) == exp_bound);
        exp_available = {L"C", L"X"};
        BOOST_TEST(get_available_attachment_point_names(atom1) == exp_available);
    }
}

} // namespace sketcher
} // namespace schrodinger
