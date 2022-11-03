#!/usr/bin/env bash

directory=$(dirname "$0")
mkdir "$directory"/build;
(cd "$directory"/build || exit; cmake ..; make all;)
"$directory"/build/test/RunTests