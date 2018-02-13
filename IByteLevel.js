/*
* ALLE Methoden der IByteLevel schnittstelle muessen (TopLevel) verfuegbar sein
* diese vereinbarung stellt sicher das die aufrufen aus dem Host, einfach, ausgefuehrt werden koennen
*
* mit der weiterentwicklung beziehen wir den (aktuell globalen g_cmdParser) durch ein require()
*/
function OnRXSingleByte(nByte) {
	/*
	* document.getElementById('lblCommand').innerText = String.fromCharCode(nByte);
	* solange ein parser (g_cmdParser) active ist muss dieser die hoechste prioritaet haben
	*/
	if (g_cmdParser) {
		if (g_cmdParser.Drive(nByte)) {
			// console.log("finished:", g_cmdParser.rgCmd.sCmd);
			document.getElementById('lblCommand').innerText = "finished: " + g_cmdParser.rgCmd.sCmd;
			g_cmdParser = null; // enter IDLE
		}
	}

	else if ("S".charCodeAt(0) == nByte) {
		console.log("ping");
		document.getElementById('lblCommand').innerText = "ping";
	}

	else if (2 == nByte) { // STX
		if (g_cmdParser)
			console.log("previous command unsuccessfull/interrupted");

		g_cmdParser = new CommandParser();
	}

	else if (6 == nByte && g_cmdParserAckNakHandler) { // ACK
		console.log("delegate to initiator of txBytes()");
		g_cmdParserAckNakHandler(nByte);
	}

	else if (15 == nByte && g_cmdParserAckNakHandler) { // NAK
		console.log("delegate to initiator of txBytes()");
		g_cmdParserAckNakHandler(nByte);
	}

	else {
		console.log("invalid sequence");
	}
}

/*
* der CommandParser sollte konsequent in ein eigenes Module ausgelagert werden
*
* umstellen von Pseudoklassischer Vererbung (Philip Ackermann S. 145)
* auf Prototypische Vererbung (Philip Ackermann S. 136)
*/
var g_cmdParser;
var g_cmdParserAckNakHandler;
function CommandParser() {
	this.rgCmd = {};
	this.fnVerify; // undefined OR null laeuft hier auf das gleiche hinaus, KEINE verification routine
	this.rgKeys = [];
	this.nIndex = 0;

	// functions
	this.Drive = function (nByte) {
		var sCmd = String.fromCharCode(nByte);

		if (this.nIndex) {
			this.rgCmd[this.rgKeys[this.nIndex]] = nByte;
			this.nIndex++;
			if (this.nIndex == this.rgKeys.length) {
				window.clearTimeout(this.tTimeout);
				return this.fnVerify ? this.fnVerify() : true;
			}
			return false;
		}

			// single byte commands
		else if ("O" == sCmd) {
			this.rgCmd = { sCmd: sCmd };
			this.fnVerify = null; // single byte commands haben generell KEINE verification function
			/*
			* wird mit dem constructor (STX) gestartet
			* fuer MultiByte commands wird der Timeout, zentral, mit der bedingung (this.nIndex == this.rgKeys.length) zurueckgesetzt
			*/
			window.clearTimeout(this.tTimeout);
			return true;
		}

		else if ("o" == sCmd) {
			this.rgCmd = { sCmd: sCmd };
			this.fnVerify = null; // single byte commands haben generell KEINE verification function
			window.clearTimeout(this.tTimeout); // wird mit dem constructor (STX) gestartet
			return true;
		}

			// MultiByte commands
		else if ("R" == sCmd) {
			this.rgCmd = { sCmd: sCmd, nMHZ: 0, nkHZ: 0, sStation0: ' ', sStation1: ' ', sStation2: ' ', sStation3: ' ', sStation4: ' ', sStation5: ' ', sStation6: ' ', sStation7: ' ', nNr: 0, nChkSum: 0 };
			/*
			* die this.fnVerify() funktion wird ausgefuehrt wenn das letzte Byte eines commands gelesen wurde
			* fnVerifyChkSum() liefert eine funktion die folgenden ausdruck verifiziert (eval): 'nMHZ' ^ 'nkHZ' == 'nChkSum'
			*/
			this.fnVerify = this.fnVerifyChkSum('nMHZ', 'nkHZ', 'nChkSum');
		}

		else if ("U" == String.fromCharCode(nByte)) {
			this.rgCmd = { sCmd: sCmd, nMHZ: 0, nkHZ: 0, sStation0: ' ', sStation1: ' ', sStation2: ' ', sStation3: ' ', sStation4: ' ', sStation5: ' ', sStation6: ' ', sStation7: ' ', nChkSum: 0 };
			this.fnVerify = null; // KEIN(E) verify (funktion) nachdem alle bytes des command gelesen wurden
		}

		this.rgKeys = Object.keys(this.rgCmd);
		this.nIndex = 1;
	}
	this.fnVerifyChkSum = function (sNamedId1, sNamedId2, sNamedIdResult) {
		return function () { return this.rgCmd[sNamedIdResult] == this.rgCmd[sNamedId1] ^ this.rgCmd[sNamedId2] };
	}
	this.tTimeout = window.setTimeout(function () {
		console.log("reset the global parser");
		document.getElementById('lblCommand').innerText = "reset the global parser";
		g_cmdParser = null; // enter IDLE
	}, 5000); // 9600 baud => 100 Char/Sec => 50 Char/0.5 Sec
}
