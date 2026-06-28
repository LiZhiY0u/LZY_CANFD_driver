#include "fdcan.h"
#include "can.h"
#include "stm32h7xx_hal.h"
#include <string.h>
#include "Usart_driver/usart_log.h"

#define CAN_TX_QUEUE_SIZE 128

#ifndef nullptr
#define nullptr ((void *)0)
#endif

typedef struct
{
    uint32_t frq;
    uint32_t bitrate;
    uint8_t sample;
    uint8_t brp;
    uint8_t tsync;
    uint8_t tseg1;
    uint8_t tseg2;
} bitrate_config_t;

typedef struct
{
    uint32_t id;
    bool fdf;
    bool ide;
    bool brs;
    uint8_t len;
    uint8_t data[64];
} can_frame_t;

extern FDCAN_HandleTypeDef hfdcan1;
static can_frame_t frm_buffer[CAN_TX_QUEUE_SIZE];
static uint32_t wr_pos[CAN_BUS_TOTAL], rd_pos[CAN_BUS_TOTAL], tx_num[CAN_BUS_TOTAL];
static bool connected = false;
static can_err_notify_t can_err_notify = nullptr;
static can_tx_confirm_t can_tx_confirm = nullptr;
static can_rx_indicate_t fdcan1_rx_indicate = nullptr, fdcan2_rx_indicate = nullptr;
const static bitrate_config_t bitrate_configs[] = {
    // clock   = apb1 = 120MHz
    // bitrate = clock / (brp * (tsync + tseg1 + tseg2))
    // sample  = (tsync + tseg1) / (tsync + tseg1 + tseg2) * 100%
    // clang-format off
    // frq        bitrate  sample(%)  brp, tsync  tseg1  tseg2
    {  120000000, 100000,   80,       24,  10,     39,    10   },  // 100k
    {  120000000, 200000,   80,       12,  10,     39,    10   },  // 200k
    {  120000000, 500000,   80,        4,  10,     47,    12   },  // 500k
    {  120000000, 800000,   80,        3,  10,     39,    10   },  // 800k
    {  120000000, 1000000,  80,        4,   5,     23,    6    },  // 1.00M
    {  120000000, 2000000,  80,        2,   5,     23,    6    },  // 2.00M
    {  120000000, 4000000,  80,        1,   5,     23,    6    },  // 4.00M
    {  120000000, 5000000,  75,        1,   5,     17,    6    },  // 5.00M
    {  120000000, 8000000,  73,        1,   1,     10,    4    },  // 8.00M
    {  120000000, 10000000, 75,        3,   1,     2,     1    },  // 10.00M
    // clang-format on
};

static uint8_t dlc_to_len(uint32_t dlc)
{
    static const uint8_t dlc2len[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};
    return dlc2len[dlc & 0x0f];
}

static uint32_t len_to_dlc(uint8_t len)
{
    if (len <= 8)
    {
        return len;
    }
    if (len <= 12)
    {
        return FDCAN_DLC_BYTES_12;
    }
    if (len <= 16)
    {
        return FDCAN_DLC_BYTES_16;
    }
    if (len <= 20)
    {
        return FDCAN_DLC_BYTES_20;
    }
    if (len <= 24)
    {
        return FDCAN_DLC_BYTES_24;
    }
    if (len <= 32)
    {
        return FDCAN_DLC_BYTES_32;
    }
    if (len <= 48)
    {
        return FDCAN_DLC_BYTES_48;
    }
    if (len <= 64)
    {
        return FDCAN_DLC_BYTES_64;
    }
    return 0;
}

