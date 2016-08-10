
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <Python.h>

#define NETLINK_DEFAULT		30

#define DEFAULT_GROUP   	1
#define DEFAULT_PORTID  	1
#define DEFAULT_RECV_TYPE	0
#define DEFAULT_DEST_TYPE	0
#define DEFAULT_DEST_PORTID	0
#define DEFAULT_DEST_GROUP	0

#define MAX_PAYLOAD 60000      /* maximum payload size*/
#define MAX_NL_BUFSIZ  NLMSG_SPACE(MAX_PAYLOAD)

#define MAX_FD 1024
static uint32_t fd_portid[MAX_FD] = {0};


static PyObject* None()
{
    Py_INCREF(Py_None);
    return Py_None;
}


static int nl_create(uint32_t pid, uint32_t groups, uint32_t protocol)
{
	int fd = socket(PF_NETLINK, SOCK_RAW, protocol);
	if (fd == -1) {
		return -1;
	}

	struct sockaddr_nl addr;
	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_pid = pid;
	addr.nl_groups = groups;

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		close(fd);
		return -1;
	}
	return fd;
}


static int nl_send(int fd, void *buffer, size_t size, struct sockaddr_nl *addr, unsigned char type)
{
	char nl_tmp_buffer[MAX_NL_BUFSIZ];
	struct nlmsghdr *nlh;
	unsigned char * tmp;

	if (size+1 > MAX_NL_BUFSIZ) {
		return -1;
	}

	// 设置Netlink消息缓冲区
	nlh = (struct nlmsghdr *)&nl_tmp_buffer;
	memset(nlh, 0, MAX_NL_BUFSIZ);
	nlh->nlmsg_pid = fd_portid[fd];
	//nlh->nlmsg_flags = 0;
	//nlh->nlmsg_seq = 0;

	tmp = NLMSG_DATA(nlh);
	*tmp = type;
	memcpy(tmp + 1, buffer, size);
	//nlh->nlmsg_len = NLMSG_SPACE(size+1);
	nlh->nlmsg_len = NLMSG_LENGTH(size + 1);

	return sendto(fd, nlh, NLMSG_SPACE(size+1), 0, (struct sockaddr *)addr, sizeof(*addr));

	// return sendto(fd, nlh, NLMSG_SPACE(size+1), 0,
	// 		(struct sockaddr *)addr, sizeof(*addr));

	// // 设置发送信息
	// struct iovec iov;
	// struct msghdr msg;
	// iov.iov_base = (void *)nlh;
	// iov.iov_len = nlh->nlmsg_len;
	// msg.msg_name = (void *)dst_addr;
	// msg.msg_namelen = sizeof(*dst_addr);
	// msg.msg_iov = &iov;
	// msg.msg_iovlen = 1;

	// return sendmsg(sock_fd, &msg, 0);
}

//// ==================

// create([pid=1, group=1, protocol=30])
static PyObject* py_nl_create(PyObject *self, PyObject *args, PyObject *keywds)
{
	int fd = -1;
	unsigned long pid = DEFAULT_PORTID;	// 1
	unsigned long group = DEFAULT_GROUP;	// 1
	unsigned long protocol = NETLINK_DEFAULT;  // 30
	static char *kwlist[] = {"pid", "group", "protocol", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, keywds, "|kkk", kwlist, &pid, &group, &protocol))
		return Py_BuildValue("i", -1);

	fd = nl_create((uint32_t)pid, (uint32_t)group, protocol);
	if (fd < 0)
		return Py_BuildValue("i", -2);

	if (fd >= MAX_FD) {
		close(fd);
		return Py_BuildValue("i", -3);
	}

	fd_portid[fd] = pid;
	return Py_BuildValue("i", fd);
}

// recv(fd [, type=0])
static PyObject* py_nl_recv(PyObject *self, PyObject *args, PyObject *keywds)
{
	int fd;
	int ret;
	unsigned char type = DEFAULT_RECV_TYPE;
	char buf[MAX_NL_BUFSIZ];
	PyObject *result = NULL;
	unsigned char *data;
	struct nlmsghdr *nlh;

	static char *kwlist[] = {"fd", "type", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, keywds, "i|b", kwlist, &fd, &type)) {
		return None();
		//return NULL;
	}

	memset(buf, 0, MAX_NL_BUFSIZ);
	ret = recvfrom(fd, buf, MAX_NL_BUFSIZ, 0, NULL, NULL);
	if (ret < 0) {
		return None();
	}

	nlh = (struct nlmsghdr *)buf;
	data = (unsigned char *)NLMSG_DATA(nlh);

	if (NLMSG_PAYLOAD(nlh, 0) <= 1) {
		return None();
	}

	if (*data != type)  {
		return None();
	}

	result = Py_BuildValue("(s#kHHkk)", (char *)(data+1), (unsigned long)NLMSG_PAYLOAD(nlh, 0)-1,
			(unsigned long)NLMSG_PAYLOAD(nlh, 0)-1, (unsigned short)(nlh->nlmsg_type),
			(unsigned short)(nlh->nlmsg_flags), (unsigned long)(nlh->nlmsg_seq),
			(unsigned long)(nlh->nlmsg_pid));

	if (result)
		return result;
	return None();
}

// send(fd, data, size [, pid=0, group=0, type=0])
static PyObject* py_nl_send(PyObject *self, PyObject *args, PyObject *keywds)
{
	int fd;
	int ret;
	char *data = NULL;
	unsigned long size;
	int _size;
	unsigned long pid = DEFAULT_DEST_PORTID;
	unsigned long group = DEFAULT_DEST_GROUP;
	unsigned char type = DEFAULT_DEST_TYPE;
	struct sockaddr_nl addr;
	static char *kwlist[] = {"fd", "data", "size", "pid", "group", "type", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, keywds, "iz#k|kkb", kwlist, &fd, &data, &_size,  &size, &pid, &group, &type)) {
		return Py_BuildValue("i", -2);
		//return NULL;    // If return NULL, raise a Exception
	}

	if (size != _size) {
		return Py_BuildValue("i", -2);
	}

	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_pid = pid;
	addr.nl_groups = group;

	ret = nl_send(fd, data, (size_t)size, &addr, type);
	if (ret < 0) {
		ret = -1;
	}
	return Py_BuildValue("i", ret);
}

// close(fd)
static PyObject* py_nl_close(PyObject *self, PyObject *args)
{
	int fd;
	if (!PyArg_ParseTuple(args, "i", &fd)) {
		return None();
	}
	close(fd);
	return None();
}


static PyMethodDef NetlinkMethods[] = {
	{"create", (PyCFunction)py_nl_create, METH_VARARGS|METH_KEYWORDS, "create a netlink socket"},
	{"recv", (PyCFunction)py_nl_recv, METH_VARARGS|METH_KEYWORDS, "receive a netlink service message from the kernel or the userspace"},
	{"send", (PyCFunction)py_nl_send, METH_VARARGS|METH_KEYWORDS, "send a netlink service message to the kernel or the userspace"},
	{"close", (PyCFunction)py_nl_close, METH_VARARGS, "close the netlink socket"},
	{NULL, NULL, 0, NULL},
};


#if PYTHON_ABI_VERSION < 3
void init_netlink()
{
	(void)Py_InitModule("_netlink", NetlinkMethods);
}
#else
static struct PyModuleDef NetlinkModule = {
	PyModuleDef_HEAD_INIT,
	"_netlink",
	"The python wrapper of linux netlink.",
	-1,
	NetlinkMethods
};

PyMODINIT_FUNC
PyInit__netlink()
{
	return PyModule_Create(&NetlinkModule);
}
#endif

