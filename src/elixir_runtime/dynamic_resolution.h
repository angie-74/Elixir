// Copyright 2026 Elixir Project Authors
// Licensed under the Apache License, Version 2.0
#ifndef ELIXIR_RUNTIME_DYNAMIC_RESOLUTION_H_
#define ELIXIR_RUNTIME_DYNAMIC_RESOLUTION_H_

namespace elixir::runtime {

inline constexpr double kDynamicResolutionStep = 0.05;
inline constexpr double kDynamicResolutionFloor = 0.5;

class DynamicResolution final {
 public:
  DynamicResolution(double target_fps, double highest_scale);

  double scale() const { return scale_; }
  double lowest() const { return lowest_; }
  double highest() const { return highest_; }

  bool Update(double frames_per_second);

 private:
  double target_;
  double highest_;
  double lowest_;
  double scale_;
  int settling_ = 0;
  int below_ = 0;
  int at_target_ = 0;
  int wait_before_raising_ = 0;
  int next_wait_ = 15;
  bool raised_ = false;
  int raised_for_ = 0;
  double scale_before_raise_ = 0.0;
  bool lowered_ = false;
  int lowered_for_ = 0;
  double scale_before_lowering_ = 0.0;
  double rate_before_lowering_ = 0.0;
  int not_the_picture_ = 0;
};

}

#endif
