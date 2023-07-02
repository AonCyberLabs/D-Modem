#!/bin/sh

PORT=1234

if [ -z $1 ];
then
  exec socat tcp-listen:$PORT fd:$2
else
  exec socat tcp-connect:$1:$PORT fd:$2
fi