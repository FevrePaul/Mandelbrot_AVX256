#!/bin/sh

build/./view $1 build/test.bmp
~/Downloads/./view $1 build/ref.bmp

compare -metric ae build/test.bmp build/ref.bmp build/tt.bmp
