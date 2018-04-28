var roundtripTime = document.getElementById('roundtripTime');
var messageCount = document.getElementById('messageCount');
var missedReplies = document.getElementById('missedReplies');
var messageCounter = 0;
var missedMessageCounter = 0;
var messageSendMilis;

var sampleThreshold;
var pressure;

var connection;
var connectionStatus = document.getElementById('connectionStatus');
var connectionPings = document.getElementById('connectionPings');
//var hostName = document.getElementById('hostName');
var messageWindow = document.getElementById('messageWindow');
var messageSize = document.getElementById('messageSize');
var throughput = document.getElementById('throughput');
var extraPayload = "";

var pingCounter = 0;
var sendPingVar;
var setRetryInterval;
var dateObject;

var pressure = 0;
var pressure_LOW;
var pressure_HIGH;
var color = false;

var ws;
var retries = 0;
document.getElementById("retries").innerHTML = retries;
var uptime = 0;
document.getElementById("uptime").innerHTML = uptime;



var updateCount = 0;

var time = new Date();

var d;
var n;
var e;

var updateTime = function(count) {
    d = new Date();
    n = d.getTime();
    e = d.getTime() - time.getTime();

    var field = String(d).split(" ");
    document.getElementById("time").innerHTML = field[4];

}


var lastTimeDataReceived = n;

document.getElementById('hostName').textContent = location.host;

var hostName = "ws://" + location.host + "/ws";






var canvasCircle = document.getElementById('canvasCircle');
var ctx = canvasCircle.getContext('2d');

ctx.strokeStyle = "black";
ctx.lineWidth = 5;
ctx.strokeRect(0, 0, 30, 30);

var varBlinkRect;

function redRect() {
    //ctx.clearRect(0, 0, 0, 0);
    ctx.fillStyle = 'white';
    ctx.fillRect(2.5, 2.5, 12.5, 25);
    ctx.fillStyle = 'red';
    //ctx.fillRect(7.5, 7.5, 0.5 * canvasCircle.width, 0.5 * canvasCircle.height);
    ctx.fillRect(15, 2.5, 12.5, 25);
}



window.onload = function() {
    //wsOpen();
    //connectWebsocket(hostName.value);
    //connectWebsocket(hostName);
    connectionOpen();
    document.getElementById("led-switch").disabled = true;
    //startPolling();
}


function setMsg(cls, text) {
    sbox = document.getElementById('status_box');
    sbox.className = "alert alert-" + cls;
    sbox.innerHTML = text;
    console.log(text);
}




var opts = {
    angle: -0.3, // The span of the gauge arc
    lineWidth: 0.02, // The line thickness
    radiusScale: 1, // Relative radius
    pointer: {
        length: 0.6, // // Relative to gauge radius
        strokeWidth: 0.013, // The thickness
        color: 'rgba(95,95,95,1.0)' // Fill color
    },
    limitMax: true, // If false, the max value of the gauge will be updated if value surpass max
    limitMin: false, // If true, the min value of the gauge will be fixed unless you set it manually
    // colorStart: '#6F6EA0', // Colors
    colorStop: 'rgba(3, 169, 244,1.0)', // just experiment with them
    strokeColor: '#EEEEEE', // to see which ones work best for you
    //generateGradient: true,
    highDpiSupport: true, // High resolution support
    staticLabels: {
        font: "14px sans-serif", // Specifies font
        // labels: [-0.2, 0, 0.5, 1.0, 1.5, 2], // Print labels at these values
        labels: [-0.2, 0, 0.2, 0.4, 0.6, 0.8, 1.0, 1.2, 1.4, 1.6, 1.8, 2], // Print labels at these values
        color: "#000000", // Optional: Label text color
        // color: "rgba(244, 78, 3,1.0)",
        fractionDigits: 1 // Optional: Numerical precision. 0=round off.
    },
};
var target = document.getElementById('gauge'); // your canvas element
var gauge = new Gauge(target).setOptions(opts); // create sexy gauge!
gauge.maxValue = 2; // set max gauge value
gauge.setMinValue(-0.2); // Prefer setter over gauge.minValue = 0
//gauge.minValue = -1;
gauge.animationSpeed = 5; // set animation speed (32 is default value)
//gauge.setTextField(document.getElementById("preview-textfield",2));
// var textRenderer = new TextRenderer(document.getElementById('preview-textfield'))
// textRenderer.render = function(gauge) {
//     //percentage = gauge.displayedValue / gauge.maxValue
//     percentage = gauge.displayedValue
//     // this.el.innerHTML = (percentage * 100).toFixed(2) + "%"
//     this.el.innerHTML = percentage.toFixed(2)
// };
// gauge.setTextField(textRenderer);
gauge.set(0); // set actual value
document.getElementById('preview-textfield').innerHTML = pressure.toFixed(2);
document.getElementById('unit-pressure').innerHTML = "bar";

