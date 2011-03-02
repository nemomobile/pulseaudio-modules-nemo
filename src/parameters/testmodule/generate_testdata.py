import os
pjoin = os.path.join

def create_testdata(directory, testdata):
    modes_dir = pjoin(directory, "modes")
    algs_dir = pjoin(directory, "algs")

    os.mkdir(directory)
    os.mkdir(modes_dir)
    os.mkdir(algs_dir)

    for mode in testdata:
        mode_dir = pjoin(modes_dir, mode[0])

        if not os.path.exists(mode_dir):
            os.mkdir(mode_dir)

        for (alg, params) in mode[1]:
            alg_dir = pjoin(algs_dir, alg)
            params_file = pjoin(alg_dir, params)

            if not os.path.exists(alg_dir):
                os.mkdir(alg_dir)

            f = open(params_file, "w")
            f.write(params + "_parameters")
            f.close()

            os.symlink(pjoin("../../algs", alg, params), pjoin(mode_dir,alg))

testdata_basic = [
    # These are for disabling all algs
    # i.e. when the mode is switched to mode_reset_1 and then mode_reset_2, all algs
    # should be disabled with the status MEEGO_PARAM_DISABLE.
    ("mode_reset1", [("alg_"+alg, "set_reset_"+alg) for alg in ["a", "b", "c"]]),
    ("mode_reset2", []),

    ("mode_a", [("alg_a", "set_a1"),
                ("alg_b", "set_b1")]),

    ("mode_b", [("alg_a", "set_a2"),
                ("alg_b", "set_b1")]),

    ("mode_c", [("alg_a", "set_a3"),
                ("alg_b", "set_b2")]),

    ("mode_d", [("alg_c", "set_c1")])
]

create_testdata("testdata_basic", testdata_basic)

