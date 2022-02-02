/*
 * scd30 - SCD30 Monitor
 * Copyright 2022  Simon Arlott
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
 */

#include "scd30/app.h"

#include <Arduino.h>

#include <memory>
#include <vector>

#include <uuid/common.h>
#include <uuid/console.h>
#include <uuid/log.h>
#include <uuid/syslog.h>
#include <uuid/telnet.h>

#include "scd30/config.h"
#include "scd30/console.h"
#include "scd30/network.h"

static const char __pstr__logger_name[] __attribute__((__aligned__(sizeof(int)))) PROGMEM = "scd30";
static const char __pstr__enabled[] __attribute__((__aligned__(sizeof(int)))) PROGMEM = "enabled";
static const char __pstr__disabled[] __attribute__((__aligned__(sizeof(int)))) PROGMEM = "disabled";

namespace scd30 {

uuid::log::Logger App::logger_{FPSTR(__pstr__logger_name), uuid::log::Facility::KERN};
scd30::Network App::network_;
uuid::syslog::SyslogService App::syslog_;
uuid::telnet::TelnetService App::telnet_([] (Stream &stream, const IPAddress &addr, uint16_t port) -> std::shared_ptr<uuid::console::Shell> {
	return std::make_shared<scd30::SCD30StreamConsole>(stream, addr, port);
});
std::shared_ptr<SCD30Shell> App::shell_;
bool App::local_console_;

void App::start() {
	pinMode(CONSOLE_PIN, INPUT_PULLUP);
	local_console_ = digitalRead(CONSOLE_PIN) == LOW;
	pinMode(CONSOLE_PIN, INPUT);
	pinMode(SENSOR_PIN, INPUT);

	syslog_.start();

	logger_.info(F("System startup (scd30 " SCD30_REVISION ")"));
	logger_.info(F("Reset: %s"), ESP.getResetInfo().c_str());
	logger_.info(F("Local console %S"), local_console_ ? F("enabled") : F("disabled"));

	if (local_console_) {
		serial_console_.begin(SERIAL_CONSOLE_BAUD_RATE);
		serial_console_.println();
		serial_console_.println(F("scd30 " SCD30_REVISION));
	} else {
		serial_modbus_.begin(SERIAL_MODBUS_BAUD_RATE);
		serial_modbus_.setDebugOutput(0);
	}

	network_.start();
	config_syslog();
	telnet_.default_write_timeout(1000);
	telnet_.start();
	if (local_console_) {
		shell_prompt();
	}
}

void App::loop() {
	uuid::loop();
	syslog_.loop();
	telnet_.loop();
	uuid::console::Shell::loop_all();

	if (local_console_) {
		if (shell_) {
			if (!shell_->running()) {
				shell_.reset();
				shell_prompt();
			}
		} else {
			int c = serial_console_.read();
			if (c == '\x03' || c == '\x0C') {
				shell_ = std::make_shared<SCD30StreamConsole>(serial_console_, c == '\x0C');
				shell_->start();
			}
		}
	}
}

void App::shell_prompt() {
	serial_console_.println();
	serial_console_.println(F("Press ^C to activate this console"));
}

void App::config_syslog() {
	Config config;
	IPAddress addr;

	if (!addr.fromString(config.syslog_host().c_str())) {
		addr = (uint32_t)0;
	}

	syslog_.hostname(config.hostname());
	syslog_.log_level(config.syslog_level());
	syslog_.mark_interval(config.syslog_mark_interval());
	syslog_.destination(addr);
}

} // namespace scd30
