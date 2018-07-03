#!/bin/bash

if [ $# -eq 0 ]; then
	target="`pwd`"
else
	target="$1"
fi

wdir="`dirname \"$0\"`"
enigma2_base="$target/meta-openvuplus/recipes-vuplus/enigma2"

echo Copying files..

mkdir "$enigma2_base/enigma2-unicablesrv" && \
cp "$wdir/unicablesrv/enigma2-unicablesrv.bb" "$enigma2_base" && \
cp "$wdir/unicablesrv/unicablesrv.c" "$enigma2_base/enigma2-unicablesrv" && \
cp "$wdir/patch/enigma2_vuplus_dynamic_unicable.patch" "$enigma2_base/enigma2"

if [ $? -ne 0 ]; then
	echo "Copying files failed."
	exit 1
fi

echo Patching files..

patch -p1 -d "$target" << _EOF
diff --git a/meta-openvuplus/recipes-vuplus/enigma2/enigma2.bb b/meta-openvuplus/recipes-vuplus/enigma2/enigma2.bb
index 879c00f..5710f9a 100644
--- a/meta-openvuplus/recipes-vuplus/enigma2/enigma2.bb
+++ b/meta-openvuplus/recipes-vuplus/enigma2/enigma2.bb
@@ -256,6 +256,7 @@ SRC_URI = "git://code.vuplus.com/git/dvbapp.git;protocol=http;branch=${BRANCH};r
         file://enigma2_vuplus_fix_standby_name.patch \\
 	file://enigma2_vuplus_disable_subtitle_sync_mode_bug.patch \\
 	file://enigma2_vuplus_networksetup_update_ifaces.patch \\
+	file://enigma2_vuplus_dynamic_unicable.patch \\
 	file://spinner \\
 	file://number_key \\
 "
diff --git a/meta-openvuplus/recipes-vuplus/packagegroups/packagegroup-vuplus-enigma2.bb b/meta-openvuplus/recipes-vuplus/packagegroups/packagegroup-vuplus-enigma2.bb
index 8251abd..d078e50 100644
--- a/meta-openvuplus/recipes-vuplus/packagegroups/packagegroup-vuplus-enigma2.bb
+++ b/meta-openvuplus/recipes-vuplus/packagegroups/packagegroup-vuplus-enigma2.bb
@@ -14,6 +14,7 @@ RDEPENDS_${PN} += " \\
   enigma2-streamproxy \\
   tuxbox-tuxtxt-32bpp \\
   showiframe \\
+  enigma2-unicablesrv \\
   enigma2-meta \\
   enigma2-plugins-meta \\
   enigma2-skins-meta \\

_EOF

if [ $? -ne 0 ]; then
	echo "Patching files failed."
	exit 1
fi

exit 0