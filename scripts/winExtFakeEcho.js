/*
* (OUT) IHost (window.external)
* - txBytes()
*/
define({
	name: "winExt",
	description: "no real hardware simply a fake/dummy, Echo",

	txBytes: function (sHexBytes) {
		console.log("(winExtFakeACK.js)::txBytes()", sHexBytes);
		window.setTimeout(function () {
			sHexBytes.split(' ').forEach(function (hexByte) {
				var nByte = parseInt(hexByte, 16);
				OnRXSingleByte(nByte);
			});
		}, 500); // MUSS natuerlich kleiner g_tWaitForAckNakTimeout sein
	}
});
