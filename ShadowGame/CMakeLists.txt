cmake_minimum_required(VERSION 3.20)
project(ShadowGame LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 可选：让输出统一到 build/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---- sources ----
set(SRC
    src/main.cpp
    src/background.cpp
    src/camera.cpp
    src/collision_detect.cpp
    src/LightSource.cpp
    src/object.cpp
    src/people.cpp
    src/scene.cpp
    src/shadow.cpp
)

add_executable(${PROJECT_NAME} ${SRC})

# 头文件在 src/ 下
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 可选：编译警告
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---- Dependencies ----
find_package(OpenGL REQUIRED)

# 优先使用 vcpkg / cmake config 包
find_package(glfw3 CONFIG QUIET)
find_package(GLEW CONFIG QUIET)
find_package(glm CONFIG QUIET)

if (glfw3_FOUND AND GLEW_FOUND AND glm_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL glfw GLEW::GLEW glm::glm)
else()
    message(STATUS "find_package(CONFIG) not fully found. Trying pkg-config fallback...")

    find_package(PkgConfig QUIET)
    if (NOT PkgConfig_FOUND)
        message(FATAL_ERROR "PkgConfig not found and CONFIG packages missing. Use vcpkg or install dev packages.")
    endif()

    pkg_check_modules(GLFW REQUIRED glfw3)
    pkg_check_modules(GLEW_PKG REQUIRED glew)
    pkg_check_modules(GLM_PKG REQUIRED glm)

    target_include_directories(${PROJECT_NAME} PRIVATE
        ${GLFW_INCLUDE_DIRS}
        ${GLEW_PKG_INCLUDE_DIRS}
        ${GLM_PKG_INCLUDE_DIRS}
    )
    target_link_directories(${PROJECT_NAME} PRIVATE
        ${GLFW_LIBRARY_DIRS}
        ${GLEW_PKG_LIBRARY_DIRS}
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        OpenGL::GL
        ${GLFW_LIBRARIES}
        ${GLEW_PKG_LIBRARIES}
    )
endif()

# ---- Copy shaders directory next to executable ----
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${SHADER_DIR}
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
)

# 运行时工作目录提示（VS 友好）
message(STATUS "Runtime shader dir will be copied to: $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders")
