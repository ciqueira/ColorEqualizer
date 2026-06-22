// =============================================================================
// ColorMath.h
// -----------------------------------------------------------------------------
// Host-side color math used for LUT construction and numerical tests.
//
// Provenance and licensing audit:
//   See ../THIRD_PARTY_NOTICES.md.
// =============================================================================

#ifndef COLOR_MATH_H
#define COLOR_MATH_H

#include <cmath>
#include <algorithm>

namespace colormath {

// ─── Constants ────────────────────────────────────────────────────────────
static constexpr float PI      = 3.141592653589f;
static constexpr float EPSILON = 1e-10f;
static constexpr int   EQ_NODES = 10;

// ─── float3 type ──────────────────────────────────────────────────────────
struct float3 { float x, y, z; };

inline float3 make_float3(float a, float b, float c) { return {a, b, c}; }

inline float3 operator+(float3 a, float3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
inline float3 operator-(float3 a, float3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
inline float3 operator*(float3 a, float3 b) { return {a.x*b.x, a.y*b.y, a.z*b.z}; }
inline float3 operator/(float3 a, float3 b) { return {a.x/b.x, a.y/b.y, a.z/b.z}; }
inline float3 operator*(float3 a, float s)  { return {a.x*s, a.y*s, a.z*s}; }
inline float3 operator+(float3 a, float s)  { return {a.x+s, a.y+s, a.z+s}; }

inline float3& operator*=(float3& a, float3 b) { a.x*=b.x; a.y*=b.y; a.z*=b.z; return a; }
inline float3& operator+=(float3& a, float s)  { a.x+=s; a.y+=s; a.z+=s; return a; }
inline float3& operator*=(float3& a, float s)  { a.x*=s; a.y*=s; a.z*=s; return a; }

inline float dot(float3 a, float3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline float clampf(float x, float lo, float hi) { return fminf(fmaxf(x, lo), hi); }
inline float max3(float a, float b, float c) { return fmaxf(a, fmaxf(b, c)); }

// ─── Color matrices ───────────────────────────────────────────────────────

inline float3 rgb_to_xyz(float3 rgb, int input_cs) {
    float x = rgb.x, y = rgb.y, z = rgb.z;
    if (input_cs == 0) { // ACEScg (AP1)
        x = rgb.x *  0.66245418f + rgb.y * 0.13400421f + rgb.z * 0.15618766f;
        y = rgb.x *  0.27222872f + rgb.y * 0.67408177f + rgb.z * 0.05368952f;
        z = rgb.x * -0.00557465f + rgb.y * 0.00406073f + rgb.z * 1.01033910f;
    } else if (input_cs == 1) { // DaVinci Wide Gamut
        x = rgb.x *  0.70062239f + rgb.y *  0.14877482f + rgb.z *  0.10105872f;
        y = rgb.x *  0.27411851f + rgb.y *  0.87363190f + rgb.z * -0.14775041f;
        z = rgb.x * -0.09896291f + rgb.y * -0.13789533f + rgb.z *  1.32591599f;
    } else if (input_cs == 2) { // ARRI Wide Gamut 3
        x = rgb.x * 0.63800764f + rgb.y *  0.21470386f + rgb.z *  0.09774445f;
        y = rgb.x * 0.29195377f + rgb.y *  0.82384104f + rgb.z * -0.11579482f;
        z = rgb.x * 0.00279827f + rgb.y * -0.06703423f + rgb.z *  1.15329373f;
    } else if (input_cs == 3) { // ARRI Wide Gamut 4
        // ARRI LogC4 Specification: linear AWG4 (D65) to CIE XYZ.
        x = rgb.x * 0.70485832f + rgb.y * 0.12976030f + rgb.z *  0.11583731f;
        y = rgb.x * 0.25452418f + rgb.y * 0.78147773f + rgb.z * -0.03600191f;
        z = rgb.x * 0.00000000f + rgb.y * 0.00000000f + rgb.z *  1.08905775f;
    }
    return make_float3(x, y, z);
}

inline float3 xyz_to_rgb(float3 xyz, int output_cs) {
    float r = xyz.x, g = xyz.y, b = xyz.z;
    if (output_cs == 0) { // ACEScg (AP1)
        r = xyz.x *  1.64102338f + xyz.y * -0.32480329f + xyz.z * -0.23642470f;
        g = xyz.x * -0.66366286f + xyz.y *  1.61533159f + xyz.z *  0.01675635f;
        b = xyz.x *  0.01172189f + xyz.y * -0.00828444f + xyz.z *  0.98839486f;
    } else if (output_cs == 1) { // DaVinci Wide Gamut
        r = xyz.x *  1.51667204f + xyz.y * -0.28147805f + xyz.z * -0.14696363f;
        g = xyz.x * -0.46491710f + xyz.y *  1.25142378f + xyz.z *  0.17488461f;
        b = xyz.x *  0.06484905f + xyz.y *  0.10913934f + xyz.z *  0.76141462f;
    } else if (output_cs == 2) { // ARRI Wide Gamut 3
        r = xyz.x *  1.78906548f + xyz.y * -0.48253384f + xyz.z * -0.20007578f;
        g = xyz.x * -0.63984859f + xyz.y *  1.39639986f + xyz.z *  0.19443229f;
        b = xyz.x * -0.04153153f + xyz.y *  0.08233536f + xyz.z *  0.87886840f;
    } else if (output_cs == 3) { // ARRI Wide Gamut 4
        // Inverse of the official AWG4-to-XYZ matrix.
        r = xyz.x *  1.50921547f + xyz.y * -0.25059735f + xyz.z * -0.16881148f;
        g = xyz.x * -0.49154545f + xyz.y *  1.36124555f + xyz.z *  0.09728294f;
        b = xyz.x *  0.00000000f + xyz.y *  0.00000000f + xyz.z *  0.91822495f;
    }
    return make_float3(r, g, b);
}

// ─── Scene-linear transfer functions ─────────────────────────────────────

inline float decode_acescct(float value) {
    constexpr float cut = 0.155251141552511f;
    return value <= cut
        ? (value - 0.0729055341958355f) / 10.5402377416545f
        : exp2f(value * 17.52f - 9.72f);
}

inline float encode_acescct(float value) {
    return value <= 0.0078125f
        ? value * 10.5402377416545f + 0.0729055341958355f
        : (log2f(value) + 9.72f) / 17.52f;
}

inline float decode_davinci_intermediate(float value) {
    return value <= 0.02740668f
        ? value / 10.44426855f
        : exp2f(value / 0.07329248f - 7.0f) - 0.0075f;
}

inline float encode_davinci_intermediate(float value) {
    return value <= 0.00262409f
        ? value * 10.44426855f
        : (log2f(value + 0.0075f) + 7.0f) * 0.07329248f;
}

inline float decode_logc3_ei800(float value) {
    constexpr float cut = 0.149658f;
    return value > cut
        ? (powf(10.0f, (value - 0.385537f) / 0.247190f) - 0.052272f)
              / 5.555556f
        : (value - 0.092809f) / 5.367655f;
}

inline float encode_logc3_ei800(float value) {
    return value > 0.010591f
        ? 0.247190f * log10f(5.555556f * value + 0.052272f) + 0.385537f
        : 5.367655f * value + 0.092809f;
}

inline float decode_logc4(float value) {
    constexpr float a = 2231.82630907f;
    constexpr float b = 0.907135874878f;
    constexpr float c = 0.092864125122f;
    constexpr float s = 0.113597208611f;
    constexpr float t = -0.018056996120f;
    return value < 0.0f
        ? value * s + t
        : (exp2f(14.0f * (value - c) / b + 6.0f) - 64.0f) / a;
}

inline float encode_logc4(float value) {
    constexpr float a = 2231.82630907f;
    constexpr float b = 0.907135874878f;
    constexpr float c = 0.092864125122f;
    constexpr float s = 0.113597208611f;
    constexpr float t = -0.018056996120f;
    return value < t
        ? (value - t) / s
        : (log2f(a * value + 64.0f) - 6.0f) * b / 14.0f + c;
}

inline float decode_input_transfer(float value, int input_cs) {
    if (input_cs == 0) return decode_acescct(value);
    if (input_cs == 1) return decode_davinci_intermediate(value);
    if (input_cs == 2) return decode_logc3_ei800(value);
    if (input_cs == 3) return decode_logc4(value);
    return value;
}

inline float encode_input_transfer(float value, int input_cs) {
    if (input_cs == 0) return encode_acescct(value);
    if (input_cs == 1) return encode_davinci_intermediate(value);
    if (input_cs == 2) return encode_logc3_ei800(value);
    if (input_cs == 3) return encode_logc4(value);
    return value;
}

inline float3 decode_input_transfer(float3 rgb, int input_cs) {
    return make_float3(decode_input_transfer(rgb.x, input_cs),
                       decode_input_transfer(rgb.y, input_cs),
                       decode_input_transfer(rgb.z, input_cs));
}

inline float3 encode_input_transfer(float3 rgb, int input_cs) {
    return make_float3(encode_input_transfer(rgb.x, input_cs),
                       encode_input_transfer(rgb.y, input_cs),
                       encode_input_transfer(rgb.z, input_cs));
}

// ─── Normalized log signal ───────────────────────────────────────────────
// RGB Spherical operates on a normalized version of the selected encoded
// signal. Remove only the transfer curve's code-value offset and scale so that
// scene-linear black maps to 0 and scene-linear white maps to 1. The curve
// shape remains intact and the mapping is reversible.

inline float log_signal_black(int input_cs) {
    if (input_cs == 0) return 0.0729055341958355f; // ACEScct
    if (input_cs == 1) return 0.0f;                // DaVinci Intermediate
    if (input_cs == 2) return 0.092809f;           // LogC3 EI800
    if (input_cs == 3) return 0.092864125122f;     // LogC4
    return 0.0f;
}

inline float log_signal_range(int input_cs) {
    if (input_cs == 0) return 0.481888986352110f;
    if (input_cs == 1) return 0.513837441116225f;
    if (input_cs == 2) return 0.477822558120417f;
    if (input_cs == 3) return 0.334655239713281f;
    return 1.0f;
}

inline float normalize_log_signal(float value, int input_cs) {
    return (value - log_signal_black(input_cs)) / log_signal_range(input_cs);
}

inline float denormalize_log_signal(float value, int input_cs) {
    return value * log_signal_range(input_cs) + log_signal_black(input_cs);
}

inline float3 normalize_log_signal(float3 rgb, int input_cs) {
    return make_float3(normalize_log_signal(rgb.x, input_cs),
                       normalize_log_signal(rgb.y, input_cs),
                       normalize_log_signal(rgb.z, input_cs));
}

inline float3 denormalize_log_signal(float3 rgb, int input_cs) {
    return make_float3(denormalize_log_signal(rgb.x, input_cs),
                       denormalize_log_signal(rgb.y, input_cs),
                       denormalize_log_signal(rgb.z, input_cs));
}

// ─── Matrix multiply 3x3 × float3 (for Oklab) ────────────────────────────

inline float3 mv33(const float* mat, float3 v) {
    return make_float3(
        mat[0]*v.x + mat[1]*v.y + mat[2]*v.z,
        mat[3]*v.x + mat[4]*v.y + mat[5]*v.z,
        mat[6]*v.x + mat[7]*v.y + mat[8]*v.z);
}

// ─── Oklab matrices ──────────────────────────────────────────────────────

static const float XYZ_to_LMS_mat[9] = {
    0.8189330101f, 0.3618667424f, -0.1288597137f,
    0.0329845436f, 0.9293118715f,  0.0361456387f,
    0.0482003018f, 0.2643662691f,  0.6338517070f
};
static const float LMS_to_Oklab_mat[9] = {
    0.2104542553f,  0.7936177850f, -0.0040720468f,
    1.9779984951f, -2.4285922050f,  0.4505937099f,
    0.0259040371f,  0.7827717662f, -0.8086757660f
};
static const float Oklab_to_LMS_mat[9] = {
    1.0f,  0.3963377774f,  0.2158037573f,
    1.0f, -0.1055613458f, -0.0638541728f,
    1.0f, -0.0894841775f, -1.2914855480f
};
static const float LMS_to_XYZ_mat[9] = {
     1.2270138511f, -0.5577999807f,  0.2812561490f,
    -0.0405801784f,  1.1122568696f, -0.0716766787f,
    -0.0763812845f, -0.4214819784f,  1.5861632239f
};

// ─── Oklab conversions ────────────────────────────────────────────────────

inline float3 XYZ_to_Oklab(float3 xyz) {
    float3 lms = mv33(XYZ_to_LMS_mat, xyz);
    float lx = lms.x < 0.0f ? -powf(-lms.x, 1.0f/3.0f) : powf(lms.x, 1.0f/3.0f);
    float ly = lms.y < 0.0f ? -powf(-lms.y, 1.0f/3.0f) : powf(lms.y, 1.0f/3.0f);
    float lz = lms.z < 0.0f ? -powf(-lms.z, 1.0f/3.0f) : powf(lms.z, 1.0f/3.0f);
    return mv33(LMS_to_Oklab_mat, make_float3(lx, ly, lz));
}

inline float3 Oklab_to_XYZ(float3 oklab) {
    float3 lms_p = mv33(Oklab_to_LMS_mat, oklab);
    float3 lms = make_float3(
        lms_p.x * lms_p.x * lms_p.x,
        lms_p.y * lms_p.y * lms_p.y,
        lms_p.z * lms_p.z * lms_p.z);
    return mv33(LMS_to_XYZ_mat, lms);
}

// ─── OKLCH conversion ─────────────────────────────────────────────────────

inline float3 OKLAB_to_OKLCH(float3 lab) {
    float C = hypotf(lab.y, lab.z);
    float h = atan2f(lab.z, lab.y);
    if (h < 0.0f) h += 2.0f * PI;
    h /= (2.0f * PI);
    return make_float3(h, C, lab.x);
}

inline float3 OKLCH_to_OKLAB(float3 lch) {
    float h = lch.x * 2.0f * PI;
    float a = lch.y * cosf(h);
    float b = lch.y * sinf(h);
    return make_float3(lch.z, a, b);
}

// ─── RGB Spherical model ──────────────────────────────────────────────────
// Independent implementation using an orthonormal basis around the neutral
// RGB axis followed by standard Cartesian/spherical coordinate conversion.
// See ../THIRD_PARTY_NOTICES.md.

inline float3 RGB_to_Spherical(float3 rgb) {
    float r = rgb.x, g = rgb.y, b = rgb.z;
    const float inv_sqrt2 = 1.0f / sqrtf(2.0f);
    const float inv_sqrt3 = 1.0f / sqrtf(3.0f);
    const float inv_sqrt6 = 1.0f / sqrtf(6.0f);

    // Orthonormal opponent basis. The w axis follows neutral RGB (1,1,1).
    float u = (2.0f*r - g - b) * inv_sqrt6;
    float v = (g - b) * inv_sqrt2;
    float w = (r + g + b) * inv_sqrt3;

    float rho = sqrtf(u*u + v*v + w*w);
    if (rho < EPSILON) return make_float3(0.0f, 0.0f, 0.0f);

    float theta = atan2f(v, u);
    if (theta < 0.0f) theta += 2.0f * PI;
    float phi = atan2f(hypotf(u, v), w);

    return make_float3(theta / (2.0f * PI), phi, rho);
}

inline float3 Spherical_to_RGB(float3 spherical) {
    float theta = spherical.x * 2.0f * PI;
    float phi = spherical.y;
    float rho = spherical.z;
    if (rho < EPSILON) return make_float3(0.0f, 0.0f, 0.0f);

    float u = rho * sinf(phi) * cosf(theta);
    float v = rho * sinf(phi) * sinf(theta);
    float w = rho * cosf(phi);

    const float inv_sqrt2 = 1.0f / sqrtf(2.0f);
    const float inv_sqrt3 = 1.0f / sqrtf(3.0f);
    const float inv_sqrt6 = 1.0f / sqrtf(6.0f);
    float r = 2.0f*u*inv_sqrt6 + w*inv_sqrt3;
    float g = -u*inv_sqrt6 + v*inv_sqrt2 + w*inv_sqrt3;
    float b = -u*inv_sqrt6 - v*inv_sqrt2 + w*inv_sqrt3;
    return make_float3(r, g, b);
}

// ─── Unified color-space conversion ───────────────────────────────────────

inline float3 convert_colorSpace_model(float3 rgb, int space_type, bool direction, int input_cs) {
    if (direction) {
        if (space_type >= 11) {
            // Preserve the legacy OKLCH behavior: apply the gamut matrix
            // directly to the host's selected encoded signal.
            float3 xyz   = rgb_to_xyz(rgb, input_cs);
            float3 oklab = XYZ_to_Oklab(xyz);
            return OKLAB_to_OKLCH(oklab);
        }
        if (space_type == 8) return RGB_to_Spherical(rgb);
        const float3 log_rgb = normalize_log_signal(rgb, input_cs);
        return log_rgb;
    } else {
        if (space_type >= 11) {
            float3 oklab = OKLCH_to_OKLAB(rgb);
            float3 xyz   = Oklab_to_XYZ(oklab);
            return xyz_to_rgb(xyz, input_cs);
        }
        if (space_type == 8) return Spherical_to_RGB(rgb);
        float3 log_rgb = rgb;
        return denormalize_log_signal(log_rgb, input_cs);
    }
}

// ─── Equalizer helpers ────────────────────────────────────────────────────
// Independent periodic interpolation using the real discrete Fourier series
// for an even number of equally spaced samples. This is a direct implementation
// of standard trigonometric interpolation identities; see
// ../THIRD_PARTY_NOTICES.md for the mathematical references.

inline float eq_rad_correction(float angle, float angle_shift) {
    return angle - angle_shift * ((2.0f * PI) / 360.0f);
}

struct PeriodicSeries10 {
    float dc;
    float cosine[4];
    float sine[4];
    float nyquist;
};

inline PeriodicSeries10 make_periodic_series10(const float samples[EQ_NODES]) {
    PeriodicSeries10 series{};
    constexpr float inv_count = 1.0f / (float)EQ_NODES;
    constexpr int highest_paired_harmonic = EQ_NODES / 2 - 1;

    for (int node = 0; node < EQ_NODES; ++node) {
        const float value = samples[node];
        const float phase = (2.0f * PI * (float)node) * inv_count;

        series.dc += value * inv_count;
        series.nyquist += value * ((node & 1) ? -inv_count : inv_count);

        for (int harmonic = 1; harmonic <= highest_paired_harmonic; ++harmonic) {
            const float harmonic_phase = (float)harmonic * phase;
            const float scale = 2.0f * inv_count;
            series.cosine[harmonic - 1] += value * cosf(harmonic_phase) * scale;
            series.sine[harmonic - 1] += value * sinf(harmonic_phase) * scale;
        }
    }

    return series;
}

inline float evaluate_periodic_series10(const PeriodicSeries10& series,
                                        float angle) {
    float value = series.dc;
    constexpr int highest_paired_harmonic = EQ_NODES / 2 - 1;

    for (int harmonic = 1; harmonic <= highest_paired_harmonic; ++harmonic) {
        const float harmonic_angle = (float)harmonic * angle;
        value += series.cosine[harmonic - 1] * cosf(harmonic_angle);
        value += series.sine[harmonic - 1] * sinf(harmonic_angle);
    }

    return value + series.nyquist * cosf((float)(EQ_NODES / 2) * angle);
}

// ─── Build Equalizer LUT (CPU Pre-calc) ───────────────────────────────────

// Lógica de pré-cálculo de 10-bandas para uma LUT 1D de `num_points`.
// A LUT resultante tem `num_points * 3` floats intercalados (Hue, Sat, Lum).
inline void build_equalizer_lut(float* out_lut, int num_points, int space_type,
                                const float hue_adjustments[10], float hue_master,
                                const float sat_adjustments[10], float sat_master,
                                const float luma_adjustments[10], float luma_master) {
    const PeriodicSeries10 hue_series = make_periodic_series10(hue_adjustments);
    const PeriodicSeries10 sat_series = make_periodic_series10(sat_adjustments);
    const PeriodicSeries10 luma_series = make_periodic_series10(luma_adjustments);
    const float angle_shift = (space_type == 11) ? 5.0f : -10.0f;
    const float step = 1.0f / (float)num_points;
    
    for (int i = 0; i < num_points; ++i) {
        float hue_normalized = ((float)i + 0.5f) * step; // center of bin
        float hue_rad = hue_normalized * 2.0f * PI;
        float corrected = eq_rad_correction(hue_rad, angle_shift);
        
        // Hue
        float hue_offset = evaluate_periodic_series10(hue_series, corrected);
        float h_delta = hue_offset * hue_master * (30.0f / 360.0f);
        
        // Sat
        float sat_target = evaluate_periodic_series10(sat_series, corrected);
        // Fator de diluição para diminuir a sensibilidade dos sliders de Saturação (50% mais suave)
        float sat_delta = (sat_target - 1.0f) * 0.5f; 
        float s_gain = 1.0f + sat_delta * sat_master;
        
        // Lum
        float lum_target = evaluate_periodic_series10(luma_series, corrected);
        float l_delta = (lum_target - 1.0f) * luma_master;
        
        // Interleaved: RGB -> HSL per point
        out_lut[i * 3 + 0] = h_delta;
        out_lut[i * 3 + 1] = s_gain;
        out_lut[i * 3 + 2] = l_delta;
    }
}

// ─── LUT Sampler (Bilinear 1D) ────────────────────────────────────────────

inline float3 sample_lut_1d(const float* lut, int num_points, float normalized_x) {
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
    
    return make_float3(
        lut[idx0 + 0] * (1.0f - t) + lut[idx1 + 0] * t,
        lut[idx0 + 1] * (1.0f - t) + lut[idx1 + 1] * t,
        lut[idx0 + 2] * (1.0f - t) + lut[idx1 + 2] * t
    );
}

// ─── Apply All Equalizers — Parallel Pipeline (1 roundtrip via LUT) ───────

inline float3 apply_equalizers_parallel(float3 rgb, int space_type, int input_cs,
                                        const float* lut, int lut_size) {
    // ── Single forward conversion ─────────────────────────────────────────
    float3 cs = convert_colorSpace_model(rgb, space_type, true, input_cs);
    float hue_normalized = cs.x;
    
    // Sample baked EQ adjustments
    float3 eq = sample_lut_1d(lut, lut_size, hue_normalized);
    float h_delta = eq.x;
    float s_gain = eq.y;
    float l_delta = eq.z;

    if (fabsf(h_delta) < 1e-6f &&
        fabsf(s_gain - 1.0f) < 1e-6f &&
        fabsf(l_delta) < 1e-6f) {
        return rgb;
    }

    // ── Apply Adjustments ────────────────────────────────────────────────
    cs.x += h_delta;
    if (cs.x < 0.0f) cs.x += 1.0f;
    if (cs.x >= 1.0f) cs.x -= 1.0f;

    if (space_type == 8) {
        // Preserve the original spherical transform. Apply saturation to the
        // opponent-plane radius rather than multiplying the inclination angle.
        float chroma_radius = cs.z * sinf(cs.y);
        float neutral_axis = cs.z * cosf(cs.y);
        if (fabsf(chroma_radius) < 1e-7f) chroma_radius = 0.0f;
        chroma_radius = fmaxf(0.0f, chroma_radius * s_gain);

        if (neutral_axis > 1e-7f && chroma_radius > 0.0f) {
            const float theta = cs.x * 2.0f * PI;
            const float ct = cosf(theta);
            const float st = sinf(theta);
            const float base = neutral_axis / sqrtf(3.0f);
            const float kr = 2.0f * ct / sqrtf(6.0f);
            const float kg = -ct / sqrtf(6.0f) + st / sqrtf(2.0f);
            const float kb = -ct / sqrtf(6.0f) - st / sqrtf(2.0f);
            float limit = 1e30f;
            if (kr < 0.0f) limit = fminf(limit, base / -kr);
            if (kg < 0.0f) limit = fminf(limit, base / -kg);
            if (kb < 0.0f) limit = fminf(limit, base / -kb);
            chroma_radius = fminf(chroma_radius, limit);
        }

        const float weight = chroma_radius /
            (chroma_radius + fabsf(neutral_axis) + EPSILON);
        const float brightness_gain =
            fmaxf(0.0f, 1.0f + l_delta * weight);
        chroma_radius *= brightness_gain;
        neutral_axis *= brightness_gain;

        cs.y = atan2f(chroma_radius, neutral_axis);
        cs.z = hypotf(chroma_radius, neutral_axis);

        // ── Inverse conversion ───────────────────────────────────────────
        float3 out = convert_colorSpace_model(cs, space_type, false, input_cs);
        return out;
    } else if (space_type >= 11) {
        // OKLCH has no intrinsic upper bound on chroma. A hard C<=1 clamp
        // creates visible slope changes in saturated and HDR gradients.
        cs.y = fmaxf(0.0f, cs.y * s_gain);
        const float weight = cs.y;
        cs.z *= fmaxf(0.0f, 1.0f + l_delta * weight);
    } else {
        cs.y = clampf(cs.y * s_gain, 0.0f, 1.0f);
        const float weight = cs.y;
        cs.z *= fmaxf(0.0f, 1.0f + l_delta * weight);
    }

    // ── Single inverse conversion ─────────────────────────────────────────
    float3 out = convert_colorSpace_model(cs, space_type, false, input_cs);

    return out;
}

} // namespace colormath

#endif // COLOR_MATH_H
