# Bhop Surf Aim Training

## macOS and Linux

The macOS and Linux builds use CMake and fetch GLFW, FreeType, libcurl, and miniaudio during configuration. A working C++20 compiler, CMake 3.24 or newer, Git, and an internet connection are required.

### macOS

Install the build tools with Homebrew:

```sh
brew install cmake git
```

### Linux (Debian/Ubuntu)

Install the compiler, graphics/window-system development files, audio backend, and TLS dependencies:

```sh
sudo apt update
sudo apt install build-essential cmake git pkg-config libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libasound2-dev libssl-dev libwayland-dev libwayland-bin wayland-protocols
```

### Build

From the repository's `Project1` directory:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Run `build/AimTrainer` on Linux or `build/AimTrainer` on macOS. CMake copies the shaders and model resources beside the executable after building. macOS/Linux builds store settings and scores under the platform's application-data directory. The account API URL is configured in `auth_api_config.h`. Authenticated sessions are remembered locally without saving passwords; select **PLAY AS GUEST** on the sign-in screen to play without account synchronization.

The existing Visual Studio project remains the Windows build path.

## Downloadable macOS and Linux builds

GitHub Actions builds downloadable packages automatically for every push to `main` and every pull request. Open the repository's **Actions** tab, select **Build portable game packages**, open a successful run, and download the `AimTrainer-macos-linux` artifact from the **Artifacts** section. Extract the downloaded `AimTrainer-macos-linux.tar.gz` archive; it contains an executable `AimTrainer-macos-linux.command` launcher that detects macOS or Linux, extracts the matching build, and starts the game. On macOS, double-click the `.command` file. On Linux, run it with:

```sh
./AimTrainer-macos-linux.command
```

Users do not need CMake installed.

To publish packages under **Releases**, create and push a version tag:

```sh
git tag v1.0.0
git push origin v1.0.0
```

The workflow attaches `AimTrainer-macos-linux.tar.gz` plus the individual `AimTrainer-linux-x86_64.tar.gz` and `AimTrainer-macos-x86_64.tar.gz` archives to the GitHub release.
