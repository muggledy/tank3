#gcc freeglut makefile
#####################################################
target :=PainterEngineEXE
project_path := ./src
painterengine_path := ./PainterEngine
#####################################################
#project_build := $(wildcard $(project_path)/*.c)
# 使用 shell 命令递归查找 src 目录下的所有 .c 文件
project_build := $(shell find $(project_path) -name "*.c")
project_build_cpp = $(wildcard $(project_path)/*.cpp)

# 定义要排除的模式(暂不关注.cpp)
#exclude_pattern := $(project_path)/not_make_*.c
# 打印排除模式和待编译文件列表
$(info Project build: $(project_build))
#$(info Exclude pattern: $(exclude_pattern))
# 过滤掉符合排除模式的文件
#project_build := $(filter-out $(wildcard $(exclude_pattern)), $(project_build))

# 使用 find 命令结合 shell 函数来实现递归查找和过滤
exclude_files := $(shell find $(project_path) -name "not_make_*.c")
project_build := $(filter-out $(exclude_files), $(project_build))

# 打印过滤后的待编译文件列表
$(info Filtered project build: $(project_build))

LDFLAGS = -lm  # 添加链接数学库的选项

project_build_o := $(patsubst %.c,%.o,$(project_build))
project_build_o += $(patsubst %.cpp,%.o,$(project_build_cpp))

$(info Filtered project build .o: $(project_build_o))

painterengine_build_core := $(wildcard $(painterengine_path)/core/*.c)
painterengine_build_painterengine_o := $(patsubst %.c,%.o,$(painterengine_build_core))

painterengine_build_kernel := $(wildcard $(painterengine_path)/kernel/*.c)
painterengine_build_painterengine_o += $(patsubst %.c,%.o,$(painterengine_build_kernel))

painterengine_build_runtime := $(wildcard $(painterengine_path)/runtime/*.c)
painterengine_build_painterengine_o += $(patsubst %.c,%.o,$(painterengine_build_runtime))

painterengine_build_platform := $(wildcard $(painterengine_path)/platform/linux/*.c)
painterengine_build_painterengine_o += $(patsubst %.c,%.o,$(painterengine_build_platform))

painterengine_build_platform := $(wildcard $(painterengine_path)/platform/linux/*.cpp)
painterengine_build_painterengine_o += $(patsubst %.cpp,%.o,$(painterengine_build_platform))

all:$(project_build_o)  $(painterengine_build_painterengine_o) 
	gcc $(project_build_o) $(painterengine_build_painterengine_o) \
	-o $(target) \
	-I "$(painterengine_path)" \
	-I "$(project_path)" \
	-I "$(painterengine_path)/platform/linux" \
	-I "$(painterengine_path)/runtime" \
	-L. -lGL -lglut -lpthread \
	$(LDFLAGS)
	

# 指定头文件搜索路径列表(针对的是project_path变量目录下的.c/.cpp文件)
#header_paths := ./src/view ./src/event
header_paths := $(shell find $(project_path) -type d -mindepth 1 -printf '%p\n')
# 生成 -I 选项字符串
CFLAGS = $(foreach path,$(header_paths),-I "$(path)")
$(info CFLAGS: $(CFLAGS))

$(project_path)/%.o:$(project_path)/%.c
	gcc -c $^ -o $@ $(CFLAGS) -I "$(painterengine_path)" -I "$(painterengine_path)/platform/linux" -I "$(painterengine_path)/runtime"

$(project_path)/%.o:$(project_path)/%.cpp
	gcc -c $^ -o $@ $(CFLAGS) -I "$(painterengine_path)" -I "$(painterengine_path)/platform/linux" -I "$(painterengine_path)/runtime"

$(painterengine_path)/runtime/%.o:$(painterengine_path)/runtime/%.c 
	gcc -c $^ -o $@ -I "$(painterengine_path)"

$(painterengine_path)/kernel/%.o:$(painterengine_path)/kernel/%.c
	gcc -c $^ -o $@

$(painterengine_path)/core/%.o:$(painterengine_path)/core/%.c
	gcc -c $^ -o $@

$(painterengine_path)/platform/linux/%.o:$(painterengine_path)/platform/linux/%.c
	gcc -c $^ -o $@ -I "$(project_path)" -I "$(painterengine_path)" -I "$(painterengine_path)/platform/linux" -I "$(painterengine_path)/runtime"

clean:
	@echo "Cleaning..."
	@find $(project_path) -name "*.o" -type f -delete

