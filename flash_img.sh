#!/bin/bash

firmware=$(echo $1 | sed 's/\\/\//g')
port=$2

echo --------------------------------------------
echo file: $firmware
echo port: $port
echo --------------------------------------------
echo ''

./tools/W800/wm_tool.exe -dl $firmware -ws 2M -ds 2M -rs rts -c $port