﻿var PersistModul = PersistModul || (function () {
	return {
		rgStations: [
			{ sName: "Geratshf", nFrequence: 123500 }
		],

		readSingleFile: function (e) {
			var file = e.target.files[0];
			if (!file) return;

			var reader = new FileReader();
			reader.onload = function (e) {
				/* var sContents = e.target.result;
				displayContents(sContents);
				console.log(e.target.result); */
				var rgStations = e.target.result.split("\n");
				PersistModul.rgStations = rgStations.map(function (currentValue, index, arr) {
					var rgValues = currentValue.split(";");
					return {
						sName: rgValues[0],
						nFrequence: Math.round(parseFloat(rgValues[1]) * 1000) // ein parseFloat(128.950) * 1000 fuehrt seltsamerweise zu 128949.9999
					};
				});
				console.log("read:", rgStations.length, " stations (rgStations.length)");
			};
			reader.readAsText(file);
		}
	};
})();
