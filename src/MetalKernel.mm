// =============================================================================
// MetalKernel.mm
// -----------------------------------------------------------------------------
// Metal GPU kernel for MCColorEqualizer.
// All color math is embedded in the MSL shader string.
// Compiled as Objective-C++ (.mm).
// =============================================================================

#include "EQParams.h"
#import <Metal/Metal.h>
#include <cstdio>
#include <mutex>
#include <unordered_map>

// ─── Inline Metal shader source ───────────────────────────────────────────

static const char *kMetalSource = R"METAL(
#include <metal_stdlib>
using namespace metal;

#define PI      3.141592653589f
#define EPSILON 1e-10f
#define OKLAB_NEUTRAL_EPSILON 2e-4f
#define EQ_NODES 10

float mtl_wrap_unit(float value) {
    value = fmod(value, 1.f);
    return value < 0.f ? value + 1.f : value;
}

float mtl_circular_delta(float from, float to) {
    return fmod(to-from+1.5f, 1.f)-0.5f;
}

float mtl_smoothstep(float edge0, float edge1, float value) {
    float t=clamp((value-edge0)/(edge1-edge0),0.f,1.f);
    return t*t*(3.f-2.f*t);
}

// ─── Color Matrices ──────────────────────────────────────────────────────

float3 mtl_rgb_to_xyz(float3 rgb, int cs) {
    float x = rgb.x, y = rgb.y, z = rgb.z;
    if (cs == 0) {
        x = rgb.x* 0.66245418f + rgb.y* 0.13400421f + rgb.z* 0.15618766f;
        y = rgb.x* 0.27222872f + rgb.y* 0.67408177f + rgb.z* 0.05368952f;
        z = rgb.x*-0.00557465f + rgb.y* 0.00406073f + rgb.z* 1.01033910f;
    } else if (cs == 1) {
        x = rgb.x* 0.70062239f + rgb.y* 0.14877482f + rgb.z* 0.10105872f;
        y = rgb.x* 0.27411851f + rgb.y* 0.87363190f + rgb.z*-0.14775041f;
        z = rgb.x*-0.09896291f + rgb.y*-0.13789533f + rgb.z* 1.32591599f;
    } else if (cs == 2) {
        x = rgb.x* 0.63800764f + rgb.y* 0.21470386f + rgb.z* 0.09774445f;
        y = rgb.x* 0.29195377f + rgb.y* 0.82384104f + rgb.z*-0.11579482f;
        z = rgb.x* 0.00279827f + rgb.y*-0.06703423f + rgb.z* 1.15329373f;
    } else if (cs == 3) {
        x = rgb.x* 0.70485832f + rgb.y* 0.12976030f + rgb.z* 0.11583731f;
        y = rgb.x* 0.25452418f + rgb.y* 0.78147773f + rgb.z*-0.03600191f;
        z = rgb.x* 0.00000000f + rgb.y* 0.00000000f + rgb.z* 1.08905775f;
    }
    return float3(x, y, z);
}

float3 mtl_xyz_to_rgb(float3 xyz, int cs) {
    float r = xyz.x, g = xyz.y, b = xyz.z;
    if (cs == 0) {
        r = xyz.x* 1.64102338f + xyz.y*-0.32480329f + xyz.z*-0.23642470f;
        g = xyz.x*-0.66366286f + xyz.y* 1.61533159f + xyz.z* 0.01675635f;
        b = xyz.x* 0.01172189f + xyz.y*-0.00828444f + xyz.z* 0.98839486f;
    } else if (cs == 1) {
        r = xyz.x* 1.51667204f + xyz.y*-0.28147805f + xyz.z*-0.14696363f;
        g = xyz.x*-0.46491710f + xyz.y* 1.25142378f + xyz.z* 0.17488461f;
        b = xyz.x* 0.06484905f + xyz.y* 0.10913934f + xyz.z* 0.76141462f;
    } else if (cs == 2) {
        r = xyz.x* 1.78906548f + xyz.y*-0.48253384f + xyz.z*-0.20007578f;
        g = xyz.x*-0.63984859f + xyz.y* 1.39639986f + xyz.z* 0.19443229f;
        b = xyz.x*-0.04153153f + xyz.y* 0.08233536f + xyz.z* 0.87886840f;
    } else if (cs == 3) {
        r = xyz.x* 1.50921547f + xyz.y*-0.25059735f + xyz.z*-0.16881148f;
        g = xyz.x*-0.49154545f + xyz.y* 1.36124555f + xyz.z* 0.09728294f;
        b = xyz.x* 0.00000000f + xyz.y* 0.00000000f + xyz.z* 0.91822495f;
    }
    return float3(r, g, b);
}

