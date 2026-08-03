#
#    Copyright (c) 2023-2024 Project CHIP Authors
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

include(FetchContent)

get_filename_component(CHIP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/connectedhomeip REALPATH)
get_filename_component(TELINK_COMMON ${CHIP_ROOT}/examples/platform/telink REALPATH)
get_filename_component(GEN_DIR ${CHIP_ROOT}/zzz_generated/ REALPATH)

include(${TELINK_COMMON}/common.cmake)
include(${CHIP_ROOT}/config/telink/app/enable-gnu-std.cmake)
include(${CHIP_ROOT}/src/app/chip_data_model.cmake)

project(chip-telink-lock-aliro-minimal-example)

set(TELINK_ALIRO_SDK_URL "" CACHE STRING "URL of the Telink Aliro source SDK archive")
set(TELINK_ALIRO_SDK_SHA256 "" CACHE STRING "SHA-256 of the Telink Aliro source SDK archive")

if(CONFIG_ALIRO_TRANSPORT_NFC)
  if(FETCHCONTENT_SOURCE_DIR_TELINK_ALIRO)
    FetchContent_Declare(telink_aliro
      SOURCE_DIR "${FETCHCONTENT_SOURCE_DIR_TELINK_ALIRO}"
      SOURCE_SUBDIR cmake/zephyr
    )
  else()
    if(NOT TELINK_ALIRO_SDK_URL)
      message(FATAL_ERROR
        "TELINK_ALIRO_SDK_URL is not set. Provide the published SDK URL or "
        "-DFETCHCONTENT_SOURCE_DIR_TELINK_ALIRO=/path/to/telink_aliro."
      )
    endif()
    if(NOT TELINK_ALIRO_SDK_SHA256)
      message(FATAL_ERROR "TELINK_ALIRO_SDK_SHA256 must be set for a downloaded SDK archive")
    endif()

    FetchContent_Declare(telink_aliro
      URL "${TELINK_ALIRO_SDK_URL}"
      URL_HASH "SHA256=${TELINK_ALIRO_SDK_SHA256}"
      TLS_VERIFY TRUE
      SOURCE_SUBDIR cmake/zephyr
    )
  endif()

  FetchContent_MakeAvailable(telink_aliro)
  target_link_libraries(app PRIVATE Telink::Aliro)
endif()

target_include_directories(app PRIVATE
                           include
                           ${GEN_DIR}/app-common
                           ${GEN_DIR}/lock-app
                           ${TELINK_COMMON}/common/include
                           ${TELINK_COMMON}/util/include
                           ${TELINK_COMMON}/app/include
                           ${TELINK_COMMON}/zephyr_ext)

target_sources(app PRIVATE
               src/AppTask.cpp
               src/AliroDelegate.cpp
               src/ZclCallbacks.cpp
               src/LockManager.cpp
               ${TELINK_COMMON}/common/src/mainCommon.cpp
               ${TELINK_COMMON}/common/src/AppTaskCommon.cpp
               ${TELINK_COMMON}/util/src/LEDManager.cpp
               ${TELINK_COMMON}/util/src/ButtonManager.cpp
               ${TELINK_COMMON}/util/src/ThreadUtil.cpp
               ${TELINK_COMMON}/util/src/PWMManager.cpp
               ${TELINK_COMMON}/zephyr_ext/zephyr_key_matrix.c
               ${TELINK_COMMON}/zephyr_ext/zephyr_led_pool.c)

chip_configure_data_model(app
    ZAP_FILE ${CMAKE_CURRENT_SOURCE_DIR}/lock-app-minimal.zap
)

if(CONFIG_BOOTLOADER_MCUBOOT)
  target_sources(app PRIVATE ${TELINK_COMMON}/util/src/OTAUtil.cpp)
endif()

if(CONFIG_MCUMGR_TRANSPORT_BT)
    zephyr_library_link_libraries(MCUBOOT_BOOTUTIL)
    target_sources(app PRIVATE ${TELINK_COMMON}/util/src/DFUOverSMP.cpp)
endif()
