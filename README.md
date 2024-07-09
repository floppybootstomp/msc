## Usage
### How to make and run
```
make
./sine
```
the -lm gcc flag is required to compile

### How to play your song
```
aplay <path_to_song> -r 44100 -f S16_LE
```

### Convert to flac in ffmpeg
```
ffmpeg -f s16le -ar 44.1k -ac 1 -i <path_to_song>.pcm <path_to_song>.flac
```
