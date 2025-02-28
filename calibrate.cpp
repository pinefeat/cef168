#include <iostream>
#include <iomanip>
#include <sstream>
#include <fcntl.h>
#include <errno.h>
#include <map>
#include <set>
#include <string.h>
#include <algorithm>
#include <unistd.h>
#include <chrono>
#include <sys/ioctl.h>
#include <linux/v4l2-subdev.h>
#include <linux/i2c-dev.h>
#include <asm/byteorder.h>
#include "cef168.h"

class IDevice {
    protected:
	int fd;
	const char *dev;

    public:
	IDevice(const char *dev)
		: dev(dev)
	{
		fd = open(dev, O_RDWR);
		if (fd < 0) {
			std::ostringstream oss;
			oss << "Error opening device \"" << dev
			    << "\": " << strerror(errno);
			throw std::runtime_error(oss.str());
		}
	}
	virtual ~IDevice()
	{
		if (fd >= 0) {
			close(fd);
		}
	}
	virtual void calibrate() = 0;
	virtual void getData(struct cef168_data &data) = 0;
};

class V4L2SubDev : public IDevice {
    public:
	V4L2SubDev(const char *subdev)
		: IDevice(subdev)
	{
	}

	void calibrate() override
	{
		struct v4l2_control calibrate_ctrl = {
			.id = CEF168_V4L2_CID_CUSTOM(calibrate),
			.value = 0,
		};

		if (ioctl(fd, VIDIOC_S_CTRL, &calibrate_ctrl) < 0) {
			std::ostringstream oss;
			oss << "Failed to set control " << std::hex
			    << calibrate_ctrl.id << " value for device \""
			    << dev << "\": " << strerror(errno);
			throw std::runtime_error(oss.str());
		}
	}

	void getData(struct cef168_data &data) override
	{
		struct v4l2_ext_control ext_ctrl = {
			.id = CEF168_V4L2_CID_CUSTOM(data),
			.size = sizeof(struct cef168_data),
			.reserved2 = { 0 },
			.ptr = &data,
		};

		struct v4l2_ext_controls data_ctrls = {
			.ctrl_class = V4L2_CTRL_CLASS_USER,
			.count = 1,
			.error_idx = 0,
			.request_fd = 0,
			.reserved = { 0 },
			.controls = &ext_ctrl,
		};

		if (ioctl(fd, VIDIOC_G_EXT_CTRLS, &data_ctrls) < 0) {
			std::ostringstream oss;
			oss << "Failed to get control " << std::hex
			    << ext_ctrl.id << " value for device \"" << dev
			    << "\": " << strerror(errno);
			throw std::runtime_error(oss.str());
		}
	}
};

class I2CDev : public IDevice {
    private:
	const int addr;

    public:
	I2CDev(const char *dev, const int addr)
		: IDevice(dev)
		, addr(addr)
	{
		if (ioctl(fd, I2C_SLAVE, addr) < 0) {
			std::ostringstream oss;
			oss << "Failed to set I2C address 0x" << std::hex
			    << addr << " for device \"" << dev
			    << "\": " << strerror(errno);
			throw std::runtime_error(oss.str());
		}
	}

	void calibrate() override
	{
		const __u8 tx_data[4] = { INP_CALIBRATE, 0, 0, 0x30 };
		if (write(fd, &tx_data, sizeof(tx_data)) != sizeof(tx_data)) {
			std::ostringstream oss;
			oss << "Failed to write to I2C device \"" << dev << "@"
			    << std::hex << addr << "\": " << strerror(errno);
			throw std::runtime_error(oss.str());
		}
	}

	void getData(struct cef168_data &rx_data) override
	{
		if (read(fd, &rx_data, sizeof(struct cef168_data)) !=
		    sizeof(struct cef168_data)) {
			std::ostringstream oss;
			oss << "Failed to read from I2C device \"" << dev << "@"
			    << std::hex << addr << "\": " << strerror(errno);
			throw std::runtime_error(oss.str());
		}
		__u8 computed_crc = crc8_msb((const uint8_t *)&rx_data,
					     sizeof(struct cef168_data) - 1);
		if (computed_crc != rx_data.crc8) {
			std::ostringstream oss;
			oss << "CRC mismatch, computed=0x" << std::setfill('0')
			    << std::setw(2) << std::hex << (int)computed_crc
			    << ", read=0x" << (int)rx_data.crc8;
			throw std::runtime_error(oss.str());
		}

		rx_data.moving_time = __le16_to_cpu(rx_data.moving_time);
		rx_data.focus_position_min =
			__le16_to_cpu(rx_data.focus_position_min);
		rx_data.focus_position_max =
			__le16_to_cpu(rx_data.focus_position_max);
		rx_data.focus_position_cur =
			__le16_to_cpu(rx_data.focus_position_cur);
		rx_data.focus_distance_min =
			__le16_to_cpu(rx_data.focus_distance_min);
		rx_data.focus_distance_max =
			__le16_to_cpu(rx_data.focus_distance_max);
	}

	uint8_t crc8_msb(const uint8_t *data, size_t len, uint8_t crc = 0xFF)
	{
		for (size_t i = 0; i < len; i++) {
			crc ^= data[i];
			for (uint8_t j = 0; j < 8; j++) {
				if (crc & 0x80)
					crc = (crc << 1) ^ CEF_CRC8_POLYNOMIAL;
				else
					crc <<= 1;
			}
		}
		return crc;
	}
};

struct PwlPoint {
	double distance;
	int position;

