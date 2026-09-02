#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <memory>
#include <atomic>

namespace AC4VR {

// Manages D3D12 Present() hook to capture rendering pipeline
class D3D12Hook {
public:
    static D3D12Hook& instance();

    // Initialize hook: must be called once at startup
    bool initialize();

    // Shutdown hook
    void shutdown();

    // Query if Present() has been hooked
    bool isHooked() const { return hooked_.load(); }

    // Get the hooked device (populated after first Present call)
    ID3D12Device* getDevice() const { return device_; }

    // Get the hooked command queue (populated after first Present call)
    ID3D12CommandQueue* getCommandQueue() const { return commandQueue_; }

    // Get the hooked swap chain (populated after first Present call)
    IDXGISwapChain* getSwapChain() const { return swapChain_; }

private:
    D3D12Hook() = default;
    ~D3D12Hook();

    // Static hook function passed to MinHook
    static HRESULT STDMETHODCALLTYPE PresentHook(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags);

    // Original Present function pointer
    static decltype(&IDXGISwapChain::Present) originalPresent_;

    // Instance state
    std::atomic<bool> hooked_{false};
    ID3D12Device* device_{nullptr};
    ID3D12CommandQueue* commandQueue_{nullptr};
    IDXGISwapChain* swapChain_{nullptr};
};

} // namespace AC4VR
