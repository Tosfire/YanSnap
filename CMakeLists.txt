cmake_minimum_required(VERSION 3.24)
project(YanSnap VERSION 1.1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT MSVC)
  message(FATAL_ERROR "YanSnap requires MSVC and the Windows SDK for Windows OCR.")
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_compile_options(/W4 /permissive- /utf-8 /EHsc)

add_executable(YanSnap WIN32
  src/main.cpp
  src/app/App.cpp
  src/app/SingleInstance.cpp
  src/app/HotkeyManager.cpp
  src/app/StartupManager.cpp
  src/app/TrayIcon.cpp
  src/common/AppIcon.cpp
  src/capture/DesktopCapture.cpp
  src/capture/WindowDetector.cpp
  src/annotation/Annotations.cpp
  src/annotation/UndoStack.cpp
  src/overlay/OverlayWindow.cpp
  src/overlay/Toolbar.cpp
  src/ocr/OcrRecognizer.cpp
  src/ocr/OcrPanel.cpp
  src/pin/PinWindow.cpp
  src/export/ClipboardExporter.cpp
  src/export/ImageComposer.cpp
  src/export/PngEncoder.cpp
  src/settings/Settings.cpp
  src/settings/SettingsWindow.cpp
  resources/YanSnap.rc
)

target_include_directories(YanSnap PRIVATE src "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(YanSnap PRIVATE
  UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
  WINVER=0x0A00 _WIN32_WINNT=0x0A00
)
target_link_libraries(YanSnap PRIVATE
  user32 gdi32 shell32 ole32 comctl32 comdlg32 windowscodecs dwmapi advapi32
  windowsapp uuid
)
target_link_options(YanSnap PRIVATE /MANIFEST:NO)

install(TARGETS YanSnap RUNTIME DESTINATION .)

include(CTest)
if(BUILD_TESTING)
  add_executable(GeometryTests tests/GeometryTests.cpp)
  target_include_directories(GeometryTests PRIVATE src)
  target_compile_definitions(GeometryTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  add_test(NAME GeometryTests COMMAND GeometryTests)

  add_executable(SettingsTests tests/SettingsTests.cpp
    src/settings/Settings.cpp
    src/export/PngEncoder.cpp
  )
  target_include_directories(SettingsTests PRIVATE src)
  target_compile_definitions(SettingsTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(SettingsTests PRIVATE shell32 ole32 windowscodecs uuid)
  add_test(NAME SettingsTests COMMAND SettingsTests)

  add_executable(AnnotationTests tests/AnnotationTests.cpp
    src/annotation/Annotations.cpp
    src/annotation/UndoStack.cpp
    src/export/ImageComposer.cpp
    src/export/PngEncoder.cpp
  )
  target_include_directories(AnnotationTests PRIVATE src)
  target_compile_definitions(AnnotationTests PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
  )
  target_link_libraries(AnnotationTests PRIVATE
    user32 gdi32 shell32 ole32 windowscodecs uuid
  )
  add_test(NAME AnnotationTests COMMAND AnnotationTests)
  set_tests_properties(AnnotationTests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  )
endif()

option(YANSNAP_BUILD_INTEGRATION_TESTS
  "Build interactive Windows integration test executables" ON)
if(YANSNAP_BUILD_INTEGRATION_TESTS)
  set(YANSNAP_OVERLAY_TEST_SOURCES
    src/common/AppIcon.cpp
    src/capture/DesktopCapture.cpp
    src/capture/WindowDetector.cpp
    src/annotation/Annotations.cpp
    src/annotation/UndoStack.cpp
    src/overlay/OverlayWindow.cpp
    src/overlay/Toolbar.cpp
    src/ocr/OcrPanel.cpp
    src/ocr/OcrRecognizer.cpp
    src/export/ClipboardExporter.cpp
    src/export/ImageComposer.cpp
    src/export/PngEncoder.cpp
    src/app/TrayIcon.cpp
  )

  add_executable(OverlayIntegration
    tests/OverlayIntegration.cpp
    ${YANSNAP_OVERLAY_TEST_SOURCES}
  )
  add_executable(OverlayPinIntegration
    tests/OverlayPinIntegration.cpp
    ${YANSNAP_OVERLAY_TEST_SOURCES}
  )
  add_executable(TestWindowHost tests/TestWindowHost.cpp)
  add_executable(WindowSelectionIntegration
    tests/WindowSelectionIntegration.cpp
    ${YANSNAP_OVERLAY_TEST_SOURCES}
  )
  add_executable(PinWindowIntegration
    tests/PinWindowIntegration.cpp
    src/pin/PinWindow.cpp
    src/export/ClipboardExporter.cpp
    src/export/PngEncoder.cpp
  )
  add_executable(UiPreview
    tests/UiPreview.cpp
    src/export/PngEncoder.cpp
    src/overlay/Toolbar.cpp
  )
  add_executable(StartupIntegration tests/StartupIntegration.cpp)
  add_dependencies(StartupIntegration YanSnap)
  add_executable(OcrIntegration
    tests/OcrIntegration.cpp
    src/ocr/OcrRecognizer.cpp
  )
  add_executable(SettingsPreview WIN32
    tests/SettingsPreview.cpp
    src/settings/SettingsWindow.cpp
    src/settings/Settings.cpp
    src/export/PngEncoder.cpp
    src/common/AppIcon.cpp
  )

  foreach(test_target IN ITEMS
      OverlayIntegration OverlayPinIntegration TestWindowHost
      WindowSelectionIntegration PinWindowIntegration UiPreview
      StartupIntegration OcrIntegration SettingsPreview)
    target_include_directories(${test_target} PRIVATE src "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_definitions(${test_target} PRIVATE
      UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
      WINVER=0x0A00 _WIN32_WINNT=0x0A00
    )
  endforeach()

  foreach(test_target IN ITEMS OverlayIntegration OverlayPinIntegration
      WindowSelectionIntegration)
    target_link_libraries(${test_target} PRIVATE
      user32 gdi32 shell32 ole32 comctl32 comdlg32
      windowscodecs dwmapi windowsapp uuid
    )
  endforeach()
  target_link_libraries(TestWindowHost PRIVATE user32 gdi32)
  target_link_libraries(PinWindowIntegration PRIVATE
    user32 gdi32 shell32 ole32 comdlg32 windowscodecs uuid
  )
  target_link_libraries(UiPreview PRIVATE
    user32 gdi32 shell32 ole32 windowscodecs uuid
  )
  target_link_libraries(StartupIntegration PRIVATE user32)
  target_link_libraries(OcrIntegration PRIVATE user32 gdi32 windowsapp)
  target_link_libraries(SettingsPreview PRIVATE
    user32 gdi32 shell32 ole32 comctl32 comdlg32 windowscodecs uuid
  )
endif()