	bool operator<(const PwlPoint &other) const
	{
		return distance < other.distance;
	}
};

void update(std::set<PwlPoint> &s, PwlPoint n)
{
	auto p = s.insert(n);
	if (p.second) {
		return;
	}
	auto o = (*p.first);
	if (o.position <= n.position) {
		return;
	}
	auto hint = p.first;
	hint++;
	s.erase(p.first);
	s.insert(hint, n);
}

std::string toLower(const std::string &str)
{
	std::string lowerStr = str;
	std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(),
		       ::tolower);
	return lowerStr;
}

std::map<std::string, std::string>
parseArgs(int argc, char *argv[],
	  const std::map<std::string, std::string> &defaults)
{
	std::map<std::string, std::string> args = defaults;

	std::map<std::string, std::string> shortToLong = { { "d", "device" },
							   { "a", "address" } };

	for (int i = 1; i < argc; ++i) {
		std::string argvi = argv[i];
		std::string arg = toLower(argvi);
		std::string key;
		std::string value;

		if (arg == "-h" || arg == "--help") {
			std::cout
				<< "Usage: " << argv[0] << std::endl
				<< "  -h, --help		display this help message"
				<< std::endl
				<< "  -d, --device <dev>	device to use: /dev/v4l-subdev1, /dev/i2c-1, etc."
				<< std::endl
				<< "  -a, --address <addr>	I2C device address in hex form, default "
				<< defaults.at("address") << std::endl
				<< "  -v, --verbose		prints data as it is read from the device"
				<< std::endl;
			exit(EXIT_SUCCESS);
		} else if (arg == "-v" || arg == "--verbose") {
			args["verbose"] = "true";
		} else if (arg.rfind("--", 0) == 0) {
			size_t equalPos = arg.find('=');
			if (equalPos != std::string::npos) {
				key = arg.substr(2, equalPos - 2);
				value = argvi.substr(equalPos + 1);
				args[key] = value;
			} else if (i + 1 < argc) { // Support --key value format
				key = arg.substr(2);
				value = argv[i + 1];
				args[key] = value;
				++i;
			}
		} else if (arg.rfind("-", 0) == 0 && arg.size() == 2) {
			std::string shortKey = arg.substr(1);
			if (shortToLong.find(shortKey) != shortToLong.end() &&
			    i + 1 < argc) {
				key = shortToLong[shortKey];
				value = argv[i + 1];
				args[key] = value;
				++i;
			}
		}
	}
	return args;
}

int parseAddr(std::string hexStr)
{
	try {
		if (toLower(hexStr).rfind("0x", 0) == 0) {
			hexStr = hexStr.substr(2);
		}
		return std::stoi(hexStr, nullptr, 16);
	} catch (const std::exception &) {
		std::ostringstream oss;
		oss << "Invalid input: " << hexStr;
		throw std::runtime_error(oss.str());
	}
}

int main(int argc, char *argv[])
{
	using namespace std::chrono;

	std::map<std::string, std::string> args =
		parseArgs(argc, argv,
			  {
				  { "device", "/dev/v4l-subdev1" },
				  {
					  "address",
					  "0x0d",
				  },
			  });
	bool verbose = args.find("verbose") != args.end();

	try {
		std::set<PwlPoint> points;
		IDevice *device;

		std::string dev = args["device"];
		if (toLower(dev).find("i2c") != std::string::npos) {
			device = new I2CDev(dev.c_str(),
					    parseAddr(args["address"]));
		} else {
			device = new V4L2SubDev(dev.c_str());
		}

		device->calibrate();

		cef168_data data;
		auto start = high_resolution_clock::now();
		do {
			usleep(500);
			device->getData(data);
			if (data.calibrating == 2 &&
			    data.focus_distance_min != 0) {
				update(points,
				       {
					       100 / (double)data
							       .focus_distance_min,
					       data.focus_position_cur,
				       });
			}
			if (!verbose) {
				continue;
			}
			printf("Focus: %5d, time: %4d ms, distance: %6.2f - %6.2f m, status: %d/%d\n",
			       data.focus_position_cur, data.moving_time,
			       (double)data.focus_distance_min / 100,
			       (double)data.focus_distance_max / 100,
			       data.moving, data.calibrating);
		} while (data.calibrating != 0 ||
			 duration_cast<milliseconds>(
				 high_resolution_clock::now() - start)
					 .count() < 100);

		delete device;

		if (points.empty()) {
			throw std::runtime_error("No PWL points for output");
		}

		auto it = points.begin();
		auto last = --points.end();

		std::cout
			<< "------------------------------------------------------------"
			<< std::endl;
		std::cout
			<< "Parameters for \"rpi.af\" section of camera tuning JSON file"
			<< std::endl;
		std::cout
			<< "------------------------------------------------------------"
			<< std::endl;
		std::cout << std::setprecision(3) << std::defaultfloat;
		std::cout << "Inverse of focus distance, m⁻¹:" << std::endl;
		std::cout << "\t\"min\": " << (*it).distance << ","
			  << std::endl;
		std::cout << "\t\"max\": " << (*last).distance << ","
			  << std::endl;
		std::cout << "\t\"default\": " << (*last).distance << ","
			  << std::endl;
		std::cout << "PWL function:" << std::endl;
		std::cout << "\t\"map\": [ ";
		for (; it != points.end(); ++it) {
			const PwlPoint &point = *it;
			std::cout << point.distance << ", " << point.position;
			if (it != last) {
				std::cout << ", ";
			}
		}
		std::cout << " ]" << std::endl;

	} catch (const std::exception &e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