static void can_rx_proc(void)
{
    FDCAN_RxHeaderTypeDef FDCAN1_RxHeader, FDCAN2_RxHeader;
    uint8_t FD1_data[64], FD2_data[64];
    if (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0) > 0)
    {
        if (HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &FDCAN1_RxHeader, FD1_data) == HAL_OK)
        {
            if (fdcan1_rx_indicate) // 是否有效
            {
                fdcan1_rx_indicate(0,                                             // bus
                                   FDCAN1_RxHeader.Identifier,                    // id
                                   FDCAN1_RxHeader.IdType,                        // ide
                                   FDCAN1_RxHeader.FDFormat == FDCAN_FD_CAN,      // fdf
                                   FDCAN1_RxHeader.BitRateSwitch == FDCAN_BRS_ON, // brs
                                   FD1_data,                                      // data
                                   dlc_to_len(FDCAN1_RxHeader.DataLength)         // len
                );
            }
        }
    }

    if (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan2, FDCAN_RX_FIFO1) > 0)
    {
        if (HAL_FDCAN_GetRxMessage(&hfdcan2, FDCAN_RX_FIFO1, &FDCAN2_RxHeader, FD2_data) == HAL_OK)
        {
            if (fdcan2_rx_indicate)
            {
                fdcan2_rx_indicate(1,                                             // bus
                                   FDCAN2_RxHeader.Identifier,                    // id
                                   FDCAN2_RxHeader.IdType,                        // ide
                                   FDCAN2_RxHeader.FDFormat == FDCAN_FD_CAN,      // fdf
                                   FDCAN2_RxHeader.BitRateSwitch == FDCAN_BRS_ON, // brs
                                   FD2_data,                                      // data
                                   dlc_to_len(FDCAN2_RxHeader.DataLength)         // len
                );
            }
        }
    }
}

uint8_t tempData1[8] = {1, 2, 3, 4, 5, 6, 7, 8};

FDCAN_TxHeaderTypeDef FDCAN1_TxHeader;
uint32_t FDCAN1_Send_Msgs(uint32_t can_id, uint8_t *msg, uint32_t length_code)
{

    FDCAN1_TxHeader.Identifier = can_id;
    FDCAN1_TxHeader.IdType = FDCAN_STANDARD_ID;
    FDCAN1_TxHeader.TxFrameType = FDCAN_DATA_FRAME;
    FDCAN1_TxHeader.DataLength = FDCAN_DLC_BYTES_8;
    FDCAN1_TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    FDCAN1_TxHeader.BitRateSwitch = FDCAN_BRS_ON;
    FDCAN1_TxHeader.FDFormat = FDCAN_FD_CAN;
    FDCAN1_TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    FDCAN1_TxHeader.MessageMarker = 0;

    return HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &FDCAN1_TxHeader, msg);
}

static void can_tx_proc(void)
{
    if (tx_num[CAN_BUS_1] != 0 && HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1) > 0)
    {
        FDCAN_TxHeaderTypeDef header1 = {0}; // important to init 0
        can_frame_t *frame = frm_buffer + rd_pos[CAN_BUS_1];

        rd_pos[CAN_BUS_1] = (rd_pos[CAN_BUS_1] + 1) % CAN_TX_QUEUE_SIZE;
        tx_num[CAN_BUS_1]--;

        header1.Identifier = frame->id;
        header1.BitRateSwitch = frame->brs ? FDCAN_BRS_ON : FDCAN_BRS_OFF;
        header1.IdType = frame->ide ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
        header1.FDFormat = frame->fdf ? FDCAN_FD_CAN : FDCAN_CLASSIC_CAN;
        header1.DataLength = len_to_dlc(frame->len);
        header1.TxFrameType = FDCAN_DATA_FRAME;
        header1.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
        header1.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
        header1.MessageMarker = 0;

        if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &header1, frame->data) != 0)
        {
            // log_printf("send frame fail\n");
        }
    }
    if (tx_num[CAN_BUS_2] != 0 && HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan2) > 0)
    {
        FDCAN_TxHeaderTypeDef header2 = {0}; // important to init 0
        can_frame_t *frame = frm_buffer + rd_pos[CAN_BUS_2];

        rd_pos[CAN_BUS_2] = (rd_pos[CAN_BUS_2] + 1) % CAN_TX_QUEUE_SIZE;
        tx_num[CAN_BUS_2]--;

        header2.Identifier = frame->id;
        header2.BitRateSwitch = frame->brs ? FDCAN_BRS_ON : FDCAN_BRS_OFF;
        header2.IdType = frame->ide ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
        header2.FDFormat = frame->fdf ? FDCAN_FD_CAN : FDCAN_CLASSIC_CAN;
        header2.DataLength = len_to_dlc(frame->len);
        header2.TxFrameType = FDCAN_DATA_FRAME;
        header2.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
        header2.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
        header2.MessageMarker = 0;

        if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &header2, frame->data) != 0)
        {
            // log_printf("send frame fail\n");
        }
    }
}

