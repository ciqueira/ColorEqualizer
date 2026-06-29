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
static constexpr float OKLAB_NEUTRAL_EPSILON = 2e-4f;
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

inline float wrap_unit(float value) {
    value = fmodf(value, 1.0f);
    return value < 0.0f ? value + 1.0f : value;
}

inline float circular_delta(float from, float to) {
    return fmodf(to - from + 1.5f, 1.0f) - 0.5f;
}

inline float smoothstepf(float edge0, float edge1, float value) {
    const float t = clampf((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

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

// Bradford chromatic adaptation between the ACES white point (D60) and the
// D65-relative XYZ domain expected by Oklab. The other supported RGB spaces
// already use D65 and must not pass through these matrices.
inline float3 adapt_xyz_d60_to_d65(float3 xyz) {
    return make_float3(
        xyz.x *  0.987224008703f + xyz.y * -0.006113228607f +
            xyz.z *  0.015953288336f,
        xyz.x * -0.007598371812f + xyz.y *  1.001861484740f +
            xyz.z *  0.005330035791f,
        xyz.x *  0.003072577059f + xyz.y * -0.005095961511f +
            xyz.z *  1.081680603066f);
}

inline float3 adapt_xyz_d65_to_d60(float3 xyz) {
    return make_float3(
        xyz.x *  1.013034914650f + xyz.y *  0.006105257823f +
            xyz.z * -0.014970943627f,
        xyz.x *  0.007698230125f + xyz.y *  0.998163352118f +
            xyz.z * -0.005032038535f,
        xyz.x * -0.002841317432f + xyz.y *  0.004685156723f +
            xyz.z *  0.924506137458f);
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

inline float3 neutralize_small_oklab_chroma(float3 oklab) {
    const float chroma = hypotf(oklab.y, oklab.z);
    const float threshold =
        OKLAB_NEUTRAL_EPSILON * fmaxf(1.0f, fabsf(oklab.x));
    if (chroma <= threshold) {
        oklab.y = 0.0f;
        oklab.z = 0.0f;
    }
    return oklab;
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

// ─── Stable blue-band selector ─────────────────────────────────────────────
// Camera-wide gamuts can place saturated blue near signed-LMS zero crossings,
// where OKLCH hue changes rapidly. Keep OKLCH for the actual H/C/L adjustment,
// but use a smooth RGB-opponent hue only to select the equalizer band in the
// cyan-blue-magenta region. ACES keeps its existing OKLCH selector unchanged.

inline float rgb_opponent_hue(float3 rgb, float fallback_hue) {
    const float u = (2.0f * rgb.x - rgb.y - rgb.z) / sqrtf(6.0f);
    const float v = (rgb.y - rgb.z) / sqrtf(2.0f);
    if (hypotf(u, v) < 1e-7f) return fallback_hue;

    float hue = atan2f(v, u) / (2.0f * PI);
    return wrap_unit(hue);
}

inline float stable_blue_selector_hue(float3 rgb, float model_hue,
                                      int input_cs) {
    if (input_cs == 0) return model_hue;

    const float opponent_hue = rgb_opponent_hue(rgb, model_hue);
    constexpr float BLUE_OPPONENT_HUE = 2.0f / 3.0f;
    constexpr float BLUE_EQ_CENTER = 257.0f / 360.0f;
    constexpr float MASK_INNER = 25.0f / 360.0f;
    constexpr float MASK_OUTER = 75.0f / 360.0f;

    const float distance =
        fabsf(circular_delta(BLUE_OPPONENT_HUE, opponent_hue));
    const float mask =
        1.0f - smoothstepf(MASK_INNER, MASK_OUTER, distance);
    const float stable_hue =
        wrap_unit(opponent_hue + (BLUE_EQ_CENTER - BLUE_OPPONENT_HUE));
    return wrap_unit(model_hue + circular_delta(model_hue, stable_hue) * mask);
}

// ─── Unified color-space conversion ───────────────────────────────────────

inline float3 convert_colorSpace_model(float3 rgb, int space_type, bool direction, int input_cs) {
    if (direction) {
        if (space_type >= 11) {
            // Preserve the legacy OKLCH behavior: apply the gamut matrix
            // directly to the host's selected encoded signal. AP1 is D60, so
            // adapt its XYZ values to the D65 domain expected by Oklab.
            float3 xyz   = rgb_to_xyz(rgb, input_cs);
            if (input_cs == 0) xyz = adapt_xyz_d60_to_d65(xyz);
            float3 oklab = neutralize_small_oklab_chroma(XYZ_to_Oklab(xyz));
            return OKLAB_to_OKLCH(oklab);
        }
        if (space_type == 8) return RGB_to_Spherical(rgb);
        return rgb;
    } else {
        if (space_type >= 11) {
            float3 oklab = OKLCH_to_OKLAB(rgb);
            float3 xyz   = Oklab_to_XYZ(oklab);
            if (input_cs == 0) xyz = adapt_xyz_d65_to_d60(xyz);
            return xyz_to_rgb(xyz, input_cs);
        }
        if (space_type == 8) return Spherical_to_RGB(rgb);
        return rgb;
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

// ─── RGB Direct equalizer ─────────────────────────────────────────────────

inline float rgb_direct_sat_value(float3 rgb) {
    const float neutral = (rgb.x + rgb.y + rgb.z) / 3.0f;
    const float cx = rgb.x - neutral;
    const float cy = rgb.y - neutral;
    const float cz = rgb.z - neutral;
    const float chroma_mag = sqrtf(cx * cx + cy * cy + cz * cz);
    return clampf(chroma_mag / (fabsf(neutral) + chroma_mag + 1.0e-6f),
                  0.0f, 1.0f);
}

inline float3 apply_rgb_direct_equalizer(float3 rgb, const float* lut,
                                         int lut_size) {
    const float hue_normalized = rgb_opponent_hue(rgb, 0.0f);
    const float3 eq = sample_lut_1d(lut, lut_size, hue_normalized);
    const float h_delta = eq.x;
    const float s_gain = eq.y;
    const float l_delta = eq.z;

    if (fabsf(h_delta) < 1e-6f &&
        fabsf(s_gain - 1.0f) < 1e-6f &&
        fabsf(l_delta) < 1e-6f) {
        return rgb;
    }

    const float neutral = (rgb.x + rgb.y + rgb.z) / 3.0f;
    float u = (2.0f * rgb.x - rgb.y - rgb.z) / sqrtf(6.0f);
    float v = (rgb.y - rgb.z) / sqrtf(2.0f);

    const float angle = h_delta * 2.0f * PI;
    const float ct = cosf(angle);
    const float st = sinf(angle);
    const float rotated_u = u * ct - v * st;
    const float rotated_v = u * st + v * ct;
    u = rotated_u * fmaxf(0.0f, s_gain);
    v = rotated_v * fmaxf(0.0f, s_gain);

    float3 out = make_float3(
        neutral + 2.0f * u / sqrtf(6.0f),
        neutral - u / sqrtf(6.0f) + v / sqrtf(2.0f),
        neutral - u / sqrtf(6.0f) - v / sqrtf(2.0f));

    const float weight = rgb_direct_sat_value(out);
    const float brightness_gain = fmaxf(0.0f, 1.0f + l_delta * weight);
    return out * brightness_gain;
}

// ─── Apply All Equalizers — Parallel Pipeline (1 roundtrip via LUT) ───────

inline float3 apply_equalizers_parallel(float3 rgb, int space_type, int input_cs,
                                        const float* lut, int lut_size) {
    if (space_type == -1) {
        return apply_rgb_direct_equalizer(rgb, lut, lut_size);
    }

    // ── Single forward conversion ─────────────────────────────────────────
    float3 cs = convert_colorSpace_model(rgb, space_type, true, input_cs);
    float hue_normalized = cs.x;
    if (space_type >= 11) {
        hue_normalized =
            stable_blue_selector_hue(rgb, hue_normalized, input_cs);
    }
    
    // Sample baked EQ adjustments
    float3 eq = sample_lut_1d(lut, lut_size, hue_normalized);
    float h_delta = eq.x;
    float s_gain = eq.y;
    float l_delta = eq.z;

    // Hue, saturation and chroma-weighted brightness cannot affect an exact
    // OKLCH neutral. Avoid an unnecessary matrix round trip and preserve the
    // original encoded RGB bit-for-bit.
    if (space_type >= 11 && cs.y == 0.0f) {
        return rgb;
    }

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
