#!/bin/bash

#
# This file is part of Debian packaging for kopanocore.
# Copyright (c) 2017 Carsten Schoenert
#
# SPDX-License-Identifier: AGPL-3
#

BINARY_PACKAGES=$(grep "Package: " debian/control | awk '{print $2;}' | tr '\n' ' ')
PACKAGED_LIBRARIES=$(for ITEM in ${BINARY_PACKAGES}; do find debian/"${ITEM}"/usr/ -type f -name "*.so"; done)
HOST_ARCH=$(dpkg-architecture -qDEB_HOST_ARCH)
HOST_MULTIARCH=$(dpkg-architecture -qDEB_HOST_MULTIARCH)

echo "     host architecture: ${HOST_ARCH}"
echo "multiarch architecture: ${HOST_MULTIARCH}"
echo

echo "search scope: ${BINARY_PACKAGES}"
echo

FOUND=0
NOTFOUND=0
LOOP=1
UPSTREAM_LIBS_FILE=$(find debian/tmp/usr/lib -type f -name "*.so")
UPSTREAM_LIBS_FILE_COUNT=$(find debian/tmp/usr/lib -type f -name "*.so" | wc -l)

# checking upstream installed libraries against packaged libraries
echo "###############################################"
echo "# checking *.so files installed into packages #"
echo "###############################################"
if [ "${UPSTREAM_LIBS_FILE}" != "" ]; then
    #echo ${UPSTREAM_LIBS_FILE}
    for LIB in ${UPSTREAM_LIBS_FILE}; do
        # just take the file name
        UPSTREAM_ACTUAL_LIBRARY_FILE="${LIB##*\/}"
        # pick up only the folder name
        UPSTREAM_INSTALL_FOLDER="${LIB%\/*}"
        echo "[${LOOP}/${UPSTREAM_LIBS_FILE_COUNT}] processing ${UPSTREAM_ACTUAL_LIBRARY_FILE}    (from ${UPSTREAM_INSTALL_FOLDER})"
        # now try to find *.so file in the Debian packages
        for DEBIAN_PACKAGE in ${BINARY_PACKAGES}; do
            #echo "trying in ${DEBIAN_PACKAGE}"
            SO=$(find debian/"${DEBIAN_PACKAGE}"/usr/ -type f -name "${UPSTREAM_ACTUAL_LIBRARY_FILE}")
            # just take the file name from Debian packaging
            SO_FILE="${SO##*\/}"
            # split up the folder information within the packaging
            SO_FOLDER="${SO%\/*}"
            if [ "${UPSTREAM_ACTUAL_LIBRARY_FILE}" = "${SO_FILE}" ]; then
                echo "found in package ${DEBIAN_PACKAGE}"
                FOUND=1
            fi
        done
        if [ ${FOUND} != "1" ]; then
            echo "${UPSTREAM_ACTUAL_LIBRARY_FILE} could not be found in any Debian package!"
            NOTFOUND=1
        fi
        echo
        LOOP=$((${LOOP}+1))
    done
    if [ ${NOTFOUND} != "1" ]; then
        echo "All *.so files from debian/tmp/usr/lib/* found in the packages."
		echo
    fi
else
    echo "No list of upstream libraries given!"
    exit 1
fi

