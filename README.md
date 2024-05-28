![minipro.jpg](res%2FAppIcon.ico)
### Very-much experimental GUI wrapper for the awesome [minipro](https://gitlab.com/DavidGriffith/minipro) CLI tool
![Screenshot 2024-05-28 at 09.18.19.png](res%2FScreenshot%202024-05-28%20at%2009.18.19.png)
`git clone https://github.com/twelve-chairs/minipro-gui.git`

`cd minipro-gui`

#### MacOS

`brew install minipro qt6`

`cmake -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_MAKE_PROGRAM=ninja -G Ninja -S . -B "./cmake-build-minsizerel"`

`./cmake-build-minsizerel/minipro-gui`
#### Ubuntu
TBD
#### Fedora
TBD