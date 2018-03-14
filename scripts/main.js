// Place third party dependencies in the lib folder
//
// Configure loading modules from the lib directory,
// except 'app' ones, 
requirejs.config({
	"baseUrl": "scripts",
	"paths": {
		"jquery": "//code.jquery.com/jquery-1.12.4"
	}
});

requirejs(['jquery', 'KRT4ByteLevel'], function ($, mKRT) {
	var g_bDUALMode = false;

	$('#toggleDUALMode').on('click', function () {
		var sDualModeOn = "0x02 0x4f", // Interface_Ext_V18.pdf(5), 1.3.7 DUAL-mode on
			sDualModeOff = "0x02 0x6f", // Interface_Ext_V18.pdf(5), 1.3.8 DUAL-mode off
			lAccept;

		g_bDUALMode = !g_bDUALMode;
		lAccept = mKRT.fireAndForget(g_bDUALMode ? sDualModeOn : sDualModeOff);
		if (lAccept)
			document.getElementById('command').innerText = 'command rejected, buffer full/busy';
	});

	$('#uploadToKRT2').on('click', function () {
		mKRT.uploadToKRT2();
	});
})
