# dieser BTHost.exe:
- ist eine spezialisierte variante der [Microsoft HTML-Applikation](https://de.wikipedia.org/wiki/HTML-Applikation)
- addresses any BT *classic* device which supports the (S)erial(P)ort(Profile)
  we are using the [Windows Sockets 2](https://msdn.microsoft.com/de-de/library/windows/desktop/ms740673(v=vs.85).aspx) API to communicate with the device.
- supporting Bluetooth V4.0 Low Energy [GATT Overview](https://www.bluetooth.com/specifications/gatt/generic-attributes-overview) is focused by UWP [Bluetooth GATT Client](https://docs.microsoft.com/en-us/windows/uwp/devices-sensors/gatt-client) or Browser [Interact with Bluetooth devices on the Web](https://developers.google.com/web/updates/2015/07/interact-with-ble-devices-on-the-web) Application

# goal/targets: _(JavaScript-CommandParser)_
- dieser BTHost stellt im gegensatz zum COMHost KEINEN CommandParser zur verfuegung
  deswegen ist die window.external schnittstelle vollkommen unterschiedlich
  in empfangsrichtung wird JEDES byte sofort (und ungefiltert) an function OnRXSingleByte(nByte) dispatched
  weiterhin verzichten wir auf ein aufwaendiges subcribe bzw. einen callback
  in senderichtung uebergeben wir ein byteArray UND verzichten ebenfalls auf einen callback!
  die empfaenger function ist also in der verantwortung das FAILED/SUCCEEDED fuer ein vohergehendes send zu extrahieren/bauen
- 2/3 layered architecture, 1.) physical device, 2.) basic protocol, 3.) HTML/GUI
  layer 2 (basic protocol) and 3 (HTML/GUI) are implemented in JavaScript.
- zur simulation eines Bluetooth device mit SPP verwenden wir einen einfachen TCP/IP echoserver
  [Vollständiger Winsock-Servercode](https://msdn.microsoft.com/de-de/library/windows/desktop/ms737593(v=vs.85).aspx)

# Modul(Host)
## physical device
handles uninterpreted/raw bytes
- window.external::txBytes(VTS_BSTR)

# Modul(HTML/JavaScript)
## basic protocol (window.external/ISerial)
- CommandInterpreter handles checksum generation on outgoing commands and checksum verification on incomming commands
- handles KeepAlive/Ping outgoing and incomming
- ISerial::OnRXSingleByte(nByte)

## HTML/GUI
handles functional, JSON-Encoded, commands with have no information about checksum or Ack/Nak
