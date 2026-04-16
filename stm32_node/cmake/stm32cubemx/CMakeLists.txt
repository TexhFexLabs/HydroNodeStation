cmake_minimum_required(VERSION 3.22)
# Enable CMake support for ASM and C languages
enable_language(C ASM)
# STM32CubeMX generated symbols (macros)
set(MX_Defines_Syms 
	NUMBER_OF_STACKS=1 
	SX126X 
	ENDNODE 
	CORE_CM4 
	USE_HAL_DRIVER 
	STM32WL55xx
    $<$<CONFIG:Debug>:DEBUG>
)

# STM32CubeMX generated include paths
set(MX_Include_Dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../LoRaWAN/App
    ${CMAKE_CURRENT_SOURCE_DIR}/../../LoRaWAN/Target
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Inc/Legacy
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/trace/adv_trace
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/misc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/sequencer
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/timer
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/lpm/tiny_lpm
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lorawan_api
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_hal
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lorawan_manager
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_api
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lorawan_packages/lorawan_certification
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/lr1mac_class_b
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/lr1mac_class_c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/relay/common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/relay/relay_rx
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/relay/relay_tx
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/services/smtc_multicast
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/smtc_real/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/modem_services/beacon_tx_service
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/modem_services/lfu_service
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/modem_services
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/modem_services/service_template
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/modem_utilities
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/smtc_modem_crypto
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/smtc_modem_crypto/smtc_secure_element
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/SubGHz_Phy/lorawan
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/SubGHz_Phy/radio_driver
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/SubGHz_Phy/lorawan/radio_drivers/sx126x_driver/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/SubGHz_Phy/lorawan/radio_planner/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/SubGHz_Phy/lorawan/smtc_ral/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/SubGHz_Phy/lorawan/smtc_ralf/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/CMSIS/Device/ST/STM32WLxx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/services
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/modem_services/relay_service
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/modem_services/store_and_forward
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/modem_supervisor
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/smtc_modem_crypto/soft_secure_element
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/CMSIS/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/BSP/STM32WLxx_LoRa_E5_mini
)

# STM32CubeMX generated application sources
set(MX_Application_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../LoRaWAN/Target/radio_board_if.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../LoRaWAN/App/app_lorawan.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../LoRaWAN/App/CayenneLpp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../LoRaWAN/App/lora_app.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/adc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/dma.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/i2c.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/flash_if.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/stm32_lpm_if.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/sys_app.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/sys_debug.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/sys_sensors.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/timer_if.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/adc_if.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/usart_if.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/rng.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/rtc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/subghz.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/usart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/stm32wlxx_it.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/stm32wlxx_hal_msp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/sysmem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/syscalls.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/sht45.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/bmp390.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/ltr390.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/max17048.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/scd41.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../startup_stm32wl55xx_cm4.s
)

# STM32 HAL/LL Drivers
set(STM32_Drivers_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/system_stm32wlxx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_adc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_adc_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_ll_adc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_rcc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_rcc_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_flash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_flash_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_dma.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_dma_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_pwr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_pwr_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_cortex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_exti.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_i2c.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_i2c_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_rng.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_rng_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_rtc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_rtc_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_subghz.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_uart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WLxx_HAL_Driver/Src/stm32wlxx_hal_uart_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/BSP/STM32WLxx_LoRa_E5_mini/stm32wlxx_LoRa_E5_mini.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/BSP/STM32WLxx_LoRa_E5_mini/stm32wlxx_LoRa_E5_mini_radio.c
)

# Drivers Midllewares


