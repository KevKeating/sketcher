#pragma once

#include "schrodinger/sketcher/definitions.h"
#include "schrodinger/sketcher/widget/sketcher_view.h"


namespace schrodinger
{
namespace sketcher
{

class SKETCHER_API SketcherViewWithWhiteBackground : public SketcherView
{
  public:
    SketcherViewWithWhiteBackground(QWidget* parent = nullptr);
    ~SketcherViewWithWhiteBackground();
    void paintEvent(QPaintEvent*) override;
};

} // namespace sketcher
} // namespace schrodinger
