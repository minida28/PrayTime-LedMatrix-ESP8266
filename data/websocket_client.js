var wsUri = "ws://echo.websocket.org/";
var output;
var attributes_log;
var websocket;
function init() {
    output = document.getElementById("output");
    attributes_log = document.getElementById("attributes_log");
    if (browserSupportsWebSockets() === false) {
        writeToScreen("Sorry! your web browser does not support WebSockets. Try using Google Chrome or Firefox Latest Versions");

        var element = document.getElementById("websocketelements");
        element.parentNode.removeChild(element);

        return; //
    }

    websocket = new WebSocket(wsUri);

    websocket.onopen = function() {
        writeAttributeValues('onOpen Event Fired');
        writeToScreen("CONNECTION TO " + wsUri + " has been successfully established");
    };

    websocket.onmessage = function(evt) {
        onMessage(evt);
    };

    websocket.onerror = function(evt) {
        onError(evt);
    };
}

function onClose(evt) {
    websocket.close();
    writeAttributeValues('onClose Event Fired');
    writeToScreen("DISCONNECTED");
}

function onMessage(evt) {
    writeToScreen('<span style="color: blue;">RESPONSE: ' + evt.data + '</span>');
    writeAttributeValues('onMessage Event Fired');
}

function onError(evt) {
    writeToScreen('<span style="color: red;">ERROR:</span> ' + evt.data);
    writeAttributeValues('onError Event Fired');
}

function doSend(message) {
    websocket.send(message);
    writeAttributeValues('onSend Event Fired');
    writeToScreen("SENT: " + message);
}

function writeAttributeValues(prefix) {
    var pre = document.createElement("p");
    pre.style.wordWrap = "break-word";
    pre.innerHTML = "INFO " + getCurrentDate() + " " + prefix + "<b> readyState: " + websocket.readyState + " bufferedAmount: " + websocket.bufferedAmount + "</b>";
    ;
    attributes_log.appendChild(pre);
}

function writeToScreen(message) {
    var pre = document.createElement("p");
    pre.style.wordWrap = "break-word";
    pre.innerHTML = message;
    output.appendChild(pre);
}

function getCurrentDate() {
    var now = new Date();
    var datetime = now.getFullYear() + '/' + (now.getMonth() + 1) + '/' + now.getDate();
    datetime += ' ' + now.getHours() + ':' + now.getMinutes() + ':' + now.getSeconds();

    return datetime;
}

function browserSupportsWebSockets() {
    if ("WebSocket" in window)
    {
        return true;
    }
    else
    {
        return false;
    }
}