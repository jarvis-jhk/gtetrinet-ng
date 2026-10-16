#!/usr/bin/env bash
# Plays gtetrinet-ng against TetriNET servers we did not write: tetrinetx 1.13.16 (built here
# from its pinned source release) and Andrew Church's tetrinet-server (the Debian/Ubuntu
# package), plus our own standalone server for comparison.
#
#   tests/interop.sh CORE_BUILD_DIR [GAME_BUILD_DIR]
#
# CORE_BUILD_DIR has test_interop and gtetrinet-ng-server; GAME_BUILD_DIR (optional) has
# test_online, the game itself joining through its online entry. Needs gcc, curl, libadns
# (libadns1-dev) for tetrinetx and /usr/games/tetrinet-server; a missing server is an error
# unless INTEROP_SKIP lists it ("tetrinetx", "tetrinet-server").
set -euo pipefail

core=$(cd "$1" && pwd)
game=${2:+$(cd "$2" && pwd)}
skip=" ${INTEROP_SKIP:-} "
work=$(mktemp -d)
pids=()
cleanup() {
    for p in "${pids[@]}"; do kill "$p" 2>/dev/null || true; done
    [ -f "$work/tetrinetx/bin/game.pid" ] && kill "$(cat "$work/tetrinetx/bin/game.pid")" 2>/dev/null || true
    rm -rf "$work"
}
trap cleanup EXIT

TETRINETX_URL="http://prdownloads.sourceforge.net/tetrinetx/tetrinetx-1.13.16+qirc-1.40c.tar.gz?download"
TETRINETX_FALLBACK="https://cfhcable.dl.sourceforge.net/project/tetrinetx/tetrinetx/1.13.16%2Bqirc-1.40c/tetrinetx-1.13.16%2Bqirc-1.40c.tar.gz"
TETRINETX_SHA256=2b7210a0c8950c13e648d8abc36b7bca8ce58035999526301a715aacd40d15d4

wait_port() {
    for _ in $(seq 100); do
        (exec 3<>"/dev/tcp/127.0.0.1/$1") 2>/dev/null && return 0
        sleep 0.1
    done
    echo "nothing listens on port $1" >&2
    return 1
}

failed=0
run() {
    echo "== $*"
    if ! (cd "$work" && GTN_NO_REPORT=1 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 300 "$@"); then
        echo "FAILED: $*"
        failed=1
    fi
    # the runs reuse nicknames: let the server notice the last run's players are gone
    sleep 1
}

# the server list's query: EXPECT is gtetrinet-ng-server --query's exit status, 0 when the
# server answers the query protocol (ours, tetrinetx), 3 when it only waits for a login and
# must not mind being asked (tetrinet-server)
query() {
    echo "== query 127.0.0.1:$1 (expect $2)"
    local rc=0
    "$core/gtetrinet-ng-server" --query "127.0.0.1:$1" || rc=$?
    if [ "$rc" != "$2" ]; then echo "FAILED: query exit $rc, expected $2"; failed=1; fi
}

# what a classic server gets: TetriFast must be refused with a reason, classic must work
classic_suite() {
    run "$core/test_interop" 127.0.0.1 "$1"
    run "$core/test_interop" 127.0.0.1 "$1" fast-unsupported
    [ -n "$game" ] && run "$game/test_online" 127.0.0.1 "$1" fast-unsupported
    return 0
}

echo "### gtetrinet-ng-server (reference)"
"$core/gtetrinet-ng-server" --port 31470 --local --quiet &
pids+=($!)
wait_port 31470
query 31470 0
run "$core/test_interop" 127.0.0.1 31470
run "$core/test_interop" 127.0.0.1 31470 fast
[ -n "$game" ] && run "$game/test_online" 127.0.0.1 31470
kill "${pids[-1]}"

if [[ $skip != *" tetrinet-server "* ]]; then
    echo "### tetrinet-server $(dpkg-query -W -f='${Version}' tetrinet-server 2>/dev/null || true)"
    mkdir -p "$work/church"
    HOME="$work/church" /usr/games/tetrinet-server &
    pids+=($!)
    wait_port 31457
    query 31457 3
    classic_suite 31457
    kill -0 "${pids[-1]}" || { echo "FAILED: tetrinet-server is no longer running"; failed=1; }
    kill "${pids[-1]}" 2>/dev/null || true
    wait "${pids[-1]}" 2>/dev/null || true
    sleep 0.5
fi

if [[ $skip != *" tetrinetx "* ]]; then
    echo "### tetrinetx 1.13.16+qirc-1.40c"
    curl -fsSL -o "$work/tetrinetx.tgz" "$TETRINETX_URL" || curl -fsSL -o "$work/tetrinetx.tgz" "$TETRINETX_FALLBACK"
    echo "$TETRINETX_SHA256  $work/tetrinetx.tgz" | sha256sum -c -
    tar -xzf "$work/tetrinetx.tgz" -C "$work"
    mv "$work/tetrinetx-1.13.16+qirc-1.40c" "$work/tetrinetx"
    # unmodified source; -w because 2002-era C warns a lot under a modern gcc
    gcc -O2 -w "$work/tetrinetx/src/main.c" -o "$work/tetrinetx/bin/tetrix.linux" -ladns
    # it forks into the background and wants a fully qualified host name
    (cd "$work/tetrinetx/bin" && HOSTNAME=interop.test ./tetrix.linux >/dev/null)
    wait_port 31457
    query 31457 0
    classic_suite 31457
    kill -0 "$(cat "$work/tetrinetx/bin/game.pid")" || { echo "FAILED: tetrinetx is no longer running"; failed=1; }
fi

[ $failed = 0 ] && echo "interop: all passed" || echo "interop: FAILURES above"
exit $failed