set(Utilities_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/trace/adv_trace/stm32_adv_trace.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/misc/stm32_mem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/misc/stm32_systime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/misc/stm32_tiny_sscanf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/misc/stm32_tiny_vsnprintf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/sequencer/stm32_seq.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/timer/stm32_timer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/lpm/tiny_lpm/stm32_lpm.c
)
set(LoRaWAN_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lorawan_api/lorawan_api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lorawan_manager/lorawan_cid_request_management.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lorawan_manager/lorawan_class_b_management.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lorawan_manager/lorawan_dwn_ack_management.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lorawan_manager/lorawan_join_management.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lorawan_manager/lorawan_send_management.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lorawan_packages/lorawan_certification/lorawan_certification.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/lr1mac_class_b/smtc_beacon_sniff.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/lr1mac_class_b/smtc_ping_slot.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/lr1mac_class_c/lr1mac_class_c.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/relay/common/relay_mac_parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/relay/common/relay_real.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/relay/common/wake_on_radio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/relay/common/wake_on_radio_ral.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/relay/relay_rx/relay_rx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/relay/relay_rx/relay_rx_mac_parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/relay/relay_tx/relay_tx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/relay/relay_tx/relay_tx_mac_parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/services/smtc_multicast/smtc_multicast.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/smtc_real/src/region_as_923.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/smtc_real/src/region_au_915.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/smtc_real/src/region_cn_470.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/smtc_real/src/region_cn_470_rp_1_0.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/smtc_real/src/region_eu_868.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/smtc_real/src/region_in_865.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/smtc_real/src/region_kr_920.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/smtc_real/src/region_ru_864.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/smtc_real/src/region_us_915.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/smtc_real/src/region_ww2g4.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/smtc_real/src/smtc_real.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/lr1_stack_mac_layer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/lr1mac_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/lr1mac_utilities.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/modem_services/beacon_tx_service/lorawan_beacon_tx_service_example.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/modem_services/lfu_service/file_upload.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/modem_services/service_template/service_template.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/modem_utilities/circularfs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/modem_utilities/fifo_ctrl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/modem_utilities/modem_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/modem_utilities/modem_event_utilities.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/smtc_modem_crypto/smtc_modem_crypto.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/services/smtc_lbt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/services/smtc_duty_cycle.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/lr1mac/src/services/smtc_lora_cad_bt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/modem_services/relay_service/lorawan_relay_tx_service.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/modem_services/relay_service/lorawan_relay_rx_service.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/modem_services/store_and_forward/store_and_forward_flash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/modem_services/store_and_forward/store_and_forward.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/modem_supervisor/modem_tx_protocol_manager.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/modem_supervisor/modem_supervisor_light.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/smtc_modem_crypto/soft_secure_element/aes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/smtc_modem_crypto/soft_secure_element/soft_se.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/smtc_modem_crypto/soft_secure_element/cmac.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/smtc_modem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LoRaWAN/smtc_modem_core/smtc_modem_test.c
)
set(SubGHz_Phy_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/SubGHz_Phy/lorawan/radio_drivers/sx126x_driver/src/lr_fhss_mac.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/SubGHz_Phy/lorawan/radio_drivers/sx126x_driver/src/sx126x.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/SubGHz_Phy/lorawan/radio_drivers/sx126x_driver/src/sx126x_driver_version.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/SubGHz_Phy/lorawan/radio_drivers/sx126x_driver/src/sx126x_lr_fhss.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/SubGHz_Phy/lorawan/radio_planner/src/radio_planner.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/SubGHz_Phy/lorawan/smtc_ral/src/ral_sx126x.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/SubGHz_Phy/lorawan/smtc_ralf/src/ralf_sx126x.c
)

# Link directories setup
set(MX_LINK_DIRS

)
# Project static libraries
set(MX_LINK_LIBS 
    STM32_Drivers
    ${TOOLCHAIN_LINK_LIBRARIES}
    Utilities
	LoRaWAN
	SubGHz_Phy
	
)
# Interface library for includes and symbols
add_library(stm32cubemx INTERFACE)
target_include_directories(stm32cubemx INTERFACE ${MX_Include_Dirs})
target_compile_definitions(stm32cubemx INTERFACE ${MX_Defines_Syms})

# Create STM32_Drivers static library
add_library(STM32_Drivers OBJECT)
target_sources(STM32_Drivers PRIVATE ${STM32_Drivers_Src})
target_link_libraries(STM32_Drivers PUBLIC stm32cubemx)


# Create Utilities static library
add_library(Utilities OBJECT)
target_sources(Utilities PRIVATE ${Utilities_Src})
target_link_libraries(Utilities PUBLIC stm32cubemx)

# Create LoRaWAN static library
add_library(LoRaWAN OBJECT)
target_sources(LoRaWAN PRIVATE ${LoRaWAN_Src})
target_link_libraries(LoRaWAN PUBLIC stm32cubemx)

# Create SubGHz_Phy static library
add_library(SubGHz_Phy OBJECT)
target_sources(SubGHz_Phy PRIVATE ${SubGHz_Phy_Src})
target_link_libraries(SubGHz_Phy PUBLIC stm32cubemx)

# Add STM32CubeMX generated application sources to the project
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${MX_Application_Src})

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${MX_LINK_DIRS})

# Add libraries to the project
target_link_libraries(${CMAKE_PROJECT_NAME} ${MX_LINK_LIBS})

# Add the map file to the list of files to be removed with 'clean' target
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES ADDITIONAL_CLEAN_FILES ${CMAKE_PROJECT_NAME}.map)

# Validate that STM32CubeMX code is compatible with C standard
if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "Generated code requires C11 or higher")
endif()
