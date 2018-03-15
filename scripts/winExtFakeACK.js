/*
* (OUT) IHost (window.external)
* - txBytes()
*/
define({
	name: "winExt",
	description: "no real hardware simply a fake/dummy, consume and ACK",

	txBytes: function (sHexBytes) {
		console.log("(winExtFakeACK.js)::txBytes()", sHexBytes);
		window.setTimeout(function () {
			OnRXSingleByte(6); // (BTHostGlobalCBs.js)::OnRXSingleByte(ACK)
			// OnRXSingleByte(15); // (BTHostGlobalCBs.js)::OnRXSingleByte(NAK)
		}, 500); // MUSS natuerlich kleiner g_tWaitForAckNakTimeout sein
	}
});
