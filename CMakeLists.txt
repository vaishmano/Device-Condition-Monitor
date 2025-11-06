cmake_minimum_required(VERSION 3.12)
project(MiniGridMonitor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set wxWidgets path hints
set(wxWidgets_ROOT_DIR C:/wxWidgets-3.3.1)
set(wxWidgets_LIB_DIR C:/wxWidgets-3.3.1/lib/vc_x64_lib)

# Configure wxWidgets
set(wxWidgets_CONFIGURATION mswu)
set(wxWidgets_USE_STATIC ON)
set(wxBUILD_SHARED OFF)
set(wxUSE_UNICODE ON)

# Find required wxWidgets components
find_package(wxWidgets COMPONENTS core base REQUIRED)

# Add WIN32 target for Windows GUI application
add_executable(MiniGridMonitor WIN32 Demo.cpp)

# Remove WXUSINGDLL since we're using static libs
target_compile_definitions(MiniGridMonitor PRIVATE 
    _UNICODE
    UNICODE
    wxUSE_GUI=1
    __WXMSW__
)

# Include wxWidgets headers
target_include_directories(MiniGridMonitor PRIVATE ${wxWidgets_INCLUDE_DIRS})

# Link with wxWidgets libraries
target_link_libraries(MiniGridMonitor PRIVATE
    ${wxWidgets_LIBRARIES}
    comctl32 rpcrt4 gdiplus msimg32 uxtheme
)

# Write test tool
add_executable(write_test tools/write_test.cpp)