float3 mtl_adapt_xyz_d60_to_d65(float3 xyz) {
    return float3(
        xyz.x* 0.987224008703f + xyz.y*-0.006113228607f + xyz.z* 0.015953288336f,
        xyz.x*-0.007598371812f + xyz.y* 1.001861484740f + xyz.z* 0.005330035791f,
        xyz.x* 0.003072577059f + xyz.y*-0.005095961511f + xyz.z* 1.081680603066f);
}

float3 mtl_adapt_xyz_d65_to_d60(float3 xyz) {
    return float3(
        xyz.x* 1.013034914650f + xyz.y* 0.006105257823f + xyz.z*-0.014970943627f,
        xyz.x* 0.007698230125f + xyz.y* 0.998163352118f + xyz.z*-0.005032038535f,
        xyz.x*-0.002841317432f + xyz.y* 0.004685156723f + xyz.z* 0.924506137458f);
}

float mtl_decode_transfer(float v, int cs) {
    if (cs==0) return v<=0.155251141553f ? (v-0.072905534196f)/10.540237741655f : exp2(v*17.52f-9.72f);
    if (cs==1) return v<=0.02740668f ? v/10.44426855f : exp2(v/0.07329248f-7.f)-0.0075f;
    if (cs==2) return v>0.149658f ? (pow(10.f,(v-0.385537f)/0.247190f)-0.052272f)/5.555556f : (v-0.092809f)/5.367655f;
    if (cs==3) return v<0.f ? v*0.113597208611f-0.018056996120f : (exp2(14.f*(v-0.092864125122f)/0.907135874878f+6.f)-64.f)/2231.82630907f;
    return v;
}

float mtl_encode_transfer(float v, int cs) {
    if (cs==0) return v<=0.0078125f ? v*10.540237741655f+0.072905534196f : (log2(v)+9.72f)/17.52f;
    if (cs==1) return v<=0.00262409f ? v*10.44426855f : (log2(v+0.0075f)+7.f)*0.07329248f;
    if (cs==2) return v>0.010591f ? 0.247190f*log10(5.555556f*v+0.052272f)+0.385537f : 5.367655f*v+0.092809f;
    if (cs==3) return v<-0.018056996120f ? (v+0.018056996120f)/0.113597208611f : (log2(2231.82630907f*v+64.f)-6.f)*0.907135874878f/14.f+0.092864125122f;
    return v;
}

float3 mtl_decode_transfer3(float3 rgb, int cs) {
    return float3(mtl_decode_transfer(rgb.x,cs), mtl_decode_transfer(rgb.y,cs), mtl_decode_transfer(rgb.z,cs));
}

float3 mtl_encode_transfer3(float3 rgb, int cs) {
    return float3(mtl_encode_transfer(rgb.x,cs), mtl_encode_transfer(rgb.y,cs), mtl_encode_transfer(rgb.z,cs));
}

// ─── Oklab ────────────────────────────────────────────────────────────────

constant float mtl_XYZ_to_LMS[9] = {
    0.8189330101f, 0.3618667424f,-0.1288597137f,
    0.0329845436f, 0.9293118715f, 0.0361456387f,
    0.0482003018f, 0.2643662691f, 0.6338517070f };
