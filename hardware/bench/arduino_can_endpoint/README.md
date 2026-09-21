# Arduino CAN Endpoint

Small bench utility for an Arduino Uno R3 with the Inland/Micro Center KS0411 CAN-BUS Shield (SKU MC 464875). It is isolated from DriveCore runtime and protocol headers.

## Hardware and library

- Board: Arduino Uno R3 / ATmega328P
- Shield: Keyestudio KS0411 / Inland MC 464875
- CAN controller: MCP2515
- Library: `autowp/arduino-mcp2515` v1.3.1 or newer
- MCP2515 CS: Arduino D10 (KS0411 `PB2`)
- MCP2515 INT: Arduino D8 (KS0411 `PD8`; this sketch polls, so the interrupt is reserved/not used)
- MCP2515 oscillator: 16 MHz, selected as `MCP_16MHZ`

The 16 MHz selection follows the KS0411 vendor driver timing comments and the shield's supplied 500 kbit/s configuration. The current public KS0411 documentation identifies the MCP2515 and these pin assignments through its supplied `defaults.h`; it does not state the crystal frequency as a separate technical-parameter bullet.

## Configuration

Edit `arduino_can_endpoint.ino`:

```cpp
constexpr bool CAN_LOOPBACK = true;
```

`true` selects MCP2515 internal loopback. `false` selects normal CAN mode. The CAN bitrate is defined as `CAN_BITRATE` and is initially 500000 bit/s. Serial baud is `SERIAL_BAUD`, initially 115200.

## Serial protocol

DLC is decimal. CAN IDs and payload bytes are hexadecimal.

Transmit:

```text
TX <hex-id> <decimal-dlc> <hex-byte0> <hex-byte1> ...
```

Example:

```text
TX 18A04010 3 01 02 FF
```

The ID must be in the full 29-bit range `0x00000000` through `0x1FFFFFFF`. Every transmitted frame is marked as an extended CAN frame. The payload count must exactly match DLC, which must be `0` through `8`.

Startup status:

```text
STATUS CAN=OK MODE=LOOPBACK BITRATE=500000
```

Successful transmission:

```text
OK
```

Errors:

```text
ERR <short_reason>
```

Received frames are printed as:

```text
RX <hex-id> <decimal-dlc> <hex-byte0> <hex-byte1> ...
```

Payload bytes are always printed as two hexadecimal digits. Standard frames are also read without truncating their IDs; this utility transmits extended frames only.

## Standalone loopback test

1. Install the Arduino Uno board package and `autowp/arduino-mcp2515` v1.3.1 or newer.
2. Open the `.ino` file as an Arduino sketch.
3. Select Arduino Uno and upload with `CAN_LOOPBACK = true`.
4. Open the USB serial monitor at 115200 baud.
5. Send:

```text
TX 18A04010 3 01 02 FF
```

Expected output is approximately:

```text
OK
RX 18A04010 3 01 02 FF
```

Exact ordering can vary because serial handling and MCP2515 polling are independent in the loop.

For normal physical CAN, set `CAN_LOOPBACK = false`. The bus needs correct termination and another active CAN controller to acknowledge transmitted frames. This sketch does not add termination control or Raspberry Pi integration.
