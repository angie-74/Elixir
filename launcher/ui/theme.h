// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_LAUNCHER_UI_THEME_H_
#define ELIXIR_LAUNCHER_UI_THEME_H_

#include <d2d1.h>

#include <cstdint>

namespace elixir::launcher::ui::theme {

constexpr D2D1_COLOR_F Rgb(std::uint32_t rgb, float alpha = 1.0f) {
  return D2D1_COLOR_F{static_cast<float>((rgb >> 16) & 0xFF) / 255.0f, static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
                      static_cast<float>(rgb & 0xFF) / 255.0f, alpha};
}
constexpr D2D1_COLOR_F White(float alpha) { return D2D1_COLOR_F{1.0f, 1.0f, 1.0f, alpha}; }

inline constexpr std::uint32_t kWindowRgb = 0x202020;
inline constexpr D2D1_COLOR_F kWindow = Rgb(kWindowRgb);
inline constexpr D2D1_COLOR_F kMicaTint = Rgb(0x121212, 0.35f);
inline constexpr D2D1_COLOR_F kLayer = White(0.035f);
inline constexpr D2D1_COLOR_F kContentLayer = Rgb(0x181818);
inline constexpr D2D1_COLOR_F kLayerBorder = White(0.065f);
inline constexpr D2D1_COLOR_F kCard = White(0.045f);
inline constexpr D2D1_COLOR_F kCardBorder = White(0.075f);
inline constexpr D2D1_COLOR_F kDivider = White(0.08f);
inline constexpr D2D1_COLOR_F kSubtleFill = White(0.06f);
inline constexpr D2D1_COLOR_F kSubtleHover = White(0.09f);
inline constexpr D2D1_COLOR_F kSubtlePressed = White(0.045f);
inline constexpr D2D1_COLOR_F kControlBorder = White(0.09f);
inline constexpr D2D1_COLOR_F kNavHover = White(0.045f);
inline constexpr D2D1_COLOR_F kNavSelected = White(0.075f);
inline constexpr std::uint32_t kFieldRgb = 0x2D2D2D;
inline constexpr std::uint32_t kViewerRgb = 0x161616;
inline constexpr D2D1_COLOR_F kField = Rgb(kFieldRgb);
inline constexpr D2D1_COLOR_F kViewer = Rgb(kViewerRgb);
inline constexpr D2D1_COLOR_F kFieldBorder = White(0.10f);
inline constexpr D2D1_COLOR_F kFieldUnderline = White(0.38f);
inline constexpr D2D1_COLOR_F kOverlayDim = Rgb(0x000000, 0.58f);
inline constexpr D2D1_COLOR_F kDialog = Rgb(0x202020);
inline constexpr D2D1_COLOR_F kDialogFooter = Rgb(0x1A1A1A);
inline constexpr D2D1_COLOR_F kToast = Rgb(0x2A2A2A);

inline constexpr std::uint32_t kTextRgb = 0xF2F2F2;
inline constexpr D2D1_COLOR_F kText = Rgb(kTextRgb);
inline constexpr D2D1_COLOR_F kTextSecondary = Rgb(0xB8B8B8);
inline constexpr D2D1_COLOR_F kTextTertiary = Rgb(0x9A9A9A);
inline constexpr D2D1_COLOR_F kTextDisabled = Rgb(0x666666);

inline constexpr D2D1_COLOR_F kTextOnSelection = Rgb(0x111111);
inline constexpr D2D1_COLOR_F kSelectionFill = Rgb(0xEDEDED);
inline constexpr D2D1_COLOR_F kSelectionHover = Rgb(0xFFFFFF);
inline constexpr D2D1_COLOR_F kSelectionDisabled = White(0.20f);
inline constexpr D2D1_COLOR_F kFocusRing = Rgb(0xFFFFFF);
inline constexpr float kSliderThumbRadius = 8.0f;
inline constexpr D2D1_COLOR_F kSliderTrack = White(15.0f / 255.0f);
inline constexpr D2D1_COLOR_F kSliderTrackHover = White(21.0f / 255.0f);
inline constexpr D2D1_COLOR_F kSliderTrackBorder = White(18.0f / 255.0f);
inline constexpr D2D1_COLOR_F kSliderTrackEdge = White(6.0f / 237.0f);
inline constexpr D2D1_COLOR_F kValueBox = Rgb(0x1C1C1C);
inline constexpr D2D1_COLOR_F kLogText = Rgb(0xDCDCDC);
inline constexpr D2D1_COLOR_F kHeroTop = Rgb(0x2A2A2A);
inline constexpr D2D1_COLOR_F kHeroBottom = Rgb(0x161616);

inline constexpr D2D1_COLOR_F kPass = Rgb(0x63D49B);
inline constexpr D2D1_COLOR_F kWarn = Rgb(0xF2C35B);
inline constexpr D2D1_COLOR_F kFail = Rgb(0xFF6B81);
inline constexpr D2D1_COLOR_F kPending = Rgb(0xA0A0A0);
inline constexpr D2D1_COLOR_F kFailFill = Rgb(0xFF6B81, 0.10f);
inline constexpr D2D1_COLOR_F kWarnFill = Rgb(0xF2C35B, 0.10f);
inline constexpr D2D1_COLOR_F kPassFill = Rgb(0x63D49B, 0.10f);
inline constexpr D2D1_COLOR_F kInfoFill = White(0.07f);

inline constexpr D2D1_COLOR_F kInfoBorder = White(0.28f);

inline constexpr float kHeaderHeight = 64.0f;
inline constexpr float kSidebarWidth = 236.0f;
inline constexpr float kNavItemHeight = 40.0f;
inline constexpr float kPagePaddingX = 36.0f;
inline constexpr float kPagePaddingTop = 28.0f;
inline constexpr float kContentMaxWidth = 980.0f;
inline constexpr float kCardRadius = 8.0f;
inline constexpr float kCardPadding = 20.0f;
inline constexpr float kControlRadius = 5.0f;
inline constexpr float kButtonHeight = 34.0f;
inline constexpr float kRowGap = 12.0f;
inline constexpr float kOptionGap = 4.0f;
inline constexpr float kSearchHeight = 34.0f;
inline constexpr float kBottomBarHeight = 64.0f;

inline constexpr float kPageTransitionMs = 300.0f;
inline constexpr float kPageSlide = 30.0f;
inline constexpr float kPopupMs = 167.0f;
inline constexpr float kPopupRise = 12.0f;
inline constexpr float kListMs = 200.0f;
inline constexpr float kMenuSlide = 8.0f;
inline constexpr float kNavSlideMs = 220.0f;
inline constexpr float kImageFadeMs = 200.0f;
inline constexpr float kProgressMs = 240.0f;

inline constexpr float kWindowWidth = 1180.0f;
inline constexpr float kWindowHeight = 760.0f;
inline constexpr float kMinWindowWidth = 900.0f;
inline constexpr float kMinWindowHeight = 600.0f;

}

#endif
