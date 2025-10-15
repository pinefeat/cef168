# EF & EF-S lens controller for Raspberry Pi® Camera – Canon® Lens Compatible

[Pinefeat](https://www.pinefeat.co.uk) produces the adapter designed to interface between [Canon EF](https://www.canon.co.uk/store/ef-lenses/) & [EF-S lenses](https://www.canon.co.uk/store/ef-s-lenses/) and non-Canon camera bodies, incorporating features for electronic focus and aperture adjustments. Circuit board _cef168_, that comes with the adapter, provides software programming interface allowing control of the lens focus and aperture positions.

This repository contains kernel driver, configuration and calibration tools that allow you to use the adapter with [Raspberry Pi Camera](https://www.raspberrypi.com/documentation/accessories/camera.html).

The solution allows you to mount **Canon EF** or **EF-S lens** onto the [Raspberry Pi High Quality Camera](https://www.raspberrypi.com/products/raspberry-pi-high-quality-camera/) and have features like [autofocus](https://en.wikipedia.org/wiki/Autofocus) and [aperture](https://en.wikipedia.org/wiki/Aperture) control available in [rpicam-apps](https://www.raspberrypi.com/documentation/computers/camera_software.html#rpicam-apps), _libcamera_ or V4L2 (Video4Linux2) API.

![Assembly of Pinefeat Lens Controller and Raspberry Pi High-Quality Camera](https://docs.pinefeat.co.uk/cef168-assembly-rpi-hq-camera.jpg)

Raspberry Pi cameras use [libcamera](https://www.raspberrypi.com/documentation/computers/camera_software.html#libcamera) as their primary driver and API to interact with the Linux system. This lens controller driver integrates into libcamera stack, communicating with the lens hardware through the I2C bus.

## Table of contents
* [Disclaimer](#disclaimer)
* [Assembly](#assembly)
* [Quick test](#quick-test)
* [Installation](#installation)
* [Calibration](#calibration)
* [Tuning](#tuning)
* [Autofocus](#autofocus)
* [Aperture Control](#aperture-control)
* [Serial Interface](#serial-interface)
* [Helpful Hints](#helpful-hints)
* [Troubleshooting](#troubleshooting)

## Disclaimer 

The adapter has electronic communication capabilities obtained by reverse engineering Canon’s communication protocol. While we have extensively tested the adapter with a variety of Canon lenses, compatibility cannot be guaranteed with all lens models, particularly given the numerous versions manufactured since 1987. 

Due to potential firmware differences, electronic variations, and undiscovered incompatibilities, some lenses may not function as expected, resulting in limitations in autofocus or aperture control.

This product is provided as is, without any express or implied warranties regarding its compatibility with specific lenses. Pinefeat is not liable for any damage, malfunctions, or performance issues arising from its use.

## Assembly

Theoretically, this solution supports any Raspberry Pi-compatible camera with a C/CS or M12 mount. In practice, compatibility depends on:
 - CSI Connector Matching – The control board must have the same CSI connector as the camera module.
 - Physical Alignment – The camera and control board must have compatible sizes and matching mount holes.
 - Back Focus Length – The lens mount must provide the correct back focus distance for the Canon lens.

For tested and **ready-to-use** assemblies, refer to the PDF guides below. These include step-by-step instructions for attaching the lens controller, securing the camera module, and making electrical connections.

 - [Raspberry Pi High Quality Camera (CS Mount variant)](https://docs.pinefeat.co.uk/cef168-assembly-rpi-hq-camera.pdf)
 - [Arducam IMX708 Camera Module (M12 Mount variant)](https://docs.pinefeat.co.uk/cef168-assembly-arducam-imx708.pdf)

![Assembly of Pinefeat Lens Controller and Arducam IMX708 Camera Module](https://docs.pinefeat.co.uk/cef168-assembly-arducam-imx708.jpg)

Ensure you follow the assembly guide carefully before proceeding with the setup and usage instructions in this manual.

## Quick test

The adapter features a **Self-Test Mode** to verify focus and aperture communication with the lens.

To activate Self-Test Mode, ensure the Raspberry Pi is powered on and supplies power to the adapter, then toggle the **AF/MF switch** on the lens **three times within 15 seconds**. The control board will then run a test sequence:
- _Focus Test_: moves focus from **minimum** to **infinity**, then back to **minimum** in **four steps**.
- _Aperture Test_: **closes** the aperture fully, then **reopens** it gradually in **four steps** to the maximum aperture.

[![How to use the Self-Test Mode](https://docs.pinefeat.co.uk/cef-self-test-demo.jpg)](https://youtu.be/-aLFMjMSr5M)

This sequence lasts for a few seconds and confirms that the control board is communicating correctly with the lens.

Keep in mind that STM lenses are designed to be very quiet, and their focus movements are often smooth and hard to notice. If you're unsure whether the motor is working, try placing your ear close to the lens to listen for engagement.

If the lens does not complete any part of this sequence and the issue persists after repeated attempts, it indicates that the lens and adapter are not fully compatible. In such cases, some or all electronic functions (focus and/or aperture control) may not operate reliably.

Note: Self-Test Mode does not affect any camera or lens settings and can be safely repeated at any time.    

## Installation

Make sure you've installed Raspberry PI OS **64-bit** version. Do not install the 32-bit version, as it cannot build kernel drivers. The project is compatible only with the Raspberry Pi OS Bookworm version and does not support the legacy camera stack available in Bullseye and earlier Raspberry Pi OS releases.

1\. Clone the repository and change the current directory:

```shell
git clone https://github.com/pinefeat/cef168.git
cd cef168
```

2\. Run the following script and select the image sensor of your camera (**imx477** for HQ Camera):

```shell
./configure.sh
```

The script will download the device tree overlay files for your camera from the Raspberry Pi Git repository, corresponding to your Raspberry Pi firmware version. It will then adjust the overlays to add the lens node. The lens is controlled via the I2C bus, and the kernel needs to load the lens driver. Make sure the script output confirms that the `All overlays files were updated successfully`.

3\. Compile and install the kernel driver and the overlay:

```shell
make
sudo make install
```

The compiled overlay file is copied to `/boot/firmware/overlays`, with a backup created first (e.g., `imx477.dtbo.~1~`). If you need to revert the change, simply restore the backup file.

You will need to repeat this operation if you upgrade your Raspberry Pi firmware, as the overlay files are overwritten during the upgrade.

4\. If you are using a camera other than the native Raspberry Pi High Quality Camera, you may need to disable automatic camera detection by setting `camera_auto_detect=0` in `/boot/firmware/config.txt`, then add one of the overlays:

<details>
<summary>Expand overlays table</summary>

| Camera sensor | Line in `/boot/firmware/config.txt` |
|---------------|-------------------------------------|
| imx219        | `dtoverlay=imx219,vcm`              |
| imx296        | `dtoverlay=imx296`                  |
| imx477        | `dtoverlay=imx477`                  |
| imx519        | `dtoverlay=imx519`                  |
| imx708        | `dtoverlay=imx708`                  |
| ov5647        | `dtoverlay=ov5647,vcm`              |
| ov9281        | `dtoverlay=ov9281`                  |

</details>

If you want to enable the camera with the lens on the `cam0` port of Raspberry Pi 5, please make the following modification: `dtoverlay=imx477,cam0`.

5\. Reboot Raspberry Pi.

## Calibration

The autofocus algorithm implemented in libcamera is responsible for controlling the lens driver. It requires knowledge of the minimum and maximum possible lens positions, as well as the piecewise linear (PWL) function that relates inverse distance to the hardware lens setting. These parameters depend on the specific lens, which is why calibration is required.

Many Canon lenses are zoom lenses, meaning their focal length can be adjusted. For example, a 10–18mm zoom lens allows you to go from a wide-angle view (10mm) to a more standard view (18mm). The parameters required by the algorithm may vary when the lens's focal length changes, so calibration may need to be repeated after such adjustments.

The calibration procedure produces the parameters that must be **included** in the camera **tuning file**.

To detect the lens V4L2 sub-device, set the following environment variables in your terminal or add them to your `~/.bashrc` or `~/.profile` file:

```shell
export DEV_MEDIA=$(v4l2-ctl --list-devices | awk '/unicam|rp1-cfe/ {found=1} found && /\/dev\/media/ {print; exit;}')
export DEV_LENS=$(media-ctl -d $DEV_MEDIA -p | awk '/entity.*cef168.*-000d/ {found=1} found && /\/dev\/v4l-subdev/ {print $4; exit;}')
```

Then reboot or run `source ~/.bashrc` (or `source ~/.profile`) to apply the changes.

Now perform the calibration:

```shell
./calibrate -d $DEV_LENS
```

The program will move the lens focus from minimum to infinity capturing focus position and focus distance at several points. The output should look as follows: 

```text
------------------------------------------------------------
Parameters for "rpi.af" section of camera tuning JSON file
------------------------------------------------------------
Inverse of focus distance, m⁻¹:
    "min": 0.00153,
    "max": 4.35,
    "default": 4.35,
Speed:
    step_frames: 4
PWL function:
    "map": [ 0.00153, 1069, 0.154, 1042, 0.446, 996, 0.719, 969, 0.971, 941, 1.2, 901, 1.45, 860, 1.72, 805, 2.04, 764, 2.44, 682, 2.86, 559, 3.45, 388, 3.85, 252, 4.35, 0 ]
```

The parameters `max`, `min`, `default`, `step_frames` and `map` need to be added to the camera tuning file, as described in the next section.

## Tuning

The autofocus algorithm for a custom lens must be tuned via a JSON configuration file, which defines parameters such as focus speed, search strategy, and optimization for different scenes. Refer to the [Raspberry Pi Camera Algorithm and
Tuning Guide](https://datasheets.raspberrypi.com/camera/raspberry-pi-camera-guide.pdf), chapter 5 Raspberry Pi Control Algorithms, subchapter 5.14 Auto Focus.

Raspberry Pi’s libcamera implementation includes a tuning file in JSON format for each camera sensor, located at:
 - Raspberry Pi 5: `/usr/share/libcamera/ipa/rpi/pisp/`
 - Raspberry Pi 4: `/usr/share/libcamera/ipa/rpi/vc4/`

Create a copy of the tuning file that corresponds to your camera sensor and place it somewhere convenient for editing. You can also modify existing tuning files to customize camera behavior.

The JSON file lists camera algorithms by name, where `rpi.af` represents the algorithm to control a lens driver for Auto Focus. The autofocus algorithm section in the original tuning file is available only for cameras with an associated lens driver, such as Camera Module 3 (`imx708.json`). 

For High Quality Camera (`imx477.json`) the `rpi.af` section is absent in the file, so it needs to be added. Open the copied tuning file and navigate to the `rpi.sharpen` section almost at the end of the file. From the example below copy the `rpi.af` section and place it between `rpi.sharpen` and `rpi.hdr` blocks. 

<details>
<summary>Example of "rpi.af"</summary>

```json
{
    "rpi.af":
    {
        "ranges":
        {
            "normal":
            {
                "min": 0.00153,
                "max": 4.55,
                "default": 4.55
            }
        },
        "speeds":
        {
            "normal":
            {
                "step_coarse": 0.2,
                "step_fine": 0.05,
                "contrast_ratio": 0.75,
                "pdaf_gain": 0.0,
                "pdaf_squelch": 0.0,
                "max_slew": 2.0,
                "pdaf_frames": 0,
                "dropout_frames": 0,
                "step_frames": 4
            }
        },
        "conf_epsilon": 0,
        "conf_thresh": 0,
        "conf_clip": 0,
        "skip_frames": 5,
        "map": [ 0.00153, 358, 0.108, 347, 0.383, 333, 1.35, 319, 1.54, 296, 2.08, 266, 2.78, 235, 3.03, 224, 3.45, 192, 3.7, 171, 4, 141, 4.35, 128, 4.55, 7 ]
    }
},
```

</details>
 
**Replace** the existing parameters in the `rpi.af` section with the ones **generated by the calibration** tool. Be sure to remove any outdated values and only keep the new parameters.

For simplicity, retain only the `normal` range and speed, removing the `macro` and `full` settings if they are present.

## Autofocus

Ensure that you have **completed** the one-time **calibration** for the lens and prepared the **tuning** file as described above.

Now you can run any `rpicam` app and let libcamera adjust lens focus automatically. Use the `tuning-file` option to specify an override, if you haven't altered the original tuning file.

```shell
rpicam-hello --tuning-file imx477-EF-S10-18mm.json --timeout 0
```

To have move verbose output from the autofocus algorithm you can set the following environment variable `LIBCAMERA_LOG_LEVELS=RPiAf:DEBUG`.

[![Watch the demo video](https://docs.pinefeat.co.uk/cef168-product-demo.jpg)](https://youtu.be/oHfA0CyQ0Xc)

## Aperture Control

The lens aperture can be controlled via the V4L2 API. The driver provides two controls: `iris_absolute` and `iris_relative`. Canon lenses do not report the current aperture, so these controls are write-only. If the lens aperture range is from f/5.6 to f/22.6, you can set the minimum aperture by multiplying 22.6 by 100, resulting in an integer value of `2260`. Passing this value to the control will close the shutter as much as possible. To fully open the shutter, pass the value `560` (which is 5.6 multiplied by 100). Any value in between will partially open the shutter.

```shell
v4l2-ctl -d $DEV_LENS -c iris_absolute=2260
```

`DEV_LENS` variable was declared in [Calibration](#calibration) section of this document.

This control  `iris_relative` modifies the len’s aperture by the specified amount. Positive values close the iris further, negative values open it further. The minimum step is f/1 or 100 as a control value:

```shell
v4l2-ctl -d $DEV_LENS -c iris_relative=+100
v4l2-ctl -d $DEV_LENS -c iris_relative=-100
```

In zoom lenses, the aperture range varies as the focal length changes. The controller will check the possible maximum and minimum aperture values before sending the value to the lens. If the values are out of range, the lens will not engage. The relative control value will take effect if the absolute one has been set at least once after the focal length has been changed.

## Serial Interface

A serial interface is not required for autofocus to operate. However, it can be used to enable third-party software integrations, such as with astronomical imaging platforms like INDI. In this setup, the INDI [driver](https://github.com/indilib/indi/blob/master/drivers/focuser/doc/pinefeat_cef/index.md)
communicates with the lens via the serial connection, which requires the board to be connected through a serial cable. Please note that in this configuration, installing the kernel driver described above is not necessary, since the board is controlled through the serial interface rather than I2C.

The control board serial protocol is described [here](doc/serial.md).

## Helpful Hints

This section provides a collection of useful tips and recommendations to help you get the most out of this project.

### Image Sensor Size

Canon EF lenses are designed for full-frame sensors with a diagonal of 43.3mm. Canon EF-S lenses are specifically made for smaller APS-C sensors, which have a diagonal of 26.8mm. Raspberry Pi cameras have even smaller sensors, with the largest being 7.9mm in the High Quality (HQ) Camera.

This difference in sensor size creates a crop factor because a smaller sensor captures only the central portion of the image projected by the lens. This effectively narrows the field of view, making the image appear more zoomed in.

For example, if you mount a 10mm EF-S lens on the HQ Camera, the effective focal length becomes `10 x 26.8 / 7.9 = 34 mm`. So, even though it’s a 10mm wide-angle lens, it behaves like a 34mm telephoto lens in terms of framing.

### Lens Selection

For compact and efficient setup EF-S lenses are the better choice since they are designed for smaller sensors and may handle the crop factor more effectively.

For maximum zoom (such as for aerial imaging) EF lenses could be useful as they will result in an extreme telephoto effect (a 200mm EF lens becomes 1100mm with HQ Camera).

Canon lenses use different types of motors for autofocus:
- DC (direct current) motors are simple and reliable, but tend to be slower and noisier compared to other motor types. If you use this type of lens, you may need to bump up `step_frames` parameter in the camera tuning file to allow the lens to settle between focusing steps.
- USM (Ultrasonic Motor) lenses are faster, quieter, and more precise. They offer better autofocus performance, especially in high-end lenses.
- STM (Stepping Motor) lenses are the quietest and smoothest. They provide fast, almost silent autofocus, making them the best choice for this project.

### Autofocus Methods

Contrast Detection Autofocus (CDAF) is the primary method used in Raspberry Pi cameras for autofocus. It works by measuring image contrast at different focus positions to determine the sharpest focus. Since it relies on detecting contrast, the algorithm may struggle in low-light conditions where contrast is reduced. Adequate lighting is essential to improve autofocus accuracy, minimize focus hunting, and ensure more reliable performance.

Phase Detection Autofocus (PDAF) used in some Raspberry Pi cameras, such as imx708, is optimized for specific sensor-lens combinations where phase detection pixels are aligned for accurate focus. When using Canon lenses, misalignment and calibration issues prevent the system from accurately determining focus shifts. We recommend disabling PDAF if is available by zeroing all PDAF related settings in the camera tuning file (see example in [Tuning](#tuning) section). 

### Manual Focus

The autofocus algorithm in libcamera has several modes of operation. In Manual mode, it simply sets the lens to focus at a distance specified by the user. While this mode is active, the algorithm maintains the lens focus at the specified distance by activating the lens motor, and further adjustments are not possible. If you would like to adjust the focus manually, you will need to toggle the **AF/MF switch**, found on many Canon lenses, to the MF (manual focus) side.

Use the manual focus switch on the Canon lens to check if focus is achieved at all. Due to the magnifying effect, the minimum focus distance may be greater than what is specified on the lens body. There may be combinations of object and lens positions where the lens cannot achieve focus.

After adjusting the focus manually, the lens may not find sharp focus immediately due to drifting from its base position. It may appear "lost" until it synchronizes its position by bumping into the limiters. Just give it another try, and it should correct itself.

### Focus Hunting

The CDAF (Contrast Detection Autofocus) algorithm on a Raspberry Pi uses two primary settings to control its operation: `step_coarse` and `step_fine`, measured in dioptres (inverse metres).

Start with a moderate value for `step_coarse`. A larger value makes the lens move faster and cover a larger area. If the initial search seems slow, increase `step_coarse` to make the lens move more aggressively. If the initial search overshoots the focus, reduce the value for finer adjustments.

`step_fine` ensures the lens fine-tunes its position after the coarse search. If the focus is not sharp enough, decrease `step_fine` to achieve more delicate adjustments.  If the lens takes too long to lock focus, increase `step_fine` for quicker but less precise adjustments.

## Troubleshooting

If you're experiencing issues with the setup or the adapter, please refer to our [troubleshooting guide](doc/troubleshooting.md) for more help.
