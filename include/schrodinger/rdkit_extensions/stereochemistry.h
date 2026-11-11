#pragma once

#include <optional>
#include <string>

#include <boost/noncopyable.hpp>

#include "schrodinger/rdkit_extensions/definitions.h"

// Forward declarations:
namespace RDKit
{
class Atom;
class Conformer;
class Bond;
class ROMol;
enum class StereoGroupType;
} // namespace RDKit

namespace schrodinger
{
namespace rdkit_extensions
{

const std::string HAIR_SPACE = " ";
const std::string ABSOLUTE_STEREO_PREFIX = "abs" + HAIR_SPACE;
const std::string OR_STEREO_PREFIX = "or" + HAIR_SPACE;
const std::string AND_STEREO_PREFIX = "and" + HAIR_SPACE;

using EnhancedStereo = std::pair<RDKit::StereoGroupType, unsigned int>;

class RDKIT_EXTENSIONS_API UseModernStereoPerception : public boost::noncopyable
{
  public:
    UseModernStereoPerception();
    ~UseModernStereoPerception();

  private:
    bool m_stereo_algo_state;
};

/*
 * Assigns stereochemistry to the given molecule, from 3D or 2D if possible
 * @param rdk_mol rdkit mol
 */
RDKIT_EXTENSIONS_API void assign_stereochemistry(RDKit::ROMol& mol);

/**
 * A custom wrapper around RDkit's WedgeMolBonds() that makes sure
 * we don't wedge attachment point dummy atoms
 *
 * @param mol The molecule to wedge
 * @param conf The conformer to use when determining wedge directions
 * @param keep_existing_wedges If false, all existing single bond directions
 * (and any MDL wedging info) will be cleared and wedges will be recalculated
 * for all chiral centers.  If true, existing wedged, dashed, and wavy bonds
 * will be kept as-is, and wedges will only be added for chiral centers that
 * don't already have one.  This should only be used when the existing bond
 * directions are known to be consistent with the conformer.
 */
RDKIT_EXTENSIONS_API void wedgeMolBonds(RDKit::ROMol& mol,
                                        const RDKit::Conformer* conf,
                                        bool keep_existing_wedges = false);

} // namespace rdkit_extensions
} // namespace schrodinger