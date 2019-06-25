# Moon Melon Control
**Software for the moon melon control smart light bulb**
<table><tr><td>
It runs on a NodeMcu v3 and was used to power an art installation called "Moon Melon Field". The installation is consisting of 25 lamps hanging in the air around 3m above ground.
Each of the lamp has a LED strip (60pixels), a sensor, reporting people's movement below the lamps over a MQTT broker to python program which in turn triggers sounds.

The lamps in turn can be controlled (brightness, type of animation, sensor threshhold...) over a set of commands which can be sent over MQTT.
</td><td>
  
To get a better understanding check out the [documentation of the project here](http://lab.stephanpetzl.com/index.php?/projects/moon-melon-field/)
<img align="right" margin="30" alt="Moon Melon Field installation at Nowhere 2018" src="http://dev.stephanpetzl.com/i.php?/000/237/IMG-1048,medium.2x.1531383328.JPG"/>

</td></tr></table>

## How to run it

Even though I developed most of the code with Arduino IDE, I recommend using [PlattfromIO](https://platformio.org/) for developing since it's managing the library dependencies for you.
In order to control the lamp over MQTT, you need to connect to your wifi network. Therefore you need to edit wifi credentials in [MoonMelon.ino](https://github.com/stoefln/MoonMelonControl/blob/master/src/MoonMelon.ino) before deploying the software to your nodeMcu.

## Roadmap

My plans are to develop the project towards a "Smart Light IOT Kit". So far on my task list:
* Support a good set of nice animations
* MQTT API for controlling lamp settings, changing animations, brightness, etc...
* MQTT API for setting each pixel independently
* MQTT API for setting frame by frame animations
* Easy pairing so you don't need to compile the code to connect to the wifi of your choice
