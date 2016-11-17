compile_all:
	gcc -O3 -fomit-frame-pointer -w -DPAGING -o naskitis_copybased_burst_sort naskitis_copybased_burst_sort.c sort_module.o common.c
	@cat USAGE_POLICY.txt
