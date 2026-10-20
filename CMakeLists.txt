cmake_minimum_required(VERSION 3.20)

# LGL Papercutter's maintained GTK 4 (gtkmm) application.
project(lgl-papercutter VERSION 0.4.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CTest)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wall> $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
                        $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic> $<$<COMPILE_LANGUAGE:CXX>:-Werror>)
endif()

option(PAPERCUTTER_ENABLE_SANITIZERS
    "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(PAPERCUTTER_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0>=4.20)
pkg_check_modules(ADWAITA REQUIRED IMPORTED_TARGET libadwaita-1>=1.6)
pkg_check_modules(JSON_GLIB REQUIRED IMPORTED_TARGET json-glib-1.0)
find_program(GLIB_COMPILE_RESOURCES glib-compile-resources REQUIRED)

set(PAPERCUTTER_GTK_VERSION ${PROJECT_VERSION})
set(PAPERCUTTER_REPO_ROOT ${PROJECT_SOURCE_DIR})

# --- resources -----------------------------------------------------------------

set(RESOURCE_XML ${PROJECT_SOURCE_DIR}/data/papercutter.gresource.xml)
set(RESOURCE_C ${CMAKE_CURRENT_BINARY_DIR}/papercutter-resources.c)
set(RESOURCE_DEPS
    ${PROJECT_SOURCE_DIR}/data/style.css
    ${PAPERCUTTER_REPO_ROOT}/packaging/icons/256x256/lgl-papercutter.png
    ${PAPERCUTTER_REPO_ROOT}/HELP.md)
add_custom_command(
    OUTPUT ${RESOURCE_C}
    COMMAND ${GLIB_COMPILE_RESOURCES} --generate-source --target=${RESOURCE_C}
            --sourcedir=${PROJECT_SOURCE_DIR}/data --sourcedir=${PAPERCUTTER_REPO_ROOT}
            ${RESOURCE_XML}
    DEPENDS ${RESOURCE_XML} ${RESOURCE_DEPS}
    VERBATIM)
set_source_files_properties(${RESOURCE_C} PROPERTIES COMPILE_OPTIONS "-w")

# --- core (no widgets; testable) ------------------------------------------------

add_library(papercutter_gtk_core STATIC
    src/core/composition.cpp
    src/core/composition.h
    src/core/imagejob.cpp
    src/core/imagejob.h
    src/core/imageinfo.cpp
    src/core/imageinfo.h
    src/core/numbering.cpp
    src/core/numbering.h
    src/core/processor.cpp
    src/core/processor.h
    src/core/settings.cpp
    src/core/settings.h
    src/core/theme.cpp
    src/core/theme.h
)
target_include_directories(papercutter_gtk_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(papercutter_gtk_core PUBLIC PkgConfig::GTKMM PkgConfig::JSON_GLIB)

# --- application ----------------------------------------------------------------

add_executable(lgl-papercutter
    ${RESOURCE_C}
    src/main.cpp
    src/ui/canvas.cpp
    src/ui/canvas.h
    src/ui/dialogs.cpp
    src/ui/dialogs.h
    src/ui/exportflow.cpp
    src/ui/exportflow.h
    src/ui/helpwindow.cpp
    src/ui/helpwindow.h
    src/ui/imageloader.cpp
    src/ui/imageloader.h
    src/ui/queuelist.cpp
    src/ui/queuelist.h
    src/ui/sidebar.cpp
    src/ui/sidebar.h
    src/ui/themewatcher.cpp
    src/ui/themewatcher.h
    src/ui/window.cpp
    src/ui/window.h
)
target_compile_definitions(lgl-papercutter PRIVATE
    PAPERCUTTER_VERSION="${PAPERCUTTER_GTK_VERSION}")
target_link_libraries(lgl-papercutter PRIVATE papercutter_gtk_core PkgConfig::ADWAITA)

install(TARGETS lgl-papercutter RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${PAPERCUTTER_REPO_ROOT}/packaging/com.linuxgamerlife.lgl-papercutter.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ${PAPERCUTTER_REPO_ROOT}/packaging/com.linuxgamerlife.lgl-papercutter.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
foreach(size 48 64 128 256)
    install(FILES ${PAPERCUTTER_REPO_ROOT}/packaging/icons/${size}x${size}/lgl-papercutter.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps)
endforeach()

# --- tests ----------------------------------------------------------------------

if(BUILD_TESTING)
    foreach(test composition numbering processor settings theme)
        add_executable(${test}_test tests/${test}_test.cpp)
        target_link_libraries(${test}_test PRIVATE papercutter_gtk_core)
        add_test(NAME ${test} COMMAND ${test}_test)
    endforeach()
    # g_test_skip() reports 77 under the TAP harness.
    set_tests_properties(composition numbering processor settings theme PROPERTIES SKIP_RETURN_CODE 77)
endif()
