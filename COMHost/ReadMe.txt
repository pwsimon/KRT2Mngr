wir brauchen eine NATIVE windows anwendung fuer den zugriff auf geraete schnittstellen
dieser XXHost.exe ist eine spezialisierte variante der
Microsoft HTML-Applikation, https://de.wikipedia.org/wiki/HTML-Applikation
variante 1: (COMHost.exe) via COM Port,
            dazu muss entweder eine physikalische Serielle vorhanden sein od. ein BT geraet als virtueller COM Port konfiguriert werden.
variante 2: (BTHost.exe) echter zugriff ueber die BT API

Branches:
  BTCom-OpenTCP-IOCompletion                      // hier werden wir den minimalistischen C/C++ ansatz weiterverfolgen
  HighLevel-sendCommand                           // wir verfolgen eine 3 schichtige architektur, 
master (Integrations Branch)
  trial-ResetEvent-ClearOverlapped                // 2 Schicht architektur

History/Ziele: (HighLevel-sendCommand)
- das ist guter ansatz wiederspricht aber der idee hier ein komplexes JavaScript Projekt zu ueben

History/Ziele: (trial-ResetEvent-ClearOverlapped)
- in sende richtung werden an der schnittstelle ByteStreams uebergeben
  weil der ByteStream untypisiert ist muss es fireAndForget UND sendCommand geben
  in empfangs richtung werden (abstrakte) JSONCommands an die UI geliefert