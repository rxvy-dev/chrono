cmake_minimum_required(VERSION 3.20)
project(chrono VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)
find_package(PkgConfig REQUIRED)

option(CHRONO_COMPOSITOR "Build the chrono compositor (needs wlroots 0.20)" ON)
option(CHRONO_XWAYLAND   "Run X11 apps through XWayland" ON)
option(CHRONO_QT_APPS    "Build chrono-shell and chrono-settings (Qt 6)" ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)

set(CHRONO_DATADIR "${CMAKE_INSTALL_FULL_DATADIR}/chrono")

# ---------------------------------------------------------------- common (theme engine)
pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo pangocairo)
add_library(chrono-common STATIC common/ini.c common/theme.c common/paint.c)
target_include_directories(chrono-common PUBLIC common)
target_link_libraries(chrono-common PUBLIC PkgConfig::CAIRO m)
target_compile_definitions(chrono-common PRIVATE CHRONO_DATADIR="${CHRONO_DATADIR}")
set_target_properties(chrono-common PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(chrono-theme-preview tools/preview.c)
target_link_libraries(chrono-theme-preview chrono-common)

add_executable(chronoctl tools/chronoctl.c)

install(TARGETS chrono-theme-preview chronoctl DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY themes/ DESTINATION ${CHRONO_DATADIR}/themes FILES_MATCHING PATTERN "*.theme")

# ---------------------------------------------------------------- compositor
if(CHRONO_COMPOSITOR)
  pkg_check_modules(WLROOTS REQUIRED IMPORTED_TARGET wlroots-0.20)
  pkg_check_modules(WLDEPS REQUIRED IMPORTED_TARGET
    wayland-server xkbcommon libinput pixman-1 libdrm)
  pkg_get_variable(WL_PROTOCOLS_DIR wayland-protocols pkgdatadir)
  pkg_get_variable(WAYLAND_SCANNER wayland-scanner wayland_scanner)
  if(NOT WAYLAND_SCANNER)
    find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
  endif()

  # Server headers for every protocol whose enums/types wlroots headers or our code use.
  set(PROTOCOLS
    stable/xdg-shell/xdg-shell.xml
    stable/viewporter/viewporter.xml
    stable/presentation-time/presentation-time.xml
    stable/linux-dmabuf/linux-dmabuf-v1.xml
    stable/tablet/tablet-v2.xml
    unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
    unstable/pointer-constraints/pointer-constraints-unstable-v1.xml
    unstable/relative-pointer/relative-pointer-unstable-v1.xml
    unstable/idle-inhibit/idle-inhibit-unstable-v1.xml
    unstable/xdg-output/xdg-output-unstable-v1.xml
    unstable/primary-selection/primary-selection-unstable-v1.xml
    unstable/tablet/tablet-unstable-v2.xml
    staging/cursor-shape/cursor-shape-v1.xml
    staging/ext-session-lock/ext-session-lock-v1.xml
    staging/ext-idle-notify/ext-idle-notify-v1.xml
    staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml
    staging/ext-image-capture-source/ext-image-capture-source-v1.xml
    staging/ext-image-copy-capture/ext-image-copy-capture-v1.xml
    staging/ext-data-control/ext-data-control-v1.xml
    staging/xdg-activation/xdg-activation-v1.xml
    staging/content-type/content-type-v1.xml
    staging/alpha-modifier/alpha-modifier-v1.xml
    staging/fractional-scale/fractional-scale-v1.xml
    staging/single-pixel-buffer/single-pixel-buffer-v1.xml
    LOCAL/wlr-layer-shell-unstable-v1.xml
    LOCAL/wlr-output-power-management-unstable-v1.xml
  )
  set(PROTO_OUT ${CMAKE_CURRENT_BINARY_DIR}/protocols)
  file(MAKE_DIRECTORY ${PROTO_OUT})
  set(PROTO_HEADERS)
  foreach(p ${PROTOCOLS})
    if(p MATCHES "^LOCAL/")
      string(REPLACE "LOCAL/" "${CMAKE_CURRENT_SOURCE_DIR}/protocols/" xml ${p})
    else()
      set(xml ${WL_PROTOCOLS_DIR}/${p})
    endif()
    if(NOT EXISTS ${xml})
      message(STATUS "protocol not found, skipping: ${xml}")
      continue()
    endif()
    get_filename_component(base ${xml} NAME_WE)
    set(h ${PROTO_OUT}/${base}-protocol.h)
    add_custom_command(OUTPUT ${h}
      COMMAND ${WAYLAND_SCANNER} server-header ${xml} ${h}
      DEPENDS ${xml} VERBATIM)
    list(APPEND PROTO_HEADERS ${h})
  endforeach()
  add_custom_target(chrono-protocols DEPENDS ${PROTO_HEADERS})

  pkg_check_modules(PAM_PC IMPORTED_TARGET pam)
  find_library(PAM_LIB pam)
  if(NOT PAM_LIB)
    message(FATAL_ERROR "libpam is required for the lock screen")
  endif()

  add_executable(chrono
    compositor/main.c compositor/config.c compositor/buffer.c compositor/decor.c
    compositor/view.c compositor/output.c compositor/layer.c compositor/input.c
    compositor/switcher.c compositor/lock.c compositor/idle.c compositor/ipc.c
    compositor/xwayland.c)
  add_dependencies(chrono chrono-protocols)
  target_include_directories(chrono PRIVATE ${PROTO_OUT} compositor)
  target_compile_definitions(chrono PRIVATE
    WLR_USE_UNSTABLE
    CHRONO_VERSION="${PROJECT_VERSION}"
    CHRONO_DATADIR="${CHRONO_DATADIR}"
    CHRONO_LIBEXECDIR="${CMAKE_INSTALL_FULL_LIBEXECDIR}")
  target_link_libraries(chrono PRIVATE chrono-common PkgConfig::WLROOTS PkgConfig::WLDEPS ${PAM_LIB} m)

  if(CHRONO_XWAYLAND)
    pkg_check_modules(XCB IMPORTED_TARGET xcb xcb-icccm)
    if(XCB_FOUND)
      target_compile_definitions(chrono PRIVATE CHRONO_HAVE_XWAYLAND)
      target_link_libraries(chrono PRIVATE PkgConfig::XCB)
    else()
      message(WARNING "xcb/xcb-icccm not found: building without XWayland")
    endif()
  endif()

  install(TARGETS chrono DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# ---------------------------------------------------------------- Qt apps
if(CHRONO_QT_APPS)
  add_subdirectory(qtcommon)
  add_subdirectory(shell)
  add_subdirectory(settings)
endif()

# ---------------------------------------------------------------- session files
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/data)
  install(PROGRAMS data/chrono-session data/chrono-screenshot data/chrono-autostart
          DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)
  install(FILES data/chrono.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/wayland-sessions OPTIONAL)
  install(FILES data/chrono-portals.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/xdg-desktop-portal OPTIONAL)
  install(FILES data/pam/chrono DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/pam.d OPTIONAL)
  install(FILES data/chrono-settings.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications OPTIONAL)
endif()
