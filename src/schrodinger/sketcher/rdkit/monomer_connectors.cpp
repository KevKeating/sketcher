#include "schrodinger/sketcher/rdkit/monomer_connectors.h"

#include <ranges>
#include <string>
#include <unordered_map>

#include <rdkit/GraphMol/Atom.h>
#include <rdkit/GraphMol/Bond.h>
#include <rdkit/GraphMol/MonomerInfo.h>
#include <rdkit/GraphMol/ROMol.h>

#include "schrodinger/rdkit_extensions/helm.h"

// TODO: rename this file to monomeric now that it includes get_monomer_type?

namespace schrodinger
{
namespace sketcher
{

namespace
{
const std::string PEPTIDE_POLYMER_PREFIX = "PEPTIDE";
// According to HELM, DNA is a subtype of RNA, so DNA also uses the RNA prefix
const std::string NUCLEOTIDE_POLYMER_PREFIX = "RNA";

const std::unordered_map<MonomerType, unsigned int> NUM_CONNECTIONS = {
    {MonomerType::PEPTIDE, 3},
    {MonomerType::NA_BASE, 1},
    {MonomerType::NA_PHOSPHATE, 2},
    {MonomerType::NA_SUGAR, 3},
};

} // namespace

/**
 * Determine what type of monomer the given atom represents.
 */
MonomerType get_monomer_type(const RDKit::Atom* atom)
{
    const auto* monomer_info = atom->getMonomerInfo();
    if (monomer_info == nullptr) {
        throw std::runtime_error("Atom has no monomer info");
    }
    const auto* res_info =
        dynamic_cast<const RDKit::AtomPDBResidueInfo*>(monomer_info);
    if (res_info == nullptr) {
        return MonomerType::CHEM;
    }
    const auto& chain_id = res_info->getChainId();
    if (chain_id.starts_with(PEPTIDE_POLYMER_PREFIX)) {
        return MonomerType::PEPTIDE;
    } else if (chain_id.starts_with(NUCLEOTIDE_POLYMER_PREFIX)) {
        const auto& res_name = res_info->getResidueName();
        if (res_name.empty()) {
            return MonomerType::NA_BASE;
        }
        auto last_char = std::tolower(res_name.back());
        if (last_char == 'p') {
            return MonomerType::NA_PHOSPHATE;
        } else if (last_char == 'r') {
            return MonomerType::NA_SUGAR;
        }
        return MonomerType::NA_BASE;
    }
    return MonomerType::CHEM;
}

bool contains_two_monomer_linkages(const RDKit::Bond* bond)
{
    std::string linkage, custom_linkage;
    bond->getPropIfPresent(LINKAGE, linkage);
    bool custom_linkage_exists =
        bond->getPropIfPresent(CUSTOM_BOND, custom_linkage);
    return custom_linkage_exists && custom_linkage != linkage;
}

int get_attachment_point_for_atom(std::string linkage, bool is_start_atom)
{
    // auto num_dashes = std::
    auto dash_pos = linkage.find("-");
    if (dash_pos == std::string::npos) {
        return -1;
    }
    std::string attachment_point_name = is_start_atom
                                            ? linkage.substr(0, dash_pos)
                                            : linkage.substr(dash_pos + 1);
    if (attachment_point_name[0] != 'R') {
        return -1;
    }
    // remove the leading 'R' now that we've confirmed it exists
    attachment_point_name.erase(0, 1);
    try {
        return std::stoi(attachment_point_name);
    } catch (const std::logic_error&) {
        // it's not an integer
        return -1;
    }
}

std::unordered_set<int> get_bound_attachment_points(const RDKit::Atom* monomer)
{
    const auto& mol = monomer->getOwningMol();
    std::unordered_set<int> bound_aps;

    auto record_linkage = [&bound_aps](const RDKit::Bond* bond,
                                       const bool is_start_atom,
                                       const std::string& prop_name) {
        std::string linkage;
        if (bond->getPropIfPresent(prop_name, linkage)) {
            auto ap_value =
                get_attachment_point_for_atom(linkage, is_start_atom);
            if (ap_value > 0) {
                bound_aps.insert(ap_value);
            }
        }
    };

    for (auto* bond : mol.atomBonds(monomer)) {
        bool is_start_atom = monomer == bond->getBeginAtom();
        record_linkage(bond, is_start_atom, LINKAGE);
        record_linkage(bond, is_start_atom, CUSTOM_BOND);
    }
    return bound_aps;
}

std::unordered_set<int>
get_available_attachment_points(const RDKit::Atom* monomer)
{
    auto bound_aps = get_bound_attachment_points(monomer);
    auto monomer_type = get_monomer_type(monomer);
    int num_aps = 3;
    if (monomer_type == MonomerType::CHEM) {
        
    }
    std::unordered_set<int> available_connectors;
    std::ranges::insert(available_connectors, std::views::iota(1, num_aps + 1));
}

} // namespace sketcher
} // namespace schrodinger
