#ifndef CEF168_CEF168_H
#define CEF168_CEF168_H

#define CEF168_V4L2_CID_CUSTOM(ctrl) \
	((V4L2_CID_USER_BASE | 168) + custom_##ctrl)

enum { custom_lens_id, custom_data, custom_calibrate };

struct cef168_data {
	__u8 lens_id;
	__u8 moving : 1;
	__u8 calibrating : 2;
	__u16 moving_time;
	__u16 focus_position_min;
	__u16 focus_position_max;
	__u16 focus_position_cur;
	__u16 focus_distance_min;
	__u16 focus_distance_max;
	__u8 crc8;
} __packed;

#define INP_CALIBRATE 0x22
#define INP_SET_FOCUS 0x80
#define INP_SET_FOCUS_P 0x81
#define INP_SET_FOCUS_N 0x82
#define INP_SET_APERTURE 0x7A
#define INP_SET_APERTURE_P 0x7B
#define INP_SET_APERTURE_N 0x7C

#define CEF_CRC8_POLYNOMIAL 168

#endif //CEF168_CEF168_H
