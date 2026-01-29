################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../App/systems/can_system.c \
../App/systems/ex_system.c \
../App/systems/heartbeat_system.c \
../App/systems/pcb_led_system.c 

OBJS += \
./App/systems/can_system.o \
./App/systems/ex_system.o \
./App/systems/heartbeat_system.o \
./App/systems/pcb_led_system.o 

C_DEPS += \
./App/systems/can_system.d \
./App/systems/ex_system.d \
./App/systems/heartbeat_system.d \
./App/systems/pcb_led_system.d 


# Each subdirectory must supply rules for building sources it contributes
App/systems/%.o App/systems/%.su App/systems/%.cyclo: ../App/systems/%.c App/systems/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L476xx -c -I../Core/Inc -I../App/Inc -I../Platform/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-App-2f-systems

clean-App-2f-systems:
	-$(RM) ./App/systems/can_system.cyclo ./App/systems/can_system.d ./App/systems/can_system.o ./App/systems/can_system.su ./App/systems/ex_system.cyclo ./App/systems/ex_system.d ./App/systems/ex_system.o ./App/systems/ex_system.su ./App/systems/heartbeat_system.cyclo ./App/systems/heartbeat_system.d ./App/systems/heartbeat_system.o ./App/systems/heartbeat_system.su ./App/systems/pcb_led_system.cyclo ./App/systems/pcb_led_system.d ./App/systems/pcb_led_system.o ./App/systems/pcb_led_system.su

.PHONY: clean-App-2f-systems

