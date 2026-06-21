################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/subdir.mk
-include Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/subdir.mk
-include Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/subdir.mk
-include Middlewares/Third_Party/FreeRTOS/Source/subdir.mk
-include Drivers/STM32F4xx_HAL_Driver/Src/subdir.mk
-include Core/Startup/subdir.mk
-include Core/Src/subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

OPTIONAL_TOOL_DEPS := \
$(wildcard ../makefile.defs) \
$(wildcard ../makefile.init) \
$(wildcard ../makefile.targets) \


BUILD_ARTIFACT_NAME := BALCKPILL_BASED_GENERIC_GATEWAY
BUILD_ARTIFACT_EXTENSION := elf
BUILD_ARTIFACT_PREFIX :=
BUILD_ARTIFACT := $(BUILD_ARTIFACT_PREFIX)$(BUILD_ARTIFACT_NAME)$(if $(BUILD_ARTIFACT_EXTENSION),.$(BUILD_ARTIFACT_EXTENSION),)

# Add inputs and outputs from these tool invocations to the build variables 
EXECUTABLES += \
BALCKPILL_BASED_GENERIC_GATEWAY.elf \

MAP_FILES += \
BALCKPILL_BASED_GENERIC_GATEWAY.map \

SIZE_OUTPUT += \
default.size.stdout \

OBJDUMP_LIST += \
BALCKPILL_BASED_GENERIC_GATEWAY.list \


# All Target
all: main-build

# Main-build Target
main-build: BALCKPILL_BASED_GENERIC_GATEWAY.elf secondary-outputs

# Tool invocations
BALCKPILL_BASED_GENERIC_GATEWAY.elf BALCKPILL_BASED_GENERIC_GATEWAY.map: $(OBJS) $(USER_OBJS) /Users/shubhambarkur/Documents/projects/stm_codes/Embedded-Stuff/publisher_base_code/BALCKPILL_BASED_GENERIC_GATEWAY/STM32F407VETX_FLASH.ld makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-gcc -o "BALCKPILL_BASED_GENERIC_GATEWAY.elf" @"objects.list" $(USER_OBJS) $(LIBS) -mcpu=cortex-m4 -T"/Users/shubhambarkur/Documents/projects/stm_codes/Embedded-Stuff/publisher_base_code/BALCKPILL_BASED_GENERIC_GATEWAY/STM32F407VETX_FLASH.ld" --specs=nosys.specs -Wl,-Map="BALCKPILL_BASED_GENERIC_GATEWAY.map" -Wl,--gc-sections -static --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -Wl,--start-group -lc -lm -Wl,--end-group
	@echo 'Finished building target: $@'
	@echo ' '

default.size.stdout: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-size  $(EXECUTABLES)
	@echo 'Finished building: $@'
	@echo ' '

BALCKPILL_BASED_GENERIC_GATEWAY.list: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-objdump -h -S $(EXECUTABLES) > "BALCKPILL_BASED_GENERIC_GATEWAY.list"
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) BALCKPILL_BASED_GENERIC_GATEWAY.elf BALCKPILL_BASED_GENERIC_GATEWAY.list BALCKPILL_BASED_GENERIC_GATEWAY.map default.size.stdout
	-@echo ' '

secondary-outputs: $(SIZE_OUTPUT) $(OBJDUMP_LIST)

fail-specified-linker-script-missing:
	@echo 'Error: Cannot find the specified linker script. Check the linker settings in the build configuration.'
	@exit 2

warn-no-linker-script-specified:
	@echo 'Warning: No linker script specified. Check the linker settings in the build configuration.'

.PHONY: all clean dependents main-build fail-specified-linker-script-missing warn-no-linker-script-specified

-include ../makefile.targets
