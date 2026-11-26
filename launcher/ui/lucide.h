// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_UI_LUCIDE_H_
#define ELIXIR_LAUNCHER_UI_LUCIDE_H_

#include <d2d1.h>

#include <string_view>

namespace elixir::launcher::ui::lucide {

struct Icon {
  std::string_view name;
  std::string_view svg;
};
Icon IconFor(wchar_t glyph);

bool BuildGeometry(ID2D1Factory* factory, std::string_view svg, ID2D1PathGeometry** geometry);

}

#endif
