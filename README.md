# gazebo_plugin_setup

## Quick Start

* Clone repository

```
git clone https://github.com/tylerlum/gazebo_plugin_setup.git
```

* Enter repository

```
cd gazebo_plugin_setup
```

* Create build directory and enter it

```
mkdir build
cd build
```

* Build

```
cmake ../
make
```

* Set plugin path

```
export GAZEBO_PLUGIN_PATH=${GAZEBO_PLUGIN_PATH}:`pwd`
```

* Run world

```
gzserver -u ../model_push.world --verbose
```

* Start a new terminal and create client. Unpause to see block moving

```
gzclient
```

* Look at output (hoping to see prints from all plugins, but only get it from model plugin)

```
[Msg] Waiting for master.
[Msg] Connected to gazebo master @ http://127.0.0.1:11345
[Msg] Publicized address: 10.224.242.26
[Msg] IN MODEL PUSH
```
