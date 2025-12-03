#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x017de3d5, "nr_cpu_ids" },
	{ 0x5a5a2271, "__cpu_online_mask" },
	{ 0x53a1e8d9, "_find_next_bit" },
	{ 0x7ee55633, "perf_event_release_kernel" },
	{ 0x037a0cba, "kfree" },
	{ 0xdc50aae2, "__ref_stack_chk_guard" },
	{ 0xc60d0620, "__num_online_cpus" },
	{ 0x6a5cc518, "__kmalloc_noprof" },
	{ 0x4b6442a6, "perf_event_create_kernel_counter" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x30cfb3dc, "cpu_number" },
	{ 0x92997ed8, "_printk" },
	{ 0xf453a3f2, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "822E8EF40546DBE71106041");
