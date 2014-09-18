
#ifndef TEST_NETLINK
#define TEST_NETLINK

#include <linux/skbuff.h>
#include <linux/netlink.h>

#define NETLINK_DEFAULT 30

#define DEFAULT_DEST_GROUP   	1
#define DEFAULT_DEST_PORTID  	1
#define DEFAULT_SEND_TYPE	0
#define DEFAULT_RECV_TYPE	0

typedef void (*nl_recv_msg_t)(struct sk_buff *skb, struct nlmsghdr *nlh, void *data, size_t size);

extern void register_service_handler(nl_recv_msg_t handler, __u8 type);

// The basic function
extern int upcall_service_to_pid_or_group(void *data, size_t size, __u8 type, __u32 pg, bool group);

// The following is auxiliary functions based on `upcall_service_to_pid_or_group`.
// Unicast. The service type is DEFAULT_SEND_TYPE, that's, the default service type.
extern int unicast_to_pid(void *data, size_t size, __u32 pid);
extern int unicast(void *data, size_t size);

// Broadcast. The service type is DEFAULT_SEND_TYPE, that's, the default service type.
extern int broadcast_to_group(void *data, size_t size, __u32 group);
extern int broadcast(void *data, size_t size);

//////////////////////////

// Unicast. Must have a service type.
extern int unicast_service_to_pid(void *data, size_t size, __u8 type, __u32 pid);
extern int unicast_service(void *data, size_t size, __u8 type);

// Broadcast. Must have a service type.
extern int broadcast_servie_to_group(void *data, size_t size, __u8 type, __u32 group);
extern int broadcast_service(void *data, size_t size, __u8 type);


extern int test_netlink_init(void);
extern void test_netlink_exit(void);

#endif  /* TEST_NETLINK  */
