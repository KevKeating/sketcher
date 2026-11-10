
#define BOOST_TEST_MODULE mol_update

#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>
#include "schrodinger/sketcher/rdkit/mol_update.h"
#include "schrodinger/rdkit_extensions/convert.h"
#include "schrodinger/rdkit_extensions/constants.h"
#include <algorithm>

#include <rdkit/GraphMol/Chirality.h>
#include <rdkit/GraphMol/RWMol.h>

namespace schrodinger
{
namespace sketcher
{

namespace bdata = boost::unit_test::data;

std::string LIST_QUERY = R"(
     RDKit          2D

  0  0  0  0  0  0  0  0  0  0999 V3000
M  V30 BEGIN CTAB
M  V30 COUNTS 3 2 0 0 0
M  V30 BEGIN ATOM
M  V30 1 C -14.969697 7.242424 0.000000 0
M  V30 2 [C,N] -13.670659 7.992424 0.000000 0
M  V30 3 C -12.371621 7.242424 0.000000 0
M  V30 END ATOM
M  V30 BEGIN BOND
M  V30 1 1 1 2
M  V30 2 1 2 3
M  V30 END BOND
M  V30 END CTAB
M  END
$$$$)";

// SKETCH-2710: a molecule with query bonds (V3000 bond type 8 = "any") and a
// stereocenter must not throw when update_molecule_on_change is called. The
// CIPLabeler cannot handle non-integer (UNSPECIFIED) bond orders produced by
// query bonds, so CIP label assignment must be skipped for such molecules.
std::string QUERY_BOND_WITH_STEREO = R"(
     RDKit          2D

  0  0  0  0  0  0  0  0  0  0999 V3000
M  V30 BEGIN CTAB
M  V30 COUNTS 11 11 0 0 0
M  V30 BEGIN ATOM
M  V30 1 C 1.275976 0.000000 0.000000 0
M  V30 2 C 0.394298 1.213525 0.000000 0
M  V30 3 C -1.032286 0.750000 0.000000 0
M  V30 4 C -1.032286 -0.750000 0.000000 0
M  V30 5 C 0.394298 -1.213525 0.000000 0
M  V30 6 C -2.245812 1.631678 0.000000 0
M  V30 7 C -2.089019 3.123461 0.000000 0
M  V30 8 C -0.718701 3.733566 0.000000 0
M  V30 9 C -3.302545 4.005139 0.000000 0
M  V30 10 N 0.494824 2.851888 0.000000 0
M  V30 11 C -0.561908 5.225349 0.000000 0
M  V30 END ATOM
M  V30 BEGIN BOND
M  V30 1 8 1 2
M  V30 2 8 2 3
M  V30 3 8 3 4
M  V30 4 8 4 5
M  V30 5 8 5 1
M  V30 6 1 3 6
M  V30 7 2 6 7
M  V30 8 1 7 8
M  V30 9 1 7 9
M  V30 10 1 8 10
M  V30 11 1 8 11 CFG=1
M  V30 END BOND
M  V30 BEGIN COLLECTION
M  V30 MDLV30/STEABS ATOMS=(1 8)
M  V30 END COLLECTION
M  V30 END CTAB
M  END
)";

BOOST_AUTO_TEST_CASE(test_update_mol_query_bonds_with_stereo_no_throw)
{
    auto mol = rdkit_extensions::to_rdkit(
        QUERY_BOND_WITH_STEREO, rdkit_extensions::Format::MDL_MOLV3000);
    BOOST_REQUIRE(mol != nullptr);
    // prepare_mol applies wedge bond directions from V3000 CFG fields, which
    // sets CHI tags on atom 3 — needed to trigger CIPLabeler traversal.
    prepare_mol(*mol);
    BOOST_REQUIRE_NO_THROW(update_molecule_on_change(*mol));
}

/**
 * Make sure that prepare_mol flags list queries as dummy atoms
 */
BOOST_AUTO_TEST_CASE(test_prepare_mol_list_query)
{
    auto mol = rdkit_extensions::to_rdkit(
        LIST_QUERY, rdkit_extensions::Format::MDL_MOLV3000);
    BOOST_TEST(mol->getNumAtoms() == 3);

    prepare_mol(*mol);

    // the second atom is a list query, check that it's a dummy atom
    BOOST_TEST(mol->getAtomWithIdx(1)->getAtomicNum() ==
               rdkit_extensions::DUMMY_ATOMIC_NUMBER);
}

// A single stereocenter (atom 3) with the wedge on the C3-C4 bond (bond index
// 2)
std::string WEDGED_STEREOCENTER = R"(
     RDKit          2D

  0  0  0  0  0  0  0  0  0  0999 V3000
M  V30 BEGIN CTAB
M  V30 COUNTS 5 4 0 0 0
M  V30 BEGIN ATOM
M  V30 1 C 0.000000 0.000000 0.000000 0
M  V30 2 C 1.299038 0.750000 0.000000 0
M  V30 3 C 2.598076 -0.000000 0.000000 0
M  V30 4 C 3.897114 0.750000 0.000000 0
M  V30 5 O 2.598076 -1.500000 0.000000 0
M  V30 END ATOM
M  V30 BEGIN BOND
M  V30 1 1 1 2
M  V30 2 1 2 3
M  V30 3 1 3 4 CFG=1
M  V30 4 1 3 5
M  V30 END BOND
M  V30 BEGIN COLLECTION
M  V30 MDLV30/STEABS ATOMS=(1 3)
M  V30 END COLLECTION
M  V30 END CTAB
M  END
)";

