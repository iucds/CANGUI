cmake_minimum_required(VERSION 3.15)

# 1. Force CMake to use vcpkg
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain" FORCE)

project(CANLogger)

# 2. Tell the compiler to use C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 3. Find the wxWidgets GUI library
find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

# 4. Tell CMake where the IXXAT SDK is located
# Point to the 'inc' folder for the header files (vci4.h / vciapi.h)
include_directories("C:/Program Files/HMS/Ixxat VCI/sdk/vci/inc")

# Point directly to the folder in your screenshot for the library file (vciapi.lib)
# Point to the vcinpl.lib file instead of vciapi.lib
find_library(IXXAT_LIB NAMES vcinpl PATHS "C:/Program Files/HMS/Ixxat VCI/sdk/vci/lib/x64/release" REQUIRED)

# 5. Create the executable (WIN32 hides the background console window)
add_executable(CANLogger WIN32 main.cpp)

# 6. Link everything together
target_include_directories(CANLogger PRIVATE ${wxWidgets_INCLUDE_DIRS})
target_link_libraries(CANLogger PRIVATE ${wxWidgets_LIBRARIES} ${IXXAT_LIB})
