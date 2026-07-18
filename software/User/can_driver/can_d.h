#ifndef CAN_D_H
#define CAN_D_H

#include <stdbool.h>
#include <stdint.h>
#include "fdcan.h"

enum
{
    CAN_BUS_1,
    CAN_BUS_2,
    CAN_BUS_TOTAL
};

typedef struct
{
    uint32_t id;
    bool fdf;
    bool ide;
    bool brs;
    uint8_t len;
    uint8_t data[64];
} can_frame_t;

int fdcan_config(FDCAN_HandleTypeDef *hfdcan, uint32_t nomi_bitrate, uint32_t data_bitrate);
int can_connect(uint8_t bus);
int can_unconnect(uint8_t bus);
int can_send(uint8_t channel, uint32_t id, bool ide, bool fdf, bool brs, const uint8_t *data, uint8_t len);
int can_receive(uint8_t bus, can_frame_t *frame);
uint32_t can_get_rx_drop_count(uint8_t bus);
uint32_t can_get_error_status(uint8_t bus);
uint32_t can_get_hal_error(uint8_t bus);
void can_clear_errors(uint8_t bus);
void can_process(void);
int can_set_bus_active(uint8_t bus, bool active);
void can_protocol_init(void);
#endif /* CAN_D_H */
