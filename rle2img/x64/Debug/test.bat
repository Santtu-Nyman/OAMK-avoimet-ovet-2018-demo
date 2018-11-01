:loop
srt -p COM14 --single_data_burst -o rle_data.dat
rle2img rle_data.dat
GOTO loop