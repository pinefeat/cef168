// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2025 Pinefeat LLP

#include <linux/crc8.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/v4l2-controls.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>

#define CEF168_NAME "cef168"

#ifndef V4L2_CID_USER_CEF168_BASE
#define V4L2_CID_USER_CEF168_BASE	(V4L2_CID_USER_BASE | 168)
#endif

#define CEF168_V4L2_CID_CUSTOM(ctrl) \
	(V4L2_CID_USER_CEF168_BASE + custom_##ctrl)

enum { custom_lens_id, custom_data, custom_calibrate };

#define INP_CALIBRATE 0x22
#define INP_SET_FOCUS 0x80
#define INP_SET_FOCUS_P 0x81
#define INP_SET_FOCUS_N 0x82
#define INP_SET_APERTURE 0x7A
#define INP_SET_APERTURE_P 0x7B
#define INP_SET_APERTURE_N 0x7C

#define CEF_CRC8_POLYNOMIAL 168

DECLARE_CRC8_TABLE(cef168_crc8_table);

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

struct cef168_device {
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_subdev sd;
};

static inline struct cef168_device *to_cef168(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct cef168_device, ctrls);
}

static inline struct cef168_device *sd_to_cef168(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct cef168_device, sd);
}

static int cef168_i2c_write(struct cef168_device *cef168_dev, u8 cmd, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&cef168_dev->sd);
	int retry, ret;

	__le16 le_data = cpu_to_le16(val);
	char tx_data[4] = { cmd, ((u8 *)&le_data)[0], ((u8 *)&le_data)[1] };

	tx_data[3] = crc8(cef168_crc8_table, tx_data, 3, CRC8_INIT_VALUE);

	for (retry = 0; retry < 3; retry++) {
		ret = i2c_master_send(client, tx_data, sizeof(tx_data));
		if (ret == sizeof(tx_data))
			return 0;
		else if (ret != -EIO && ret != -EREMOTEIO)
			break;
	}

	dev_err(&client->dev, "I2C write fail after %d retries, ret=%d\n",
		retry, ret);
	return -EIO;
}

static int cef168_i2c_read(struct cef168_device *cef168_dev,
			   struct cef168_data *rx_data)
{
	struct i2c_client *client = v4l2_get_subdevdata(&cef168_dev->sd);

	int ret = i2c_master_recv(client, (char *)rx_data,
				  sizeof(struct cef168_data));
	if (ret != sizeof(struct cef168_data)) {
		dev_err(&client->dev, "I2C read fail, ret=%d\n", ret);
		return -EIO;
	}

	u8 computed_crc = crc8(cef168_crc8_table, (const u8 *)rx_data,
			       sizeof(struct cef168_data) - 1, CRC8_INIT_VALUE);
	if (computed_crc != rx_data->crc8) {
		dev_err(&client->dev,
			"CRC mismatch calculated=0x%02X read=0x%02X\n",
			computed_crc, rx_data->crc8);
		return -EIO;
	}

	rx_data->moving_time = le16_to_cpup((__le16 *)&rx_data->moving_time);
	rx_data->focus_position_min = le16_to_cpup((__le16 *)&rx_data->focus_position_min);
	rx_data->focus_position_max = le16_to_cpup((__le16 *)&rx_data->focus_position_max);
	rx_data->focus_position_cur = le16_to_cpup((__le16 *)&rx_data->focus_position_cur);
	rx_data->focus_distance_min = le16_to_cpup((__le16 *)&rx_data->focus_distance_min);
	rx_data->focus_distance_max = le16_to_cpup((__le16 *)&rx_data->focus_distance_max);

	return 0;
}

static int cef168_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct cef168_device *dev = to_cef168(ctrl);
	u8 cmd;

	switch (ctrl->id) {
	case V4L2_CID_FOCUS_ABSOLUTE:
		return cef168_i2c_write(dev, INP_SET_FOCUS, ctrl->val);
	case V4L2_CID_FOCUS_RELATIVE:
		cmd = ctrl->val < 0 ? INP_SET_FOCUS_N : INP_SET_FOCUS_P;
		return cef168_i2c_write(dev, cmd, abs(ctrl->val));
	case V4L2_CID_IRIS_ABSOLUTE:
		return cef168_i2c_write(dev, INP_SET_APERTURE, ctrl->val);
	case V4L2_CID_IRIS_RELATIVE:
		cmd = ctrl->val < 0 ? INP_SET_APERTURE_N : INP_SET_APERTURE_P;
		return cef168_i2c_write(dev, cmd, abs(ctrl->val));
	case CEF168_V4L2_CID_CUSTOM(calibrate):
		return cef168_i2c_write(dev, INP_CALIBRATE, 0);
	}

	return -EINVAL;
}

static int cef168_get_ctrl(struct v4l2_ctrl *ctrl)
{
	struct cef168_data data;
	struct cef168_device *dev = to_cef168(ctrl);
	int rval;

	rval = cef168_i2c_read(dev, &data);
	if (rval < 0)
		return rval;

	switch (ctrl->id) {
	case V4L2_CID_FOCUS_ABSOLUTE:
		__v4l2_ctrl_modify_range(ctrl,
					 data.focus_position_min,
					 data.focus_position_max, 1, 0);
		ctrl->val = data.focus_position_cur;
		return 0;
	case CEF168_V4L2_CID_CUSTOM(lens_id):
		ctrl->p_new.p_u8[0] = data.lens_id;
		return 0;
	case CEF168_V4L2_CID_CUSTOM(data):
		memcpy(ctrl->p_new.p_u8, &data, sizeof(data));
		return 0;
	}

	return -EINVAL;
}

