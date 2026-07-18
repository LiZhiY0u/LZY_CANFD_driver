#include "fdcan.h"
#include "can_d.h"
#include "stm32h7xx_hal.h"
#include <string.h>

#define CAN_TX_QUEUE_SIZE 128
#define CAN_RX_QUEUE_SIZE 32
#define CAN_BUS_OFF_RECOVERY_DELAY_MS 100U

#define util_arraylen(array) (sizeof(array) / sizeof(array[0]))

typedef void (*can_rx_indicate_t)(uint8_t bus, uint32_t id, bool ide, bool fdf, bool brs,
                                  const uint8_t *data, uint8_t len);

typedef struct
{
    uint32_t bitrate;
    uint16_t brp;
    uint16_t sjw;
    uint16_t tseg1;
    uint16_t tseg2;
} bitrate_config_t;

static can_frame_t frm_buffer[CAN_BUS_TOTAL][CAN_TX_QUEUE_SIZE];
static uint32_t wr_pos[CAN_BUS_TOTAL], rd_pos[CAN_BUS_TOTAL], tx_num[CAN_BUS_TOTAL];
static can_frame_t rx_buffer[CAN_BUS_TOTAL][CAN_RX_QUEUE_SIZE];
static uint32_t rx_wr_pos[CAN_BUS_TOTAL], rx_rd_pos[CAN_BUS_TOTAL], rx_num[CAN_BUS_TOTAL];
static uint32_t rx_drop_count[CAN_BUS_TOTAL];
static volatile uint32_t error_status[CAN_BUS_TOTAL], hal_error_code[CAN_BUS_TOTAL];
static volatile uint32_t bus_off_recovery_tick[CAN_BUS_TOTAL];
static volatile bool bus_off_recovery_pending[CAN_BUS_TOTAL];
static bool bus_active[CAN_BUS_TOTAL];
static can_rx_indicate_t fdcan1_rx_indicate = NULL, fdcan2_rx_indicate = NULL;
static const bitrate_config_t bitrate_configs[] = {
    // FDCAN kernel clock = 120 MHz
    // bitrate = clock / (brp * (1 + tseg1 + tseg2))
    // sample point = (1 + tseg1) / (1 + tseg1 + tseg2) * 100%
    // clang-format off
    // bitrate   brp  sjw  tseg1  tseg2
    {  100000,    24,  10,    39,    10 },  // 100 kbit/s
    {  200000,    12,  10,    39,    10 },  // 200 kbit/s
    {  500000,     4,  10,    47,    12 },  // 500 kbit/s
    {  800000,     3,  10,    39,    10 },  // 800 kbit/s
    { 1000000,     4,   5,    23,     6 },  // 1 Mbit/s
    { 2000000,     2,   5,    23,     6 },  // 2 Mbit/s
    { 4000000,     1,   5,    23,     6 },  // 4 Mbit/s
    { 5000000,     1,   5,    17,     6 },  // 5 Mbit/s
    { 8000000,     1,   1,    10,     4 },  // 8 Mbit/s
    {10000000,     3,   1,     2,     1 },  // 10 Mbit/s
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

static FDCAN_HandleTypeDef *can_get_handle(uint8_t bus)
{
    if (bus == CAN_BUS_1)
    {
        return &hfdcan1;
    }
    if (bus == CAN_BUS_2)
    {
        return &hfdcan2;
    }
    return NULL;
}

static int can_get_bus(FDCAN_HandleTypeDef *hfdcan)
{
    if (hfdcan == &hfdcan1)
    {
        return CAN_BUS_1;
    }
    if (hfdcan == &hfdcan2)
    {
        return CAN_BUS_2;
    }
    return -1;
}

static void can_queue_reset(uint8_t bus)
{
    wr_pos[bus] = 0;
    rd_pos[bus] = 0;
    tx_num[bus] = 0;
    rx_wr_pos[bus] = 0;
    rx_rd_pos[bus] = 0;
    rx_num[bus] = 0;
    rx_drop_count[bus] = 0;
}

static void can_rx_proc(void)
{
    FDCAN_RxHeaderTypeDef FDCAN1_RxHeader, FDCAN2_RxHeader;
    uint8_t FD1_data[64], FD2_data[64];
    if (bus_active[CAN_BUS_1] && !bus_off_recovery_pending[CAN_BUS_1] &&
        HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0) > 0)
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

    if (bus_active[CAN_BUS_2] && !bus_off_recovery_pending[CAN_BUS_2] &&
        HAL_FDCAN_GetRxFifoFillLevel(&hfdcan2, FDCAN_RX_FIFO1) > 0)
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

static void can_tx_bus_proc(uint8_t bus, FDCAN_HandleTypeDef *hfdcan)
{
    if (!bus_active[bus] || bus_off_recovery_pending[bus] || tx_num[bus] == 0 ||
        HAL_FDCAN_GetTxFifoFreeLevel(hfdcan) == 0)
    {
        return;
    }

    FDCAN_TxHeaderTypeDef header = {0};
    can_frame_t *frame = &frm_buffer[bus][rd_pos[bus]];

    header.Identifier = frame->id;
    header.BitRateSwitch = frame->brs ? FDCAN_BRS_ON : FDCAN_BRS_OFF;
    header.IdType = frame->ide ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
    header.FDFormat = frame->fdf ? FDCAN_FD_CAN : FDCAN_CLASSIC_CAN;
    header.DataLength = len_to_dlc(frame->len);
    header.TxFrameType = FDCAN_DATA_FRAME;
    header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    header.MessageMarker = 0;

    if (HAL_FDCAN_AddMessageToTxFifoQ(hfdcan, &header, frame->data) == HAL_OK)
    {
        rd_pos[bus] = (rd_pos[bus] + 1) % CAN_TX_QUEUE_SIZE;
        tx_num[bus]--;
    }
}

static void can_tx_proc(void)
{
    can_tx_bus_proc(CAN_BUS_1, &hfdcan1);
    can_tx_bus_proc(CAN_BUS_2, &hfdcan2);
}

static void can_bus_off_recovery_proc(void)
{
    uint32_t now = HAL_GetTick();

    for (uint8_t bus = 0; bus < CAN_BUS_TOTAL; bus++)
    {
        if (!bus_active[bus] || !bus_off_recovery_pending[bus] ||
            (int32_t)(now - bus_off_recovery_tick[bus]) < 0)
        {
            continue;
        }

        FDCAN_HandleTypeDef *hfdcan = can_get_handle(bus);
        HAL_FDCAN_StateTypeDef state = HAL_FDCAN_GetState(hfdcan);
        if (state == HAL_FDCAN_STATE_BUSY)
        {
            if (HAL_FDCAN_Stop(hfdcan) != HAL_OK)
            {
                hal_error_code[bus] |= hfdcan->ErrorCode;
                bus_off_recovery_tick[bus] = now + CAN_BUS_OFF_RECOVERY_DELAY_MS;
                continue;
            }
            state = HAL_FDCAN_GetState(hfdcan);
        }

        if (state == HAL_FDCAN_STATE_READY && HAL_FDCAN_Start(hfdcan) == HAL_OK)
        {
            bus_off_recovery_pending[bus] = false;
        }
        else
        {
            hal_error_code[bus] |= hfdcan->ErrorCode;
            bus_off_recovery_tick[bus] = now + CAN_BUS_OFF_RECOVERY_DELAY_MS;
        }
    }
}

void can_process(void)
{
    can_bus_off_recovery_proc();
    can_rx_proc();
    can_tx_proc();
}

/// @brief Send a CAN frame
/// @param channel 通道号
/// @param id 帧ID
/// @param ide 是否为扩展帧
/// @param fdf 是否为CAN-FD帧
/// @param brs 是否启用bitrate switch
/// @param data 数据指针
/// @param len 数据长度
/// @return
int can_send(uint8_t channel, uint32_t id, bool ide, bool fdf, bool brs, const uint8_t *data, uint8_t len)
{
    if (channel >= CAN_BUS_TOTAL || len > 64 || (len > 0 && data == NULL))
    {
        return -1;
    }
    if (!bus_active[channel])
    {
        return -1;
    }
    if ((!ide && id > 0x7ffU) || (ide && id > 0x1fffffffU))
    {
        return -1;
    }
    if ((!fdf && len > 8) || (!fdf && brs))
    {
        return -1;
    }
    if (tx_num[channel] >= CAN_TX_QUEUE_SIZE)
    {
        return -1;
    }

    can_frame_t *frame = &frm_buffer[channel][wr_pos[channel]];
    frame->id = id;
    frame->ide = ide;
    frame->fdf = fdf;
    frame->brs = brs;
    frame->len = len;
    memset(frame->data, 0, sizeof(frame->data));
    if (len > 0)
    {
        memcpy(frame->data, data, len);
    }
    wr_pos[channel] = (wr_pos[channel] + 1) % CAN_TX_QUEUE_SIZE;
    tx_num[channel]++;
    return 0;
}

static bool is_valid_data_bitrate_config(const bitrate_config_t *config)
{
    return config->brp >= 1U && config->brp <= 32U &&
           config->sjw >= 1U && config->sjw <= 16U &&
           config->tseg1 >= 1U && config->tseg1 <= 32U &&
           config->tseg2 >= 1U && config->tseg2 <= 16U &&
           ((uint32_t)config->brp * config->tseg1) <= 0x7fU;
}

int can_receive(uint8_t bus, can_frame_t *frame)
{
    if (bus >= CAN_BUS_TOTAL || frame == NULL || rx_num[bus] == 0)
    {
        return -1;
    }

    *frame = rx_buffer[bus][rx_rd_pos[bus]];
    rx_rd_pos[bus] = (rx_rd_pos[bus] + 1) % CAN_RX_QUEUE_SIZE;
    rx_num[bus]--;
    return 0;
}

uint32_t can_get_rx_drop_count(uint8_t bus)
{
    if (bus >= CAN_BUS_TOTAL)
    {
        return 0;
    }
    return rx_drop_count[bus];
}

uint32_t can_get_error_status(uint8_t bus)
{
    return (bus < CAN_BUS_TOTAL) ? error_status[bus] : 0;
}

uint32_t can_get_hal_error(uint8_t bus)
{
    return (bus < CAN_BUS_TOTAL) ? hal_error_code[bus] : 0;
}

void can_clear_errors(uint8_t bus)
{
    if (bus < CAN_BUS_TOTAL)
    {
        error_status[bus] = 0;
        hal_error_code[bus] = 0;
        can_get_handle(bus)->ErrorCode = HAL_FDCAN_ERROR_NONE;
    }
}

static int FDCAN_Filter_Init(FDCAN_HandleTypeDef *hfdcan)
{
    FDCAN_FilterTypeDef sFilterConfig = {0};

    if (hfdcan == &hfdcan1)
    {
        sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    }
    else if (hfdcan == &hfdcan2)
    {
        sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO1;
    }
    else
    {
        return -1;
    }

    sFilterConfig.FilterIndex = 0;
    sFilterConfig.FilterType = FDCAN_FILTER_MASK;
    sFilterConfig.FilterID1 = 0;
    sFilterConfig.FilterID2 = 0;

    sFilterConfig.IdType = FDCAN_STANDARD_ID;
    if (HAL_FDCAN_ConfigFilter(hfdcan, &sFilterConfig) != HAL_OK)
    {
        return -1;
    }

    sFilterConfig.IdType = FDCAN_EXTENDED_ID;
    if (HAL_FDCAN_ConfigFilter(hfdcan, &sFilterConfig) != HAL_OK)
    {
        return -1;
    }

    if (HAL_FDCAN_ConfigGlobalFilter(hfdcan, FDCAN_REJECT, FDCAN_REJECT,
                                     FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE) != HAL_OK ||
        HAL_FDCAN_ActivateNotification(hfdcan,
                                       FDCAN_IT_ERROR_WARNING |
                                           FDCAN_IT_ERROR_PASSIVE |
                                           FDCAN_IT_BUS_OFF |
                                           FDCAN_IT_ARB_PROTOCOL_ERROR |
                                           FDCAN_IT_DATA_PROTOCOL_ERROR,
                                       0) != HAL_OK ||
        HAL_FDCAN_ConfigTxDelayCompensation(hfdcan,
                                            hfdcan->Init.DataPrescaler * hfdcan->Init.DataTimeSeg1,
                                            1) != HAL_OK ||
        HAL_FDCAN_EnableTxDelayCompensation(hfdcan) != HAL_OK ||
        HAL_FDCAN_Start(hfdcan) != HAL_OK)
    {
        return -1;
    }

    return 0;
}

int fdcan_config(FDCAN_HandleTypeDef *hfdcan, uint32_t nomi_bitrate, uint32_t data_bitrate)
{
    int nomi_cfg = -1, data_cfg = -1;

    if (hfdcan != &hfdcan1 && hfdcan != &hfdcan2)
    {
        return -1;
    }

    for (uint32_t i = 0; i < util_arraylen(bitrate_configs); i++)
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

    if (nomi_cfg < 0 || data_cfg < 0)
    {
        return -1;
    }
    if (!is_valid_data_bitrate_config(&bitrate_configs[data_cfg]))
    {
        return -1;
    }

    uint8_t bus = (hfdcan == &hfdcan1) ? CAN_BUS_1 : CAN_BUS_2;
    bus_active[bus] = false;
    can_queue_reset(bus);
    error_status[bus] = 0;
    hal_error_code[bus] = 0;
    bus_off_recovery_pending[bus] = false;

    HAL_FDCAN_StateTypeDef state = HAL_FDCAN_GetState(hfdcan);
    if (state == HAL_FDCAN_STATE_BUSY && HAL_FDCAN_Stop(hfdcan) != HAL_OK)
    {
        return -1;
    }
    if (state != HAL_FDCAN_STATE_BUSY && state != HAL_FDCAN_STATE_READY &&
        state != HAL_FDCAN_STATE_RESET && state != HAL_FDCAN_STATE_ERROR)
    {
        return -1;
    }
    if (HAL_FDCAN_DeInit(hfdcan) != HAL_OK)
    {
        return -1;
    }

    if (hfdcan == &hfdcan1)
    {
        hfdcan->Instance = FDCAN1;
        hfdcan->Init.FrameFormat = FDCAN_FRAME_FD_BRS;
        hfdcan->Init.Mode = FDCAN_MODE_NORMAL;
        hfdcan->Init.AutoRetransmission = ENABLE;
        hfdcan->Init.TransmitPause = DISABLE;
        hfdcan->Init.ProtocolException = ENABLE;

        hfdcan->Init.NominalPrescaler = bitrate_configs[nomi_cfg].brp;
        hfdcan->Init.NominalSyncJumpWidth = bitrate_configs[nomi_cfg].sjw;
        hfdcan->Init.NominalTimeSeg1 = bitrate_configs[nomi_cfg].tseg1;
        hfdcan->Init.NominalTimeSeg2 = bitrate_configs[nomi_cfg].tseg2;
        hfdcan->Init.DataPrescaler = bitrate_configs[data_cfg].brp;
        hfdcan->Init.DataSyncJumpWidth = bitrate_configs[data_cfg].sjw;
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
            return -1;
        }
        if (FDCAN_Filter_Init(hfdcan) != 0)
        {
            return -1;
        }
    }

    if (hfdcan == &hfdcan2)
    {
        hfdcan->Instance = FDCAN2;
        hfdcan->Init.FrameFormat = FDCAN_FRAME_FD_BRS;
        hfdcan->Init.Mode = FDCAN_MODE_NORMAL;
        hfdcan->Init.AutoRetransmission = ENABLE;
        hfdcan->Init.TransmitPause = DISABLE;
        hfdcan->Init.ProtocolException = ENABLE;

        hfdcan->Init.NominalPrescaler = bitrate_configs[nomi_cfg].brp;
        hfdcan->Init.NominalSyncJumpWidth = bitrate_configs[nomi_cfg].sjw;
        hfdcan->Init.NominalTimeSeg1 = bitrate_configs[nomi_cfg].tseg1;
        hfdcan->Init.NominalTimeSeg2 = bitrate_configs[nomi_cfg].tseg2;
        hfdcan->Init.DataPrescaler = bitrate_configs[data_cfg].brp;
        hfdcan->Init.DataSyncJumpWidth = bitrate_configs[data_cfg].sjw;
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
            return -1;
        }
        if (FDCAN_Filter_Init(hfdcan) != 0)
        {
            return -1;
        }
    }

    return 0;
}

