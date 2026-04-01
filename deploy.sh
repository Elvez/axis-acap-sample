#!/bin/bash
curl -u $1:$2 --anyauth -F "file=@$3" "http://$4/axis-cgi/applications/upload.cgi"