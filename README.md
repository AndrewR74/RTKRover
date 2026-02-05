# Components Required
[Arduino Nano ESP32](https://www.amazon.com/dp/B0C947BHK5?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_4&th=1)

[RTK Module w/t Antenna](https://www.amazon.com/dp/B0G3PLKT69?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_1)

[Tripod](https://a.co/d/00Bj14sZ) - Optional

[Phone mount](https://a.co/d/02GpWcwg) - Optional

# NTRIP Setup
New York only - Register for a Network Connections & RINEX Products from NY CORS NTRIP (it is free, but they need to approve you). 

Alternatively, check out https://rtk2go.com for [base stations that are hosted by the community. ](http://rtk2go.com:2101/SNIP::STATUS)

# Wiring
1. Connect module GND and 3V pins to the corresponding pins on Arduino nano board. (Jumpers or the provided connector can be used)
2. Connect RX2 on the Arduino nano to the TX pin on the RTK module. 
3. Connect TX2 on the Arduino nano to the RX pin on the RTK module.

# IDE Setup + Flashing Board
1. The BLESerial library needs to be added to the Arduino IDE Library manager. A copy of the library is located in the Libraries.bak folder.
2. Add Arduino ESP32 Boards from Arduino through the board manager if not already pre-installed.
3. Optionally, if the Wi-Fi credentials and NTRIP details are known at this time, it is HIGHLY recommended to enter the details in the source code. It will elimnate the need Web UI setup steps below

# Rover Web UI
1. Connect to the Wi-FI network "RTK-Rover-Setup" that the rover is broadcasting
2. Navigate to 192.168.4.1 in a browser address bar. (Sometimes it hangs and doesn't load)
3. Scroll to the bottom of the page. Select the desired Wi-Fi that the rover will use to the internet. (It can be a mobile phone hotspot. iOS required compatibility mode enabled.)
4. Enter the password of the Wi-Fi network if applicable.
5. Enter your NTRIP server, port, and credentials.
6. Press Save & Reboot.
7. The smartphone may need to reconnect to the RTK-Rover-Setup Wi-Fi network again.
8. Refresh the browser page and verify the Rover connect to the Wi-Fi network that has internet access.

# Operation
1. Install SWMaps on a smartphone from the App Stores (Android and iOS)
2. Scan for External receivers. ESP32 should be listed if the Arduino is configured correctly.
3. If the RTK module is operating properly with a clear view of the sky, SWMaps will show the location of the rover.
4. RTK Float and RTK Fix locations may take a few minutes to become available.
