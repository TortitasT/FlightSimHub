# Adapted from FreeOpenKneeboard's third-party/windowsappsdk.cmake.
# Versions must stay in sync with the cppwinrt/wil overrides in vcpkg.json.
set(CPPWINRT_VERSION "2.0.240111.5" CACHE INTERNAL "")
set(WINDOWS_IMPLEMENTATION_LIBRARY_VERSION "1.0.250325.1" CACHE INTERNAL "")
set(WINDOWS_APP_SDK_VERSION "1.5.240607001" CACHE INTERNAL "")
set(WINDOWS_SDK_BUILDTOOLS_VERSION "10.0.22621.756" CACHE INTERNAL "")

function(target_link_nuget_packages TARGET)
  set_property(
    TARGET "${TARGET}"
    APPEND
    PROPERTY VS_PACKAGE_REFERENCES
    ${ARGN}
  )
  get_target_property(VS_PACKAGE_REFERENCES "${TARGET}" VS_PACKAGE_REFERENCES)
  list(REMOVE_DUPLICATES VS_PACKAGE_REFERENCES)
  set_target_properties(
    "${TARGET}"
    PROPERTIES
    VS_PACKAGE_REFERENCES "${VS_PACKAGE_REFERENCES}"
    VS_GLOBAL_NuGetTargetMoniker "native,Version=v0.0"
    VS_GLOBAL_RestoreProjectStyle "PackageReference"
  )
endfunction()

function(target_link_windows_app_sdk TARGET)
  target_link_nuget_packages(
    "${TARGET}"
    "Microsoft.Windows.CppWinRT_${CPPWINRT_VERSION}"
    "Microsoft.WindowsAppSDK_${WINDOWS_APP_SDK_VERSION}"
    "Microsoft.Windows.SDK.BuildTools_${WINDOWS_SDK_BUILDTOOLS_VERSION}"
    "Microsoft.Windows.ImplementationLibrary_${WINDOWS_IMPLEMENTATION_LIBRARY_VERSION}"
  )
  set(WINRT_OUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/Generated Files")
  if (NOT EXISTS "${WINRT_OUT_PATH}")
    file(MAKE_DIRECTORY "${WINRT_OUT_PATH}")
  endif ()
  target_include_directories("${TARGET}" PRIVATE "${WINRT_OUT_PATH}")
endfunction()
