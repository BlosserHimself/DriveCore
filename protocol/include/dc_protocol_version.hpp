#pragma once

#define DC_PROTOCOL_MAJOR 1
#define DC_PROTOCOL_MINOR 0
#define DC_PROTOCOL_PATCH 0

#define DC_PROTOCOL_VERSION ((DC_PROTOCOL_MAJOR << 16) | \
                             (DC_PROTOCOL_MINOR << 8) | \
                             (DC_PROTOCOL_PATCH))