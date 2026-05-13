################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../User/Uart_driver/uart_handle.c 

OBJS += \
./User/Uart_driver/uart_handle.o 

C_DEPS += \
./User/Uart_driver/uart_handle.d 


# Each subdirectory must supply rules for building sources it contributes
User/Uart_driver/%.o User/Uart_driver/%.su User/Uart_driver/%.cyclo: ../User/Uart_driver/%.c User/Uart_driver/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_PWR_LDO_SUPPLY -DUSE_HAL_DRIVER -DSTM32H723xx -DINCLUDE_LIN_INTERFACE=1 -DPCAN_PRO_FD=1 -c -I../Core/Inc -I"D:/001_work_projects/022_pcan/pcan_H723_V4_led/User/LED_driver" -I../USB_DEVICE/App -I../USB_DEVICE/Target -I../Drivers/STM32H7xx_HAL_Driver/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc -I../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../Drivers/CMSIS/Include -I"D:/001_work_projects/022_pcan/pcan_H723_V4_led/User" -I"D:/001_work_projects/022_pcan/pcan_H723_V4_led/User/Pcan_driver" -I"D:/001_work_projects/022_pcan/pcan_H723_V4_led/User/Uart_driver" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-User-2f-Uart_driver

clean-User-2f-Uart_driver:
	-$(RM) ./User/Uart_driver/uart_handle.cyclo ./User/Uart_driver/uart_handle.d ./User/Uart_driver/uart_handle.o ./User/Uart_driver/uart_handle.su

.PHONY: clean-User-2f-Uart_driver

