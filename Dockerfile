FROM ubuntu:24.04

ARG USERNAME=developer
ARG USER_UID=1000
ARG USER_GID=1000
ARG DEBIAN_FRONTEND=noninteractive

RUN set -eu; \
    existing_user="$(getent passwd "$USER_UID" | cut -d: -f1 || true)"; \
    if [ -n "$existing_user" ]; then userdel "$existing_user"; fi; \
    groupadd -g "$USER_GID" "$USERNAME"; \
    useradd -u "$USER_UID" -g "$USER_GID" -m "$USERNAME"; \
    apt-get update; \
    apt-get install -y sudo python3-pip; \
    printf '%s ALL=(root) NOPASSWD:ALL\n' "$USERNAME" > "/etc/sudoers.d/$USERNAME"; \
    chmod 0440 "/etc/sudoers.d/$USERNAME"; \
    mkdir -p /commandhistory; \
    touch /commandhistory/.bash_history; \
    chown -R "$USERNAME" /commandhistory; \
    echo "export PROMPT_COMMAND='history -a' && export HISTFILE=/commandhistory/.bash_history" \
        >> "/home/$USERNAME/.bashrc"; \
    rm -rf /var/lib/apt/lists/*

ENV SHELL=/bin/bash

# Install packages
RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    lsof \
    wget \
    sudo \
    python3 \
    python3-jinja2 \
    python3-pip \
    python3-argcomplete \
    python3-websockify \
    gdb \
    gdbserver \
    valgrind \
    software-properties-common \
    build-essential \
    cmake \
    git \
    tzdata \
    doxygen \
    libglm-dev \
    clinfo \
    ocl-icd-libopencl1 \
    ocl-icd-opencl-dev \
    pocl-opencl-icd \
    opencl-headers \
    libsdl2-dev \
    libsdl2-image-dev \
    freeglut3-dev \
    ninja-build \
    libx11-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev \
    libgl1-mesa-dev \
    libstb-dev \
    mesa-utils \
    libwayland-dev \
    libxkbcommon-dev \
    wayland-protocols \
    tigervnc-standalone-server \
    novnc \
    fluxbox \
    && rm -rf /var/lib/apt/lists/*

# Browser desktop for hosts without a display the container can reach (see compose.yaml).
COPY docker/start-desktop.sh /usr/local/bin/start-desktop
RUN sed -i 's/\r$//' /usr/local/bin/start-desktop && chmod 755 /usr/local/bin/start-desktop

# The NVIDIA container runtime injects libnvidia-opencl.so.1 on Linux hosts but does
# not register it with the ICD loader. Harmless where the library is absent (the
# loader skips it), e.g. on Docker Desktop for Windows, which ships no NVIDIA OpenCL.
RUN mkdir -p /etc/OpenCL/vendors \
    && echo "libnvidia-opencl.so.1" > /etc/OpenCL/vendors/nvidia.icd
ENV NVIDIA_DRIVER_CAPABILITIES=compute,utility,graphics,display

# Private runtime dir. Without one GLFW/SDL warn, and on WSLg pointing at its Wayland
# socket gives windows without a title bar; this way they use X11 and get decorations.
ENV XDG_RUNTIME_DIR=/tmp/runtime-${USERNAME}
RUN install -d -m 700 -o ${USER_UID} -g ${USER_GID} /tmp/runtime-${USERNAME}

USER $USERNAME
ENV USERNAME=${USERNAME}
WORKDIR /home/workspace/

CMD ["/bin/bash"]