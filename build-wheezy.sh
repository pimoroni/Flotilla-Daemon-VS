#!/bin/bash

sudo apt-get update

sudo apt-get --yes --force-yes install git cmake gcc-4.8 g++-4.8 automake autoconf libtool libboost1.50-dev libboost-system1.50-dev

git clone --depth=1 https://github.com/martinling/libserialport
cd libserialport
./autogen.sh
./configure
make
sudo make install
cd ../

git clone --depth=1 https://github.com/zaphoyd/websocketpp
cd websocketpp
mkdir build
cd build
cmake ..
make
sudo make install
cd ../../

git clone --depth=1 https://github.com/pimoroni/flotilla-daemon-vs
cd flotilla-daemon-vs/Flotilla
make CC=g++-4.8 DEPS=../../
