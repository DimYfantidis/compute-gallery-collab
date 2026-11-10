#!/usr/bin/env bash
# compute-gallery launcher.
#
# Picks the docker compose service that fits this machine, starts it, builds the
# project inside it and runs the exhibits.
#
#   ./run.sh              open the gallery (starting and building everything it needs)
#   ./run.sh menu         the text menu
#   ./run.sh <command>    run one command and exit (./run.sh help lists them)
#
# Runs on Linux, macOS (bash 3.2+), WSL and Git Bash on Windows; from PowerShell
# or cmd use run.cmd, which starts this script through Git Bash.

set -u
cd "$(dirname "$0")" || exit 1

BUILD_DIR=build/docker
NOVNC_URL=http://localhost:6080/vnc.html
IMAGE=compute-gallery-dev

# ----------------------------------------------------------------------------
# Output helpers
# ----------------------------------------------------------------------------

if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
    B=$'\033[1m'; D=$'\033[2m'; R=$'\033[0m'
    RED=$'\033[31m'; GRN=$'\033[32m'; YLW=$'\033[33m'; MAG=$'\033[35m'; CYN=$'\033[36m'
else
    B=; D=; R=; RED=; GRN=; YLW=; MAG=; CYN=
fi

case "${LC_ALL:-${LC_CTYPE:-${LANG:-}}}" in
    *UTF-8*|*utf-8*|*UTF8*|*utf8*) UNICODE=1 ;;
    *) UNICODE=0 ;;
esac
[ -n "${WT_SESSION:-}" ] && UNICODE=1   # Windows Terminal

if [ "$UNICODE" = 1 ]; then
    S_OK='✔'; S_NO='✘'; S_WARN='▲'; S_RUN='●'; S_ARROW='›'; S_LINE='─'
else
    S_OK='ok'; S_NO='XX'; S_WARN='!!'; S_RUN='*'; S_ARROW='>'; S_LINE='-'
fi

rule()  { local i line=""; for i in $(seq 1 "${1:-74}"); do line="$line$S_LINE"; done; printf '  %s%s%s\n' "$D" "$line" "$R"; }
info()  { printf '  %s%s%s %s\n' "$CYN" "$S_ARROW" "$R" "$*"; }
ok()    { printf '  %s%s%s %s\n' "$GRN" "$S_OK" "$R" "$*"; }
warn()  { printf '  %s%s%s %s\n' "$YLW" "$S_WARN" "$R" "$*"; }
fail()  { printf '  %s%s%s %s\n' "$RED" "$S_NO" "$R" "$*"; }
hint()  { printf '      %s%s%s\n' "$D" "$*" "$R"; }
pause() { [ -t 0 ] || return 0; printf '\n  %sPress Enter to return to the menu%s ' "$D" "$R"; read -r _; }

banner() {
    if [ "$UNICODE" = 1 ]; then
        printf '%s' "$MAG$B"
        printf '  ┌─┐┌─┐┌┬┐┌─┐┬ ┬┌┬┐┌─┐   ┌─┐┌─┐┬  ┬  ┌─┐┬─┐┬ ┬\n'
        printf '  │  │ ││││├─┘│ │ │ ├┤ ───│ ┬├─┤│  │  ├┤ ├┬┘└┬┘\n'
        printf '  └─┘└─┘┴ ┴┴  └─┘ ┴ └─┘   └─┘┴ ┴┴─┘┴─┘└─┘┴└─ ┴ \n'
        printf '%s' "$R"
    else
        printf '  %sCOMPUTE-GALLERY%s\n' "$MAG$B" "$R"
    fi
    printf '  %sHPC exhibition gallery: OpenCL, OpenGL and SDL2 demos%s\n' "$D" "$R"
}

# ----------------------------------------------------------------------------
# Host and Docker detection
# ----------------------------------------------------------------------------

