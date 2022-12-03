#!/bin/bash

BASE_DIR="SET_YOUR_BASE_DIR"
TRIGGER_DIR="SET YOUR TRIGGER DIR"
OUTPUT_DIR="SET YOUR OUTPUT DIR"

CURR_KEY=`cat $TRIGGER_DIR/currentKey.txt`
NEW_KEY=$( php $BASE_DIR/my-qr_guest_key.php ) #this uses an existing script that I have to get the text of the SSID Key.

if [ $CURR_KEY != $NEW_KEY ]; then
        #echo "Keys DO NOT Match!"
        convert -size 560x60 xc:white -gravity center -font Palatino-Bold -pointsize 42 -fill black -draw "text 0,0 'Guest Wifi'" $OUTPUT_DIR/headerImg.png
        convert -size 560x40 xc:white -gravity center -font Palatino-Bold -pointsize 32 -fill black -draw "text 0,0 'SSID: $( php $BASE_DIR/my-qr_guest_ssid.php )'" $OUTPUT_DIR/ssidImg.png
        php my-qr_guest.php | qrencode -m1 -s8 -o $OUTPUT_DIR/guestCode.png

        convert -size 560x40 xc:white -gravity center -font Palatino-Bold -pointsize 32 -fill black -draw "text 0,0 'KEY: $( php $BASE_DIR/my-qr_guest_key.php )'" $OUTPUT_DIR/keyImg.png
        convert -size 560x240 xc:white $OUTPUT_DIR/fillerImg.png
        convert -bordercolor green -border 4 $OUTPUT_DIR/headerImg.png $OUTPUT_DIR/headerImg.png
        convert -append $OUTPUT_DIR/headerImg.png $OUTPUT_DIR/ssidImg.png $OUTPUT_DIR/fillerImg.png $OUTPUT_DIR/keyImg.png $OUTPUT_DIR/mypage.png

        composite -gravity north $OUTPUT_DIR/guestCode.png -geometry +0+104 $OUTPUT_DIR/mypage.png $OUTPUT_DIR/mypage.png

        convert -bordercolor green -border 4 $OUTPUT_DIR/mypage.png $OUTPUT_DIR/finalCode.png

        echo $NEW_KEY > $TRIGGER_DIR/currentKey.txt
        echo $NEW_KEY > $OUTPUT_DIR/currentKey.txt
        echo `date +%s` > $OUTPUT_DIR/lastChange.txt
fi
