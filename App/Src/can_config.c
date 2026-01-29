#include "can_config.h"

/*
 * Edit this allowlist to restrict which standard arbitration IDs are decoded
 * into parameters.
 *
 * Example:
 *   const uint32_t g_can_rx_id_filter[] = { 0x080, 0x123, 0x456 };
 *
 * If the list is empty (count == 0), all standard IDs are accepted.
 */

const uint32_t g_can_rx_id_filter[] =
{
  /* empty = accept all */
};

const size_t g_can_rx_id_filter_count = (sizeof(g_can_rx_id_filter) / sizeof(g_can_rx_id_filter[0]));
