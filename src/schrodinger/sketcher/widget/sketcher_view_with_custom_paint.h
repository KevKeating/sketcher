#pragma once

#include "schrodinger/sketcher/definitions.h"
#include "schrodinger/sketcher/widget/sketcher_view.h"


namespace schrodinger
{
namespace sketcher
{

class SKETCHER_API SketcherViewWithCustomPaint : public SketcherView
{
  public:
    SketcherViewWithCustomPaint(QWidget* parent = nullptr);
    ~SketcherViewWithCustomPaint();
    void paintEvent(QPaintEvent*) override;
};

} // namespace sketcher
} // namespace schrodinger
