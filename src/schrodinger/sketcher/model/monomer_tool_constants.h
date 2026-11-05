/**
 * Qt-free definitions of the standard amino acid and nucleic acid monomer
 * tools, along with the tables that map them to residue names, full names and
 * display orders. These are kept separate from sketcher_model.h so that
 * Qt-free code can use them.
 */

#pragma once

#include <array>
#include <string>
#include <unordered_map>

namespace schrodinger
{
namespace sketcher
{

enum class AminoAcidTool {
    ALA,
    ARG,
    ASN,
    ASP,
    CYS,
    GLN,
    GLU,
    GLY,
    HIS,
    ILE,
    LEU,
    LYS,
    MET,
    PHE,
    PRO,
    SER,
    THR,
    TRP,
    TYR,
    VAL,
    UNK,
    UNCLASSIFIED,
};

const std::unordered_map<AminoAcidTool, std::string>
    AMINO_ACID_TOOL_TO_RES_NAME{
        {AminoAcidTool::ALA, "A"}, // clang-format off
        {AminoAcidTool::ARG, "R"},
        {AminoAcidTool::ASN, "N"},
        {AminoAcidTool::ASP, "D"},
        {AminoAcidTool::CYS, "C"},
        {AminoAcidTool::GLN, "Q"},
        {AminoAcidTool::GLU, "E"},
        {AminoAcidTool::GLY, "G"},
        {AminoAcidTool::HIS, "H"},
        {AminoAcidTool::ILE, "I"},
        {AminoAcidTool::LEU, "L"},
        {AminoAcidTool::LYS, "K"},
        {AminoAcidTool::MET, "M"},
        {AminoAcidTool::PHE, "F"},
        {AminoAcidTool::PRO, "P"},
        {AminoAcidTool::SER, "S"},
        {AminoAcidTool::THR, "T"},
        {AminoAcidTool::TRP, "W"},
        {AminoAcidTool::TYR, "Y"},
        {AminoAcidTool::VAL, "V"},
        {AminoAcidTool::UNK, "X"},
    }; // clang-format on

const std::unordered_map<AminoAcidTool, std::string>
    AMINO_ACID_TOOL_TO_FULL_NAME{
        {AminoAcidTool::ALA, "Alanine"}, // clang-format off
        {AminoAcidTool::ARG, "Arginine"},
        {AminoAcidTool::ASN, "Asparagine"},
        {AminoAcidTool::ASP, "Aspartate"},
        {AminoAcidTool::CYS, "Cysteine"},
        {AminoAcidTool::GLN, "Glutamine"},
        {AminoAcidTool::GLU, "Glutamate"},
        {AminoAcidTool::GLY, "Glycine"},
        {AminoAcidTool::HIS, "Histidine"},
        {AminoAcidTool::ILE, "Isoleucine"},
        {AminoAcidTool::LEU, "Leucine"},
        {AminoAcidTool::LYS, "Lysine"},
        {AminoAcidTool::MET, "Methionine"},
        {AminoAcidTool::PHE, "Phenylalanine"},
        {AminoAcidTool::PRO, "Proline"},
        {AminoAcidTool::SER, "Serine"},
        {AminoAcidTool::THR, "Threonine"},
        {AminoAcidTool::TRP, "Tryptophan"},
        {AminoAcidTool::TYR, "Tyrosine"},
        {AminoAcidTool::VAL, "Valine"},
        {AminoAcidTool::UNK, "Unknown"},
    }; // clang-format on

// Display order for the 21 natural amino acids — mirrored by the toolbar
// button layout (`monomer_tool_widget.cpp`) and by the Mutate Residue
// submenu in `MonomerContextMenu`. Both call sites iterate this array so
// they cannot drift apart.
constexpr std::array<AminoAcidTool, 21> AMINO_ACID_TOOL_DISPLAY_ORDER{
    AminoAcidTool::ALA, AminoAcidTool::ARG, AminoAcidTool::ASN,
    AminoAcidTool::ASP, AminoAcidTool::CYS, AminoAcidTool::GLN,
    AminoAcidTool::GLU, AminoAcidTool::GLY, AminoAcidTool::HIS,
    AminoAcidTool::ILE, AminoAcidTool::LEU, AminoAcidTool::LYS,
    AminoAcidTool::MET, AminoAcidTool::PHE, AminoAcidTool::PRO,
    AminoAcidTool::SER, AminoAcidTool::THR, AminoAcidTool::TRP,
    AminoAcidTool::TYR, AminoAcidTool::VAL, AminoAcidTool::UNK,
};

enum class NucleicAcidTool {
    A,
    U,
    G,
    C,
    T,
    N,
    R,
    P,
    dR,
    RNA_NUCLEOTIDE,
    DNA_NUCLEOTIDE,
    CUSTOM_NUCLEOTIDE,
    UNCLASSIFIED,
};

const std::unordered_map<NucleicAcidTool, std::string>
    NUCLEIC_ACID_TOOL_TO_RES_NAME{
        {NucleicAcidTool::A, "A"}, // clang-format off
        {NucleicAcidTool::U, "U"},
        {NucleicAcidTool::G, "G"},
        {NucleicAcidTool::C, "C"},
        {NucleicAcidTool::T, "T"},
        {NucleicAcidTool::N, "N"},
        {NucleicAcidTool::R, "R"},
        {NucleicAcidTool::P, "P"},
        {NucleicAcidTool::dR, "dR"},
    }; // clang-format on

const std::unordered_map<NucleicAcidTool, std::string> NA_BASE_TO_FULL_NAME{
    {NucleicAcidTool::A, "Adenine"}, {NucleicAcidTool::C, "Cytosine"},
    {NucleicAcidTool::G, "Guanine"}, {NucleicAcidTool::U, "Uracil"},
    {NucleicAcidTool::T, "Thymine"},
};

// Display order for the 5 natural NA bases — used by the Mutate Base submenu
// in `MonomerContextMenu`.
constexpr std::array<NucleicAcidTool, 5> NA_BASE_DISPLAY_ORDER{
    NucleicAcidTool::A, NucleicAcidTool::C, NucleicAcidTool::G,
    NucleicAcidTool::U, NucleicAcidTool::T,
};

} // namespace sketcher
} // namespace schrodinger