detect_host() {
    case "$(uname -s)" in
        Darwin) HOST=macos; HOST_LABEL="macOS" ;;
        MINGW*|MSYS*|CYGWIN*)
            HOST=windows; HOST_LABEL="Windows (Git Bash)"
            export MSYS_NO_PATHCONV=1 ;;   # keep /home/workspace etc. from becoming C:/...
        Linux)
            if grep -qi microsoft /proc/version 2>/dev/null; then
                HOST=wsl; HOST_LABEL="Windows (WSL)"
            else
                HOST=linux; HOST_LABEL="Linux"
            fi ;;
        *) HOST=other; HOST_LABEL="$(uname -s)" ;;
    esac

    GPU=""
    local smi
    for smi in nvidia-smi /usr/lib/wsl/lib/nvidia-smi; do
        if command -v "$smi" >/dev/null 2>&1; then
            GPU="$("$smi" --query-gpu=name --format=csv,noheader 2>/dev/null | head -n 1 | tr -d '\r')"
            [ -n "$GPU" ] && break
        fi
    done

    if [ "$HOST" = linux ]; then
        # Files the container writes into the mounted workspace stay owned by you.
        HOST_UID="$(id -u)"; HOST_GID="$(id -g)"
        export HOST_UID HOST_GID
    fi
}

DOCKER_STATE=""   # missing | stopped | ok
detect_docker() {
    DOCKER_OS=""; DOCKER_KERNEL=""; DOCKER_RUNTIMES=""; DOCKER_VERSION=""
    if ! command -v docker >/dev/null 2>&1; then DOCKER_STATE=missing; return; fi
    local out
    if ! out="$(docker info --format '{{.OperatingSystem}}|{{.KernelVersion}}|{{json .Runtimes}}|{{.ServerVersion}}' 2>/dev/null)"; then
        DOCKER_STATE=stopped; return
    fi
    DOCKER_STATE=ok
    DOCKER_OS="${out%%|*}"; out="${out#*|}"
    DOCKER_KERNEL="${out%%|*}"; out="${out#*|}"
    DOCKER_RUNTIMES="${out%|*}"; DOCKER_VERSION="${out##*|}"
}

is_docker_desktop() { case "$DOCKER_OS" in *"Docker Desktop"*) return 0 ;; esac; return 1; }
is_wsl2_engine()    { case "$DOCKER_KERNEL" in *[Mm]icrosoft*WSL2*|*WSL2*) return 0 ;; esac; return 1; }
has_nvidia_docker() { case "$DOCKER_RUNTIMES" in *nvidia*) return 0 ;; esac; docker info 2>/dev/null | grep -q 'nvidia.com/gpu'; }

service_label() {
    case "$1" in
        linux-nvidia)          echo "Linux, NVIDIA GPU: OpenGL + OpenCL on the GPU" ;;
        windows-wsl)           echo "Windows, NVIDIA GPU: OpenGL on the GPU, OpenCL on the CPU" ;;
        windows-wsl-amd-intel) echo "Windows, AMD/Intel GPU: OpenGL on the GPU, OpenCL on the CPU" ;;
        cpu)                   echo "CPU only: software OpenGL, OpenCL on all cores, GUI in the browser" ;;
        *)                     echo "unknown service" ;;
    esac
}

pick_service() {
    if [ -n "${CG_SERVICE:-}" ]; then SERVICE="$CG_SERVICE"; SERVICE_WHY="set by CG_SERVICE / --service"; return; fi
    SERVICE=cpu; SERVICE_WHY="fallback"
    case "$HOST" in
        macos) SERVICE_WHY="Docker on macOS has no GPU access" ;;
        windows|wsl)
            if is_docker_desktop && is_wsl2_engine; then
                if [ -n "$GPU" ]; then SERVICE=windows-wsl; SERVICE_WHY="Docker Desktop (WSL2) + NVIDIA GPU"
                else SERVICE=windows-wsl-amd-intel; SERVICE_WHY="Docker Desktop (WSL2), no NVIDIA GPU"; fi
            elif is_docker_desktop; then
                SERVICE_WHY="Docker Desktop is not using the WSL 2 engine (no GPU, no WSLg)"
            elif [ -n "$GPU" ] && has_nvidia_docker; then
                SERVICE=linux-nvidia; SERVICE_WHY="Docker Engine inside WSL + NVIDIA runtime"
            fi ;;
        linux)
            if [ -z "${DISPLAY:-}" ]; then SERVICE_WHY="no \$DISPLAY (headless session)"
            elif [ -n "$GPU" ] && has_nvidia_docker; then SERVICE=linux-nvidia; SERVICE_WHY="NVIDIA GPU + NVIDIA container runtime"
            elif [ -n "$GPU" ]; then SERVICE_WHY="NVIDIA GPU found but no NVIDIA container runtime (see doctor)"
            else SERVICE_WHY="no NVIDIA GPU found"; fi ;;
    esac
}

