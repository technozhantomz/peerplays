#!/bin/bash

find ./libraries/plugins/peerplays_sidechain -regex ".*[c|h]pp" | xargs clang-format -i