static void can_state_reset(void)
{
    wr_pos[CAN_BUS_1] = 0;
    wr_pos[CAN_BUS_2] = 0;
    rd_pos[CAN_BUS_1] = 0;
    rd_pos[CAN_BUS_2] = 0;
    tx_num[CAN_BUS_1] = 0;
    tx_num[CAN_BUS_2] = 0;
    connected = false;
}

void can_process(void)
{
    if (connected)
    {
        can_rx_proc();
        can_tx_proc();
    }
}

// id:id ide:扩展帧 fdf: CAN-FD帧 brs: bitrate switch data:数据指针 len:数据长度
int can_send(uint8_t channel, uint32_t id, bool ide, bool fdf, bool brs, const uint8_t *data, uint8_t len)
{
    if (tx_num[channel] == CAN_TX_QUEUE_SIZE)
    {
        return -1;
    }
    frm_buffer[wr_pos[channel]].id = id;
    frm_buffer[wr_pos[channel]].ide = ide;
    frm_buffer[wr_pos[channel]].fdf = fdf;
    frm_buffer[wr_pos[channel]].brs = brs;
    frm_buffer[wr_pos[channel]].len = len;
    memcpy(frm_buffer[wr_pos[channel]].data, data, len);
    wr_pos[channel] = (wr_pos[channel] + 1) % CAN_TX_QUEUE_SIZE;
    tx_num[channel]++;
    return 0;
}

void FDCAN_Filter_Init(FDCAN_HandleTypeDef *hfdcan)
{
    FDCAN_FilterTypeDef sFilterConfig = {0};

    sFilterConfig.IdType = FDCAN_STANDARD_ID;
    sFilterConfig.FilterIndex = 0;
    sFilterConfig.FilterType = FDCAN_FILTER_MASK;
    sFilterConfig.FilterID1 = 0;
    sFilterConfig.FilterID2 = 0;
    sFilterConfig.RxBufferIndex = 0;
    sFilterConfig.IsCalibrationMsg = 0;

    if (hfdcan == &hfdcan1)
    {
        sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    }
    else if (hfdcan == &hfdcan2)
    {
        sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO1;
    }

    HAL_FDCAN_ConfigFilter(hfdcan, &sFilterConfig);
    HAL_FDCAN_ConfigGlobalFilter(hfdcan, FDCAN_REJECT, FDCAN_REJECT, FDCAN_FILTER_REMOTE, FDCAN_REJECT_REMOTE);

    if (hfdcan == &hfdcan1)
    {
        HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
    }
    else if (hfdcan == &hfdcan2)
    {
        HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_RX_FIFO1_NEW_MESSAGE, 0);
    }

    HAL_FDCAN_ConfigTxDelayCompensation(hfdcan, hfdcan->Init.DataPrescaler * hfdcan->Init.DataTimeSeg1, 1);
    HAL_FDCAN_EnableTxDelayCompensation(hfdcan);

    HAL_FDCAN_Start(hfdcan);
}