# ----------------------------------------------------------------------------
# Container helpers
# ----------------------------------------------------------------------------

TTY_FLAGS=-T; WINPTY=""
if [ -t 0 ] && [ -t 1 ]; then
    TTY_FLAGS=-it
    # Git Bash's mintty is not a Windows console; docker.exe needs winpty there.
    if [ "${TERM_PROGRAM:-}" = mintty ] && command -v winpty >/dev/null 2>&1; then WINPTY=winpty; fi
fi

is_running() { [ -n "$(docker compose ps -q --status running "$SERVICE" 2>/dev/null)" ]; }
image_exists() { docker image inspect "$IMAGE" >/dev/null 2>&1; }
is_built() { [ -f "$BUILD_DIR/build.ninja" ]; }

in_container() { $WINPTY docker compose exec $TTY_FLAGS -e CG_ENVIRONMENT="$SERVICE" "$SERVICE" bash -lc "$1"; }
capture_in_container() { docker compose exec -T "$SERVICE" bash -lc "$1" 2>/dev/null | tr -d '\r'; }

ensure_docker() {
    case "$DOCKER_STATE" in
        ok) return 0 ;;
        missing) fail "Docker is not installed."; docker_install_hint; return 1 ;;
        stopped) fail "Docker is installed but the engine is not reachable."; docker_start_hint; return 1 ;;
    esac
}

ensure_up() {
    ensure_docker || return 1
    is_running && return 0

    if [ "$SERVICE" = linux-nvidia ] && command -v xhost >/dev/null 2>&1; then
        # Let processes running as your UID (the container user has the same UID) open windows.
        xhost "+SI:localuser:$(id -un)" >/dev/null 2>&1 || warn "xhost failed; windows may not open (see doctor)."
    fi

    if image_exists; then info "Starting ${B}$SERVICE${R}..."
    else info "Starting ${B}$SERVICE${R}. First run: building the image, this takes a few minutes..."; fi

    if ! docker compose up -d "$SERVICE"; then
        fail "Could not start the $SERVICE container."
        up_failure_hint
        return 1
    fi
}

ensure_built() {
    ensure_up || return 1
    in_container "set -e
        if [ ! -f $BUILD_DIR/build.ninja ]; then
            echo '  Configuring CMake (first build only)...'
            if ! cmake -S . -B $BUILD_DIR -G Ninja -DCMAKE_BUILD_TYPE=Release > /tmp/cg-configure.log 2>&1; then
                tail -n 30 /tmp/cg-configure.log; exit 1
            fi
        fi
        cmake --build $BUILD_DIR" || { fail "Build failed (output above)."; hint "Try: ./run.sh clean, then build again. Still failing? ./run.sh doctor"; return 1; }
}

open_url() {
    case "$HOST" in
        windows) cmd.exe /c start "" "$1" >/dev/null 2>&1 ;;
        wsl)     explorer.exe "$1" >/dev/null 2>&1 ;;
        macos)   open "$1" >/dev/null 2>&1 ;;
        *)       command -v xdg-open >/dev/null 2>&1 && xdg-open "$1" >/dev/null 2>&1 ;;
    esac
}

BROWSER_OPENED=0
run_exhibit() {   # name, path in build dir, needs-window (1/0)
    local name="$1" path="$2" gui="$3" rc
    ensure_built || return 1
    if [ "$gui" = 1 ] && [ "$SERVICE" = cpu ]; then
        info "Windows appear in the browser desktop: ${B}$NOVNC_URL${R}"
        if [ "$BROWSER_OPENED" = 0 ]; then open_url "$NOVNC_URL"; BROWSER_OPENED=1; fi
    fi
    printf '\n'; info "Running ${B}$name${R}  (Ctrl+C stops it)"; rule
    in_container "./$BUILD_DIR/$path"
    rc=$?
    rule
    case "$rc" in
        0|130) ok "$name finished."; return 0 ;;
        *) fail "$name exited with code $rc."
           [ "$gui" = 1 ] && hint "If no window appeared, run ./run.sh doctor to check the display connection."
           return "$rc" ;;
    esac
}

