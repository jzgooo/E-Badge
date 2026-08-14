# Waveshare ESP32-S3-Touch-AMOLED-1.75C Product Engineering Sample Program

**English** | [中文](README_zh.md)

ESP32-S3-Touch-AMOLED-1.75C ESP32-S3 1.75Cinch AMOLED Touch Watch Development Board, 466×466 Pixels, QSPI Interface, Onboard Dual Digital Microphones Array, ESP32 With Display

---

## Directory

| Path | Role |
| --- | --- |
| [`app/`](app/) | Base firmware on `main`: stable APIs + skeleton. Feature branches customize `src/`. |
| [`docs/PRD.md`](docs/PRD.md) | Product requirements (Codex quota display). |
| [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md) | Branch model: `main` is the base, other branches do follow-on work. |
| [`examples/`](examples/) | Waveshare Arduino / ESP-IDF samples. Treat as read-only reference. |
| [`Firmware/`](Firmware/) | Factory binary. |
| [`Schematic/`](Schematic/) | Hardware schematic PDF. |
| [`HARDWARE.md`](HARDWARE.md) | Board specifications. |

Build the **base** firmware with ESP-IDF 5.5+ from `app/` (see [`app/README.md`](app/README.md)). Product features (for example Codex quota UI) should be developed on a branch created from `main`. See [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md).

---

## 🔧 Configuration

Hardware specifications are documented in [HARDWARE.md](HARDWARE.md). You can also find detailed configuration information on the [product wiki](https://docs.waveshare.net/ESP32-S3-Touch-AMOLED-1.75C).

---

## 🛠️ Contributing

`main` holds the base firmware and public headers. Do follow-on work on a branch from `main`; prefer changing `src/` rather than `include/` APIs. Details: [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md).

1. Fork the repository.
2. Branch from `main`.
3. Commit your changes with clear descriptions.
4. Submit a pull request (merge to `main` only when the **base/API** should change).

---

## 🧩 Issues and Support

If you encounter any issues:

- This repository: [Issues](https://github.com/jzgooo/esp32-s3-badge/issues)
- Hardware board: [Waveshare Issues](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C/issues), or contact Waveshare with your order number.

---

## 📜 License

This repository is licensed under the Apache License License. See the `LICENSE` file for details.

---

## 🙌 Acknowledgments

- Waveshare for their excellent hardware platforms and software support
- The Espressif Team for their continuous support.
- Open-source contributors who make these projects possible.

---

Thank you for using Waveshare Electronics Products! 🚀