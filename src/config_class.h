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

#define MCU_APP_CONFIG_DATA \
        MCU_APP_CONFIG_PRIMITIVE(bool, "", sensor_automatic_calibration, "", false) \
        MCU_APP_CONFIG_PRIMITIVE(unsigned long, "", sensor_temperature_offset, "", 0) \
        MCU_APP_CONFIG_PRIMITIVE(unsigned long, "", sensor_altitude_compensation, "", 0) \
        MCU_APP_CONFIG_PRIMITIVE(unsigned long, "", sensor_measurement_interval, "", 2) \
        MCU_APP_CONFIG_PRIMITIVE(unsigned long, "", sensor_ambient_pressure, "", 0) \
        MCU_APP_CONFIG_PRIMITIVE(unsigned long, "", take_measurement_interval, "", 5) \
        MCU_APP_CONFIG_PRIMITIVE(bool, "", report_enabled, "", true) \
        MCU_APP_CONFIG_PRIMITIVE(unsigned long, "", report_threshold, "", 12) \
        MCU_APP_CONFIG_SIMPLE(std::string, "", report_url, "", "") \
        MCU_APP_CONFIG_SIMPLE(std::string, "", report_username, "", "") \
        MCU_APP_CONFIG_SIMPLE(std::string, "", report_password, "", "") \
        MCU_APP_CONFIG_SIMPLE(std::string, "", report_sensor_name, "", "")

public:
    bool sensor_automatic_calibration() const;
    void sensor_automatic_calibration(bool sensor_automatic_calibration);

    unsigned long sensor_temperature_offset() const;
    void sensor_temperature_offset(unsigned long sensor_temperature_offset);

    unsigned long sensor_altitude_compensation() const;
    void sensor_altitude_compensation(unsigned long sensor_altitude_compensation);

    unsigned long sensor_measurement_interval() const;
    void sensor_measurement_interval(unsigned long sensor_measurement_interval);

    unsigned long sensor_ambient_pressure() const;
    void sensor_ambient_pressure(unsigned long sensor_ambient_pressure);

    unsigned long take_measurement_interval() const;
    void take_measurement_interval(unsigned long take_measurement_interval);

    bool report_enabled() const;
    void report_enabled(bool report_enabled);

    unsigned long report_threshold() const;
    void report_threshold(unsigned long report_threshold);

    std::string report_url() const;
    void report_url(const std::string &report_url);

    std::string report_username() const;
    void report_username(const std::string &report_username);

    std::string report_password() const;
    void report_password(const std::string &report_password);

    std::string report_sensor_name() const;
    void report_sensor_name(const std::string &report_sensor_name);

private:
	static bool sensor_automatic_calibration_;
	static unsigned long sensor_temperature_offset_;
	static unsigned long sensor_altitude_compensation_;
	static unsigned long sensor_measurement_interval_;
	static unsigned long sensor_ambient_pressure_;
	static unsigned long take_measurement_interval_;
	static bool report_enabled_;
	static unsigned long report_threshold_;
	static std::string report_url_;
	static std::string report_username_;
	static std::string report_password_;
	static std::string report_sensor_name_;
