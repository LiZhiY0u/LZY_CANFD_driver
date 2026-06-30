#include "User.h"

void User_main(void)
{
   fdcan_config(&hfdcan1, 1000000, 2000000);
   fdcan_config(&hfdcan2, 1000000, 5000000);
   can_protocol_init();

   while (1)
   {
      can_process();
   }
}