constant float mtl_LMS_to_Oklab[9] = {
    0.2104542553f, 0.7936177850f,-0.0040720468f,
    1.9779984951f,-2.4285922050f, 0.4505937099f,
    0.0259040371f, 0.7827717662f,-0.8086757660f };
constant float mtl_Oklab_to_LMS[9] = {
    1.0f, 0.3963377774f, 0.2158037573f,
    1.0f,-0.1055613458f,-0.0638541728f,
    1.0f,-0.0894841775f,-1.2914855480f };
constant float mtl_LMS_to_XYZ[9] = {
     1.2270138511f,-0.5577999807f, 0.2812561490f,
    -0.0405801784f, 1.1122568696f,-0.0716766787f,
    -0.0763812845f,-0.4214819784f, 1.5861632239f };

float3 mtl_mv33(constant float* m, float3 v) {
    return float3(m[0]*v.x+m[1]*v.y+m[2]*v.z,
                  m[3]*v.x+m[4]*v.y+m[5]*v.z,
                  m[6]*v.x+m[7]*v.y+m[8]*v.z);
}

float3 mtl_XYZ_to_Oklab(float3 xyz) {
    float3 lms = mtl_mv33(mtl_XYZ_to_LMS, xyz);
    float lx = lms.x<0.f ? -pow(-lms.x, 1.f/3.f) : pow(lms.x, 1.f/3.f);
    float ly = lms.y<0.f ? -pow(-lms.y, 1.f/3.f) : pow(lms.y, 1.f/3.f);
    float lz = lms.z<0.f ? -pow(-lms.z, 1.f/3.f) : pow(lms.z, 1.f/3.f);
    return mtl_mv33(mtl_LMS_to_Oklab, float3(lx, ly, lz));
}

float mtl_oklch_neutral_weight(float3 lch) {
    float scale = max(1.f, abs(lch.z));
    float inner = OKLAB_NEUTRAL_EPSILON * scale;
    return mtl_smoothstep(inner, inner * 3.f, lch.y);
}

float3 mtl_Oklab_to_XYZ(float3 lab) {
    float3 lp = mtl_mv33(mtl_Oklab_to_LMS, lab);
    return mtl_mv33(mtl_LMS_to_XYZ, float3(lp.x*lp.x*lp.x, lp.y*lp.y*lp.y, lp.z*lp.z*lp.z));
}

float3 mtl_OKLAB_to_OKLCH(float3 lab) {
    float C = sqrt(lab.y*lab.y + lab.z*lab.z);
    float h = atan2(lab.z, lab.y);
    if (h < 0.f) h += 2.f*PI;
    return float3(h/(2.f*PI), C, lab.x);
}

float3 mtl_OKLCH_to_OKLAB(float3 lch) {
    float h = lch.x * 2.f * PI;
    return float3(lch.z, lch.y*cos(h), lch.y*sin(h));
}

// ─── RGB Spherical ───────────────────────────────────────────────────────

float3 mtl_RGB_to_Spherical(float3 rgb) {
    float r=rgb.x, g=rgb.y, b=rgb.z;
    float is2=1.f/sqrt(2.f), is3=1.f/sqrt(3.f), is6=1.f/sqrt(6.f);
    float u=(2.f*r-g-b)*is6, v=(g-b)*is2, w=(r+g+b)*is3;
    float rho = sqrt(u*u+v*v+w*w);
    if (rho < EPSILON) return float3(0,0,0);
    float theta=atan2(v,u);
    if (theta<0.f) theta+=2.f*PI;
    float phi=atan2(sqrt(u*u+v*v),w);
    return float3(theta/(2.f*PI), phi, rho);
}

