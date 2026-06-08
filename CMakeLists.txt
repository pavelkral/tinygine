cmake_minimum_required(VERSION 3.16)
project(tinygine)

set(CMAKE_CXX_STANDARD 20)
add_compile_definitions(UNICODE _UNICODE)

# -----------------------------------------------------------------------------
# FIX OUTPUT DIRECTORIES
# -----------------------------------------------------------------------------

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG_UPPER )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} ${CMAKE_BINARY_DIR}/bin )
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib )
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib )
endforeach()

option(TRACY_ENABLE "Enable Tracy profiler" ON)

include(FetchContent)

# -----------------------------------------------------------------------------
# CONFIGURE JOLT PHYSICS LIB
# -----------------------------------------------------------------------------

message(STATUS "Configure JoltPhysics...")

set(JPH_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(JPH_BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(JPH_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(JPH_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(JPH_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(JPH_ENABLE_ASSERTS ON CACHE BOOL "" FORCE)
set(JPH_USE_SSE4_2 ON CACHE BOOL "" FORCE) 
set(JPH_DEBUG_RENDERER ON CACHE BOOL "" FORCE)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/JoltPhysics/Build/CMakeLists.txt")
    add_subdirectory(libs/JoltPhysics/Build libs/JoltPhysics/Build_Out)
    set_property(TARGET Jolt PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
else()
    message(FATAL_ERROR "error finding: libs/JoltPhysics/Build!...")
endif()

# -----------------------------------------------------------------------------
#  DIRECTX TOOL KIT & RmlUi & Freetype & ImGui
# -----------------------------------------------------------------------------

if(WIN32)
    message(STATUS "Downloading D3D12 Memory Allocator...")
    FetchContent_Declare(
        D3D12MemoryAllocator 
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator.git 
        GIT_TAG master
    )
    FetchContent_MakeAvailable(D3D12MemoryAllocator)

    message(STATUS "Downloading nlohmann/json...")
    FetchContent_Declare(json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(json)

    message(STATUS "Downloading DirectXTK (DX11)...")
    FetchContent_Declare(DirectXTK GIT_REPOSITORY https://github.com/microsoft/DirectXTK.git GIT_TAG main)
    message(STATUS "Downloading DirectXTK12 (DX12)...")
    FetchContent_Declare(DirectXTK12 GIT_REPOSITORY https://github.com/microsoft/DirectXTK12.git GIT_TAG main)
    FetchContent_MakeAvailable(DirectXTK DirectXTK12)

    message(STATUS "Downloading Freetype...")
    set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
    FetchContent_Declare(freetype GIT_REPOSITORY https://github.com/freetype/freetype.git GIT_TAG VER-2-13-2)
    FetchContent_MakeAvailable(freetype)
    if(NOT TARGET Freetype::Freetype)
        add_library(Freetype::Freetype ALIAS freetype)
    endif()

    message(STATUS "Downloading RmlUi...")
    set(RMLUI_ENABLE_SAMPLES OFF CACHE BOOL "" FORCE)
    set(RMLUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(RMLUI_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(rmlui GIT_REPOSITORY https://github.com/mikke89/RmlUi.git GIT_TAG master)
    FetchContent_MakeAvailable(rmlui)

    message(STATUS "Downloading ImGui (Docking)...")
    FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG docking)
    FetchContent_GetProperties(imgui)
    if(NOT imgui_POPULATED)
        FetchContent_Populate(imgui)
    endif()

    # ---  ImGuizmo ---
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/ImGuizmo/ImGuizmo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/ImGuizmo/GraphEditor.cpp
    )

    target_include_directories(imgui PUBLIC 
        ${imgui_SOURCE_DIR} 
        ${imgui_SOURCE_DIR}/backends
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/ImGuizmo    
    )
    target_link_libraries(imgui PRIVATE d3d11 d3d12 dxgi d3dcompiler)
endif()

find_package(Vulkan REQUIRED)

set(SOURCES
    libs/glad/src/glad.cpp
    libs/tracy/public/TracyClient.cpp
)

set(HEADERS
    libs/glad/include/KHR/khrplatform.h
    libs/glad/include/glad/glad.h
    libs/stb/image.h
    libs/stb/stb_image.h 
    include/vulkan/vk_mem_alloc.h
)

file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)


include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/glad/include
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/sdl2/include
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/tracy/public
    ${Vulkan_INCLUDE_DIRS}
    ${rmlui_SOURCE_DIR}/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/JoltPhysics
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/ImGuizmo
)

add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${ENGINE_SOURCES}
    ${ENGINE_HEADERS}
)

if(TRACY_ENABLE)
    target_compile_definitions(${PROJECT_NAME} PUBLIC TRACY_ENABLE)
endif()

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PUBLIC NOMINMAX)
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC
    JPH_PROFILE_ENABLED
    JPH_DEBUG_RENDERER
    JPH_USE_SSE4_2
)

if(WIN32)
    # DLL Copy commands
    set(DLL_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/glfw/glfw3.dll
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/dx/dxcompiler.dll
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/dx/dxc.exe
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/dx/dxil.dll
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/dx/dxv.exe
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/assimp/bin/assimp-vc143-mt.dll
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2/lib/x64/SDL2.dll
    )
    foreach(dll ${DLL_FILES})
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different ${dll} $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    endforeach()

    target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Jolt
        DirectXTK12
        D3D12MemoryAllocator
        nlohmann_json::nlohmann_json
        imgui 

        ${CMAKE_CURRENT_BINARY_DIR}/../../../libs/glfw/glfw3dll.lib
        ${CMAKE_CURRENT_BINARY_DIR}/../../../libs/assimp/lib/assimp-vc143-mt.lib
        ${CMAKE_CURRENT_BINARY_DIR}/../../../libs/SDL2/lib/x64/SDL2.lib
        ${CMAKE_CURRENT_BINARY_DIR}/../../../libs/SDL2/lib/x64/SDL2main.lib
        
        RmlUi::Core
        RmlUi::Debugger
        Freetype::Freetype 

        d3d11 d3d12 dxgi d3dcompiler dxguid
        Vulkan::Vulkan
        opengl32 glu32 ws2_32 dbghelp
    )

else()
    find_package(OpenGL REQUIRED)
    find_package(glfw3 REQUIRED)
    target_link_libraries(${PROJECT_NAME} glfw ${OPENGL_LIBRARIES} Jolt imgui)
endif()

set(DISTFILES
 
    # RHI shaders

)

file(COPY src/shaders/ DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders)
file(COPY assets/ DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets)