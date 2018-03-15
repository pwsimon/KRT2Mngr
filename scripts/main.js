// Place third party dependencies "jquery", ... in from CDN
//
// Configure loading modules from the "scripts" directory,
// except 'app' ones, 
requirejs.config({
	"baseUrl": "scripts",
	"paths": {
		"jquery": "//code.jquery.com/jquery-1.12.4",
		"jquery-ui": "//code.jquery.com/ui/1.12.1/jquery-ui"
	}
});

requirejs(['jquery', 'jquery-ui', 'KRT4ByteLevel', 'sevenSeg.js'], function ($, ui, mKRT) {
	var g_bDUALMode = false;

	$('#toggleDUALMode').on('click', function () {
		var sDualModeOn = "0x02 0x4f", // Interface_Ext_V18.pdf(5), 1.3.7 DUAL-mode on
			sDualModeOff = "0x02 0x6f", // Interface_Ext_V18.pdf(5), 1.3.8 DUAL-mode off
			lAccept;

		g_bDUALMode = !g_bDUALMode;
		lAccept = mKRT.fireAndForget(g_bDUALMode ? sDualModeOn : sDualModeOff);
		if (lAccept)
			$('#lblCommand').text('command rejected, buffer full/busy');
	});

	$('#uploadToKRT2').on('click', function () {
		mKRT.uploadToKRT2();
	});

	ko.applyBindings({ station: ko.observable(123.500) });
})