int can_connect(uint8_t bus)
{
    FDCAN_HandleTypeDef *hfdcan = can_get_handle(bus);
    if (hfdcan == NULL)
    {
        return -1;
    }

    if (bus_active[bus])
    {
        return 0;
    }

    HAL_FDCAN_StateTypeDef state = HAL_FDCAN_GetState(hfdcan);
    if (state == HAL_FDCAN_STATE_READY)
    {
        if (HAL_FDCAN_Start(hfdcan) != HAL_OK)
        {
            return -1;
        }
    }
    else if (state != HAL_FDCAN_STATE_BUSY)
    {
        return -1;
    }

    can_queue_reset(bus);
    bus_active[bus] = true;
    return 0;
}

int can_unconnect(uint8_t bus)
{
    FDCAN_HandleTypeDef *hfdcan = can_get_handle(bus);
    if (hfdcan == NULL)
    {
        return -1;
    }

    HAL_FDCAN_StateTypeDef state = HAL_FDCAN_GetState(hfdcan);
    if (state == HAL_FDCAN_STATE_BUSY)
    {
        if (HAL_FDCAN_Stop(hfdcan) != HAL_OK)
        {
            return -1;
        }
    }
    else if (state != HAL_FDCAN_STATE_READY)
    {
        bus_active[bus] = false;
        bus_off_recovery_pending[bus] = false;
        can_queue_reset(bus);
        return -1;
    }

    bus_active[bus] = false;
    bus_off_recovery_pending[bus] = false;
    can_queue_reset(bus);
    return 0;
}

