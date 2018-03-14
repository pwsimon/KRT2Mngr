console.log("Enter(krt4bytelevel.js)::globalCode()");

define(["./winExtFake"], function (winExt) {
	// Do setup work here

	// return an object to define the module.
	return {
		name: "KRT",
		description: "IKRT to winExtFake",

		OnRXSingleByte: function (nByte) {
			console.log("parser::drive()", nByte);
		},

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
			winExt.txBytes(rgCommands[0]); // start transmit
		},
		fireAndForget: function (sCommand) {
			winExt.txBytes(sCommand); // start transmit
		},
		sendCommand: function (sCommand, fnCB) {
			/*
			* delegate/redirect callback
			* koennen wir "leider" nicht direct verwenden weil der CB auf KRT ebene eine abweichende syntax hat
			* fnCB ist function(error, result)
			*/
			g_cmdParserAckNakHandler = function (nByte) {
				if (6 == nByte) {
					console.log("continue");
					fnCB(null, nByte);
				} else {
					console.log("abort/finish");
					fnCB("abort/finish", nByte);
				}
			}
			winExt.txBytes(sCommand); // start transmit
		}
	}
});

console.log("Leave(krt4bytelevel.js)::globalCode()");
