#pragma once

#include <QGraphicsItem>
#include <QGraphicsItemGroup>
#include <QList>

#include <rdkit/GraphMol/ROMol.h>

#include "schrodinger/rdkit_extensions/definitions.h"
#include "schrodinger/sketcher/molviewer/constants.h"

namespace RDKit
{
class ROMol;
}

namespace schrodinger::sketcher
{

class Fonts;

// TODO: bond is visible behind monomer because of transparent background
// TODO: bond is hidden by predictive highlighting
// TODO: add attachment point labels for bond
class MonomerHintFragmentItem : public QGraphicsItemGroup
{
  public:
    /**
     * @param fragment The fragment to display
     * @param fonts The fonts to use for displaying the fragment.  This object
     * must not be destroyed while this graphics item is in use.
     * @param parent The parent graphics item, if any.
     */
    MonomerHintFragmentItem(const RDKit::ROMol& fragment, const Fonts& fonts, const int atom_index_to_hide, const QColor monomer_background_color,
                             QGraphicsItem* parent = nullptr);

    // TODO: get rid of this method?
    void updateConformer(const RDKit::Conformer& conformer);

  protected:
    RDKit::ROMol m_frag;
    const Fonts* m_fonts = nullptr;
    int m_atom_index_to_hide = -1;
    QColor m_monomer_background_color;
    /// A list of all child AbstractMonomerItems
    QList<QGraphicsItem*> m_atom_items;
    /// A list of all child MonomerConnectortems
    QList<QGraphicsItem*> m_bond_items;

    /**
     * Create all graphics items required to represent the fragment
     */
    void createGraphicsItems();
};

}