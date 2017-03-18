#!/bin/sh
export ROOT=$(cd `dirname $0`; pwd)

$ROOT/skynet/3rd/lua/lua $ROOT/client-simple/simpleclient.lua $ROOT $1
