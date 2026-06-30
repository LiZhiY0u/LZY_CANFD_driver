#ifndef _CAN_H_
#define _CAN_H_

#include <stdbool.h>
#include <stdint.h>

enum
{
    CAN_BUS_1,
    CAN_BUS_2,
    CAN_BUS_TOTAL
};

typedef void (*can_rx_indicate_t)(uint8_t bus, uint32_t id, bool ide, bool fdf, bool brs, const uint8_t *data, uint8_t len);
typedef void (*can_tx_confirm_t)(uint8_t bus, uint32_t id, bool ide, bool fdf, bool brs, const uint8_t *data, uint8_t len);
typedef void (*can_err_notify_t)(uint8_t bus, uint32_t err);

int fdcan_config(FDCAN_HandleTypeDef *hfdcan, uint32_t nomi_bitrate, uint32_t data_bitrate);
int can_connect(uint8_t bus);
int can_unconnect(uint8_t bus);
int can_send(uint8_t channel, uint32_t id, bool ide, bool fdf, bool brs, const uint8_t *data, uint8_t len);
void can_process(void);
void can_set_bus_active(uint8_t bus, bool active);
void can_protocol_init(void);
#endif
