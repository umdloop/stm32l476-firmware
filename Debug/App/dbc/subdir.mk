################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../App/dbc/can_dbc_text.c 

OBJS += \
./App/dbc/can_dbc_text.o 

C_DEPS += \
./App/dbc/can_dbc_text.d 


# Each subdirectory must supply rules for building sources it contributes
App/dbc/%.o App/dbc/%.su App/dbc/%.cyclo: ../App/dbc/%.c App/dbc/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L476xx -c -I../Core/Inc -I../App/Inc -I../Platform/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-App-2f-dbc

clean-App-2f-dbc:
	-$(RM) ./App/dbc/can_dbc_text.cyclo ./App/dbc/can_dbc_text.d ./App/dbc/can_dbc_text.o ./App/dbc/can_dbc_text.su

.PHONY: clean-App-2f-dbc

