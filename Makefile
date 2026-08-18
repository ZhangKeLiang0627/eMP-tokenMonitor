#
# Makefile
#

# 使用方式：
# 本地编译：make CROSS=0 -j32
# 交叉编译：make CROSS=1 -j32

# 编译模式控制宏，默认本地编译
CROSS ?= 0

# ==============================================================================
# 交叉编译分支 (CROSS=1)
# ==============================================================================
ifeq ($(CROSS), 1)
    # 工具链路径：默认指向本地 tina-sdk；设置 T113_SDK=<工具链仓库根> 可切换
    # （例如从 https://github.com/ZhangKeLiang0627/eMP-toolchain 拉取后：
    #   export T113_SDK=/path/to/eMP-toolchain
    #   export STAGING_DIR=$(T113_SDK)/sysroot
    #   make CROSS=1 -j32）
    ifdef T113_SDK
        TOOLCHAIN_DIR = $(T113_SDK)/toolchain/bin/
        SYSROOT_DIR  = $(T113_SDK)/sysroot
        FREETYPE_INC = $(SYSROOT_DIR)/usr/include/freetype2
    else
        TOOLCHAIN_DIR = /home/hugokkl/tina-sdk/prebuilt/gcc/linux-x86/arm/toolchain-sunxi-musl/toolchain/bin/
        SYSROOT_DIR  = /home/hugokkl/tina-sdk/out/t113-pi/staging_dir/target
        FREETYPE_INC = /home/hugokkl/tina-sdk/out/t113-pi/compile_dir/target/freetype-2.13.2/include
    endif

    # 编译器设置
    CC = $(TOOLCHAIN_DIR)arm-openwrt-linux-gcc
    CXX = $(TOOLCHAIN_DIR)arm-openwrt-linux-g++

    # 可执行文件名
    BIN = eMP_tokenMonitor

    # 路径设置
    LVGL_DIR_NAME ?= lvgl
    LVGL_DIR ?= ./libs
    BUILD_DIR = ./build
    PROJECT_DIR ?= ${shell pwd}

    # 编译选项
    CFLAGS ?= -O3 -g0 -I$(LVGL_DIR)/ -Wall -Wno-unused-function -Wno-unused-variable -Wno-return-type -Wno-sign-compare -Wno-memset-transposed-args 
    LDFLAGS ?= -lm
    CXXFLAGS ?= $(CFLAGS) -std=c++17

    # httplib 开启 OpenSSL（HTTPS 请求需要）
    CFLAGS += -DCPPHTTPLIB_OPENSSL_SUPPORT

    # lodepng 截图：C++ 中只声明、不内联编译实现（实现由 lvgl 的 lodepng.c 提供）
    CFLAGS += -DLODEPNG_NO_COMPILE_CPP

    # 头文件路径
    CFLAGS += -I$(SYSROOT_DIR)/usr/include
    CFLAGS += -I$(SYSROOT_DIR)/usr/include/allwinner
    CFLAGS += -I$(SYSROOT_DIR)/usr/include/allwinner/include 
    CFLAGS += -I$(PROJECT_DIR)/inc
    CFLAGS += -I$(PROJECT_DIR)/utils
    CFLAGS += -I$(PROJECT_DIR)/libs/cpp-httplib
    CFLAGS += -I$(FREETYPE_INC)
    CFLAGS += -I$(PROJECT_DIR)/libs/spdlog/include

    # 架构相关参数
    CFLAGS += -pipe -march=armv7-a -mtune=cortex-a7 -mfpu=neon -mfloat-abi=hard -fstack-protector  

    # 链接选项
    LDFLAGS += -L$(SYSROOT_DIR)/lib
    LDFLAGS += -L$(SYSROOT_DIR)/usr/lib  
    LDFLAGS += -lpthread -lstdc++ -lfreetype -lstdc++fs -lssl -lcrypto -lz -lbz2

    # 源文件收集
    MAINSRC += ./main.cpp
    CSRCS += $(shell find -L $(PROJECT_DIR)/src -name "*.c")
    CXXSRCS += $(shell find -L $(PROJECT_DIR)/src -name "*.cpp")
    CSRCS += $(shell find -L $(PROJECT_DIR)/utils -name "*.c")
    CXXSRCS += $(shell find -L $(PROJECT_DIR)/utils -name "*.cpp")

    # 包含LVGL的Makefile
    include $(LVGL_DIR)/lvgl/lvgl.mk
    include $(LVGL_DIR)/lv_drivers/lv_drivers.mk

    # 目标文件处理
    OBJEXT ?= .o
    AOBJS = $(ASRCS:.S=$(OBJEXT))
    COBJS = $(CSRCS:.c=$(OBJEXT))
    CXXOBJS = $(CXXSRCS:.cpp=$(OBJEXT))
    MAINOBJ = $(MAINSRC:.cpp=$(OBJEXT))
    SRCS = $(ASRCS) $(CSRCS) $(CXXSRCS) $(MAINSRC)
    OBJS = $(AOBJS) $(COBJS) $(CXXOBJS) $(MAINOBJ)

