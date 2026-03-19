# BDG rec

Audio recorder for podcasts and long recording sessions.

Built with [JUCE](https://juce.com/) framework in C++.

## Features

- **Audio capture** with real-time VU metering
- **WAV recording** — mono, 24-bit, native sample rate
- **Crash-safe** — 5-minute chunk rotation, recover interrupted recordings
- **Disk space monitoring** — warnings and auto-stop when disk is full
- **DSP treatments** (post-recording):
  - Normalize (RMS + brick-wall limiter)
  - Noise Reduction (RNNoise neural network)
  - Multiband Compressor (3 bands)
  - De-Esser (4-8 kHz sibilance reduction)
- **Bilingual** — Portuguese (pt-BR) and English
- **Settings persistence** — device, folder, volume, treatments saved between sessions

## Download

Go to [Releases](../../releases) for the latest installers:
- **macOS** — DMG (universal binary: Apple Silicon + Intel)
- **Windows** — EXE (x64)

## Build from source

### Requirements
- CMake 3.22+
- macOS: Xcode Command Line Tools
- Windows: Visual Studio 2022 with "Desktop development with C++"

### macOS
```bash
./build-macos.sh
```
Output: `dist/BDG_REC_<version>_macOS.dmg`

### Windows
Open "Developer Command Prompt for VS 2022" and run:
```bat
build-windows.bat
```
Output: `dist\BDG_REC_<version>_Windows\BDG rec.exe`

## Tech Stack

- C++17
- JUCE 8 (fetched automatically via CMake)
- RNNoise (Xiph) for noise suppression
- GPLv3 license

## Screenshots

<img width="700" height="442" alt="BDG rec" src="https://github.com/user-attachments/assets/38afda96-a6a8-4d7b-a2fe-d9a94889a87f" />