var slider1 = document.getElementById('slider1');

noUiSlider.create(slider1, {
    start: [0.0, 2.0],
    tooltips: [true, true],
    connect: true,
    step: 0.05,
    range: {
        'min': 0,
        'max': 2
    }
});
slider1.setAttribute('disabled', true);
slider1.noUiSlider.on('change', function(values, handle) {
    var sliderValue = values;
    //if (handle == 0) {
    connection.send("c " + sliderValue[0] + " " + sliderValue[1]);
    //}
});



setInterval(function() {
    //retry();
    updateTime();
    var rate;
    rate = (n - lastTimeDataReceived) / 1000;
    document.getElementById("lastTimeDataReceived").innerHTML = rate;
}, 1);



function connectionOpen() {
    if (connection === undefined || connection.readyState != 1) {
        if (retries)
            setMsg("error", "WebSocket timeout, retrying..");
        else
            setMsg("info", "Opening WebSocket..");
        connection = new WebSocket(hostName, ['arduino']);
        connection.binaryType = 'arraybuffer';

        connection.onopen = function() {
            setRetryInterval = setInterval(function() {
                upTime();
            }, 1000);
            retries = 0;
            setMsg("done", "WebSocket is open.");
            document.getElementById("led-switch").disabled = false;
            document.getElementById("set-switch").disabled = false;
            sendMessage();
        };

        connection.onclose = function(event) {
            gauge.set(0);
            connection = 0;
            document.getElementById("led-switch").disabled = true;
            document.getElementById("set-switch").disabled = true;
            document.getElementById('preview-textfield').innerHTML = "0.00";
            clearInterval(setRetryInterval);
            setMsg("error", "Disconnected!");
            slider1.setAttribute('disabled', true);
            retries = 0;
            uptime = 0;
            messageCounter = 0;
            missedMessageCounter = 0;
        };
        connection.onerror = function(error) {
            setMsg("error", "WebSocket error!");
            console.log("WebSocket Error ", error);
        };
        connection.onmessage = function(message) {
            onMessage(message);
        };
        retries = 0;
        console.log("aha");
    }
}

function onMessage(message) {
    retries = 0;
    color = !color;

    if (color == true) {
        ctx.fillStyle = 'rgba(3, 169, 244,1.0)';
        ctx.fillRect(2.5, 2.5, 25, 25);
        //ctx.fillStyle = 'white';
        //ctx.fillRect(15, 2.5, 12.5, 25);
    } else {
        ctx.fillStyle = 'white';
        ctx.fillRect(2.5, 2.5, 25, 25);
        //ctx.fillStyle = 'rgba(3, 169, 244,1.0)';
        //ctx.fillRect(15, 2.5, 12.5, 25);
    }

    //updateTime();

    var throughput;

    throughput = (n - lastTimeDataReceived) / 1000;
    lastTimeDataReceived = n;

    document.getElementById("throughput").innerHTML = throughput;

    var fileds = message.data.split(" ");

    // show message
    messageWindow.value = message.data;
    messageSize.value = message.data.length;

    if (fileds[0] == "#") {

        if (fileds[1] != messageCounter) {
            //missedMessageCounter++;
            //missedReplies.value = missedMessageCounter;
            //connection.close();
            console.log("missed reply");
        }

        sendMessage();

        dateObject = new Date();

        pressure = fileds[2];
        pressure_LOW = fileds[3];
        pressure_HIGH = fileds[4];
        gauge.set(pressure);
        document.getElementById('preview-textfield').innerHTML = pressure;
        var handleValue = slider1.noUiSlider.get();

        if (updateSlider1) {
            console.log("NO");
        } else {
            slider1.noUiSlider.set([fileds[3], fileds[4]]);
            console.log("OK");
        }

        if (fileds[3] == handleValue[0]) {
            //console.log("yes");
        } else {
            //console.log("no");
        }
        // slider1.noUiSlider.set([fileds[3], fileds[4]]);
    }

    if (fileds[1] > messageCounter + 1) {
        missedMessageCounter = fileds[1] - messageCounter;
        //missedReplies.value = missedMessageCounter;
        document.getElementById("missedReplies").innerHTML = missedMessageCounter;
    }
    //messageCounter = fileds[1];

    var finalFloat;

    document.getElementById("pressure").innerHTML = pressure;
    document.getElementById("messageWindow").innerHTML = message.data;
    document.getElementById("messageSize").innerHTML = message.data.length;
}

