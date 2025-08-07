# Copyright (C) 2017 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.

LOCAL_PATH := $(call my-dir)

ifeq ($(notdir $(LOCAL_PATH)),$(strip $(LINUX_KERNEL_VERSION)))
ifneq ($(strip $(TARGET_NO_KERNEL)),true)
include $(LOCAL_PATH)/kenv.mk

ifeq ($(wildcard $(TARGET_PREBUILT_KERNEL)),)
# .config cannot be PHONY due to config_data.gz
$(TARGET_KERNEL_CONFIG): $(KERNEL_CONFIG_FILE) $(LOCAL_PATH)/Android.mk
$(TARGET_KERNEL_CONFIG): $(shell find $(KERNEL_DIR) -name "Kconfig*") | $(KERNEL_OUT)
	echo "Generating a .config file for header generation"
	$(hide) mkdir -p $(dir $@)
	$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) $(KERNEL_DEFCONFIG)

$(KERNEL_MODULES_DEPS): $(KERNEL_ZIMAGE_OUT) ;
$(BUILT_DTB_OVERLAY_TARGET): $(KERNEL_ZIMAGE_OUT)

.KATI_RESTAT: $(KERNEL_ZIMAGE_OUT)
$(KERNEL_ZIMAGE_OUT): $(TARGET_KERNEL_CONFIG) FORCE | $(KERNEL_OUT)
	$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION)
	$(hide) $(call fixup-kernel-cmd-file,$(KERNEL_OUT)/arch/$(KERNEL_TARGET_ARCH)/boot/compressed/.piggy.xzkern.cmd)
ifneq ($(KERNEL_CONFIG_MODULES),)
	#$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) INSTALL_MOD_PATH=$(KERNEL_MODULES_SYMBOLS_OUT) modules_install
	#$(hide) $(call move-kernel-module-files,$(KERNEL_MODULES_SYMBOLS_OUT),$(KERNEL_OUT))
	#$(hide) $(call clean-kernel-module-dirs,$(KERNEL_MODULES_SYMBOLS_OUT),$(KERNEL_OUT))
	#$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) INSTALL_MOD_PATH=$(KERNEL_MODULES_OUT) modules_install
	#$(hide) $(call move-kernel-module-files,$(KERNEL_MODULES_OUT),$(KERNEL_OUT))
	#$(hide) $(call clean-kernel-module-dirs,$(KERNEL_MODULES_OUT),$(KERNEL_OUT))
endif

ifeq ($(strip $(MTK_HEADER_SUPPORT)), yes)
$(BUILT_KERNEL_TARGET): $(KERNEL_ZIMAGE_OUT) $(LOCAL_PATH)/Android.mk $(KERNEL_HEADERS_INSTALL) | $(HOST_OUT_EXECUTABLES)/mkimage$(HOST_EXECUTABLE_SUFFIX)
	$(hide) $(HOST_OUT_EXECUTABLES)/mkimage$(HOST_EXECUTABLE_SUFFIX) $< KERNEL 0xffffffff > $@
else
$(BUILT_KERNEL_TARGET): $(KERNEL_ZIMAGE_OUT) $(LOCAL_PATH)/Android.mk $(KERNEL_HEADERS_INSTALL) | $(ACP)
	$(copy-file-to-target)
endif

$(TARGET_PREBUILT_KERNEL): $(BUILT_KERNEL_TARGET) $(LOCAL_PATH)/Android.mk | $(ACP)
	$(copy-file-to-new-target)

endif#TARGET_PREBUILT_KERNEL is empty

$(INSTALLED_KERNEL_TARGET): $(BUILT_KERNEL_TARGET) $(LOCAL_PATH)/Android.mk | $(ACP)
	$(copy-file-to-target)

ifneq ($(KERNEL_CONFIG_MODULES),)
$(BUILT_SYSTEMIMAGE): $(KERNEL_MODULES_DEPS)
endif

.PHONY: kernel save-kernel kernel-savedefconfig %config-kernel clean-kernel odmdtboimage

