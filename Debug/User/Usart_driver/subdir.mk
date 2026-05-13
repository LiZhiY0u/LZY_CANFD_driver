################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../User/Usart_driver/usart_log.c 

OBJS += \
./User/Usart_driver/usart_log.o 

C_DEPS += \
./User/Usart_driver/usart_log.d 


# Each subdirectory must supply rules for building sources it contributes
User/Usart_driver/%.o User/Usart_driver/%.su User/Usart_driver/%.cyclo: ../User/Usart_driver/%.c User/Usart_driver/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_PWR_LDO_SUPPLY -DUSE_HAL_DRIVER -DSTM32H723xx -c -I../Core/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../Drivers/CMSIS/Include -I"D:/003_own_projects/CAN_learn/pcan_H723_V5_iwdg/User" -I"D:/003_own_projects/CAN_learn/pcan_H723_V5_iwdg/User/Pcan_driver" -I"D:/003_own_projects/CAN_learn/pcan_H723_V5_iwdg/User/Usart_driver" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-User-2f-Usart_driver

clean-User-2f-Usart_driver:
	-$(RM) ./User/Usart_driver/usart_log.cyclo ./User/Usart_driver/usart_log.d ./User/Usart_driver/usart_log.o ./User/Usart_driver/usart_log.su

.PHONY: clean-User-2f-Usart_driver

