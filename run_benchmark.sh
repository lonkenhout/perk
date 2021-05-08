#!/bin/bash

path=`pwd`
cmake . -D DEB:INTEGER=0 -D BM_CLIENT_LAT:INTEGER=1
