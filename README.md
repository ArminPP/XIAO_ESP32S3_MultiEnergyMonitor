# Multi Energy Monitor Project (BETA!)

This project is designed quick and dirty to monitor multiple energy sources using the ESP32 microcontroller and 3 PZEM_004T Sensors and displays it on a simple web interface. Key features include:

- **Solar Data Monitoring**: Measures power, current, energy, frequency, voltage, and power factor from solar panels.
- **Consumption Data Monitoring**: Tracks power consumption, current, energy usage, frequency, voltage, and power factor of a house.
- **Grid Data Monitoring**: Monitors power, current, energy, frequency, voltage, and power factor from the power grid.
- **WiFi Connectivity**: Connects to a WiFi network to provide real-time data access via a web server.
- **EmonCMS**: Sends Data to a local EmonCMS server hosted on a Respberry PI 
- **Web Interface**: Displays collected data on a simple web page.
- **Reboot Functionality**: Includes a reboot button on the web interface to restart the ESP32 remotely.
- **Runtime Display**: Shows the system's runtime in days, hours, minutes, seconds, and milliseconds.
- **Signal Strength**: Displays the WiFi signal strength (RSSI) on the web interface.

<img src="DOCUMENTATION/IMG20240724165352.jpg" alt="ESP32-S3 from SeedStudio" width="400px" style="display: inline; margin: 0 auto">

*ESP32-S3 from SeedStudio*
<br>
<br>
<img src="DOCUMENTATION/IMG20240724165547.jpg" alt="The 3 sensors and the clamp meters" width="400px" style="display: inline; margin: 0 auto">

*The 3 PZEM_004T Sensors and the clamp meters*
<br>
<br>
<img src="DOCUMENTATION/Webserver ESP32.png" alt="ESP32 simple webserver" width="400px" style="display: inline; margin: 0 auto">

*ESP32 simple webserver*
<br>
<br>
<img src="DOCUMENTATION/EmonCMS.png" alt="EmonCMS dashboard" width="400px" style="display: inline; margin: 0 auto">

*EmonCMS Dashboard*
<br>
<br>
<br>
This project is ideal for monitoring energy usage from the power grid, the solar panels and the consumption of a house.