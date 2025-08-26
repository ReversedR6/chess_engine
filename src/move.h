#pragma once
#include <cstdint>
struct Move {
  uint16_t from{0};
  uint16_t to{0};
  uint8_t promo{0};
  uint8_t flags{0};
};