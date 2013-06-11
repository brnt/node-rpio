var rpio = require('../lib/rpio');

rpio.setmode(rpio.BCM);

var pin = 9;
rpio.setup(pin, rpio.IN);
var value = rpio.input(pin);

console.log('pin ' + pin + ' = ' + value);
