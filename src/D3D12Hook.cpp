#include "D3D12Hook.h"
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <stdexcept>

// MinHook function pointer types
typedef HRESULT(WINAPI* PFN_CreateDXGIFactory1)(REFIID riid, void** ppFactory);
typedef HRESULT(STDMETHODCALLTYPE* PFN_Present)(IDXGISwapChain* pThis, UINT SyncInterval, UINT Flags);

namespace AC4VR {

// Static member initialization
decltype(&IDXGISwapChain::Present) D3D12Hook::originalPresent_ = nullptr;

D3D12Hook& D3D12Hook::instance() {
    static D3D12Hook s_instance;
    return s_instance;
}

D3D12Hook::~D3D12Hook() {
    shutdown();
}

bool D3D12Hook::initialize() {
    if (hooked_.load()) {
        return true;
    }

    // This is a simplified D3D12 hook that waits for the first Present() call
    // In production, you would use MinHook library (minhook.com) to hook at runtime
    // For now, we set up the hook infrastructure

    // Note: Actual hooking of Present() requires:
    // 1. Finding the IDXGISwapChain vftable
    // 2. Replacing the Present function pointer with our hook
    // 3. Calling the original when needed

    hooked_.store(true);
    return true;
}

void D3D12Hook::shutdown() {
    hooked_.store(false);
    // In production, unhook the Present function here
}

HRESULT STDMETHODCALLTYPE D3D12Hook::PresentHook(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags) {
    auto& hook = instance();

    // Capture the swap chain and device on first call
    if (hook.swapChain_ == nullptr) {
        hook.swapChain_ = swapChain;

        // Get device from swap chain
        HRESULT hr = swapChain->GetDevice(IID_PPV_ARGS(&hook.device_));
        if (SUCCEEDED(hr) && hook.device_) {
            // Get command queue from device
            // Note: D3D12 doesn't have a direct way to get the command queue from the device
            // The actual implementation would require storing it during device creation
            // or finding it through the active rendering context
        }
    }

    // Call original Present
    if (hook.originalPresent_) {
        return hook.originalPresent_(swapChain, syncInterval, flags);
    }

    return S_OK;
}

} // namespace AC4VR
