#include <Windows.h>
#include <openvr.h>
#include "AC4VR_API.h"
#include "D3D12Hook.h"
#include "StereoRenderer.h"

#include <atomic>
#include <chrono>
#include <cwchar>
#include <mutex>
#include <thread>

namespace {

struct Vec3 {
    float x{};
    float y{};
    float z{};
};

struct Pose {
    Vec3 position{};
    Vec3 forward{0.0f, 0.0f, -1.0f};
    bool valid{};
};

struct VrFrame {
    Pose head{};
    Pose left{};
    Pose right{};
    bool leftGrip{};
    bool rightGrip{};
    bool leftTrigger{};
    bool rightTrigger{};
};

class GameBridge {
public:
    void registerCallbacks(const AC4VR_GameCallbacks& callbacks) {
        std::scoped_lock lock(mutex_);
        callbacks_ = callbacks;
    }

    void applyCamera(const VrFrame& frame) { dispatch(CallbackKind::Camera, frame); }
    void updatePointingRay(const VrFrame& frame) { dispatch(CallbackKind::PointingUi, frame); }
    void updateClimb(const VrFrame& frame) { dispatch(CallbackKind::Climbing, frame); }
    void updateShipControls(const VrFrame& frame) { dispatch(CallbackKind::ShipControls, frame); }

private:
    enum class CallbackKind { Camera, PointingUi, Climbing, ShipControls };

    static AC4VR_Frame convert(const VrFrame& frame) {
        const auto pose = [](const Pose& source) {
            return AC4VR_Pose{{source.position.x, source.position.y, source.position.z},
                              {source.forward.x, source.forward.y, source.forward.z}, source.valid ? 1 : 0};
        };
        return {pose(frame.head), pose(frame.left), pose(frame.right), frame.leftGrip ? 1 : 0,
                frame.rightGrip ? 1 : 0, frame.leftTrigger ? 1 : 0, frame.rightTrigger ? 1 : 0};
    }

    void dispatch(CallbackKind kind, const VrFrame& frame) {
        AC4VR_GameCallbacks callbacks;
        {
            std::scoped_lock lock(mutex_);
            callbacks = callbacks_;
        }
        AC4VR_FrameCallback callback = nullptr;
        switch (kind) {
        case CallbackKind::Camera: callback = callbacks.camera; break;
        case CallbackKind::PointingUi: callback = callbacks.pointingUi; break;
        case CallbackKind::Climbing: callback = callbacks.climbing; break;
        case CallbackKind::ShipControls: callback = callbacks.shipControls; break;
        }
        if (callback == nullptr) {
            return;
        }
        const auto publicFrame = convert(frame);
        callback(&publicFrame, callbacks.userData);
    }

    std::mutex mutex_;
    AC4VR_GameCallbacks callbacks_{};
};

class VrRuntime {
public:
    bool start() {
        vr::EVRInitError error = vr::VRInitError_None;
        system_ = vr::VR_Init(&error, vr::VRApplication_Scene);
        if (error != vr::VRInitError_None || system_ == nullptr) {
            system_ = nullptr;
            return false;
        }
        return true;
    }

    void stop() {
        if (system_ != nullptr) {
            vr::VR_Shutdown();
            system_ = nullptr;
        }
    }

    bool poll(VrFrame& frame) {
        if (system_ == nullptr) {
            return false;
        }

        vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount]{};
        system_->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, poses,
                                                  vr::k_unMaxTrackedDeviceCount);
        frame.head = makePose(poses[vr::k_unTrackedDeviceIndex_Hmd]);

        for (vr::TrackedDeviceIndex_t index = 0; index < vr::k_unMaxTrackedDeviceCount; ++index) {
            if (!poses[index].bPoseIsValid || system_->GetTrackedDeviceClass(index) != vr::TrackedDeviceClass_Controller) {
                continue;
            }
            const auto role = system_->GetControllerRoleForTrackedDeviceIndex(index);
            Pose& hand = role == vr::TrackedControllerRole_LeftHand ? frame.left : frame.right;
            hand = makePose(poses[index]);
            vr::VRControllerState_t state{};
            if (!system_->GetControllerState(index, &state, sizeof(state))) {
                continue;
            }
            const bool grip = (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_Grip)) != 0;
            const bool trigger = (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger)) != 0;
            if (role == vr::TrackedControllerRole_LeftHand) {
                frame.leftGrip = grip;
                frame.leftTrigger = trigger;
            } else if (role == vr::TrackedControllerRole_RightHand) {
                frame.rightGrip = grip;
                frame.rightTrigger = trigger;
            }
        }
        return frame.head.valid;
    }

