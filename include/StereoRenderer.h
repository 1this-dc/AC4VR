#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <openvr.h>
#include <memory>

namespace AC4VR {

// Manages stereo eye rendering and VR submission to SteamVR
class StereoRenderer {
public:
    static StereoRenderer& instance();

    // Initialize renderer with D3D12 device and swap chain
    bool initialize(ID3D12Device* device, IDXGISwapChain* swapChain, ID3D12CommandQueue* commandQueue);

    // Shutdown renderer
    void shutdown();

    // Submit a stereo frame to SteamVR
    // eyeTextures: array of 2 D3D12_CPU_DESCRIPTOR_HANDLE (left, right)
    // poses: array of 2 vr::HmdMatrix34_t (left, right eye transforms)
    bool submitFrame(const D3D12_CPU_DESCRIPTOR_HANDLE eyeTextures[2], 
                     const vr::HmdMatrix34_t poses[2]);

    // Get recommended render target size for each eye
    void getRenderTargetSize(uint32_t& width, uint32_t& height) const;

    // Get projection matrix for an eye
    DirectX::XMMATRIX getProjectionMatrix(vr::EVREye eye, float near = 0.01f, float far = 1000.0f) const;

    // Get eye pose (head position + eye offset)
    DirectX::XMMATRIX getEyeTransform(vr::EVREye eye) const;

    bool isInitialized() const { return initialized_; }

private:
    StereoRenderer() = default;
    ~StereoRenderer();

    bool initialized_{false};
    ID3D12Device* device_{nullptr};
    IDXGISwapChain* swapChain_{nullptr};
    ID3D12CommandQueue* commandQueue_{nullptr};
    vr::IVRCompositor* compositor_{nullptr};
    uint32_t renderWidth_{0};
    uint32_t renderHeight_{0};
};

} // namespace AC4VR
