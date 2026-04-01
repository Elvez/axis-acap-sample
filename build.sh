#!/bin/bash
sudo docker build --tag acap31 . &&
sudo docker cp $(sudo docker create acap31):/opt/app ./build