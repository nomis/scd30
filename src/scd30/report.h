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

#ifndef SCD30_REPORT_H_
#define SCD30_REPORT_H_

#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecureBearSSL.h>

#include <cmath>
#include <deque>
#include <string>

#include <uuid/log.h>

namespace scd30 {

struct __attribute__((packed)) Reading {
	static constexpr size_t TEMP_BITS = 14;
	static constexpr int TEMP_DIV = 100;
	static constexpr int TEMP_MUL = 100 / TEMP_DIV;
	static_assert(TEMP_DIV * TEMP_MUL == 100);
	static constexpr long TEMP_MIN = -(1 << (TEMP_BITS - 1)) + 1;
	static_assert(TEMP_MIN == -8191); /* -81.91°C */
	static constexpr long TEMP_MAX = (1 << (TEMP_BITS - 1)) - 1;
	static_assert(TEMP_MAX == 8191); /* 81.91°C */
	static constexpr long TEMP_NAN = TEMP_MIN - 1;

	static constexpr size_t RHUM_BITS = 14;
	static constexpr int RHUM_DIV = 100;
	static constexpr int RHUM_MUL = 100 / RHUM_DIV;
	static_assert(RHUM_DIV * RHUM_MUL == 100);
	static constexpr long RHUM_MIN = 0; /* 0% */
	static constexpr long RHUM_MAX = (1 << RHUM_BITS) - 2;
	static_assert(RHUM_MAX == 16382); /* 163.82% */
	static constexpr long RHUM_NAN = RHUM_MAX + 1;

	static constexpr size_t CO2_BITS = 20;
	static constexpr int CO2_DIV = 20;
	static constexpr int CO2_MUL = 100 / CO2_DIV;
	static_assert(CO2_DIV * CO2_MUL == 100);
	static constexpr long CO2_MIN = 0; /* 0 ppm */
	static constexpr long CO2_MAX = (1 << CO2_BITS) - 2;
	static_assert(CO2_MAX == 1048574); /* 41942.96 ppm */
	static constexpr long CO2_NAN = CO2_MAX + 1;

	Reading(uint32_t timestamp_, float temperature_c_,
			float relative_humidity_pc_, float co2_ppm_)
			: timestamp(timestamp_) {
		if (std::isnormal(temperature_c_)) {
			temperature_c = std::max(TEMP_MIN, std::min(TEMP_MAX, std::lroundf(temperature_c_ * TEMP_DIV)));
		} else {
			temperature_c = TEMP_NAN;
		}

		if (std::isnormal(relative_humidity_pc_)) {
			relative_humidity_pc = std::max(RHUM_MIN, std::min(RHUM_MAX, std::lroundf(relative_humidity_pc_ * RHUM_DIV)));
		} else {
			relative_humidity_pc = RHUM_NAN;
		}

		if (std::isnormal(co2_ppm_)) {
			co2_ppm = std::max(CO2_MIN, std::min(CO2_MAX, std::lroundf(co2_ppm_ * CO2_DIV)));
		} else {
			co2_ppm = CO2_NAN;
		}
	}

	uint32_t timestamp;
	signed int temperature_c : TEMP_BITS;
	unsigned int relative_humidity_pc : RHUM_BITS;
	unsigned int co2_ppm : CO2_BITS;
};
static_assert(sizeof(Reading) == 10);

enum class UploadState : uint8_t {
	IDLE,
	CONNECT,
	SEND,
	RECEIVE,
	CLEANUP,
};

class Report {
public:
	void config();
	void add(uint32_t timestamp, float temperature_c, float relative_humidity_pc, float co2_ppm);
	void loop();

private:
	static constexpr size_t MAXIMUM_STORE_READINGS = 360; /* 30 minutes at a 5 second interval */
	static constexpr size_t MAXIMUM_UPLOAD_BYTES = 640;
	static constexpr int HTTP_TIMEOUT_MS = 2000;

	static uuid::log::Logger logger_;

	void upload(bool begin = false);

	std::deque<Reading> readings_;
	bool enabled_ = false;
	bool overflow_ = false;
	size_t threshold_ = 0;
	std::string url_;
	std::string username_;
	std::string password_;
	std::string sensor_name_;

	BearSSL::CertStore tls_certs_;
	BearSSL::WiFiClientSecure tls_client_;
	WiFiClient tcp_client_;
	WiFiClient *conn_client_ = nullptr;
	bool tls_loaded_ = false;
	HTTPClient http_client_;
	UploadState state_ = UploadState::IDLE;
	uint32_t upload_ts_first_;
	uint32_t upload_ts_last_;
};

} // namespace scd30

#endif
