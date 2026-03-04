#!/bin/bash
mkdir -p /workspace/dsfconnector/common/.gensrc/
fastddsgen $1 -replace -d /workspace/dsfconnector/common/.gensrc/
