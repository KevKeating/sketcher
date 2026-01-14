#include "schrodinger/sketcher/rdkit/monomer_connectors.h"

#include <string>
#include <unordered_map>

#include <rdkit/GraphMol/Atom.h>
#include <rdkit/GraphMol/Bond.h>
#include <rdkit/GraphMol/MonomerInfo.h>

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

std::vector<std::string> get_available_connectors(const RDKit::Atom* monomer)
{
    std::vector<std::string> available_connectors;
    auto monomer_type = get_monomer_type(monomer);
    if (monomer_type == MonomerType::CHEM) {
        
        return available_connectors;
    }
    
}

} // namespace sketcher
} // namespace schrodinger
