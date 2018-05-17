#include "render.hpp"
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
  std::byte *buffer2 = buffer + (height - 1) * stride;
  std::vector<int> histogram(n_iterations + 1, 0);
  std::vector<int> iterations(width * height + 1, 0);


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
    }
  }

  int total = 0;
  for (auto i = 0; i < n_iterations; ++i)
      total += histogram[i];

  for (auto y = 0; y < height / 2.f; ++y)
  {
      rgb8_t* lineptr = reinterpret_cast<rgb8_t*>(buffer);
      rgb8_t* lineptr_sym = reinterpret_cast<rgb8_t*>(buffer2);
      for (auto x = 0; x < width; ++x)
      {
          int curr_it = iterations[y * width + x];
          if (curr_it == n_iterations)
          {
              lineptr[x] = rgb8_t{0,0,0};
              lineptr_sym[x] = rgb8_t{0,0,0};
          }
          else
          {
              float hue = 0;
              if ((curr_it + 1) % 2 == 0)
              {
                  for (auto i = 0; i <= curr_it; i += 2)
                  {
                    hue += histogram[i];
                    hue += histogram[i + 1];
                  }
              }
              else
              {
                  for (auto i = 0; i <= curr_it; ++i)
                    hue += histogram[i];
              }
              rgb8_t heat = heat_lut(hue / (float)total);
              lineptr[x] = heat;
              lineptr_sym[x] = heat;
          }
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

  std::vector<int> iterations(height*width);

  tbb::parallel_for(0, int(std::ceil(height / 2.f)), 1, [&](int y)
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

  tbb::parallel_for(0, int(std::ceil(height / 2.f)), 1, [&](auto y) {
      for (auto x = 0; x < width; ++x)
      {
          rgb8_t *str = reinterpret_cast<rgb8_t*>(buffer + y * stride + x * sizeof(rgb8_t));
          rgb8_t *str_sym = reinterpret_cast<rgb8_t*>(buffer + (height - 1) * stride + x * sizeof(rgb8_t) - y * stride);
          int curr_it = iterations[y * width + x];
          if (curr_it == n_iterations)
          {
              *str = rgb8_t{0,0,0};
              *str_sym = rgb8_t{0,0,0};
          }
          else
          {
              float hue = 0;
              if ((curr_it + 1) % 2 == 0)
              {
                  for (auto i = 0; i <= curr_it; i += 2)
                  {
                    hue += histogram[i];
                    hue += histogram[i + 1];
                  }
              }
              else
              {
                  for (auto i = 0; i <= curr_it; ++i)
                    hue += histogram[i];
              }
              rgb8_t heat = heat_lut(hue / (float)total);
              *str_sym = heat;
              *str = heat;
          }
      }
  });
}
