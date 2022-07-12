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

#include <initializer_list>
#include <memory>
#include <vector>

#include <uuid/common.h>
#include <uuid/console.h>
#include <uuid/log.h>
#include <uuid/modbus.h>
#include <uuid/syslog.h>
#include <uuid/telnet.h>

#include "app/config.h"
#include "app/console.h"
#include "app/network.h"
#include "scd30/report.h"
#include "scd30/sensor.h"

namespace scd30 {

App::App() : sensor_(App::serial_modbus_, App::SENSOR_PIN, report_) {

}

void App::start() {
	app::App::start();

	if (!local_console_enabled()) {
		serial_modbus_.begin(SERIAL_MODBUS_BAUD_RATE, SERIAL_8N1);
		serial_modbus_.setDebugOutput(0);
		sensor_.start();
	}

	config_report();
}

void App::loop() {
	app::App::loop();

	if (!local_console_enabled()) {
		sensor_.loop();
		report_.loop();
	}
}

void App::config_sensor(std::initializer_list<Operation> operations) {
	sensor_.config(operations);
}

void App::calibrate_sensor(unsigned long ppm) {
	sensor_.calibrate(ppm);
}

void App::config_report() {
	report_.config();
}

} // namespace scd30