# ----------------------------------------------------------------------------
# Hints
# ----------------------------------------------------------------------------

docker_install_hint() {
    case "$HOST" in
        windows|wsl) hint "Install Docker Desktop: https://docs.docker.com/desktop/setup/install/windows-install/"
                     hint "During setup keep 'Use WSL 2 instead of Hyper-V' enabled." ;;
        macos)       hint "Install Docker Desktop: https://docs.docker.com/desktop/setup/install/mac-install/" ;;
        *)           hint "Install Docker Engine + compose plugin: https://docs.docker.com/engine/install/" ;;
    esac
}

docker_start_hint() {
    case "$HOST" in
        windows|wsl|macos) hint "Start Docker Desktop and wait until it shows 'Engine running', then retry." ;;
        *) hint "Start it:            sudo systemctl start docker"
           hint "'permission denied': sudo usermod -aG docker \$USER, then log out and back in." ;;
    esac
}

up_failure_hint() {
    case "$SERVICE" in
        windows-wsl)
            hint "'could not select device driver \"nvidia\"': update the NVIDIA driver and Docker Desktop,"
            hint "or use the AMD/Intel variant:  ./run.sh --service windows-wsl-amd-intel" ;;
        windows-wsl-amd-intel)
            hint "Mount errors under /run/desktop/mnt/host/wslg: WSLg is missing or outdated."
            hint "In PowerShell: wsl --update; wsl --shutdown; then restart Docker Desktop." ;;
        linux-nvidia)
            hint "GPU errors: install the NVIDIA Container Toolkit, then"
            hint "  sudo nvidia-ctk runtime configure --runtime=docker && sudo systemctl restart docker"
            hint "  https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/latest/install-guide.html" ;;
        cpu)
            hint "'port is already allocated': something else uses port 6080; stop it and retry." ;;
    esac
    hint "Full diagnosis: ./run.sh doctor"
}

# ----------------------------------------------------------------------------
# Commands
# ----------------------------------------------------------------------------

