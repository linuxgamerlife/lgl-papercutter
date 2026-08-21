cmake_minimum_required(VERSION 3.20)

project(lgl-papercutter VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)
include(CTest)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

option(PAPERCUTTER_ENABLE_SANITIZERS
    "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(PAPERCUTTER_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
elseif(CMAKE_BUILD_TYPE STREQUAL "TSan" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-g -O1 -fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

add_library(papercutter_core STATIC
    src/core/compositionstate.cpp
    src/core/compositionstate.h
    src/core/imagejob.cpp
    src/core/imagejob.h
    src/app/settingsservice.cpp
    src/app/settingsservice.h
    src/app/backupservice.cpp
    src/app/backupservice.h
    src/processing/imageprocessor.cpp
    src/processing/imageprocessor.h
)
target_include_directories(papercutter_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(papercutter_core
    PUBLIC Qt6::Core Qt6::Gui Qt6::Concurrent
)

qt_add_executable(lgl-papercutter
    src/main.cpp
    src/ui/compositioncanvas.cpp
    src/ui/compositioncanvas.h
    src/ui/mainwindow.cpp
    src/ui/mainwindow.h
)
target_link_libraries(lgl-papercutter
    PRIVATE papercutter_core Qt6::Widgets
)

install(TARGETS lgl-papercutter RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/com.linuxgamerlife.lgl-papercutter.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/com.linuxgamerlife.lgl-papercutter.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES packaging/icons/hicolor/512x512/apps/lgl-papercutter.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps)

if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    qt_add_executable(compositionstate_test tests/compositionstate_test.cpp)
    target_link_libraries(compositionstate_test PRIVATE papercutter_core Qt6::Test)
    add_test(NAME compositionstate COMMAND compositionstate_test)

    qt_add_executable(imageprocessor_test tests/imageprocessor_test.cpp)
    target_link_libraries(imageprocessor_test PRIVATE papercutter_core Qt6::Test)
    add_test(NAME imageprocessor COMMAND imageprocessor_test)
endif()
