set(
  CMAKE_CXX_STANDARD_LIBRARIES

  # Keep stuff that NuGet package references added
  "%(AdditionalDependencies)"
)

add_executable(
  FSHub-App
  WIN32
  app.manifest
  App.idl App.xaml App.xaml.cpp App.xaml.h
  EnvironmentPage.idl EnvironmentPage.xaml EnvironmentPage.xaml.cpp EnvironmentPage.xaml.h
  FilePicker.cpp FilePicker.h
  Globals.cpp Globals.h
  LaunchersPage.idl LaunchersPage.xaml LaunchersPage.xaml.cpp LaunchersPage.xaml.h
  Main.cpp
  MainWindow.idl MainWindow.xaml MainWindow.xaml.cpp MainWindow.xaml.h
)

include(winui3-boilerplate.cmake)

target_include_directories(
  FSHub-App
  PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}/Generated Files"
)

set_property(
  SOURCE App.xaml
  PROPERTY VS_XAML_TYPE
  "ApplicationDefinition"
)

target_link_libraries(
  FSHub-App
  PRIVATE
  FSHub-Lib
)

target_precompile_headers(
  FSHub-App
  PRIVATE
  pch.h
)

target_compile_definitions(
  FSHub-App
  PRIVATE
  DISABLE_XAML_GENERATED_MAIN
  MICROSOFT_WINDOWSAPPSDK_UNDOCKEDREGFREEWINRT_AUTO_INITIALIZE_LOADLIBRARY
)

set_target_properties(
  FSHub-App
  PROPERTIES
  OUTPUT_NAME "FlightSimHub"
)

add_custom_command(
  TARGET FSHub-App
  POST_BUILD
  COMMAND
  "${CMAKE_COMMAND}" -E copy_if_different
  "${CMAKE_SOURCE_DIR}/data/catalog.json"
  "$<TARGET_FILE_DIR:FSHub-App>/catalog.json"
)
