# Color Equalizer

### Selective color shaping with ten independent bands

Color Equalizer is an OFX color tool designed for precise, natural control of
hue, saturation, and brightness.

Inspired by the scene-referred color equalizer workflow popularized by
darktable, it expands conventional six-vector tools into ten smoothly
connected color regions:

```text
Red · Orange · Yellow · Lime · Green
Teal · Cyan · Blue · Purple · Magenta
```

The goal is simple: make selective color adjustments feel continuous and
musical, without turning a gradient into a collection of isolated corrections.

## Shape color, not masks

Each color region offers independent control over:

- **Hue** — move a color toward its neighboring tones.
- **Saturation** — strengthen or soften color intensity.
- **Brightness** — reshape the presence of a color without a separate key.

The ten controls are connected through periodic interpolation, so neighboring
bands blend around the complete hue circle. Adjustments remain responsive
without creating hard boundaries between red and magenta or between any other
adjacent colors.

## Two different views of color

Color Equalizer includes two processing models. They share the same controls
but produce different creative responses.

### RGB Spherical

RGB Spherical reorganizes the selected log signal around the neutral axis.
It is useful when you want a direct relationship between color direction,
distance from gray, and signal intensity.

Its opponent-space geometry helps preserve neutral tones while providing
smooth movement through highly saturated regions.

### OKLCH

OKLCH offers a perceptual view of color based on Oklab. It is especially
useful for controlled hue movement and intuitive separation between chroma and
lightness.

The implementation preserves the smooth compressed response of the original
Color Equalizer while using corrected color-space matrices and dedicated
handling for difficult blue and purple gradients.

## Designed for modern color pipelines

The plugin supports:

- ACES AP1 / ACEScct
- DaVinci Wide Gamut / Intermediate
- ARRI Wide Gamut 3 / LogC3 EI800
- ARRI Wide Gamut 4 / LogC4

RGB Spherical normalizes the selected log signal internally so that its
geometry reacts consistently across supported pipelines. OKLCH preserves the
encoded-signal response of the original equalizer.

## GPU processing

Pixel processing runs on:

- **Metal** on macOS
- **CUDA** on Windows and Linux

The two backends share the same color mathematics, equalizer response, alpha
handling, and numerical safeguards.

## Get Color Equalizer

Color Equalizer is available through
[MCNexus](https://github.com/ciqueira/MCNexus), the official application for
discovering and managing MC plugins.

MCNexus keeps the plugin, license, and available updates together in one place.
It also provides the current information for installation, activation,
compatibility, and product support.

### Get your license key

[![Claim Free License](https://img.shields.io/badge/Claim%20License-GitHub%20Login-2ea44f?logo=github)](https://bridge.magnociqueira.com.br/github/claim?t=colorequalizer-oss&tmpl=bf1b283c-c8ed-4608-91a9-348a342a55a4&sig=67251aabd72f21ba)

Use the button above to request your Color Equalizer key with your GitHub
account. The key is then managed through MCNexus.

## Project information

Color Equalizer is a source-available project. Public access to this repository
is intended for inspection, documentation, and technical transparency; it
does not make the project open-source software.

Licensing and required community notices are available in
[LICENSE.md](LICENSE.md) and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
