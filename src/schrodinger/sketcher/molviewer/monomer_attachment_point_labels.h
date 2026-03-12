#pragma once

#include <string>
#include <tuple>
#include <utility>

#include "schrodinger/sketcher/definitions.h"
#include "schrodinger/sketcher/model/sketcher_model.h"
#include "schrodinger/sketcher/molviewer/fonts.h"
#include "schrodinger/sketcher/tool/standard_scene_tool_base.h"

class QPointF;
class QRectF;

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
} // namespace rdkit_extensions

namespace sketcher
{

/**
 * @return the attachment point name to a QString after converting apostrophes
 * to Unicode primes.
 */
SKETCHER_API QString prep_attachment_point_name(const std::string& name);

/**
 * Position the given rectangle to label a monomer's attachment point
 * @param ap_label_rect The rectangle to position. It should already be sized
 * correctly for the attachment point label.
 * @param monomer_coords The coordinates of the monomer being labeled
 * @param bound_coords The coordinates of the other monomer involved in the bond
 */
SKETCHER_API void position_ap_label_rect(QRectF& ap_label_rect,
                                         const QPointF& monomer_coords,
                                         const QPointF& bound_coords);

SKETCHER_API QGraphicsItem* create_label_for_bound_attachment_point(
    const RDKit::Atom* const monomer, const RDKit::Atom* const bound_monomer,
    const bool is_secondary_connection, const std::string& ap_name, const Fonts& fonts, const Scene* const scene);

SKETCHER_API std::vector<QGraphicsItem*>
create_attachment_point_labels_for_connector(const RDKit::Bond* const connector,
                                             const bool is_secondary_connection, const Fonts& fonts, const Scene* const scene);


} // namespace sketcher
} // namespace schrodinger
