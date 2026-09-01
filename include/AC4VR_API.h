#pragma once

#ifdef _WIN32
#define AC4VR_API __declspec(dllexport)
#define AC4VR_CALL __cdecl
#else
#define AC4VR_API
#define AC4VR_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define AC4VR_API_VERSION 1u

typedef struct AC4VR_Vec3 {
    float x;
    float y;
    float z;
} AC4VR_Vec3;

typedef struct AC4VR_Pose {
    AC4VR_Vec3 position;
    AC4VR_Vec3 forward;
    int valid;
} AC4VR_Pose;

typedef struct AC4VR_Frame {
    AC4VR_Pose head;
    AC4VR_Pose left;
    AC4VR_Pose right;
    int leftGrip;
    int rightGrip;
    int leftTrigger;
    int rightTrigger;
} AC4VR_Frame;

typedef void(AC4VR_CALL* AC4VR_FrameCallback)(const AC4VR_Frame* frame, void* userData);

typedef struct AC4VR_GameCallbacks {
    unsigned int apiVersion;
    AC4VR_FrameCallback camera;
    AC4VR_FrameCallback pointingUi;
    AC4VR_FrameCallback climbing;
    AC4VR_FrameCallback shipControls;
    void* userData;
} AC4VR_GameCallbacks;

AC4VR_API int AC4VR_CALL AC4VR_RegisterGameCallbacks(const AC4VR_GameCallbacks* callbacks);
AC4VR_API void AC4VR_CALL AC4VR_Start(void);
AC4VR_API void AC4VR_CALL AC4VR_Stop(void);
AC4VR_API int AC4VR_CALL AC4VR_IsRunning(void);

#ifdef __cplusplus
}
#endif