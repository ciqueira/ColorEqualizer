# Architecture

## Runtime responsibilities

```text
┌──────────────────────────── OFX host ────────────────────────────┐
│ Fetch source/destination images, render window, alpha mode,      │
│ selected input signal, model, and ten-band control values.       │
└───────────────────────────────┬───────────────────────────────────┘
                                │
                    ┌───────────▼───────────┐
                    │ CPU / plugin wrapper  │
                    │ Build 256×3 LUT       │
                    │ Select GPU backend    │
                    └───────────┬───────────┘
                                │
               ┌────────────────┴────────────────┐
               │                                 │
        ┌──────▼──────┐                   ┌──────▼──────┐
        │ Metal/macOS │                   │ CUDA/Win-Lin│
        └──────┬──────┘                   └──────┬──────┘
               └────────────────┬────────────────┘
                                │
                    ┌───────────▼───────────┐
                    │ Model conversion      │
                    ├───────────────────────┤
                    │ RGB Spherical:        │
                    │ encoded RGB directly  │
                    │                       │
                    │ OKLCH: encoded RGB    │
                    │ → linear RGB → XYZ    │
                    │ D65 → Oklab           │
                    │ → OKLCH               │
                    └───────────┬───────────┘
                                │
                    ┌───────────▼───────────┐
                    │ Hue-indexed LUT       │
                    │ H / S / brightness    │
                    └───────────┬───────────┘
                                │
                    ┌───────────▼───────────┐
                    │ Inverse conversion    │
                    │ Restore alpha         │
                    └───────────────────────┘
```

## Main files

| File | Responsibility |
| --- | --- |
| `src/MCColorEqualizer.cpp` | OFX registration, parameters, image setup, LUT construction, and GPU dispatch |
| `src/ColorMath.h` | Host-side color mathematics and numerical test reference |
| `src/MetalKernel.mm` | macOS Metal pixel-processing implementation |
| `src/CudaKernel.cu` | Windows/Linux CUDA pixel-processing implementation |
| `src/EQParams.h` | Shared parameter layout |
| `../tests/` | Shared host-side OKLCH numerical and regression tests |

## Model domains

### RGB Spherical

RGB Spherical works directly on the selected encoded RGB signal. The input-space
choice does not alter its spherical geometry; it documents the signal supplied
by the host and is used by the OKLCH path.

### OKLCH

OKLCH decodes the transfer function selected by `Input Space` before applying
the corresponding linear RGB gamut matrix:

```text
encoded RGB
  → scene-linear RGB
  → CIE XYZ D65
  → Oklab
  → OKLCH
  → equalizer
  → inverse path
  → encoded RGB
```

The current implementation keeps the corrected AP1 and AWG4 matrices, modern
GPU infrastructure, and an independent periodic Fourier-series implementation
for the 10-band equalizer interpolation. OKLCH chroma remains unbounded; no
arbitrary upper chroma clamp is applied.

ACES AP1 produces D60-relative XYZ, so that path applies Bradford adaptation
from D60 to D65 before Oklab and the inverse D65-to-D60 adaptation on output.
Near the neutral axis, corrections fade continuously between an inner protected
region and full-strength chroma. The implementation never snaps Oklab `a` and
`b` to zero, and the protected region bypasses the round trip exactly.

## GPU parity

Metal and CUDA implement the same model, transfer, LUT-sampling, safety, render
window, stride, and alpha logic.

Metal disables fast-math because reassociation breaks the neutral-axis
cancellation used by RGB Spherical. CUDA is compiled with
conservative floating-point flags for the same reason.

The manual `Validate CUDA` workflow compiles the Windows/CUDA bundle without
uploading or publishing an artifact.

## CI and release gate

1. Run host-side numerical tests.
2. Validate that `src/VERSION` is greater than the latest `eq-v*` tag.
3. Build macOS and/or Windows artifacts.
4. Publish the release after successful platform builds.
