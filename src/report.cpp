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
#ifdef ARDUINO_ARCH_ESP8266
# include <ESP8266HTTPClient.h>
# include <WiFiClientSecureBearSSL.h>
#else
# include <HTTPClient.h>
#endif

#include <cmath>
#include <string>
#include <vector>

#include <uuid/log.h>

#include "scd30/config.h"
#include "scd30/fs.h"

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

	if (url_.empty()
			|| (url_.rfind(uuid::read_flash_string(F("https://")), 0) != 0
				&& url_.rfind(uuid::read_flash_string(F("http://")), 0) != 0)) {
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

	if (enabled_) {
#ifdef ARDUINO_ARCH_ESP8266
		if (url_.rfind(uuid::read_flash_string(F("https://")), 0) == 0) {
			if (!tls_loaded_) {
				tls_client_.setBufferSizes(512, 512);
				tls_client_.setSSLVersion(BR_TLS12);

				logger_.info(F("Loading CA certificates"));
				int certs = tls_certs_.initCertStore(FS, PSTR("/certs.idx"), PSTR("/certs.ar"));
				tls_client_.setCertStore(&tls_certs_);
				logger_.info(F("Loaded CA certificates: %u"), certs);

				tls_loaded_ = true;
			}

			conn_client_ = &tls_client_;
		} else {
			conn_client_ = &tcp_client_;
		}
#endif
	}

	if (state_ > UploadState::CONNECT && state_ < UploadState::CLEANUP) {
		http_client_.end();
	}
	state_ = UploadState::IDLE;

	http_client_.setReuse(true);
	http_client_.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
	http_client_.setTimeout(HTTP_TIMEOUT_MS);
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
			logger_.alert(F("Reading storage overflow, discarding old readings"));
			overflow_ = true;
		}

		logger_.trace(F("Discard reading from %u"), readings_.front().timestamp);
		readings_.pop_front();
	}

	readings_.emplace_back(timestamp, temperature_c, relative_humidity_pc, co2_ppm);
	logger_.trace(F("Add reading %u at %u"), readings_.size(), timestamp);

	upload(true);
}

void Report::upload(bool begin) {
	switch (state_) {
	case UploadState::IDLE:
		if (begin && enabled_ && readings_.size() >= threshold_) {
			state_ = UploadState::CONNECT;
		}
		break;

	case UploadState::CONNECT:
		http_client_.begin(*conn_client_, url_.c_str());
		state_ = UploadState::SEND;
		break;

	case UploadState::SEND:
		{
			String payload(static_cast<char*>(nullptr));
			size_t count = 0;

			payload.reserve(MAXIMUM_UPLOAD_BYTES);

			// TODO urlencode values
			payload.concat(F("u="));
			payload.concat(username_.c_str());
			payload.concat(F("&p="));
			payload.concat(password_.c_str());
			payload.concat(F("&n="));
			payload.concat(sensor_name_.c_str());

			upload_ts_first_ = 0;
			upload_ts_last_ = 0;
			for (const auto &reading : readings_) {
				String text(static_cast<char*>(nullptr));
				std::vector<char> value(16);
				int len;

				text.reserve(64);

				len = snprintf_P(value.data(), value.size(), PSTR("&s=%u"), reading.timestamp);
				if (len < 0 || len >= (int)value.size()) {
					break;
				}

				text.concat(value.data());

				if (reading.temperature_c != Reading::TEMP_NAN) {
					len = snprintf_P(value.data(), value.size(), PSTR("&t=%d.%02u"),
							reading.temperature_c / Reading::TEMP_DIV,
							(std::abs(reading.temperature_c) % Reading::TEMP_DIV) * Reading::TEMP_MUL);
					if (len < 0 || len >= (int)value.size()) {
						break;
					}

					text.concat(value.data());
				} else {
					text.concat(F("&t="));
				}

				if (reading.relative_humidity_pc != Reading::RHUM_NAN) {
					len = snprintf_P(value.data(), value.size(), PSTR("&h=%u.%02u"),
							reading.relative_humidity_pc / Reading::RHUM_DIV,
							(reading.relative_humidity_pc % Reading::RHUM_DIV) * Reading::RHUM_MUL);
					if (len < 0 || len >= (int)value.size()) {
						break;
					}


					text.concat(value.data());
				} else {
					text.concat(F("&h="));
				}

				if (reading.co2_ppm != Reading::CO2_NAN) {
					len = snprintf_P(value.data(), value.size(), PSTR("&c=%u.%02u"),
							reading.co2_ppm / Reading::CO2_DIV,
							(reading.co2_ppm % Reading::CO2_DIV) * Reading::CO2_MUL);
					if (len < 0 || len >= (int)value.size()) {
						break;
					}

					text.concat(value.data());
				} else {
					text.concat(F("&c="));
				}

				if (count > 0 && payload.length() + text.length() > MAXIMUM_UPLOAD_BYTES) {
					break;
				}

				count++;
				if (upload_ts_first_ == 0) {
					upload_ts_first_ = reading.timestamp;
				}
				upload_ts_last_ = reading.timestamp;

				payload.concat(text);
			}

			if (upload_ts_first_ == 0) {
				logger_.err(F("Failed to encode any readings"));
				state_ = UploadState::IDLE;
			} else {
				logger_.debug(F("Uploading %lu readings from %u to %u (%u bytes)"),
					static_cast<unsigned long>(count), upload_ts_first_, upload_ts_last_, payload.length());
				http_client_.addHeader(F("Content-Type"), F("application/x-www-form-urlencoded"));

				int response = http_client_.POST(payload);
				if (response == 200) {
					logger_.trace(F("HTTP POST %u"), response);
					state_ = UploadState::RECEIVE;
				} else if (response >= 0) {
					logger_.err(F("Upload failure for %u to %u, received HTTP response code %d"),
						upload_ts_first_, upload_ts_last_, response);
					http_client_.end();
					state_ = UploadState::IDLE;
				} else {
					logger_.err(F("Upload failure for %u to %u: %s"),
						upload_ts_first_, upload_ts_last_,
						HTTPClient::errorToString(response).c_str());
					http_client_.end();
					state_ = UploadState::IDLE;
				}
			}
		}
		break;

	case UploadState::RECEIVE:
		if (http_client_.getString() == F("OK\n")) {
			logger_.trace(F("Upload successful"));
			state_ = UploadState::CLEANUP;
		} else {
			logger_.err(F("Upload failure for %u to %u, received unexpected response"),
				upload_ts_first_, upload_ts_last_);
			state_ = UploadState::IDLE;
		}
		http_client_.end();
		break;

	case UploadState::CLEANUP:
		size_t before = readings_.size();

		while (!readings_.empty() && readings_.front().timestamp <= upload_ts_last_) {
			readings_.pop_front();
		}

		logger_.trace(F("Removed %lu readings"), static_cast<unsigned long>(before - readings_.size()));
		state_ = UploadState::IDLE;
		break;
	}
}

void Report::loop() {
	if (readings_.empty()) {
		overflow_ = false;
	} else {
		upload();
	}
}

} // namespace scd30
