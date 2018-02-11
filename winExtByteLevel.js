﻿/*
* dieses module nutzt/braucht fogende abhaengigkeiten:
* - global, IByteLevel.js():g_cmdParserAckNakHandler
* - PersistModul, require('persist')
* - document/DOM, GUI-Elemente: 'command'
* Hinweis(e):
*   da wir ausschliesslich ueber eine txBytes() methode ohne callback verfuegen ABER
*   dennoch das Device NICHT mit requests ueberrennen duerfen
*   (wir muessen nach jedem 'Z' command auf ein Ack/Nak warten)
*   muessen wir hier einen anderweitigen synchronisationsmechanismuss/abhaengigkeit verbauen
*/
var WinExtByteLevelModul = WinExtByteLevelModul || (function () {
	return {
		uploadToKRT2: function () {
			var nIndex = 0,
				rgCommands = PersistModul.rgStations.map(function (station, nStationIndex) {
					var sFrequence = station.nFrequence,
						nMHZ = Math.floor(station.nFrequence / 1000),
						nKHZ = (station.nFrequence - nMHZ * 1000) / 5,
						nChkSum = nMHZ ^ nKHZ,

						/*
						* 1.3.5 Set a single record & name to database with address
						*/
						rgCommand = [2, "Z".charCodeAt(0), nMHZ, nKHZ],
						nIndex = 0,
						sCommand = "";

					for (nIndex; nIndex < station.sName.length; nIndex++)
						rgCommand.push(station.sName.charCodeAt(nIndex));

					/*
					* ensure TextLength is 8 Byte
					* String.prototype.padEnd(), https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/padEnd
					* sorry not for Internet Explorer
					*/
					for (nIndex = station.sName.length; nIndex <= 7 ; nIndex++)
						rgCommand.push(" ".charCodeAt(0));

					rgCommand.push(nStationIndex);
					rgCommand.push(nChkSum);

					// einen string fuer die anzeige, hex codiert, space delemitted
					rgCommand.forEach(function (digit) {
						sCommand += "0x";
						sCommand += 15 < digit ? digit.toString(16) : "0" + digit.toString(16); // ensure 2 Digits
						sCommand += " ";
					});
					return sCommand;
				});

			console.log("rgCommands.length: ", rgCommands.length);
			// wir duerfen erst mit einem callback weitersenden
			g_cmdParserAckNakHandler = function (nByte) {
				if (6 == nByte && nIndex < rgCommands.length) {
					console.log("continue");
					window.external.txBytes(rgCommands[nIndex++]);
				} else {
					console.log("abort/finish");
					g_cmdParserAckNakHandler = null;
				}
			}
			window.external.txBytes(rgCommands[0]); // start transmit
		}
	};
})();
