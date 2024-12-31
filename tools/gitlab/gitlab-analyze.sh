#!/usr/bin/env bash

cd $(realpath -m "$0/../../..")

set -ex

export TMPDIR_TEST="${TMPDIR_TEST:-bin/tmp/}"
mkdir -p "$TMPDIR_TEST"

declare -A modes=(
    gcc_release_valgrind 0
    clang_san 0
    gcc_debug_coverage 0
)
declare -i trigger_value=0

for m in "$@" ; do
    if [[ -z "${modes[$m]}" ]]; then
        set +x
        m="${!modes[@]}"
        echo "$0 {${m// /|}}"
        exit 1
    fi
    modes[$m]=1
    trigger_value=1
done

# BJAM Build Test
echo -e "
using gcc : : g++ -DREDEMPTION_DISABLE_NO_BOOST_PREPROCESSOR_WARNING ;
using clang : : clang++ -DREDEMPTION_DISABLE_NO_BOOST_PREPROCESSOR_WARNING -Wno-reserved-identifier ;
" > project-config.jam
toolset_gcc=toolset=gcc
gcovbin=gcov
toolset_clang=toolset=clang

export REDEMPTION_TEST_DO_NOT_SAVE_IMAGES=1
export LSAN_OPTIONS=exitcode=0 # re-trace by valgrind
export UBSAN_OPTIONS=print_stacktrace=1

# export BOOST_TEST_COLOR_OUTPUT=0

# export REDEMPTION_LOG_PRINT=1
export REDEMPTION_LOG_PRINT=0
# export cxx_color=never

export BOOST_TEST_RANDOM=$RANDOM
echo random seed = $BOOST_TEST_RANDOM

run_bjam() {
    /usr/bin/time --format='%Es - %MK' bjam -q "$@"
}

build()
{
    run_bjam "$@" || {
        local e=$?
        export REDEMPTION_LOG_PRINT=1
        run_bjam "$@" -j1
        exit $e
    }
}

rootlist()
{
    ls -1
}

timestamp=$(printf '%(%s)T')
show_duration()
{
    local timestamp2=$(printf '%(%s)T')
    echo duration"[$@]": $(((timestamp2-timestamp)/60))m $(((timestamp2-timestamp)%60))s
    timestamp=$timestamp2
}


if (( ${modes[gcc_release_valgrind]} == trigger_value )); then
    # implicitly created by bjam
    mkdir -p bin
    beforerun=$(rootlist)

    # release for -Warray-bounds and not assert
    build $toolset_gcc -j4 release cxxflags='-g -Wno-deprecated-declarations'

    show_duration $toolset_gcc release

    # Warn new files created by tests.
    set -o pipefail
    diff <(echo "$beforerun") <(rootlist) | while read l ; do
        echo "Jenkins:${diffline:-0}:0: warnings: $l [directory integrity]"
        ((++diffline))
    done || echo "Directory integrity error: ^^^^^^^^^^^^^^^^^^^^^^^^^^^^"
    set +o pipefail

    # valgrind
    cmds=()
    while read l ; do
        f=${l#*release/tests/}
        f=valgrind_reports/"${f//\//@}".xml
        cmds+=("valgrind --max-stackframe=234425648 --child-silent-after-fork=yes --xml=yes --xml-file=$f $l")
    done < <(find ./bin/gcc*/release/tests/ -executable -type f -name test_'*')
    mkdir -p valgrind_reports
    /usr/bin/time --format="%Es - %MK" parallel -j4 ::: "${cmds[@]}"

    show_duration valgrind
fi

if (( ${modes[clang_san]} == trigger_value )); then
    build $toolset_clang -j4 san cxxflags='-Wno-deprecated-declarations'
    rm -rf bin/clang*

    show_duration $toolset_clang
fi

if (( ${modes[gcc_debug_coverage]} == trigger_value )); then
    # debug with coverage
    build $toolset_gcc -j4 debug -s FAST_CHECK=1 \
        cxxflags='--coverage -Wno-deprecated-declarations' \
        linkflags=-lgcov

    gcovr --gcov-executable $gcovbin --xml -r . -f src/ bin/gcc*/debug/ \
      --gcov-ignore-errors=source_not_found --merge-mode-functions separate \
      > gcovr_report.xml

    show_duration $toolset_gcc coverage
fi

rm -rf "$TMPDIR_TEST"
