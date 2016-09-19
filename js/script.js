"use strict";

/*
 * script.js
 * client-side functionality
 */

function strcmp(s1, s2)
{
	/* returns 0 if strings are identical */
	return (s1 == s2) ? 0 : (s1 > s2) ? 1 : -1;
}

function strstr(haystack, needle)
{
	/* returns first occurance of needle in haystack */
	var len_h = haystack.length;
	var len_n = needle.length;
	var i, j;
	for (i = 0; i < len_h; i++)
	{
		var matches = 0;
		for (j = 0; j < len_n; j++)
		{
			if (i + len_n <= len_h) /* bounds check */
			{
				if (haystack.charAt(i + j) == needle.charAt(j))
					matches++;
			}
			else
				break;
		}
		if (matches == len_n)
			return (i < 1) ? -1 : i; /* return non-null location */
	}
	return null;
}

function is_visible(el)
{
	/* check if element is completely visible */
	var rect = el.getBoundingClientRect();
	var width = document.documentElement.clientWidth;
	var height = document.documentElement.clientHeight;
	return (rect.top > 0 && rect.left > 0 &&
	        rect.right < width && rect.bottom < height);
}

function quote(num)
{
	/* insert quoted post no into textarea */
	var el = document.getElementById("pBox");
	var before = el.value.slice(0, el.selectionStart);
	var after = el.value.slice(el.selectionStart);
	var insert = ">>" + num + "\n";
	var caretPos = before.length + insert.length;
	el.value = before + insert + after;
	el.setSelectionRange(caretPos, caretPos);
	el.focus();
}

function highlight(request)
{
	/* reset previous highlights */
	var posts = document.getElementsByClassName("pContainer");
	for (var i = 0; i < posts.length - 1; i++)
		if (strstr(posts[i].getAttribute("class"), "highlight"))
			posts[i].setAttribute("class", "pContainer");
	if (request)
	{
		var el = document.getElementById(request); /* set new */
		el.setAttribute("class", el.getAttribute("class") + " highlight");
	}
}

function popup(self, request, hover)
{
	/* displays quoted post on hover */
	var orig = document.getElementById(request);
	var visible = (!orig) ? 0 : is_visible(orig);
	var parent = document.getElementById(self);
	var linkquote = parent.getElementsByClassName("linkquote");
	for (var i in linkquote)
	{
		var current = linkquote[i];
		var href = current.getAttribute("href");
		if (!strcmp(href, "#" + request))
		{
			if (!orig) /* post doesn't exist */
			{
				current.removeAttribute("onmouseover");
				current.removeAttribute("onmouseout");
				current.setAttribute("style", "text-decoration:line-through;");
			}
			else if (hover)
			{
				if (!visible) /* display in viewport */
				{
					var style = " popup";
					var copy = orig.cloneNode(1); /* prevent id collision */
					copy.setAttribute("id", copy.getAttribute("id") + "prev");
					copy.setAttribute("class", copy.getAttribute("class") + style);
					current.parentNode.insertBefore(copy, current.nextSibling);
				}
				else
					highlight(request);
			}
			else
			{
				highlight(0); /* unset highlights */
				if (!visible) /* remove from viewport */
				{
					var popup = document.getElementById(request + "prev");
					if (popup)
						popup.parentNode.removeChild(popup);
				}
			}
			break;
		}
	}
}
