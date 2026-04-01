#pragma once

#include <string>
#include <tuple>
#include <utility>
#include <variant>

#include "schrodinger/sketcher/definitions.h"
#include "schrodinger/sketcher/model/sketcher_model.h"
#include "schrodinger/sketcher/molviewer/constants.h"
#include "schrodinger/sketcher/molviewer/fonts.h"
#include "schrodinger/sketcher/molviewer/monomer_constants.h"
#include "schrodinger/sketcher/tool/standard_scene_tool_base.h"
#include "schrodinger/sketcher/rdkit/monomeric.h"

namespace RDKit
{
class Atom;
class Bond;
} // namespace RDKit

namespace RDGeom
{
class Point3D;
} // namespace RDGeom

namespace schrodinger
{

namespace rdkit_extensions
{
enum class ChainType;
enum class Direction;
} // namespace rdkit_extensions

namespace sketcher
{

class MonomerHintFragmentItem;
class UnboundMonomericAttachmentPointItem;
class HintFragmentMonomerInfo;

using MonomerAndAPItems = std::pair<AbstractMonomerItem*, UnboundMonomericAttachmentPointItem*>;
using DragEndInfo = std::variant<std::monostate, rdkit_extensions::Direction, MonomerAndAPItems>;

/**
 * Return the default unbound attachment point; that is, the attachment point
 * that should be selected when the user hovers over a monomer.
 * @param hovered_type The type of monomer being hovered over
 * @param tool_type  The type of monomer that would be drawn by the active scene
 * tool
 * @param unbound_ap_items A list of all graphics items representing unbound
 * attachment points of the hovered monomer
 */
SKETCHER_API UnboundMonomericAttachmentPointItem* get_default_attachment_point(
    const MonomerType hovered_type, const MonomerType tool_type,
    const std::vector<UnboundMonomericAttachmentPointItem*>& unbound_ap_items);

/**
 * When adding a new monomer bound to an existing monomer, determine the
 * appropriate attach point to use for the new-monomer-end of the connection.
 */
SKETCHER_API std::string
get_attachment_point_for_new_monomer(const MonomerType existing_monomer_type,
                                     const std::string_view existing_monomer_ap,
                                     const MonomerType new_monomer_type);

/**
 * A scene tools that draws a monomer
 */
class SKETCHER_API DrawMonomerSceneTool : public StandardSceneToolBase
{
  public:
    DrawMonomerSceneTool(const std::string& res_name,
                         const rdkit_extensions::ChainType chain_type,
                         const Fonts& fonts, Scene* scene, MolModel* mol_model);
    virtual ~DrawMonomerSceneTool();

    // Reimplemented AbstractSceneTool methods
    std::vector<QGraphicsItem*> getGraphicsItems() override;
    void onMouseMove(QGraphicsSceneMouseEvent* const event) override;
    void onLeftButtonClick(QGraphicsSceneMouseEvent* const event) override;
    void onLeftButtonDragStart(QGraphicsSceneMouseEvent* const event) override;
    void onLeftButtonDragMove(QGraphicsSceneMouseEvent* const event) override;
    void
    onLeftButtonDragRelease(QGraphicsSceneMouseEvent* const event) override;
    void updateColorsAfterBackgroundColorChange(bool is_dark_mode) override;

  protected:
    std::string m_res_name;
    rdkit_extensions::ChainType m_chain_type;
    Fonts m_fonts;
    MonomerType m_monomer_type;
    QGraphicsItemGroup m_attachment_point_labels_group;
    const QGraphicsItem* m_hovered_item = nullptr;
    const UnboundMonomericAttachmentPointItem* m_hovered_ap_item = nullptr;
    std::vector<UnboundMonomericAttachmentPointItem*> m_unbound_ap_items;
    MonomerHintFragmentItem* m_hint_fragment_item = nullptr;
    std::shared_ptr<RDKit::RWMol> m_frag = nullptr;
    QColor m_monomer_background_color = LIGHT_BACKGROUND_COLOR;
    QColor m_unbound_ap_label_color = UNBOUND_AP_LABEL_COLOR;
    QColor m_bound_ap_label_color = BOUND_AP_LABEL_COLOR;
    bool m_cursor_hint_shown = true;
    
    bool m_drag_ignored;
    AbstractMonomerItem* m_drag_start_monomer_item;
    std::optional<UnboundAttachmentPoint> m_drag_start_ap;
    AbstractMonomerItem* m_drag_end_monomer_item;
    DragEndInfo m_drag_end_info;
    QGraphicsItemGroup m_drag_end_attachment_point_labels_group;
    std::vector<UnboundMonomericAttachmentPointItem*> m_drag_end_unbound_ap_items;
    

    QPixmap createDefaultCursorPixmap() const override;

    /**
     * Label all attachment points on the given monomer
     */
    void
    labelAttachmentPointsOnHoveredMonomer(const RDKit::Atom* const monomer,
                                   AbstractMonomerItem* const monomer_item);

    void
    labelAttachmentPointsOnDragEndMonomer(const RDKit::Atom* const monomer,
                                   AbstractMonomerItem* const monomer_item);

    void
    labelAttachmentPointsOnMonomer(const RDKit::Atom* const monomer,
                                   AbstractMonomerItem* const monomer_item, QGraphicsItemGroup& attachment_point_labels_group,
    std::vector<UnboundMonomericAttachmentPointItem*>& unbound_ap_items);