int fdcan_config(FDCAN_HandleTypeDef *hfdcan, uint32_t nomi_bitrate, uint8_t nomi_sample, uint32_t data_bitrate, uint8_t data_sample)
{

    int nomi_cfg = -1, data_cfg = -1;

    for (int i = 0; i < util_arraylen(bitrate_configs); i++)
    {
        if (bitrate_configs[i].bitrate == nomi_bitrate)
        {
            nomi_cfg = i;
        }
        if (bitrate_configs[i].bitrate == data_bitrate)
        {
            data_cfg = i;
        }
    }

    if (hfdcan == &hfdcan1)
    {
        HAL_FDCAN_Stop(hfdcan);
        HAL_FDCAN_DeInit(hfdcan);

        hfdcan->Instance = FDCAN1;
        hfdcan->Init.FrameFormat = FDCAN_FRAME_FD_BRS;
        hfdcan->Init.Mode = FDCAN_MODE_NORMAL;
        hfdcan->Init.AutoRetransmission = ENABLE;
        hfdcan->Init.TransmitPause = DISABLE;
        hfdcan->Init.ProtocolException = ENABLE;

        hfdcan->Init.NominalPrescaler = bitrate_configs[nomi_cfg].brp;
        hfdcan->Init.NominalSyncJumpWidth = bitrate_configs[nomi_cfg].tsync;
        hfdcan->Init.NominalTimeSeg1 = bitrate_configs[nomi_cfg].tseg1;
        hfdcan->Init.NominalTimeSeg2 = bitrate_configs[nomi_cfg].tseg2;
        hfdcan->Init.DataPrescaler = bitrate_configs[data_cfg].brp;
        hfdcan->Init.DataSyncJumpWidth = bitrate_configs[data_cfg].tsync;
        hfdcan->Init.DataTimeSeg1 = bitrate_configs[data_cfg].tseg1;
        hfdcan->Init.DataTimeSeg2 = bitrate_configs[data_cfg].tseg2;

        hfdcan->Init.MessageRAMOffset = 0;
        hfdcan->Init.StdFiltersNbr = 1;
        hfdcan->Init.ExtFiltersNbr = 1;
        hfdcan->Init.RxFifo0ElmtsNbr = 32;
        hfdcan->Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_64;
        hfdcan->Init.RxFifo1ElmtsNbr = 0;
        hfdcan->Init.RxFifo1ElmtSize = FDCAN_DATA_BYTES_64;
        hfdcan->Init.RxBuffersNbr = 0;
        hfdcan->Init.RxBufferSize = FDCAN_DATA_BYTES_64;
        hfdcan->Init.TxEventsNbr = 0;
        hfdcan->Init.TxBuffersNbr = 0;
        hfdcan->Init.TxFifoQueueElmtsNbr = 32;
        hfdcan->Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
        hfdcan->Init.TxElmtSize = FDCAN_DATA_BYTES_64;

        if (HAL_FDCAN_Init(hfdcan) != HAL_OK)
        {
            Error_Handler();
        }
        FDCAN_Filter_Init(hfdcan);
    }

    if (hfdcan == &hfdcan2)
    {
        HAL_FDCAN_Stop(hfdcan);
        HAL_FDCAN_DeInit(hfdcan);

        hfdcan->Instance = FDCAN2;
        hfdcan->Init.FrameFormat = FDCAN_FRAME_FD_BRS;
        hfdcan->Init.Mode = FDCAN_MODE_NORMAL;
        hfdcan->Init.AutoRetransmission = ENABLE;
        hfdcan->Init.TransmitPause = DISABLE;
        hfdcan->Init.ProtocolException = ENABLE;

        hfdcan->Init.NominalPrescaler = bitrate_configs[nomi_cfg].brp;
        hfdcan->Init.NominalSyncJumpWidth = bitrate_configs[nomi_cfg].tsync;
        hfdcan->Init.NominalTimeSeg1 = bitrate_configs[nomi_cfg].tseg1;
        hfdcan->Init.NominalTimeSeg2 = bitrate_configs[nomi_cfg].tseg2;
        hfdcan->Init.DataPrescaler = bitrate_configs[data_cfg].brp;
        hfdcan->Init.DataSyncJumpWidth = bitrate_configs[data_cfg].tsync;
        hfdcan->Init.DataTimeSeg1 = bitrate_configs[data_cfg].tseg1;
        hfdcan->Init.DataTimeSeg2 = bitrate_configs[data_cfg].tseg2;

        hfdcan->Init.MessageRAMOffset = 1280;
        hfdcan->Init.StdFiltersNbr = 1;
        hfdcan->Init.ExtFiltersNbr = 1;
        hfdcan->Init.RxFifo0ElmtsNbr = 0;
        hfdcan->Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_64;
        hfdcan->Init.RxFifo1ElmtsNbr = 32;
        hfdcan->Init.RxFifo1ElmtSize = FDCAN_DATA_BYTES_64;
        hfdcan->Init.RxBuffersNbr = 0;
        hfdcan->Init.RxBufferSize = FDCAN_DATA_BYTES_64;
        hfdcan->Init.TxEventsNbr = 0;
        hfdcan->Init.TxBuffersNbr = 0;
        hfdcan->Init.TxFifoQueueElmtsNbr = 32;
        hfdcan->Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
        hfdcan->Init.TxElmtSize = FDCAN_DATA_BYTES_64;

        if (HAL_FDCAN_Init(hfdcan) != HAL_OK)
        {
            Error_Handler();
        }
        FDCAN_Filter_Init(hfdcan);
    }
}

