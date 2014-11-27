/*
 * Copyright
 *
 *     Copyright (C) 2013 Brent Thomson
 *
 * License
 *
 *     This program is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU Lesser General Public License as published
 *     by the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU Lesser General Public License for more details at
 *     <http://www.gnu.org/licenses/lgpl-3.0-standalone.html>
 */

var RPIO = require('../build/Release/rpio');
var util = require('util');
var EventEmitter = require('events').EventEmitter;

function _rpio() {
  EventEmitter.call(this);
}
util.inherits(_rpio, EventEmitter);

_rpio.prototype.setup = function(channel, direction, pud_or_value) {
	if (arguments.length > 2) {
		if (direction == this.IN) {
			return RPIO.setup(channel, direction, pud_or_value || this.PUD_OFF);
		} else {
			return RPIO.setup(channel, direction, this.PUD_OFF, pud_or_value);
		}
	} else {
		return RPIO.setup(channel, direction);
	}
};

_rpio.prototype.cleanup = function() {
	return RPIO.cleanup();
};

_rpio.prototype.output = function(channel, value) {
	return RPIO.output(channel, value);
};

_rpio.prototype.input = function(channel) {
	return RPIO.input(channel);
};

_rpio.prototype.setmode = function(mode) {
	var ret_val = RPIO.setmode(mode);
	return ret_val;
};

_rpio.prototype.setwarnings = function(warn) {
	return RPIO.setwarnings(warn);
};

_rpio.prototype.set_pullupdn = function(channel, pud) {
	if (!pud) pud = this.PUD_OFF;
	return RPIO.set_pullupdown(channel, pud);
};

_rpio.prototype.gpio_function = function(channel) {
	return RPIO.gpio_function(channel);
};

_rpio.prototype.channel_to_gpio = function(channel) {
	return RPIO.channel_to_gpio(channel);
};

_rpio.prototype.IN = 1;
_rpio.prototype.OUT = 0;

_rpio.prototype.LOW = 0x0;
_rpio.prototype.HIGH = 0x1;

_rpio.prototype.PUD_OFF = 0;
_rpio.prototype.PUD_DOWN = 1;
_rpio.prototype.PUD_UP = 2;

_rpio.prototype.MODE_UNKNOWN = -1;
_rpio.prototype.BOARD = 10;
_rpio.prototype.BCM = 11;

module.exports = new _rpio;