    /**
     * Label both attachment points for the given monomeric connector
     * @param connector The monomeric connector to label
     * @param is_secondary_connection Whether this method should label the
     * secondary connection of the bond instead of the primary connection.
     * Secondary connections occur when a single RDKit::Bond* represents
     * multiple connections, e.g. two neighboring cysteines that are disulfide
     * bonded to each other.
     */
    void labelAttachmentPointsOnConnector(const RDKit::Bond* const connector,
                                          const bool is_secondary_connection);

    /**
     * Label the specified attachment point
     * @param monomer The monomer containing the attachment point to label
     * @param bound_monomer The other monomer involved in the connection
     * @param is_secondary_connection Whether we should label the secondary
     * connection of the bond instead of the primary connection.
     * @param label The text to display in the attachment point label
     */
    void labelBoundAttachmentPoint(const RDKit::Atom* const monomer,
                                   const RDKit::Atom* const bound_monomer,
                                   const bool is_secondary_connection,
                                   const std::string& label);

    void addAttachmentPointLabel(const QString& label,
                                 const QRectF& label_rect);

    /**
     * Clear all attachment point labels drawn by this scene tool
     */
    void clearAttachmentPointsLabelsAndHintFragmentItem();
    void clearHintFragmentItem();
    void clearDragEndAttachmentPointsLabels();

    /**
     * Return the top graphics item representing a monomer at the given
     * coordinates. If there's no such graphics item, return nullptr.  Note that
     * this method will consider the cursor to be over a monomer if the cursor
     * is over an unbound attachment point belonging to that monomer, or if the
     * cursor *would be* over an unbound attachment point once it's drawn.
     * @param scene_pos The position in Scene coordinates
     */
    AbstractMonomerItem* getTopMonomerItemAt(const QPointF& scene_pos) const;

    /**
     * Clear any existing attachment point labels and draw new ones for the
     * specified monomeric graphics item. If item is nullptr, then all labels
     * will be cleared and no new ones will be drawn.
     */
    void drawAttachmentPointLabelsFor(QGraphicsItem* const item);

    /**
     * Return the unbound attachment point graphics item that should be "active"
     * (i.e. that we'd draw a connection for if the user clicked) for the given
     * coordinates. If the coordinates are over an unbound attachment point
     * item, that item will be returned. If the coordinates are over the
     * monomer, then the default unbound attachment point will be returned,
     * assuming one exists for the current tool. If no default attachment point
     * exists (e.g. if a click would mutate the monomer or if the tool and the
     * hovered monomer are of different molecule types), then nullptr will be
     * returned.
     *
     * @note This method only returns accurate results for the currently hovered
     * monomer, and assumes that the unbound attachment point graphics items
     * have already been drawn for this monomer.
     */
    UnboundMonomericAttachmentPointItem*
    getUnboundAttachmentPointAt(const QPointF& scene_pos, const bool no_default_if_click_should_mutate) const;

    UnboundMonomericAttachmentPointItem*
    getUnboundDragEndAttachmentPointAt(
    const QPointF& scene_pos) const;

    /**
     * @return the unbound attachment point that should be active when the user
     * is hovering over the monomer itself
     */
    UnboundMonomericAttachmentPointItem*
    getDefaultUnboundAttachmentPointForHoveredMonomer(const bool no_default_if_click_should_mutate) const;

    /**
     * Draw a hint structure showing a monomer bound to the specified attachment
     * point
     */
    void
    drawBoundMonomerHintFor(UnboundMonomericAttachmentPointItem* const ap_item);

    std::tuple<const RDKit::Atom*, MonomerType>
    getHoveredMonomerAndType() const;

    /**
     * @return whether this tool should show the predictive highlighting outline
     * for the currently hovered monomer.  The highlighting is shown if the
     * hovered monomer can be mutated (i.e. its the same monomer type as this
     * tool but a different residue name) or if there is a reasonable way to
     * connect this tool's monomer to the hovered monomer (an unreasonable
     * connection would be, e.g., connecting a nucleic acid base directly to a
     * phosphate with no intervening sugar.)
     */
    bool shouldShowPredictiveHighlighting() const;

    /**
     * Whether clicking on the specified monomer should mutate that monomer. We
     * only mutate monomers that are the same monomer type as this tool but have
     * a different residue name.
     */
    bool clickShouldMutate(const RDKit::Atom* monomer,
                           const MonomerType monomer_type) const;
    
    void createHintFragmentItem(const HintFragmentMonomerInfo& monomer_one, const HintFragmentMonomerInfo& monomer_two);
    
    bool createDragHint(const DragEndInfo& drag_end_info);
    HintFragmentMonomerInfo createHintFragmentMonomerInfoForHintFromEmptySpace(const QPointF& scene_pos) const;
    HintFragmentMonomerInfo createHintFragmentMonomerInfoForHintFromExistingMonomer(const AbstractMonomerItem* const monomer_item, const UnboundAttachmentPoint& ap) const;
    HintFragmentMonomerInfo createHintFragmentMonomerInfoForHintToExistingMonomer(const AbstractMonomerItem* const monomer_item,
    const UnboundMonomericAttachmentPointItem* const ap_item) const;
    HintFragmentMonomerInfo createHintFragmentMonomerInfoForHintToDirection(const HintFragmentMonomerInfo& start_monomer_info, const rdkit_extensions::Direction direction) const;
    std::pair<DragEndInfo, AbstractMonomerItem*> getDragEndInfo(const QPointF& scene_pos);

    rdkit_extensions::Direction getDragDirection(const QPointF& cur_scene_pos) const;
};

} // namespace sketcher
} // namespace schrodinger