/**
 * Load WEDGED_STEREOCENTER, then move the wedge from the C3-C4 bond to the C3-O
 * bond the same way a user would using RDKit directly, i.e. without any MDL
 * wedging info.
 */
static auto get_mol_with_nonstandard_wedge()
{
    auto mol = rdkit_extensions::to_rdkit(
        WEDGED_STEREOCENTER, rdkit_extensions::Format::MDL_MOLV3000);
    for (auto bond : mol->bonds()) {
        bond->setBondDir(RDKit::Bond::BondDir::NONE);
        bond->clearProp(RDKit::common_properties::_MolFileBondCfg);
    }
    auto o_bond = mol->getBondWithIdx(3);
    RDKit::Chirality::wedgeBond(o_bond, o_bond->getBeginAtomIdx(),
                                &mol->getConformer());
    return mol;
}

/**
 * SKETCH-2872: wedges set on an RDKit mol with 2D coordinates should be
 * honored, even if there's no MDL wedging info present
 */
BOOST_AUTO_TEST_CASE(test_prepare_mol_keeps_existing_wedges)
{
    auto mol = get_mol_with_nonstandard_wedge();
    auto o_bond = mol->getBondWithIdx(3);
    auto input_dir = o_bond->getBondDir();
    BOOST_REQUIRE(input_dir == RDKit::Bond::BondDir::BEGINWEDGE ||
                  input_dir == RDKit::Bond::BondDir::BEGINDASH);
    auto input_chirality = mol->getAtomWithIdx(2)->getChiralTag();

    prepare_mol(*mol);
    BOOST_TEST(o_bond->getBondDir() == input_dir);
    BOOST_TEST(mol->getBondWithIdx(2)->getBondDir() ==
               RDKit::Bond::BondDir::NONE);

    // make sure that the stereochemistry is unchanged once the sketcher
    // recalculates it from the wedges
    update_molecule_on_change(*mol);
    BOOST_TEST(o_bond->getBondDir() == input_dir);
    BOOST_TEST(mol->getAtomWithIdx(2)->getChiralTag() == input_chirality);
}

/**
 * SKETCH-2872: If an input mol with 2D coordinates has wedges that contradict
 * its chiral tags, the wedges take precedence and the stereochemistry is
 * recalculated from them (the same as for MDL input). This test documents that
 * behavior so that it isn't changed unintentionally.
 */
BOOST_AUTO_TEST_CASE(test_prepare_mol_wedges_override_conflicting_chiral_tags)
{
    auto mol = get_mol_with_nonstandard_wedge();
    auto chiral_atom = mol->getAtomWithIdx(2);
    auto input_chirality = chiral_atom->getChiralTag();
    BOOST_REQUIRE(input_chirality == RDKit::Atom::CHI_TETRAHEDRAL_CW ||
                  input_chirality == RDKit::Atom::CHI_TETRAHEDRAL_CCW);
    auto inverted_chirality = input_chirality == RDKit::Atom::CHI_TETRAHEDRAL_CW
                                  ? RDKit::Atom::CHI_TETRAHEDRAL_CCW
                                  : RDKit::Atom::CHI_TETRAHEDRAL_CW;

    // flip the wedge so that it no longer agrees with the chiral tag
    auto o_bond = mol->getBondWithIdx(3);
    auto conflicting_dir =
        o_bond->getBondDir() == RDKit::Bond::BondDir::BEGINWEDGE
            ? RDKit::Bond::BondDir::BEGINDASH
            : RDKit::Bond::BondDir::BEGINWEDGE;
    o_bond->setBondDir(conflicting_dir);

    prepare_mol(*mol);
    BOOST_TEST(o_bond->getBondDir() == conflicting_dir);
    BOOST_TEST(mol->getBondWithIdx(2)->getBondDir() ==
               RDKit::Bond::BondDir::NONE);

    update_molecule_on_change(*mol);
    BOOST_TEST(o_bond->getBondDir() == conflicting_dir);
    BOOST_TEST(chiral_atom->getChiralTag() == inverted_chirality);
}

/**
 * If the input has no coordinates, then any existing bond directions are
 * meaningless and should be recalculated
 */
BOOST_AUTO_TEST_CASE(test_prepare_mol_recalculates_wedges_without_coords)
{
    auto mol = get_mol_with_nonstandard_wedge();
    // add a wedge to a bond that doesn't touch a stereocenter, which no
    // wedging calculation would ever produce
    auto non_chiral_bond = mol->getBondWithIdx(0);
    non_chiral_bond->setBondDir(RDKit::Bond::BondDir::BEGINWEDGE);
    mol->clearConformers();

    prepare_mol(*mol);
    BOOST_TEST(non_chiral_bond->getBondDir() == RDKit::Bond::BondDir::NONE);
    // the stereocenter should still have exactly one wedge
    auto bonds = mol->bonds();
    auto num_wedged = std::count_if(bonds.begin(), bonds.end(), [](auto bond) {
        return bond->getBondDir() == RDKit::Bond::BondDir::BEGINWEDGE ||
               bond->getBondDir() == RDKit::Bond::BondDir::BEGINDASH;
    });
    BOOST_TEST(num_wedged == 1);
}

} // namespace sketcher
} // namespace schrodinger
