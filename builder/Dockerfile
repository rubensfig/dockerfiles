FROM fedora:30
WORKDIR /workdir
LABEL maintainer="rubens@bisdn.de"

RUN dnf install -y git clang make meson ninja-build dnf-plugins-core gettext findutils
CMD ["/usr/bin/bash"]
