/*
 * Copyright (c) 2014 xgfone
 *
 * Communicate between the kernel and the userspace through the netlink.
 *
 * Use:
 *   1. Call test_netlink_init to register the Netlink Protocol.
 *   2. Call register_service_handler to register the service type, in order to
 *      receive the data from userspace.
 *   3. Call the unicast or broadcast function to send the data to the userspace
 *      from the kernel.
 *   4. Call test_netlink_exit to cleanup the Netlink.
 *
 */

#include <linux/module.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>

#include "test_netlink.h"

static struct sock *nl_sk = NULL;
static nl_recv_msg_t service_msg_handler[256] = {NULL};

void register_service_handler(nl_recv_msg_t handler, __u8 type)
{
	rcu_assign_pointer(service_msg_handler[type], handler);
}
EXPORT_SYMBOL(register_service_handler);


// upcall_service_to_pid_or_group:
//     Upcall message to the userspace through unicast or broadcast/multicast.
//
// @data: the data which is sent to the userspace.
// @size: the size of `data`.
// @type: the type of the service.
// @pg:   the pid or group of the receiver, according to `group`.
// @group: If true, broadcast the message; or, unicast.
int upcall_service_to_pid_or_group(void *data, size_t size, __u8 type, __u32 pg, bool group)
{
	struct sk_buff *skb_out;
	struct nlmsghdr *nlh;
	unsigned char *buffer;

	size += 1;	// Add a byte for `type`

	skb_out = nlmsg_new(size, GFP_ATOMIC);
	if(!skb_out) {
		printk(KERN_ERR "Failed to allocate a new sk_buff\n");
		return -1;
	}

	/*
	* struct nlmsghdr *
	* __nlmsg_put(struct sk_buff *skb, u32 portid, u32 seq, int type, int len, int flags);
	*/
	nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, size, 0);
	buffer = (unsigned char *)nlmsg_data(nlh);
	*buffer = type;
	memcpy(buffer+1, data, size-1);


	/*
	* 当编译时， 如果提示 struct netlink_skb_parms 结构体没有 pid 字段，
	* 请把下面此行注释掉， 并把带有 portid 字段的那一行打开。
	*/
	//NETLINK_CB(skb_out).portid = 0;  // Source ID, for Linux 3.8 above
	NETLINK_CB(skb_out).pid = 0;

	if (group) {  // 多播
		NETLINK_CB(skb_out).dst_group = pg;

		/*
		 * int
		 * netlink_broadcast(struct sock *ssk, struct sk_buff *skb, __u32 portid, __u32 group, gfp_t allocation);
		 * 向Group为group、并排除PortID为portid的所有 Netlink Socket 广播此消息。
		 */
		if (netlink_broadcast(nl_sk, skb_out, 0, pg, GFP_ATOMIC) < 0) {
			printk(KERN_ERR "Error while sending a msg to userspace\n");
			return -1;
		}
	}
	else {  // 单播
		NETLINK_CB(skb_out).dst_group = 0;  /* not in multicast group */
		if(nlmsg_unicast(nl_sk, skb_out, pg) < 0) {
			printk(KERN_INFO "Error while sending a msg to userspace\n");
			return -1;
		}
	}

	return 0;
}
EXPORT_SYMBOL(upcall_service_to_pid_or_group);


// Unicast the data to the userspace whose pid is `pid`.
int unicast_to_pid(void *data, size_t size, __u32 pid)
{
	return upcall_service_to_pid_or_group(data, size, DEFAULT_SEND_TYPE, pid, false);
}
EXPORT_SYMBOL(unicast_to_pid);

// Unicast the data to the userspace whose pid is DEFAULT_DEST_PORTID.
int unicast(void *data, size_t size)
{
	return unicast_to_pid(data, size, DEFAULT_DEST_PORTID);
}
EXPORT_SYMBOL(unicast);


// Unicast the service whose type is `type` to the userspace whose pid is `pid`.
int unicast_service_to_pid(void *data, size_t size, __u8 type, __u32 pid)
{
	return upcall_service_to_pid_or_group(data, size, type, pid, false);
}
EXPORT_SYMBOL(unicast_service_to_pid);

// Unicast the service whose type is `type` to the userspace whose pid is DEFAULT_DEST_PORTID.
int unicast_service(void *data, size_t size, __u8 type)
{
	return unicast_service_to_pid(data, size, type, DEFAULT_DEST_PORTID);
}
EXPORT_SYMBOL(unicast_service);

/// ---------------

