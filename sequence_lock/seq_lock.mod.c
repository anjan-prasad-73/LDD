#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

#ifdef CONFIG_UNWINDER_ORC
#include <asm/orc_header.h>
ORC_HEADER;
#endif

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
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

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xf9a482f9, "msleep" },
	{ 0xb3f7646e, "kthread_should_stop" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x1f337bd7, "kthread_create_on_node" },
	{ 0xf87c611c, "wake_up_process" },
	{ 0x2ef1b23, "kthread_stop" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0xba8fbd64, "_raw_spin_lock" },
	{ 0x122c3a7e, "_printk" },
	{ 0xb5b54b34, "_raw_spin_unlock" },
	{ 0xe2fd41e5, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "BB5F876E2E5332B5C683488");
