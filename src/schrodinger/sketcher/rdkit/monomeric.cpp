#include "schrodinger/sketcher/rdkit/monomeric.h"

#include <fmt/core.h>

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
const std::unordered_map<MonomerType, std::vector<std::string>> AP_NAMES = {
    {MonomerType::PEPTIDE, {"N", "C", "X"}},
    {MonomerType::NA_BASE, {"S", "BP"}},
    // note that phosphate attachment point names reflect the attachment point
    // of the bound sugar (as the sites on the phosphate itself are chemically
    // identical), so they're only used when exactly one sugar is bound
    {MonomerType::NA_PHOSPHATE, {"3′", "5′"}},
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

static int get_attachment_point_for_atom(std::string linkage,
                                         bool is_start_atom)
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

/**
 * @return a set of all attachment points on the specified monomer that are
 * currently bound to another monomer. Attachment points are specified using
 * integers, e.g. 1 for "R1".
 */
static std::unordered_set<int> get_bound_attachment_points(const RDKit::Atom* monomer)
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

/**
 * @return a set of all attachment points on the specified monomer that are
 * currently unbound. Attachment points are specified using integers, e.g. 1 for
 * "R1".
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
        // we don't know how many attachment points a CHEM monomer should have,
        // so assume that it has one additional attachment point beyond what's
        // already bound
        num_aps = *std::max_element(bound_aps.begin(), bound_aps.end());
    }
    std::unordered_set<int> available_aps;
    for (int ap = 1; ap <= num_aps; ++ap) {
        if (!bound_aps.contains(ap)) {
            available_aps.insert(ap);
        }
    }
    return available_aps;
}

std::unordered_set<std::string>
get_available_attachment_point_names(const RDKit::Atom* monomer)
{
    auto available_aps = get_available_attachment_points(monomer);
    auto monomer_type = get_monomer_type(monomer);
    std::unordered_set<std::string> available_names;
    if (AP_NAMES.contains(monomer_type)) {
        auto all_names = AP_NAMES.at(monomer_type);
        if (monomer_type == MonomerType::NA_PHOSPHATE) {
            // Phosphate attachment point names reflect the attachment point of
            // the bound sugar, as the sites on the phosphate itself are
            // chemically identical. As a result, they're only meaningful when
            // exactly one sugar is bound, so use blank names otherwise.
            const auto& mol = monomer->getOwningMol();
            if (mol.getAtomDegree(monomer) == 1) {
                all_names = {"", ""};
            }
        }
        std::transform(
            available_aps.begin(), available_aps.end(),
            std::inserter(available_names, available_names.end()),
            [&all_names](int ap_num) { return all_names[ap_num - 1]; });
    } else {
        std::transform(
            available_aps.begin(), available_aps.end(),
            std::inserter(available_names, available_names.end()),
            [](int ap_num) { return  fmt::format("R{}", ap_num);});
    };
    return available_names;
}

} // namespace sketcher
} // namespace schrodinger