float3 mtl_Spherical_to_RGB(float3 c) {
    float t=c.x*2.f*PI, p=c.y, rho=c.z;
    if (rho < EPSILON) return float3(0,0,0);
    float u=rho*sin(p)*cos(t), v=rho*sin(p)*sin(t), w=rho*cos(p);
    float is2=1.f/sqrt(2.f), is3=1.f/sqrt(3.f), is6=1.f/sqrt(6.f);
    return float3(2.f*u*is6+w*is3,
                  -u*is6+v*is2+w*is3,
                  -u*is6-v*is2+w*is3);
}

float mtl_rgb_opponent_hue(float3 rgb, float fallback_hue) {
    float u=(2.f*rgb.x-rgb.y-rgb.z)/sqrt(6.f);
    float v=(rgb.y-rgb.z)/sqrt(2.f);
    if (length(float2(u,v))<1e-7f) return fallback_hue;
    return mtl_wrap_unit(atan2(v,u)/(2.f*PI));
}

float mtl_stable_blue_selector_hue(float3 rgb, float model_hue, int input_cs) {
    if (input_cs==0) return model_hue;

    float opponent_hue=mtl_rgb_opponent_hue(rgb,model_hue);
    const float blue_opponent_hue=2.f/3.f;
    const float blue_eq_center=257.f/360.f;
    const float mask_inner=25.f/360.f;
    const float mask_outer=75.f/360.f;
    float distance=abs(mtl_circular_delta(blue_opponent_hue,opponent_hue));
    float mask=1.f-mtl_smoothstep(mask_inner,mask_outer,distance);
    float stable_hue=
        mtl_wrap_unit(opponent_hue+blue_eq_center-blue_opponent_hue);
    return mtl_wrap_unit(
        model_hue+mtl_circular_delta(model_hue,stable_hue)*mask);
}

// ─── convert ──────────────────────────────────────────────────────────────

float3 mtl_convert(float3 rgb, int st, bool dir, int cs) {
    if (dir) {
        if (st >= 11) {
            float3 linear_rgb = mtl_decode_transfer3(rgb, cs);
            float3 xyz = mtl_rgb_to_xyz(linear_rgb, cs);
            if (cs == 0) xyz = mtl_adapt_xyz_d60_to_d65(xyz);
            return mtl_OKLAB_to_OKLCH(mtl_XYZ_to_Oklab(xyz));
        }
        if (st == 8) return mtl_RGB_to_Spherical(rgb);
        return rgb;
    }

    if (st >= 11) {
        float3 xyz = mtl_Oklab_to_XYZ(mtl_OKLCH_to_OKLAB(rgb));
        if (cs == 0) xyz = mtl_adapt_xyz_d65_to_d60(xyz);
        return mtl_encode_transfer3(mtl_xyz_to_rgb(xyz, cs), cs);
    }
    if (st == 8) return mtl_Spherical_to_RGB(rgb);
    return rgb;
}

// ─── LUT Sampler (Bilinear 1D) ────────────────────────────────────────────

float3 mtl_sample_lut_1d(const device float* lut, int num_points, float normalized_x) {
    float x = normalized_x * (float)num_points - 0.5f;
    if (x < 0.0f) x += (float)num_points;
    if (x >= (float)num_points) x -= (float)num_points;
    
    int i0 = (int)floor(x);
    int i1 = i0 + 1;
    float t = x - (float)i0;
    
    if (i0 < 0) i0 += num_points;
    if (i1 >= num_points) i1 -= num_points;
    
    int idx0 = i0 * 3;
    int idx1 = i1 * 3;
    
    return float3(
        lut[idx0 + 0] * (1.0f - t) + lut[idx1 + 0] * t,
        lut[idx0 + 1] * (1.0f - t) + lut[idx1 + 1] * t,
        lut[idx0 + 2] * (1.0f - t) + lut[idx1 + 2] * t
    );
}

// ─── RGB Direct equalizer ─────────────────────────────────────────────────

