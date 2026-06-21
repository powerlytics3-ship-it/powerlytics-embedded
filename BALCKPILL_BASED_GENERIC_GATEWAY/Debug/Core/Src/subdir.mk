################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/app_board_io.c \
../Core/Src/app_modbus_payload.c \
../Core/Src/cJSON.c \
../Core/Src/config_apply.c \
../Core/Src/debug_uart.c \
../Core/Src/freertos.c \
../Core/Src/main.c \
../Core/Src/modbus_config_flash.c \
../Core/Src/modbus_config_json.c \
../Core/Src/modbus_crc.c \
../Core/Src/modbus_rtu.c \
../Core/Src/modbus_rtu_master.c \
../Core/Src/modem_handler.c \
../Core/Src/mqtt_config.c \
../Core/Src/mqtt_handler.c \
../Core/Src/pub_queue.c \
../Core/Src/rs4585.c \
../Core/Src/rtc_manager.c \
../Core/Src/sha256.c \
../Core/Src/stm32f4xx_hal_msp.c \
../Core/Src/stm32f4xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32f4xx.c \
../Core/Src/uart2_rx_engine.c \
../Core/Src/w25q16.c 

OBJS += \
./Core/Src/app_board_io.o \
./Core/Src/app_modbus_payload.o \
./Core/Src/cJSON.o \
./Core/Src/config_apply.o \
./Core/Src/debug_uart.o \
./Core/Src/freertos.o \
./Core/Src/main.o \
./Core/Src/modbus_config_flash.o \
./Core/Src/modbus_config_json.o \
./Core/Src/modbus_crc.o \
./Core/Src/modbus_rtu.o \
./Core/Src/modbus_rtu_master.o \
./Core/Src/modem_handler.o \
./Core/Src/mqtt_config.o \
./Core/Src/mqtt_handler.o \
./Core/Src/pub_queue.o \
./Core/Src/rs4585.o \
./Core/Src/rtc_manager.o \
./Core/Src/sha256.o \
./Core/Src/stm32f4xx_hal_msp.o \
./Core/Src/stm32f4xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32f4xx.o \
./Core/Src/uart2_rx_engine.o \
./Core/Src/w25q16.o 

C_DEPS += \
./Core/Src/app_board_io.d \
./Core/Src/app_modbus_payload.d \
./Core/Src/cJSON.d \
./Core/Src/config_apply.d \
./Core/Src/debug_uart.d \
./Core/Src/freertos.d \
./Core/Src/main.d \
./Core/Src/modbus_config_flash.d \
./Core/Src/modbus_config_json.d \
./Core/Src/modbus_crc.d \
./Core/Src/modbus_rtu.d \
./Core/Src/modbus_rtu_master.d \
./Core/Src/modem_handler.d \
./Core/Src/mqtt_config.d \
./Core/Src/mqtt_handler.d \
./Core/Src/pub_queue.d \
./Core/Src/rs4585.d \
./Core/Src/rtc_manager.d \
./Core/Src/sha256.d \
./Core/Src/stm32f4xx_hal_msp.d \
./Core/Src/stm32f4xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32f4xx.d \
./Core/Src/uart2_rx_engine.d \
./Core/Src/w25q16.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/app_board_io.cyclo ./Core/Src/app_board_io.d ./Core/Src/app_board_io.o ./Core/Src/app_board_io.su ./Core/Src/app_modbus_payload.cyclo ./Core/Src/app_modbus_payload.d ./Core/Src/app_modbus_payload.o ./Core/Src/app_modbus_payload.su ./Core/Src/cJSON.cyclo ./Core/Src/cJSON.d ./Core/Src/cJSON.o ./Core/Src/cJSON.su ./Core/Src/config_apply.cyclo ./Core/Src/config_apply.d ./Core/Src/config_apply.o ./Core/Src/config_apply.su ./Core/Src/debug_uart.cyclo ./Core/Src/debug_uart.d ./Core/Src/debug_uart.o ./Core/Src/debug_uart.su ./Core/Src/freertos.cyclo ./Core/Src/freertos.d ./Core/Src/freertos.o ./Core/Src/freertos.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/modbus_config_flash.cyclo ./Core/Src/modbus_config_flash.d ./Core/Src/modbus_config_flash.o ./Core/Src/modbus_config_flash.su ./Core/Src/modbus_config_json.cyclo ./Core/Src/modbus_config_json.d ./Core/Src/modbus_config_json.o ./Core/Src/modbus_config_json.su ./Core/Src/modbus_crc.cyclo ./Core/Src/modbus_crc.d ./Core/Src/modbus_crc.o ./Core/Src/modbus_crc.su ./Core/Src/modbus_rtu.cyclo ./Core/Src/modbus_rtu.d ./Core/Src/modbus_rtu.o ./Core/Src/modbus_rtu.su ./Core/Src/modbus_rtu_master.cyclo ./Core/Src/modbus_rtu_master.d ./Core/Src/modbus_rtu_master.o ./Core/Src/modbus_rtu_master.su ./Core/Src/modem_handler.cyclo ./Core/Src/modem_handler.d ./Core/Src/modem_handler.o ./Core/Src/modem_handler.su ./Core/Src/mqtt_config.cyclo ./Core/Src/mqtt_config.d ./Core/Src/mqtt_config.o ./Core/Src/mqtt_config.su ./Core/Src/mqtt_handler.cyclo ./Core/Src/mqtt_handler.d ./Core/Src/mqtt_handler.o ./Core/Src/mqtt_handler.su ./Core/Src/pub_queue.cyclo ./Core/Src/pub_queue.d ./Core/Src/pub_queue.o ./Core/Src/pub_queue.su ./Core/Src/rs4585.cyclo ./Core/Src/rs4585.d ./Core/Src/rs4585.o ./Core/Src/rs4585.su ./Core/Src/rtc_manager.cyclo ./Core/Src/rtc_manager.d ./Core/Src/rtc_manager.o ./Core/Src/rtc_manager.su ./Core/Src/sha256.cyclo ./Core/Src/sha256.d ./Core/Src/sha256.o ./Core/Src/sha256.su ./Core/Src/stm32f4xx_hal_msp.cyclo ./Core/Src/stm32f4xx_hal_msp.d ./Core/Src/stm32f4xx_hal_msp.o ./Core/Src/stm32f4xx_hal_msp.su ./Core/Src/stm32f4xx_it.cyclo ./Core/Src/stm32f4xx_it.d ./Core/Src/stm32f4xx_it.o ./Core/Src/stm32f4xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32f4xx.cyclo ./Core/Src/system_stm32f4xx.d ./Core/Src/system_stm32f4xx.o ./Core/Src/system_stm32f4xx.su ./Core/Src/uart2_rx_engine.cyclo ./Core/Src/uart2_rx_engine.d ./Core/Src/uart2_rx_engine.o ./Core/Src/uart2_rx_engine.su ./Core/Src/w25q16.cyclo ./Core/Src/w25q16.d ./Core/Src/w25q16.o ./Core/Src/w25q16.su

.PHONY: clean-Core-2f-Src

