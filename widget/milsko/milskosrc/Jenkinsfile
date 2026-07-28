pipeline {
	agent {
		label "built-in"
	}
	stages {
		stage("Build document") {
			when {
				branch "master"
			}
			agent {
				label "built-in"
			}
			steps {
				sh("rm -rf include/Mw/LowLevel/Wayland")
				sh("doxygen")
				sh("rm -rf /var/www/milsko-doxygen")
				sh("mv doxygen/html /var/www/milsko-doxygen")
			}
			post {
				always {
					notifyDiscord()
				}
			}
		}
		stage("Build") {
			parallel {
				stage("Build for Linux 64-bit") {
					agent {
						label "built-in"
					}
					steps {
						sh("git clean -dfx")
						sh("./configure --enable-opengl --enable-vulkan --without-vulkan-string-helper")
						sh("make -j4")
						sh("mv src/libMw.so libMw64.so")
						archiveArtifacts("libMw64.so")
					}
				}
				stage("Build for Windows 32-bit") {
					agent {
						label "built-in"
					}
					steps {
						sh("git clean -dfx")
						sh("./configure --enable-opengl --enable-stb-truetype --disable-freetype2 --cross --target=Windows --host=i686-w64-mingw32")
						sh("make -j4")
						sh("mv src/Mw.dll Mw32.dll")
						sh("mv src/libMw.dll.a libMw32.dll.a")
						archiveArtifacts("Mw32.dll,libMw32.dll.a")
					}
				}
				stage("Build for Windows 64-bit") {
					agent {
						label "built-in"
					}
					steps {
						sh("git clean -dfx")
						sh("./configure --enable-opengl --enable-stb-truetype --disable-freetype2 --cross --target=Windows --host=x86_64-w64-mingw32")
						sh("make -j4")
						sh("mv src/Mw.dll Mw64.dll")
						sh("mv src/libMw.dll.a libMw64.dll.a")
						archiveArtifacts("Mw64.dll,libMw64.dll.a")
					}
				}
				stage("Build for Windows 32-bit (MSVC)") {
					agent {
						label "2012r2"
					}
					steps {
						bat("git clean -dfx")
						bat("nmake -f NTMakefile")
						bat("move /y src\\Mw.dll MwMSVC32.dll")
						bat("move /y src\\Mw.lib MwMSVC32.lib")
						archiveArtifacts("MwMSVC32.dll,MwMSVC32.lib")
					}
				}
				stage("Build for Windows 32-bit (Watcom)") {
					agent {
						label "built-in"
					}
					environment {
						WATCOM = "/usr/watcom"
						INCLUDE = "/usr/watcom/h:/usr/watcom/h/nt"
						PATH = "/usr/watcom/binl64:${env.PATH}"
					}
					steps {
						sh("git clean -dfx")
						sh("wmake -f WatMakefile")
						sh("mv src/Mw.dll MwWat32.dll")
						sh("mv src/Mw.lib MwWat32.lib")
						archiveArtifacts("MwWat32.dll,MwWat32.lib")
						sh("./tools/watcom-pack.sh")
						sh("mv milsko-examples.zip milsko-examples-win32.zip")
						archiveArtifacts("milsko-examples-win32.zip")
					}
				}
			}
			post {
				always {
					notifyDiscord()
				}
			}
		}
	}
}
