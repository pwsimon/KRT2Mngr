wir brauchen eine NATIVE windows anwendung fuer den zugriff auf geraete schnittstellen
dieser XXHost.exe ist eine spezialisierte variante der
Microsoft HTML-Applikation, https://de.wikipedia.org/wiki/HTML-Applikation
variante 1: (COMHost.exe) via COM Port,
            dazu muss entweder eine physikalische Serielle vorhanden sein od. ein BT geraet als virtueller COM Port konfiguriert werden.
variante 2: (BTHost.exe) echter zugriff ueber die BT API

History: (der letzte/neueste eintrag steht oben)
- open socket and write data to HardCoded BT Address: 98:D3:31:FD:5A:F2
- SoftButtons im OnDocumentComplete() beschriften
- ::WSAStartup/WSACleanup eingefuehrt, enumDevice (using bluetoothapis.h)
- enum Devices/Services
- enum Radio BluetoothFindFirstRadio
