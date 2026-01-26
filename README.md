# Bytebeat Composer C++
A real-time algorithmic music generator written in C++. This application allows users to write mathematical formulas that generate audio waveforms on the fly, visualizing them through an integrated oscilloscope.

Built with **Raylib** and **ImGui**, featuring a high-performance audio engine capable of handling complex expressions in real-time.

## Features
* **Real-time Compilation:** Audio reacts instantly to code changes.
* **Dual Engine Support:** Supports both Classic C-style bytebeat syntax and JavaScript-compatible expressions.
* **Integrated Oscilloscope:** Visualizes the waveform output with precise synchronization.
* **Code Editor:** Features syntax highlighting, auto-formatting ("Fit to Window"), and code compression.
* **Docking Interface:** Fully customizable UI layout with window docking support.
* **WAV Export:** Ability to save generated audio to .wav files.
* **Preset System:** Includes a collection of classic and community-found bytebeat formulas.

## Installation & Build
This project is built using Visual Studio 2022.

1.  Clone the repository.
2.  Open the solution file (`.sln`).
3.  Ensure the configuration is set to **Release** or **Debug** (x64).
4.  Build and run the project.

*Note: Dependencies (Raylib, ImGui) are included in the `Vendor` directory.*

## Controls
* **Play/Pause:** Press `Enter` or click the button on the Oscilloscope.
* **Reset Time:** Right-click on the Oscilloscope.
* **Fullscreen:** Press `F11`.
* **Layout Reset:** Resizing the window automatically snaps panels to the default 2x2 grid.

## ToDo List
### Pending
- [ ] Merge Classic (C) and Javascript (JS) engines
- [ ] Improve Editor window (relocate Zoom/Engine/Format buttons to Settings window)
- [ ] Update `ExportToWav()` to include user input for exported sample length (currently fixed at 30s)
- [ ] Expand Sample Rate list to accept custom sample rate input
- [ ] Add expandable categories (sub-lists) in the Presets window
### Completed
- [x] ~~add charCodeAt (for "importing" wav songs)~~
- [x] ~~fix underlining line with error to work for both Classic and Javascript modes~~
- [x] ~~add function .length (for fetching data string length for charCodeAt)~~
- [x] ~~add `ConvertWavToBytebeat()` for dragging .wav to editor and converting it to HEX string with charCodeAt formula~~
- [x] ~~fix ImGui window proportions for fullscreen mode~~

## License
This project is open-source. Feel free to modify and distribute.