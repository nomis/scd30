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

#ifndef SCD30_APP_H_
#define SCD30_APP_H_

#include <Arduino.h>

#include <initializer_list>
#include <memory>
#include <vector>

#include <uuid/syslog.h>
#include <uuid/telnet.h>

#include "scd30/console.h"
#include "scd30/network.h"
#include "scd30/sensor.h"

namespace scd30 {

class App {
private:
#if defined(ARDUINO_ESP8266_WEMOS_D1MINI) || defined(ESP8266_WEMOS_D1MINI)
	static constexpr unsigned long SERIAL_CONSOLE_BAUD_RATE = 115200;
	static constexpr auto& serial_console_ = Serial;

	static constexpr unsigned long SERIAL_MODBUS_BAUD_RATE = 19200;
	static constexpr auto& serial_modbus_ = Serial;

	static constexpr int SENSOR_PIN = 12; /* D6 */
	static constexpr int CONSOLE_PIN = 14; /* D5 */
#else
# error "Unknown board"
#endif

public:
	static void start();
	static void loop();

	static void config_syslog();
	static void config_ota();
	static void config_sensor(std::initializer_list<Operation> operations = {});
	static void calibrate_sensor(unsigned long ppm);

	static const Sensor& sensor() { return sensor_; }

private:
	App() = delete;

	static void shell_prompt();

	static uuid::log::Logger logger_;
	static scd30::Network network_;
	static uuid::syslog::SyslogService syslog_;
	static uuid::telnet::TelnetService telnet_;
	static std::shared_ptr<scd30::SCD30Shell> shell_;
	static scd30::Sensor sensor_;
	static bool local_console_;
	static bool ota_running_;
};

} // namespace scd30

#endif
