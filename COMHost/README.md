# dieser COMHost.exe:
- is a specalized version of [Microsoft HTML-Applikation](https://de.wikipedia.org/wiki/HTML-Applikation)
- addresses a single COM Port with it's associated device
  therefore you must have a logical serial interface. either a real physical serial port or a virtual (Bluetooth) Device.
  we are using the [File Management Functions](https://msdn.microsoft.com/en-us/library/windows/desktop/aa364232(v=vs.85).aspx) API to communicate with the device.

# goal/targets: _(master)_
- the Host ist responsible to bring the functionality to discover and connect a logical COM port/device.
- 3 layered architecture, 1.) physical device, 2.) basic protocol, 3.) HTML/GUI
  layer 1 (physical device) and 2 (basic protocol) are implemented in C/C++.
- zur simulation eines COM device lesen bzw. schreiben wir eine Datei (krt2input.bin/krt2output.bin)

# Modul(Host)
## physical device
handles uninterpreted/raw bytes

## basic protocol
- CommandInterpreter handles checksum generation on outgoing commands and checksum verification on incomming commands
- handles KeepAlive/Ping outgoing and incomming

# Modul(HTML/JavaScript)
## HTML/GUI
handles functional, JSON-Encoded, commands with have no information about checksum or Ack/Nak.
the GUI is always the active part and responsible to request for incomming commands.
- window.external.receiveCommand()
- window.external.sendCommand()
