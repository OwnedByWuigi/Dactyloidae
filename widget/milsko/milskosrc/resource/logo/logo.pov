#include "shapes.inc"
#include "colors.inc"
#include "textures.inc"
#include "skies.inc"

object {
	O_Cloud1
}

sky_sphere {
	pigment {
		gradient y
		color_map {
			[0.0 White*0.8]
			[0.2 rgb<0.2 0.2 0.7>]
		}
	}
}

union {
object {
	Cube
	translate <-1, 0, 1>
	pigment {
		color Red
	}
}

object {
	Cube
	translate <1, 0, 1>
	pigment {
		color Cyan
	}
}

object {
	Cube
	translate <-1, 0, -1>
	pigment {
		color Blue
	}
}

object {
	Cube
	translate <1, 0, -1>
	pigment {
		color Green
	}
}

object {
	Cube
	translate <0, 2, 0>
	pigment {
		color Red
	}
}

rotate <0, clock * 360, 0>
}

plane {
	y, -1
	pigment {
		checker Red Gray
	}
	finish {
		phong 1
		reflection 0.5
	}
}

light_source {
	<-250, 250, -500>
	color White*2
	parallel point_at <0, 0, 0>
}

#ifndef(FAR)
#declare FAR = 1;
#end

#ifndef(Y)
#declare Y = 0;
#end

#ifndef(BASE)
camera {
	location <1.5 * FAR, 2.5 + Y, -10 * FAR>
	look_at <0, 0.75 + Y, 0>
	angle 35
	up <0, 1, 0>
	right image_width / image_height * x
}
#end