KERNEL_HEADER_DEFCONFIG := $(strip $(KERNEL_HEADER_DEFCONFIG))
ifeq ($(KERNEL_HEADER_DEFCONFIG),)
KERNEL_HEADER_DEFCONFIG := $(KERNEL_DEFCONFIG)
endif
KERNEL_CONFIG := $(TARGET_KERNEL_CONFIG)

kernel: $(INSTALLED_KERNEL_TARGET)
save-kernel: $(TARGET_PREBUILT_KERNEL)

kernel-savedefconfig: $(TARGET_KERNEL_CONFIG)
	cp $(TARGET_KERNEL_CONFIG) $(KERNEL_CONFIG_FILE)

kernel-menuconfig: | $(KERNEL_OUT)
	$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) menuconfig

%config-kernel: | $(KERNEL_OUT)
	$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) $(patsubst %config-kernel,%config,$@)

clean-kernel:
	$(hide) rm -rf $(KERNEL_OUT) $(KERNEL_MODULES_OUT) $(INSTALLED_KERNEL_TARGET)
	$(hide) rm -f $(INSTALLED_DTB_OVERLAY_TARGET)


$(KERNEL_OUT):
	$(hide) mkdir -p $@

$(KERNEL_HEADERS_TIMESTAMP) : $(KERNEL_HEADERS_INSTALL)
$(KERNEL_HEADERS_INSTALL) : $(TARGET_KERNEL_CONFIG) | $(KERNEL_OUT)
	$(hide) if [ ! -z "$(KERNEL_HEADER_DEFCONFIG)" ]; then \
				rm -f ../$(KERNEL_CONFIG); \
				$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) $(KERNEL_HEADER_DEFCONFIG) headers_install; fi
	$(hide) if [ "$(KERNEL_HEADER_DEFCONFIG)" != "$(KERNEL_DEFCONFIG)" ]; then \
				echo "Used a different defconfig for header generation"; \
				rm -f ../$(KERNEL_CONFIG); \
				$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) $(KERNEL_DEFCONFIG); fi
	$(hide) if [ ! -z "$(KERNEL_CONFIG_OVERRIDE)" ]; then \
				echo "Overriding kernel config with '$(KERNEL_CONFIG_OVERRIDE)'"; \
				echo $(KERNEL_CONFIG_OVERRIDE) >> $(TARGET_KERNEL_CONFIG); \
				$(MAKE) -C $(KERNEL_DIR) $(KERNEL_MAKE_OPTION) oldconfig; fi
	$(hide) touch $@/build-timestamp


.PHONY: check-kernel-config check-kernel-dotconfig
droid: check-kernel-config check-kernel-dotconfig
check-mtk-config: check-kernel-config check-kernel-dotconfig
check-kernel-config: PRIVATE_COMMAND := $(if $(wildcard device/mediatek/build/build/tools/check_kernel_config.py),$(if $(filter yes,$(DISABLE_MTK_CONFIG_CHECK)),-)python device/mediatek/build/build/tools/check_kernel_config.py -c $(MTK_TARGET_PROJECT_FOLDER)/ProjectConfig.mk -k $(KERNEL_CONFIG_FILE) -p $(MTK_PROJECT_NAME))
check-kernel-config:
	$(PRIVATE_COMMAND)

ifneq ($(filter check-mtk-config check-kernel-dotconfig,$(MAKECMDGOALS)),)
.PHONY: $(TARGET_KERNEL_CONFIG)
endif
check-kernel-dotconfig: PRIVATE_COMMAND := $(if $(wildcard device/mediatek/build/build/tools/check_kernel_config.py),$(if $(filter yes,$(DISABLE_MTK_CONFIG_CHECK)),-)python device/mediatek/build/build/tools/check_kernel_config.py -c $(MTK_TARGET_PROJECT_FOLDER)/ProjectConfig.mk -k $(TARGET_KERNEL_CONFIG) -p $(MTK_PROJECT_NAME))
check-kernel-dotconfig: $(TARGET_KERNEL_CONFIG)
	$(PRIVATE_COMMAND)


endif#TARGET_NO_KERNEL
endif#LINUX_KERNEL_VERSION
