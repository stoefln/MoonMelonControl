# MoonMelonControl
**Software for the moon melon control smart light bulb**

It runs on a NodeMcu v3 and was used to power an art installation called "Moon Melon Field". The installation is consisting of 25 lamps hanging in the air around 3m above ground.
Each of the lamp has a sensor, reporting people's movement below the lamps over a MQTT broker to python program which in turn triggers sounds.

The lamps in turn can be controlled over a set of commands which can be sent over MQTT.