private:
    static Pose makePose(const vr::TrackedDevicePose_t& tracked) {
        Pose pose{};
        pose.valid = tracked.bPoseIsValid;
        if (!pose.valid) {
            return pose;
        }
        const auto& matrix = tracked.mDeviceToAbsoluteTracking;
        pose.position = {matrix.m[0][3], matrix.m[1][3], matrix.m[2][3]};
        pose.forward = {-matrix.m[0][2], -matrix.m[1][2], -matrix.m[2][2]};
        return pose;
    }

    vr::IVRSystem* system_{};
};

std::atomic_bool running{false};
std::mutex lifecycleMutex;
std::thread worker;
VrRuntime runtime;
GameBridge bridge;
HMODULE moduleHandle{};

bool isEnabled() {
    wchar_t modulePath[MAX_PATH]{};
    if (moduleHandle == nullptr || GetModuleFileNameW(moduleHandle, modulePath, MAX_PATH) == 0) {
        return true;
    }
    const wchar_t* fileName = wcsrchr(modulePath, L'\\');
    if (fileName == nullptr) {
        fileName = modulePath;
    } else {
        ++fileName;
    }
    wchar_t configPath[MAX_PATH]{};
    wcsncpy_s(configPath, modulePath, fileName - modulePath);
    wcscat_s(configPath, L"ac4vr.ini");
    return GetPrivateProfileIntW(L"AC4VR", L"enabled", 1, configPath) != 0;
}

void run() {
    if (!runtime.start()) {
        running.store(false, std::memory_order_relaxed);
        return;
    }
    while (running.load(std::memory_order_relaxed)) {
        VrFrame frame{};
        if (runtime.poll(frame)) {
            bridge.applyCamera(frame);
            bridge.updatePointingRay(frame);
            bridge.updateClimb(frame);
            bridge.updateShipControls(frame);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    runtime.stop();
}

} // namespace

extern "C" AC4VR_API int AC4VR_CALL AC4VR_RegisterGameCallbacks(const AC4VR_GameCallbacks* callbacks) {
    if (callbacks == nullptr) {
        bridge.registerCallbacks({});
        return 1;
    }
    if (callbacks->apiVersion != AC4VR_API_VERSION) {
        return 0;
    }
    bridge.registerCallbacks(*callbacks);
    return 1;
}

extern "C" AC4VR_API void AC4VR_CALL AC4VR_SetD3D12Resources(AC4VR_D3D12Device device, AC4VR_D3D12CommandQueue commandQueue) {
    // Initialize stereo renderer with provided D3D12 resources
    if (device != nullptr && commandQueue != nullptr) {
        // Get swap chain from D3D12Hook (it will be populated by Present hook)
        auto& stereoRenderer = AC4VR::StereoRenderer::instance();
        
        // Note: Swap chain will be obtained through D3D12Hook after first Present() call
        // For now, we just store the device and command queue references
        // The full initialization will happen when the hook captures the swap chain
        
        // This is a placeholder - actual integration with D3D12 device requires
        // either the loader to provide the swap chain directly, or hooking Present()
    }
}

extern "C" AC4VR_API void AC4VR_CALL AC4VR_Start() {
    std::scoped_lock lock(lifecycleMutex);
    if (!isEnabled()) {
        return;
    }
    bool expected = false;
    if (!running.compare_exchange_strong(expected, true)) {
        return;
    }
    if (worker.joinable()) {
        worker.join();
    }
    worker = std::thread(run);
}

extern "C" AC4VR_API void AC4VR_CALL AC4VR_Stop() {
    std::scoped_lock lock(lifecycleMutex);
    running.store(false, std::memory_order_relaxed);
    if (worker.joinable() && worker.get_id() != std::this_thread::get_id()) {
        worker.join();
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        moduleHandle = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}

extern "C" AC4VR_API int AC4VR_CALL AC4VR_IsRunning() {
    return running.load(std::memory_order_relaxed) ? 1 : 0;
}