cmd_doctor() {
    printf '\n  %sDoctor%s  checking %s with service %s\n' "$B" "$R" "$HOST_LABEL" "$SERVICE"; rule

    case "$DOCKER_STATE" in
        missing) fail "Docker CLI not found"; docker_install_hint; return 1 ;;
        stopped) fail "Docker engine not reachable"; docker_start_hint; return 1 ;;
        ok) ok "Docker $DOCKER_VERSION ($DOCKER_OS)" ;;
    esac
    if docker compose version >/dev/null 2>&1; then ok "Docker Compose $(docker compose version --short 2>/dev/null)"
    else fail "Docker Compose v2 missing"; hint "Update Docker Desktop, or install the docker-compose-plugin package."; return 1; fi

    if [ -n "$GPU" ]; then ok "Host GPU: $GPU"; else warn "No NVIDIA GPU detected on the host"; fi
    info "Service: ${B}$SERVICE${R} ($SERVICE_WHY)"

    case "$SERVICE" in
        windows-wsl|windows-wsl-amd-intel)
            if is_wsl2_engine; then ok "Docker Desktop uses the WSL 2 engine"
            else fail "Docker Desktop is not on the WSL 2 engine"; hint "Docker Desktop > Settings > General > Use the WSL 2 based engine"; fi ;;
        linux-nvidia)
            if has_nvidia_docker; then ok "NVIDIA container runtime available"
            else fail "NVIDIA container runtime missing"; up_failure_hint; fi
            if [ -n "${DISPLAY:-}" ] && [ -d /tmp/.X11-unix ]; then ok "X11 display $DISPLAY"
            else fail "No X11 display"; hint "Run from a desktop session (on Wayland, XWayland provides X11)."; fi ;;
    esac

    ensure_up || return 1
    ok "Container running"

    local report line renderer
    report="$(capture_in_container '
        [ -n "${DISPLAY:-}" ] && { [ -S /tmp/.X11-unix/X${DISPLAY#:} ] && echo "X|ok" || echo "X|missing"; }
        R="$(glxinfo -B 2>/dev/null | sed -n "s/^OpenGL renderer string: //p")"
        V="$(glxinfo -B 2>/dev/null | sed -n "s/^OpenGL core profile version string: //p")"
        echo "GL|$R|$V"
        clinfo -l 2>/dev/null | sed -n "s/.*Device #[0-9]*: /CL|/p"
    ')"
    while IFS= read -r line; do
        case "$line" in
            GL\|*) renderer="$(echo "$line" | cut -d'|' -f2)"
                   if [ -z "$renderer" ]; then fail "OpenGL: no renderer (display not reachable)"
                   else
                       case "$renderer" in
                           *llvmpipe*) if [ "$SERVICE" = cpu ]; then ok "OpenGL: $renderer (software, as expected)"
                                       else warn "OpenGL: $renderer (software: the GPU was not reached)"; fi ;;
                           *) ok "OpenGL: $renderer, $(echo "$line" | cut -d'|' -f3)" ;;
                       esac
                       if [ "$SERVICE" = windows-wsl ]; then
                           case "$renderer" in *NVIDIA*) ;; *) hint "Not on the NVIDIA GPU: set MESA_D3D12_DEFAULT_ADAPTER_NAME in compose.yaml to part of its name." ;; esac
                       fi
                   fi ;;
            CL\|*) ok "OpenCL device: ${line#CL|}" ;;
            X\|ok) ok "Display socket mounted" ;;
            X\|missing) fail "Display socket missing in the container"
                case "$SERVICE" in
                    windows-wsl*) hint "In PowerShell: wsl --update; wsl --shutdown; restart Docker Desktop." ;;
                    linux-nvidia) hint "Check that /tmp/.X11-unix exists on the host and DISPLAY is set." ;;
                    cpu) hint "The browser desktop did not start: ./run.sh rebuild" ;;
                esac ;;
        esac
    done <<EOF
$report
EOF
    case "$SERVICE" in windows-wsl*|cpu) hint "OpenCL runs on the CPU here; Docker gives containers no GPU OpenCL on this setup." ;; esac
    if is_built; then ok "Project built in $BUILD_DIR"; else warn "Project not built yet (it builds on first run)"; fi
}

cmd_shell() {
    ensure_up || return 1
    info "Shell in ${B}$SERVICE${R}. Build: cmake --build $BUILD_DIR   Leave: exit"
    $WINPTY docker compose exec $TTY_FLAGS "$SERVICE" bash
}

cmd_rebuild() {
    ensure_docker || return 1
    info "Rebuilding the image and recreating ${B}$SERVICE${R}..."
    docker compose up -d --build --force-recreate "$SERVICE" || { up_failure_hint; return 1; }
    ok "Image rebuilt."
}

cmd_clean() {
    ensure_up || return 1
    in_container "rm -rf $BUILD_DIR" && ok "Removed $BUILD_DIR (next run rebuilds from scratch)."
}

cmd_stop() {
    ensure_docker || return 1
    docker compose down && ok "Containers stopped."
}

cmd_switch() {
    local svcs="windows-wsl windows-wsl-amd-intel linux-nvidia cpu" i=1 s choice
    printf '\n  %sChoose an environment%s\n' "$B" "$R"; rule
    for s in $svcs; do
        printf '   %s%s%s  %-22s %s%s%s\n' "$CYN$B" "$i" "$R" "$s" "$D" "$(service_label "$s")" "$R"
        i=$((i + 1))
    done
    printf '\n  %s%s%s number: ' "$CYN" "$S_ARROW" "$R"; read -r choice
    i=1
    for s in $svcs; do
        if [ "$choice" = "$i" ]; then SERVICE="$s"; SERVICE_WHY="chosen in the menu"; ok "Using $s."; return; fi
        i=$((i + 1))
    done
    warn "No change."
}

cmd_gallery() {
    run_exhibit "gallery" "compute-gallery" 1 || return
    if [ -z "${CG_KEEP_RUNNING:-}" ]; then
        info "Stopping the container to free its memory (CG_KEEP_RUNNING=1 keeps it running)..."
        docker compose stop "$SERVICE" >/dev/null 2>&1 && ok "Stopped."
    fi
}

