#include "schrodinger/sketcher/molviewer/monomer_hint_fragment_item.h"

#include <boost/range/join.hpp>

#include "schrodinger/sketcher/molviewer/abstract_monomer_item.h"
#include "schrodinger/sketcher/molviewer/monomer_connector_item.h"
#include "schrodinger/sketcher/molviewer/scene_utils.h"

namespace schrodinger::sketcher
{

MonomerHintFragmentItem::MonomerHintFragmentItem(
    const RDKit::ROMol& fragment, const Fonts& fonts, const int atom_index_to_hide,
    QGraphicsItem* parent) :
    QGraphicsItemGroup(parent),
    m_frag(fragment),
    m_fonts(&fonts),
    m_atom_index_to_hide(atom_index_to_hide)
{
    createGraphicsItems();
}

void MonomerHintFragmentItem::createGraphicsItems()
{
    auto [all_items, atom_to_atom_item, bond_to_bond_item,
          bond_to_secondary_connection_item, s_group_to_s_group_item] =
        create_graphics_items_for_mol(
            &m_frag, *m_fonts);
    for (auto* item : all_items) {
        addToGroup(item);
    }
    for (auto& kv : atom_to_atom_item) {
        if (auto* monomer_item = dynamic_cast<AbstractMonomerItem*>(kv.second)) {
            monomer_item->setMonomerColors(Qt::GlobalColor::transparent,
                                   CURSOR_HINT_COLOR, CURSOR_HINT_COLOR);
        }
        // hide the monomer where this fragment connects to the existing
        // structure
        if (kv.first->getIdx() == m_atom_index_to_hide) {
            kv.second->setVisible(false);
        }
        m_atom_items.append(kv.second);
    }
    for (auto& kv : boost::range::join(bond_to_bond_item, bond_to_secondary_connection_item)) {
        if (auto* connector_item = qgraphicsitem_cast<MonomerConnectorItem*>(kv.second)) {
            // TODO: make a constant for the width
            connector_item->setConnectorStyle(CURSOR_HINT_COLOR, 3);
        }
        m_bond_items.append(kv.second);
    }
}

void MonomerHintFragmentItem::updateConformer(const RDKit::Conformer& conformer)
{
    m_frag.getConformer() = conformer;
    update_conf_for_mol_graphics_items(m_atom_items, m_bond_items,
                                       {}, m_frag);
}

}
