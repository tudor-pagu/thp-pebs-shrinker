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
	{ 0x122c3a7e, "_printk" },
	{ 0xe007de41, "kallsyms_lookup_name" },
	{ 0xb65a5ebe, "first_online_pgdat" },
	{ 0x65487097, "__x86_indirect_thunk_rax" },
	{ 0xc07351b3, "__SCT__cond_resched" },
	{ 0x5a8d03d4, "pfn_to_online_page" },
	{ 0xbcb36fe4, "hugetlb_optimize_vmemmap_key" },
	{ 0x8d522714, "__rcu_read_lock" },
	{ 0x2469810f, "__rcu_read_unlock" },
	{ 0x95c1f5bc, "node_data" },
	{ 0xe2c17b5d, "__SCT__might_resched" },
	{ 0x07f2811a, "folio_unlock" },
	{ 0x0dee6953, "cpu_number" },
	{ 0xcd7d9327, "perf_event_create_kernel_counter" },
	{ 0xc1007102, "__folio_put" },
	{ 0xf8d751c2, "__folio_lock" },
	{ 0x87a21cb3, "__ubsan_handle_out_of_bounds" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x5423446b, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "407D0147028586083726172");
