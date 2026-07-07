#pragma once

#ifdef __EMSCRIPTEN__

#include <emscripten/val.h>

#include <QByteArray>

#include "schrodinger/sketcher/image_generation.h"

emscripten::val qbyte_array_to_uint8_array(QByteArray bytes);
schrodinger::sketcher::RenderOptions
render_options_from_js(const emscripten::val& options);

#endif
