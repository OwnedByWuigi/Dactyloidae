#declare BASE = 1;
#include "resource/logo/logo.pov"

#ifdef(VERSION)
object {
	text {
		ttf "resource/font/IBMPlexSerif.ttf",
		VERSION,
		1, 0
	}
	scale 5
	translate <7.5, 0, 1>
	pigment {
		color Red
	}
	finish {
		phong 1
		reflection 0.3
	}
}
#end

object {
	text {
		ttf "resource/font/IBMPlexSerif.ttf",
#ifdef(NODEJS)
		"for Node.JS",
#else
		"Milsko Toolkit",
#end
		1, 0
	}
	scale 2
#ifdef(NODEJS)
	translate <5, -1, -1>
#else
	translate <2.5, -1, -1>
#end
	pigment {
		color White
	}
	finish {
		phong 1
		reflection 0.3
	}
}

camera {
	location <8+5, 2.5, -37.5>
	look_at <7, 0.75, 0>
	angle 35
	up <0, 1, 0>
	right image_width / image_height * x
}