// Broadcast the data to the userspace, which belongs to the group of `group`.
int broadcast_to_group(void *data, size_t size, __u32 group)
{
	return upcall_service_to_pid_or_group(data, size, DEFAULT_SEND_TYPE, group, true);
}
EXPORT_SYMBOL(broadcast_to_group);

// Broadcast the data to the userspace, which belongs to the group of DEFAULT_DEST_GROUP.
int broadcast(void *data, size_t size)
{
	return broadcast_to_group(data, size, DEFAULT_DEST_GROUP);
}
EXPORT_SYMBOL(broadcast);


// Broadcast the service whose type is `type` to the userspace, which belongs to the group of `group`.
int broadcast_service_to_group(void *data, size_t size, __u8 type, __u32 group)
{
	return upcall_service_to_pid_or_group(data, size, type, group, true);
}
EXPORT_SYMBOL(broadcast_service_to_group);

// Broadcast the service whose type is `type` to the userspace, which belongs to the group of DEFAULT_DEST_GROUP.
int broadcast_service(void *data, size_t size, __u8 type)
{
	return broadcast_service_to_group(data, size, type, DEFAULT_DEST_GROUP);
}
EXPORT_SYMBOL(broadcast_service);

/// -----------------------------------------------------------------------

static void msg_handler_default(struct sk_buff *skb, struct nlmsghdr *nlh, void *data, size_t size)
{
	unsigned char *buffer = (unsigned char *)data;

	if (size == 0) {
		printk(KERN_ERR "No Netlink Message\n");
	}

	if (service_msg_handler[*buffer]) {
		service_msg_handler[*buffer](skb, nlh, buffer+1, size-1);
	} else {
		printk(KERN_ERR "Netlink Protocol(%d) received a unknown service message: ServiceType(%d)\n", NETLINK_DEFAULT, *buffer);
	}
}

static void nl_recv_msg(struct sk_buff *skb)
{
	struct nlmsghdr *nlh = (struct nlmsghdr*)skb->data;
	msg_handler_default(skb, nlh, nlmsg_data(nlh), nlmsg_len(nlh));
}

static void default_service_handler(struct sk_buff *skb, struct nlmsghdr *nlh, void *data, size_t size)
{
	printk(KERN_INFO "Recived a default service message\n");
}

int test_netlink_init(void)
{
	pr_info("Loading Netlink Module\n");

	/*
	// 对于不同版本的Linux内核， NETLINK接口有所变化， 下面供参考

	// Args:
	//      net:   &init_net
	//      unit:  User-defined Protocol Type
	//      input: the callback function when received the data from the userspace.

	// 3.8 kernel and above
	// struct sock* __netlink_kernel_create(struct net *net, int unit,
	//                                      struct module *module,
	//                                      struct netlink_kernel_cfg *cfg);
	// struct sock* netlink_kernel_create(struct net *net, int unit, struct netlink_kernel_cfg *cfg)
	// {
	//     return __netlink_kernel_create(net, unit, THIS_MODULE, cfg);
	// }
	//

	// 3.6 or 3.7 kernel
	// struct sock* netlink_kernel_create(struct net *net, int unit,
	//                                    struct module *module,
	//                                    struct netlink_kernel_cfg *cfg);

	// 2.6 - 3.5 kernel
	// struct sock *netlink_kernel_create(struct net *net,
	//                                    int unit,
	//                                    unsigned int groups,
	//                                    void (*input)(struct sk_buff *skb),
	//                                    struct mutex *cb_mutex,
	//                                    struct module *module);

	*/


	// Linux Kernel from 2.6.32 - 3.5
	nl_sk = netlink_kernel_create(&init_net, NETLINK_DEFAULT, 0, nl_recv_msg, NULL, THIS_MODULE);
	if(!nl_sk) {
		printk(KERN_ALERT "Failed to create socket.\n");
		return -10;
	}

	/*
	//This is for 3.8 kernels and above.
	struct netlink_kernel_cfg cfg = {
		.input = nl_recv_msg,
	};
	nl_sk = netlink_kernel_create(&init_net, NETLINK_DEFAULT, &cfg);
	if(!nl_sk) {
		printk(KERN_ALERT "Error creating socket.\n");
		return -10;
	}
	*/

	register_service_handler(default_service_handler, DEFAULT_RECV_TYPE);

	return 0;
}

void  test_netlink_exit(void) {
	printk(KERN_INFO "Unloading Netlink Module\n");
	if (nl_sk)
		netlink_kernel_release(nl_sk);
}

module_init(test_netlink_init);
module_exit(test_netlink_exit);

MODULE_DESCRIPTION("TEST Netlink");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("xgfone <xgfone@126.com>");
