wir brauchen eine NATIVE windows anwendung fuer den zugriff auf geraete schnittstellen.

# dieser BTHost.exe:
- ist eine spezialisierte variante der [Microsoft HTML-Applikation](https://de.wikipedia.org/wiki/HTML-Applikation)
- addresses any BT device which supports the (S)erial(P)ort(Profile)
  we are using the [Windows Sockets 2](https://msdn.microsoft.com/de-de/library/windows/desktop/ms740673(v=vs.85).aspx) API to communicate with the device.

# Branches:
- JavaScript-CommandParser _// (FeatureBranch) hier werden wir den minimalistischen C/C++ ansatz weiterverfolgen_
- HighLevel-sendCommand _// (FeatureBranch) wir verfolgen eine 3 schichtige architektur_
- master _// (Integrations Branch)_

# History: _(JavaScript-CommandParser)_
- dieser BTHost stellt im gegensatz zum COMHost KEINEN CommandParser zur verfuegung
  deswegen ist die window.external schnittstelle vollkommen unterschiedlich
  in empfangsrichtung wird JEDES byte sofort (und ungefiltert) an function receiveByte(nByte) dispatched
  weiterhin verzichten wir auf ein aufwaendiges subcribe bzw. einen callback
  in senderichtung uebergeben wir ein byteArray UND verzichten ebenfalls auf einen callback!
  die empfaenger funktion ist also in der verantwortung das FAILED/SUCCEEDED fuer ein vohergehendes send zu extrahieren/bauen