// bus-off错误回调
void HAL_FDCAN_ErrorStatusCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t ErrorStatusITs)
{
    int bus = can_get_bus(hfdcan);
    if (bus < 0)
    {
        return;
    }

    error_status[bus] |= ErrorStatusITs;
    if ((ErrorStatusITs & FDCAN_IT_BUS_OFF) != 0U)
    {
        bus_off_recovery_tick[bus] = HAL_GetTick() + CAN_BUS_OFF_RECOVERY_DELAY_MS;
        bus_off_recovery_pending[bus] = true;
    }
}

void HAL_FDCAN_ErrorCallback(FDCAN_HandleTypeDef *hfdcan)
{
    int bus = can_get_bus(hfdcan);
    if (bus >= 0)
    {
        hal_error_code[bus] |= hfdcan->ErrorCode;
    }
}

static void can_install_rx_callback(uint8_t bus, can_rx_indicate_t rx_indicate)
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

int can_set_bus_active(uint8_t bus, bool active)
{
    if (active)
    {
        return can_connect(bus);
    }
    return can_unconnect(bus);
}

static void can_protocol_rx_frame(uint8_t channel, uint32_t id, bool ide, bool fdf, bool brs,
                                  const uint8_t *data, uint8_t len)
{
    if (channel >= CAN_BUS_TOTAL || len > 64 || (len > 0 && data == NULL))
    {
        return;
    }
    if (rx_num[channel] >= CAN_RX_QUEUE_SIZE)
    {
        rx_drop_count[channel]++;
        return;
    }

    can_frame_t *frame = &rx_buffer[channel][rx_wr_pos[channel]];
    frame->id = id;
    frame->ide = ide;
    frame->fdf = fdf;
    frame->brs = brs;
    frame->len = len;
    memset(frame->data, 0, sizeof(frame->data));
    if (len > 0)
    {
        memcpy(frame->data, data, len);
    }

    rx_wr_pos[channel] = (rx_wr_pos[channel] + 1) % CAN_RX_QUEUE_SIZE;
    rx_num[channel]++;
}

void can_protocol_init(void)
{
    can_install_rx_callback(CAN_BUS_1, can_protocol_rx_frame); // 安装CAN接收回调

    can_install_rx_callback(CAN_BUS_2, can_protocol_rx_frame);
}
