#ifndef CAN_CONFIG_H
#define CAN_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/*
 * Optional CAN ID allowlist for RX processing.
 *
 * - If g_can_rx_id_filter_count == 0, the CAN system accepts ALL standard IDs.
 * - If non-zero, only IDs present in g_can_rx_id_filter[] will be decoded into parameters.
 *
 * The CAN system will also attempt to apply this allowlist as a hardware filter
 * (using CAN IDLIST filter banks). Any IDs beyond available hardware banks will
 * still be enforced in software.
 */
extern const uint32_t g_can_rx_id_filter[];
extern const size_t   g_can_rx_id_filter_count;

#ifdef __cplusplus
}
#endif

#endif /* CAN_CONFIG_H */
