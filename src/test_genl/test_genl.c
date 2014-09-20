/*
 * Copyright (C) 2014, xgfone <xgfone@126.com>
 */

#include <linux/module.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/genetlink.h>
#include <net/genetlink.h>


/////
extern int genl_send_msg_to_user(void *data, int len, int pid);


////////////////////////////////////
// First: Define a family.

// 1.attributes
enum {
	DOC_EXMPL_A_UNSPEC,
	DOC_EXMPL_A_MSG,
	__DOC_EXMPL_A_MAX,
};
#define DOC_EXMPL_A_MAX (__DOC_EXMPL_A_MAX - 1)

// 2. attribute policy
static struct nla_policy doc_exmpl_genl_policy[DOC_EXMPL_A_MAX + 1] = {
	[DOC_EXMPL_A_MSG] = { .type = NLA_NUL_STRING },
};

// 3. family definition
static struct genl_family doc_exmpl_genl_family = {
	.id = GENL_ID_GENERATE,
	.hdrsize = 0,
	.name = "DOC_EXMPL",
	.version = 1,
	.maxattr = DOC_EXMPL_A_MAX,
};

////////////////////////////////////
// Second: Define a handler.

// 1. handler
// Return 0 on success, negative values on failure.
static int doc_exmpl_echo(struct sk_buff *skb, struct genl_info *info)
{
	// doit 没有运行在中断上下文.
	struct nlmsghdr     *nlhdr;
	struct genlmsghdr   *genlhdr;
	struct nlattr       *nlh;
	void *data;

	nlhdr = nlmsg_hdr(skb);
	genlhdr = nlmsg_data(nlhdr);
	nlh = genlmsg_data(genlhdr);
	data = nla_data(nlh);

	// Handler the message here.
	// TODO:)
	pr_info("====== %s\n", (char *)data);
	// For kernel 3.8 above
	genl_send_msg_to_user(data, strlen(data)+1, info->snd_portid);
	// For kernel 3.8 below
	//genl_send_msg_to_user(data, strlen(data)+1, info->snd_pid);

	return 0;
}

// 2. commands
enum {
	DOC_EXMPL_C_UNSPEC,
	DOC_EXMPL_C_ECHO,
	__DOC_EXMPL_C_MAX,
};
#define DOC_EXMPL_C_MAX (__DOC_EXMPL_C_MAX - 1)

// 3. operation definition
static struct genl_ops doc_exmpl_genl_ops[] = {
	{
		.cmd = DOC_EXMPL_C_ECHO,
		.flags = 0,
		.policy = doc_exmpl_genl_policy,
		.doit = doc_exmpl_echo,
		.dumpit = NULL,
	},
};

/////////////////////////////////////
// Third: Register the DOC_EXMPL family with the Generic Netlink operation.
// Fourth: Register the operations for the family.
static int doc_exmpl_genl_reg(void)
{
	// 3st and 4st
	// For kernel 3.8 below
	//return genl_register_family_with_ops(&doc_exmpl_genl_family, doc_exmpl_genl_ops, sizeof(doc_exmpl_genl_ops));
	return genl_register_family_with_ops(&doc_exmpl_genl_family, doc_exmpl_genl_ops);
}


static int doc_exmpl_genl_unreg(void)
{
	return genl_unregister_family(&doc_exmpl_genl_family);
}


/**
 * * genl_send_msg_to_user - 通过generic netlink发送数据到netlink
 * *
 * * @data: 发送数据缓存
 * * @len:  数据长度 单位：byte
 * * @pid:  发送到的客户端pid 这个pid要从用户空间发来数据触发的doit中的info->snd_pid参数获得
 * *
 * * return:
 * *    =0:       成功
 * *    <0:       失败
 * */
int genl_send_msg_to_user(void *data, int len, int pid)
{
	struct sk_buff *skb;
	size_t size;
	void *head;
	int err;

	// Calculate total length of attribute including padding
	size = nla_total_size(len);

	// Create a new netlink msg
	skb = genlmsg_new(size, GFP_KERNEL);
	if (!skb) {
		err = -ENOMEM;
		goto skb_error;
	}

	// Add a new netlink message to an skb
	head = genlmsg_put(skb, pid, 0, &doc_exmpl_genl_family, 0, DOC_EXMPL_C_ECHO);
	if (!head) {
		err = -1;
		goto error;
	}

	// Add a netlink attribute to a socket buffer
	err = nla_put(skb, DOC_EXMPL_A_MSG, len, data);
	if (err) {
		goto error;
	}

	head = genlmsg_data(nlmsg_data(nlmsg_hdr(skb)));

	err = genlmsg_end(skb, head);
	if (err < 0) {
		goto error;
	}

	err = genlmsg_unicast(&init_net, skb, pid);
	if (err < 0) {
		goto error;
	}

	return 0;

error:
	kfree_skb(skb);
skb_error:
	return err;
}


static int __init doc_exmpl_genl_init(void)
{
	pr_info("Register DOC_EXMPL Generic Netlink\n");
	return doc_exmpl_genl_reg();
}

static void __exit doc_exmpl_genl_exit(void)
{
	pr_info("Unregister DOC_EXMPL Generic Netlink\n");
	doc_exmpl_genl_unreg();
}

module_init(doc_exmpl_genl_init);
module_exit(doc_exmpl_genl_exit);

MODULE_AUTHOR("xgfone <xgfone@126.com>");
MODULE_DESCRIPTION("Test Generic Netlink");
MODULE_LICENSE("GPL");
