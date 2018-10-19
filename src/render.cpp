#include "render.hpp"
#include <iostream>
#include <cstdint>
#include <cassert>
#include <vector>
#include <stdlib.h>
#include <atomic>
#include <tbb/parallel_for.h>
#include <cmath>
#include <immintrin.h>

struct rgb8_t {
  std::uint8_t r;
  std::uint8_t g;
  std::uint8_t b;
};

rgb8_t heat_lut(float x)
{
  assert(0 <= x && x <= 1);
  constexpr float x0 = 1.f / 4.f;
  constexpr float x1 = 2.f / 4.f;
  constexpr float x2 = 3.f / 4.f;

  if (x == 0)
    return rgb8_t{0, 0, 255};
  else if (x < x0)
  {
    auto g = static_cast<std::uint8_t>(x / x0 * 255);
    return rgb8_t{0, g, 255};
  }
  else if (x < x1)
  {
    auto b = static_cast<std::uint8_t>((x1 - x) / x0 * 255);
    return rgb8_t{0, 255, b};
  }
  else if (x < x2)
  {
    auto r = static_cast<std::uint8_t>((x - x1) / x0 * 255);
    return rgb8_t{r, 255, 0};
  }
  else
  {
    auto b = static_cast<std::uint8_t>((1.f - x) / x0 * 255);
    return rgb8_t{255, b, 0};
  }
}


__m256 scalable_x(int off, int width)
{
    auto x = _mm256_set1_ps(0.0f);
    auto x0 = _mm256_set1_ps(0.0f);
    for (int i = 0; i < 8 && off + i < width; ++i)
        x[i] = off + i;
    x0 = x * 3.5f / (width - 1.f) - 2.5f;
    return x0;
}

static __m256 it_calculus(int n_iterations, int x, int width, float y0)
{
  auto x0 = scalable_x(x, width);
  auto v = _mm256_set1_ps(0.0f);
  auto w = _mm256_set1_ps(0.0f);
  auto mask = _mm256_set1_ps(1.0f);
  auto it = _mm256_set1_ps(0.0f);
  for (int i = 0; i < n_iterations; ++i)
  {
      it = _mm256_and_ps(mask, _mm256_set1_ps(1.0f)) + it;
      auto vtemp = v * v - w * w + x0;
      w = _mm256_set1_ps(2.0f) * v * w + y0;
      v = vtemp;
      mask = (v * v + w * w) < _mm256_set1_ps(4.0f);
      if (!_mm256_movemask_ps(mask))
          break;
  }
  return it;
}

static void add_colors(int x, int y, int width, std::vector<int>& iterations, int n_iterations, rgb8_t *str, rgb8_t *str_sym, int total, std::vector<float>& hues)
{
  int curr_it = iterations[y * width + x];
  if (curr_it == n_iterations)
  {
      *str = rgb8_t{0,0,0};
      *str_sym = rgb8_t{0,0,0};
  }
  else
  {
      rgb8_t heat = heat_lut(hues[curr_it] / (float)total);
      *str = heat;
      *str_sym = heat;
  }
}

template<typename T>
static std::vector<float> init_hues(int& n_iterations, std::vector<T>& histogram)
{
  std::vector<float> hues(n_iterations, 0);
  float hue = 0.0f;

  for (auto i = 0; i < n_iterations; ++i)
  {
      hue += histogram[i];
      hues[i] = hue;
  }

  return hues;
}

void render(std::byte* buffer,
            int width,
            int height,
            std::ptrdiff_t stride,
            int n_iterations)
{
  std::byte *buffer2 = buffer + (height - 1) * stride;
  std::vector<int> histogram(n_iterations + 4, 0);
  std::vector<int> iterations(width * height + 1, 0);

  for (auto y = 0; y < height / 2.f; ++y)
  {
    float y0 = y * 2.f / (height - 1.f) - 1.f;
    for (auto x = 0; x < width; x += 8)
    {
      auto it = it_calculus(n_iterations, x, width, y0);
      for (int i = 0; i < 8; ++i)
      {
          if (x + i < width)
          {
              iterations[y * width + x + i] = it[i];
              histogram[it[i]]++;
          }
      }
    }
  }

  int total = 0;
  for (auto i = 0; i < n_iterations; ++i)
      total += histogram[i];

  std::vector<float> hues = init_hues(n_iterations, histogram);

  for (auto y = 0; y < height / 2.f; ++y)
  {
      rgb8_t* str = reinterpret_cast<rgb8_t*>(buffer);
      rgb8_t* str_sym = reinterpret_cast<rgb8_t*>(buffer2);
      for (auto x = 0; x < width; ++x)
      {
          add_colors(x, y, width, iterations, n_iterations, str + x, str_sym + x, total, hues);
      }
      buffer += stride;
      buffer2 -= stride;
  }
}

void render_mt(std::byte* buffer,
               int width,
               int height,
               std::ptrdiff_t stride,
               int n_iterations)
{
  std::vector<std::atomic<int>> histogram(n_iterations);
  for (auto i = 0; i < n_iterations; ++i)
      histogram[i] = 0;

  std::vector<int> iterations(height * width);

  tbb::parallel_for(0, int(std::ceil(height / 2.f)), 1, [&](int y)
  {
    float y0 = y * 2.f / (height - 1.f) - 1.f;
    for (auto x = 0; x < width; x += 8)
    {
      auto it = it_calculus(n_iterations, x, width, y0);
      for (int i = 0; i < 8; ++i)
      {
          if (x + i < width)
          {
              iterations[y * width + x + i] = it[i];
              histogram[it[i]]++;
          }
      }
    }
  });

  std::atomic<int> total = 0;
  tbb::parallel_for (0, n_iterations, 1, [&](auto i) {total += histogram[i];});

  std::vector<float> hues = init_hues(n_iterations, histogram);

  tbb::parallel_for(0, int(std::ceil(height / 2.f)), 1, [&](auto y) {
      for (auto x = 0; x < width; ++x)
      {
          rgb8_t *str = reinterpret_cast<rgb8_t*>(buffer + y * stride + x * sizeof(rgb8_t));
          rgb8_t *str_sym = reinterpret_cast<rgb8_t*>(buffer + (height - 1) * stride + x * sizeof(rgb8_t) - y * stride);
          add_colors(x, y, width, iterations, n_iterations, str, str_sym, total, hues);
      }
  });
}
