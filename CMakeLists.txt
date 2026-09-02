cmake_minimum_required(VERSION 3.24)
project(AC4VR LANGUAGES CXX)

include(FetchContent)

FetchContent_Declare(
    openvr
    GIT_REPOSITORY https://github.com/ValveSoftware/openvr.git
    GIT_TAG v2.12.14
)
FetchContent_MakeAvailable(openvr)

add_library(AC4VR SHARED 
    src/AC4VR.cpp 
    src/D3D12Hook.cpp 
    src/StereoRenderer.cpp
)
target_compile_features(AC4VR PRIVATE cxx_std_20)
target_compile_definitions(AC4VR PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX AC4VR_EXPORTS OPENVR_BUILD_STATIC)
target_include_directories(AC4VR PRIVATE ${openvr_SOURCE_DIR}/headers ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    target_link_libraries(AC4VR PRIVATE openvr_api64 d3d12 dxgi dxguid)
else()
    target_link_libraries(AC4VR PRIVATE openvr_api d3d12 dxgi dxguid)
endif()
set_target_properties(AC4VR PROPERTIES OUTPUT_NAME "AC4VR")

add_custom_command(TARGET AC4VR POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/ac4vr.ini
        $<TARGET_FILE_DIR:AC4VR>/ac4vr.ini
)

    install(TARGETS AC4VR RUNTIME DESTINATION . LIBRARY DESTINATION . ARCHIVE DESTINATION .)
    install(FILES ac4vr.ini DESTINATION .)
    install(FILES include/AC4VR_API.h DESTINATION include)

if(MSVC)
    target_compile_options(AC4VR PRIVATE /W4 /permissive-)
endif()