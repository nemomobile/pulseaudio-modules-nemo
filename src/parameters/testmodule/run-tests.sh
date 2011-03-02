# These are needed for detecting mode changes (the names are important)
pactl load-module module-null-sink sink_name=sink.hw0
pacat /dev/zero --device=sink.hw0 --stream-name="Voice module master sink input" &
pid=$!

pactl load-module module-meego-parameters directory=/root/testdata_basic
pactl load-module module-meego-test-parameters
kill $pid
