"use strict";

function quote(text)
{
	/* insert quoted post into textarea */
	var el = document.getElementById("pBox");
	var before = el.value.slice(0, el.selectionStart);
	var after = el.value.slice(el.selectionStart);
	var insert = ">>" + text + "\n";
	var caretPos = before.length + insert.length;
	el.value = before + insert + after;
	el.setSelectionRange(caretPos, caretPos);
	el.focus();
}
