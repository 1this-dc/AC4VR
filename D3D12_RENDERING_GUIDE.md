# D3D12 VR Rendering Layer Implementation Guide

## Overview

This document describes the D3D12 rendering layer that has been added to AC4VR to enable stereo eye rendering and SteamVR submission for Assassin's Creed IV: Black Flag Resynced in VR.

## New Components

### 1. D3D12Hook (include/D3D12Hook.h, src/D3D12Hook.cpp)

**Purpose**: Intercepts the game's D3D12 rendering pipeline to capture device and swap chain references.

**Key Functions**:
- `initialize()` - Sets up the hook infrastructure
- `getDevice()` - Returns the captured D3D12 device
- `getCommandQueue()` - Returns the captured command queue  
- `getSwapChain()` - Returns the captured swap chain

**Implementation Notes**:
- Uses singleton pattern for global access
- Hooks `IDXGISwapChain::Present()` to capture device resources on first call
- For production use, integrate with MinHook library (https://github.com/TsudaKageyu/minhook) for robust function hooking

**Integration Point**: The Resynced loader should call `AC4VR_SetD3D12Resources()` with the game's D3D12 device and command queue after the game initializes its renderer.

### 2. StereoRenderer (include/StereoRenderer.h, src/StereoRenderer.cpp)

**Purpose**: Manages stereo eye rendering and frame submission to SteamVR's IVRCompositor.

**Key Functions**:
- `initialize(device, swapChain, commandQueue)` - Sets up rendering with D3D12 resources
- `getRenderTargetSize(width, height)` - Gets recommended render target size from SteamVR
- `getProjectionMatrix(eye, near, far)` - Gets per-eye projection matrix from SteamVR
- `getEyeTransform(eye)` - Gets per-eye transform (eye-to-head offset)
- `submitFrame(eyeTextures, poses)` - Submits rendered textures to SteamVR compositor

**Implementation Notes**:
- Queries SteamVR for optimal render target resolution
- Handles left/right eye submission independently
- Uses DirectX::XMMatrix for matrix operations
- Converts SteamVR matrices to DirectX format

### 3. Updated API (include/AC4VR_API.h)

**New Structures**:
```c
typedef void* AC4VR_D3D12Device;      // Opaque D3D12 device pointer
typedef void* AC4VR_D3D12CommandQueue; // Opaque command queue pointer
```

**New Function**:
```c
AC4VR_API void AC4VR_CALL AC4VR_SetD3D12Resources(
    AC4VR_D3D12Device device,
    AC4VR_D3D12CommandQueue commandQueue
);
```

**API Version**: Bumped from v1 to v2

## Integration Steps for Resynced Loader

### Step 1: Provide D3D12 Resources

After the game initializes its D3D12 device and command queue:

```cpp
#include "AC4VR_API.h"

// After game has created D3D12 device
ID3D12Device* gameDevice = ...; // Get from game
ID3D12CommandQueue* gameQueue = ...; // Get from game

AC4VR_SetD3D12Resources((AC4VR_D3D12Device)gameDevice, (AC4VR_D3D12CommandQueue)gameQueue);
```

### Step 2: Implement Camera Callback with Stereo Rendering

In the camera callback, render stereo eyes instead of single view:

```cpp
void CameraCallback(const AC4VR_Frame* frame, void* userData) {
    // Get stereo renderer instance
    auto& renderer = AC4VR::StereoRenderer::instance();
    
    // Get render target size
    uint32_t width, height;
    renderer.getRenderTargetSize(width, height);
    
    // Render left eye
    uint32_t leftRtIndex = ...; // Create render target for left eye
    DirectX::XMMATRIX leftProj = renderer.getProjectionMatrix(vr::Eye_Left, 0.01f, 1000.0f);
    DirectX::XMMATRIX leftView = renderer.getEyeTransform(vr::Eye_Left);
    // Render game scene with leftProj * leftView
    
    // Render right eye  
    uint32_t rightRtIndex = ...; // Create render target for right eye
    DirectX::XMMATRIX rightProj = renderer.getProjectionMatrix(vr::Eye_Right, 0.01f, 1000.0f);
    DirectX::XMMATRIX rightView = renderer.getEyeTransform(vr::Eye_Right);
    // Render game scene with rightProj * rightView
    
    // Get head pose from frame and apply to camera
    vr::HmdMatrix34_t headPose;
    // Convert frame.head to headPose format
    
    // Submit both eyes to SteamVR
    D3D12_CPU_DESCRIPTOR_HANDLE eyeTextures[2] = {leftRt, rightRt};
    vr::HmdMatrix34_t eyePoses[2] = {leftPose, rightPose};
    renderer.submitFrame(eyeTextures, eyePoses);
}
```

### Step 3: Register Updated Callbacks

```cpp
AC4VR_GameCallbacks callbacks = {};
callbacks.apiVersion = AC4VR_API_VERSION; // Now version 2
callbacks.camera = CameraCallback;  // With stereo rendering
callbacks.pointingUi = PointingCallback;
callbacks.climbing = ClimbingCallback;
callbacks.shipControls = ShipCallback;
callbacks.userData = userData;

AC4VR_RegisterGameCallbacks(&callbacks);
```

## Rendering Pipeline

1. **Capture Phase**: D3D12Hook captures device/queue on first Present() call
2. **Initialization Phase**: Resynced loader calls AC4VR_SetD3D12Resources() with device and queue
3. **Render Phase**: For each frame:
   - Camera callback renders left eye to render target
   - Camera callback renders right eye to render target
   - Submits both textures + poses to SteamVR via IVRCompositor::Submit()
4. **Presentation Phase**: SteamVR compositor displays the stereo frames to headset

## Performance Optimization

- **Recommended Render Target Size**: Query from `IVRSystem::GetRecommendedRenderTargetSize()` (typically 2016x2240 for modern headsets)
- **Culling Optimization**: Render unique frustums for each eye rather than duplicating full scene
- **Eye Projection Offset**: Use per-eye transform matrices to position cameras correctly
- **Async Timewarp**: SteamVR handles eye correction via distortion shaders

## Edward's Hands and Character Visibility

To render Edward's hands and body:

1. **Hand Models**: 
   - Attach hand mesh to controller poses from `AC4VR_Frame::left` and `AC4VR_Frame::right`
   - Apply controller rotation/position to hand bone hierarchy
   
2. **Body/Outfit**:
   - Render Edward's torso/legs using camera position from head pose with downward-facing view
   - Place character model at ground level, camera height at ~1.6m above ground
   - Update body animation based on head and hand tracking

3. **Camera Setup**:
   - First-person camera at head pose position
   - Look direction from frame.head.forward (but head tracking overrides this)
   - Hands rendered at controller poses with proper IK constraints

## Next Steps

1. Install or link MinHook library for robust function hooking
2. Implement D3D12 render target creation and descriptor heap management
3. Add stereo framebuffer management in StereoRenderer
4. Integrate with Resynced loader to provide D3D12 device context
5. Implement hand mesh rendering at controller poses
6. Test eye tracking and distortion correction

## Building

From the project root:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release  
cmake --install build --config Release --prefix package
```

The compiled `AC4VR.dll` will be in the `package` folder.

## Notes

- D3D12Hook currently uses a simplified approach; MinHook integration recommended for production
- StereoRenderer requires access to SteamVR's IVRCompositor interface
- Render targets should be created in DXGI_FORMAT_R8G8B8A8_UNORM or HDR format depending on headset
- Frame submission should happen after both eyes are rendered but before game's normal Present() call