static const struct v4l2_ctrl_ops cef168_ctrl_ops = {
	.g_volatile_ctrl = cef168_get_ctrl,
	.s_ctrl = cef168_set_ctrl,
};

static const struct v4l2_ctrl_config cef168_lens_id_ctrl = {
	.ops = &cef168_ctrl_ops,
	.id = CEF168_V4L2_CID_CUSTOM(lens_id),
	.type = V4L2_CTRL_TYPE_U8,
	.name = "Lens ID",
	.min = 0,
	.max = U8_MAX,
	.step = 1,
	.def = 0,
	.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY,
};

static const struct v4l2_ctrl_config cef168_data_ctrl = {
	.ops = &cef168_ctrl_ops,
	.id = CEF168_V4L2_CID_CUSTOM(data),
	.type = V4L2_CTRL_TYPE_U8,
	.name = "Data",
	.min = 0,
	.max = U8_MAX,
	.step = 1,
	.def = 0,
	.dims = { sizeof(struct cef168_data) / sizeof(u8) },
	.elem_size = sizeof(u8),
	.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY,
};

static const struct v4l2_ctrl_config cef168_calibrate_ctrl = {
	.ops = &cef168_ctrl_ops,
	.id = CEF168_V4L2_CID_CUSTOM(calibrate),
	.type = V4L2_CTRL_TYPE_BUTTON,
	.name = "Calibrate",
};

static const struct v4l2_subdev_core_ops cef168_core_ops = {
	.log_status = v4l2_ctrl_subdev_log_status,
	.subscribe_event = v4l2_ctrl_subdev_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_ops cef168_ops = {
	.core = &cef168_core_ops,
};

static int cef168_init_controls(struct cef168_device *dev)
{
	struct v4l2_ctrl *ctrl;
	struct v4l2_ctrl_handler *hdl = &dev->ctrls;
	const struct v4l2_ctrl_ops *ops = &cef168_ctrl_ops;

	v4l2_ctrl_handler_init(hdl, 7);

	ctrl = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FOCUS_ABSOLUTE, 0, S16_MAX,
				 1, 0);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			       V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_FOCUS_RELATIVE, S16_MIN, S16_MAX,
			  1, 0);
	ctrl = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_IRIS_ABSOLUTE, 0, S16_MAX,
				 1, 0);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_WRITE_ONLY |
			       V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_IRIS_RELATIVE, S16_MIN, S16_MAX, 1,
			  0);
	ctrl = v4l2_ctrl_new_custom(hdl, &cef168_calibrate_ctrl, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_WRITE_ONLY |
			       V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;
	v4l2_ctrl_new_custom(hdl, &cef168_data_ctrl, NULL);
	v4l2_ctrl_new_custom(hdl, &cef168_lens_id_ctrl, NULL);

	if (hdl->error)
		dev_err(dev->sd.dev, "%s fail error: 0x%x\n", __func__,
			hdl->error);
	dev->sd.ctrl_handler = hdl;
	return hdl->error;
}

static int cef168_probe(struct i2c_client *client)
{
	struct cef168_device *cef168_dev;
	int rval;

	cef168_dev = devm_kzalloc(&client->dev, sizeof(*cef168_dev),
				  GFP_KERNEL);
	if (!cef168_dev)
		return -ENOMEM;

	v4l2_i2c_subdev_init(&cef168_dev->sd, client, &cef168_ops);
	cef168_dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
				V4L2_SUBDEV_FL_HAS_EVENTS;

	rval = cef168_init_controls(cef168_dev);
	if (rval)
		goto err_cleanup;

	rval = media_entity_pads_init(&cef168_dev->sd.entity, 0, NULL);
	if (rval < 0)
		goto err_cleanup;

	cef168_dev->sd.entity.function = MEDIA_ENT_F_LENS;

	rval = v4l2_async_register_subdev(&cef168_dev->sd);
	if (rval < 0)
		goto err_cleanup;

	crc8_populate_msb(cef168_crc8_table, CEF_CRC8_POLYNOMIAL);

	return 0;

err_cleanup:
	v4l2_ctrl_handler_free(&cef168_dev->ctrls);
	media_entity_cleanup(&cef168_dev->sd.entity);

	return rval;
}

static void cef168_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct cef168_device *cef168_dev = sd_to_cef168(sd);

	v4l2_async_unregister_subdev(&cef168_dev->sd);
	v4l2_ctrl_handler_free(&cef168_dev->ctrls);
	media_entity_cleanup(&cef168_dev->sd.entity);
}

static const struct of_device_id cef168_of_table[] = {
	{ .compatible = "pinefeat,cef168" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, cef168_of_table);

static struct i2c_driver cef168_i2c_driver = {
	.driver = {
		.name = CEF168_NAME,
		.of_match_table = cef168_of_table,
	},
	.probe = cef168_probe,
	.remove = cef168_remove,
};

module_i2c_driver(cef168_i2c_driver);

MODULE_AUTHOR("support@pinefeat.co.uk>");
MODULE_DESCRIPTION("CEF168 lens driver");
MODULE_LICENSE("GPL");
