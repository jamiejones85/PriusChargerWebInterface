/*
 * This file is part of the esp8266 web interface
 *
 * Copyright (C) 2018 Johannes Huebner <dev@johanneshuebner.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/** @brief excutes when page finished loading. Creates tables and chart */
var output, websocket;
function onLoad() {
	output = document.getElementById("output");
	chargerWebSocket("ws://"+ location.host +":81");

	const showLog = document.getElementById("showLog");
	showLog.addEventListener('change', (event) => {
		doSend("{showLog: " + event.target.checked + "}")
	})
}

function onOpen(evt) {
	console.log("Socket Connected")
}
   
 function onMessage(evt) {
	const json = JSON.parse(evt.data);

	if (json.message) {
		writeToScreen('<span style = "color: blue;">' +
		json.message+'</span>');
	}

 }

 function onError(evt) {
	console.log("Socket Error")
 }
   
 function doSend(message) {
	websocket.send(message);
 }
   
 function writeToScreen(message) {
	var pre = document.createElement("p"); 
	pre.style.wordWrap = "break-word"; 
	pre.innerHTML = message; output.appendChild(pre);
 }

function chargerWebSocket(wsUri) {
	websocket = new WebSocket(wsUri);
	   
	websocket.onopen = function(evt) {
	   onOpen(evt)
	};

	websocket.onclose = function(evt) {
		console.log(evt)
	};
   
	websocket.onmessage = function(evt) {
	   onMessage(evt)
	};
   
	websocket.onerror = function(evt) {
	   onError(evt)
	};
 }
