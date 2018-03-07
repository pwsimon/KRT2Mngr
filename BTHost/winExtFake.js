/*
* static/global object, wir emulieren einen namespace
* vgl. Philip Ackermann, S. 162, Namespace-Entwurfsmuster in Object-Literal-Schreibweise
*
* (OUT) IHost (window.external)
* - txBytes()
*/
var WinExt = WinExt || (function () {
	// privates

	// public API
	return {
		/*
		* sHexBytes enthaelt space delimitted asccii hex codierte bytes
		* z.B. 0x06 0x45 ...
		*/
		txBytes: function (sHexBytes) {
			console.log("txBytes()", sHexBytes);
			window.setTimeout(function () {
				OnRXSingleByte(6); // IByteLevel::OnRXSingleByte(ACK)
				// OnRXSingleByte(15); // IByteLevel::OnRXSingleByte(NAK)
			}, 500); // MUSS natuerlich kleiner g_tWaitForAckNakTimeout sein
		}
	};
})();
