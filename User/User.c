#include "User.h"
// 记得改晶振

uint8_t tempData[64] = {1, 2, 3, 4, 5, 6, 7, 8};
uint8_t tempData2[64] = {8, 7, 6, 5, 4, 3, 2, 1};
static FDCAN_TxHeaderTypeDef FDCAN1_TxHeader, FDCAN2_TxHeader;
FDCAN_RxHeaderTypeDef FDCAN1_RxHeader, FDCAN2_RxHeader;

void BspFDCANInit(void)
{
   FDCAN_FilterTypeDef sFilterConfig = {0};

   sFilterConfig.IdType = FDCAN_STANDARD_ID;
   sFilterConfig.FilterIndex = 0;
   sFilterConfig.FilterType = FDCAN_FILTER_MASK;
   sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
   sFilterConfig.FilterID1 = 0;
   sFilterConfig.FilterID2 = 0;
   sFilterConfig.RxBufferIndex = 0;
   sFilterConfig.IsCalibrationMsg = 0;

   HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfig);

   sFilterConfig.IdType = FDCAN_EXTENDED_ID;
   sFilterConfig.FilterIndex = 0;
   sFilterConfig.FilterType = FDCAN_FILTER_MASK;
   sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
   sFilterConfig.FilterID1 = 0;
   sFilterConfig.FilterID2 = 0;
   sFilterConfig.RxBufferIndex = 0;
   sFilterConfig.IsCalibrationMsg = 0;

   HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfig);

   HAL_FDCAN_ConfigGlobalFilter(&hfdcan1, FDCAN_REJECT, FDCAN_REJECT, FDCAN_FILTER_REMOTE, FDCAN_REJECT_REMOTE);

   HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);

   HAL_FDCAN_Start(&hfdcan1);

   FDCAN_FilterTypeDef sFilterConfig2 = {0};

   sFilterConfig2.IdType = FDCAN_STANDARD_ID;
   sFilterConfig2.FilterIndex = 0;
   sFilterConfig2.FilterType = FDCAN_FILTER_MASK;
   sFilterConfig2.FilterConfig = FDCAN_FILTER_TO_RXFIFO1;
   sFilterConfig2.FilterID1 = 0;
   sFilterConfig2.FilterID2 = 0;
   sFilterConfig2.RxBufferIndex = 0;
   sFilterConfig2.IsCalibrationMsg = 0;
   HAL_FDCAN_ConfigFilter(&hfdcan2, &sFilterConfig2);

   sFilterConfig2.IdType = FDCAN_EXTENDED_ID;
   sFilterConfig2.FilterIndex = 0;
   sFilterConfig2.FilterType = FDCAN_FILTER_MASK;
   sFilterConfig2.FilterConfig = FDCAN_FILTER_TO_RXFIFO1;
   sFilterConfig2.FilterID1 = 0;
   sFilterConfig2.FilterID2 = 0;
   sFilterConfig2.RxBufferIndex = 0;
   sFilterConfig2.IsCalibrationMsg = 0;
   HAL_FDCAN_ConfigFilter(&hfdcan2, &sFilterConfig2);

   HAL_FDCAN_ConfigGlobalFilter(&hfdcan2, FDCAN_REJECT, FDCAN_REJECT, FDCAN_FILTER_REMOTE, FDCAN_REJECT_REMOTE);

   HAL_FDCAN_ActivateNotification(&hfdcan2, FDCAN_IT_RX_FIFO1_NEW_MESSAGE, 0);

   HAL_FDCAN_Start(&hfdcan2);
}

uint32_t FDCAN1_Receive_Msg(uint8_t *buf)
{
   if (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0) > 0)
   {
      HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &FDCAN1_RxHeader, buf);
      return FDCAN1_RxHeader.DataLength;
   }
}

uint32_t FDCAN2_Receive_Msg(uint8_t *buf)
{
   if (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan2, FDCAN_RX_FIFO1) > 0)
   {
      HAL_FDCAN_GetRxMessage(&hfdcan2, FDCAN_RX_FIFO1, &FDCAN2_RxHeader, buf);
      return FDCAN2_RxHeader.DataLength;
   }
}

uint32_t FDCAN1_Send_Msg(uint32_t can_id, uint8_t *msg, uint32_t length_code)
{

   FDCAN1_TxHeader.Identifier = can_id;
   FDCAN1_TxHeader.IdType = FDCAN_EXTENDED_ID;
   FDCAN1_TxHeader.TxFrameType = FDCAN_DATA_FRAME;
   FDCAN1_TxHeader.DataLength = length_code;
   FDCAN1_TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
   FDCAN1_TxHeader.BitRateSwitch = FDCAN_BRS_ON;
   FDCAN1_TxHeader.FDFormat = FDCAN_FD_CAN;
   FDCAN1_TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
   FDCAN1_TxHeader.MessageMarker = 0;

   return HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &FDCAN1_TxHeader, msg);
}

uint32_t FDCAN2_Send_Msg(uint32_t can_id, uint8_t *msg, uint32_t length_code)
{

   FDCAN2_TxHeader.Identifier = can_id;
   FDCAN2_TxHeader.IdType = FDCAN_EXTENDED_ID;
   FDCAN2_TxHeader.TxFrameType = FDCAN_DATA_FRAME;
   FDCAN2_TxHeader.DataLength = FDCAN_DLC_BYTES_8;
   FDCAN2_TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
   FDCAN2_TxHeader.BitRateSwitch = FDCAN_BRS_ON;
   FDCAN2_TxHeader.FDFormat = FDCAN_FD_CAN;
   FDCAN2_TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
   FDCAN2_TxHeader.MessageMarker = 0;

   return HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &FDCAN2_TxHeader, msg);
}

extern IWDG_HandleTypeDef hiwdg1;

void BSP_IWDG1_Init(void)
{
   hiwdg1.Instance = IWDG1;
   hiwdg1.Init.Prescaler = IWDG_PRESCALER_64;
   hiwdg1.Init.Window = 4095;
   hiwdg1.Init.Reload = 500;
   if (HAL_IWDG_Init(&hiwdg1) != HAL_OK)
   {
      Error_Handler();
   }
}

void User_main(void)
{
   BspFDCANInit();
   while (1)
   {
      can_process();
   }
}
