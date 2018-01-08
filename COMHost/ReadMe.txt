wir brauchen eine NATIVE windows anwendung fuer den zugriff auf geraete schnittstellen
dieser XXHost.exe ist eine spezialisierte variante der
Microsoft HTML-Applikation, https://de.wikipedia.org/wiki/HTML-Applikation
variante 1: (COMHost.exe) via COM Port,
            dazu muss entweder eine physikalische Serielle vorhanden sein od. ein BT geraet als virtueller COM Port konfiguriert werden.
variante 2: (BTHost.exe) echter zugriff ueber die BT API

History: (der letzte/neueste eintrag steht oben)
- absturtz bei ShowLastError() behoben. MessageBox() bei Sharing violation
- comunicate received Bytes/Commands to MainThread and GUI
- extract values for command, rename first state to IDLE
- die logik/structure der ReadFile loop MUSS in jedem fall Synchrones UND Asynchrones verhalten behandeln koennen
- funktioniert ganz leidlich mit einem File base input ("krt2input.bin") offensichtlich funktioniert ein/dieser COM Port nicht mit OVERLAPPED
- ShowLastError fuer CreateFile/WriteFile und ClearCommError wobei ich befuerchte mit einem ECHTEN COM Port gibt es keine Probleme
- wir oeffnen den COM port nur EINMAL und speichern ihn als InstanceVariable so koennen wir MEHRERE commands abschicken
- File Read (OVERLAPPED)
- Command "R" fuer 1.3.3 Set passive frequency (MHz,KHz) & name, padding bytes fuer den Stationsnamen
- der URL fuer die GUI ist ueber den ersten CommandLine Parameter konfigurierbar (EINFACHES Deployment)
- ToDo: Commandos von COM Port lesen
- ToDo: COM Port configurierbar, "COM2"
- ToDo: Parameter fuer COM Port configurierbar, BaudRate
- den hexcodierten und space delemitteten command string in ein ByteArray convertieren und an COM1 schreiben
- _DEBUG Build, debuggen der MFC sourcen durch linken der MFC als static lib
- mit DDX_DHtml_ElementInnerText() bzw. GetElementText() holen wir uns das zu sendende command aus dem DOM
- mit m_strCurrentUrl navigieren wir zur initialen page via. http:// anstatt res://
- mit FEATURE_BROWSER_EMULATION damit unser HTML GUI die noetigen HTML5/CSS features unterstuetzt
