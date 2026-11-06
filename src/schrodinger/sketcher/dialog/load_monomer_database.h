#pragma once

#include "schrodinger/sketcher/definitions.h"

class QWidget;

namespace schrodinger
{
namespace sketcher
{

/**
 * Ask the user to choose a monomer database JSON file, then load it as the
 * custom monomer database, replacing any existing custom monomers. The result
 * (success or the reasons for any failures) is reported in a dialog. Nothing
 * is loaded and no dialog is shown if the user cancels the file chooser.
 *
 * Note that this function returns before the user chooses a file, since the
 * file chooser is asynchronous on WASM builds.
 *
 * @param parent The parent widget for the file chooser and result dialogs
 */
SKETCHER_API void load_monomer_database_from_file(QWidget* parent);

} // namespace sketcher
} // namespace schrodinger