cmd_help() {
    banner
    cat <<EOF

  ${B}Usage${R}
    ./run.sh                     open the gallery (starts and builds what it needs)
    ./run.sh menu                the text menu
    ./run.sh <command>           run one command and exit
    ./run.sh --service <name> …  use a specific environment (or set CG_SERVICE)
    run.cmd …                    same, from PowerShell or cmd on Windows

  ${B}Gallery${R}
    gallery        the graphical gallery; the container stops when it closes
                   (keep it running with CG_KEEP_RUNNING=1)

  ${B}Exhibits${R}
    bodies         thousands of textured spheres, instanced OpenGL
    test_opencl    OpenCL vs plain C on 11M floats, timed and verified
    grayscale_gpu  grayscale filter as an OpenCL kernel
    grayscale_cpu  the same filter in single-threaded C
    test_sdl       move an image around with SDL2

  ${B}Tools${R}
    build          compile everything (incremental)
    shell          bash shell inside the container
    doctor         check Docker, GPU, display and OpenCL, with fixes
    rebuild        rebuild the Docker image (after Dockerfile changes)
    clean          delete the container build directory
    stop           stop the containers

  ${B}Environments${R}
    windows-wsl            $(service_label windows-wsl)
    windows-wsl-amd-intel  $(service_label windows-wsl-amd-intel)
    linux-nvidia           $(service_label linux-nvidia)
    cpu                    $(service_label cpu)

  ${B}Troubleshooting${R}
    Docker not running        start Docker Desktop (or: sudo systemctl start docker)
    No window appears         ./run.sh doctor; on Windows: wsl --update, wsl --shutdown
    Window on the wrong GPU   MESA_D3D12_DEFAULT_ADAPTER_NAME in compose.yaml
    "nvidia" driver error     ./run.sh --service windows-wsl-amd-intel (Windows)
                              or install the NVIDIA Container Toolkit (Linux)
    Build errors after pulls  ./run.sh clean
    Dockerfile changed        ./run.sh rebuild
    OpenCL only shows a CPU   expected in Docker on Windows/macOS; build natively for GPU OpenCL
EOF
}

# ----------------------------------------------------------------------------
# Menu
# ----------------------------------------------------------------------------

#        key  name           path                  gui  description                                          controls
EXHIBITS="1|bodies|bodies/bodies|1|Thousands of textured spheres, instanced OpenGL|WASD/arrows orbit, numpad +/- zoom, Esc quits
2|test_opencl|test_opencl|0|OpenCL vs plain C on 11M floats, timed + verified|prints results, no input
3|grayscale_gpu|grayscale_gpu|1|Grayscale filter as an OpenCL kernel|Esc closes the image
4|grayscale_cpu|grayscale_cpu|1|The same filter in single-threaded C|Esc closes the image
5|test_sdl|test_sdl|1|Move an image around with SDL2|WASD/arrows move, close the window to quit"

run_by_key() {  # key or name -> runs exhibit and returns its status; 127 if nothing matched
    local query="$1"
    local key name path gui
    local found=0

    while IFS='|' read -r key name path gui _ _; do
        if [ "$query" = "$key" ] || [ "$query" = "$name" ]; then
            found=1
            break
        fi
    done <<EOF
$EXHIBITS
EOF

    [ "$found" -eq 1 ] || return 127

    # Execute OUTSIDE the redirected stdin loop.
    # Docker now inherits the original terminal.
    run_exhibit "$name" "$path" "$gui"
}