float mtl_rgb_direct_sat_value(float3 rgb) {
    float neutral = (rgb.x + rgb.y + rgb.z) / 3.0f;
    float3 chroma = rgb - float3(neutral);
    float chroma_mag = length(chroma);
    return clamp(chroma_mag / (abs(neutral) + chroma_mag + 1.0e-6f),
                 0.0f, 1.0f);
}

float3 mtl_apply_rgb_direct_equalizer(float3 rgb, const device float* lut,
                                      int lut_size) {
    float hue_normalized = mtl_rgb_opponent_hue(rgb, 0.0f);
    float3 eq = mtl_sample_lut_1d(lut, lut_size, hue_normalized);
    float h_delta = eq.x;
    float s_gain = eq.y;
    float l_delta = eq.z;

    if (abs(h_delta) < 1e-6f &&
        abs(s_gain - 1.0f) < 1e-6f &&
        abs(l_delta) < 1e-6f) {
        return rgb;
    }

    float neutral = (rgb.x + rgb.y + rgb.z) / 3.0f;
    float u = (2.0f * rgb.x - rgb.y - rgb.z) / sqrt(6.0f);
    float v = (rgb.y - rgb.z) / sqrt(2.0f);

    float angle = h_delta * 2.0f * PI;
    float ct = cos(angle);
    float st = sin(angle);
    float rotated_u = u * ct - v * st;
    float rotated_v = u * st + v * ct;
    u = rotated_u * max(0.0f, s_gain);
    v = rotated_v * max(0.0f, s_gain);

    float3 out = float3(
        neutral + 2.0f * u / sqrt(6.0f),
        neutral - u / sqrt(6.0f) + v / sqrt(2.0f),
        neutral - u / sqrt(6.0f) - v / sqrt(2.0f));

    float weight = mtl_rgb_direct_sat_value(out);
    float brightness_gain = max(0.0f, 1.0f + l_delta * weight);
    return out * brightness_gain;
}

// ─── Main Kernel — Parallel Pipeline (1 roundtrip) ────────────────────────

