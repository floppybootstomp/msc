## Usage
### How to make and run
```
make
./sine
```

### How to play your song
songs are output to the ./data/ directory
```
aplay ./data/<your_song_name> -r 44100 -f S16_LE
```

### Convert to flac in ffmpeg
```
ffmpeg -f s16le -ar 44.1k -ac 1 -i <path_to_song>.pcm <path_to_song>.flac
```
