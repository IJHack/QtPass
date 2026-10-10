# SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Build image for Anthropic's OSS Scanner (https://github.com/anthropics/oss-scanner).
# The scanner builds this with the repository as context; network access ends
# after this build, so everything the tests need is installed here.
# QtPass 2.x needs Qt 6.8 or newer: Debian 13 (trixie) ships 6.8.
FROM debian:trixie
ENV DEBIAN_FRONTEND=noninteractive \
    QT_QPA_PLATFORM=offscreen
RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates build-essential pkg-config git python3 \
      qmake6 qt6-base-dev qt6-base-dev-tools qt6-svg-dev qt6-tools-dev \
      qt6-tools-dev-tools qt6-l10n-tools libgl-dev \
      gnupg gpg-agent pass pwgen qrencode \
    && rm -rf /var/lib/apt/lists/*

# scanner contract: the checkout lives inside the image, at /src
COPY . /src
WORKDIR /src

# A debug build with frame pointers, as the scanner asks; a failing build
# fails the image.
RUN qmake6 -r CONFIG+=debug \
      QMAKE_CXXFLAGS+="-g -fno-omit-frame-pointer" \
    && make -j"$(nproc)"

# The unit and integration tests (offscreen, each with its own GNUPGHOME),
# as an unprivileged user: QtPass is not meant to run as root, and root
# ignores the permission and path failures some tests set up. -k runs every
# suite; a failing test is reported but does not abort the image.
RUN useradd --create-home --uid 1000 qtpass && chown -R qtpass:qtpass /src
USER qtpass
RUN make -k check || echo "WARNING: make check reported failures"
