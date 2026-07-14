#include "User.h"

void User_main(void)
{
   if (fdcan_config(&hfdcan1, 1000000, 2000000) != 0 ||
       fdcan_config(&hfdcan2, 1000000, 5000000) != 0)
   {
      Error_Handler();
   }

   can_protocol_init();

   if (can_connect(CAN_BUS_1) != 0 || can_connect(CAN_BUS_2) != 0)
   {
      Error_Handler();
   }

   while (1)
   {
      can_process();
   }
}
