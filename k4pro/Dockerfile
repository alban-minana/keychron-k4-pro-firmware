# Build image: official QMK toolchain + the Python deps of this (2023-era) QMK tree.
FROM ghcr.io/qmk/qmk_cli@sha256:b7d7fa8fb4432b569931de5ad59098cb788f440ed61a62c5126746b71aee0f4a
COPY requirements.txt /tmp/requirements.txt
RUN /opt/uv/tools/qmk/bin/python3 -m pip install -r /tmp/requirements.txt \
 || (/opt/uv/tools/qmk/bin/python3 -m ensurepip && /opt/uv/tools/qmk/bin/python3 -m pip install -r /tmp/requirements.txt)
