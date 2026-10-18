find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick DBus)
find_package(LayerShellQt REQUIRED)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_executable(chrono-shell
  main.cpp
  layer.cpp layer.h
  apps.cpp apps.h
  notifications.cpp notifications.h
  system.cpp system.h
  shell.qrc)
target_link_libraries(chrono-shell PRIVATE
  chrono-qtcommon Qt6::DBus LayerShellQt::Interface)
install(TARGETS chrono-shell DESTINATION ${CMAKE_INSTALL_BINDIR})
