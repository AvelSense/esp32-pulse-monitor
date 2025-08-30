/**
 * @file realtime_plotter.js
 * @description This file generates the client-side live visualisation of the measurements made by the ESP32.
 * @author Matthieu Bouveron
 * @copyright 2025 Matthieu Bouveron
 * @license MIT
 *
 * This file is part of ESP32_Pulse_Monitor.
 *
 * ESP32_Pulse_Monitor is free software: you can redistribute it and/or modify
 * it under the terms of the MIT License.
 *
 * Dependencies:
 * - plotly.js (v3.1.0-rc.0) - Copyright 2012-2025, Plotly, Inc. (MIT License)
 */

const timeout = 30;
let pause = false;
const timestamps = [];
const trace1 = {
    x: [],
    y: [],
    fill: 'tozeroy',
    type: 'scatter',
    mode: 'lines+markers',
    marker: {
        color: 'red',
        size: 10
    },
    line: {
        color: 'blue',
        shape: 'hv'
    }
};

const layout = {
    title: 'PWM monitor',
    xaxis: {
        title: 'Time (s)',
        range: [-timeout, 0]
    },
    yaxis: {
        title: 'PWM value',
//    range: [600, 2400],
        side: 'right'
    }
};

Plotly.newPlot('plotDiv', [trace1], layout);

let sequence = 0;

function process_data(data) {
    sequence += 1;

    let last_rising_edge = timestamps.length > 0 ? timestamps[timestamps.length - 1] : null;

    // Extract data
    data.forEach((element) => {
        if (element && element.length === 2) {
            const [time_rising, time_falling] = element;
            if (time_rising > time_falling) {
                console.log("Negative time up", data);
            }
            if (last_rising_edge && last_rising_edge > time_rising) {
                console.log("Next in sequence appeared earlier in time", data);
                return;
            }
            timestamps.push(time_rising);
            trace1.y.push(time_falling - time_rising);
            last_rising_edge = time_rising;
        }
    });

    // Remove outdated data
    if (last_rising_edge !== null) {
        while (timestamps.length > 0 && last_rising_edge - timestamps[0] > 1000000 * timeout) {
            timestamps.shift();
            trace1.y.shift();
        }

        // Adjust timestamps for display relative to the last rising edge
        trace1.x = timestamps.map(timestamp => (timestamp - last_rising_edge) / 1000000);

        if (!pause) {
            Plotly.update('plotDiv', [trace1], layout);
        }
    }
}

const interval = setInterval(function () {
    const xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function () {
        if (this.readyState === 4 && this.status === 200) {
            const values_history = JSON.parse(this.responseText);
            if (values_history) {
                process_data(values_history);
            }
        }
    };
    xhttp.open("GET", "/data", true);
    xhttp.send();
}, 200);