int can_connect(uint8_t bus)
{
    if (bus == CAN_BUS_1)
    {
        wr_pos[CAN_BUS_1] = 0;
        rd_pos[CAN_BUS_1] = 0;
        tx_num[CAN_BUS_1] = 0;
        // HAL_FDCAN_Start(&hfdcan1);
    }
    else if (bus == CAN_BUS_2)
    {
        wr_pos[CAN_BUS_2] = 0;
        rd_pos[CAN_BUS_2] = 0;
        tx_num[CAN_BUS_2] = 0;
        // HAL_FDCAN_Start(&hfdcan2);
    }
    // log_printf("can connect\n");
    // can_state_reset();
    // // error notification
    // // HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_LIST_PROTOCOL_ERROR, 0);
    // HAL_FDCAN_Start(&hfdcan1);
    connected = true;
    return 0;
}

int can_unconnect(uint8_t bus)
{
    if (bus == CAN_BUS_1)
    {
        HAL_FDCAN_Stop(&hfdcan1);
        // Vofa_FireWater("FDCAN1 stop\n");
    }
    else if (bus == CAN_BUS_2)
    {
        HAL_FDCAN_Stop(&hfdcan2);
        // Vofa_FireWater("FDCAN2 stop\n");
    }
    // log_printf("can unconnect\n");
    connected = false;
    // HAL_FDCAN_Stop(&hfdcan1);
    return 0;
}

// bus-off错误回调
void HAL_FDCAN_ErrorStatusCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t ErrorStatusITs)
{
    // log_printf("Can ErrorStatus %08X\n", ErrorStatusITs);
}

void HAL_FDCAN_ErrorCallback(FDCAN_HandleTypeDef *hfdcan)
{
    // log_printf("Can Error\n");
}

int can_set_filter_mask(uint8_t bus, bool ide, uint32_t id, uint32_t mask)
{
    // todo: set filter
    // HAL_CAN_ConfigFilter(p_can, &filter)
    return 0;
}

void can_install_rx_callback(uint8_t bus, can_rx_indicate_t rx_indicate)
{
    if (bus == CAN_BUS_1)
    {
        fdcan1_rx_indicate = rx_indicate;
    }
    else if (bus == CAN_BUS_2)
    {
        fdcan2_rx_indicate = rx_indicate;
    }
}

void can_install_tx_callback(uint8_t bus, can_tx_confirm_t tx_confirm)
{
    can_tx_confirm = tx_confirm;
}

void can_install_err_callback(uint8_t bus, can_err_notify_t err_notify)
{
    can_err_notify = err_notify;
}

void can_set_silent(uint8_t bus, bool silent)
{
    // todo
    // p_can->Init.Mode = silent_mode ? CAN_MODE_SILENT : CAN_MODE_NORMAL;
}

/* bxCAN does not support FDCAN ISO mode switch */
void can_set_iso_mode(uint8_t bus, bool iso_mode)
{
    // todo
}

void can_set_loopback(uint8_t bus, bool loopback)
{
    // todo
}

void can_set_bus_active(uint8_t bus, bool active)
{
    if (active)
    {
        can_connect(bus);
    }
    else
    {
        can_unconnect(bus);
    }
}

/* set predefined best values */
void can_set_bitrate(uint8_t bus, uint32_t bitrate, uint8_t sample, bool is_data_bitrate)
{
    static uint32_t nomi_bitrate = 1000000;
    static uint32_t data_bitrate = 2000000;
    static uint8_t nomi_sample = 80;
    static uint8_t data_sample = 80;

    static bool flag1 = false, flag2 = false;

    if (!is_data_bitrate)
    {
        // log_printf("pcan set nomi %d-%d%%\n", bitrate, sample);
        nomi_bitrate = bitrate;
        nomi_sample = sample; // unused

        flag1 = true;
    }
    else
    {
        // log_printf("pcan set data %d-%d%%\n", bitrate, sample);
        data_bitrate = bitrate;
        data_sample = sample; // unused

        flag2 = true;
    }

    if (flag1 && flag2)
    {
        flag1 = false;
        flag2 = false;
        if (bus == CAN_BUS_1)
        {
            fdcan_config(&hfdcan1, nomi_bitrate, nomi_sample, data_bitrate, data_sample);
        }
        else if (bus == CAN_BUS_2)
        {
            fdcan_config(&hfdcan2, nomi_bitrate, nomi_sample, data_bitrate, data_sample);
        }
    }
}
