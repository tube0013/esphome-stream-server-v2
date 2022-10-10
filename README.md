# esphome-stream-server-v2
Custom Component for ESPHome enabling Serial over TCP

This project is forked from the original work at 
https://github.com/oxan/esphome-stream-server and has been updated to work reliably on ESPHome versions after 2021.10

Stream server for ESPHome
=========================

Custom component for ESPHome to expose a UART stream over Ethernet. Can be used as a serial-to-ethernet bridge as
known from ESPLink or ser2net by using ESPHome.

This component creates a TCP server listening on port 6638 (by default), and relays all data between the connected
clients and the serial port. It doesn't support any control sequences, telnet options or RFC 2217, just raw data.

Usage
-----

Requires ESPHome v2021.10 or higher.

```yaml
external_components:
  - source: github://tube0013/esphome-stream-server-v2

stream_server:
```

You can set the UART ID and port to be used under the `stream_server` component.

```yaml
uart:
   id: uart_bus
   # add further configuration for the UART here

stream_server:
   uart_id: uart_bus
   port: 6638
```

You can optionally create a `binary_sensor` to indicate whether a client is currently connected.

```yaml
stream_server:
  id: ss

binary_sensor:
  - platform: stream_server
    stream_server: ss
```

Many thank @oxan for the original project.

Credit for completing the socket-abstraction - @vallejoyuridamian

Binary sensor credit to @joshuaspence
