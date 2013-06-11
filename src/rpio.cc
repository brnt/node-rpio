/*
 * This file is based on RPIO.
 *
 * Copyright
 *
 *     Copyright (C) 2013 Chris Hager <chris@linuxuser.at> (original Python version)
 *     Copyright (C) 2013 Brent Thomson (modifications for NodeJS)
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
 *
 */

#include <stdio.h>
#include <unistd.h>

#include <node.h>
#include <v8.h>

#include "c_gpio.h"
#include "cpuinfo.h"

using namespace v8;

#define NUM_GPIOS   54
static int gpio_direction[NUM_GPIOS];

// GPIO Modes
#define MODE_UNKNOWN -1
#define BOARD       10
#define BCM         11
static int gpio_mode = MODE_UNKNOWN;

// Flag whether to show warnings
static int gpio_warnings = 1;

// Which Raspberry Pi Revision is used (will be 1 or 2; 0 if not a Raspberry Pi).
// Source: /proc/cpuinfo (via cpuinfo.c)
static int revision_int = 0;
static char revision_hex[1024] = {'\0'};

// Conversion from board_pin_id to gpio_id
// eg. gpio_id = *(*pin_to_gpio_rev2 + board_pin_id);
static const int pin_to_gpio_rev1[27] = {-1, -1, -1, 0, -1, 1, -1, 4, 14, -1, 15, 17, 18, 21, -1, 22, 23, -1, 24, 10, -1, 9, 25, 11, 8, -1, 7};
static const int pin_to_gpio_rev2[27] = {-1, -1, -1, 2, -1, 3, -1, 4, 14, -1, 15, 17, 18, 27, -1, 22, 23, -1, 24, 10, -1, 9, 25, 11, 8, -1, 7};
static const int (*pin_to_gpio)[27];

// Board header info is shifted left 8 bits (leaves space for up to 255 channel ids per header)
#define HEADER_P1 0<<8
#define HEADER_P5 5<<8
static const int gpio_to_pin_rev1[32] = {3, 5, -1, -1, 7, -1, -1, 26, 24, 21, 19, 23, -1, -1, 8, 10, -1, 11, 12, -1, -1, 13, 15, 16, 18, 22, -1, -1, -1, -1, -1, -1};
static const int gpio_to_pin_rev2[32] = {-1, -1, 3, 5, 7, -1, -1, 26, 24, 21, 19, 23, -1, -1, 8, 10, -1, 11, 12, -1, -1, -1, 15, 16, 18, 22, -1, 15, 3 | HEADER_P5, 4 | HEADER_P5, 5 | HEADER_P5, 6 | HEADER_P5};
static const int (*gpio_to_pin)[32];

// Read /proc/cpuinfo once and keep the info at hand for further requests
void
cache_rpi_revision(void)
{
    revision_int = get_cpuinfo_revision(revision_hex);
}

// bcm_to_board() returns the pin for the supplied bcm_gpio_id or -1
// if not a valid gpio-id. P5 pins are returned with | HEADER_P5, so
// you can know the header with (retval >> 8) (either 0 or 5) and the
// exact pin number with (retval & 255).
int bcm_to_board(int bcm_gpio_id)
{
    return *(*gpio_to_pin+bcm_gpio_id);
}

// channel_to_bcm() returns the bcm gpio id for the supplied channel
// depending on current setmode. Only P1 header channels are supported.
// To use P5 you need to use BCM gpio ids (`setmode(BCM)`).
int board_to_bcm(int board_pin_id)
{
    return *(*pin_to_gpio+board_pin_id);
}

int channel_to_gpio(int channel)
{
    int gpio;

     if (gpio_mode != BOARD && gpio_mode != BCM) {
        printf("Please set pin numbering mode using RPIO.setmode(RPIO.BOARD) or RPIO.setmode(RPIO.BCM)");
        return -1;
    }

   if ( (gpio_mode == BCM && (channel < 0 || channel > 31)) ||
        (gpio_mode == BOARD && (channel < 1 || channel > 26)) ) {
        printf("The channel sent is invalid on a Raspberry Pi (outside of range)");
        return -2;
    }

    if (gpio_mode == BOARD) {
        if ((gpio = board_to_bcm(channel)) == -1) {
            printf("The channel sent is invalid on a Raspberry Pi (not a valid pin)");
            return -3;
        }
    } else {
        // gpio_mode == BCM
        gpio = channel;
        if (bcm_to_board(gpio) == -1) {
            printf("The channel sent is invalid on a Raspberry Pi (not a valid gpio)");
            return -3;
        }
    }

    return gpio;
}

int verify_input(int channel, int *gpio)
{
    if ((*gpio = channel_to_gpio(channel)) == -1)
        return 0;

    if ((gpio_direction[*gpio] != INPUT) && (gpio_direction[*gpio] != OUTPUT)) {
        printf("GPIO channel has not been set up");
        return 0;
    }

    return 1;
}

