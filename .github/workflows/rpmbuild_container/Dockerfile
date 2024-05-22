#
#  Copyright (c) 2015 - 2024, Intel Corporation
#  SPDX-License-Identifier: BSD-3-Clause
#
ARG BUILDER_CONTAINER_IMAGE=docker.io/opensuse/leap:15.4
FROM ${BUILDER_CONTAINER_IMAGE}
RUN zypper --non-interactive install \
            bash-completion-devel \
            curl \
            doxygen \
            fdupes \
            gcc-c++ \
            git \
            gobject-introspection \
            graphviz \
            gzip \
            libcap-devel \
            libelf-devel \
            libtool \
            make \
            python3 \
            python3-cffi \
            createrepo_c \
            python3-devel \
            python3-gobject \
            python3-jsonschema \
            python3-numpy \
            python3-pip \
            python3-psutil \
            python3-setuptools \
            rpm-build \
            systemd-devel \
            systemd-rpm-macros \
            unzip \
            wget \
            zlib-devel
RUN pip install --upgrade pip && pip install --upgrade build wheel setuptools_scm

# Tell rpmbuild where to save its output. This path is expected to be a
# writable mount to wherever you want the build output to be saved.
RUN echo '%_topdir /geopm_rpmbuild' >> ~/.rpmmacros
# RUN wget https://github.com/dasbus-project/dasbus/archive/refs/tags/v1.6.tar.gz -O dasbus-1.6.tar.gz \
#     && tar zxf /dasbus-1.6.tar.gz -C / && mkdir -p /dasbus_rpmbuild/SOURCES \
#     && mv /dasbus-1.6.tar.gz /dasbus_rpmbuild/SOURCES/dasbus-1.6.tar.gz \
#     && rpmbuild --define '_topdir /dasbus_rpmbuild' -ba /dasbus-1.6/python-dasbus.spec \
#     && zypper --non-interactive install --allow-unsigned-rpm /dasbus_rpmbuild/RPMS/noarch/python3-dasbus-1.6-1.noarch.rpm
COPY package_component.sh /

ENTRYPOINT ["/package_component.sh"]