kernel void ColorEqualizerKernel(
    constant int& p_RenderX1           [[ buffer(0) ]],
    constant int& p_RenderY1           [[ buffer(1) ]],
    constant int& p_RenderWidth        [[ buffer(2) ]],
    constant int& p_RenderHeight       [[ buffer(3) ]],
    constant int& p_SrcBoundsX1        [[ buffer(4) ]],
    constant int& p_SrcBoundsY1        [[ buffer(5) ]],
    constant int& p_DstBoundsX1        [[ buffer(6) ]],
    constant int& p_DstBoundsY1        [[ buffer(7) ]],
    constant int& p_SrcRowBytes        [[ buffer(8) ]],
    constant int& p_DstRowBytes        [[ buffer(9) ]],
    constant int& p_InputPremultiplied [[ buffer(10) ]],
    constant int& p_OutputPremultiplied[[ buffer(11) ]],
    constant int& p_inputCS            [[ buffer(12) ]],
    constant int& p_spaceType          [[ buffer(13) ]],
    const device float* p_Input        [[ buffer(14) ]],
    device       float* p_Output       [[ buffer(15) ]],
    const device float* p_Lut          [[ buffer(16) ]],
    uint2 id [[ thread_position_in_grid ]])
{
    if (id.x >= (uint)p_RenderWidth || id.y >= (uint)p_RenderHeight) return;

    const int pixelX = p_RenderX1 + (int)id.x;
    const int pixelY = p_RenderY1 + (int)id.y;
    const int srcX = pixelX - p_SrcBoundsX1;
    const int srcY = pixelY - p_SrcBoundsY1;
    const int dstX = pixelX - p_DstBoundsX1;
    const int dstY = pixelY - p_DstBoundsY1;
    const int srcIndex =
        srcY * (p_SrcRowBytes / (int)sizeof(float)) + srcX * 4;
    const int dstIndex =
        dstY * (p_DstRowBytes / (int)sizeof(float)) + dstX * 4;

    const float alpha = p_Input[srcIndex+3];
    float3 rgb =
        float3(p_Input[srcIndex], p_Input[srcIndex+1], p_Input[srcIndex+2]);
    if (p_InputPremultiplied != 0) {
        if (alpha <= EPSILON) {
            p_Output[dstIndex+0] = 0.f;
            p_Output[dstIndex+1] = 0.f;
            p_Output[dstIndex+2] = 0.f;
            p_Output[dstIndex+3] = alpha;
            return;
        }
        rgb /= alpha;
    }

    if (p_spaceType == -1) {
        rgb = mtl_apply_rgb_direct_equalizer(rgb, p_Lut, 256);
        if (p_OutputPremultiplied != 0) rgb *= alpha;
        p_Output[dstIndex+0] = rgb.x;
        p_Output[dstIndex+1] = rgb.y;
        p_Output[dstIndex+2] = rgb.z;
        p_Output[dstIndex+3] = alpha;
        return;
    }

    // ── Single forward conversion ─────────────────────────────────────────
    float3 cs = mtl_convert(rgb, p_spaceType, true, p_inputCS);
    float hue_normalized = cs.x;
    if (p_spaceType>=11) {
        hue_normalized=
            mtl_stable_blue_selector_hue(rgb,hue_normalized,p_inputCS);
    }

    // Sample baked EQ adjustments
    float3 eq = mtl_sample_lut_1d(p_Lut, 256, hue_normalized);
    float h_delta = eq.x;
    float s_gain = eq.y;
    float l_delta = eq.z;

    if (abs(h_delta)<1e-6f && abs(s_gain-1.f)<1e-6f && abs(l_delta)<1e-6f) {
        if (p_OutputPremultiplied != 0) rgb *= alpha;
        p_Output[dstIndex+0]=rgb.x; p_Output[dstIndex+1]=rgb.y;
        p_Output[dstIndex+2]=rgb.z; p_Output[dstIndex+3]=alpha;
        return;
    }

    if (p_spaceType>=11) {
        float neutral_weight=mtl_oklch_neutral_weight(cs);
        if (neutral_weight<=0.f) {
            if (p_OutputPremultiplied != 0) rgb *= alpha;
            p_Output[dstIndex+0]=rgb.x; p_Output[dstIndex+1]=rgb.y;
            p_Output[dstIndex+2]=rgb.z; p_Output[dstIndex+3]=alpha;
            return;
        }
        h_delta*=neutral_weight;
        s_gain=1.f+(s_gain-1.f)*neutral_weight;
        l_delta*=neutral_weight;
    }

    // ── Apply Adjustments ────────────────────────────────────────────────
    cs.x += h_delta;
    if (cs.x < 0.f) cs.x += 1.f;
    if (cs.x >= 1.f) cs.x -= 1.f;

    if (p_spaceType==8) {
        float chroma_radius=cs.z*sin(cs.y);
        float neutral_axis=cs.z*cos(cs.y);
        if (abs(chroma_radius)<1e-7f) chroma_radius=0.f;
        chroma_radius=max(0.f,chroma_radius*s_gain);
        if (neutral_axis>1e-7f && chroma_radius>0.f) {
            float theta=cs.x*2.f*PI;
            float ct=cos(theta),st=sin(theta);
            float base=neutral_axis/sqrt(3.f);
            float kr=2.f*ct/sqrt(6.f);
            float kg=-ct/sqrt(6.f)+st/sqrt(2.f);
            float kb=-ct/sqrt(6.f)-st/sqrt(2.f);
            float limit=1e30f;
            if (kr<0.f) limit=min(limit,base/-kr);
            if (kg<0.f) limit=min(limit,base/-kg);
            if (kb<0.f) limit=min(limit,base/-kb);
            chroma_radius=min(chroma_radius,limit);
        }
        float weight=chroma_radius/(chroma_radius+abs(neutral_axis)+EPSILON);
        float brightness_gain=max(0.f,1.f+l_delta*weight);
        chroma_radius*=brightness_gain;
        neutral_axis*=brightness_gain;
        cs.y=atan2(chroma_radius,neutral_axis);
        cs.z=length(float2(chroma_radius,neutral_axis));

        // ── Inverse conversion ────────────────────────────────────────────
        rgb = mtl_convert(cs, p_spaceType, false, p_inputCS);
    } else if (p_spaceType>=11) {
        cs.y=max(0.f,cs.y*s_gain);
        float weight=cs.y;
        cs.z*=max(0.f,1.f+l_delta*weight);
        rgb = mtl_convert(cs, p_spaceType, false, p_inputCS);
    } else {
        cs.y=clamp(cs.y*s_gain,0.f,1.f);
        float weight=cs.y;
        cs.z*=max(0.f,1.f+l_delta*weight);
        rgb = mtl_convert(cs, p_spaceType, false, p_inputCS);
    }

    if (p_OutputPremultiplied != 0) rgb *= alpha;
    p_Output[dstIndex+0] = rgb.x;
    p_Output[dstIndex+1] = rgb.y;
    p_Output[dstIndex+2] = rgb.z;
    p_Output[dstIndex+3] = alpha;
}
)METAL";

