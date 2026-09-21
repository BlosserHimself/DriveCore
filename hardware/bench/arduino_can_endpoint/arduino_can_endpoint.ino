#include <SPI.h>
#include <mcp2515.h>
#include <string.h>

constexpr bool CAN_LOOPBACK = true;
constexpr uint32_t SERIAL_BAUD = 115200;
// Keep these paired: the enum configures MCP2515 and the bit/s value is status text.
constexpr CAN_SPEED CAN_SPEED_SETTING = CAN_500KBPS;
constexpr uint32_t CAN_BITRATE = 500000UL;
constexpr uint8_t CAN_CS_PIN = 10;
constexpr uint8_t CAN_INT_PIN = 8;
constexpr uint8_t MAX_LINE_LENGTH = 79;
constexpr uint32_t MAX_CAN_ID = 0x1FFFFFFFUL;

MCP2515 mcp2515(CAN_CS_PIN);
char command_line[MAX_LINE_LENGTH + 1];
uint8_t command_length = 0;
bool discard_command_line = false;

bool is_space(char value) {
    return value == ' ' || value == '\t';
}

bool is_hex_digit(char value) {
    return (value >= '0' && value <= '9') ||
           (value >= 'A' && value <= 'F') ||
           (value >= 'a' && value <= 'f');
}

uint8_t hex_value(char value) {
    if (value >= '0' && value <= '9') return static_cast<uint8_t>(value - '0');
    if (value >= 'A' && value <= 'F') return static_cast<uint8_t>(value - 'A' + 10);
    return static_cast<uint8_t>(value - 'a' + 10);
}

bool parse_hex(const char* token, uint32_t& value) {
    if (*token == '\0') return false;

    uint32_t parsed = 0;
    while (*token != '\0') {
        if (!is_hex_digit(*token)) return false;
        if (parsed > 0x0FFFFFFFUL) return false;
        parsed = (parsed << 4) | hex_value(*token);
        ++token;
    }
    value = parsed;
    return true;
}

bool parse_decimal(const char* token, uint8_t& value) {
    if (*token == '\0') return false;

    uint16_t parsed = 0;
    while (*token != '\0') {
        if (*token < '0' || *token > '9') return false;
        parsed = static_cast<uint16_t>(parsed * 10 + (*token - '0'));
        if (parsed > 8) return false;
        ++token;
    }
    value = static_cast<uint8_t>(parsed);
    return true;
}

char* next_token(char*& cursor) {
    while (is_space(*cursor)) ++cursor;
    if (*cursor == '\0') return nullptr;

    char* token = cursor;
    while (*cursor != '\0' && !is_space(*cursor)) ++cursor;
    if (*cursor != '\0') {
        *cursor = '\0';
        ++cursor;
    }
    return token;
}

void print_hex_byte(uint8_t value) {
    if (value < 0x10) Serial.print('0');
    Serial.print(value, HEX);
}

void print_can_frame(const struct can_frame& frame) {
    const bool extended = (frame.can_id & CAN_EFF_FLAG) != 0;
    const uint32_t id = frame.can_id & (extended ? CAN_EFF_MASK : CAN_SFF_MASK);

    Serial.print("RX ");
    Serial.print(id, HEX);
    Serial.print(' ');
    Serial.print(frame.can_dlc, DEC);
    for (uint8_t index = 0; index < frame.can_dlc; ++index) {
        Serial.print(' ');
        print_hex_byte(frame.data[index]);
    }
    Serial.println();
}

void report_error(const __FlashStringHelper* reason) {
    Serial.print(F("ERR "));
    Serial.println(reason);
}

void transmit_command(char* line) {
    char* cursor = line;
    char* command = next_token(cursor);
    if (command == nullptr || strcmp(command, "TX") != 0) {
        report_error(F("syntax"));
        return;
    }

    char* id_token = next_token(cursor);
    char* dlc_token = next_token(cursor);
    if (id_token == nullptr || dlc_token == nullptr) {
        report_error(F("syntax"));
        return;
    }

    uint32_t id = 0;
    uint8_t dlc = 0;
    if (!parse_hex(id_token, id)) {
        report_error(F("id"));
        return;
    }
    if (id > MAX_CAN_ID) {
        report_error(F("id_range"));
        return;
    }
    if (!parse_decimal(dlc_token, dlc)) {
        report_error(F("dlc"));
        return;
    }

    struct can_frame frame{};
    frame.can_id = id | CAN_EFF_FLAG;
    frame.can_dlc = dlc;
    for (uint8_t index = 0; index < dlc; ++index) {
        char* byte_token = next_token(cursor);
        uint32_t byte_value = 0;
        if (byte_token == nullptr || !parse_hex(byte_token, byte_value) ||
            byte_value > 0xFF) {
            report_error(F("data"));
            return;
        }
        frame.data[index] = static_cast<uint8_t>(byte_value);
    }

    if (next_token(cursor) != nullptr) {
        report_error(F("byte_count"));
        return;
    }
    if (mcp2515.sendMessage(&frame) != MCP2515::ERROR_OK) {
        report_error(F("can_tx"));
        return;
    }
    Serial.println(F("OK"));
}

void read_serial_commands() {
    while (Serial.available() > 0) {
        const char value = static_cast<char>(Serial.read());
        if (value == '\r' || value == '\n') {
            if (discard_command_line) {
                report_error(F("line_too_long"));
            } else if (command_length > 0) {
                command_line[command_length] = '\0';
                transmit_command(command_line);
            }
            command_length = 0;
            discard_command_line = false;
            continue;
        }

        if (discard_command_line) continue;
        if (command_length >= MAX_LINE_LENGTH) {
            discard_command_line = true;
            continue;
        }
        command_line[command_length++] = value;
    }
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    pinMode(CAN_INT_PIN, INPUT_PULLUP);

    const MCP2515::ERROR reset_result = mcp2515.reset();
    const MCP2515::ERROR bitrate_result = reset_result == MCP2515::ERROR_OK
        ? mcp2515.setBitrate(CAN_SPEED_SETTING, MCP_16MHZ)
        : reset_result;
    const MCP2515::ERROR mode_result =
        reset_result == MCP2515::ERROR_OK &&
        bitrate_result == MCP2515::ERROR_OK
            ? (CAN_LOOPBACK ? mcp2515.setLoopbackMode()
                            : mcp2515.setNormalMode())
            : bitrate_result;

    Serial.print(F("STATUS CAN="));
    Serial.print(
        bitrate_result == MCP2515::ERROR_OK && mode_result == MCP2515::ERROR_OK
            ? F("OK")
            : F("FAIL"));
    Serial.print(F(" MODE="));
    Serial.print(CAN_LOOPBACK ? F("LOOPBACK") : F("NORMAL"));
    Serial.print(F(" BITRATE="));
    Serial.println(CAN_BITRATE);
}

void loop() {
    read_serial_commands();

    struct can_frame frame{};
    while (mcp2515.readMessage(&frame) == MCP2515::ERROR_OK) {
        print_can_frame(frame);
    }
}
