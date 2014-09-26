/*
 * Copyright (C) 2014, xgfone <xgfone@126.com>
 * Generic Netlink Doc: http://lwn.net/Articles/208755/
 *
 * =============================================================================
 *
 * The Generic Netlink mechanism is based on a client-server model. The Generic
 * Netlink servers register families, which are a collection of well defined
 * services, with the controller communicate with the server through these
 * service registerations.
 *
 * genl_family: Family Type
 * genl_ops:    Family Operation
 *
 *
 * struct genl_family {
 *     unsigned int     id;
 *     unsigned int     hdrsize;
 *     char             name[GENL_NAMSIZ];
 *	   unsigned int     version;
 *     unsigned int     maxattr;
 *
 *     // Private, NOT USE
 *     struct nlattr ** attrbuf;
 *     struct list_head ops_list;
 *     struct list_head family_list;
 * }
 * @id: 应总是0x0，不应使用0x10。
 * @hdrsize: 如果family使用一个特定的头（即在Generic Netlink协议头的基础上再添加一个自己的
 *           私有协议头），则表示该头的大小。如果没有特定的头，则应总是0。
 * @name: Family的名字，必须唯一；Controller会使用它来查找频道号（Channel），即字段id。
 * @version: Faimily的版本号，可以随意指定。
 * @maxattr: Faimily所使用的属性的最大个数。
 *
 *
 * struct genl_ops {
 *     u8                cmd;
 *     unsigned int      flag;
 *     struct nla_policy *policy;
 *     int               (*doit)(struct sk_buff *skb, struct genl_info *info);
 *     int               (*dumpit)(struct sk_buff *skb, struct ntlink_callback *cb);
 *
 *     // Private, NOT USE
 *     struct list_head  ops_list;
 * }
 * @cmd: 在Family中必须唯一，表示一个操作。
 * @flag: 表示该操作的特殊属性，如：是否需要ROOT权限（GENL_ADMIN_PERM）。如果有多个属性，则
 *        使用 OR 连接。
 * @policy: 定义该操作中请求消息的Netlink属性策略。如果指定，那么在调用操作处理器（即doit）
 *          之前，Generic Netlink机制则使用这个策略去验证这个操作所请求的消息的所有属性。
 *          这个属性策略是一个 struct nla_policy 结构体类型的数组，并通过属性号来索引。
 *          struct nla_policy 结构体见下文。
 * @doit: 返回 0 表示成功，返回负值表示失败；doit 处理器应该做所有的处理工作。
 * @dumpit: 仅当Generic Netlink消息中设置了 NLM_F_DUMP 标志/属性时，dumpit 才会被调用。
 *
 *
 * struct nla_policy {
 *     u16 type;
 *     u16 len;
 * }
 * @len:  如果 @type 是字符串，@len 是字符串的最大长度（不包括NULL）；
 *        如果未知或为NLA_UNSPEC，@len 应设置成属性负载的正确长度。
 * @type: 属性类型。如下：
 *           NLA_UNSPEC
 *           NLA_U8
 *           NLA_U16
 *           NLA_U32
 *           NLA_U64
 *           NLA_FLAG(bool)
 *           NLA_MSECS
 *           NLA_STRING
 *           NLA_NUL_STRING
 *           NLA_NESTED
 *        其中，
 *           NLA_MSECS: 64位时间值（以毫秒ms为单位）。
 *           NLA_STRING: 可变长度字符串。
 *           NLA_NUL_STRING: 以NULL结尾的可变长度字符串。
 *           NLA_NESTED: A stream of attributes。
 *
 *
 * struct genl_info {
 *	  u32                 snd_seq;
 *    u32                 snd_pid;
 *    struct nlmsghdr *   nlhdr;
 *    struct genlmsghdr * genlhdr;
 *    void *              userhdr;
 *    struct nlattr **    attrs;
 * }
 * @snd_seq: Generic Netlink的请求序列号。
 * @snd_pid: 发送此请求的客户端的PID号。注：3.8以的内核版本将此字段改成了 portid。
 * @nlhdr:   Generic Neltink请求的Netlink消息协议头。
 * @genlhdr: Generic Neltink请求的Generic Netlink消息协议头。
 * @userhdr: 如果Generic Netlink Family使用了特殊消息协议头，那么该字段就指向这个消息头的起始处。
 * @attrs:   从请求中解析出来的Generic Netlink属性（如果在Generic Netlink Family定义中
 *           指定了Netlink消息策略），并且这些属性已经经过了验证。
 *
 *
 * Generic Neltink Message Format
 * ==============================
 *
 *  0                                          31
 * +--------------------------------------------+
 * |      Netlink Message Header(nlmsghdr)      |
 * +--------------------------------------------+
 * | Generic Neltink Message Header(genlmsghdr) |
 * +--------------------------------------------+
 * |   Optional User Specific Message Header    |
 * +--------------------------------------------+
 * |  Optional Generic Netlink Message Payload  |
 * +--------------------------------------------+
 *
 *
 * Generic Netlink 一般使用方法
 * 一、内核空间：
 *    1、注册一个Generic Netlink Family（分为四步）
 *      (1)定义一个Family；
 *      (2)定义一个操作ops；
 *      (3)注册一个Family；
 *      (4)注册一个操作ops。
 *
 *    2、处理消息
 *       <I> 发送(分为三步):
 *           (1)为消息缓冲区分配内存；
 *           (2)创建消息（Payload）；
 *           (3)发送消息。
 *      <II> 接收(分两种情况):
 *           (1)典型地，内核扮演了Generic Netlink服务器的角色，这意味着消息的接收被Generic
 *              Netlink 总线自动处理：一旦总线接收到了消息并确定了正确的路由，此消息将直接被
 *              传递给与Family相关的、特定的操作（回调函数）来处理。
 *           (2)如果内核作为一个客户端的角色，那么服务器的响应消息能够使用监听在 Generic
 *              Netlink Socket上的标准内核Socket接口来接收。
 *
 * 二、用户空间：
 *    (1)可以使用标准的Socket API来接收和发送Generic Netlink消息。
 *    (2)也可以使用 libnl 库：  http://www.carisma.slowglass.com/~tgr/libnl/。
 *
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
