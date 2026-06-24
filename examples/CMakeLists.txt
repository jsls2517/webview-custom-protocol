find_package(Threads REQUIRED)

set(COPYRIGHT "Copyright [year], Your Name")
set(VENDOR "Your Name")
set(DESCRIPTION "Example app made with webview")
set(SHARED_SOURCES)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    include(resources/macos/resources.cmake)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    include(resources/windows/resources.cmake)
endif()

add_executable(webview_example_basic_c MACOSX_BUNDLE WIN32)
target_sources(webview_example_basic_c PRIVATE basic.c ${SHARED_SOURCES})
target_link_libraries(webview_example_basic_c PRIVATE webview::core_static)

add_executable(webview_example_basic_cc MACOSX_BUNDLE WIN32)
target_sources(webview_example_basic_cc PRIVATE basic.cc ${SHARED_SOURCES})
target_link_libraries(webview_example_basic_cc PRIVATE webview::core)

add_executable(webview_example_bind_c MACOSX_BUNDLE WIN32)
target_sources(webview_example_bind_c PRIVATE bind.c ${SHARED_SOURCES})
target_link_libraries(webview_example_bind_c PRIVATE webview::core_static Threads::Threads)

if(MSVC)
    # Disable some warnings when using MSVC
    target_compile_definitions(webview_example_bind_c PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_executable(webview_example_bind_cc MACOSX_BUNDLE WIN32)
target_sources(webview_example_bind_cc PRIVATE bind.cc ${SHARED_SOURCES})
target_link_libraries(webview_example_bind_cc PRIVATE webview::core Threads::Threads)

add_executable(webview_example_assets_mapping_rpc MACOSX_BUNDLE WIN32)
target_sources(webview_example_assets_mapping_rpc PRIVATE assets_mapping_rpc.cc ${SHARED_SOURCES})
target_link_libraries(webview_example_assets_mapping_rpc PRIVATE webview::core Threads::Threads)

# Ship the example's web assets so the app:// mapping can resolve them at
# runtime. On macOS the assets are placed directly in the app bundle's
# Contents/Resources directory; on other platforms they are copied next to the
# executable.
set(EXAMPLE_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources")
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    file(GLOB EXAMPLE_ASSET_FILES "${EXAMPLE_ASSETS_DIR}/*")
    set_source_files_properties(${EXAMPLE_ASSET_FILES} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(webview_example_assets_mapping_rpc PRIVATE ${EXAMPLE_ASSET_FILES})
else()
    add_custom_command(TARGET webview_example_assets_mapping_rpc POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${EXAMPLE_ASSETS_DIR}"
                "$<TARGET_FILE_DIR:webview_example_assets_mapping_rpc>/resources")
endif()
