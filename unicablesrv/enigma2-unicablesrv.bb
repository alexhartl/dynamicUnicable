DESCRIPTION = "Dynamic Unicable Server"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/GPL-2.0;md5=801f80980d171dd6425610833a22dbe6"

SRC_URI = "file://unicablesrv.c"


do_compile() {
	${CC} -O2 -o unicablesrv ${WORKDIR}/unicablesrv.c
}

do_install() {
	install -d ${D}/usr/bin
	install -m 0755 ${S}/unicablesrv ${D}/${bindir}
}

FILES_${PN} = "${bindir}/unicablesrv"