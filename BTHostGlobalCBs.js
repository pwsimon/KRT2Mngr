/*
* ALLE Methoden der IByteLevel schnittstelle muessen (TopLevel) verfuegbar sein
* diese vereinbarung stellt sicher das die aufrufe aus dem Host, einfach, ausgefuehrt werden koennen
*/
function OnRXSingleByte(nByte) {
	requirejs(['parser'], function (mParser) {
		mParser.topLevelDrive(nByte);
	})
}
