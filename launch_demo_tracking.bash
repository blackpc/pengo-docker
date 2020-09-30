#!/bin/bash

set -e 

SLEEP_TIME=5

roscore &
sleep 20

#
# OpenVino Myriad
#
docker run -e ROS_MASTER_URI -e ROS_IP --net=host --privileged --rm -v /dev:/dev intelpengo/openvino bash -ic "roslaunch vino_launch pengo_detection.launch myriad:=true camera_name:=cam1"
sleep ${SLEEP_TIME}

#
# D435 front camera
#
docker run -e ROS_MASTER_URI -e ROS_IP --privileged --net=host --rm intelpengo/realsense2 /rs_camera.sh cam1 939622072996 0.17 0 0.27 0 0 0
# sleep ${SLEEP_TIME}

#
# T265
#
# docker run -d -v /dev:/dev -e ROS_MASTER_URI -e ROS_IP --privileged --net=host --rm intelpengo/realsense2-t265 cam5 11622110757 0.17 0 0.27 0 0 0

#
# Kobuki
#
docker run -e ROS_MASTER_URI -e ROS_IP --net=host --privileged --rm intelpengo/kobuki-driver:latest /dev/ttyUSB1

docker run -e ROS_MASTER_URI -e ROS_IP --net=host intelpengo/kobuki-navigation:latest

#
# Person follower
#

docker run -e ROS_MASTER_URI -e ROS_IP --net=host intelpengo/person-follower:latest

#
# ROS slam-toolbox
#

docker run -e ROS_MASTER_URI -e ROS_IP --net=host intelpengo/slam-toolbox:latest

#
# RP Lidar
#

docker run -e ROS_MASTER_URI -e ROS_IP --rm -it --net=host --privileged intelpengo/rplidar 0 0 0.22 -1.5707 0 3.1415