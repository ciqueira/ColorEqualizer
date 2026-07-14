# Color Equalizer

Color Equalizer allows selective hue, saturation, and brightness adjustment
across ten continuous color regions.

The plugin was inspired by the scene-referred workflow of darktable's Color
Equalizer module. The OFX adaptation keeps the central idea: change colors from
the pixel's original color, with continuous response between neighboring
regions and less tendency toward artificial edges in gradients.

Color Equalizer is distributed through
[MCNexus](https://github.com/ciqueira/MCNexus). Nexus provides distribution,
licensing, update delivery, and product support. MCNexus is the desktop
application used to activate, install, update, and manage the plugin.

## Included Plugins

| Plugin | Version | Distribution | Free Key | Support Project |
| --- | --- | --- | --- | --- |
| Color Equalizer | Current | OpenKey | [Get Key](https://bridge.magnociqueira.com.br/github/claim?t=colorequalizer-oss&tmpl=bf1b283c-c8ed-4608-91a9-348a342a55a4&sig=67251aabd72f21ba) | [Become a Supporter](https://bridge.magnociqueira.com.br/commerce/start?t=colorequalizer-oss&offer=color-equalizer-supporter) |

## Color Equalizer

Color Equalizer expands the conventional six-color workflow into ten connected
regions:

```text
Red · Orange · Yellow · Lime · Green
Teal · Cyan · Blue · Purple · Magenta
```

Each region has independent controls for:

- `Hue`: shifts hue toward neighboring tones.
- `Saturation`: increases or reduces color intensity.
- `Brightness`: changes the region's luminance presence without creating a
  separate key.

The `Hue Equalizer`, `Saturation Equalizer`, and `Brightness Equalizer` groups
provide per-color controls and a master control for scaling the group's effect.
Hue, saturation, and brightness are evaluated from the same chromatic position,
keeping continuity between adjacent bands.

In `RGB Spherical` and `OKLCH`, the plugin converts once into the selected
model, applies the three combined deltas, and converts back to RGB. In
`RGB Direct`, the correction is applied directly through the relationship
between RGB channels.

`Model / Space Type` defines how the color position is interpreted:

- `RGB Direct`: works directly with the relationship between RGB channels.
- `RGB Spherical`: uses a spherical reading around the neutral axis, with color
  direction, distance from gray, and intensity in the same model.
- `OKLCH`: uses a perceptual reading based on Oklab, useful for separating hue,
  chroma, and lightness.

Available input presets:

- ACES AP1 / ACEScct
- DaVinci Wide Gamut / Intermediate
- ARRI Wide Gamut 3 / LogC3
- ARRI Wide Gamut 4 / LogC4

## Processing Models

The processing model is parallel. The Hue, Saturation, and Brightness groups do
not form a serial stack where one adjustment feeds the next. All three groups
are precomputed into one LUT, sampled from the pixel's original color position,
and applied together in the same processing step.

```text
Input RGB -> original color position

original position -> Hue Equalizer        -> hue delta
original position -> Saturation Equalizer -> saturation gain
original position -> Brightness Equalizer -> brightness delta

Input RGB + combined deltas -> Output RGB
```

## Platform Support

Current builds support:

- macOS, Apple Silicon and compatible Intel Macs
- Windows x64

Supported processing backends:

- Metal on macOS
- CUDA on Windows

## Installation

1. Use the `Get Key` link above to generate the OpenKey license with a GitHub
   account.
2. Open MCNexus.
3. Activate Color Equalizer with the issued key.
4. Install or update the plugin through MCNexus.

Lost key: open the same claim link with the same GitHub account to recover the
issued license.

## Support the Project

Color Equalizer remains available free of charge with all currently published
plugin features. If it is useful in your work, you can optionally support its
maintenance and continued development.

The Color Equalizer Supporter benefit includes:

- priority private email support for 12 months; and
- operational email notices about Color Equalizer releases, compatibility,
  maintenance, security, and material service changes.

The purchase does not add exclusive plugin features. To deliver and associate
the Supporter benefit, Nexus may issue a new technical key or associate and
update an existing key. You do not need to obtain the free key before checkout;
existing users should use the same GitHub account and verified email.

[Purchase Color Equalizer Supporter](https://bridge.magnociqueira.com.br/commerce/start?t=colorequalizer-oss&offer=color-equalizer-supporter)

Before purchasing, read the [Supporter Terms](legal/TERMS.md),
[Refund Policy](legal/REFUND_POLICY.md), [Privacy Policy](legal/PRIVACY.md), and
[Support Policy](legal/SUPPORT_POLICY.md). Messages about unrelated products
are not included automatically and require a separate marketing choice if one
is offered in the future.

## License

Color Equalizer is source-available for review, documentation, and technical
transparency. Public access to this repository does not make the project
open-source software.

See:

- [LICENSE.md](LICENSE.md)
- [BINARY_LICENSE.md](BINARY_LICENSE.md)
- [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
- [Product legal documents](legal/README.md)

## Binary Releases

Official binary releases are distributed through Nexus and installed with
MCNexus. Use only official MCNexus or project release channels for binaries,
updates, and activation.