FOUND=0
NOTFOUND=0
LOOP=1
UPSTREAM_LIBS_LINK=$(find debian/tmp/usr/lib -type l -name "*.so*")
UPSTREAM_LIBS_LINK_COUNT=$(find debian/tmp/usr/lib -type l -name "*.so*" | wc -l)
echo
# checking upstream installed links against packaged libraries
echo "###############################################"
echo "# checking *.so links installed into packages #"
echo "###############################################"
if [ "${UPSTREAM_LIBS_LINK}" != "" ]; then
    #echo ${UPSTREAM_LIBS_LINK}
    for LIB in ${UPSTREAM_LIBS_LINK}; do
        # just take the file name
        UPSTREAM_ACTUAL_LIBRARY_LINK="${LIB##*\/}"
        # pick up only the folder name
        UPSTREAM_INSTALL_FOLDER="${LIB%\/*}"
        echo "[${LOOP}/${UPSTREAM_LIBS_LINK_COUNT}] processing ${UPSTREAM_ACTUAL_LIBRARY_LINK}    (from ${UPSTREAM_INSTALL_FOLDER})"
        # now try to find *.so file in the Debian packages
        for DEBIAN_PACKAGE in ${BINARY_PACKAGES}; do
            #echo "trying in ${DEBIAN_PACKAGE}"
            SO=$(find debian/"${DEBIAN_PACKAGE}"/usr/ -type l -name "${UPSTREAM_ACTUAL_LIBRARY_LINK}")
            # just take the file name from Debian packaging
            SO_FILE="${SO##*\/}"
            # split up the folder information within the packaging
            SO_FOLDER="${SO%\/*}"
            if [ "${UPSTREAM_ACTUAL_LIBRARY_LINK}" = "${SO_FILE}" ]; then
                echo "found in package ${DEBIAN_PACKAGE}"
                FOUND=1
            fi
        done
        if [ ${FOUND} != "1" ]; then
            echo "${UPSTREAM_ACTUAL_LIBRARY_LINK} could not be found in any Debian package!"
            NOTFOUND=1
        fi
        echo
        LOOP=$((${LOOP}+1))
    done
    if [ ${NOTFOUND} != "1" ]; then
        echo "All *.so* links from debian/tmp/usr/lib/* found in the packages."
        echo
    fi
else
    echo "No list of upstream libraries given!"
    exit 1
fi

FOUND=0
NOTFOUND=0
LOOP=1
UPSTREAM_FILE=$(find debian/tmp/ -path tmp/misc -prune -o -name '*.*' -print)
UPSTREAM_FILE_COUNT=$(find debian/tmp/ -path tmp/misc -prune -o -name '*.*' -print | wc -l)
echo
# checking upstream installed files against packaged libraries
echo "###############################################"
echo "# checking *.* files installed into packages  #"
echo "###############################################"
if [ "${UPSTREAM_FILE}" != "" ]; then
    #echo ${UPSTREAM_FILE}
    for FILE in ${UPSTREAM_FILE}; do
        # just take the file name
        UPSTREAM_ACTUAL_FILE="${FILE##*\/}"
        # pick up only the folder name
        UPSTREAM_INSTALL_FOLDER="${FILE%\/*}"
        echo "[${LOOP}/${UPSTREAM_FILE_COUNT}] processing ${UPSTREAM_ACTUAL_FILE}    (from ${UPSTREAM_INSTALL_FOLDER})"
        # now try to find *.so file in the Debian packages
        for DEBIAN_PACKAGE in ${BINARY_PACKAGES}; do
            #echo "trying in ${DEBIAN_PACKAGE}"
            EXTENSION=$(find debian/"${DEBIAN_PACKAGE}"/usr/ -type f -name "${UPSTREAM_ACTUAL_FILE}")
            # just take the file name from Debian packaging
            EXTENSION_FILE="${EXTENSION##*\/}"
            # split up the folder information within the packaging
            EXTENSION_FOLDER="${EXTENSION%\/*}"
            if [ "${UPSTREAM_ACTUAL_FILE}" = "${EXTENSION_FILE}" ]; then
                echo "found in package ${DEBIAN_PACKAGE}"
                FOUND=1
            fi
        done
        if [ ${FOUND} != "1" ]; then
            echo "*** ${UPSTREAM_ACTUAL_FILE} could not be found in any Debian package!"
            NOTFOUND=1
        fi
        echo
        LOOP=$((${LOOP}+1))
    done
    if [ ${NOTFOUND} != "1" ]; then
        echo "All *.* files from debian/tmp/* found in the packages."
        echo
    fi
else
    echo "No list of upstream files given!"
    exit 1
fi




exit
