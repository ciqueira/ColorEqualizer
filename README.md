# Color Equalizer

Color Equalizer allows selective hue, saturation, and brightness adjustment
across ten continuous color regions.

The plugin was inspired by the scene-referred workflow of darktable's Color
Equalizer module. The OFX adaptation keeps the central idea: change colors from
the pixel's original color, with continuous response between neighboring
regions and less tendency toward artificial edges in gradients.

Color Equalizer is distributed through
[MCNexus](https://mcnexus.app). Nexus provides distribution,
licensing, update delivery, and product support. MCNexus is the desktop
application used to activate, install, update, and manage the plugin.

> **New · Native MIDI control:** Color Equalizer sliders can be adjusted with a
> MIDI controller through MCNexus as Hub, without mouse input, manual mapping,
> or screen-resolution dependency. Connect the controller and open MCNexus. See
> [Native MIDI Control](#native-midi-control) for details.

## Included Plugins

| Plugin | Version | Distribution | Free Key | Support Project |
| --- | --- | --- | --- | --- |
| Color Equalizer | Current | OpenKey | [Get Key](https://get.mcnexus.app/color-equalizer?utm_source=github&utm_medium=readme&utm_content=colorequalizer-en) | [Become a Supporter](https://buy.mcnexus.app/color-equalizer?utm_source=github&utm_medium=readme&utm_content=colorequalizer-en) |

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

The free license permits one activation at a time.

Lost key: open the same claim link with the same GitHub account to recover the
issued license.

## Native MIDI Control

Color Equalizer sliders can be adjusted with a MIDI controller through MCNexus
as Hub without mouse input, manual mapping, screen-resolution dependency, or
Accessibility permissions. Connect the controller and open MCNexus.

- **Node focus:** active only when the Color Equalizer node is focused in the
  Inspector, with no input sent on native nodes (requires DaVinci Resolve
  Studio).
- **Banks and paging:** full parameter access through controller banks and
  virtual-bank paging for controllers with fewer knobs.
- **Sensitivity curve:** velocity-adaptive knob response scaled to each
  parameter's range, providing fine control on slow turns and immediate reset
  when reversing direction.
- **Reset and Undo:** press a knob to reset a parameter to default; continuous
  adjustments are grouped in Resolve's Undo history.

### NexControl MIDI HUB

- **Supported controllers:** built-in profiles for Midi Fighter Twister and
  Behringer X-TOUCH MINI, downloadable profiles for other models, and
  auto-detection for generic controllers.
- **Ports and hub control:** enable or release individual MIDI ports and pause
  or resume the hub from the menu bar (macOS) or system tray (Windows).

Implementation details, additional models, and profiles are available in the
[NexControl documentation](https://mcnexus.app/docs/developers/midi/).

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

[Purchase Color Equalizer Supporter](https://buy.mcnexus.app/color-equalizer?utm_source=github&utm_medium=readme&utm_content=colorequalizer-en)

Before purchasing, read the
[Supporter Terms](https://legal.magnociqueira.com.br/products/color-equalizer/terms/),
[Refund Policy](https://legal.magnociqueira.com.br/products/color-equalizer/refunds/),
[Privacy Policy](https://legal.magnociqueira.com.br/products/color-equalizer/privacy/),
and [Support Policy](https://legal.magnociqueira.com.br/products/color-equalizer/support/).
Markdown copies remain under [`legal/`](legal/README.md) for repository
discoverability. Messages about unrelated products are not included
automatically and require a separate marketing choice if one is offered in the
future.

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
