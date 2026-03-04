#pragma once

#include <QGraphicsItem>
#include <QGraphicsItemGroup>
#include <QList>

#include <rdkit/GraphMol/ROMol.h>

#include "schrodinger/rdkit_extensions/definitions.h"

namespace RDKit
{
class ROMol;
}

namespace schrodinger::sketcher
{

class Fonts;

class MonomerHintFragmentItem : public QGraphicsItemGroup
{
  public:
    /**
     * @param fragment The fragment to display
     * @param fonts The fonts to use for displaying the fragment.  This object
     * must not be destroyed while this graphics item is in use.
     * @param parent The parent graphics item, if any.
     */
    MonomerHintFragmentItem(const RDKit::ROMol& fragment, const Fonts& fonts, const int atom_index_to_hide,
                             QGraphicsItem* parent = nullptr);

    void updateConformer(const RDKit::Conformer& conformer);

  protected:
    RDKit::ROMol m_frag;
    const Fonts* m_fonts = nullptr;
    int m_atom_index_to_hide = -1;
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