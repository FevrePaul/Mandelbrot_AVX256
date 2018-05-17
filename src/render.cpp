#include "render.hpp"
#include <iostream>
#include <cstdint>
#include <cassert>
#include <vector>
#include <stdlib.h>
#include <atomic>
#include <tbb/parallel_for.h>
#include <cmath>

struct rgb8_t {
  std::uint8_t r;
  std::uint8_t g;
  std::uint8_t b;
};

rgb8_t heat_lut(float x)
{
  assert(0 <= x && x <= 1);
  float x0 = 1.f / 4.f;
  float x1 = 2.f / 4.f;
  float x2 = 3.f / 4.f;

  if (x == 1)
      return rgb8_t{0, 0, 0};
  else if (x == 0)
    return rgb8_t{0, 0, 0};
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
  else if (x < 1)
  {
    auto b = static_cast<std::uint8_t>((1.f - x) / x0 * 255);
    return rgb8_t{255, b, 0};
  }
}

void render(std::byte* buffer,
            int width,
            int height,
            std::ptrdiff_t stride,
            int n_iterations)
{
  std::vector<int> histogram(n_iterations + 1, 0);
  std::vector<int> iterations(width * height + 1, 0);

  auto size = height * width;
  auto total = 0;

  for (auto y = 0; y < height / 2.f; ++y)
  {
    float y0 = y * 2.f / (height - 1.f) - 1.f;
    for (auto x = 0; x < width; ++x)
    {
      float x0 = x * 3.5f / (width - 1.f) - 2.5f;
      float v = 0;
      float w = 0;
      int it = n_iterations;
      do
      {
          float vtemp = v * v - w * w + x0;
          w = 2 * v * w + y0;
          v = vtemp;
      } while (--it && v * v + w * w < 4);
      int curr_iter = n_iterations - it;
      iterations[y * width + x] = curr_iter;
      histogram[curr_iter]++;
      total++;
    }
  }

  total -= histogram[n_iterations];

  std::vector<float> hues(n_iterations, 0);
  float hue = 0.0;
  for (int i = 0; i < n_iterations; ++i)
  {
      hue += histogram[i];
      hues[i] = hue;
  }

  rgb8_t *lineptr = reinterpret_cast<rgb8_t*>(buffer);

  for (int i = 0; i < width * (height / 2.f); ++i)
  {
      auto color = heat_lut(hues[iterations[i]] / (float)total);
      lineptr[i] = color;
      auto curr_x = int(i / width);
      lineptr[size - (width * (curr_x + 1)) + (i - (curr_x * width))] = color;
  }
}


void render_mt(std::byte* buffer,
               int width,
               int height,
               std::ptrdiff_t stride,
               int n_iterations)
{
  std::vector<std::atomic_int> histogram(n_iterations);
  tbb::parallel_for(0, n_iterations, 1, [&](auto i){histogram[i] = 0;});

  auto size = height * width;
  std::vector<int> iterations(size);

  tbb::parallel_for(0, int(height / 2.f), 1, [&](int y)
  {
    float y0 = y * 2.f / (height - 1.f) - 1.f;
    for (auto x = 0; x < width; ++x)
    {
      float x0 = x * 3.5f / (width - 1.f) - 2.5f;
      float v = 0;
      float w = 0;
      int it = n_iterations;
      do
      {
          float vtemp = v * v - w * w + x0;
          w = 2 * v * w + y0;
          v = vtemp;
      } while (--it && v * v + w * w < 4);
      auto nb_it = n_iterations - it;
      iterations[y * width + x] = nb_it;
      histogram[nb_it]++;
    }
  });

  std::atomic<int> total = 0;
  tbb::parallel_for (0, n_iterations, 1, [&](auto i) {total += histogram[i];});

  std::vector<float> hues(n_iterations, 0);
  float hue = 0.0;
  for(int i = 0; i < n_iterations; ++i){
      hue = hue + histogram[i];
      hues[i] = hue;
  };

  rgb8_t *lineptr = reinterpret_cast<rgb8_t*>(buffer);

  tbb::parallel_for(0, int(size / 2.f), 1, [&](auto i){
      auto color = heat_lut(hues[iterations[i]] / (float)total);
      lineptr[i] = color;
      auto curr_x = int(i / width);
      lineptr[size - (width * (curr_x + 1)) + (i - (curr_x * width))] = color;
  });
}
