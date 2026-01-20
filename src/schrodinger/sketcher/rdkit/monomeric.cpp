#include "schrodinger/sketcher/rdkit/monomeric.h"

#include <functional>

#include <fmt/core.h>
#include <fmt/xchar.h>

#include <rdkit/GraphMol/Atom.h>
#include <rdkit/GraphMol/Bond.h>
#include <rdkit/GraphMol/MonomerInfo.h>
#include <rdkit/GraphMol/ROMol.h>

#include "schrodinger/rdkit_extensions/helm.h"

namespace schrodinger
{
namespace sketcher
{

namespace
{
const std::string PEPTIDE_POLYMER_PREFIX = "PEPTIDE";
// According to HELM, DNA is a subtype of RNA, so DNA also uses the RNA prefix
const std::string NUCLEOTIDE_POLYMER_PREFIX = "RNA";

// note that the ′ marks are unicode primes, not apostrophes
// NA_PHOSPHATE takes its attachment point names from the bound sugar
// CHEM monomers don't have "pretty" attachment point names (we just use R1, R2, etc.)
const std::unordered_map<MonomerType, std::vector<std::wstring>> AP_NAMES = {
    {MonomerType::PEPTIDE, {"N", "C", "X"}},
    {MonomerType::NA_BASE, {"S", "BP"}},
    // {MonomerType::NA_PHOSPHATE, {"3′", "5′"}},
    {MonomerType::NA_SUGAR, {"3′", "5′", "X"}},
};

} // namespace

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

/**
 * Return the number of the attachment point specified in the given linkage
 * string.
 * @param linkage A description of the linkage taken from the bond properties.
 * Formatted similar to "R2-R3".
 * @param is_begin_atom If true, we'll return the attachment point for the
 * bond's begin atom.  Otherwise, we'll return the attachment point for the
 * bond's end atom.
 */
static int get_attachment_point_for_atom(std::string linkage,
                                         bool is_begin_atom)
{
    auto dash_pos = linkage.find("-");
    if (dash_pos == std::string::npos) {
        return -1;
    }
    std::string attachment_point_name = is_begin_atom
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

static int get_attachment_point_for_atom(const RDKit::Atom* monomer, const RDKit::Bond* bond)
{
    std::string linkage;
    if (bond->getPropIfPresent(LINKAGE, linkage)) {
        bool is_start_atom = bond->getBeginAtom() == monomer;
        return get_attachment_point_for_atom(linkage, is_start_atom);
    }
    return -1;
}

/**
 * @return a map of {attachment point: bound monomer} for all bound attachment
 * points of the specified monomer. Attachment points are specified using
 * integers, e.g. 1 for "R1".
 */
static std::unordered_map<int, const RDKit::Atom*>
get_bound_attachment_points(const RDKit::Atom* monomer)
{
    const auto& mol = monomer->getOwningMol();
    std::unordered_map<int, const RDKit::Atom*> bound_aps;

    auto record_linkage = [&bound_aps, monomer](const RDKit::Bond* bond,
                                                const bool is_start_atom,
                                                const std::string& prop_name) {
        std::string linkage;
        if (bond->getPropIfPresent(prop_name, linkage)) {
            auto ap_value =
                get_attachment_point_for_atom(linkage, is_start_atom);
            if (ap_value > 0) {
                bound_aps[ap_value] = bond->getOtherAtom(monomer);
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

/**
 * @return a set of all attachment points on the specified monomer that are
 * currently unbound. Attachment points are specified using integers, e.g. 1 for
 * "R1".
 *
 * Note that we don't have a good way to determine how many attachment points a
 * CHEM monomer should have, so we assume that it has one additional attachment
 * point beyond the highest numbered bound attachment point.
 */
static std::unordered_set<int>
get_available_attachment_points(const RDKit::Atom* monomer)
{
    auto bound_aps = get_bound_attachment_points(monomer);
    auto monomer_type = get_monomer_type(monomer);
    int num_aps = -1;
    if (AP_NAMES.contains(monomer_type)) {
        num_aps = AP_NAMES.at(monomer_type).size();
    } else {
        // a CHEM monomer
        std::unordered_set<int> bound_ap_nums;
        std::transform(bound_aps.begin(), bound_aps.end(),
                       std::inserter(bound_ap_nums, bound_ap_nums.end()),
                       [](auto num_and_atom) { return num_and_atom.first; });
        num_aps = *std::max_element(bound_ap_nums.begin(), bound_ap_nums.end());
        num_aps += 1;
    }
    std::unordered_set<int> available_aps;
    for (int ap = 1; ap <= num_aps; ++ap) {
        if (!bound_aps.contains(ap)) {
            available_aps.insert(ap);
        }
    }
    return available_aps;
}

/**
 * Convert an attachment point number to a name
 * @param ap_num The attachment point number to convert
 * @param all_names A list of "pretty" names for attachment points, starting
 * with R1.
 * @return If all_names contains a "pretty" name for ap_num, then that name will
 * be returned. Otherwise "R<ap_num>" will be returned.
 */
static std::wstring
ap_num_to_name(const int ap_num, const std::vector<std::wstring>& all_names)
{
    if (0 < ap_num && ap_num <= all_names.size()) {
        return all_names[ap_num - 1];
    }
    return fmt::format(L"R{}", ap_num);
}

/**
 * @return a list of all "pretty" attachment point names (e.g. "N" instead of
 * "R1" for amino acids) for the given monomer, regardless of whether those
 * attachment points are bound or available.
 *
 * Note that CHEM monomers don't have special names, so we return an empty list.
 *
 * Also note that phosphate attachment point names reflect the attachment point
 * of the bound sugar, as the sites themselves are chemically identical. As a
 * result, these names are only meaningful when exactly one sugar is bound.
 * Because of this, we return blank attachment point names (i.e. empty strings,
 * *not* an empty list) unless there is exactly one attachment point bound to a
 * sugar.
 */
static std::vector<std::wstring>
get_all_attachment_point_names(const RDKit::Atom* monomer)
{
    auto monomer_type = get_monomer_type(monomer);
    
    if (AP_NAMES.contains(monomer_type)) {
        return AP_NAMES.at(monomer_type);
    } else if (monomer_type == MonomerType::NA_PHOSPHATE) {
        const auto& mol = monomer->getOwningMol();
        // If this phosphate is bound to a sugar, or if it's at the end of a
        // chain of phosphates bound to a sugar, then use the attachment point
        // name from the sugar for the unbound attachment point. In all other
        // scenarios, leave the attachment points unnamed since they're
        // chemically identical and there's no meaningful point of reference
        
        // TODO: create get_ap_num_of_bound_sugar or something like that, just
        //       so that the weird logic is behing a clearly named function?
        std::vector<std::wstring> phos_ap_names = {L"", L""};
        if (mol.getAtomDegree(monomer) != 1) {
            return phos_ap_names;
        }
        const RDKit::Bond* phos_bond = *mol.atomBonds(monomer).begin();
        auto neighbor = monomer;
        const RDKit::Bond* neighbor_bond;
        do {
            neighbor_bond = *mol.atomBonds(neighbor).begin();
            neighbor = *mol.atomNeighbors(neighbor).begin();
        } while (mol.getAtomDegree(neighbor) == 1 && get_monomer_type(neighbor) == MonomerType::NA_PHOSPHATE);
        if (get_monomer_type(neighbor) == MonomerType::NA_SUGAR) {
            auto sugar_ap_num = get_attachment_point_for_atom(neighbor, neighbor_bond);
            auto bound_phos_ap_num =  get_attachment_point_for_atom(monomer, phos_bond);
            if ((sugar_ap_num == 1 || sugar_ap_num == 2) && (bound_phos_ap_num == 1 || bound_phos_ap_num == 2)) {
                auto sugar_ap_name = ap_num_to_name(sugar_ap_num, AP_NAMES.at(MonomerType::NA_SUGAR));
                int unbound_phos_ap_name_idx = bound_phos_ap_num == 1 ? 1 : 0;
                phos_ap_names[unbound_phos_ap_name_idx] = sugar_ap_name;
            }
        }
        return phos_ap_names;
    } else {
        // for CHEM monomers, we return an empty list, meaning that the
        // attachment points will be named R1, R2, etc
        return {};
    }
}

// TODO: attachment points for phosphates aren't uniquely names - use vector of pairs instead?
std::unordered_map<std::wstring, const RDKit::Atom*>
get_bound_attachment_point_names_and_atoms(const RDKit::Atom* monomer)
{
    auto bound_aps = get_bound_attachment_points(monomer);
    auto all_names = get_all_attachment_point_names(monomer);
    std::unordered_map<std::wstring, const RDKit::Atom*> bound_ap_names;
    std::transform(bound_aps.begin(), bound_aps.end(),
                   std::inserter(bound_ap_names, bound_ap_names.end()),
                   [&all_names](auto num_and_atom) {
                       auto& [ap_num, atom] = num_and_atom;
                       auto ap_name = ap_num_to_name(ap_num, all_names);
                       return std::make_pair(ap_name, atom);
                   });
    return bound_ap_names;
}

std::unordered_set<std::wstring>
get_available_attachment_point_names(const RDKit::Atom* monomer)
{
    auto available_aps = get_available_attachment_points(monomer);
    auto all_names = get_all_attachment_point_names(monomer);
    std::unordered_set<std::wstring> available_names;
    std::transform(available_aps.begin(), available_aps.end(),
                   std::inserter(available_names, available_names.end()),
                   std::bind(ap_num_to_name, std::placeholders::_1, all_names));
    return available_names;
}

} // namespace sketcher
} // namespace schrodinger
