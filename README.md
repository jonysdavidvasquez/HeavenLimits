# HEAVEN LIMITS — DSP + GUI starter (v0.2)

A JUCE 8 / C++ stereo brickwall limiter project, configured for VST3 and Standalone builds. The custom GUI uses a brushed blue-silver metal theme inspired by vintage mastering rack hardware.

## Included
- Stereo audio processor and parameter state save/restore.
- Input Gain, Output Ceiling, Attack, Release, Lookahead, Stereo Link.
- Oversampling selector (1x / 2x / 4x / 8x) and True Peak switch.
- Bypass switch.
- Custom JUCE GUI with blue-metal panel, rotary controls, input/output LED meters and gain-reduction display.
- GitHub Actions workflow at `.github/workflows/build-windows.yml` to compile Windows Release artifacts.
- VST3 and Standalone CMake targets.
- Visual reference at `Design/Heaven_Limits_Blue_Metal_Reference.png`.

## Build locally on Windows
1. Install Visual Studio 2022 with **Desktop development with C++**.
2. Install Git and CMake 3.22 or newer.
3. Open PowerShell in this folder.
4. Run:
   ```powershell
   cmake -S . -B build -A x64
   cmake --build build --config Release --parallel
   ```
5. Find the VST3 under `build/HeavenLimits_artefacts/Release/VST3/`.

JUCE is fetched from its official Git repository by CMake, so the first configuration needs internet access.

## Build with GitHub Actions
1. Create a new GitHub repository and upload/push the complete contents of this folder.
2. Open the repository's **Actions** tab.
3. Run **Build Heaven Limits (Windows VST3)** using **Run workflow**, or push to `main` / `master`.
4. When the job finishes, open the successful run and download the `Heaven-Limits-Windows-Release` artifact.
5. Extract the ZIP. The VST3 bundle is inside the `VST3` folder.
6. Install/copy the `.vst3` bundle to your VST3 folder (commonly `C:\Program Files\Common Files\VST3`) and rescan plugins in FL Studio.

## Engineering status / caveats
This is a development build, not yet a release-ready mastering limiter. The custom UI is connected to the plugin parameters and reads the processor's input/output peak and gain-reduction telemetry. The DSP needs further validation for hard brickwall behavior, true-peak compliance, lookahead latency, bypass transitions, and oversampling behavior before commercial or critical mastering use. The current True Peak option uses oversampled peak detection; it should not be described as certified ITU-R BS.1770 true-peak compliance without a dedicated conformance test.

Test at a conservative monitoring level and verify latency compensation in the target DAW.
