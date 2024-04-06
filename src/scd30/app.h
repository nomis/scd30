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

#pragma once

#include <Arduino.h>

#include <initializer_list>
#include <memory>
#include <vector>

#include <uuid/syslog.h>
#include <uuid/telnet.h>

#include "app/app.h"
#include "app/console.h"
#include "app/network.h"
#include "report.h"
#include "sensor.h"

namespace scd30 {

class App: public app::App {
private:
	static constexpr unsigned long SERIAL_MODBUS_BAUD_RATE = 19200;

#if defined(ARDUINO_ESP8266_WEMOS_D1MINI) || defined(ESP8266_WEMOS_D1MINI)
	static constexpr auto& serial_modbus_ = Serial;

	static constexpr int SENSOR_PIN = 12; /* D6 */
#elif defined(ARDUINO_LOLIN_S2_MINI)
	static constexpr auto& serial_modbus_ = Serial1;

	/* RX = 18 */
	/* TX = 17 */
	static constexpr int SENSOR_PIN = 12;
#else
# error "Unknown board"
#endif

public:
	App();
	void start() override;
	void loop() override;

	void config_sensor(std::initializer_list<Operation> operations = {});
	void calibrate_sensor(unsigned long ppm);
	void config_report();

	const Sensor& sensor() { return sensor_; }

private:
	scd30::Report report_;
	scd30::Sensor sensor_;
};

} // namespace scd30