int module_setup()
{
	int result;
    int i=0;
    for (i=0; i<54; i++) {
        gpio_direction[i] = -1;
	}
	result = setup();
    if (result == SETUP_DEVMEM_FAIL) {
        printf("No access to /dev/mem. Try running as root!");
    } else if (result == SETUP_MMAP_FAIL) {
        printf("Mmap failed on module import");
    }
	return result;
}

// sets everything back to input
Handle<Value> _cleanup(const Arguments& args)
{
	HandleScope scope;

    int i;
    for (i=0; i<54; i++) {
        if (gpio_direction[i] != -1) {
            setup_gpio(i, INPUT, PUD_OFF);
            gpio_direction[i] = -1;
        }
    }
	return scope.Close(Undefined());
}

// node function setup(channel, direction, pud_or_initial)
Handle<Value> _setup_channel(const Arguments& args)
{
	HandleScope scope;

    int gpio, channel, direction;
    int pud = PUD_OFF;
    int initial = -1;

	int len = args.Length();
	if (len < 2 || len > 4) {
		ThrowException(Exception::TypeError(String::New("Incorrect number of arguments")));
		return scope.Close(Undefined());
	}

	if (!args[0]->IsNumber() || !args[1]->IsNumber()
			|| (len > 2 && !args[2]->IsNumber())
			|| (len > 3 && !args[3]->IsNumber())) {
		ThrowException(Exception::TypeError(String::New("Incorrect argument type(s)")));
		return scope.Close(Undefined());
	}

	channel = args[0]->NumberValue();
	direction = args[1]->NumberValue();
	if (len > 2) {
		pud = args[2]->NumberValue();
	}
	if (len > 3) {
		initial = args[3]->NumberValue();
	}

    if (direction != INPUT && direction != OUTPUT) {
		ThrowException(Exception::TypeError(String::New("An invalid direction was passed to setup()")));
		return scope.Close(Undefined());
    }

    if (direction == OUTPUT)
        pud = PUD_OFF;

    if (pud != PUD_OFF && pud != PUD_DOWN && pud != PUD_UP) {
		ThrowException(Exception::TypeError(String::New("Invalid value for pull_up_down - should be either PUD_OFF, PUD_UP or PUD_DOWN")));
		return scope.Close(Undefined());
    }

    if ((gpio = channel_to_gpio(channel)) < 0) {
		ThrowException(Exception::TypeError(String::New("GPIO not found for channel")));
		return scope.Close(Undefined());
	}

    int func = gpio_function(gpio);
    if (gpio_warnings &&                              // warnings enabled and
         ((func != 0 && func != 1) ||                 // (already one of the alt functions or
         (gpio_direction[gpio] == -1 && func == 1)))  // already an output not set from this program)
    {
        printf("This channel is already in use, continuing anyway.  Use RPIO.setwarnings(False) to disable warnings.");
    }

    if (direction == OUTPUT && (initial == LOW || initial == HIGH)) {
        output_gpio(gpio, initial);
    }
    setup_gpio(gpio, direction, pud);
    gpio_direction[gpio] = direction;

	return scope.Close(Integer::New(0));
}

// node function output(channel, value)
Handle<Value> _output_gpio(const Arguments& args)
{
	HandleScope scope;

	if (args.Length() != 2) {
		ThrowException(Exception::TypeError(String::New("Incorrect number of arguments")));
		return scope.Close(Undefined());
	}

	if (!args[0]->IsNumber() || !args[1]->IsNumber()) {
		ThrowException(Exception::TypeError(String::New("Incorrect argument type(s)")));
		return scope.Close(Undefined());
	}

    int gpio,
		channel = args[0]->NumberValue(),
		value = args[1]->NumberValue();

    if ((gpio = channel_to_gpio(channel)) < 0)
		return scope.Close(Undefined());

    if (gpio_direction[gpio] != OUTPUT) {
		ThrowException(Exception::TypeError(String::New("The GPIO channel has not been set up as an OUTPUT")));
		return scope.Close(Undefined());
    }

    output_gpio(gpio, value);

	return scope.Close(Integer::New(0));
}

// node function set_pullupdn(channel, pud=PUD_OFF) without direction check
Handle<Value> _set_pullupdn(const Arguments& args)
{
	HandleScope scope;
    int gpio, channel;
    int pud = PUD_OFF;


	if (args.Length() != 2) {
		ThrowException(Exception::TypeError(String::New("Incorrect number of arguments")));
		return scope.Close(Undefined());
	}

	if (!args[0]->IsNumber() || !args[1]->IsNumber()) {
		ThrowException(Exception::TypeError(String::New("Incorrect argument type(s)")));
		return scope.Close(Undefined());
	}

	channel = args[0]->NumberValue();
	pud = args[1]->NumberValue();

    if ((gpio = channel_to_gpio(channel)) < 0) {
		return scope.Close(Undefined());
	}

    set_pullupdn(gpio, pud);

	return scope.Close(Integer::New(0));
}

