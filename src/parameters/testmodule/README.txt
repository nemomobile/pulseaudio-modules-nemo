Copy the compiled module to the module path in the DUT (/usr/lib/pulse-x.y.z/modules)

Copy the following files to /root/ in the DUT
    generate_testdata.py
    parameter-test.pa
    run-tests.sh
    start-pa.sh

Login to DUT as root
    python generate_testdata.py
    stop pulseaudio
    sh start-pa.sh
    sh run-tests.sh (from a different shell)

If the tests fail, pulseaudio will abort due to an assertion. Check the log for
specific info on what went wrong.

