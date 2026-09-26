cmake_minimum_required(VERSION 3.16)
project(AlfaRaceX C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(ARX_STRICT_WARNINGS "Treat compiler warnings as errors" ON)
option(ARX_ENABLE_SANITIZERS "Enable ASan/UBSan in host builds" OFF)

if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if (ARX_STRICT_WARNINGS)
        add_compile_options(-Werror)
    endif()
    if (ARX_ENABLE_SANITIZERS)
        add_compile_options(-fno-omit-frame-pointer -fsanitize=address,undefined)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

add_library(arx_core
    src/core/arx_can.c
    src/core/arx_config.c
    src/core/arx_crc.c
    src/core/arx_diag.c
    src/core/arx_feature_catalog.c
    src/core/arx_interchip.c
    src/core/arx_isotp.c
    src/core/arx_log_export.c
    src/core/arx_link.c
    src/core/arx_ingress.c
    src/core/arx_power.c
    src/core/arx_runtime.c
    src/core/arx_sgw.c
    src/core/arx_storage.c
    src/core/arx_telemetry_db.c
    src/core/arx_usb_mode.c

    src/vehicle/arx_vehicle_state.c
    targets/stm32f072_common/src/arx_stm32f072_target.c
    src/vehicle/arx_decoder.c

    src/features/arx_acc.c
    src/features/arx_awd.c
    src/features/arx_brake_override.c
    src/features/arx_dashboard.c
    src/features/arx_dpf_alert.c
    src/features/arx_drive_style.c
    src/features/arx_dynamic_shift.c
    src/features/arx_dyno.c
    src/features/arx_elm327.c
    src/features/arx_elm_transport.c
    src/features/arx_experimental.c
    src/features/arx_faults.c
    src/features/arx_immobilizer.c
    src/features/arx_led_strip.c
    src/features/arx_max_hold.c
    src/features/arx_menu.c
    src/features/arx_menu_input.c
    src/features/arx_odometer.c
    src/features/arx_park_mirror.c
    src/features/arx_park_mute.c
    src/features/arx_pedal_controller.c
    src/features/arx_performance.c
    src/features/arx_qv_exhaust.c
    src/features/arx_router.c
    src/features/arx_route_service.c
    src/features/arx_seatbelt.c
    src/features/arx_sniffer.c
    src/features/arx_start_stop.c
    src/features/arx_windows.c
)

target_include_directories(arx_core PUBLIC include targets/stm32f072_common/include)

enable_testing()

function(arx_add_test test_name)
    add_executable(test_${test_name} tests/test_${test_name}.c)
    target_link_libraries(test_${test_name} PRIVATE arx_core)
    # Release builds normally define NDEBUG, which would compile standard assert()
    # expressions away and could produce false-green test results. Tests always
    # keep assertions active, independently of build type.
    if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(test_${test_name} PRIVATE -UNDEBUG)
    endif()
    add_test(NAME ${test_name} COMMAND test_${test_name})
endfunction()

foreach(test_name
    dynamic_shift
    crc
    features
    pedal_controller
    parity_core
    dyno_faults
    shift_exact
    infrastructure
    link
    config_performance
    target_contract
    isotp_elm
    isotp_integrity
    elm_isolation
    elm_queue
    interchip_stress
    can_backpressure
    ingress
    storage
    drive_led_usb
    runtime
    telemetry_menu
    log_export
    menu_input_runtime
    stress_runtime
)
    arx_add_test(${test_name})
endforeach()


find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME size_gate_selftest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/release/test_size_gate.py)
add_test(NAME target_completeness
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/release/check_target_completeness.py)
