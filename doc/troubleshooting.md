# 🛠 Troubleshooting Guide

A list of common problems and their solutions.

## 1: Unusual LED Activity on First Boot

If, after the assembling of the unit, the Raspberry Pi LEDs show unusual activity (such as blinking or staying off completely), it likely indicates a **short circuit** caused by incorrect orientation of one of the flex cables. There are two cables to consider: one connects the controller to the camera module, and the other connects the camera module to the Raspberry Pi.

1. Disconnect the cables and check the assembly guide to ensure the correct orientation of the short flex cable. This cable must be inserted into the connector labeled **CAMERA** on the lens control board.
2. For the longer cable, make sure the metallic contacts are oriented the same way as when connecting the camera module directly (without the control board) to the Raspberry Pi. This orientation may vary depending on the Raspberry Pi version, so refer to the camera module manual for guidance.

## 2: Camera Is Not Detected After Assembly

If the camera was working before but is not detected after the unit assembly, the issue is likely caused by a poor connection of the flex cable.

1. Ensure all cables are properly aligned and fully inserted into their connectors.
2. Verify that you are using the correct camera port on the Raspberry Pi 5. If the cable is connected to the **CAM0** port, it must be specified in the config file. 

## 3. Camera Is Detected, but Not Lens Media Device

If the camera is detected but the lens media device is not discovered, there are several potential causes to check.

1. **Multiple Cameras Connected**: If multiple cameras are connected, use `v4l2-ctl --list-devices` to check which camera is being used. Look for _unicam_ or _rp1-cfe_ in the driver name. Additionally, use `media-ctl -d <camera-device> -p` to view the device topology and check if the lens node `cef168` is listed. When found, feed the lens device name into the calibration tool.
2. **Lens Node Present but Disabled**: If the lens node `cef168` appears in the device tree but is disabled, you are likely using a camera with the `imx219` or `ov5647` sensor. These sensors require the lens node (`vcm`) to be explicitly enabled in the overlay with `dtoverlay=imx219,vcm`.

## 4. Lens Does Not Focus at All

If the lens does not focus and the image remains static when running `rpicam-hello` with autofocus enabled, it is likely because the lens has not been calibrated and does not know its full focus range.

Check the range with the following command:

```shell
v4l2-ctl -d $DEV_LENS --all | awk '/focus_absolute/ {print $6;}'
```

A valid, calibrated lens will return a positive value (e.g., `max=1203`). If the result is 0 or 32767, the lens has not been calibrated.

Calibrate the lens as described [here](../readme.md#calibration). You only need to do this once per lens. After calibration, run the focus range check again to confirm proper setup.

## 5. No PWL points for output

If calibration utility returns _No PWL points for output_, first check whether the lens moves at all. If the lens doesn't move, it may indicate incompatibility with the adapter.

If it does move, add the `-v` option when running calibration to get verbose output. Pay attention to the reported distance values — they should vary as the focus position traverses from minimum to infinity. If you see a constant value on every line such as `0.00 - 496.64 m`, it indicates that the lens does not have a distance encoder. That’s okay; some lenses simply lack this feature. 

Since the Raspberry Pi autofocus algorithm operates in diopters, the calibration process converts distance in meters to diopters, which is simply the inverse of the distance. Without a distance encoder, it is not possible to generate PWL data, hence _No PWL points for output_.

As a workaround, you can prepare the mapping table manually.

Check the verbose output again and determine the focus position range. Let's say it goes from 0 to approximately 650. Reduce the maximum focus position to 630 to avoid the very end of the range near infinity.

Check the minimum focus distance printed on the lens body. Let's say it is 0.45 m.

Assuming the focusing distance ranges from 0.45 m to infinity, the maximum diopter value will be 1 / 0.45 = 2.22 and the minimum diopter value is always 0 (infinity). Based on this, the simplest mapping table and autofocus settings would look like this:

```text
------------------------------------------------------------
Parameters for "rpi.af" section of camera tuning JSON file
------------------------------------------------------------
Inverse of focus distance, m⁻¹:
    "min": 0.0,
    "max": 2.22,
    "default": 2.2,
Speed:
    step_frames: 4
PWL function:
    "map": [ 0.0, 630, 2.22, 0 ]
```

From the verbose output, determine the maximum time reported during calibration. If it exceeds 350 ms, bump `step_frames` parameter up to allow the lens more time to settle between focusing steps.

You can use the above to prepare the camera tuning file.

## 6. The Search Overshoots the Focus

If the coarse search frequently overshoots the focus, regardless of the settings, it may be due to the lag of the lens motor.

For lenses with a DC (direct current) motor, increase the `step_frames=6` parameter in the camera tuning file to allow the lens more time to settle between focusing steps.

## 7. Autofocus Not Working After Raspberry Pi Full Upgrade

When the Raspberry Pi is updated, the firmware, Linux kernel, and all installed packages — including device overlay files — are upgraded to their latest versions. These overlay files define which lens driver should be loaded.

However, any changes made to these files during the driver [installation](../readme.md#installation) are not part of the official Raspberry Pi upstream repository. As a result, the overlay files are overwritten and reverted to their original versions during the update.

#### Steps to Fix the Issue:

1. **Reboot** the Raspberry Pi to load the new kernel after the update.
2. **Rebuild the driver** by running the following in the cloned repository directory:
    ```shell
    make
    sudo make install
    ```
3. **Reboot again** to apply the changes.
