#pragma once

namespace schrodinger
{
namespace sketcher
{

enum class SelectMode {
    SELECT,      // Shift + click
    DESELECT,    //
    TOGGLE,      // Ctrl + click
    SELECT_ONLY, // normal click (no Shift or Ctrl)
};

/**
 * Possible selection tools to equip
 */
enum class SelectionTool {
    RECTANGLE,
    ELLIPSE,
    LASSO,
    FRAGMENT,
};

enum class InterfaceType {
    ATOMISTIC = 1 << 0,
    MONOMERIC = 1 << 1,
    ATOMISTIC_OR_MONOMERIC = ATOMISTIC | MONOMERIC,
};

} // namespace sketcher
} // namespace schrodinger
