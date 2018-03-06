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
			window.external.txBytes(sHexBytes);
		}
	};
})();
