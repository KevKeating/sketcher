#include "schrodinger/sketcher/widget/sketcher_view_with_custom_paint.h"

#include <QPainter>
#include <QPaintEvent>
#include <QStyleOption>

namespace schrodinger
{
namespace sketcher
{

SketcherVeiwWithCustomPaint::SketcherVeiwWithCustomPaint(QWidget* parent) :
    SketcherView(parent)
{
}

SketcherVeiwWithCustomPaint::~SketcherVeiwWithCustomPaint() = default;

void SketcherVeiwWithCustomPaint::paintEvent(QPaintEvent*)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

} // namespace sketcher
} // namespace schrodinger
