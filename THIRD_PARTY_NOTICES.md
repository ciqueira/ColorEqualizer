# Third-Party Notices

This file lists third-party material actually used by Color Equalizer. It does
not license the project-owned code, which is governed by `LICENSE.md`.

Audit date: 2026-07-29.

## MCOpenNex SDK

Color Equalizer statically links the MCOpenNex SDK for anonymous update
discovery and product notices.

- Project: <https://github.com/ciqueira/MCOpenNex-SDK>
- Version: `0.1.x`
- License: Apache License 2.0
- Used in: `src/MCOpenNexPresenter.*` and the MC OFX build wrapper.

Binary distributions reproduce the MCOpenNex Apache license and NOTICE in the
OFX bundle. MCOpenNex does not activate licenses, authorize downloads, or
install updates.

## Oklab and OKLCH

The Oklab matrices and reference conversion structure are based on Björn
Ottosson's Oklab reference implementation:

- Author: Björn Ottosson
- Source: <https://bottosson.github.io/posts/oklab/>
- Used in: `src/ColorMath.h`, `src/MetalKernel.mm`, and `src/CudaKernel.cu`

The author states that the reference code is available in the public domain
and alternatively under the MIT License. Color Equalizer relies on the
public-domain grant and retains this notice for provenance.

## OpenFX

Color Equalizer is built against the shared OpenFX SDK used by MC OFX. The API
headers and support library are provided by The Open Effects Association Ltd.
under their retained BSD-style licenses.

- Project: <https://github.com/AcademySoftwareFoundation/openfx>
- Version: `OFX_Release_1.5.1`
- Support-library license: `MCPlugins/third_party/openfx/Support/LICENSE`
- Individual API headers retain their original notices.

Binary distributions must reproduce the applicable OpenFX copyright,
conditions, and disclaimer. The MC OFX build wrapper copies the shared SDK
support license into the OFX bundle as `OPENFX-BSD-3-CLAUSE.txt`.

## NVIDIA CUDA Runtime

Windows and Linux builds use the NVIDIA CUDA Runtime. The Windows build links
`cudart_static.lib`; the CUDA Toolkit EULA identifies the CUDA Runtime,
including the static runtime libraries, as distributable when incorporated
into an application that complies with NVIDIA's distribution requirements.

- CUDA Toolkit EULA:
  <https://docs.nvidia.com/cuda/eula/index.html>
- Used in: `src/CudaKernel.cu` and the Windows/Linux build configuration.

CUDA and NVIDIA are trademarks or registered trademarks of NVIDIA
Corporation. NVIDIA does not endorse Color Equalizer.

## Technical specifications

The project implements transfer functions and RGB/XYZ matrices from published
technical specifications. These are reference documents, not incorporated
software dependencies:

- ACEScg/AP1: <https://docs.acescentral.com/encodings/acescg/>
- ACEScct: <https://docs.acescentral.com/encodings/acescct/>
- DaVinci Wide Gamut / Intermediate:
  <https://documents.blackmagicdesign.com/InformationNotes/DaVinci_Resolve_17_Wide_Gamut_Intermediate.pdf>
- ARRI LogC3 / Wide Gamut 3:
  <https://www.arri.com/resource/blob/31918/66f56e6abb6e5b6553929edf9aa7483e/2017-03-alexa-logc-curve-in-vfx-data.pdf>
- ARRI LogC4 / Wide Gamut 4:
  <https://www.arri.com/resource/blob/278790/bea879ac0d041a925bed27a096ab3ec2/2022-05-arri-logc4-specification-data.pdf>

RGB Spherical, periodic interpolation, LUT construction, safety logic, and OFX
integration are project-owned implementations and require no additional
third-party attribution in this notice.

The 10-band equalizer interpolation is an independent periodic Fourier-series
implementation. Lagrange/Dirichlet trigonometric interpolation identities and
Aurélien Pierre's public discussions of hue-periodic interpolation are treated
as mathematical references only; no third-party interpolation code is
incorporated.
