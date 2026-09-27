#include <wr_internal.h>

int
wr_size_mul_overflow(size_t a, size_t b)
{
  return b != 0 && a > SIZE_MAX / b;
}

int
wr_reserve(void** data, size_t* capacity, size_t count, size_t element_size)
{
  size_t new_capacity;
  void* new_data;

  if (count <= *capacity)
    return 1;
  if (element_size == 0 || wr_size_mul_overflow(count, element_size))
    return 0;

  new_capacity = *capacity ? *capacity : 8;
  while (new_capacity < count) {
    if (new_capacity > SIZE_MAX / 2) {
      new_capacity = count;
      break;
    }
    new_capacity *= 2;
  }
  if (wr_size_mul_overflow(new_capacity, element_size))
    return 0;

  new_data = realloc(*data, new_capacity * element_size);
  if (!new_data)
    return 0;

  *data = new_data;
  *capacity = new_capacity;
  return 1;
}

int
wr_valid_rect(wr_rect rect)
{
  return isfinite(rect.x) && isfinite(rect.y) &&
         isfinite(rect.width) && isfinite(rect.height) &&
         rect.width > 0.0f && rect.height > 0.0f;
}

int
wr_valid_transform(wr_transform transform)
{
  return isfinite(transform.m11) && isfinite(transform.m12) &&
         isfinite(transform.m21) && isfinite(transform.m22) &&
         isfinite(transform.m31) && isfinite(transform.m32);
}

wr_transform
wr_identity_transform(void)
{
  wr_transform transform = { 1, 0, 0, 1, 0, 0 };
  return transform;
}

wr_transform
wr_multiply_transform(wr_transform a, wr_transform b)
{
  wr_transform result;
  result.m11 = a.m11 * b.m11 + a.m21 * b.m12;
  result.m12 = a.m12 * b.m11 + a.m22 * b.m12;
  result.m21 = a.m11 * b.m21 + a.m21 * b.m22;
  result.m22 = a.m12 * b.m21 + a.m22 * b.m22;
  result.m31 = a.m11 * b.m31 + a.m21 * b.m32 + a.m31;
  result.m32 = a.m12 * b.m31 + a.m22 * b.m32 + a.m32;
  return result;
}

wr_rect
wr_empty_rect(void)
{
  wr_rect rect = { 0, 0, 0, 0 };
  return rect;
}

float
wr_clamp01(float value)
{
  if (value < 0.0f)
    return 0.0f;
  if (value > 1.0f)
    return 1.0f;
  return value;
}

wr_color
wr_normalize_color(wr_color color)
{
  color.r = wr_clamp01(color.r);
  color.g = wr_clamp01(color.g);
  color.b = wr_clamp01(color.b);
  color.a = wr_clamp01(color.a);
  return color;
}

int
wr_colors_equal(wr_color a, wr_color b)
{
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

int
wr_rects_equal(wr_rect a, wr_rect b)
{
  return a.x == b.x && a.y == b.y &&
         a.width == b.width && a.height == b.height;
}

int
wr_intersects(wr_rect a, wr_rect b)
{
  return a.x < b.x + b.width && a.x + a.width > b.x &&
         a.y < b.y + b.height && a.y + a.height > b.y;
}

wr_rect
wr_intersect_rect(wr_rect a, wr_rect b)
{
  float left = a.x > b.x ? a.x : b.x;
  float top = a.y > b.y ? a.y : b.y;
  float right = a.x + a.width < b.x + b.width ?
                a.x + a.width : b.x + b.width;
  float bottom = a.y + a.height < b.y + b.height ?
                 a.y + a.height : b.y + b.height;
  wr_rect result = { left, top, right - left, bottom - top };
  return result;
}

wr_rect
wr_transform_bounds(wr_rect rect, wr_transform transform)
{
  float x1 = rect.x * transform.m11 + rect.y * transform.m21 + transform.m31;
  float y1 = rect.x * transform.m12 + rect.y * transform.m22 + transform.m32;
  float x2 = (rect.x + rect.width) * transform.m11 +
             rect.y * transform.m21 + transform.m31;
  float y2 = (rect.x + rect.width) * transform.m12 +
             rect.y * transform.m22 + transform.m32;
  float x3 = rect.x * transform.m11 +
             (rect.y + rect.height) * transform.m21 + transform.m31;
  float y3 = rect.x * transform.m12 +
             (rect.y + rect.height) * transform.m22 + transform.m32;
  float x4 = (rect.x + rect.width) * transform.m11 +
             (rect.y + rect.height) * transform.m21 + transform.m31;
  float y4 = (rect.x + rect.width) * transform.m12 +
             (rect.y + rect.height) * transform.m22 + transform.m32;
  float left = fminf(fminf(x1, x2), fminf(x3, x4));
  float right = fmaxf(fmaxf(x1, x2), fmaxf(x3, x4));
  float top = fminf(fminf(y1, y2), fminf(y3, y4));
  float bottom = fmaxf(fmaxf(y1, y2), fmaxf(y3, y4));
  wr_rect result = { left, top, right - left, bottom - top };
  return result;
}

