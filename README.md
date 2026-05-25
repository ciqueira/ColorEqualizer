# Color Equalizer (Selective Isolation)

10-band chromatic isolation.
Inspired by darktable’s scene-referred processing architecture, Color Equalizer utilizes guided filters and multiple polarized color spaces for selective manipulation of hue, saturation, and brightness, minimizing common artifacts found in curve tools.

---

## Signal Engineering and Key Features

Color Equalizer was developed to maintain image integrity even under extreme corrections on the timeline, internally managing signal consistency directly through the plugin.

* **Guided Filter:** An adaptive smoothing algorithm that protects transitions between modified areas and the rest of the image, reducing pixel fragmentation and chromatic noise.
* **10-Band Resolution:** An expansion of traditional 6-vector control into 10 independent, interpolated bands: Red, Orange, Yellow, Lime, Green, Teal, Cyan, Blue, Purple, and Magenta.
* **Independent Modules:** Processing is divided into three layers for complete control over the image signal:
    * **Hue:** Precise angular shift of the source color.
    * **Saturation:** Intensification or attenuation of color purity per band.
    * **Brightness:** Isolated photometric gain adjustment per color vector.

---

## Available Models and Color Spaces

Switch the mathematical basis of calculation to match your pipeline requirements:

* **Chen (Spherical Model):** Maps the RGB cube into spherical coordinates. It eliminates typical HSV visual distortions, delivering smoother, more harmonious gradients.
* **Reuleaux (Filmic):** A cylindrical/spherical model for film characterization (inspired by the *Cone Coords* concept). Delivers natural color transitions with a subtractive behavior.
* **HCL (Perceptual):** Focused on human visual perception with weights mapped to Rec.709. Ensures an exact separation between color purity (*Chroma*) and actual brightness (*Luminance*).
* **OKLCH (Uniformity):** A polar representation of the Oklab color space. Designed to offer superior numerical stability, correcting the classic hue shift in the blue channel (common in CIELAB) for predictable gradients.

---

## Development & Installation

The plugin is currently distributed and updated through the **[Nexus](https://github.com/ciqueira/MCNexus)** ecosystem, supporting seamless license activation, version switching, and multi-platform deployment (Windows & macOS).

Get Your Free License

Claim your license key in seconds — no forms, no waiting.

[![Claim Free License](https://img.shields.io/badge/Claim%20License-GitHub%20Login-2ea44f?logo=github)](https://bridge.magnociqueira.com.br/github/claim?t=colorequalizer-oss&tmpl=bf1b283c-c8ed-4608-91a9-348a342a55a4&sig=67251aabd72f21ba)

**Steps:**
1. Click the badge above
2. Authorize with your GitHub account (read-only: name + email)
3. Your license key appears — copy it
4. Paste the key in the plugin's activation screen

> **Lost your key?** Click the link again — same account, same key, always.
