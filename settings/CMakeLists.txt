find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_executable(chrono-settings main.cpp backend.cpp backend.h settings.qrc)
target_link_libraries(chrono-settings PRIVATE chrono-qtcommon Qt6::QuickControls2)
install(TARGETS chrono-settings DESTINATION ${CMAKE_INSTALL_BINDIR})
