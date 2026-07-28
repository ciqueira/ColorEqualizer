// =============================================================================
// CudaKernel.cu
// -----------------------------------------------------------------------------
// CUDA GPU kernel for MCColorEqualizer.
// Contains all color math inline (RGB Spherical, OKLCH + Equalizers).
// Used on Windows and Linux only (__APPLE__ excluded at build time).
// =============================================================================

#include "EQParams.h"
#include <cstring>

// ─── Constants ────────────────────────────────────────────────────────────
#define PI 3.141592653589f
#define EPSILON 1e-10f
#define OKLAB_NEUTRAL_EPSILON 2e-4f
#define EQ_NODES 10

// ─── float3 helpers (CUDA float3 has no operator overloads) ───────────────

__device__ inline float3 f3_make(float x, float y, float z) {
  return make_float3(x, y, z);
}
__device__ inline float3 f3_add(float3 a, float3 b) {
  return make_float3(a.x + b.x, a.y + b.y, a.z + b.z);
}
__device__ inline float3 f3_mul_comp(float3 a, float3 b) {
  return make_float3(a.x * b.x, a.y * b.y, a.z * b.z);
}
__device__ inline float3 f3_scale(float3 a, float s) {
  return make_float3(a.x * s, a.y * s, a.z * s);
}
__device__ inline float3 f3_add_scalar(float3 a, float s) {
  return make_float3(a.x + s, a.y + s, a.z + s);
}
__device__ inline float f3_dot(float3 a, float3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
__device__ inline float clampf(float x, float lo, float hi) {
  return fminf(fmaxf(x, lo), hi);
}
__device__ inline float max3f(float a, float b, float c) {
  return fmaxf(a, fmaxf(b, c));
}
__device__ inline float cu_wrap_unit(float value) {
  value = fmodf(value, 1.f);
  return value < 0.f ? value + 1.f : value;
}
__device__ inline float cu_circular_delta(float from, float to) {
  return fmodf(to - from + 1.5f, 1.f) - 0.5f;
}
__device__ inline float cu_smoothstep(float edge0, float edge1, float value) {
  float t = clampf((value - edge0) / (edge1 - edge0), 0.f, 1.f);
  return t * t * (3.f - 2.f * t);
}

// ─── Color Matrices ──────────────────────────────────────────────────────

__device__ float3 cu_rgb_to_xyz(float3 rgb, int cs) {
  float x = rgb.x, y = rgb.y, z = rgb.z;
  if (cs == 0) {
    x = rgb.x * 0.66245418f + rgb.y * 0.13400421f + rgb.z * 0.15618766f;
    y = rgb.x * 0.27222872f + rgb.y * 0.67408177f + rgb.z * 0.05368952f;
    z = rgb.x * -0.00557465f + rgb.y * 0.00406073f + rgb.z * 1.01033910f;
  } else if (cs == 1) {
    x = rgb.x * 0.70062239f + rgb.y * 0.14877482f + rgb.z * 0.10105872f;
    y = rgb.x * 0.27411851f + rgb.y * 0.87363190f + rgb.z * -0.14775041f;
    z = rgb.x * -0.09896291f + rgb.y * -0.13789533f + rgb.z * 1.32591599f;
  } else if (cs == 2) {
    x = rgb.x * 0.63800764f + rgb.y * 0.21470386f + rgb.z * 0.09774445f;
    y = rgb.x * 0.29195377f + rgb.y * 0.82384104f + rgb.z * -0.11579482f;
    z = rgb.x * 0.00279827f + rgb.y * -0.06703423f + rgb.z * 1.15329373f;
  } else if (cs == 3) {
    x = rgb.x * 0.70485832f + rgb.y * 0.12976030f + rgb.z * 0.11583731f;
    y = rgb.x * 0.25452418f + rgb.y * 0.78147773f + rgb.z * -0.03600191f;
    z = rgb.x * 0.00000000f + rgb.y * 0.00000000f + rgb.z * 1.08905775f;
  }
  return f3_make(x, y, z);
}

__device__ float3 cu_xyz_to_rgb(float3 xyz, int cs) {
  float r = xyz.x, g = xyz.y, b = xyz.z;
  if (cs == 0) {
    r = xyz.x * 1.64102338f + xyz.y * -0.32480329f + xyz.z * -0.23642470f;
    g = xyz.x * -0.66366286f + xyz.y * 1.61533159f + xyz.z * 0.01675635f;
    b = xyz.x * 0.01172189f + xyz.y * -0.00828444f + xyz.z * 0.98839486f;
  } else if (cs == 1) {
    r = xyz.x * 1.51667204f + xyz.y * -0.28147805f + xyz.z * -0.14696363f;
    g = xyz.x * -0.46491710f + xyz.y * 1.25142378f + xyz.z * 0.17488461f;
    b = xyz.x * 0.06484905f + xyz.y * 0.10913934f + xyz.z * 0.76141462f;
  } else if (cs == 2) {
    r = xyz.x * 1.78906548f + xyz.y * -0.48253384f + xyz.z * -0.20007578f;
    g = xyz.x * -0.63984859f + xyz.y * 1.39639986f + xyz.z * 0.19443229f;
    b = xyz.x * -0.04153153f + xyz.y * 0.08233536f + xyz.z * 0.87886840f;
  } else if (cs == 3) {
    r = xyz.x * 1.50921547f + xyz.y * -0.25059735f + xyz.z * -0.16881148f;
    g = xyz.x * -0.49154545f + xyz.y * 1.36124555f + xyz.z * 0.09728294f;
    b = xyz.x * 0.00000000f + xyz.y * 0.00000000f + xyz.z * 0.91822495f;
  }
  return f3_make(r, g, b);
}

__device__ float3 cu_adapt_xyz_d60_to_d65(float3 xyz) {
  return f3_make(
      xyz.x * 0.987224008703f + xyz.y * -0.006113228607f +
          xyz.z * 0.015953288336f,
      xyz.x * -0.007598371812f + xyz.y * 1.001861484740f +
          xyz.z * 0.005330035791f,
      xyz.x * 0.003072577059f + xyz.y * -0.005095961511f +
          xyz.z * 1.081680603066f);
}

__device__ float3 cu_adapt_xyz_d65_to_d60(float3 xyz) {
  return f3_make(
      xyz.x * 1.013034914650f + xyz.y * 0.006105257823f +
          xyz.z * -0.014970943627f,
      xyz.x * 0.007698230125f + xyz.y * 0.998163352118f +
          xyz.z * -0.005032038535f,
      xyz.x * -0.002841317432f + xyz.y * 0.004685156723f +
          xyz.z * 0.924506137458f);
}

__device__ float cu_decode_transfer(float v, int cs) {
  if (cs == 0)
    return v <= 0.155251141553f
               ? (v - 0.072905534196f) / 10.540237741655f
               : exp2f(v * 17.52f - 9.72f);
  if (cs == 1)
    return v <= 0.02740668f ? v / 10.44426855f
                            : exp2f(v / 0.07329248f - 7.f) - 0.0075f;
  if (cs == 2)
    return v > 0.149658f
               ? (powf(10.f, (v - 0.385537f) / 0.247190f) - 0.052272f)
                     / 5.555556f
               : (v - 0.092809f) / 5.367655f;
  if (cs == 3)
    return v < 0.f
               ? v * 0.113597208611f - 0.018056996120f
               : (exp2f(14.f * (v - 0.092864125122f) / 0.907135874878f + 6.f)
                  - 64.f) / 2231.82630907f;
  return v;
}

__device__ float cu_encode_transfer(float v, int cs) {
  if (cs == 0)
    return v <= 0.0078125f
               ? v * 10.540237741655f + 0.072905534196f
               : (log2f(v) + 9.72f) / 17.52f;
  if (cs == 1)
    return v <= 0.00262409f ? v * 10.44426855f
                            : (log2f(v + 0.0075f) + 7.f) * 0.07329248f;
  if (cs == 2)
    return v > 0.010591f
               ? 0.247190f * log10f(5.555556f * v + 0.052272f) + 0.385537f
               : 5.367655f * v + 0.092809f;
  if (cs == 3)
    return v < -0.018056996120f
               ? (v + 0.018056996120f) / 0.113597208611f
               : (log2f(2231.82630907f * v + 64.f) - 6.f)
                     * 0.907135874878f / 14.f + 0.092864125122f;
  return v;
}

__device__ float3 cu_decode_transfer(float3 rgb, int cs) {
  return f3_make(cu_decode_transfer(rgb.x, cs), cu_decode_transfer(rgb.y, cs),
                 cu_decode_transfer(rgb.z, cs));
}

__device__ float3 cu_encode_transfer(float3 rgb, int cs) {
  return f3_make(cu_encode_transfer(rgb.x, cs), cu_encode_transfer(rgb.y, cs),
                 cu_encode_transfer(rgb.z, cs));
}

// ─── Oklab ────────────────────────────────────────────────────────────────

__device__ float3 cu_mv33(const float *m, float3 v) {
  return f3_make(m[0] * v.x + m[1] * v.y + m[2] * v.z,
                 m[3] * v.x + m[4] * v.y + m[5] * v.z,
                 m[6] * v.x + m[7] * v.y + m[8] * v.z);
}

__device__ static const float cu_XYZ_to_LMS[9] = {
    0.8189330101f, 0.3618667424f, -0.1288597137f, 0.0329845436f, 0.9293118715f,
    0.0361456387f, 0.0482003018f, 0.2643662691f,  0.6338517070f};
__device__ static const float cu_LMS_to_Oklab[9] = {
    0.2104542553f, 0.7936177850f, -0.0040720468f, 1.9779984951f, -2.4285922050f,
    0.4505937099f, 0.0259040371f, 0.7827717662f,  -0.8086757660f};
__device__ static const float cu_Oklab_to_LMS[9] = {
    1.0f, 0.3963377774f,  0.2158037573f, 1.0f, -0.1055613458f, -0.0638541728f,
    1.0f, -0.0894841775f, -1.2914855480f};
__device__ static const float cu_LMS_to_XYZ[9] = {
    1.2270138511f,  -0.5577999807f, 0.2812561490f,
    -0.0405801784f, 1.1122568696f,  -0.0716766787f,
    -0.0763812845f, -0.4214819784f, 1.5861632239f};

__device__ float3 cu_XYZ_to_Oklab(float3 xyz) {
  float3 lms = cu_mv33(cu_XYZ_to_LMS, xyz);
  float lx = lms.x < 0.f ? -powf(-lms.x, 1.f / 3.f) : powf(lms.x, 1.f / 3.f);
  float ly = lms.y < 0.f ? -powf(-lms.y, 1.f / 3.f) : powf(lms.y, 1.f / 3.f);
  float lz = lms.z < 0.f ? -powf(-lms.z, 1.f / 3.f) : powf(lms.z, 1.f / 3.f);
  return cu_mv33(cu_LMS_to_Oklab, f3_make(lx, ly, lz));
}

__device__ float cu_oklch_neutral_weight(float3 lch) {
  const float scale = fmaxf(1.f, fabsf(lch.z));
  const float inner = OKLAB_NEUTRAL_EPSILON * scale;
  return cu_smoothstep(inner, inner * 3.f, lch.y);
}

__device__ float3 cu_Oklab_to_XYZ(float3 lab) {
  float3 lp = cu_mv33(cu_Oklab_to_LMS, lab);
  return cu_mv33(cu_LMS_to_XYZ, f3_make(lp.x * lp.x * lp.x, lp.y * lp.y * lp.y,
                                        lp.z * lp.z * lp.z));
}

__device__ float3 cu_OKLAB_to_OKLCH(float3 lab) {
  float C = hypotf(lab.y, lab.z);
  float h = atan2f(lab.z, lab.y);
  if (h < 0.f)
    h += 2.f * PI;
  return f3_make(h / (2.f * PI), C, lab.x);
}

__device__ float3 cu_OKLCH_to_OKLAB(float3 lch) {
  float h = lch.x * 2.f * PI;
  return f3_make(lch.z, lch.y * cosf(h), lch.y * sinf(h));
}

// ─── RGB Spherical ───────────────────────────────────────────────────────

__device__ float3 cu_RGB_to_Spherical(float3 rgb) {
  float r = rgb.x, g = rgb.y, b = rgb.z;
  float is2 = 1.f / sqrtf(2.f), is3 = 1.f / sqrtf(3.f), is6 = 1.f / sqrtf(6.f);
  float u = (2.f * r - g - b) * is6;
  float v = (g - b) * is2;
  float w = (r + g + b) * is3;
  float rho = sqrtf(u * u + v * v + w * w);
  if (rho < EPSILON)
    return f3_make(0, 0, 0);
  float theta = atan2f(v, u);
  if (theta < 0.f)
    theta += 2.f * PI;
  float phi = atan2f(hypotf(u, v), w);
  return f3_make(theta / (2.f * PI), phi, rho);
}

__device__ float3 cu_Spherical_to_RGB(float3 c) {
  float t = c.x * 2.f * PI, p = c.y, rho = c.z;
  if (rho < EPSILON)
    return f3_make(0, 0, 0);
  float u = rho * sinf(p) * cosf(t);
  float v = rho * sinf(p) * sinf(t);
  float w = rho * cosf(p);
  float is2 = 1.f / sqrtf(2.f), is3 = 1.f / sqrtf(3.f), is6 = 1.f / sqrtf(6.f);
  return f3_make(2.f * u * is6 + w * is3,
                 -u * is6 + v * is2 + w * is3,
                 -u * is6 - v * is2 + w * is3);
}

__device__ float cu_rgb_opponent_hue(float3 rgb, float fallback_hue) {
  float u = (2.f * rgb.x - rgb.y - rgb.z) / sqrtf(6.f);
  float v = (rgb.y - rgb.z) / sqrtf(2.f);
  if (hypotf(u, v) < 1e-7f) return fallback_hue;
  return cu_wrap_unit(atan2f(v, u) / (2.f * PI));
}

__device__ float cu_stable_blue_selector_hue(float3 rgb, float model_hue,
                                             int input_cs) {
  if (input_cs == 0) return model_hue;

  float opponent_hue = cu_rgb_opponent_hue(rgb, model_hue);
  const float blue_opponent_hue = 2.f / 3.f;
  const float blue_eq_center = 257.f / 360.f;
  const float mask_inner = 25.f / 360.f;
  const float mask_outer = 75.f / 360.f;
  float distance =
      fabsf(cu_circular_delta(blue_opponent_hue, opponent_hue));
  float mask = 1.f - cu_smoothstep(mask_inner, mask_outer, distance);
  float stable_hue =
      cu_wrap_unit(opponent_hue + blue_eq_center - blue_opponent_hue);
  return cu_wrap_unit(
      model_hue + cu_circular_delta(model_hue, stable_hue) * mask);
}

// ─── convert_colorSpace_model ─────────────────────────────────────────────

__device__ float3 cu_convert(float3 rgb, int st, bool dir, int cs) {
  if (dir) {
    if (st >= 11) {
      const float3 linear_rgb = cu_decode_transfer(rgb, cs);
      float3 xyz = cu_rgb_to_xyz(linear_rgb, cs);
      if (cs == 0) xyz = cu_adapt_xyz_d60_to_d65(xyz);
      return cu_OKLAB_to_OKLCH(cu_XYZ_to_Oklab(xyz));
    }
    if (st == 8) return cu_RGB_to_Spherical(rgb);
    return rgb;
  }

  if (st >= 11) {
    float3 xyz = cu_Oklab_to_XYZ(cu_OKLCH_to_OKLAB(rgb));
    if (cs == 0) xyz = cu_adapt_xyz_d65_to_d60(xyz);
    return cu_encode_transfer(cu_xyz_to_rgb(xyz, cs), cs);
  }
  if (st == 8) return cu_Spherical_to_RGB(rgb);
  return rgb;
}

// ─── LUT Sampler (Bilinear 1D) ────────────────────────────────────────────

struct CudaLut {
  float values[256 * 3];
};

__device__ float3 cu_sample_lut_1d(const CudaLut &lut, int num_points,
                                   float normalized_x) {
    float x = normalized_x * (float)num_points - 0.5f;
    if (x < 0.0f) x += (float)num_points;
    if (x >= (float)num_points) x -= (float)num_points;
    
    int i0 = (int)floorf(x);
    int i1 = i0 + 1;
    float t = x - (float)i0;
    
    if (i0 < 0) i0 += num_points;
    if (i1 >= num_points) i1 -= num_points;
    
    int idx0 = i0 * 3;
    int idx1 = i1 * 3;
    
    return f3_make(
        lut.values[idx0 + 0] * (1.0f - t) + lut.values[idx1 + 0] * t,
        lut.values[idx0 + 1] * (1.0f - t) + lut.values[idx1 + 1] * t,
        lut.values[idx0 + 2] * (1.0f - t) + lut.values[idx1 + 2] * t
    );
}

// ─── RGB Direct equalizer ─────────────────────────────────────────────────

__device__ float cu_rgb_direct_sat_value(float3 rgb) {
  float neutral = (rgb.x + rgb.y + rgb.z) / 3.0f;
  float cx = rgb.x - neutral;
  float cy = rgb.y - neutral;
  float cz = rgb.z - neutral;
  float chroma_mag = sqrtf(cx * cx + cy * cy + cz * cz);
  return clampf(chroma_mag / (fabsf(neutral) + chroma_mag + 1.0e-6f),
                0.0f, 1.0f);
}

__device__ float3 cu_apply_rgb_direct_equalizer(float3 rgb,
                                                const CudaLut &lut,
                                                int lut_size) {
  float hue_normalized = cu_rgb_opponent_hue(rgb, 0.0f);
  float3 eq = cu_sample_lut_1d(lut, lut_size, hue_normalized);
  float h_delta = eq.x;
  float s_gain = eq.y;
  float l_delta = eq.z;

  if (fabsf(h_delta) < 1e-6f &&
      fabsf(s_gain - 1.0f) < 1e-6f &&
      fabsf(l_delta) < 1e-6f) {
    return rgb;
  }

  float neutral = (rgb.x + rgb.y + rgb.z) / 3.0f;
  float u = (2.0f * rgb.x - rgb.y - rgb.z) / sqrtf(6.0f);
  float v = (rgb.y - rgb.z) / sqrtf(2.0f);

  float angle = h_delta * 2.0f * PI;
  float ct = cosf(angle);
  float st = sinf(angle);
  float rotated_u = u * ct - v * st;
  float rotated_v = u * st + v * ct;
  u = rotated_u * fmaxf(0.0f, s_gain);
  v = rotated_v * fmaxf(0.0f, s_gain);

  float3 out = f3_make(
      neutral + 2.0f * u / sqrtf(6.0f),
      neutral - u / sqrtf(6.0f) + v / sqrtf(2.0f),
      neutral - u / sqrtf(6.0f) - v / sqrtf(2.0f));

  float weight = cu_rgb_direct_sat_value(out);
  float brightness_gain = fmaxf(0.0f, 1.0f + l_delta * weight);
  return f3_make(out.x * brightness_gain, out.y * brightness_gain,
                 out.z * brightness_gain);
}

// ─── Main Kernel — Parallel Pipeline (1 roundtrip) ────────────────────────

__global__ void ColorEqualizerKernel(
    int p_RenderX1, int p_RenderY1, int p_RenderWidth, int p_RenderHeight,
    int p_SrcBoundsX1, int p_SrcBoundsY1, int p_DstBoundsX1,
    int p_DstBoundsY1, int p_SrcRowBytes, int p_DstRowBytes,
    int p_InputPremultiplied, int p_OutputPremultiplied, int inputCS,
    int spaceType, const float *p_Input, float *p_Output, CudaLut p_Lut) {
  const int x = blockIdx.x * blockDim.x + threadIdx.x;
  const int y = blockIdx.y * blockDim.y + threadIdx.y;
  if (x >= p_RenderWidth || y >= p_RenderHeight)
    return;

  const int pixelX = p_RenderX1 + x;
  const int pixelY = p_RenderY1 + y;
  const int srcX = pixelX - p_SrcBoundsX1;
  const int srcY = pixelY - p_SrcBoundsY1;
  const int dstX = pixelX - p_DstBoundsX1;
  const int dstY = pixelY - p_DstBoundsY1;
  const int srcIdx =
      srcY * (p_SrcRowBytes / (int)sizeof(float)) + srcX * 4;
  const int dstIdx =
      dstY * (p_DstRowBytes / (int)sizeof(float)) + dstX * 4;

  const float alpha = p_Input[srcIdx + 3];
  float3 rgb =
      f3_make(p_Input[srcIdx], p_Input[srcIdx + 1], p_Input[srcIdx + 2]);
  if (p_InputPremultiplied != 0) {
    if (alpha <= EPSILON) {
      p_Output[dstIdx + 0] = 0.f;
      p_Output[dstIdx + 1] = 0.f;
      p_Output[dstIdx + 2] = 0.f;
      p_Output[dstIdx + 3] = alpha;
      return;
    }
    rgb.x /= alpha;
    rgb.y /= alpha;
    rgb.z /= alpha;
  }

  if (spaceType == -1) {
    rgb = cu_apply_rgb_direct_equalizer(rgb, p_Lut, 256);
    if (p_OutputPremultiplied != 0) {
      rgb.x *= alpha;
      rgb.y *= alpha;
      rgb.z *= alpha;
    }
    p_Output[dstIdx + 0] = rgb.x;
    p_Output[dstIdx + 1] = rgb.y;
    p_Output[dstIdx + 2] = rgb.z;
    p_Output[dstIdx + 3] = alpha;
    return;
  }

  // ── Single forward conversion ─────────────────────────────────────────
  float3 cs = cu_convert(rgb, spaceType, true, inputCS);
  float hue_normalized = cs.x;
  if (spaceType >= 11) {
    hue_normalized =
        cu_stable_blue_selector_hue(rgb, hue_normalized, inputCS);
  }

  // Sample baked EQ adjustments
  float3 eq = cu_sample_lut_1d(p_Lut, 256, hue_normalized);
  float h_delta = eq.x;
  float s_gain = eq.y;
  float l_delta = eq.z;

  if (fabsf(h_delta) < 1e-6f && fabsf(s_gain - 1.f) < 1e-6f &&
      fabsf(l_delta) < 1e-6f) {
    if (p_OutputPremultiplied != 0) { rgb.x*=alpha; rgb.y*=alpha; rgb.z*=alpha; }
    p_Output[dstIdx+0]=rgb.x; p_Output[dstIdx+1]=rgb.y;
    p_Output[dstIdx+2]=rgb.z; p_Output[dstIdx+3]=alpha;
    return;
  }

  if (spaceType >= 11) {
    const float neutral_weight = cu_oklch_neutral_weight(cs);
    if (neutral_weight <= 0.f) {
      if (p_OutputPremultiplied != 0) {
        rgb.x *= alpha;
        rgb.y *= alpha;
        rgb.z *= alpha;
      }
      p_Output[dstIdx + 0] = rgb.x;
      p_Output[dstIdx + 1] = rgb.y;
      p_Output[dstIdx + 2] = rgb.z;
      p_Output[dstIdx + 3] = alpha;
      return;
    }
    h_delta *= neutral_weight;
    s_gain = 1.f + (s_gain - 1.f) * neutral_weight;
    l_delta *= neutral_weight;
  }

  // ── Apply Adjustments ────────────────────────────────────────────────
  cs.x += h_delta;
  if (cs.x < 0.f) cs.x += 1.f;
  if (cs.x >= 1.f) cs.x -= 1.f;

  if (spaceType == 8) {
    float chroma_radius = cs.z * sinf(cs.y);
    float neutral_axis = cs.z * cosf(cs.y);
    if (fabsf(chroma_radius) < 1e-7f) chroma_radius = 0.f;
    chroma_radius = fmaxf(0.f, chroma_radius * s_gain);
    if (neutral_axis > 1e-7f && chroma_radius > 0.f) {
      float theta = cs.x * 2.f * PI;
      float ct = cosf(theta), st = sinf(theta);
      float base = neutral_axis / sqrtf(3.f);
      float kr = 2.f * ct / sqrtf(6.f);
      float kg = -ct / sqrtf(6.f) + st / sqrtf(2.f);
      float kb = -ct / sqrtf(6.f) - st / sqrtf(2.f);
      float limit = 1e30f;
      if (kr < 0.f) limit = fminf(limit, base / -kr);
      if (kg < 0.f) limit = fminf(limit, base / -kg);
      if (kb < 0.f) limit = fminf(limit, base / -kb);
      chroma_radius = fminf(chroma_radius, limit);
    }
    float weight = chroma_radius / (chroma_radius + fabsf(neutral_axis) + EPSILON);
    float brightness_gain = fmaxf(0.f, 1.f + l_delta * weight);
    chroma_radius *= brightness_gain;
    neutral_axis *= brightness_gain;
    cs.y = atan2f(chroma_radius, neutral_axis);
    cs.z = hypotf(chroma_radius, neutral_axis);

    // ── Inverse conversion ────────────────────────────────────────────
    rgb = cu_convert(cs, spaceType, false, inputCS);
  } else if (spaceType >= 11) {
    cs.y = fmaxf(0.f, cs.y * s_gain);
    float weight = cs.y;
    cs.z *= fmaxf(0.f, 1.f + l_delta * weight);
    rgb = cu_convert(cs, spaceType, false, inputCS);
  } else {
    cs.y = clampf(cs.y * s_gain, 0.f, 1.f);
    float weight = cs.y;
    cs.z *= fmaxf(0.f, 1.f + l_delta * weight);
    rgb = cu_convert(cs, spaceType, false, inputCS);
  }

  if (p_OutputPremultiplied != 0) { rgb.x*=alpha; rgb.y*=alpha; rgb.z*=alpha; }
  p_Output[dstIdx + 0] = rgb.x;
  p_Output[dstIdx + 1] = rgb.y;
  p_Output[dstIdx + 2] = rgb.z;
  p_Output[dstIdx + 3] = alpha;
}

// ─── Host entry point ─────────────────────────────────────────────────────

extern "C" bool RunCudaKernel(
    void *p_Stream, int p_RenderX1, int p_RenderY1, int p_RenderWidth,
    int p_RenderHeight, int p_SrcBoundsX1, int p_SrcBoundsY1,
    int p_DstBoundsX1, int p_DstBoundsY1, int p_SrcRowBytes,
    int p_DstRowBytes, int p_InputPremultiplied, int p_OutputPremultiplied,
    int p_inputCS, int p_spaceType, const float *p_Input, float *p_Output,
    const float *p_Lut) {
  if (!p_Input || !p_Output || !p_Lut || p_RenderWidth <= 0 ||
      p_RenderHeight <= 0 || p_SrcRowBytes <= 0 || p_DstRowBytes <= 0) {
    return false;
  }

  cudaStream_t stream = (cudaStream_t)p_Stream;

  // Kernel parameters are stream-local, avoiding races between simultaneous
  // plugin instances that use different equalizer LUTs.
  CudaLut lut;
  std::memcpy(lut.values, p_Lut, sizeof(lut.values));

  dim3 block(16, 16);
  dim3 grid((p_RenderWidth + block.x - 1) / block.x,
            (p_RenderHeight + block.y - 1) / block.y);

  ColorEqualizerKernel<<<grid, block, 0, stream>>>(
      p_RenderX1, p_RenderY1, p_RenderWidth, p_RenderHeight, p_SrcBoundsX1,
      p_SrcBoundsY1, p_DstBoundsX1, p_DstBoundsY1, p_SrcRowBytes,
      p_DstRowBytes, p_InputPremultiplied, p_OutputPremultiplied, p_inputCS,
      p_spaceType, p_Input, p_Output, lut);
  return cudaGetLastError() == cudaSuccess;
}
