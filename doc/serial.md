# 📘 Serial Interface Protocol Manual

**Device Name:** Pinefeat lens controller _cef168_  
**Interface Type:** UART (Serial Communication)  
**Protocol Type:** ASCII-based Serial Protocol

## 1. Overview

This manual defines the serial communication protocol used to interact with the device over a UART interface using ASCII commands. Communication is synchronous and text-based.

## 2. Electrical Interface

- **Voltage Levels:** 3.3V TTL logic
- **Connector Type:** Molex PicoBlade, 1.25mm pitch
- **Pinout:**
  - **Pin 1:** GND
  - **Pin 2:** RXD (device input)
  - **Pin 3:** TXD (device output)
  - **Pin 4:** VCC

## 3. Communication Settings

| Parameter     | Value              |
|---------------|--------------------|
| Baud Rate     | 115200             |
| Data Bits     | 8                  |
| Parity        | None               |
| Stop Bits     | 1                  |
| Flow Control  | None               |
| Encoding      | ASCII              |
| Line Ending   | `\r`, `\n` or both |

## 4. Protocol Format

### 4.1 Command Format

Each command is a plain ASCII string, optionally followed by parameters. Commands are case-insensitive.

```
<Command>[<Parameters>]<CR>
```

- `<Command>` — Single character
- `<Parameters>` — Optional integer or decimal numbers, represented as ASCII strings
- `<CR>` — Carriage return (`\r`, `\n` or both)

### 4.2. Response Format

- All responses are ASCII text terminated by a carriage return (`\r\n`)
- Values are typically numeric or short status messages (`ok`, `er`, `nc`)

### 4.3. Summary of Commands

| Command               | Example Input | Description                                                                                                                                                                                                                                   | Response Example                                                                                                          |
|-----------------------|---------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------|
| `h`                   | `h`           | Print lens info.                                                                                                                                                                                                                              | `Lens ID: 0xEB`<br>`Focal length: 10-22 mm`<br>`Zoom: 1x`<br>`Aperture: f/3.5(3.5)-22.6`<br>`Focus distance: 0.23-0.23 m` |
| `c`                   | `c`           | Run the calibration process to traverse the full focus range, from minimum to infinity, and determine the total number of focus steps.<br>The procedure must be executed **at least once** for each lens. The value then is stored in EEPROM. | `ok`                                                                                                                      |
| `l`                   | `l`           | Read focal length in millimeters.                                                                                                                                                                                                             | `12`                                                                                                                      |
| `d`                   | `d`           | Read focus distance range in meters.                                                                                                                                                                                                          | `2.24-6.48`                                                                                                               |
| `f` or `p`            | `f`           | Read absolute focus value.<br>Requires calibration procedure described above.                                                                                                                                                                 | `405`                                                                                                                     |
| `f<val>`<br>or<br>`m<val>` | `f500`        | Set absolute focus value to 500.                                                                                                                                                                                                              | `ok`                                                                                                                      |
| `f+<val>`<br>or<br>`m+<val>` | `f+100`       | Move focus towards infinity by 100.                                                                                                                                                                                                           | `ok`                                                                                                                      |
| `f-<val>`<br>or<br>`m-<val>` | `f-200`       | Move focus closer to the camera by 200.                                                                                                                                                                                                       | `ok`                                                                                                                      |
| `a`                   | `a`           | Get aperture range in f-stops.                                                                                                                                                                                                                | `3.5-22.6`                                                                                                                |
| `a<val>`              | `a5.6`        | Set the camera's aperture to an f-stop value.                                                                                                                                                                                                 | `ok`                                                                                                                      |
| `a+<val>`             | `a+4.0`       | Open the iris by a certain number of f-stops further.                                                                                                                                                                                         | `ok`                                                                                                                      |
| `a-<val>`             | `a-1.0`       | Close the iris by a certain number of f-stops further.                                                                                                                                                                                        | `ok`                                                                                                                      |
| `i`                   | `i`           | Move the focus towards infinity.                                                                                                                                                                                                              | `ok`                                                                                                                      |
| `m`                   | `m`           | Move the focus to the minimum.                                                                                                                                                                                                                | `ok`                                                                                                                      |
| `s<val>`              | `s2`          | Set the focusing speed from 1 to 4 if supported.                                                                                                                                                                                              | `ok`                                                                                                                      |
| `e`                   | `e`           | Returns `y`, when the lens motor is activated, otherwise returns `n`.                                                                                                                                                                         | `n`                                                                                                                       |
| `t`                   | `t`           | Get the time, in milliseconds, spent on focusing or setting the aperture.                                                                                                                                                                     | `95`                                                                                                                      |
| `n`                   | `n`           | Read the control board serial number.                                                                                                                                                                                                         | `PINEFEAT CEF1680000011`                                                                                                              |
| `v`                   | `v`           | Read the control board firmware version.                                                                                                                                                                                                                    | `1.0`                                                                                                                     |



### 4.4. Error Handling

- **Unknown Command:**  
  Response: `er`

- **Invalid Parameters:**  
  Response: `er`

- **Lens not connected:**  
  Response: `nc`

## 5. Notes

- No checksums or framing bytes are required due to the simplicity of the protocol.
- When using Raspberry Pi, the Serial Interface must be enabled in the Raspberry Pi configuration tool.
- If using a terminal emulator (like PuTTY or Minicom), it is recommended to enable local echo to see the characters you type.
