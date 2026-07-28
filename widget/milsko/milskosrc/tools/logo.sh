#!/bin/sh
povray +A +W256 +H256 +Ologo.png resource/logo/logo.pov
povray +A +W1280 +H640 +Obanner.png resource/logo/banner.pov
povray +A +W1280 +H640 +Obanner-nodejs.png Declare=NODEJS=1 resource/logo/banner.pov
povray +A +W102 +H47 +O102x47.png resource/logo/102x47.pov
mv logo.png banner.png banner-nodejs.png resource/logo/
magick resource/logo/logo.png -scale x64 resource/logo/logo64.png
magick 102x47.png resource/logo/overlay-102x47.png -compose over -composite resource/logo/102x47.png
rm -f 102x47.png
