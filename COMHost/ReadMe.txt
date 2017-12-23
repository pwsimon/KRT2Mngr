wir brauchen eine NATIVE windows anwendung fuer den zugriff auf geraete schnittstellen
dieser XXHost.exe ist eine spezialisierte variante der
Microsoft HTML-Applikation, https://de.wikipedia.org/wiki/HTML-Applikation
variante 1: (COMHost.exe) via COM Port,
            dazu muss entweder eine physikalische Serielle vorhanden sein od. ein BT geraet als virtueller COM Port konfiguriert werden.
variante 2: (BTHost.exe) echter zugriff ueber die BT API

- mit FEATURE_BROWSER_EMULATION damit unser HTML GUI die noetigen HTML5/CSS features unterstuetzt
- mit m_strCurrentUrl navigieren wir zur initialen page via. http:// anstatt res://
