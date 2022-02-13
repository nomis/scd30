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

#include <scd30/report.h>

#include <Arduino.h>

#include <uuid/log.h>

#include "scd30/config.h"

static const char __pstr__logger_name[] __attribute__((__aligned__(sizeof(int)))) PROGMEM = "report";

namespace scd30 {

uuid::log::Logger Report::logger_{FPSTR(__pstr__logger_name), uuid::log::Facility::DAEMON};

void Report::config() {
	Config config;

	bool was_enabled = enabled_;

	enabled_ = config.report_enabled();
	threshold_ = config.report_threshold();
	url_ = config.report_url();
	username_ = config.report_username();
	password_ = config.report_password();
	sensor_name_ = config.report_sensor_name();

	if (threshold_ == 0) {
		enabled_ = false;
	}

	if (url_.empty()) {
		enabled_ = false;
	}

	if (username_.empty()) {
		enabled_ = false;
	}

	if (password_.empty()) {
		enabled_ = false;
	}

	if (sensor_name_.empty()) {
		enabled_ = false;
	}

	if (was_enabled != enabled_) {
		logger_.info(F("Reporting %s"), enabled_ ? F("enabled") : F("disabled"));
	}
}

void Report::add(uint32_t timestamp, float temperature_c, float relative_humidity_pc, float co2_ppm) {
	if (timestamp < 19035 * 86400) {
		return;
	}

	if (!readings_.empty()) {
		if (readings_.back().timestamp >= timestamp) {
			logger_.trace(F("Ignoring old reading at %u, before %u"), timestamp, readings_.back().timestamp);
			return;
		}
	}

	while (readings_.size() >= MAXIMUM_STORE_READINGS) {
		if (!overflow_) {
			logger_.warning(F("Reading storage overflow, discarding old readings"));
			overflow_ = true;
		}

		logger_.trace(F("Discard reading from %u"), readings_.front().timestamp);
		readings_.pop_front();
	}

	readings_.emplace_back(timestamp, temperature_c, relative_humidity_pc, co2_ppm);
	logger_.trace(F("Add reading %u at %u"), readings_.size(), timestamp);

	upload();
}

void Report::upload() {
	if (enabled_ && readings_.size() >= threshold_) {

	}
}

void Report::loop() {
	if (readings_.empty()) {
		overflow_ = false;
	}
}

} // namespace scd30
