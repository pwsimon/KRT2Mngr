wir brauchen eine NATIVE windows anwendung fuer den zugriff auf geraete schnittstellen
dieser XXHost.exe ist eine spezialisierte variante der
Microsoft HTML-Applikation, https://de.wikipedia.org/wiki/HTML-Applikation
variante 1: (COMHost.exe) via COM Port,
            dazu muss entweder eine physikalische Serielle vorhanden sein od. ein BT geraet als virtueller COM Port konfiguriert werden.
variante 2: (BTHost.exe) echter zugriff ueber die BT API

Branches:
  JavaScript-CommandParser                        // hier werden wir den minimalistischen C/C++ ansatz weiterverfolgen
  HighLevel-sendCommand                           // wir verfolgen eine 3 schichtige architektur
master (Integrations Branch)
  trial-ResetEvent-ClearOverlapped                // 2 Schicht architektur

History: (JavaScript-CommandParser)
- dieser BTHost stellt im gegensatz zum COMHost KEINEN CommandParser zur verfuegung
  deswegen ist die window.external schnittstelle vollkommen unterschiedlich
  in empfangsrichtung wird JEDES byte sofort (und ungefiltert) an function receiveByte(nByte) dispatched
  weiterhin verzichten wir auf ein aufwaendiges subcribe bzw. einen callback
  in senderichtung uebergeben wir ein byteArray UND verzichten ebenfalls auf einen callback!
  die empfaenger funktion ist also in der verantwortung das FAILED/SUCCEEDED fuer ein vohergehendes send zu extrahieren/bauen