draw_menu() {
    [ -t 1 ] && printf '\033[H\033[2J'
    banner; printf '\n'

    local status build
    if [ "$DOCKER_STATE" != ok ]; then status="${RED}docker $DOCKER_STATE${R}"
    elif is_running; then status="${GRN}$S_RUN running${R}"
    else status="${D}$S_RUN stopped${R}"; fi
    if is_built; then build="${GRN}built${R}"; else build="${YLW}not built (builds on first run)${R}"; fi

    printf '  %-9s %s\n' "host" "$HOST_LABEL"
    printf '  %-9s %s\n' "gpu" "${GPU:-none detected}"
    printf '  %-9s %s%s%s  %s%s%s\n' "env" "$B" "$SERVICE" "$R" "$D" "$(service_label "$SERVICE")" "$R"
    printf '  %-9s %s   %s\n' "status" "$status" "$build"
    [ "$SERVICE" = cpu ] && printf '  %-9s %s\n' "windows" "$NOVNC_URL"
    printf '\n'; rule

    printf '   %sg%s  %s%-14s%s %s\n' "$CYN$B" "$R" "$B" "gallery" "$R" "Graphical launcher for all exhibits (recommended)"
    printf '      %-14s %s%s%s\n\n' "" "$D" "thumbnails, details, OpenCL device choice, live console" "$R"

    printf '  %sEXHIBITS%s\n' "$B" "$R"
    local key name path gui desc controls
    while IFS='|' read -r key name path gui desc controls; do
        printf '   %s%s%s  %s%-14s%s %s\n' "$CYN$B" "$key" "$R" "$B" "$name" "$R" "$desc"
        printf '      %-14s %s%s%s\n' "" "$D" "$controls" "$R"
    done <<EOF
$EXHIBITS
EOF
    printf '\n  %sTOOLS%s\n' "$B" "$R"
    printf '   %sb%s  build      %s   %ss%s  shell     %s\n' "$CYN$B" "$R" "compile everything  " "$CYN$B" "$R" "bash inside the container"
    printf '   %sd%s  doctor     %s   %se%s  env       %s\n' "$CYN$B" "$R" "diagnose + fixes    " "$CYN$B" "$R" "switch environment"
    printf '   %sr%s  rebuild    %s   %sc%s  clean     %s\n' "$CYN$B" "$R" "rebuild the image   " "$CYN$B" "$R" "delete the build"
    printf '   %sx%s  stop       %s   %sh%s  help      %s\n' "$CYN$B" "$R" "stop the container  " "$CYN$B" "$R" "usage + troubleshooting"
    printf '   %sq%s  quit\n' "$CYN$B" "$R"
    rule
}

dispatch() {   # returns 2 for "quit"
    local rc
    case "$1" in
        g|gallery) cmd_gallery ;;
        m|menu)    menu ;;
        b|build)   ensure_built && ok "Build up to date." ;;
        s|shell)   cmd_shell ;;
        d|doctor)  cmd_doctor ;;
        e|env)     cmd_switch ;;
        r|rebuild) cmd_rebuild ;;
        c|clean)   cmd_clean ;;
        x|stop)    cmd_stop ;;
        h|help|-h|--help) cmd_help ;;
        q|quit|exit) return 2 ;;
        "") return 0 ;;
        *) run_by_key "$1"; rc=$?
           [ "$rc" = 127 ] && { warn "Unknown option: $1  (./run.sh help lists them)"; return 1; }
           return "$rc" ;;
    esac
}

menu() {
    local choice rc
    while true; do
        draw_menu
        printf '  %s%s%s choose: ' "$CYN$B" "$S_ARROW" "$R"
        read -r choice || { printf '\n'; return 0; }
        [ -z "$choice" ] && continue
        printf '\n'
        trap ':' INT                # Ctrl+C stops the exhibit, not the menu
        dispatch "$choice"; rc=$?
        trap - INT
        [ "$rc" = 2 ] && return 0
        pause
    done
}

# ----------------------------------------------------------------------------
# Main
# ----------------------------------------------------------------------------

while [ $# -gt 0 ]; do
    case "$1" in
        --service) CG_SERVICE="${2:-}"; shift 2 ;;
        --service=*) CG_SERVICE="${1#*=}"; shift ;;
        *) break ;;
    esac
done

detect_host
detect_docker
pick_service

case "$SERVICE" in
    linux-nvidia|windows-wsl|windows-wsl-amd-intel|cpu) ;;
    *) fail "Unknown environment '$SERVICE'. Choose: linux-nvidia, windows-wsl, windows-wsl-amd-intel, cpu"; exit 1 ;;
esac

if [ $# -gt 0 ]; then
    dispatch "$1"; rc=$?
    [ "$rc" = 2 ] && rc=0
    exit "$rc"
fi

# No command: straight into the gallery.
cmd_gallery
