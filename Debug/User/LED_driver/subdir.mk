################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../User/LED_driver/led.c 

OBJS += \
./User/LED_driver/led.o 

C_DEPS += \
./User/LED_driver/led.d 


# Each subdirectory must supply rules for building sources it contributes
User/LED_driver/%.o User/LED_driver/%.su User/LED_driver/%.cyclo: ../User/LED_driver/%.c User/LED_driver/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_PWR_LDO_SUPPLY -DUSE_HAL_DRIVER -DSTM32H723xx -DINCLUDE_LIN_INTERFACE=1 -DPCAN_PRO_FD=1 -c -I../Core/Inc -I"D:/001_work_projects/022_pcan/pcan_H723_V5_iwdg/User/LED_driver" -I../USB_DEVICE/App -I../USB_DEVICE/Target -I../Drivers/STM32H7xx_HAL_Driver/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc -I../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../Drivers/CMSIS/Include -I"D:/001_work_projects/022_pcan/pcan_H723_V5_iwdg/User" -I"D:/001_work_projects/022_pcan/pcan_H723_V5_iwdg/User/Pcan_driver" -I"D:/001_work_projects/022_pcan/pcan_H723_V5_iwdg/User/Usart_driver" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-User-2f-LED_driver

clean-User-2f-LED_driver:
	-$(RM) ./User/LED_driver/led.cyclo ./User/LED_driver/led.d ./User/LED_driver/led.o ./User/LED_driver/led.su

.PHONY: clean-User-2f-LED_driver