var updateSlider1;

function tempStopUpdateSlider1() {
    if (document.getElementById('set-switch').checked) {
        updateSlider1 = 1;
        slider1.removeAttribute('disabled');
    } else {
        updateSlider1 = 0;
        slider1.setAttribute('disabled', true);
    }
}

function retry() {
    retries++;
    document.getElementById("retries").innerHTML = retries;
}

function upTime() {
    uptime++;
    document.getElementById("uptime").innerHTML = uptime;
}

function SwitchWs() {
    if (connection === 0 || connection === undefined) {
        connectionOpen();
    } else if (connection.readyState === 1) {
        disconnect();
    }
}

function sendMessage() {
    if (connection.readyState === 1) {
        messageCounter++;
        //messageCount.value = messageCounter;
        //dateObject = new Date();
        //messageSendMilis = dateObject.getTime();
        //
        // Message format
        // # MESSAGE_NUMBER SAMPLE_THRESHOLD NUMBER_OF_SAMPLES
        //
        connection.send("r " + messageCounter);
        //ws.send("# " + messageCounter + " " + sampleThreshold + " " + sampleSize);
    }
}

function disconnect() {
    if (connection.readyState === 1) {
        connection.close();
    } else if (connection.readyState === 3 || connection === undefined) {
        //connectionStatus.value = "Not connected yet";
        setMsg("done", "Not connected yet.");
    }
}

// -----------------------------------------------------------------------
// Event: Save Pressure Settings
// -----------------------------------------------------------------------
function savePressureSettings() {
    var pressureSet = {
        Lo: pressure_LOW,
        Hi: pressure_HIGH
    };

    document.getElementById("save-pressure").innerHTML = "Saving...";
    var r = new XMLHttpRequest();
    //r.open("POST", "http://192.168.10.122/savepressure", true);
    r.open("POST", "savepressure", true);
    r.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
    r.send("&LO_SETTING=" + pressureSet.Lo + "&HI_SETTING=" + pressureSet.Hi);
    r.onreadystatechange = function() {
        console.log(pressureSet);
        if (r.readyState != 4 || r.status != 200) return;
        var str = r.responseText;
        console.log(str);
        if (str !== 0) document.getElementById("save-pressure").innerHTML = "Saved";
    };
}

function sendPing() {
    connection.send('ping');
    pingCounter++;
    document.getElementById("connectionPings").innerHTML = pingCounter;
}

function wsWrite(data) {

    connection.send(data);
    if (connection.readyState == 3)
        //wsOpen();
        //connectWebsocket(hostName.value);
        //connectWebsocket(hostName);
        //connectionOpen();
        console.log("readyState == 3");
    //else if (retries++ > 1)
    //console.log("retries++ > 5");
    else if (connection.readyState == 1)
        //connection.send(data);
        //connection.send('Hallo from Browser :-) ' + new Date());
        console.log(data);
}

function gpio() {
    //document.getElementById('retries').innerHTML = retries;
    if (document.getElementById('led-switch').checked) {
        wsWrite('c E E E');
        //document.getElementById('retries').innerHTML = retries;
    } else {
        wsWrite('c D D D');
        //document.getElementById('retries').innerHTML = retries;
    }
}