// node function value = input(channel)
Handle<Value> _input_gpio(const Arguments& args)
{
	HandleScope scope;
    int gpio, channel;

	if (args.Length() != 1) {
		ThrowException(Exception::TypeError(String::New("Incorrect number of arguments")));
		return scope.Close(Undefined());
	}

	if (!args[0]->IsNumber()) {
		ThrowException(Exception::TypeError(String::New("Incorrect argument type(s)")));
		return scope.Close(Undefined());
	}

	channel = args[0]->NumberValue();
	if (!verify_input(channel, &gpio))
		return scope.Close(Undefined());

    if (input_gpio(gpio))
		return scope.Close(Integer::New(1));
    else
		return scope.Close(Integer::New(0));
}

// node function setmode(mode)
Handle<Value> _setmode(const Arguments& args)
{
	HandleScope scope;
	if (args.Length() != 1) {
		ThrowException(Exception::TypeError(String::New("Incorrect number of arguments")));
		return scope.Close(Undefined());
	}

	if (!args[0]->IsNumber()) {
		ThrowException(Exception::TypeError(String::New("Incorrect argument type(s)")));
		return scope.Close(Undefined());
	}

	gpio_mode = args[0]->NumberValue();

    if (gpio_mode != BOARD && gpio_mode != BCM) {
		ThrowException(Exception::TypeError(String::New("An invalid mode was passed to setmode()")));
		return scope.Close(Undefined());
    }
	return scope.Close(Integer::New(0));
}

// node function value = gpio_function(gpio)
Handle<Value> _gpio_function(const Arguments& args)
{
	HandleScope scope;
    int gpio, channel;

	if (args.Length() != 1) {
		ThrowException(Exception::TypeError(String::New("Incorrect number of arguments")));
		return scope.Close(Undefined());
	}

	if (!args[0]->IsNumber()) {
		ThrowException(Exception::TypeError(String::New("Incorrect argument type(s)")));
		return scope.Close(Undefined());
	}

	channel = args[0]->NumberValue();

    if ((gpio = channel_to_gpio(channel)) < 0)
        return scope.Close(Undefined());

	return scope.Close(Integer::New(gpio_function(gpio)));
}

// node function setwarnings(state)
Handle<Value> _setwarnings(const Arguments& args)
{
	HandleScope scope;
	if (args.Length() != 1) {
		ThrowException(Exception::TypeError(String::New("Incorrect number of arguments")));
		return scope.Close(Undefined());
	}

	if (!args[0]->IsNumber()) {
		ThrowException(Exception::TypeError(String::New("Incorrect argument type(s)")));
		return scope.Close(Undefined());
	}

	gpio_warnings = args[0]->NumberValue();
	return scope.Close(Undefined());
}

// node function channel_to_gpio(channel)
Handle<Value> _channel_to_gpio(const Arguments& args)
{
	HandleScope scope;
    int channel, gpio;

	if (args.Length() != 1) {
		ThrowException(Exception::TypeError(String::New("Incorrect number of arguments")));
		return scope.Close(Undefined());
	}

	if (!args[0]->IsNumber()) {
		ThrowException(Exception::TypeError(String::New("Incorrect argument type(s)")));
		return scope.Close(Undefined());
	}

	channel = args[0]->NumberValue();

    if ((gpio = channel_to_gpio(channel)) < 0) {
		ThrowException(Exception::TypeError(String::New("Unknown error")));
		return scope.Close(Undefined());
	}

	return scope.Close(Integer::New(gpio));
}

void init(Handle<Object> exports, Handle<Object> module)
{

	exports->Set(String::NewSymbol("setup"),
	    FunctionTemplate::New(_setup_channel)->GetFunction());

	exports->Set(String::NewSymbol("cleanup"),
	    FunctionTemplate::New(_cleanup)->GetFunction());

	exports->Set(String::NewSymbol("output"),
	    FunctionTemplate::New(_output_gpio)->GetFunction());

	exports->Set(String::NewSymbol("input"),
	    FunctionTemplate::New(_input_gpio)->GetFunction());

	exports->Set(String::NewSymbol("setmode"),
	    FunctionTemplate::New(_setmode)->GetFunction());

	exports->Set(String::NewSymbol("setwarnings"),
	    FunctionTemplate::New(_setwarnings)->GetFunction());

	exports->Set(String::NewSymbol("set_pullupdn"),
	    FunctionTemplate::New(_set_pullupdn)->GetFunction());

	exports->Set(String::NewSymbol("gpio_function"),
	    FunctionTemplate::New(_gpio_function)->GetFunction());

	exports->Set(String::NewSymbol("channel_to_gpio"),
	    FunctionTemplate::New(_channel_to_gpio)->GetFunction());

    cache_rpi_revision();
    if (revision_int < 1) {
		ThrowException(Exception::TypeError(String::New("This module can only be run on a Raspberry Pi!")));
    } else if (revision_int == 1) {
        pin_to_gpio = &pin_to_gpio_rev1;
        gpio_to_pin = &gpio_to_pin_rev1;
    } else {
        // assume revision 2
        pin_to_gpio = &pin_to_gpio_rev2;
        gpio_to_pin = &gpio_to_pin_rev2;
    }

    // set up mmaped areas
    if (module_setup() != SETUP_OK ) {
		cleanup();
    }
}

NODE_MODULE(rpio, init)
