'use strict';


function quote(text)
{
	/* click to quote posts */
	var elem = document.getElementById("pBox");
	elem.value += ">>" + text + "\n";
}
