cmake_minimum_required(VERSION 3.16)
project(sonos_wifi_rpi CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2")
add_compile_options(-Wall -Wextra)
find_package(ALSA REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL)        # opcional: HTTPS (imprescindible para micrófono/Web Bluetooth en el móvil)
# protocol.h vive en ../common (compartido con el firmware ESP32)
include_directories(include ../common ${ALSA_INCLUDE_DIRS})
add_executable(sonos_wifi
    src/main.cpp
    src/audio_capture.cpp
    src/udp_streamer.cpp
    src/web_server.cpp
    src/ble_manager.cpp
)
target_link_libraries(sonos_wifi ${ALSA_LIBRARIES} Threads::Threads)
if(OpenSSL_FOUND)
  target_compile_definitions(sonos_wifi PRIVATE WITH_TLS)
  target_link_libraries(sonos_wifi OpenSSL::SSL OpenSSL::Crypto)
else()
  message(WARNING "OpenSSL no encontrado: sin HTTPS (apt install libssl-dev)")
endif()
install(TARGETS sonos_wifi DESTINATION /usr/local/bin)
