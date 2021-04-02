#!/bin/bash

sudo modprobe ib_core
sudo modprobe rdma_ucm
sudo modprobe siw

sudo rdma link add siw_eno1 type siw netdev eno1
sudo rdma link add siw_lo type siw netdev lo


#if $(ibv_devices | grep -q siw)
#then
#	sudo rdma link delete siw_eno
#	sudo rdma link delete siw_lo
#fi 
