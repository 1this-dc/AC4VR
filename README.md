# AC4VR

Native SteamVR plugin foundation for an Assassin's Creed IV: Black Flag Resynced VR mod.

## Current scope

- OpenVR runtime startup and shutdown.
- Headset and left/right controller pose polling.
- Controller grip and trigger state for interaction, climbing, and ship input layers.
- A versioned C API for the Resynced plugin to connect first-person camera, pointing UI, free climbing, and ship controls.

The bridge is intentionally unbound: Resynced's executable version, loader contract, renderer, and gameplay addresses are not present in this repository. Filling it with guessed offsets would be unsafe. The game-specific work must be implemented against the exact installed build, ideally through a documented mod-loader API or validated signatures.

## Build and package

Install Visual Studio 2022 with the Desktop C++ workload and CMake, then run from PowerShell:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
cmake --install build --config Release --prefix package
```

CMake downloads the OpenVR SDK during configuration. The resulting `package` folder is the drop-in payload: copy its contents into the Resynced plugin directory used by your installation. The folder contains `AC4VR.dll`, `ac4vr.ini`, and the API header for the loader-side adapter.

### Build without installing development tools

The repository includes a GitHub Actions workflow at `.github/workflows/build.yml`. Push this project to a GitHub repository, open the **Actions** tab, select **Build AC4VR**, and choose **Run workflow**. When it finishes, download the ZIP from the workflow run's **Artifacts** section. This uses GitHub's hosted Windows runner and does not install Visual Studio on your computer.

The first build still requires internet access on the hosted runner because CMake downloads the OpenVR SDK. GitHub may require a verified account for Actions on a new repository.

To publish this folder, create an empty GitHub repository first, then run these commands from the project folder:

```powershell
git init
git add .
git commit -m "Initial AC4VR project"
git branch -M main
git remote add origin https://github.com/YOUR_NAME/YOUR_REPOSITORY.git
git push -u origin main
```

After the push, open the repository's **Actions** tab and download the artifact from the completed **Build AC4VR** run. GitHub authentication may open a browser or require a personal access token; never put that token in this project or workflow file.

The Resynced loader must load `AC4VR.dll`, call `AC4VR_RegisterGameCallbacks` with `apiVersion=1`, and then call `AC4VR_Start`. Call `AC4VR_Stop`, then call `AC4VR_RegisterGameCallbacks(NULL)` before unloading the DLL. Callbacks must not call `AC4VR_Stop` themselves. The callback contract is in `include/AC4VR_API.h`; callbacks are invoked once per tracked VR frame.

## Start and play

1. Install SteamVR and confirm the headset works in SteamVR Home.
2. Start SteamVR before launching the game.
3. Copy the packaged files into the Resynced plugin directory, preserving `ac4vr.ini` beside `AC4VR.dll`.
4. Set `enabled=1` in `ac4vr.ini`.
5. Launch Assassin's Creed IV through the Resynced launcher.
6. Use the right controller trigger for pointing UI. Grip inputs are reserved for free climbing; ship controls are supplied by the loader's ship callback.
7. To disable the VR runtime, set `enabled=0` and restart the game.

This repository does not include the Resynced loader adapter. Until that adapter registers real game callbacks, the VR runtime can track controllers but cannot change the game's camera, UI, climbing, or ship behavior.

## Loader integration

1. Confirm the Resynced loader/plugin ABI and exact game executable hash.
2. Bind the camera callback to the camera transform and stereo render path.
3. Bind the pointing callback to the game's UI hit-test, trigger click, and haptic feedback.
4. Bind grip-relative hand motion to climbable-surface queries and player movement.
5. Map wheel, sail, camera, and combat actions through the ship callback.