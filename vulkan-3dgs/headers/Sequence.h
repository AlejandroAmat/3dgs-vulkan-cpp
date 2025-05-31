#pragma once

#include "RenderSettings.h"
#include "glm/glm.hpp"
#include <chrono>

class Sequence {
public:
  Sequence(){};
  void setSequence(std::vector<CameraKeyframe> *seq) {
    _sequence = seq;
    _currTime = 0;
  }
  void Play(float deltaTime);

private:
  std::vector<CameraKeyframe> *_sequence;
  float _currTime = 0;
};
