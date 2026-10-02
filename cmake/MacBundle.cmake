include_guard(GLOBAL)

function(encoder_mac_bundle target display_name component)
  set_target_properties(${target} PROPERTIES
    MACOSX_BUNDLE TRUE
    OUTPUT_NAME "${display_name}"
    MACOSX_BUNDLE_BUNDLE_NAME "${display_name}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.mihamix328.encoder.${component}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_ICON_FILE "app_icon.icns"
    MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/cmake/MacInfo.plist.in"
  )
  if(component STREQUAL "client")
    set(config "${PROJECT_SOURCE_DIR}/config/client.conf.in")
    # The resource must be named client.conf rather than client.conf.in.
    configure_file("${config}" "${CMAKE_CURRENT_BINARY_DIR}/client.conf" COPYONLY)
    set(config "${CMAKE_CURRENT_BINARY_DIR}/client.conf")
    file(READ "${config}" defaults)
    string(REPLACE "server_host=127.0.0.1" "server_host=orangepi3b.local" defaults "${defaults}")
    file(WRITE "${config}" "${defaults}")
  else()
    set(config "${PROJECT_SOURCE_DIR}/config/admin.conf")
  endif()
  set_source_files_properties("${config}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/config")
  target_sources(${target} PRIVATE "${config}")
  set(icon "${PROJECT_SOURCE_DIR}/client/assets/app_icon.icns")
  set_source_files_properties("${icon}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
  target_sources(${target} PRIVATE "${icon}")
endfunction()
