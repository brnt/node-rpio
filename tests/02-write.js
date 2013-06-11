var rpio = require('../lib/rpio');

rpio.setmode(rpio.BCM);

var gpio = 7;
rpio.setup(gpio, rpio.OUT, rpio.HIGH);
var old_value = rpio.input(gpio);
console.log('gpio ' + gpio + ' = ' + old_value);

rpio.output(gpio, (old_value+1)%2);
var new_value = rpio.input(gpio);

console.log('gpio ' + gpio + ' = ' + new_value);
rpio.cleanup();
