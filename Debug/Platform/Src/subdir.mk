################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Platform/Src/can.c \
../Platform/Src/gpio.c \
../Platform/Src/stm32l4xx_hal_msp.c \
../Platform/Src/stm32l4xx_it.c \
../Platform/Src/syscalls.c \
../Platform/Src/sysmem.c \
../Platform/Src/system_clock.c \
../Platform/Src/system_stm32l4xx.c \
../Platform/Src/usart.c 

OBJS += \
./Platform/Src/can.o \
./Platform/Src/gpio.o \
./Platform/Src/stm32l4xx_hal_msp.o \
./Platform/Src/stm32l4xx_it.o \
./Platform/Src/syscalls.o \
./Platform/Src/sysmem.o \
./Platform/Src/system_clock.o \
./Platform/Src/system_stm32l4xx.o \
./Platform/Src/usart.o 

C_DEPS += \
./Platform/Src/can.d \
./Platform/Src/gpio.d \
./Platform/Src/stm32l4xx_hal_msp.d \
./Platform/Src/stm32l4xx_it.d \
./Platform/Src/syscalls.d \
./Platform/Src/sysmem.d \
./Platform/Src/system_clock.d \
./Platform/Src/system_stm32l4xx.d \
./Platform/Src/usart.d 


# Each subdirectory must supply rules for building sources it contributes
Platform/Src/%.o Platform/Src/%.su Platform/Src/%.cyclo: ../Platform/Src/%.c Platform/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L476xx -c -I../Core/Inc -I../App/Inc -I../Platform/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Platform-2f-Src

clean-Platform-2f-Src:
	-$(RM) ./Platform/Src/can.cyclo ./Platform/Src/can.d ./Platform/Src/can.o ./Platform/Src/can.su ./Platform/Src/gpio.cyclo ./Platform/Src/gpio.d ./Platform/Src/gpio.o ./Platform/Src/gpio.su ./Platform/Src/stm32l4xx_hal_msp.cyclo ./Platform/Src/stm32l4xx_hal_msp.d ./Platform/Src/stm32l4xx_hal_msp.o ./Platform/Src/stm32l4xx_hal_msp.su ./Platform/Src/stm32l4xx_it.cyclo ./Platform/Src/stm32l4xx_it.d ./Platform/Src/stm32l4xx_it.o ./Platform/Src/stm32l4xx_it.su ./Platform/Src/syscalls.cyclo ./Platform/Src/syscalls.d ./Platform/Src/syscalls.o ./Platform/Src/syscalls.su ./Platform/Src/sysmem.cyclo ./Platform/Src/sysmem.d ./Platform/Src/sysmem.o ./Platform/Src/sysmem.su ./Platform/Src/system_clock.cyclo ./Platform/Src/system_clock.d ./Platform/Src/system_clock.o ./Platform/Src/system_clock.su ./Platform/Src/system_stm32l4xx.cyclo ./Platform/Src/system_stm32l4xx.d ./Platform/Src/system_stm32l4xx.o ./Platform/Src/system_stm32l4xx.su ./Platform/Src/usart.cyclo ./Platform/Src/usart.d ./Platform/Src/usart.o ./Platform/Src/usart.su

.PHONY: clean-Platform-2f-Src