# ==============================================================================
# 本地编译分支 (CROSS=0)
# ==============================================================================
else
    # 编译器设置
    CC = gcc
    CXX = g++

    # 可执行文件名
    BIN = eMP_tokenMonitor

    # 路径设置
    LVGL_DIR_NAME ?= lvgl
    LVGL_DIR ?= ./libs
    PROJECT_DIR ?= ${shell pwd}

    # 编译选项
    CFLAGS ?= -O3 -g -I$(LVGL_DIR)/ -Wall -Wno-unused-function -Wno-unused-variable
	CFLAGS += -DLV_USE_DRAW_SDL=0 -DUSE_SDL=1 -DUSE_MOUSE=1
    CXXFLAGS ?= $(CFLAGS) -std=c++17

    # httplib 开启 OpenSSL（HTTPS 请求需要）
    CFLAGS += -DCPPHTTPLIB_OPENSSL_SUPPORT

    # lodepng 截图：C++ 中只声明、不内联编译实现（实现由 lvgl 的 lodepng.c 提供）
    CFLAGS += -DLODEPNG_NO_COMPILE_CPP

    # 头文件路径
    CFLAGS += -I$(PROJECT_DIR)/inc
    CFLAGS += -I$(PROJECT_DIR)/utils
    CFLAGS += -I$(PROJECT_DIR)/libs/cpp-httplib
    CFLAGS += -I$(PROJECT_DIR)/libs/spdlog/include
    CFLAGS += -I/usr/include/freetype2
    CFLAGS += $(shell pkg-config --cflags sdl2)

    # 链接选项
    LDFLAGS ?= -lm
    LDFLAGS += -lpthread -lstdc++ 
    LDFLAGS += $(shell pkg-config --libs sdl2)
    LDFLAGS += -lfreetype -lncurses -lstdc++fs -lssl -lcrypto

    # 源文件收集
    MAINSRC += ./main.cpp
    CSRCS += $(shell find -L $(PROJECT_DIR)/src -name "*.c")
    CXXSRCS += $(shell find -L $(PROJECT_DIR)/src -name "*.cpp")
    CSRCS += $(shell find -L $(PROJECT_DIR)/utils -name "*.c")
    CXXSRCS += $(shell find -L $(PROJECT_DIR)/utils -name "*.cpp")

    # 包含LVGL的Makefile
    include $(LVGL_DIR)/lvgl/lvgl.mk
    include $(LVGL_DIR)/lv_drivers/lv_drivers.mk

    # 目标文件处理
    OBJEXT ?= .o
    AOBJS = $(ASRCS:.S=$(OBJEXT))
    COBJS = $(CSRCS:.c=$(OBJEXT))
    CXXOBJS = $(CXXSRCS:.cpp=$(OBJEXT))
    MAINOBJ = $(MAINSRC:.cpp=$(OBJEXT))
    SRCS = $(ASRCS) $(CSRCS) $(CXXSRCS) $(MAINSRC)
    OBJS = $(AOBJS) $(COBJS) $(CXXOBJS) $(MAINOBJ)

    # 本地编译的BUILD_DIR定义（与交叉编译保持一致）
    BUILD_DIR = ./build
endif

# ==============================================================================
# 公共编译规则
# ==============================================================================
.PHONY: clean all

all: default

# C文件编译规则
%.o: %.c
	@$(CC)  $(CFLAGS) -c $< -o $@
	@echo "CC $<"

# C++文件编译规则
%.o: %.cpp
	@$(CXX)  $(CXXFLAGS) -c $< -o $@
	@echo "CXX $<"
    
# 链接生成可执行文件
default: $(OBJS)
	$(CXX) -o $(BIN) $(MAINOBJ) $(AOBJS) $(COBJS) $(CXXOBJS) $(LDFLAGS)

# 清理目标文件
clean: 
	rm -f $(BIN) $(AOBJS) $(COBJS) $(MAINOBJ) $(CXXOBJS)
	rm -r $(BUILD_DIR)