// ─── Pipeline cache ──────────────────────────────────────────────────────

static std::mutex s_Mutex;
static std::unordered_map<id<MTLCommandQueue>, id<MTLComputePipelineState>>
    s_PipelineMap;

// ─── Entry point called from MCColorEqualizer.cpp ─────────────────────────

extern "C" bool RunMetalKernel(
    void *p_CmdQ, int p_RenderX1, int p_RenderY1, int p_RenderWidth,
    int p_RenderHeight, int p_SrcBoundsX1, int p_SrcBoundsY1,
    int p_DstBoundsX1, int p_DstBoundsY1, int p_SrcRowBytes,
    int p_DstRowBytes, int p_InputPremultiplied, int p_OutputPremultiplied,
    int p_inputCS, int p_spaceType, const float *p_Input, float *p_Output,
    const float *p_Lut) {
  @autoreleasepool {
    if (!p_CmdQ || !p_Input || !p_Output || !p_Lut ||
        p_RenderWidth <= 0 || p_RenderHeight <= 0 ||
        p_SrcRowBytes <= 0 || p_DstRowBytes <= 0) {
      return false;
    }

    id<MTLCommandQueue> queue = static_cast<id<MTLCommandQueue>>(p_CmdQ);
    id<MTLDevice> device = queue.device;

    // ── Build / retrieve cached pipeline ──────────────────────────────────
    id<MTLComputePipelineState> pipelineState;
    {
      std::lock_guard<std::mutex> lock(s_Mutex);
      auto it = s_PipelineMap.find(queue);
      if (it == s_PipelineMap.end()) {
        NSError *err = nil;
        MTLCompileOptions *options = [[MTLCompileOptions alloc] init];
        // RGB Spherical identifies the neutral axis through cancellation
        // (2R-G-B and G-B). Fast-math reassociation can destroy that
        // cancellation and turn neutral pixels into false chroma.
        if (@available(macOS 15.0, *)) {
          options.mathMode = MTLMathModeSafe;
        }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        // MTLMathModeSafe alone still allowed neutral-axis reassociation on
        // tested macOS 15 hardware. Keep the legacy switch explicitly off.
        options.fastMathEnabled = NO;
#pragma clang diagnostic pop

        id<MTLLibrary> library = [device newLibraryWithSource:@(kMetalSource)
                                                      options:options
                                                        error:&err];
        [options release];

        if (!library) {
          fprintf(stderr, "[MCColorEqualizer] Metal compile error: %s\n",
                  err.localizedDescription.UTF8String);
          return false;
        }

        id<MTLFunction> fn =
            [library newFunctionWithName:@"ColorEqualizerKernel"];
        [library release];

        if (!fn) {
          fprintf(stderr, "[MCColorEqualizer] Metal: could not find "
                          "'ColorEqualizerKernel'\n");
          return false;
        }

        pipelineState = [device newComputePipelineStateWithFunction:fn
                                                              error:&err];
        [fn release];

        if (!pipelineState) {
          fprintf(stderr, "[MCColorEqualizer] Metal pipeline error: %s\n",
                  err.localizedDescription.UTF8String);
          return false;
        }
        s_PipelineMap[queue] = pipelineState;
      } else {
        pipelineState = it->second;
      }
    }

    // ── Encode and dispatch ────────────────────────────────────────────────

    id<MTLBuffer> srcBuf =
        reinterpret_cast<id<MTLBuffer>>(const_cast<float *>(p_Input));
    id<MTLBuffer> dstBuf = reinterpret_cast<id<MTLBuffer>>(p_Output);

    id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
    id<MTLComputeCommandEncoder> encoder = [cmdBuf computeCommandEncoder];
    if (!cmdBuf || !encoder) return false;

    [encoder setComputePipelineState:pipelineState];

    [encoder setBytes:&p_RenderX1 length:sizeof(int) atIndex:0];
    [encoder setBytes:&p_RenderY1 length:sizeof(int) atIndex:1];
    [encoder setBytes:&p_RenderWidth length:sizeof(int) atIndex:2];
    [encoder setBytes:&p_RenderHeight length:sizeof(int) atIndex:3];
    [encoder setBytes:&p_SrcBoundsX1 length:sizeof(int) atIndex:4];
    [encoder setBytes:&p_SrcBoundsY1 length:sizeof(int) atIndex:5];
    [encoder setBytes:&p_DstBoundsX1 length:sizeof(int) atIndex:6];
    [encoder setBytes:&p_DstBoundsY1 length:sizeof(int) atIndex:7];
    [encoder setBytes:&p_SrcRowBytes length:sizeof(int) atIndex:8];
    [encoder setBytes:&p_DstRowBytes length:sizeof(int) atIndex:9];
    [encoder setBytes:&p_InputPremultiplied length:sizeof(int) atIndex:10];
    [encoder setBytes:&p_OutputPremultiplied length:sizeof(int) atIndex:11];
    [encoder setBytes:&p_inputCS length:sizeof(int) atIndex:12];
    [encoder setBytes:&p_spaceType length:sizeof(int) atIndex:13];
    [encoder setBuffer:srcBuf offset:0 atIndex:14];
    [encoder setBuffer:dstBuf offset:0 atIndex:15];
    
    // Using setBytes for LUT since 256 * 3 * 4 = 3072 bytes (under 4096 limit)
    [encoder setBytes:p_Lut length:256 * 3 * sizeof(float) atIndex:16];

    NSUInteger exeWidth = pipelineState.threadExecutionWidth;
    NSUInteger maxHeight = pipelineState.maxTotalThreadsPerThreadgroup / exeWidth;

    MTLSize threadsPerGroup = MTLSizeMake(exeWidth, maxHeight, 1);
    MTLSize threadgroups =
        MTLSizeMake(((NSUInteger)p_RenderWidth + exeWidth - 1) / exeWidth,
                    ((NSUInteger)p_RenderHeight + maxHeight - 1) / maxHeight,
                    1);

    [encoder dispatchThreadgroups:threadgroups
            threadsPerThreadgroup:threadsPerGroup];
    [encoder endEncoding];
    [cmdBuf commit];
    [cmdBuf waitUntilCompleted];
    if (cmdBuf.status == MTLCommandBufferStatusError) {
      fprintf(stderr, "[MCColorEqualizer] Metal command error: %s\n",
              cmdBuf.error.localizedDescription.UTF8String);
      return false;
    }
    return cmdBuf.status == MTLCommandBufferStatusCompleted;
  }
}
