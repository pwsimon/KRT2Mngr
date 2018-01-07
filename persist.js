var PersistModul = PersistModul || (function () {
	return {
		rgStations: [
			{ nFrequence: 123500, sName: "Geratshf" },
			{ nFrequence: 123995, sName: "BadWoer" },
			{ nFrequence: 120100, sName: "KemptenD" },
			{ nFrequence: 126025, sName: "Agathazl" },
			{ nFrequence: 118500, sName: "Memmingn" },
			{ nFrequence: 122500, sName: "Paterzel" },
			{ nFrequence: 126900, sName: "PETER" }
		],

		readSingleFile: function (e) {
			var file = e.target.files[0];
			if (!file) return;

			var reader = new FileReader();
			reader.onload = function (e) {
				var contents = e.target.result;
				// displayContents(contents);
				console.log(e.target.result);
				var rgStations = str.split("\n\a");
			};
			reader.readAsText(file);
		}
	};
})();
