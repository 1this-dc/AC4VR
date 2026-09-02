#include "StereoRenderer.h"
#include <DirectXMath.h>
#include <cmath>
#include <stdexcept>

namespace AC4VR {

StereoRenderer& StereoRenderer::instance() {
    static StereoRenderer s_instance;
    return s_instance;
}

StereoRenderer::~StereoRenderer() {
    shutdown();
}

bool StereoRenderer::initialize(ID3D12Device* device, IDXGISwapChain* swapChain, ID3D12CommandQueue* commandQueue) {
    if (initialized_) {
        return true;
    }

    if (device == nullptr || swapChain == nullptr || commandQueue == nullptr) {
        return false;
    }

    device_ = device;
    swapChain_ = swapChain;
    commandQueue_ = commandQueue;

    // Get recommended render target size from SteamVR
    compositor_ = vr::VRCompositor();
    if (compositor_ == nullptr) {
        return false;
    }

    uint32_t width, height;
    vr::VRSystem()->GetRecommendedRenderTargetSize(&width, &height);
    renderWidth_ = width;
    renderHeight_ = height;

    initialized_ = true;
    return true;
}

void StereoRenderer::shutdown() {
    if (device_) {
        device_->Release();
        device_ = nullptr;
    }
    if (swapChain_) {
        swapChain_->Release();
        swapChain_ = nullptr;
    }
    if (commandQueue_) {
        commandQueue_->Release();
        commandQueue_ = nullptr;
    }
    initialized_ = false;
}

bool StereoRenderer::submitFrame(const D3D12_CPU_DESCRIPTOR_HANDLE eyeTextures[2],
                                  const vr::HmdMatrix34_t poses[2]) {
    if (!initialized_ || compositor_ == nullptr) {
        return false;
    }

    // Submit left eye
    vr::Texture_t leftEyeTexture{};
    leftEyeTexture.handle = reinterpret_cast<void*>(eyeTextures[vr::Eye_Left].ptr);
    leftEyeTexture.eType = vr::TextureType_DirectX12;
    leftEyeTexture.eColorSpace = vr::ColorSpace_Linear;

    vr::EVRCompositorError leftError = compositor_->Submit(vr::Eye_Left, &leftEyeTexture, &poses[vr::Eye_Left]);
    if (leftError != vr::VRCompositorError_None) {
        return false;
    }

    // Submit right eye
    vr::Texture_t rightEyeTexture{};
    rightEyeTexture.handle = reinterpret_cast<void*>(eyeTextures[vr::Eye_Right].ptr);
    rightEyeTexture.eType = vr::TextureType_DirectX12;
    rightEyeTexture.eColorSpace = vr::ColorSpace_Linear;

    vr::EVRCompositorError rightError = compositor_->Submit(vr::Eye_Right, &rightEyeTexture, &poses[vr::Eye_Right]);
    return rightError == vr::VRCompositorError_None;
}

void StereoRenderer::getRenderTargetSize(uint32_t& width, uint32_t& height) const {
    width = renderWidth_;
    height = renderHeight_;
}

DirectX::XMMATRIX StereoRenderer::getProjectionMatrix(vr::EVREye eye, float near, float far) const {
    if (vr::VRSystem() == nullptr) {
        return DirectX::XMMatrixIdentity();
    }

    // Get HMD projection from SteamVR
    vr::HmdMatrix44_t steamvrProjection = vr::VRSystem()->GetProjectionMatrix(eye, near, far);

    // Convert to DirectX::XMMATRIX
    DirectX::XMMATRIX projection;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            projection.r[row].m128_f32[col] = steamvrProjection.m[row][col];
        }
    }

    return projection;
}

DirectX::XMMATRIX StereoRenderer::getEyeTransform(vr::EVREye eye) const {
    if (vr::VRSystem() == nullptr) {
        return DirectX::XMMatrixIdentity();
    }

    // Get eye-to-head transform from SteamVR
    vr::HmdMatrix34_t eyeMatrix = vr::VRSystem()->GetEyeToHeadTransform(eye);

    // Convert to DirectX::XMMATRIX (4x4 from 3x4)
    DirectX::XMMATRIX transform;
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 4; ++col) {
            transform.r[row].m128_f32[col] = eyeMatrix.m[row][col];
        }
    }
    transform.r[3] = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

    return transform;
}

} // namespace AC4VR
