################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../User/Pcan_driver/can.c 

OBJS += \
./User/Pcan_driver/can.o 

C_DEPS += \
./User/Pcan_driver/can.d 


# Each subdirectory must supply rules for building sources it contributes
User/Pcan_driver/%.o User/Pcan_driver/%.su User/Pcan_driver/%.cyclo: ../User/Pcan_driver/%.c User/Pcan_driver/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_PWR_LDO_SUPPLY -DUSE_HAL_DRIVER -DSTM32H723xx -c -I../Core/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../Drivers/CMSIS/Include -I"D:/003_own_projects/CAN_learn/pcan_H723_V5_iwdg/User" -I"D:/003_own_projects/CAN_learn/pcan_H723_V5_iwdg/User/Pcan_driver" -I"D:/003_own_projects/CAN_learn/pcan_H723_V5_iwdg/User/Usart_driver" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-User-2f-Pcan_driver

clean-User-2f-Pcan_driver:
	-$(RM) ./User/Pcan_driver/can.cyclo ./User/Pcan_driver/can.d ./User/Pcan_driver/can.o ./User/Pcan_driver/can.su

.PHONY: clean-User-2f-Pcan_driver

